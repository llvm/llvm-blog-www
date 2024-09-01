---
author: "Manuel Brito"
date: "2024-09-01"
tags: ["GSoC", "optimization"]
title: "GSoC 2024: Reviving NewGVN"
---

This summer I participated in GSoC under the LLVM Compiler Infrastructure. The goal of the project was to improve the NewGVN pass so that it can replace GVN as the main value numbering pass in LLVM.

# Motivation

NewGVN was introduced in 2016 to replace GVN. NewGVN improves upon GVN by not relying on MemoryDependenceAnalysis to detect redundancies among memory operations, using a sparse formulation, being complete for loops, among other aspects. However, it is still not widely used, mainly because it lacks partial redundancy elimination (PRE) and is bug-ridden.

# Implementing PRE

Our main contribution was the development of a PRE stage for NewGVN. Our solution relied on generalizing Phi-of-Ops. It performs a special case of PRE where the instruction depends on a phi instruction, and an equivalent value is available on every reaching path. Our generalization eliminated the need for a dependent phi and introduced the ability to insert the missing values in cases where the instruction is partially redundant.

Integrating PRE into the existing framework also allowed us to gain loop-invariant code motion (LICM) for free. NewGVN is complete for loops because when it first processes loops, it assumes that only the first iteration will be executed, later corroborating these assumptions—this is known as the optimistic assumption. The optimistic assumption, combined with PRE, allows it to speculatively hoist instructions out of loops. On the other hand, LICM in GVN relies on using LoopInfo.

# Missing Features 

The two main features our PRE implementation lacks are critical edge splitting and load coercion. Critical edge splitting is required to ensure that we do not insert instructions into paths where they won't be used. Currently, our implementation simply bails in such cases. Load coercion allows us to detect equivalences of loaded values with different types, such as loads of `i32` and `float`, and then coerce the loaded type using conversion operations.

The difficulty in implementing these features is that NewGVN is designed to perform analysis and transformation in separate steps, while these features involve modifying the function during the analysis phase.

# Results

We evaluated our implementation using a set of 20 benchmarks. Despite the missing features, we observed an average improvement of 0.4% over GVN. It is also important to mention that our solution hasn't been fine-tuned to consider the rest of the optimization pipeline, which resulted in some cases where our implementation regressed in comparison to both GVN and the existing NewGVN.


# Future Work

In the future, we plan to implement the aforementioned missing features and fine-tune the heuristics for when to perform PRE to prevent the regressions discussed in the results section. Once these issues are addressed, we'll upstream our implementation, bringing us a step closer to reviving NewGVN.