---
author: Tanya Lattner
date: "2022-09-30"
tags: ["llvmdevmtg", "foundation"]
title: Announcing the 2022 LLVM Developers' Meeting Program
---

![2022 LLVM Dev Mtg Logo](/img/2022-LLVMDevMtgLogo.jpg)

We had an amazing group of talk proposals submitted for the [2022 LLVM Developers' Meeting](https://llvm.swoogo.com/2022devmtg/2359289). Thank you to all that submitted a talk proposal this year! 

Here is the 2022 LLVM Developers' Meeting program:

**Keynotes:**

* Paths towards unifying LLVM and MLIR - Nicolai Hähnle
* Implementing Language Support for ABI-Stable Software Evolution in Swift and LLVM - Doug Gregor

**Technical Talks:**

* Implementing the Unimplementable: Bringing HLSL's Standard Library into Clang - Chris Bieneman
* Heterogeneous Debug Metadata in LLVM - Scott Linder
* Clang, Clang: Who's there? WebAssembly!	- Paulo Matos
* MC/DC: Enabling easy-to-use safety-critical code coverage analysis with LLVM - Alan Phipps
* What does it take to run LLVM Buildbots? - David Spickett
* llvm-gitbom: Building Software Artifact Dependency Graphs for Vulnerability Detection - Bharathi Seshadri, Yongkui Han
* CuPBoP: CUDA for Parallelized and Broad-range Processors - Ruobing Han
* Uniformity Analysis for Irreducible CFGs - Sameer Sahasrabuddhe
* Using Content-Addressable Storage in Clang for Caching Computations and Eliminating Redundancy - Steven Wu, Ben Langmuir
* Direct GPU Compilation and Execution for Host Applications with OpenMP Parallelism - Shilei Tian, Joseph Huber
* Linker Code Size Optimization for Native Mobile Applications - Gai Liu
* Minotaur: A SIMD Oriented Superoptimizer - Zhengyang Liu
* ML-based Hardware Cost Model for High-Level MLIR - Dibyendu Das, Sandya Mannarswamy
* VAST: MLIR for program analysis of C/C++ - Henrich Lauko
* MLIR for Functional Programming - Siddharth Bhat
* SPIR-V Backend in LLVM: Upstream and Beyond - Michal Paszkowski, Alex Bezzubikov
* IRDL: A Dialect for dialects - Mathieu Fehr, Théo Degioanni
* Automated translation validation for an LLVM backend - Nader Boushehrinejad Moradi
* llvm-dialects: bringing dialects to the LLVM IR substrate - Nicolai Hähnle
* YARPGen: A Compiler Fuzzer for Loop Optimizations and Data-Parallel Languages - Vsevolod Livinskii
* RISC-V Sign Extension Optimizations - Craig Topper
* Execution Domain Transition: Binary and LLVM IR can run in conjunction - Jaeyong Ko

**Tutorials:**
* Using LLVM's libc - Sivachandra Reddy, Michael Jones, Tue Ly
* How to implement a new JITLink backend in a week - Sunho Kim

**Panels (some speakers still to be announced):**
* Machine Learning Guided Optimizations (MLGO) in LLVM
* Static Analysis in Clang - Gabor Horvath, Artem Dergachev, Bruno Cardoso Lopes
* High-level IRs for a C/C++ Optimizing Compiler - Bruno Lopes, Ivan Baev, Johannes Doerfert, Mehdi Amini
* Panel discussion on “Best practices with toolchain release and maintenance” - Aditya Kumar

**Student Technical Talks:**
* Merging Similar Control-Flow Regions in LLVM for Performance and Code Size Benefits - Charitha Saumya
* Alive-mutate: a fuzzer that cooperates with Alive2 to find LLVM bugs - Yuyou Fan
* Enabling Transformers to Understand Low-Level Programs - Zifan Guo, William S. Moses
* LAGrad: Leveraging the MLIR Ecosystem for Efficient Differentiable Programming - Mai Jacob Peng
* Scalable Loop Analysis - Vir Narula

**Quick Talks:**
* LLVM Education Initiativei - Chris Bieneman, Kit Barton, Mike Edwards
* Enabling AArch64 Instrumentation Support In BOLT - Elvina Yakubova
* Approximating at Scale: How strto<float> in LLVM’s libc is faster -  Michael Jones
* MIR support in llvm-reduce - Matthew Arsenault
* Interactive Crashlogs in LLDB - Med Ismail Bennani
* clang-extract-api: Clang support for API information generation in JSON - Zixu Wang
* Using modern CPU instructions to improve LLVM's libc math library. - Tue Ly
* Challenges Of Enabling Golang Binaries Optimization By BOLT - Vasily Leonenko,  Vladislav Khmelevskyi
* Inlining for Size -  Kyungwoo Lee, Ellis Hoag, Nathan Lanza
* Automatic indirect memory access instructions generation for pointer chasing patterns - Przemysław Ossowski
* Link-Time Attributes for LTO: Incorporating linker knowledge into the LTO recompile -  Todd Snider
* Expecting the expected: Honoring user branch hints for code placement optimizations - Stan Kvasov, Vince Del Vecchio
* CUDA-OMP — Or, Breaking the Vendor Lock - Johannes Doerfert, Joseph Huber
* Thoughts on GPUs as First-Class Citizens - Johannes Doerfert, Shilei Tian, Joseph Huber
* Building an End-to-End Toolchain for Fully Homomorphic Encryption with MLIR - Alexander Viand

**Lightning Talks:**
* LLVM Office Hours: addressing LLVM engagement and contribution barriers - Kristof Beyls
* Improved Fuzzing of Backend Code Generation in LLVM - Yuyang Rong
* Interactive Programming for LLVM TableGen - David Spickett
* Efficient JIT-based remote execution - Anubhab Ghosh
* FFTc: An MLIR Dialect for Developing HPC Fast Fourier Transform Libraries - Yifei He
* Recovering from Errors in Clang-Repl and Code Undo - Purva Chaudhari, Jun Zhang
* 10 commits towards GlobalISel for PowerPC - Kai Nacke, Amy Kwan
* Nonstandard reductions with SPRAY - Jan Hueckelheim, Johannes Doerfert
* Type Resugaring in Clang for Better Diagnostics and Beyond - Matheus Izvekov
* Swift Bindings for LLVM - Egor Zhdan
* Min-sized Function Coverage with IRPGO - Ellis Hoag, Kyungwoo Lee
* High-Performance GPU-to-CPU Transpilation and Optimization via High-Level Parallel Constructs in Polygeist/MLIR - William S. Moses,  Ivan R. Ivanov
* Tools for checking and writing non-trivial DWARF programs - Chris Jackson
* Analysis of RISC-V Vector Performance Using MCA Tools - Michael Maitland
* Optimizing Clang with BOLT using CMake - Amir Ayupov
* Exploring OpenMP target offloading for the GraphCore architecture - Jose M Monsalve Daiz

**Posters (more posters to be announced at a later date):**
* Removal of Undef: Move Uninitialized Memory to Poison - John McIver
* Optimizing Julia's ORC JIT - Prem Chintalapudi
* An LLVM-Based Compiler for Quantum-Classical Applications - Xin-Chuan Wu
* Specializing Code to New Architectures via Dynamic Adaptive Recompilation - Quinn Pham, Dhanrajbir Singh Hira
* LLFPTrax: Tracking ill-conditioned floating-point inputs using relative error amplification in LLVM - Tanmay Tirpankar
* LLVM continuous upstream integration and testing - Jay Azurin, Keerthana Subramani
* Automatic indirect memory access instructions generation for pointer chasing patterns - Adam Perdeusz

Thank you to the volunters on the Program Committee for all of their hard work and time spent reviewing proposals. A special thanks also goes out to this year's chair - Anton Korobeynikov. Here is the complete 2022 LLVM Developers' Meeting Program Committee:

* Kristof Beyls
* Andrey Bokhanko
* Chelsea Cassanova
* Johannes Doerfert
* Florian Hahn
* Petr Hosek
* Min-Yih Hsu
* Anton Korobeynikov (Chair)
* Aditya Kumar
* Hem Neema
* Diego Novillo
* Fangrui Song
* J. Ryan Stinnett
* Caroline Tice
* Mircea Trofin

Registration closes on October 31st, so register today for the [2022 LLVM Developers' Meeting](https://llvm.swoogo.com/2022devmtg/2359289).


