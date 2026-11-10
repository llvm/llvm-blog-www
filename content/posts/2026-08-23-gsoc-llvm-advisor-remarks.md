---
author: "Kamini Banait"
date: "2026-08-23"
tags:
  ["GSoC", "llvm-advisor", "optimization remarks", "visualization", "tooling"]
title: "GSoC 2026: llvm-advisor - Optimization Remarks & Visualizations"
---

This summer I worked on **llvm-advisor**, LLVM's web-based compiler advisor, as
part of Google Summer of Code 2026. llvm-advisor was started during last year's
GSoC by [Miguel Cárdenas](https://blog.llvm.org/posts/2025-08-31-gsoc-llvm-advisor/),
and this year I extended it with optimization-remarks storage, search, heatmap,
Code Explorer, PGO hotness, and snapshot diff. The goal was to make optimization
remarks (the diagnostics emitted by `clang -fsave-optimization-record`)
searchable, comparable, and visual.

Optimization remarks are the compiler's way of explaining its decisions back to
you. Every time Clang decides to vectorize a loop, inline a call, or give up on
an optimization, it can emit a structured diagnostic tied to a source location.
Without these remarks, the only way to reconstruct what happened is to read
assembly or rely on intuition. With remarks however, you can ask targeted questions: "Why wasn't this loop vectorized?", "Did my change help?", "Which missed remark is
actually worth fixing?" Real projects can produce millions of these remarks, so
you need a tool that can filter, aggregate, and visualize them. That's what
llvm-advisor does.

This matters whether you're a compiler developer tuning inlining or vectorization heuristics, a user upgrading your toolchain and watching for regressions, or someone making performance-critical source changes. The example here is small for a walkthrough, but the workflow remains the same and similar bottlenecks show up in production code, and a large build can easily emit more remarks than a person can go through by hand. This is where the viewer helps: instead of drowning in raw remark data, you can narrow things down to the handful of diagnostics that actually point at a problem.

Let's walk through the example to see how it works. I took a small C program
that was missing a vectorization opportunity, used llvm-advisor to find the exact
remark, changed one function from `noinline` to `static inline`, and measured
the result.

The work is submitted as a single pull request against `llvm/llvm-project`:

- [`llvm/llvm-project#218692`](https://github.com/llvm/llvm-project/pull/218692): the full optimization-remarks feature on top of the `minimal-advisor` foundation.

---

## The workflow: turning a missed vectorization into a faster loop

### The baseline code

Here is `baseline.c`. The helper `work` is marked `__attribute__((noinline))`,
so the loop in `process` has to call it on every iteration.

```c
#include <stdlib.h>

__attribute__((noinline))
int work(int x) {
  return x * 3 + (x >> 2);
}

void process(int *a, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = work(a[i]);
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 10000000;
  int *a = (int *)malloc(n * sizeof(int));
  for (int i = 0; i < n; ++i)
    a[i] = i & 0xff;
  for (int run = 0; run < 10; ++run)
    process(a, n);
  int sum = 0;
  for (int i = 0; i < n; ++i)
    sum += a[i];
  free(a);
  return sum & 1;
}
```

Compile it with remarks enabled:

```bash
clang -c baseline.c -O3 -g -fsave-optimization-record
```

This produces `baseline.opt.yaml` next to the object file.

### Import the remark file into llvm-advisor

```bash
rm -rf /tmp/adv-baseline
llvm-advisor import baseline.opt.yaml \
  --source-root . \
  --store /tmp/adv-baseline \
  --capability-dir /path/to/llvm-project/llvm/tools/llvm-advisor/config/capabilities
```

Then start the server:

```bash
llvm-advisor serve \
  --store /tmp/adv-baseline \
  --port 8080 \
  --capability-dir /path/to/llvm-project/llvm/tools/llvm-advisor/config/capabilities
```

Open `http://127.0.0.1:8080` and press `g r` for the remarks view. The summary
card shows 137 remarks, and the top pass/name table immediately surfaces the
problem:

| Pass           | Remark               | Total | Missed | Passed | Analysis |
| -------------- | -------------------- | ----: | -----: | -----: | -------: |
| loop-vectorize | CantVectorizeLibcall |    11 |     11 |      0 |        0 |
| loop-vectorize | MissedDetails        |    11 |     11 |      0 |        0 |

The heatmap shows the same thing grouped by function and line:

![llvm-advisor heatmap for baseline.c showing process at baseline.c:10 with missed vectorization remarks](/img/2026-08-23-gsoc-llvm-advisor-remarks-demo-heatmap.png)

And the full remarks grid shows the row for `process`:

![llvm-advisor remarks view showing CantVectorizeLibcall and MissedDetails for the process loop](/img/2026-08-23-gsoc-llvm-advisor-remarks-demo-remarks.png)

### Code Explorer: reading the source with the remark overlay

Press `g e` (or click **Code Explorer** in the sidebar), select `baseline.c`,
and filter the pass dropdown to `loop-vectorize`. The loop header on line 9 and
the assignment on line 10 are both highlighted with 11 missed remarks, exactly
matching `baseline.opt.yaml`:

![llvm-advisor Code Explorer for baseline.c with the loop-vectorize pass filter active](/img/2026-08-23-gsoc-llvm-advisor-remarks-demo-code-explorer.png)

This is the same data as the remarks grid, but now it sits directly on the
source line that produced it.

### The raw remark

If you open `baseline.opt.yaml`, the relevant entry is:

```yaml
---
!Missed
Pass: loop-vectorize
Name: CantVectorizeLibcall
DebugLoc: { File: "baseline.c", Line: 10, Column: 12 }
Function: process
Args:
  - String: "loop not vectorized: "
  - String: "call instruction cannot be vectorized"
```

Line 10 is the `a[i] = work(a[i])` assignment. The loop cannot be vectorized
because it contains a function call that the compiler is not allowed to inline.

### The fix

Remove the `noinline` attribute and make `work` `static inline` so the compiler
can fold the call into the loop body and then vectorize it:

```c
#include <stdlib.h>

static inline int work(int x) {
  return x * 3 + (x >> 2);
}

void process(int *a, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = work(a[i]);
}

/* main unchanged */
```

Recompile, re-import into a new snapshot, and look at the remarks again. The
top pass/name table now shows:

| Pass           | Remark     | Total | Missed | Passed | Analysis |
| -------------- | ---------- | ----: | -----: | -----: | -------: |
| loop-vectorize | Vectorized |    13 |      0 |     13 |        0 |

![llvm-advisor remarks view after the fix, showing loop-vectorize Vectorized for the process loop](/img/2026-08-23-gsoc-llvm-advisor-remarks-demo-optimized-remarks.png)

The raw remark in `optimized.opt.yaml` confirms the loop was vectorized:

```yaml
---
!Passed
Pass: loop-vectorize
Name: Vectorized
DebugLoc: { File: "optimized.c", Line: 8, Column: 3 }
Function: process
Args:
  - String: "vectorized "
  - String: ""
  - String: "loop (vectorization width: "
  - VectorizationFactor: "4"
  - String: ", interleaved count: "
  - InterleaveCount: "2"
  - String: ")"
```

It's a small change, but it makes a real difference, and llvm-advisor helps point out
the remarks worth fixing.

### Snapshot diff: before and after

Import both remark files into the same store and use the Compare view (`g c`):

```bash
rm -rf /tmp/adv-compare
mkdir baseline optimized
cp baseline.opt.yaml baseline/vectorize.opt.yaml
cp optimized.opt.yaml optimized/vectorize.opt.yaml

llvm-advisor import baseline/vectorize.opt.yaml \
  --source-root baseline \
  --store /tmp/adv-compare \
  --capability-dir /path/to/llvm-project/llvm/tools/llvm-advisor/config/capabilities

llvm-advisor import optimized/vectorize.opt.yaml \
  --source-root optimized \
  --store /tmp/adv-compare \
  --capability-dir /path/to/llvm-project/llvm/tools/llvm-advisor/config/capabilities

llvm-advisor serve \
  --store /tmp/adv-compare \
  --port 8080 \
  --capability-dir /path/to/llvm-project/llvm/tools/llvm-advisor/config/capabilities
```

Select the baseline snapshot as **BASE** and the optimized snapshot as
**CANDIDATE**. The diff shows one changed unit and an Optimization Impact table
that breaks down the per-function change. In this run `main` has new missed and
passed remarks from the unrolled loop, while `work` is resolved because the call
can now be inlined:

![llvm-advisor Compare view showing the baseline vs optimized snapshot diff](/img/2026-08-23-gsoc-llvm-advisor-remarks-snapshot-diff.png)

The diff answers the question "What changed in the remarks between the two
builds?" without re-parsing files or writing a custom script.

This diff workflow pinpoints exactly which remarks shifted. On large projects,
remarks can be overwhelming, but filtering, aggregation, and PGO hotness help you
prioritize which ones to investigate.

---

## PGO hotness: which remark matters most?

The vectorization example is tiny. In a real program, dozens of files can emit missed-remark warnings. PGO adds a profile-derived `Hotness` value to
remarks, which gives you workload context and tells you which ones are worth fixing first.

### Small walkthrough: hot and cold loops

To make the contrast easy to see, this small program calls `hot_work` one
million times and `cold_work` one hundred times:

```c
#include <stdio.h>
#include <stdlib.h>

int hot_work(int *a, int n) {
  int s = 0;
  for (int i = 0; i < n; ++i)
    s += a[i] * 2;
  return s;
}

int cold_work(int *a, int n) {
  int s = 0;
  for (int i = 0; i < n; ++i)
    s += a[i] * 3;
  return s;
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 100;
  int *a = (int *)malloc(n * sizeof(int));
  for (int i = 0; i < n; ++i)
    a[i] = i & 0xff;

  int total = 0;
  for (int run = 0; run < 1000000; ++run)
    total += hot_work(a, n);
  for (int run = 0; run < 100; ++run)
    total += cold_work(a, n);

  printf("%d\n", total);
  free(a);
  return 0;
}
```

Generate a profile, then compile with profile use and remarks enabled:

```bash
clang -fprofile-generate -O3 -g -fsave-optimization-record pgo_example.c -o pgo_train
./pgo_train
llvm-profdata merge default.profraw -o pgo_example.profdata
clang -fprofile-use=pgo_example.profdata -O3 -g -fsave-optimization-record \
  -c pgo_example.c -o pgo_example.o
```

Import `pgo_example.opt.yaml` and open the heatmap (`g h`). Clang inlines both
helpers into `main`, so the hot loop at lines 6 and 7 appears under `main` and
reaches 100% of the heatmap's normalized scale. The `cold_work` loop is marked
Low.

![llvm-advisor heatmap for pgo_example.c showing the hot loop at 100% normalized hotness and the cold loop at low hotness](/img/2026-08-23-gsoc-llvm-advisor-remarks-pgo-heatmap.png)

In Code Explorer, filter by `loop-vectorize` to inspect the remarks on both
loops:

![llvm-advisor Code Explorer for pgo_example.c with the loop-vectorize pass filter active](/img/2026-08-23-gsoc-llvm-advisor-remarks-pgo-explorer.png)

With hotness, you can sort by actual runtime cost instead of by file name.

---

## How the tool makes this possible

Three new C++ capability analyzers turn raw remark files into the views above:

| Capability                | Output                                                                                                |
| ------------------------- | ----------------------------------------------------------------------------------------------------- |
| `llvm.remarks.relational` | A columnar table of every remark, with deduplicated string tables for function, file, pass, and name. |
| `llvm.remarks.hotspot`    | Per-function, per-file, per-line aggregations used by the heatmap.                                    |
| `llvm.remarks.diff`       | A delta of added, removed, and changed remarks between two snapshots.                                 |

The relational endpoint uses integer-indexed columns instead of an array of
objects. That keeps JSON payloads small and makes browser-side filtering fast
for millions of rows.

![High-level data flow from remark files through analyzers to the web UI](/img/2026-08-23-gsoc-llvm-advisor-remarks-architecture.svg)

The backend is C++ because it is much faster than the initially planned Python ctypes prototype. The
C++ path is in-process,
has no new runtime dependencies, and reuses llvm-advisor's CAS cache so
repeated queries are essentially free.

---

## A few performance numbers

On real compiler output the analyzer sustains tens of thousands of remarks per
second:

| Source            | Remarks | Parse time | Throughput |
| ----------------- | ------: | ---------: | ---------: |
| `zstd_lazy.c`     |  41,537 |      3.7 s | 11,265 r/s |
| `sqlite3` amalgam | 429,075 |       27 s | 15,655 r/s |

Scale test: 572 translation units produced 10.7 million remarks in a single snapshot;
the first relational query OOM-killed the server. Three fixes (splitting HTTP sends,
per-unit fetching, streaming merger) reduced peak RSS from 10.7 GB to 5.3 GB.

![Peak RSS across the three memory fixes](/img/2026-08-23-gsoc-llvm-advisor-remarks-peak-rss.svg)

Those numbers are for YAML remark files. For large projects, Clang's bitstream remark format (`-pass-remarks-format=bitstream`) is worth using instead:

| Format    | Source                         | Remarks | Size   | Parse time |
| --------- | ------------------------------ | ------- | ------ | ---------- |
| YAML      | `llvm/lib/Support/APFloat.cpp` | 18,003  | ~20 MB | ~22 s      |
| Bitstream | `llvm/lib/Support/APFloat.cpp` | 18,003  | ~2 MB  | <1 s       |

Bitstream avoids text parsing entirely, it's already in a binary form the advisor can read directly. The gap compounds across a whole build. Building llvm-project with `-fsave-optimization-record=bitstream` across its ~800 translation units produces tens of millions of remarks. As YAML that would be hundreds of gigabytes of text to re-parse on every import; as bitstream it stays in the low single-digit gigabytes and can be memory-mapped, so the advisor can query it without a multi-minute parse step.

## Testing

I added 9 LLVM lit tests under `llvm/test/tools/llvm-advisor/`:

```bash
ninja -C llvm-build-dev llvm-advisor
../llvm-build-dev/bin/llvm-lit -v llvm/test/tools/llvm-advisor
```

All 9 pass on the `minimal-advisor-remarks` branch.

---

## Acknowledgements and links

Thank you to my mentors [Tobias Stadler](https://github.com/tobias-stadler)
and [Henrik Olsson](https://github.com/hnrklssn) for guiding me
through the summer, and to the LLVM community for the opportunity
to contribute.

Special thanks to last year's GSoC team: [Miguel Cárdenas](https://github.com/miguelcsx), with mentors [Johannes Doerfert](https://github.com/jdoerfert) and [Kevin Sala](https://github.com/kevinsala),
for starting llvm-advisor ([his post](https://blog.llvm.org/posts/2025-08-31-gsoc-llvm-advisor/))
and for the initial foundation this work builds on.

- Pull Request: [`llvm/llvm-project#218692`](https://github.com/llvm/llvm-project/pull/218692)
- GSoC 2026 final report: [gist](https://gist.github.com/kamini08/8ab23822d525856c44ad8315f9f39c01)
