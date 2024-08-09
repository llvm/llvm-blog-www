---
author: "James Hu"
date: "2024-08-09"
tags: ["GSoC", "libc", "gpu"]
title: "GSoC 2024: GPU Libc Benchmarking"
---

Hey everyone! My name is James and I worked on LLVM this summer through GSoC. My project is called [GPU Libc Benchmarking](https://summerofcode.withgoogle.com/programs/2024/projects/HkRoz49I). The main objective of this project was to develop microbenchmarking infrastructure for libc on the GPU.

# Background

The LLVM libc project was designed as an alternative to glibc that aims to be modular, configurable, and sanitizer-friendly. Currently, LLVM libc is being ported to Nvidia and AMD GPUs to give libc functionality (e.g. printf(), malloc(), and math functions) on the GPU. As of March 2024, programs can use GPU libc in offloading languages (CUDA, OpenMP) or through direct compilation and linking with the libc library. 

# What We Did

During this project, we developed a microbenchmarking framework that is directly compiled for and run on the GPU, using libc functions to display output to the user. As this was a short project (90 hours), we mostly focused on developing the infrastructure and writing a few example usages (isalnum(), isalpha(), and sin()). 

Our benchmarking infrastructure is based on Google Benchmark and measures the average cycles, minimum, maximum, and standard deviation of each benchmark. Each benchmark is run for multiple iterations to stabilize the results. Benchmark writers can measure against vendor implementations of libc functions by passing specific linking flags to the benchmark’s CMake portion and registering the corresponding vendor function from the benchmark itself.

Below is an example of our benchmarking infrastructure's output for `sinf()`

```c
Benchmark            |  Cycles |     Min |     Max | Iterations | Time / Iteration |   Stddev |  Threads |
----------------------------------------------------------------------------------------------------------
Sinf_1               |     764 |     369 |    2101 |        273 |             7 us |      323 |       32 |
Sinf_128             |     721 |     699 |     744 |          5 |           913 us |       16 |       32 |
Sinf_1024            |     661 |     650 |     689 |          9 |             7 ms |       31 |       32 |
Sinf_4096            |     666 |     663 |     669 |          5 |            28 ms |       28 |       32 |
SinfTwoPi_1          |     372 |     369 |     632 |         70 |             7 us |       39 |       32 |
SinfTwoPi_128        |     379 |     379 |     379 |          4 |           895 us |        0 |       32 |
SinfTwoPi_1024       |     335 |     335 |     338 |          5 |             7 ms |       20 |       32 |
SinfTwoPi_4096       |     335 |     335 |     335 |          4 |            28 ms |        0 |       32 |
SinfTwoPow30_1       |     371 |     369 |     510 |         70 |             7 us |       17 |       32 |
SinfTwoPow30_128     |     379 |     379 |     379 |          4 |           894 us |        0 |       32 |
SinfTwoPow30_1024    |     335 |     335 |     338 |          5 |             7 ms |       20 |       32 |
SinfTwoPow30_4096    |     335 |     335 |     335 |          4 |            28 ms |        0 |       32 |
SinfVeryLarge_1      |     477 |     369 |     632 |         70 |             7 us |       58 |       32 |
SinfVeryLarge_128    |     487 |     480 |     493 |          5 |           900 us |       14 |       32 |
SinfVeryLarge_1024   |     442 |     440 |     447 |          5 |             7 ms |       18 |       32 |
SinfVeryLarge_4096   |     441 |     441 |     442 |          4 |            28 ms |       14 |       32 |
```

Users can register benchmarks similar to Google Benchmark, using a macro: 

```c
uint64_t BM_IsAlnumCapital() {
  char x = 'A';
  return LIBC_NAMESPACE::latency(LIBC_NAMESPACE::isalnum, x);
}
BENCHMARK(LlvmLibcIsAlNumGpuBenchmark, IsAlnumCapital, BM_IsAlnumCapital);
```

# Results

This project met its major goal of creating microbenchmarking infrastructure for the GPU. However, the original scope of this proposal included a CPU component that would use vendor tools to measure GPU kernel properties. However, this was removed after discussion with the mentors due to technical obstacles in offloading specific kernels to the GPU that would require major changes to other parts of the code. 

# Future Work

As this was a short project (90 hours), we only focused on implementing the microbenchmarking infrastructure. Future contributors can use the benchmarking infrastructure to add additional benchmarks. In addition, there are improvements to microbenchmarking infrastructure that could be added, such as more options for user input ranges, better random distributions for math functions, and a CPU element that can launch multiple kernels and compare results against functions running on the CPU.

The existing code can be found in the [LLVM repo](https://github.com/llvm/llvm-project/tree/main/libc/benchmarks/gpu).

# Acknowledgements

This project would not have been possible without my amazing mentor, Joseph Huber, the LLVM Foundation admins, and the GSoC admins.

# Links
[Landed PRs](https://github.com/llvm/llvm-project/commits?author=jameshu15869)

[LLVM GitHub](https://github.com/llvm/llvm-project)

[LLVM Homepage](https://llvm.org/)

[GSoC Project Page](https://summerofcode.withgoogle.com/programs/2024/projects/HkRoz49I)
