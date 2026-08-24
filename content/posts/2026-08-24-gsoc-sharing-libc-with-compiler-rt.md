---
title: "GSOC26: Sharing LLVM-libc's floating-point routines with compiler-rt"
author: "Mohamed Emad"
date: "2026-08-24"
tags: ["gsoc", "libc", "compiler-rt", "builtins", "floating-point", "math"]
---

## Introduction

Hello LLVM community! I am Mohamed Emad, CSE student at Zagazig University, Egypt. This summer I worked on the next step of the Hand-in-Hand project, an effort that began in 2024 with [#91651](https://github.com/llvm/llvm-project/pull/91651) to make LLVM-libc a shared foundation that the rest of LLVM can build on.

My part was to share LLVM-libc's floating-point math routines with compiler-rt. It replaces compiler-rt's existing builtins, which were written from scratch in C and assembly with a separate implementation per architecture, with LLVM-libc's code, which is well-tested and well-maintained.

## Overview

When a program targets hardware without a floating-point unit, the compiler
cannot emit an `fadd` or `fcvt` instruction. Instead it emits a call to a helper
routine: `__adddf3` to add two doubles, `__fixdfsi` to convert a double to an
`int`, `__letf2` to compare two `float128`s. compiler-rt's builtins library
ships these routines, and it has carried its own soft-float implementations of
them for years.

LLVM-libc implements the same math independently, with correctly-rounded results
and a large test suite behind it. So the project starts from an awkward fact:
LLVM keeps two separate soft-float implementations of the same operations. Two
copies drift apart, each needs its own review and testing, and a bug fixed in
one rarely reaches the other. compiler-rt's versions are also older and less
rigorously verified than libc's.

This project gives compiler-rt one source of truth. LLVM-libc exposes its
routines as freestanding headers under `LIBC_NAMESPACE::shared::`, and each
compiler-rt builtin becomes a thin wrapper that forwards to the matching libc
routine. A single `.c` file such as `truncdfsf2.c` is swapped for a
`truncdfsf2.cpp` that calls `shared::truncdfsf2`. The swap happens through a
CMake macro, `use_libc_builtin()`, gated on `COMPILER_RT_USE_LIBC_MATH`, so
distributors who want the libc-backed path opt in and everyone else keeps the
existing behavior.

![compiler-rt and LLVM-libc each carried their own soft-float routine; now compiler-rt's builtin is a thin wrapper over the single shared libc routine](/img/gsoc26-sharing-libc-with-compiler-rt-01-duplication-to-shared.png)

*Figure 1: Two soft-float implementations of the same operation become one shared LLVM-libc routine that compiler-rt forwards to.*

The result covers the arithmetic builtins (add, subtract, multiply, divide for
`float`, `double`, and `float128`), float-to-integer and integer-to-float
conversions across the integer widths, float-to-float extend and truncate
across every format including x87 80-bit, half, and bfloat16, and the comparison
routines. compiler-rt gets libc's tested math, and LLVM stops maintaining the
operation twice.

![At build time use_libc_builtin swaps the .c source for a .cpp wrapper; at the call site the wrapper forwards the builtin call into the shared libc header](/img/gsoc26-sharing-libc-with-compiler-rt-02-delegation-mechanism.png)

*Figure 2: The build-time swap and the call path: `use_libc_builtin()` replaces `truncdfsf2.c` with a wrapper that forwards `__truncdfsf2` to `shared::truncdfsf2`.*

## Challenges

**A routine that calls itself.** Some of LLVM-libc's conversions are written as
an ordinary cast from one floating-point type to another. That is fine when the
hardware can do the conversion. On a target without an FPU, the compiler turns
that same cast back into a call to the very builtin we are implementing, so the
routine calls itself and never finishes. The way out was to do the conversion
through an internal, integer-only representation, so nothing the compiler sees
can turn back into a floating-point builtin. A companion project, described
below, will let us drop even this workaround.

![On the left, an ordinary cast turns back into the builtin and recurses forever; on the right, going through an integer-only representation breaks the loop](/img/gsoc26-sharing-libc-with-compiler-rt-03-self-call-loop.png)

*Figure 3: An ordinary cast turns back into the builtin and loops forever (left); going through an integer-only representation breaks the loop (right).*

**16-bit floats the target cannot name.** Half precision and bfloat16 are not
real types on every target, and even where they exist, naming one in a function
signature quietly pulls in yet another conversion builtin. So these routines
never take a 16-bit value directly. They receive the raw bits and rebuild the
number from a description of the format, which lets the half and bfloat16
conversions work even on targets that have no 16-bit float type at all.

**The 80-bit x87 format.** Intel's 80-bit extended `long double` does not follow
the same layout rules as the standard IEEE formats the shared code expects, and
it needs a 128-bit integer to hold its bit pattern, which does not exist on
32-bit x86 where `long double` is still 80 bits. We handled it the way
compiler-rt already does: convert through a standard `double` or `float` at the
edges, and compile these routines only where the 80-bit format actually exists.

**Two libraries in one binary.** LLVM-libc's routines carry their own symbol
names. Linked into compiler-rt next to the target's real C library, those names
would collide. We build the shared routines under a private namespace so nothing
clashes, and keep the few legacy alias names that some platforms still expect.

**Seventy builtins, one pattern.** Every builtin repeats the same shape in four
places: an LLVM-libc header, a compiler-rt wrapper, a build-system entry, and a
test. Keeping seventy of them aligned by hand would invite mistakes, so a small
generator produces all four from a single description of each builtin. One
change to the pattern updates every builtin at once.

## What comes next

The functional work is landing. The next phase is measuring and tuning it.

**Benchmarking.** These routines run inside the compiled output of any program
built for an FPU-less target, so both their speed and their code size matter. We
will compare each libc-backed builtin against compiler-rt's original soft-float
version across the targets that actually use them: soft-float Arm, x86 without
SSE, and bare-metal embedded configurations. The comparison covers per-call
latency and the size each routine adds to a static binary. We will test it on [raspberry pi pico 2](https://www.raspberrypi.com/products/raspberry-pi-pico-2/) since it uses Arm Cortex-M33 that doesn't contain a [floating-point unit](https://en.wikipedia.org/wiki/Floating-point_unit) for double-precision so it will be a good choice for benchmarking and finding the tradeoffs.

**Optimization.** The integer-only conversion path trades some hand-tuned bit
manipulation for a single shared implementation. Where a benchmark shows that costing more than compiler-rt's original, we will tighten the hot paths, and we will watch
code size closely so the shared routines stay usable in size-constrained builds.

**Emulated soft-float types.** Two efforts in LLVM-libc will replace the
workarounds behind the conversion builtins with something cleaner. A companion
GSoC 2026 project by [Sukumar Sawant](https://github.com/sukumarsawant),
[Emulated Float128 and Float80 in LLVM-libc](https://summerofcode.withgoogle.com/programs/2026/projects/r3Y64A3Y)
(tracked in [#206895](https://github.com/llvm/llvm-project/issues/206895)), adds
struct-backed versions of the wide formats, and [software
`float16`](https://github.com/llvm/llvm-project/pull/184283) does the same for
half. Once a format has an emulated type, converting to it no longer turns back
into the builtin we are implementing, because the compiler has no native type to
convert with and uses the software path instead. The self-call problem
disappears at its source, the half conversions stop needing the raw-bits detour,
and the code drops the integer-only workaround for a direct conversion.
That is also where most of the optimization headroom lives: an emulated type
carries a proper representation the routines can specialize against, rather than
rebuilding the value from bits on every call.

**Finishing the port.** The comparison stack and the integer, x87, half, and
bfloat16 conversion stacks are in review. Once they merge, the remaining
builtins follow the same generator-driven pattern.

## Milestones

| PR                                                                  | Title                                                                  | Status |
| ------------------------------------------------------------------- | ---------------------------------------------------------------------- | ------ |
| [#197950](https://github.com/llvm/llvm-project/pull/197950)         | Introduce libc math routines into compiler-rt builtins                 | Merged |
| [#200094](https://github.com/llvm/llvm-project/pull/200094)         | Introduce shared compiler-rt builtins in LLVM-libc                     | Merged |
| [#200196](https://github.com/llvm/llvm-project/pull/200196)         | Add precommit CI for the compiler-rt + libc integration                | Merged |
| [#200539](https://github.com/llvm/llvm-project/pull/200539)         | Document builtin compatibility                                         | Merged |
| [#205669–#205679](https://github.com/llvm/llvm-project/pull/205669) | Shared add/sub/mul/div builtins for `sf`/`df`/`tf` (LLVM-libc, 11 PRs) | Merged |
| [#207092](https://github.com/llvm/llvm-project/pull/207092)         | Libc-backed arithmetic builtins (compiler-rt)                          | Merged |
| [#207543](https://github.com/llvm/llvm-project/pull/207543)         | Libc-backed float-to-int conversion builtins                           | Merged |
| [#209900](https://github.com/llvm/llvm-project/pull/209900)         | Libc-backed int-to-float/double conversion builtins                    | Merged |
| [#209984](https://github.com/llvm/llvm-project/pull/209984)         | Libc-backed float-to-float extend/truncate builtins                    | Merged |
| [#213481](https://github.com/llvm/llvm-project/pull/213481)         | Filter libc-backed builtins superseded by assembly (CMake)             | Merged |
| [#211881](https://github.com/llvm/llvm-project/pull/211881)         | Libc-backed bfloat16 extend/truncate builtins                          | Open   |
| [#211882](https://github.com/llvm/llvm-project/pull/211882)         | Libc-backed float16 extend/truncate builtins                           | Open   |
| [#212649](https://github.com/llvm/llvm-project/pull/212649)         | Add `cmp_helper` for soft-float comparisons                            | Open   |
| [#212651](https://github.com/llvm/llvm-project/pull/212651)         | Libc-backed single-float comparison builtins                           | Open   |
| [#212652](https://github.com/llvm/llvm-project/pull/212652)         | Libc-backed double-float comparison builtins                           | Open   |
| [#212653](https://github.com/llvm/llvm-project/pull/212653)         | Libc-backed quad-float comparison builtins                             | Open   |
| [#215725](https://github.com/llvm/llvm-project/pull/215725)         | Libc-backed int-to-quad conversion builtins                            | Open   |
| [#215726](https://github.com/llvm/llvm-project/pull/215726)         | Add the missing int-float conversion tests                             | Open   |
| [#215727](https://github.com/llvm/llvm-project/pull/215727)         | Libc-backed quad-to-int conversion builtins                            | Open   |
| [#215728](https://github.com/llvm/llvm-project/pull/215728)         | Libc-backed float80-to-int conversion builtins                         | Open   |
| [#215729](https://github.com/llvm/llvm-project/pull/215729)         | Libc-backed float80/bfloat16/float16 conversion builtins               | Open   |
| [#183959](https://github.com/llvm/llvm-project/pull/183959)         | Build option to warn about unlisted builtins                           | Open   |

## Conclusion

This summer I got to take a real piece of LLVM and remove a duplication that had been sitting in it for years. compiler-rt now backs its arithmetic and conversion builtins with LLVM-libc's tested math, and the comparison and remaining conversion builtins are in review and close behind. Along the way I learned far more about floating-point corner cases than I expected: how a harmless-looking cast can end up calling itself forever, why a 16-bit float can simply not exist on a target, and how the 80-bit x87 format bends nearly every rule around it.

There is still work ahead. The open pull requests need to land, and once the emulated soft-float types arrive, the conversions get simpler and the real benchmarking and optimization phase can begin. I am excited to keep pushing this forward with the community.

## Acknowledgments

Huge thanks to my mentors, [Tue Ly](https://github.com/lntue), [Michael Jones](https://github.com/michaelrj-google), and [Muhammad Bassiouni](https://github.com/bassiounix). Their reviews, patience, and design guidance shaped this work. Thank you as well to the LLVM-libc and compiler-rt reviewers for the careful feedback, and to the whole LLVM community for making room for a project like this. It has been a wonderful summer.
