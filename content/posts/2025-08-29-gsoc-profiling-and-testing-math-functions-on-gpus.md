---
author: "Leandro Augusto Lacerda Campos"
date: "2025-08-29"
tags: ["gsoc", "libc", "profiling", "testing", "math", "gpu"]
title: "GSoC 2025: Profiling and Testing Math Functions on GPUs"
---

With the increasing importance of GPU computing, having a robust and familiar C standard library becomes a valuable asset for developers. The LLVM project is actively working to provide this foundation, as a solid libc implementation enables more complex libraries to be developed for and used on GPUs.

A key part of this effort is providing the C standard math library (LLVM-libm) on GPUs, often reusing the same target-agnostic implementations developed for CPU targets. This context creates a twofold challenge. First, there is a need to systematically **verify the conformance** of these implementations to standards like OpenCL. Second, it is crucial to **benchmark their performance** against the highly-optimized vendor libraries to understand the trade-offs involved.

This Google Summer of Code 2025 project was designed to address both challenges by developing a framework for conformance testing as well as refining and expanding the existing benchmarking infrastructure. The work provides two benefits to the LLVM community:

* It **empowers libc contributors** with a robust tool to validate their GPU math function implementations.
* It **builds trust with end-users** by providing transparent accuracy and performance data.

This report details the work completed on this project.

## Conformance Testing

To address the goal of accuracy verification, I implemented a C++ framework for conformance testing within the [`offload/unittests/Conformance`](https://github.com/llvm/llvm-project/tree/main/offload/unittests/Conformance) directory. This framework was designed to be extensible, easy to use, and capable of testing various implementation providers (`llvm-libm`, `cuda-math`, `hip-math`) across different hardware platforms (AMD, NVIDIA).

### Key Components

The framework's power and simplicity come from a few key components that work together:

* **`DeviceContext`**: A lightweight wrapper around the new Offload API that abstracts away the low-level details of device discovery, resource management, and kernel launching.
* **`InputGenerator`**: An extensible interface for test input generation. The framework provides two concrete implementations:
  * **`ExhaustiveGenerator`**: Used for functions with small input spaces (e.g., half-precision functions and single-precision univariate functions), this generator iterates over every representable point in a given space, ensuring complete coverage.
  * **`RandomGenerator`**: Used for functions with large input spaces (e.g., single-precision bivariate and double-precision functions), this generator produces a massive, deterministic stream of random points to sample the space thoroughly.
* **`GpuMathTest`**: The main test harness class that orchestrates the entire process. It manages loading the correct GPU binary, setting up device buffers, invoking the generator, launching the kernel, and triggering the verification process.
* **`HostRefChecker`**: After the GPU computation is complete, this component calculates the expected result for each input on the host CPU (using the correctly rounded LLVM-libm's implementations) and computes the ULP (Units in the Last Place) distance to the actual result from the GPU.

This architecture makes writing a new, complete test incredibly simple and concise. For example, a full exhaustive test for the `expf` function requires only a few lines of code:

```cpp
#include "mathtest/TestRunner.hpp"
// ... other includes

// 1. Configure the test for the `expf` function.
namespace mathtest {
template <> struct FunctionConfig<expf> {
  static constexpr llvm::StringRef Name = "expf";
  static constexpr llvm::StringRef KernelName = "expfKernel";
  // ULP tolerance sourced from the OpenCL C Specification
  static constexpr uint64_t UlpTolerance = 3;
};
} // namespace mathtest

// 2. Define the main function to run the test
int main(int argc, const char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "...");

  // 3. Define the input space and select the generator
  mathtest::IndexedRange<float> Range;
  mathtest::ExhaustiveGenerator<float> Generator(Range);

  // 4. Run the tests against all configured providers
  bool Passed = mathtest::runTests<expf>(
      Generator, mathtest::cl::getTestConfigs(), DEVICE_BINARY_DIR);

  return Passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

### Contributions

This part of the project was submitted to the LLVM project through a series of pull requests, which can be grouped into the following categories:

* **Framework Creation and Evolution**
  * [#149242](https://github.com/llvm/llvm-project/pull/149242): Add framework for math conformance tests on GPUs
  * [#151714](https://github.com/llvm/llvm-project/pull/151714): Build device code as C++
  * [#152362](https://github.com/llvm/llvm-project/pull/152362): Add support for CUDA Math and HIP Math providers
  * [#154252](https://github.com/llvm/llvm-project/pull/154252): Add RandomGenerator for large input spaces
* **Adding Test Coverage**
  * [#152013](https://github.com/llvm/llvm-project/pull/152013): Add tests for single-precision math functions
  * [#154663](https://github.com/llvm/llvm-project/pull/154663): Add randomized tests for single-precision bivariate math functions
  * [#155003](https://github.com/llvm/llvm-project/pull/155003): Add randomized tests for double-precision math functions
  * [#155112](https://github.com/llvm/llvm-project/pull/155112): Add exhaustive tests for half-precision math functions
* **Enabling Work and Ecosystem Improvements**
  * Enabling Functions in `libc`: [#151841](https://github.com/llvm/llvm-project/pull/151841), [#152157](https://github.com/llvm/llvm-project/pull/152157), [#154857](https://github.com/llvm/llvm-project/pull/154857), [#155060](https://github.com/llvm/llvm-project/pull/155060)
  * Infrastructure and Dependencies: [#150083](https://github.com/llvm/llvm-project/pull/150083), [#150140](https://github.com/llvm/llvm-project/pull/150140), [#151820](https://github.com/llvm/llvm-project/pull/151820)
* **Documentation**
  * [#155190](https://github.com/llvm/llvm-project/pull/155190): Add README file

### Accuracy Results

The primary deliverable of the conformance testing work is a comprehensive set of accuracy data. The framework reports the maximum observed ULP (Units in the Last Place) distance for a wide range of functions across three providers: `llvm-libm`, `cuda-math`, and `hip-math`.

The table below presents a sample of these results for a few selected single-precision functions, all tested exhaustively. The tests were run on an AMD gfx1030 and an NVIDIA RTX 4000 SFF Ada Generation GPU, with ULP tolerances based on the OpenCL C specification.

#### Exhaustive Test Results for Selected Single-Precision Univariate Math Functions

<table>
  <thead>
    <tr>
      <th rowspan="2" style="text-align: left;">Function</th>
      <th rowspan="2" style="text-align: center;">ULP Tolerance</th>
      <th colspan="4" style="text-align: center;">Max ULP Distance</th>
    </tr>
    <tr>
      <th style="text-align: center;">llvm-libm<br>(AMDGPU)</th>
      <th style="text-align: center;">llvm-libm<br>(CUDA)</th>
      <th style="text-align: center;">cuda-math<br>(CUDA)</th>
      <th style="text-align: center;">hip-math<br>(AMDGPU)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td style="text-align: left;">cosf</td>
      <td style="text-align: center;">4</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">2</td>
      <td style="text-align: center;">2</td>
    </tr>
    <tr>
      <td style="text-align: left;">expf</td>
      <td style="text-align: center;">3</td>
      <td style="text-align: center;">0</td>
      <td style="text-align: center;">0</td>
      <td style="text-align: center;">2</td>
      <td style="text-align: center;">1</td>
    </tr>
    <tr>
      <td style="text-align: left;">logf</td>
      <td style="text-align: center;">3</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">2</td>
    </tr>
    <tr>
      <td style="text-align: left;">sinf</td>
      <td style="text-align: center;">4</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">1</td>
      <td style="text-align: center;">2</td>
    </tr>
    <tr>
      <td style="text-align: left;">tanf</td>
      <td style="text-align: center;">5</td>
      <td style="text-align: center;">0</td>
      <td style="text-align: center;">0</td>
      <td style="text-align: center;">3</td>
      <td style="text-align: center;">2</td>
    </tr>
  </tbody>
</table>

The complete accuracy results for all tested functions (including half, single, and double precision) will be published on the official LLVM-libc GPU [Supported Functions](https://libc.llvm.org/gpu/support.html#math-h) page. For users and future contributors interested in running existing or adding new tests, detailed instructions are available in the project's [`README.md`](https://github.com/llvm/llvm-project/blob/main/offload/unittests/Conformance/README.md) file.

## Performance Profiling

Alongside accuracy, performance is a critical metric for a GPU math library. The second major goal of this project was to refine and expand the existing benchmarking framework to enable fair, reproducible, and insightful performance comparisons between LLVM-libc and vendor-optimized libraries. This effort involved overcoming several subtle challenges and significantly refactoring the infrastructure.

### Key Enhancements

The path to reliable performance data involved a series of foundational improvements to ensure that the results are fair, statistically sound, and reproducible. Key enhancements included:

* **Reproducibility and Fairness**: The initial framework was enhanced with a deterministic, per-thread random number generator (PRNG) to ensure that LLVM-libc and vendor libraries are compared using the exact same input sequences. Additionally, to prevent misleading results caused by compiler optimizations, loop unrolling was explicitly disabled in the throughput measurement loop. This change prevents the compiler from aggressively optimizing the transparent libc code in a way that isn't possible for vendor libraries, ensuring a true apples-to-apples comparison.
* **Statistical Soundness**: The framework's statistical calculations were improved. The standard deviation is now computed correctly using a sum-of-squares approach, and results from multiple GPU threads are aggregated using a statistically sound pooled mean and variance. The timing logic was also refined to subtract a baseline measurement of the empty benchmark loop, isolating the true cost of the function call.
* **Flexible Input Generation and New Benchmarks**: To support a wider range of functions, the framework was refactored with a pluggable input generation system. New distribution classes, `UniformExponent` (for values spanning orders of magnitude) and `UniformLinear` (for linear ranges), were introduced. This new flexibility enabled the addition of a comprehensive suite of benchmarks for the `exp` and `log` families.

### More Contributions

The contributions that refined and expanded the benchmarking infrastructure were submitted in the following pull requests: [#153512](https://github.com/llvm/llvm-project/pull/153512), [#153900](https://github.com/llvm/llvm-project/pull/153900), [#153971](https://github.com/llvm/llvm-project/pull/153971), and [#155727](https://github.com/llvm/llvm-project/pull/155727).

### Performance Results

As an example, see below part of the output from the `log` function benchmark on an NVIDIA RTX 4070 Laptop GPU. It highlights interesting performance characteristics. Notice the exceptionally low and nearly constant cycles per call for NVIDIA's `__nv_logf`. Its IR reveals this is due to a compact `float`-only routine (a fixed sequence of FMAs plus simple bit manipulations) with Flush-To-Zero enabled and no lookup tables or divergent memory accesses.

```bash
Running Suite: LlvmLibcLogGpuBenchmark
Benchmark                |  Cycles (Mean) |   Stddev |     Min |     Max |     Iterations |  Threads |
------------------------------------------------------------------------------------------------------
LogAroundOne_1           |           1031 |        8 |    1017 |    1082 |           1984 |       32 |
LogAroundOne_128         |            608 |        2 |     604 |     615 |           1984 |       32 |
LogMedMag_1              |           1033 |        6 |    1015 |    1113 |          17024 |       32 |
LogMedMag_128            |            606 |        2 |     603 |     610 |           1344 |       32 |
NvLogAroundOne_1         |           1397 |        5 |    1397 |    1473 |           8480 |       32 |
NvLogAroundOne_128       |           1341 |        0 |    1341 |    1342 |            352 |       32 |
NvLogMedMag_1            |           1403 |        4 |    1403 |    1473 |           8480 |       32 |
NvLogMedMag_128          |           1342 |        0 |    1342 |    1344 |            576 |       32 |
Running Suite: LlvmLibcLogfGpuBenchmark
Benchmark                |  Cycles (Mean) |   Stddev |     Min |     Max |     Iterations |  Threads |
------------------------------------------------------------------------------------------------------
LogfAroundOne_1          |           1047 |        5 |    1035 |    1104 |           5952 |       32 |
LogfAroundOne_128        |            496 |        2 |     492 |     500 |           2880 |       32 |
LogfMedMag_1             |           1047 |        8 |    1035 |    1649 |         258688 |       32 |
LogfMedMag_128           |            495 |        2 |     491 |     498 |           1984 |       32 |
NvLogfAroundOne_1        |             61 |        0 |      61 |      61 |           1344 |       32 |
NvLogfAroundOne_128      |             94 |        0 |      94 |      94 |            576 |       32 |
NvLogfMedMag_1           |             61 |        0 |      61 |      61 |           1344 |       32 |
NvLogfMedMag_128         |             94 |        0 |      94 |      94 |            576 |       32 |
```

## Future Work

The next logical steps include:

* Expanding conformance test coverage to include new higher math functions as they are implemented in LLVM-libm.
* Adding performance benchmarks for more higher math functions.

## Acknowledgements

I would like to express my gratitude to my mentors, Joseph Huber and Tue Ly. I am deeply thankful for their belief in my potential, for their encouragement during the most challenging moments, and for their incredible availability to guide me and review my pull requests, often at night and on weekends. Their mentorship, which was rich with lessons in mathematics, programming, and GPU architecture, was certainly the best part of this experience. I would also like to thank the entire LLVM community for creating a welcoming and collaborative environment.
