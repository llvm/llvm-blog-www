---
author: "Krish Gupta"
date: "2026-08-24"
tags: ["gsoc", "mlir", "bufferization", "memory-planning"]
title: "Static Memory Planner for MLIR: A GSoC 2026 Journey"
---

Hi, I am Krish Gupta. Over the summer I worked with the LLVM Foundation as part
of [Google Summer of Code 2026](https://summerofcode.withgoogle.com/programs/2026/projects/XsjxBQ9o)
on building a **static memory planner for MLIR**, mentored by
[Matthias Springer](https://github.com/matthias-springer) and
[Javed Absar](https://github.com/javedabsar1). This post walks through what we
built, why it matters, and what the iterative process of landing it upstream
actually looked like.

## The Problem

After bufferization, buffer lifetimes in MLIR IR are fully explicit: the
compiler knows exactly when each allocation starts and ends (for static
shapes; runtime-dependent allocations are handled conservatively). We use this
information to eliminate redundant heap allocations through a static memory
planner.

This matters for accelerator-oriented compilation. On many targets (embedded
CPUs, DSPs, custom accelerators) heap allocation is either not allowed or
slow, because access to main memory is expensive. Even where heap allocation
is available, individual small allocations hurt performance through
fragmentation and cache pressure. What you want instead is a single static
arena whose layout is computed at compile time:

```mlir
// Before: three separate heap allocations
%a = memref.alloc() : memref<1024xf32>  // 4096 bytes
%b = memref.alloc() : memref<512xf32>   // 2048 bytes
memref.dealloc %a : memref<1024xf32>
memref.dealloc %b : memref<512xf32>

// After: one arena, two views
%arena = memref.alloc() : memref<6144xi8>
%a = memref.view %arena[0][]  : memref<6144xi8> to memref<1024xf32>
%b = memref.view %arena[4096][]: memref<6144xi8> to memref<512xf32>
// ... uses of %a and %b ...
memref.dealloc %arena : memref<6144xi8>
```

The arena is `memref<Nxi8>` so it can hold buffers of different datatypes.
`memref.view` reinterprets slices back to their original types with zero
overhead. This is the core transformation the planner performs.

## How the Pass Works

The pass is a `FunctionOpInterface` pass: it runs per function and processes
the entire function body in six steps:

```
Step 1: Collect candidates: walk all memref.alloc ops, find their deallocs
        via alias analysis, apply eligibility checks.
Step 2: Build descriptors: compute (size, alignment, timeStart, timeEnd)
        for each candidate using a single O(n+m) block scan.
Step 3: Run the planner: feed descriptors into the chosen algorithm
        (trivial sequential packing or best-fit with lifetime overlap).
Step 4: Assign offsets: compute total arena size, assign byte offsets
        to each candidate.
Step 5: Create arena: allocate one memref<Nxi8> (or use a passed-in arg).
Step 6: Rewrite: replace each alloc with a memref.view into the arena,
        erase all associated deallocs.
```

The core data structure is `AllocationCandidate`, which groups a
`memref.alloc`, its set of potential deallocs (there can be more than one,
reachable through alias chains), the computed byte offset, size, and alignment.
The entire alias reasoning goes through `BufferViewFlowAnalysis`: a unified
analysis that models how buffers flow through `arith.select`,
`scf.if`/`scf.for` results, `cf` branches, and view ops without any
op-type-specific code in the pass itself. The rest of the blog explains how we
got there.

## PR 1: The Foundation ([#205125](https://github.com/llvm/llvm-project/pull/205125))

The first PR landed in late June and introduced the core pass:
`static-memory-planner-analysis`. It handled the straightforward case (allocs
and deallocs in the same basic block with a direct one-to-one relationship)
and put in place the architecture everything else builds on:

- **`trivialMemoryPlanner`**: a pure C++ function that takes a list of
  `(size, alignment, timeStart, timeEnd)` descriptors and returns offsets. Being
  a pure function means it can be unit tested independently and is easy to swap out.
- **`bestFitMemoryPlanner`**: a lifetime-aware algorithm that tries to reuse
  arena slots for allocations whose lifetimes do not overlap.
- **Arena modes**: `allocate` (create the arena inside the function) and `arg`
  (the arena is passed as a function argument, useful for external memory
  management).
- **Alignment**: the arena alignment is the LCM of all individual alignments,
  ensuring every view is correctly aligned regardless of datatype.

The review process surfaced an important point early: upstream contribution
culture expects every design decision to be justified and every edge case
explicitly tested. The `javedabsar1` feedback on adding `timeStart`/`timeEnd`
fields even before they were wired up was a preview of what was coming.

## PR 2: arith.select Deallocs ([#209106](https://github.com/llvm/llvm-project/pull/209106))

After the first PR landed, we hit the first real-world blocker. The
`ownership-based-buffer-deallocation` pipeline routinely produces patterns like
this:

```mlir
%a = memref.alloc() : memref<1024xf32>
%b = memref.alloc() : memref<1024xf32>
%sel = arith.select %cond, %a, %b : memref<1024xf32>
memref.dealloc %sel : memref<1024xf32>
```

Here neither `%a` nor `%b` has a direct `memref.dealloc` user. The original
pass just incremented a skip counter for both and moved on. The fix was a
forward DFS over the use-def graph, following `BufferViewFlowOpInterface`
(which `arith.select` implements) to find reachable deallocs.

This also required a **reverse-alias safety guard**: since `dealloc %sel` may
free either `%a` or `%b`, erasing it after replacing both with arena views is
only safe if all allocs it might free are managed by the arena. If any is not,
the dealloc cannot be erased. The review cycle on this PR shaped the final
approach: drop the group-constraint fixpoint in favour of the guard, emit hard
errors (not silent skips) for missing deallocs, and improve the
`buildAllocInfos` scan from O(n x m) to O(n+m) using a single upfront
`DenseMap`.

## PR 3: scf.if and the Architectural Shift ([#213634](https://github.com/llvm/llvm-project/pull/213634))

The deallocation pipeline also produces structured-control-flow patterns that
the DFS could not follow:

```mlir
// Pattern 1: dealloc on the scf.if result
%a = memref.alloc() : memref<1024xf32>
%0 = scf.if %c -> memref<1024xf32> {
  scf.yield %a
} else {
  scf.yield %b
}
memref.dealloc %0   // dealloc is on %0, not %a

// Pattern 2: dealloc inside the scf.if body
%a = memref.alloc() : memref<1024xf32>
scf.if %c {
  memref.dealloc %a
}
```

The mentor pointed to `BufferViewFlowAnalysis`
(`Transforms/BufferViewFlowAnalysis.h`), a unified alias analysis already used
elsewhere in the bufferization pipeline. It models buffer flow through all
relevant op types in one graph: `BufferViewFlowOpInterface` for `arith.select`,
`RegionBranchOpInterface` for `scf.if`/`scf.for`, `BranchOpInterface` for `cf`
branches, and `ViewLikeOpInterface` for `memref.view`. One analysis covers all cases.

We deleted the hand-rolled DFS entirely and replaced it with two calls:

- **`analysis.resolve(alloc)`** gives the forward alias set: every SSA value the alloc
  may flow into. Walking users of each alias finds all potential deallocs.
- **`analysis.resolveReverse(dealloc.getMemref())`** gives the reverse alias set: every
  alloc that flows into the dealloc's operand. Used by the safety guard.

Two design decisions made this correct:

**Lifetime anchoring via `findAncestorOpInBlock`.** A dealloc nested inside an
`scf.if` body is in a different block. `Block::findAncestorOpInBlock` walks up
the parent chain and returns the enclosing `scf.if` op in the plan block. That
op's index becomes `timeEnd` (conservative, but correct). The buffer is
considered live until the entire `scf.if` completes.

**The reverse-alias guard (again, but stronger).** For pattern 2 above, the
dealloc inside the `scf.if` body is attributed to `%a` and correctly anchored.
For the scf.if-sharing-a-nested-alloc case:

```mlir
%a = memref.alloc()           // entry block
%0 = scf.if %c -> memref<1024xf32> {
  memref.dealloc %a
  %b = memref.alloc()         // NESTED -- not arena-managed
  scf.yield %b
} else {
  scf.yield %a
}
memref.dealloc %0   // may free EITHER %a OR %b
```

`resolveReverse(%0)` traces back to both `%a` and `%b`. Since `%b` is nested
(not in the plan block), the guard fires: `%a` is conservatively skipped. No
miscompile, no leak.

The `else` vs `then` branch distinction turned out to be irrelevant to the
analysis: it works on SSA alias sets, not branch structure.

## PR 4: scf.for Test Cases ([#215221](https://github.com/llvm/llvm-project/pull/215221))

After the `BufferViewFlowAnalysis` switch, `scf.for` patterns were handled
correctly with no additional pass changes. This PR documented the results:

- Allocs nested inside an `scf.for` body are skipped (same rule as nested `scf.if`).
- `scf.for` with `iter_args` passing buffers across iterations hits the guard
  in all three mentor patterns: the per-iteration nested alloc makes erasing
  any shared dealloc unsafe.
- The canonical valid case (entry-block allocs used inside the loop body with
  deallocs in the entry block) transforms cleanly. The loop body gets arena
  views instead of raw alloc pointers.

This PR was deliberately tests-only. Having a clear separation between "this
was a correct transformation that just needed to be documented" and "this
required a new design decision" kept the review simple.

## PR 5: Edge and Error Cases ([#216610](https://github.com/llvm/llvm-project/pull/216610))

The final PR stress-tested the pass with patterns existing tests did not cover
and documented the results. Eight new analysis tests and four new error tests:

- Static and dynamic shapes coexisting in one function (static transforms,
  dynamic is skipped without interference).
- Both branches of an `scf.if` deallocating the same alloc (duplicate dealloc
  ops in the same arena slot, correctly deduplicated).
- Dealloc at depth 3: `findAncestorOpInBlock` walks up any depth, not just one level.
- Multi-hop alias chain: `%a/%b -> %0 -> %1 -> dealloc`. The `resolve()` BFS
  follows the full chain.
- Cross-interface chain: `arith.select -> scf.if -> dealloc`. Two different
  interface implementations in one alias path, exercised by a single
  `resolve()` call.
- `cf.cond_br` to a sibling block rejected as unstructured control flow (same
  error as `cf.br`).
- Walk-stops-on-first-error: `WalkResult::interrupt()` points the diagnostic at
  the problematic alloc, not at subsequent valid ones.

One test was removed during review: a dynamic alloc with no dealloc placed in
the errors file. It had no `expected-error` annotation since dynamic shapes are
silently skipped, not errored. A test without a defined diagnostic output does
not belong in a `-verify-diagnostics` file. Lesson learned.

## Challenges

**The group constraint dead-end.** The first implementation of `arith.select`
support used a fixpoint iteration to enforce that all allocs sharing a dealloc
must either all go into the arena or all be skipped. This was correct but
complicated and fragile. Switching to the reverse-alias guard removed the
fixpoint entirely: the guard is a simpler, more direct statement of the same
invariant.

**Understanding what already exists.** The biggest conceptual shift was
realizing that `BufferViewFlowAnalysis` already did everything the hand-rolled
DFS was trying to do, and more. The lesson: before implementing a bespoke
traversal, read the existing analysis infrastructure. You will often find that
the right tool exists and using it both simplifies your code and gives you
correctness for free on patterns you had not thought about.

**Upstream review culture.** Every error message, every test comment, every
variable name is scrutinized. Rephrasing "unstructured control flow is not
supported" replaced a longer, less precise message. Swapping the order of two
branches in a test to make the alias chain more obvious. These details matter
because the tests and messages are the documentation that future contributors
will read.

## What Was Built

| PR | What landed |
|---|---|
| [#205125](https://github.com/llvm/llvm-project/pull/205125) | Core pass, trivial + best-fit planners, alignment, arena modes |
| [#209106](https://github.com/llvm/llvm-project/pull/209106) | arith.select dealloc chains, reverse-alias guard, O(n+m) lifetime scan |
| [#213634](https://github.com/llvm/llvm-project/pull/213634) | BufferViewFlowAnalysis integration, scf.if support (nested deallocs + result aliases) |
| [#215221](https://github.com/llvm/llvm-project/pull/215221) | scf.for test coverage, canonical loop case documented |
| [#216610](https://github.com/llvm/llvm-project/pull/216610) | Edge and error case test suite (26 analysis tests, 4 error tests) |

The final pass lives in `StaticMemoryPlannerAnalysis.cpp`.

## Future Work

The immediate open question is lifting the entry-block restriction: only allocs
in the function's entry block are planned today. Two concrete directions we
discussed:

**Path-sensitive lifetime tightening.** The current `[timeStart, timeEnd]`
interval model treats branches conservatively: a buffer freed inside an
`scf.if` then-branch is considered live until the entire `scf.if` completes,
even though the else path never held it. Adding dominance-based non-overlap
detection (using MLIR's existing `DominanceInfo`) would let mutually exclusive
branches share arena slots, unlocking patterns like:

```mlir
%0 = alloc()
%1 = scf.if %c {
  %2 = alloc()    // could share %0's slot -- they never co-exist
  dealloc(%0)
  yield %2
} else {
  yield %0
}
dealloc(%1)
```

**Plugging in stronger planning algorithms.** The planner function is pure and
separately testable, so dropping in a better algorithm is straightforward.
Best-fit is already there. Beyond that, interval graph / graph-coloring
approaches and algorithms like
[minimalloc](https://github.com/google/minimalloc) map directly onto the
`(size, alignment, timeStart, timeEnd)` interface the planner already expects.

**Memory space awareness.** Heterogeneous targets have multiple distinct memory
regions with different capacity constraints. Extending the planner to assign
buffers to specific memory spaces while respecting per-space limits is a natural
follow-on.

## Acknowledgements

Huge thanks to my mentors Matthias Springer and Javed Absar for the patient and
precise review feedback, for pointing at `BufferViewFlowAnalysis` at exactly
the right moment, and for treating every PR as a teaching opportunity. The LLVM
community's review culture is demanding in the best possible way.

The project GSoC page is at
[summerofcode.withgoogle.com/programs/2026/projects/XsjxBQ9o](https://summerofcode.withgoogle.com/programs/2026/projects/XsjxBQ9o).
Feel free to reach out with questions or to build on this work at
[krishgupta2832@gmail.com](mailto:krishgupta2832@gmail.com).
