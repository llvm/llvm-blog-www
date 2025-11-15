---
author: "Miguel Rosas"
date: "2025-03-31"
tags: ["Tuning", "clang", "gcc"]
title: "Effective Tuning of Automatically Parallelized OpenMP Applications Using The State of the art Parallel Optimizing Compiler"
---


# Effective Tuning of Automatically Parallelized OpenMP Applications Using The State of the art Parallel Optimizing Compiler.

# Introduction

Automatic parallelization, still cannot achieve the required performance to be considered a true alternative to hand parallelization. However, when combined with effective tuning techniques, it provides a promising alternative to manual parallelization of sequential programs by leveraging the computational potential of modern multi-core architectures. While automatic parallelization focuses on identifying potential parallelism in code, tuning systems refine performance by optimizing efficient parallel code segments and serializing inefficient ones based on runtime metrics.

This work aims to answer an important question for the compiler community: Can effective tuning techniques in Automatic Parallelizers enable applications to consistently match or exceed the performance of manually parallelized implementations?


# Motiviation Problem

- Compiler optimizations are both program- and platform-dependent.  
- The large number of individual optimizations can be on or off, resulting in a combinatorial explosion.  
- Some optimizations have multiple sub-levels, adding more dimensions to the search space.

As previously mentioned, choosing the right combination is complex because the search space increases exponentially. Consider a scenario where an optimization's impact is evaluated by toggling it on or off. For this purpose, a vector **Z** is defined whose elements **Zi** represent a given optimization. Each optimization **Zi** can be enabled (1) or disabled (0), which is represented as:

\[|Zi| = \{ 0, 1 \}.\]

However, when multiple optimizations are considered, the vector extends to:

\[|Zi| = \{ 0, 1 \}^N,\]

where **N** represents the number of optimizations being evaluated. Consequently, the search space comprises \(2^N\) combinations. For example, with 11 different optimizations, the search space contains \(2^{11} = 2048\) different configurations.

The complexity further escalates when certain optimizations possess multiple sublevels instead of a binary state. For instance, an optimization like inlining expansion might offer 5 distinct sublevels. In such cases, the vector is redefined as:

\[|Zi| = \{ 0, 1, 2, 3, 4,... K \}^N,\]

where **K** represents the number of sublevels for that optimization. This exponential growth in the search space is a well-documented challenge in the state-of-the-art literature.

The combinatorial explosion in optimization settings is a central challenge in iterative compilation and auto-tuning research. Systematic and heuristic search techniques, sometimes enhanced by machine learning, are essential to navigate this vast space efficiently without resorting to brute-force methods.

---

# Challenge Search Space  

- Evaluating the vast number of optimization variants at runtime is non-trivial.
- The performance impact of a single optimization is often interdependent with other optimizations.
- The concept of “window size” is introduced to manage groups of loops or code sections, reducing the overall complexity.


# The Cetus Automatic Parallelizer  
**Workflow:**  
1. **Input:**  C code.  
2. **Processing:** Cetus automatically annotates the code with OpenMP directives.  
3. **Compilation:** The annotated code is compiled with GCC or Clang to produce a binary.

The Cetus tool provides an infrastructure for research on multi-core compiler optimizations that emphasizes automatic parallelization. 

# Portable Tuning Framework  

- The framework iterates over the optimization space, generating and testing program variants.
- It combines traditional compiler flags (e.g., `-O3`) with specific Cetus optimizations to form diverse binaries.
- Performance metrics guide the selection of the best-performing configuration.

We maded used of al algoirthm known as Combine Elimination Algorithm (CE)

# Combine Elimination Algorithm (CE)  
**Algorithm Overview:**  
1. **Baseline (B):** Begin with all optimizations enabled.
2. **Measurement:** Record the baseline execution time (TB).
3. **Relative Improvement Percentage (RIP):** Evaluate the impact of disabling each optimization.
4. **Iterative Removal:** Remove the optimization that most negatively affects performance, updating the baseline iteratively.


# Experimental Setup  
**Summary:**  
- **Compilers:** GCC 17.0.6 and Clang 12.2.0.
- **Benchmarks:** NAS (Class B) and PolyBench (Large Dataset).
- **Hardware:** 16-core Intel Xeon Gold 6230.
- **Compiler Flags:** All experiments use `-O3` in conjunction with 8 distinct Cetus optimizations.

# The results 
- Demonstrates how varying the window size for loop transformations affects performance.
- Indicates that optimal window size is critical and must be empirically determined.

# Conclusions and Future Work  
- **Compiler Variability:** GCC and Clang exhibit different optimization behaviors, influenced by their runtime libraries and heuristics.
- **Performance Outcomes:** Clang outperformed GCC in 5 of the 6 evaluated applications.
- **Application-Specific Tuning:** The optimal window size and set of optimizations are highly application-dependent.
- **Future Directions:** Emphasis on integrating machine learning techniques to better navigate the enormous search space and further refine auto-tuning strategies.

# CGO EXPERIENCE
Receiving the LLVM Foundation Travel Grant significantly supported my research and deepen my involvement with the LLVM community. My work explores the integration of AI-driven models into compiler optimizations, with a focus on evaluating and improving optimization techniques. Attending CGO 2025 was crucial for me to present my findings, gain valuable feedback, meeting new people, and engage with experts working on LLVM, MLIR, and AI-assisted compiler transformations. I really enjoyed the conference and I learned a lot!





