---
author: "Volodymyr Vasylkun (Poseydon)"
date: "2024-08-29"
tags: ["GSoC", "optimization", "intrinsics"]
title: "GSoC 2024: 3-way comparison intrinsics"
---

Hello everyone! My name is Volodymyr, and in this post I would like to talk about the project I have been working on for the past couple of months as part of Google Summer of Code 2024. The aim of the project was to introduce 3-way comparison intrinsics to LLVM IR and add a decent level of optimizations for them.

# Background

Three-way comparison is an operation present in many high-level languages, such as C++ and its spaceship operator or Rust and the `Ord` trait. It operates on two values for which there is a defined comparison operation and returns `-1` if the first operand is less than the second, `0` if they are equal, and `1` otherwise. At the moment, compilers that use LLVM express this operation using different sequences of instructions which are optimized and lowered individually rather than as a single operation. Adding an intrinsic for this operation would therefore help us generate better machine code on some targets, as well as potentially optimize patterns in the middle-end that we didn't optimize before.

# What was done

Over the course of the project I have added two new intrinsics to the LLVM IR: `llvm.ucmp` for an unsigned 3-way comparison and `llvm.scmp` for a signed comparison. They both take two arguments that must be integers or vectors of integers and return an integer or a vector of integers with the same number of elements. The arguments and the result do not need to have the same type.

In the middle-end the following passes received some support for these intrinsics:

  * InstSimplify ([#1](https://github.com/llvm/llvm-project/pull/93730), [#2](https://github.com/llvm/llvm-project/pull/95601))
  * InstCombine ([#1](https://github.com/llvm/llvm-project/pull/96118), [#2](https://github.com/llvm/llvm-project/pull/98360), [#3](https://github.com/llvm/llvm-project/pull/101049), [#4](https://github.com/llvm/llvm-project/pull/105272), [#5](https://github.com/llvm/llvm-project/pull/105583))
  * [CorrelatedValuePropagation](https://github.com/llvm/llvm-project/pull/97235)
  * [ConstraintElimination](https://github.com/llvm/llvm-project/pull/97974)

I have also added folds of idiomatic ways that a 3-way comparison can be expressed to a call to the corresponding intrinsic.

In the backend there are two different ways of expanding the intrinsics: [as a nested select](https://github.com/llvm/llvm-project/pull/91871) (i.e. `(x < y) ? -1 : (x > y ? 1 : 0)`) or [as a subtraction of zero-extended comparisons](https://github.com/llvm/llvm-project/pull/98774) (`zext(x > y) - zext(x < y)`). The second option is the default one, but targets can choose to use the second one through a TLI hook.

# Results

I think that overall the project was successful and brought a small positive change to LLVM. To demonstrate its impact in a small test case, the following function in C was compiled twice, first with Clang 18.1 and then with Clang built from the main branch of LLVM repository:

```C
unsigned char ucmp_32_8(unsigned int a, unsigned int b) {
    return (a < b ? -1 : a > b);
}
```

With Clang 18.1:

```text
; ====== LLVM IR ======
define i8 @ucmp_32_8(i32 %a, i32 %b) {
entry:
  %cmp = icmp ult i32 %a, %b
  %cmp1 = icmp ugt i32 %a, %b
  %0 = zext i1 %cmp1 to i8
  %conv2 = select i1 %cmp, i8 -1, i8 %0
  ret i8 %conv2
}

; ====== x86_64 assembly ======
ucmp_32_8:
  xor     ecx, ecx
  cmp     edi, esi
  seta    cl
  mov     eax, 255
  cmovae  eax, ecx
  ret
```

With freshly built Clang:

```plain
; ====== LLVM IR ======
define i8 @ucmp_32_8(i32 %a, i32 %b) {
entry:
  %conv2 = tail call i8 @llvm.ucmp.i8.i32(i32 %a, i32 %b)
  ret i8 %conv2
}

; ====== x86_64 assembly ======
ucmp_32_8:
  cmp     edi, esi
  seta    al
  sbb     al, 0
  ret
```

As you can see, the number of instructions in the generated code had gone down by 2, or 40% (excluding `ret`). Although this isn't much and is a small synthetic test, it can still make a noticeable impact if code like this is found in a hot path somewhere.

The impact of these changes on real-world code is much harder to quantify. Looking at llvm-opt-benchmark, there are quite a few places where the intrinsics are being used, which suggests that some improvement must have taken place, although it is unlikely to be significant in all but very few cases.

# Future Work

There are still many opportunities for optimization in the middle-end, some of which are already known and being worked on at the time of writing this, others are yet to be discovered. I would also like to allow pointers and vectors of pointers to be valid operands for the intrinsics, although that would be quite a minor change. In the backend I would also like to work on better handling of intrinsics in GlobalISel, which is something that I didn't have enough time for and other members of LLVM community had helped me with.

# Acknowledgements

None of this would have been possible without my two amazing mentors, Nikita Popov and Dhruv Chawla, and the LLVM community as a whole. Thank you for helping me on this journey and I am looking forward to working with you in the future.
