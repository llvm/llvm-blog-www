---
author: "Nicolas Celik"
date: "2024-08-31"
tags: ["GSoC", "libc"]
title: "GSoC 2024: Half-precision in LLVM libc"
---

C23 defines new floating-point types, such as `_Float16`, which corresponds to
the binary16 format from IEEE Std 754, also known as "half-precision," or FP16.
C23 also defines new variants of the C standard library's math functions
accordingly, such as `fabsf16` to get the absolute value of a `_Float16`.

The "Half-precision in LLVM libc" Google Summer of Code 2024 project aimed to
implement these new `_Float16` math functions in LLVM libc, making it the first
known C standard library implementation to implement these C23 functions.

We split math functions into two categories: basic operations and higher math
functions. The current implementation status of math functions in LLVM libc can
be viewed at https://libc.llvm.org/math/index.html#implementation-status.

The exact goals of this project were to:

1. Setup generated headers properly so that the `_Float16` type and `_Float16`
   functions can be used with various compilers and architectures.
2. Add generic implementations of `_Float16` basic operations for supported
   architectures.
3. Add optimized implementations of `_Float16` basic operations for specific
   architectures using special hardware instructions and compiler builtins
   whenever possible.
4. Add generic implementations of as many `_Float16` higher math functions as
   possible. We knew we would not have enough time to implement all of them.

## Work done

1. The `_Float16` type can now be used in generated headers, and declarations of
   `_Float16` math functions are generated with `#ifdef` guards to enable them
   when they are supported.
   - https://github.com/llvm/llvm-project/pull/93567
2. All 70 planned `_Float16` basic operations have been merged.
   - https://github.com/llvm/llvm-project/issues/93566
3. The `_Float16`, `float` and `double` variants of various basic operations
   have been optimized on certain architectures.
   - https://github.com/llvm/llvm-project/pull/98376
   - https://github.com/llvm/llvm-project/pull/99037
   - https://github.com/llvm/llvm-project/pull/100002
4. Out of the 54 planned `_Float16` higher math functions, 8 have been merged
   and 9 have an open pull request.
   - https://github.com/llvm/llvm-project/issues/95250

We ran into unexpected issues, such as:

- Bugs in Clang 11, which is currently still supported by LLVM libc and used in
  post-commit CI.
- Some post-commit CI workers having old versions of compiler runtimes that are
  missing some floating-point conversion functions on certain architectures.
- Inconsistent behavior of floating-point conversion functions across compiler
  runtime vendors (GCC's libgcc and LLVM's compiler-rt) and CPU architectures.

Due to these issues, LLVM libc currently only enables all `_Float16` functions
on x86-64 Linux. Some were disabled on AArch64 due to Clang 11 bugs, and all
were disabled on 32-bit Arm and on RISC-V due to issues with compiler runtimes.
Some are not available on GPUs because they take `_Float128` arguments, and the
`_Float128` type is not available on GPUs.

There is work in progress to work around issues with compiler runtimes by using
our own floating-point conversion functions.

## Work left to do

- Implement the remaining `_Float16` higher math functions.
- Enable the `_Float16` math functions that are disabled on AArch64 once LLVM
  libc bumps its minimum supported Clang version.
- Enable `_Float16` math functions on 32-bit Arm and on RISC-V once issues with
  compiler runtimes are resolved.

## Acknowledgements

I would like to thank my Google Summer of Code mentors, Tue Ly and Joseph Huber,
as well as other LLVM maintainers I interacted with, for their help. I would
also like to thank Google for organizing this program.
