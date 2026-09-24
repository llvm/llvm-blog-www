---
title: "Remembering Johannes Doerfert"
author: Tanya Lattner
date: 2026-09-24
tags: ["community", "in-memoriam"]
---

<div style="margin:0 auto;">
  <img src="/img/Johannes.jpeg"><br/>
</div>

It is with great sadness that we share the news of the passing of Johannes Doerfert, on September 17, 2026, at the age of 36, after a battle with cancer. Johannes was one of the most prolific and respected contributors to the LLVM compiler project, and his loss will be deeply felt.

Johannes was born on November 5, 1989. He earned his Ph.D. in computer science from Saarland University in Saarbrücken, Germany, in 2018, where his research focused on applying polyhedral compiler technologies to low-level code. He had been an active LLVM contributor since 2014, working in the compiler design lab of Prof. Sebastian Hack, and became a core developer on the Polly polyhedral-optimization project as early as 2012.

Over the following decade, Johannes built a career at the intersection of compiler research and high-performance computing, most recently as a researcher focused on OpenMP, LLVM, and parallel program optimization.

## Contributions to LLVM

Johannes's worked on many parts of the LLVM Project, and these are just a few of his contributions:

- **The Attributor framework.** Johannes designed and championed the Attributor, LLVM's versatile inter-procedural fixpoint iteration framework for deducing and propagating function and argument attributes across a program. He introduced it to the community at the 2019 LLVM Developers' Meeting, and it has since become an important piece of LLVM's interprocedural optimization infrastructure.
- **OpenMP and GPU offloading.** Johannes became LLVM's code owner for OpenMP target offloading in 2021, leading the compiler and runtime support that lets OpenMP programs run efficiently on GPUs across NVIDIA, AMD, and Intel hardware. His work spanned the OpenMP runtime, just-in-time compilation and link-time optimization for target offloading, and techniques for near-zero-overhead GPU execution.
- **Polly and polyhedral optimization.** Early in his career, Johannes was a core developer of Polly, LLVM's polyhedral loop optimization infrastructure, and published research on polyhedral scheduling in the presence of reductions and on optimistic loop optimization.

He authored or co-authored dozens of papers on compiler optimization, automatic differentiation of GPU kernels, performance portability, and OpenMP.

## Community Building

Johannes helped organize EuroLLVM 2017 in Saarbrücken, Germany, the LLVM-HPC workshop at CGO from 2017 onward, and the LLVM events at ISC starting in 2019, helping join together the LLVM and HPC communities.

He was frequently in attendance at the LLVM Developers' Meeting Newcomer and Community.o sessions. He welcomed newcomers to the LLVM Developers' Meetings and shared his advice and wisdom on how to get more involved in the project.

Johannes also held LLVM office hours on a weekly basis, where he answered questions on OpenMP, LLVM-IR, interprocedural optimizations, Attributor, workshops, research, and more.

## Mentoring the Next Generation

Johannes was a Google Summer of Code mentor for LLVM for several years and helped student contributors on various projects. Here are just a few:

- **2016**
  - Polly as an Analysis Pass in LLVM
- **2019**
  - Improve (function) attribute inference (with Brian Homerding)
  - Improve (function) attribute inference - 2 (with Brian Homerding)
  - Generation of Annotated Sources (with Brian Homerding)
- **2020**
  - Improve Parallelism-Aware Analyses and Optimizations (with Jon Chesterfield)
  - Advanced Heuristics for Ordering Compiler Optimization Passes (with EJ Park and Giorgis Georgakoudis)
  - Improve inter-procedural analyses and optimizations (with Brian Homerding)
  - Advanced Heuristics for Ordering Compiler Optimization Passes - 2 (with EJ Park and Giorgis Georgakoudis)
  - Latency Hiding for Host to Device Memory Transfers (with Jon Chesterfield)
  - Improve inter-procedural analyses and optimizations - 2 (with Brian Homerding)
  - Deduce attributes for non-exact functions (with Brian Homerding)
- **2021**
  - Learning Loop Transformation Heuristics (with Mircea Trofin)
  - Integrate custom derivatives of Numerical Computing routines like BLAS and Eigen into Enzyme (with William Moses and Vassil Vassilev)
  - Improving OpenMP code generation with prediction of runtime parameters (with Jon Chesterfield)
  - Integrate Enzyme into Rust to Provide High-performance Differentiation in Rust (with William Moses)
  - Improve inter-procedural analyses and optimizations (with Jon Chesterfield)
  - Use official isl C++ bindings for polly (with Michael Kruse)
  - Integrating Enzyme into Rust (with William Moses)
- **2022**
  - Non-Determinacy based optimizations in Parallel Programs (with William Moses)
  - Learning loop transformation policy and its effect on RISC-V (with Mircea Trofin)
- **2023**
  - Machine Learning Guided Ordering of Compiler Optimization Passes (with Tarindu Jayatilaka and Mircea Trofin)
- **2024**
  - The 1001 Thresholds in LLVM (with Jan Hückelheim and William Moses)
  - GPU Libc Benchmarking (with Joseph Huber)
  - Statistical Analysis of LLVM-IR Compilation (with Aiden Grossman)
- **2025**
  - Improve Rust-Enzyme Reliability and Compile Times (with Manuel Drehwald and Kevin Sala)
  - LLVM Compiler Remarks Visualization Tool for Offloading (with Jose M Monsalve Diaz and Kevin Sala)

## A Decade at the Podium

Besides the countless code contributions, mentorship, and community building, Johannes was a constant presence at US LLVM Developers' Meetings and EuroLLVM. He spoke **at 11 meetings across 11 years (2015–2024)**, for at least 26 speaking sessions and even more that he helped author.

- **2015 — US DevMtg (San Jose)**
  - [Input Space Splitting for OpenCL](https://www.youtube.com/watch?v=py3pUnvQ7cY&index=3&list=PL_R5A0lGi1AA4Lv2bBFSwhgDaHvvpVU21)
  - [Tutorial: Polly - Optimistic Loop Nest Optimizations with Schedule Trees](https://www.youtube.com/watch?v=mIBUY20d8c8&index=10&list=PL_R5A0lGi1AA4Lv2bBFSwhgDaHvvpVU21) (with Tobias Grosser)
- **2016 — EuroLLVM (Barcelona)**
  - [Analyzing and Optimizing your Loops with Polly](https://youtu.be/mXve_W4XU2g) (with Tobias Grosser)
  - BoF: Polly - Loop Optimization Infrastructure (with Tobias Grosser and Zino Benaissa)
- **2017 — US DevMtg (San Jose)**
  - BoF: Thoughts and State for Representing Parallelism with Minimal IR Extensions in LLVM (with Xinmin Tian, Hal Finkel, Tb Schardl and Vikram Adve)
  - [Polyhedral Value & Memory Analysis](https://www.youtube.com/watch?v=xSA0XLYJ-G0)
- **2017 — EuroLLVM (Saarbrücken)**
  - Co-organizer
- **2018 — US DevMtg (San Jose)**
  - [Optimizing Indirections, using abstractions without remorse](https://www.youtube.com/watch?v=zfiHaPaoQPc)
  - BoF: Ideal versus Reality: Optimal Parallelism and Offloading Support in LLVM (with Xinmin Tian, Hal Finkel, TB Schardl, and Vikram Adve)
- **2019 — EuroLLVM (Brussels)**
  - [Compiler Optimizations for (OpenMP) Target Offloading to GPUs](https://youtu.be/3AbS82C3X30)
  - BoF: IPO — Where are we, where do we want to go? (with Kit Barton)
- **2019 — US DevMtg (San Jose)**
  - [The Attributor: A Versatile Inter-procedural Fixpoint Iteration Framework](https://www.youtube.com/watch?v=CzWkc_JcfS0)
  - [Tutorial: The Attributor: A Versatile Inter-procedural Fixpoint Iteration Framework](https://youtu.be/HVvvCSSLiTw)
  - [Tutorial: An overview of LLVM](https://youtu.be/J5xExRGaIIY)
  - Poster: Attributor, a Framework for Interprocedural Information Deduction (with Hideto Ueno and Stefan Stipanovic)
- **2020 — US DevMtg (virtual)**
  - [The Present and Future of Interprocedural Optimization in LLVM](https://youtu.be/uC-x_Je_sIw) (with Brian Homerding, Stefanos Baziotis, Stefan Stipanovic, Hideto Ueno, Kuter Dinel, Shinji Okumura, Luofan Chen)
  - [(OpenMP) Parallelism-Aware Optimizations](https://youtu.be/gtxWkeLCxmU) (with S. Stipanovic; H. Mosquera; J. Chesterfield; G. Georgakoudis; J. Huber)
  - [Tutorial: A Deep Dive into the Interprocedural Optimization Infrastructure](https://youtu.be/I4Iv-HefknA) (with B. Homerding; S. Baziotis; S. Stipanovic; H. Ueno; K. Dinel; S. Okumura; L. Chen)
- **2021 — US DevMtg (virtual)**
  - [Panel: Machine Learning Guided Optimizations in LLVM](https://www.youtube.com/watch?v=PLgbczJJC_I&list=PL_R5A0lGi1AATJX6-tY7IkYjpRjv30ziN&index=30)
  - [Optimizing OpenMP GPU Execution in LLVM](https://www.youtube.com/watch?v=4EPl7De3cKg&list=PL_R5A0lGi1AATJX6-tY7IkYjpRjv30ziN&index=19) (with Giorgis Georgakoudis and Joseph Huber)
- **2022 — US DevMtg (San Jose)**
  - [Panel: Machine Learning Guided Optimizations (MLGO) in LLVM](https://youtu.be/0uUKDQyn1Z4)
  - [Panel: High-level IRs for a C/C++ Optimizing Compiler](https://youtu.be/ElxPbIX4rDU)
  - [CUDA-OMP — Or, Breaking the Vendor Lock](https://youtu.be/7m8kn5_l970) (with Joseph Huber)
  - [Thoughts on GPUs as First-Class Citizens](https://youtu.be/YMI382d4Vz4)
- **2023 — EuroLLVM (Glasgow)**
  - [How to run the LLVM-Test Suite on GPUs and what you'll find](https://youtu.be/_IYG_aMjsfs)
  - [OpenMP as GPU Kernel Language](https://youtu.be/16cedvl2Ly4)
- **2024 — US DevMtg (Santa Clara)**
  - [(Offload) ASAN via Software Managed Virtual Memory](https://youtu.be/B60jp4khrvc)

## Johannes Will Be Missed

Beyond the commits, the talks, and the papers, those who worked with Johannes remember him as a person full of life and a good sense of humor. He is someone who signed his social media bio simply as "LLVM Developer, OpenMP contributor, Beer drinker, not in this order."

A memorial service will be held on Saturday, October 3, 2026, from 1:00 to 5:00 PM at San Jose Funeral Service in San Jose, California, with a separate service planned in Germany. He is survived by his wife, Xuejin Zhang, his father, Jürgen Doerfert, and other family and friends around the world.

In lieu of flowers, his family has asked that those who wish to honor his memory consider a donation to the LLVM Foundation (either through Everloved or directly), the organization whose mission he spent his career supporting and advancing. **A very generous donor has agreed to match 50K in donations in honor of Johannes.** If you donate directly to the LLVM Foundation via a DAF, please indicate in memory of Johannes Doerfert.

More details, and a place to share memories and condolences, can be found on [Johannes's memorial page](https://everloved.com/life-of/johannes-doerfert/).
