---
author: "Vinicius Espindola"
date: "2024-09-07"
tags: ["GSoC", "mlir", "clangir"]
title: "GSoC 2024: ABI Lowering in ClangIR"
---

ClangIR is an ongoing effort to build a high-level intermediate representation
(IR) for C/C++ within the LLVM ecosystem. Its key advantage lies in its ability
to retain more source code information. While ClangIR is making progress, it
still lacks certain features, notably ABI handling. Currently, ClangIR lowers
most functions without accounting for ABI-specific calling convention details.

## Goals

The "Build & Run SingleSource Benchmarks with ClangIR - Part 2" Google Summer of
Code 2024 builds on my contributions from GSoC 2023 by addressing one of the
main issues I encountered: target-specific lowering. It focuses on extending
ClangIR’s code generation capabilities, particularly in ABI-lowering for X86-64.
Several tests rely on operations and types (e.g., `va_arg` calls and complex
data types) that require target-specific information to compile correctly.

The concrete steps to achieve this were:

1. **Implement foundational infrastructure** that can scale to multiple
   architectures while adhering to ClangIR design principles such as CodeGen
   parity, feature guarding, and AST backreferences.
2. **Handle basic calling convention scenarios** as a proof of concept to
   validate the foundational infrastructure.
3. **Add lowering for a second architecture** to further validate the
   infrastructure's extensibility to multiple architectures.
4. **Unify target-specific ClangIR lowering into the library**, as there are a
   few isolated methods handling target-specific code lowering like
   `cir.va_arg`.
5. **Integrate calling convention lowering into the main pipeline** to ensure
   future contributions and continued development of this infrastructure.

## Contributions

The list of contribution (PRs) can be found
[here](https://github.com/llvm/clangir/pulls?q=is%3Apr+is%3Aclosed+author%3Asitio-couto+closed%3A%3E2024-05-01).

### Target Lowering Library

The most significant contribution of this project was the development of a
modular [`TargetLowering` library](https://github.com/llvm/clangir/pull/643).
This ensures that target-specific MLIR lowering passes can leverage this shared
library for lowering logic. The library also follows ClangIR's feature guarding
principles, ensuring that any contributor can refer to the original CodeGen for
contributions, and any unimplemented feature is asserted at specific code
points, making it easy to track missing functionality.

### Calling Convention Lowering Pass

As a proof of concept, the initial development of the `TargetLowering` library
focused on implementing a [calling convention lowering
pass](https://github.com/llvm/clangir/pull/642) that targets multiple
architectures. Currently, ClangIR ignores the target ABI during CodeGen to
retain high-level information. For example, structs are not unraveled to improve
argument-passing efficiency. ABI-specific LLVM attributes are also ignored. This
pass addresses these issues by properly tagging LLVM attributes and rewriting
function definitions and calls to handle unraveled structs. This was implemented
for both X86-64 and [AArch64](https://github.com/llvm/clangir/pull/679),
demonstrating the library's multi-architecture support.

## Shortcomings

### Target-Specific Lowering Unification

While some target-specific lowering code was moved into the library, it was
copied and pasted rather than properly integrated. This is not ideal for
leveraging the library’s multi-architecture features.

### Inclusion in the Main Pipeline

This is still a work in progress, as the library is not yet mature enough to
handle most pre-existing ClangIR tests. There are also feature guards with
unreachable statements for many unimplemented features.

## Future Work

Now that there is a base infrastructure for handling target-agnostic to
target-specific CIR code, there is a large amount of future work to be done,
including:

- Improving DataLayout-related queries using MLIR's built-in tools.
- Implementing calling convention lowering for additional types, such as
  pointers.
- Extending the TargetLowering library to support more architectures.
- Unifying remaining target-specific lowering code from other parts of ClangIR.

## Acknowledgements

I would like to thank my Google Summer of Code mentors, Bruno Cardoso Lopes and
Nathan Lanza, for another great GSoC experience. I also want to thank the LLVM
community and Google for organizing the program.
