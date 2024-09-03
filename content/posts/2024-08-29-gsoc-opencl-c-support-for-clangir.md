---
author: "Zhi Ma (7mile)"
date: "2024-08-29"
tags: ["GSoC", "OpenCL", "ClangIR"]
title: "GSoC 2024: Compile GPU kernels using ClangIR"
---

Hello everyone! I'm 7mile. My GSoC project this summer is [Compile GPU kernels using ClangIR](https://discourse.llvm.org/t/clangir-compile-gpu-kernels-using-clangir/76984). It's been an exciting journey in compiler development, and I'm thrilled to share the progress and insights gained along the way here.

# Background

[The ClangIR project](https://github.com/llvm/clangir) aims to establish a new IR for Clang, built on top of MLIR. As part of the ongoing effort to support heterogeneous programming models, this project focuses on integrating OpenCL C language support into ClangIR. The ultimate goal is to enable the compilation of GPU kernels written in OpenCL C into LLVM IR targeting the SPIR-V architecture, laying the groundwork for future enhancements in SYCL and CUDA support.

# What We Did

Our work involved several key areas:

1. **Address Space Support**: One of the fundamental tasks was teaching ClangIR to handle address spaces, a vital feature for languages like OpenCL. Initially, we considered mimicking LLVM's approach, but this proved inadequate for ClangIR's goals. After thorough discussion and an RFC, we implemented a unified address space design that aligns with ClangIR's objectives, ensuring a clean and maintainable code structure.

2. **OpenCL Language and SPIR-V Target Integration**: We extended ClangIR to support the OpenCL language and the SPIR-V target. This involved enhancing the pipeline to accommodate the latest OpenCL 3.0 specification and implementing hooks for language-specific and target-specific customizations.

3. **Vector Type Support**: OpenCL vector types, a critical feature for GPU programming, were integrated into ClangIR. We leveraged ClangIR's existing cir.vector type to generate the necessary code, ensuring consistent compilation results.

4. **Kernel and Module Metadata Emission**: We added support for emitting OpenCL kernel and module metadata in ClangIR, a necessary step for proper integration with the SPIR-V target. This included the creation of structured attributes to represent metadata, following MLIR's preferences for well-defined structures.

5. **Global and Static Variables with Qualifiers**: We implemented support for global and static variables with qualifiers like `global`, `constant`, and `local`, ensuring that these constructs are correctly represented and lowered in the ClangIR pipeline.

6. **Calling Conventions**: We adjusted the calling conventions in ClangIR to align with SPIR-V requirements, migrating from the default `cdecl` to SPIR-V-specific conventions like `SpirKernel` and `SpirFunction`. This also enables most OpenCL built-in functions like `barrier` and `get_global_id`.

7. **User Experience Enhancements**: Finally, we ensured that the end-to-end kernel compilation experience using ClangIR was smooth and intuitive, with minimal manual intervention required.

# Results

The project successfully met its primary goals. OpenCL kernels from the Polybench-GPU benchmark suite can now be compiled using ClangIR into LLVM IR for SPIR-V. All patches have been merged into the main ClangIR repository, and the project’s progress has been well-documented in the [overview issue](https://github.com/llvm/clangir/issues/689). I believe the work not only advanced OpenCL support but also laid a solid foundation for future enhancements, such as SYCL and CUDA support in ClangIR.

We have successfully compiled and executed all 20 OpenCL C benchmarks from the [polybenchGpu](https://github.com/sgrauerg/polybenchGpu) repository, passing the built-in result validation. Please refer to our [artifact evaluation repository](https://github.com/seven-mile/clangir-ocl-ae) for detailed instructions on how to experiment with our work.

# Future Works

As we look forward, there are two key areas that require further development:

1. **Function Attribute Consistency**: For example, the `convergent` function attribute is crucial for preventing misoptimizations in SIMT languages like OpenCL. ClangIR currently lacks this attribute, which could lead to issues in parallel computing contexts. Addressing this is a priority to ensure correct optimization behavior.

2. **Support for OpenCL Built-in Types**: Another critical area for future work is the support for OpenCL built-in types, such as `pipe` and `image`. These types are essential for handling data streams and image processing tasks in various specialized OpenCL applications. Supporting these types will significantly enhance ClangIR's adherence to the OpenCL standard, broadening its applicability and ensuring better compatibility with a wide range of OpenCL programs.

# Acknowledgements

This project would not have been possible without the guidance and support of the LLVM community. I extend my deepest gratitude to my mentors, Julian Oppermann, Victor Lomüller, and Bruno Cardoso Lopes, whose expertise and encouragement were instrumental throughout this journey. Additionally, I would like to thank Vinicius Couto Espindola for his collaboration on ABI-related work. This experience has been immensely rewarding, both technically and in terms of community engagement.

# Appendix

* [Overview issue of OpenCL C support](https://github.com/llvm/clangir/issues/689)
* [Artifact Evaluation Instructions](https://github.com/seven-mile/clangir-ocl-ae)
