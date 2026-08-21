---
author: "Prasoon Kumar (prasoon054)"
date: "2026-08-21"
tags: ["GSoC", "lit", "testing", "performance", "python"]
title: "GSoC 2026: Improving lit"
---

This summer I worked on improving lit, the LLVM Integrated Tester, as part of Google Summer of Code 2026.

I got interested in compilers after seeing how much heavy lifting happens underneath a single line of code, especially on parallel accelerators like GPUs.
Two courses at IIT Bombay, [SysML](https://www.cse.iitb.ac.in/~mythili/sysml/) and Advanced Computer Architecture, turned that from a passing interest into something I actually wanted to work on.
I'd like to eventually move toward compilers or microarchitecture for parallel compute engines as a career, and LLVM felt like the right place to start: it's compiler infrastructure already deployed at a scale few individual projects reach, so any performance work here has an immediate, wide blast radius.
lit specifically appealed to me because the problems in it turned out to be systems problems, process scheduling, IPC, syscall overhead, GIL contention, not surface-level scripting fixes, which is exactly the kind of low-level work I wanted hands-on experience with before jumping into compiler internals directly.

My mentors were [Aiden Grossman](https://github.com/boomanaiden154), [Paul Kirth](https://github.com/ilovepi), and [Petr Hosek](https://github.com/petrhosek).

# The Project

lit is LLVM's test runner.
Every `RUN:` line in every test file, in every subproject in the monorepo, ends up executed by it, under `ninja check-*`.
`RUN:` lines look like shell commands, but they aren't run by a shell.
LLVM switched to lit's own integrated shell a while back, so it's lit's own code, not `/bin/sh`, doing the parsing and executing.
None of that compiles or checks anything on its own.
It's just the overhead sitting between a developer pushing a change and finding out whether it passed.
lit hadn't gotten much dedicated performance work before this project.
It's testing infrastructure, and testing infrastructure tends to get left alone once it works well enough.

[Petr Hosek](https://github.com/petrhosek)'s [project idea](https://discourse.llvm.org/t/gsoc-2026-improving-lit/89663) flagged three areas as worth a closer look:

1. **Excessive process creation**: Builtin shell utilities like `cat`, `diff`, and resource-limit wrappers were spawning fresh Python interpreter subprocesses for every single invocation line.
2. **Execution engine & IPC bottlenecks**: Locking and queueing overhead in the execution engine (`multiprocessing.Pool`), causing scaling limits on modern high core count systems.
3. **Legacy codebase idioms**: Python 2 patterns, redundant string concatenations, and sub-optimal loop structures remaining across lit's execution paths.

Before I wrote the proposal for GSoC, I ran lit under a debugger to see how testing actually happened underneath: where it spent its time, which paths were hot, and which of those hot paths had obviously unoptimized code sitting in them.
That's how the proposal ended up as about twenty numbered items across four parts instead of three paragraphs, each one tied to a line I'd actually stepped through rather than a guess.
The sections below go through what actually happened once those turned into pull requests over the summer, which was not always what the proposal predicted.

<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-four-phases.png"><br/>
</div>

Work was organized into four key areas: codebase modernization, execution engine refactoring, in-process shell builtins, and profiling-driven optimizations.

# Modernizing the Codebase

The first two weeks focused on eliminating legacy Python 2 idioms remaining in lit's codebase:

- A suffix map implemented as a plain `dict` was replaced with an `IntEnum`.
- `zip(range(len(x)), x)` loops were refactored to `enumerate(x)`.
- Explicit argument calls like `super(Class, self)` were updated to modern `super()`.
- String concatenation in the shell lexer's (`ShLexer`) hottest method was changed from repeated `+=` string building to list joins.

Because `ShLexer` runs on every character of every `RUN:` line across every test file, optimizing string construction was worth benchmarking directly.

<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-shlexer-before.png"><br/>
</div>

It reduced the `CodeGen/X86` test suite execution time from 99.31s to 94.96s (4.35s, a 4.4% improvement) and the `Transforms` suite from 51.46s to 50.89s (0.57s, a 1.1% improvement).


<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-shlexer-after.png"><br/>
</div>

However, one planned change didn't land the way the proposal described it.
The plan called for `@dataclass(slots=True)` on lit's hot data objects (`ShellCommandResult`, `ShellEnvironment`, `DiffFlags`), but `slots=True` on `@dataclass` needs Python 3.10, and LLVM's minimum supported Python is 3.8.
I used plain `__slots__` instead, which works back to 3.8 and gets the same memory and attribute-access win, leaving a `# Replace __slots__ with @dataclass(slots=True)` comment in the code pointing at [GitHub issue #200531](https://github.com/llvm/llvm-project/issues/200531), filed to track the switch once the minimum version moves.
This kept coming up over the summer: LLVM's Python 3.8 floor ruled out several other modern constructs too, and each time the resolution was the same, ship the best version 3.8 supports, and a code comment pointing at what should replace it once the floor moves.

Two other Phase 1 items didn't ship early either, despite being planned as modernization PRs: replacing the hand-rolled `_caching_re_compile` with `functools.lru_cache`, and switching `discovery.py` from `os.listdir()` plus `isdir()` to `os.scandir()`.
Both only came back months later, through Scalene profiling near the end of the summer, with the numbers to justify them that the proposal didn't have.
They're under Future Work below.

### Pull Requests
- [optimize ShLexer string construction](https://github.com/llvm/llvm-project/pull/199641)
- [modernize ParserKind implementation using Python3 IntEnum](https://github.com/llvm/llvm-project/pull/199965)
- [replace zip(range(len(x)), x) with enumerate(x) in ProgressBar](https://github.com/llvm/llvm-project/pull/199884)
- [refactor super() calls in ResultCode](https://github.com/llvm/llvm-project/pull/199891)
- [add `__slots__` to hot shell execution objects](https://github.com/llvm/llvm-project/pull/199668)
- [handle config loading safely](https://github.com/llvm/llvm-project/pull/200168)
- [make MetricValue a proper abstract base class](https://github.com/llvm/llvm-project/pull/200187)
- [remove redundant f.close() in TestingConfig](https://github.com/llvm/llvm-project/pull/200459)

# Moving lit to ProcessPoolExecutor

lit previously dispatched tests through `multiprocessing.Pool` and collected results in a loop calling `ar.get(timeout)`.

<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-current-exec.png"><br/>
</div>

That collection loop contained two issues:

1. The timeout was calculated once before entering the loop, meaning `--timeout` did not anchor to a single absolute deadline as intended.
2. Test results were matched back to tests by list index rather than by identity, meaning out-of-order task completions could attribute results to the wrong test.

Migrating to `concurrent.futures.ProcessPoolExecutor` fixed both, through `as_completed()` and an explicit mapping from futures to tests.
It also decoupled lit's execution engine from the rest of the runner.
That's what let me swap in different backends later without touching anything else, more on that further down.

My own proposal sketched this part as wrapping the whole thing in `asyncio`, an `async def _execute_async` coroutine awaiting `asyncio.as_completed()` inside `asyncio.run()`.
What actually shipped left `asyncio` out entirely: a synchronous loop around plain `concurrent.futures.as_completed()`, since that alone fixed both bugs.
`asyncio` did come back later, but as a completely separate experimental engine rather than this migration, covered further down.

The initial PR landed as a neutral refactor.
Nine days later it got reverted, it had triggered two bugs in CPython's `concurrent.futures.process` that `multiprocessing.Pool` had never hit.

First, [Douglas Yung](https://github.com/dyung) reported an AArch64 macOS bot hanging on `ninja check-all`, which [David Candler](https://github.com/dcandler) reproduced on downstream Ubuntu/AArch64 hardware.
`ProcessPoolExecutor.shutdown(wait=True)` calls `call_queue.join_thread()` before it joins the worker processes with `p.join()`.
On macOS, the feeder thread behind `join_thread()` can't finish draining until the workers are joined, so it ends up blocking on a step that hasn't happened yet.
CPython's own source has carried a comment about this exact macOS deadlock risk since 2018, which is why `p.join()` exists, it's just sequenced on the wrong side of `join_thread()`.

Second, lit was submitting every test future to the executor up front.
Each submission writes an empty `send_bytes(b"")` call into the executor's internal wakeup pipe, and even though the payload itself is nothing, CPython's framing still costs 4 bytes per write for the length header.
The OS pipe buffer defaults to 64 KiB, and 65,536 bytes divided by 4 bytes per write is exactly 16,384 writes before it fills, at which point `submit()` blocks while holding `_shutdown_lock`, which the manager thread also needs to drain the pipe ([CPython gh-105829](https://github.com/python/cpython/issues/105829)).
Running `check-llvm` submits ~64,500 tests, far exceeding that pipe limit.

The fix implements windowed submission: outstanding futures are bounded to a sliding window, submitting one new test per completed future so the wakeup pipe never fills.
After verification across macOS and AArch64 test environments, the relanded migration merged cleanly.

### Pull Requests
- [migrate lit to ProcessPoolExecutor](https://github.com/llvm/llvm-project/pull/202681)
- [reland "migrate lit to ProcessPoolExecutor"](https://github.com/llvm/llvm-project/pull/209076)

# A Regression on High Core Count Machines

In August, [Cullen Rhodes](https://github.com/c-rhodes) reported a scaling regression: `check-llvm` ran 56% slower on a 64-core AArch64 Graviton2 machine (72.98s vs a 46.70s baseline).
Benchmarks on 10-core (macOS) and 16-core (Linux) test setups had shown only a 0.2% to 0.5% difference against baseline, indicating the slowdown was specific to high core count scaling rather than a universal regression.

The cause stemmed from `concurrent.futures.wait()`.
Calling `wait()` re-registers listener callbacks against every outstanding future on each iteration, causing repeated $O(N)$ scanning cost per completion as concurrency grows.

Replacing the `wait()` rescan with a `SimpleQueue` and `add_done_callback()`, registered once per future at submit time instead of rescanning the whole pending set, reduced the 56% regression on 64 cores down to ~8%.

My mentor [Paul Kirth](https://github.com/ilovepi) built on that, adding worker-side task batching (`execute_batch()`) on top of the same `SimpleQueue` model.
Paul ran the combined change on a 64-core Threadripper workstation I didn't have access to, and measured `check-llvm` dropping from 105.2s to 48.38s, 4.6% faster than the pre-migration baseline of 50.7s.

### Pull Requests
- [collect completed tests via callbacks, not wait()](https://github.com/llvm/llvm-project/pull/214386) (closed, superseded by Paul Kirth's fix below)
- [address scaling problems with ProcessPoolExecutor](https://github.com/llvm/llvm-project/pull/214853) (Paul Kirth)

# Evaluating Execution Engine Architectures

With the `ProcessPoolExecutor` migration in place, I built and benchmarked three alternative execution engine designs, to see if process creation overhead or IPC pickling costs could be cut further.

### Thread-Based Execution (`ThreadPoolExecutor`)

The most direct way to eliminate process creation and object pickling overhead is to use worker threads instead of processes.
In theory, a single process with multiple threads running tests should perform much better.

Before testing this, I had to fix three thread-safety issues in lit: `os.umask` (which is process-global and was mutated around every subprocess launch), the regex compilation cache in `_caching_re_compile` (an unlocked dictionary), and the per-suite environment dictionary in GoogleTest formats.

Once those races were fixed, I benchmarked `ThreadPoolExecutor` on `check-llvm`:
- **Apple M5 (10 cores)**: 401s with threads versus 216s with processes (1.86x slower)
- **Intel i5-13450HX (Ubuntu 24.04)**: 591.21s with threads versus 109.15s with processes (5.42x slower)

Profiling with Scalene at `-j1` and `-j10` revealed why threads performed so much worse than processes.
Plain Python bytecode's share of time barely changed, but native and C extension time rose from 10.28% to 34.12% on macOS, and from 16.35% to 43.79% on Linux, concentrated in `subprocess.py`, `threading.py`, and `posixpath.py`.
Because lit spends most of its time spawning and waiting on subprocesses, worker threads continuously contend for Python's Global Interpreter Lock (GIL).
With ten worker threads running concurrently, GIL contention dominated runtime.
Until free-threaded Python (PEP 703) becomes standard, threads are not viable for lit.

### Phase-Pipeline Execution

In lit today, a single worker process executes all five phases of a test from start to finish: parsing the test script, applying substitutions, parsing shell syntax, running the commands, and formatting results.
I experimented with a phase-pipeline design where each phase runs in its own process, passing work down a queue like a hardware CPU pipeline.

Timing execution phases in `TestRunner.py` showed that the actual execution phase (spawning and waiting on `llc` or `FileCheck`) accounts for 84% to 89% of total runtime.
The remaining four phases combined make up only ~10.7% of execution time.
By Amdahl's Law ($\frac{1}{1 - 0.107} \approx 1.12\times$), even if we perfectly overlapped those four phases with zero IPC overhead, the theoretical maximum speedup is about 1.12x.

When I built the prototype, wall-clock time came out virtually identical to the default process backend, within 0% to 4% noise.
It also required four pickle crossings per test instead of two, and didn't reduce the process count either.
Not worth the added complexity, so I dropped it.

### Asyncio Single-Process Engine

To eliminate worker processes and IPC pickling entirely, I wrote a single-process execution engine using Python's `asyncio`.

In an asyncio engine, test objects never leave the main process.
To prevent `asyncio.create_subprocess_exec` from spawning a helper thread per child process (which would re-introduce GIL contention), I explicitly set [`SafeChildWatcher`](https://docs.python.org/3.12/library/asyncio-policy.html#asyncio.SafeChildWatcher) so process reaping happens on the main event loop via signal handlers.

The benchmark results depended heavily on core count:
- **At `-j1`**: **1.8x to 2.3x faster** than process pools, because there is no IPC, pickling, or process pool overhead.
- **At `-j4+`**: **1.5x to 1.9x slower**, because a single Python event loop cannot run tasks across multiple CPU cores in parallel.

The crossover point falls between `-j2` and `-j3` on the machine I tested this on.
Below that, asyncio wins because there's no IPC to pay for.
Above it, one Python process just can't keep enough cores fed.

### Dispatch Chunking

I also tested dispatch chunking, where the main process submits multiple tests in a single `ProcessPoolExecutor.submit()` task call to reduce pickling frequency.

Because `--order=smart` sorts tests by descending historical duration, a contiguous chunk of consecutive tests concentrates several of the slowest tests onto one worker while other workers finish early and sit idle.
Chunk sizes above 1 require `--order=random` instead, to avoid exactly that.

On `llvm-mca` at `-j10`, the smart-order baseline was 3.15s.
Chunk sizes of 1, 8, and 32 under random order measured 3.29s, 3.14s, and 3.21s.
None of that is a difference worth asking reviewers to take on a hard `--order=random` requirement, so I left it as a draft instead of pushing it through review.

### Summary of Engine Experiments

| Engine Design | Mechanism | Benchmark Performance | Result |
| :--- | :--- | :--- | :--- |
| **ProcessPoolExecutor (Baseline)** | Worker process pool with windowed submission | Baseline (`check-llvm`: ~50.7s) | **Shipped as default** |
| **ThreadPoolExecutor** | Worker thread pool | 1.86x to 5.42x slower | Rejected (GIL contention) |
| **Phase-Pipeline Engine** | Dedicated process per test phase | 0% to 4% difference vs baseline | Rejected (adds pickle crossings) |
| **Asyncio Engine** | Single-process event loop | 1.8x–2.3x faster at `-j1`, slower at `-j4+` | Experimental prototype |
| **Dispatch Chunking** | Submitting test batches per worker task | No measurable win at `-j10` under `--order=random` | Left as a draft |

### Pull Requests
- [migrate lit to ThreadPoolExecutor](https://github.com/llvm/llvm-project/pull/212397) (closed)
- [add dispatch chunking to the worker pool](https://github.com/llvm/llvm-project/pull/214687) (draft)

# Running cat and diff In-Process

By late June, `cat` and `diff` were the shell builtins spawning external subprocesses, incurring Python interpreter startup overhead for file I/O operations.

<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-prev-builtin.png"><br/>
</div>

To run them in-process while maintaining pipeline compatibility (`|` and `2>&1`), I structured the implementation across three PRs:

1. Refactored `cat.py` and `diff.py` so `main()` delegates execution to a `run(argv, stdin, stdout, stderr, cwd)` function that takes IO streams as arguments.
2. Implemented an `InProcessPipe` shim (duck-typed to `subprocess.Popen`) allowing `TestRunner.py`'s pipeline executor to treat in-process builtins transparently alongside spawned tools.
3. Enabled in-process `cat` and `diff` execution by default.


<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-current-builtin.png"><br/>
</div>

Benchmarking in-process `cat` and `diff` across six builtin-heavy test suites (`UpdateTestChecks`, `Interpreter`, `Integer`, `Feature`, `ClangScanDeps`, and `ExtractAPI`) showed median wall-clock runtime dropping from 67.66s down to 63.02s (a **6.9% speedup**) with zero RSS increase and byte-identical test output.

### Pull Requests
- [add stream-injectable run() core to builtin cat](https://github.com/llvm/llvm-project/pull/204711)
- [use provided streams in builtin diff](https://github.com/llvm/llvm-project/pull/204869)
- [run builtin cat / diff in-process instead of spawning](https://github.com/llvm/llvm-project/pull/208024)

# The env Short-Circuit Bug

LLVM issue [#115578](https://github.com/llvm/llvm-project/issues/115578) identified a bug where a bare `env` command with no subcommand returned early from the pipeline executor, silently dropping subsequent commands (causing lines like `env | FileCheck` to skip validation).

The proposal's plan for this was to separate `env` processing out of the main command loop.
By the time I actually got to it, that wasn't necessary anymore: the `InProcessPipe` helper built earlier in the summer for the `cat`/`diff` work above gave me a pipeline-stage abstraction that didn't exist when the proposal was written, so I ran bare `env` through that instead of building the bespoke fix originally planned.
Fixing the bug meant `FileCheck` finally ran on lines it had always been silently skipping.
That surfaced three previously-invisible bugs across six test fixtures: a `KEY = VALUE` vs `KEY=VALUE` format mismatch, two check-prefix typos, and one FileCheck-empty-input case needing `--allow-empty`.
All three had been masked by the same class of failure the GitHub issue itself described.

### Pull Requests
- [stop bare env from short-circuiting the pipeline](https://github.com/llvm/llvm-project/pull/214512)

# Overview

- Cleaned up the legacy Python 2 idioms still left in lit's codebase.
- Migrated lit to `ProcessPoolExecutor`.
Found and fixed two CPython concurrency bugs along the way, and helped on the follow-up fix for the high-core-count regression.
- Tried a thread-based backend.
It performed far worse than processes because of GIL contention, and I wrote up why instead of just dropping it quietly.
- Moved `cat` and `diff` to run in-process instead of spawning a subprocess per invocation.
- Built and benchmarked three more execution engine designs on top of that.
- Fixed the `env` pipeline short-circuit bug, which turned up a handful of latent test bugs on the way.

# Future Work

- **Type Annotations**: Adding type annotations across lit to improve maintainability and catch type errors early.
- **Testing PPE under Random Ordering**: Evaluating `ProcessPoolExecutor` with `--order=random` by default or exploring better load-balancing heuristics for batched execution.

Profiling with Scalene late in the project highlighted additional targeted optimizations for future work:

<div style="margin:0 auto;">
  <img src="/img/gsoc26-improving-lit-scalene-high-level.png"><br/>
</div>

- **LRU Cache regex compilation**: Replacing `TestRunner.py`'s custom `_caching_re_compile` with `functools.lru_cache` saves ~0.10s self-CPU time on `llvm-mca`.
Already upstreamed: [use functools.lru_cache instead of lit.util.memoize](https://github.com/llvm/llvm-project/pull/217757).
- **Directory scanning with `os.scandir()`**: Replacing `os.listdir()` + `os.path.isdir()` with `os.scandir()` avoids redundant `stat()` calls during test discovery, cutting ~18% of warm discovery overhead on `llvm/test` and `clang/test`.
- **Pre-compiling `kPdbgRegex`**: Compiling `kPdbgRegex` once at module load saves 108MB in allocations and ~0.048s on full `llvm-mca` test runs.

Profiling ruled some things out too.
The proposal had breaking up `TestRunner.py`, over 2,400 lines in one file, listed as a possible refactor if profiling justified it.
I tried a few substitution-engine rewrites on top of the `lru_cache` fix above and none of them beat the noise floor, so there wasn't a real case for it, and it's still one file.

# What I've Learned

- **CPython concurrency internals**: chasing the macOS shutdown-ordering deadlock and the wakeup-pipe deadlock forced me to actually learn how CPython handles multi-process shutdown and IPC, not just treat `concurrent.futures` as a black box.
- **Syscall overhead and the GIL**: Scalene made it obvious why threads lost.
Syscall-heavy work spends most of its time exactly where threads can't help.
- **Duck-typed abstractions**: `InProcessPipe` mimicking `subprocess.Popen` meant the rest of the pipeline code didn't need to know or care that `cat` and `diff` weren't spawning a process anymore.

Beyond GSoC, I'd like to keep contributing to LLVM's testing infrastructure, particularly finishing off the async engine evaluation and the type-annotation pass listed under Future Work above.

I've also been reading up on MLIR on the side this summer, working through it with an LLM whenever I got stuck, and I'd like to get more directly involved there next, especially on the GPU-facing dialects.
It's a jump from testing infrastructure to compiler internals, but it's the direction I actually want my work to go in.

# Acknowledgements

Thank you to my mentors, [Aiden Grossman](https://github.com/boomanaiden154), [Paul Kirth](https://github.com/ilovepi), and [Petr Hosek](https://github.com/petrhosek), for their guidance throughout the summer.

Thanks also to [Douglas Yung](https://github.com/dyung) and [David Candler](https://github.com/dcandler) for reporting and verifying the macOS/AArch64 shutdown fixes, [Cullen Rhodes](https://github.com/c-rhodes) for identifying and verifying the high core count scaling regression, and [Alexander Richardson](https://github.com/arichardson) for reviewing several pull requests throughout the summer.

I am grateful to the LLVM Foundation for this opportunity.
