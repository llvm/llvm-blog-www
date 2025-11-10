---
author: "Amr Hesham (amrdeveloper)"
date: "2025-11-08"
tags: ["GSoC", "LLVM", "MLIR", "ClangIR", "C++"]
title: "GSoC 2025: ClangIR upstreaming"
---

Hello everyone 👋! My name is Amr Hesham, and for Google Summer of Code 2025, I’ve been working on the ClangIR Upstreaming project.

My mentors were Bruno Cardoso Lopes, Andy Kaylor and Erich Keane.

# Background

ClangIR is a high-level representation in Clang that reflects aspects of the C/C++ languages and their extensions. It is implemented using MLIR and occupies a position between Clang’s AST and LLVM IR.

Starting from April 2024, the upstreaming process started to migrate the code from the [incubator](https://github.com/llvm/clangir) to the [main](https://github.com/llvm/llvm-project) LLVM repository.

The ClangIR upstreaming process includes the following steps:

* Improve the design of ClangIR dialect operations, attributes, syntax and implementation details if needed.
* Align with LLVM coding standards and quality expectations.
* Implement missing features in the incubator and upstream it.
* Backport the new design/implementation from the LLVM repository to the incubator.
* Implement folders for ClangIR dialect operations.

The goal of this project was to implement the following C/C++ features in the ClangIR project.

* Built-in VectorType (`vector_size` and `ext_vector_type` attributes) with operators and built-ins functions.

* ComplexType with operators and all `-complex-range` flag values to allow selecting different arithmatic calculation algorithms, for example, the SMITH, R. L. Algorithm 116: Complex division. Commun. ACM 5, 8 (1962).

* C++ Exception handling.

# What we did

Over the coding period, I did the following.

## Builtin VectorType

I have implemented support for all C/C++ operators for built-in VectorType that includes arithmetic, comparison, shifts and logical operators, also supporting using VectorType in many expressions, for example, index and ternary expression, and now we can compile C++ code successfully.

Also, implemented folders for ClangIR VectorType operations to fold specific patterns. For example, in cir::VecExtractOp, if both the vector and the index are constant, we can fold that operation and replace it with the actual result.

```
// Before
%vector  = cir.const #cir.const_vector<[#cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i, #cir.int<4> : !s32i]> : !cir.vector<4 x !s32i>
%index   = cir.const #cir.int<1> : !s32i
%element = cir.vec.extract %vector[%index : !s32i] : !cir.vector<4 x !s32i>

// After
%element = cir.const #cir.int<2> : !s32i
```

I also implemented folders for other operators, for example, Shuffle, DynamicShuffle, Comparisons...etc.

## ComplexType

Guided by my mentors, I changed the design of the complex operations in ClangIR dialect to introduce a single operator for each arithmetic operator, like ComplexAddOp, ComplexMulOp...etc, which makes it easy to read in IR and to apply folders.

Also, to represent `__real__` and `__imag__` unary operators as MLIR operations not only when applying them on Complex but also on scalar values, for example.

```
%real = cir.complex.real %complex : !cir.complex<!s32i> -> !s32i;
%real = cir.complex.real %scalar  : !cir.float -> !cir.float
```

During writing tests to cover all cases for complex types with type promotions, I found an unhandled case in the classic Clang codegen (The default Clang code generator clang/lib/CodeGen), which led to a crash with valid code, and I fixed it 🥳 (Fix will be available in clang-22).

Also, all `-complex-range` flag values to allow selecting different arithmatic calculation algorithms are fully implemented, for example, to use the `SMITH, R. L. Algorithm 116: Complex division. Commun. ACM 5, 8 (1962)`. 

```
%result = cir.complex.mul %a, %b range(basic)    : !cir.complex<!cir.float>
%result = cir.complex.mul %a, %b range(improved) : !cir.complex<!cir.float>
%result = cir.complex.mul %a, %b range(promoted) : !cir.complex<!cir.float>
```

With that representation that abstracts the actual calculations of the complex multiplications at that stage, it's easy to write a folder that, in the case of the element type of complex, is integer and one of the operands is `{0, 0}`, then we can fold the ComplexMulOp to just complex with 0 as real and imaginary values 🤓.

Also implemented support for using ComplexType with all C/C++ expressions except expressions related to OpenMP or Coroutine because they are not fully implemented, and implemented floating-point operators for built-in functions, for example, `acos`, `asign`, `atan`...etc, which are required to implement future built-in functions for ComplexType.

And implemented some not yet implemented features for Aggregate and Constant expressions to allow using more features in the language, and also to make it easy to test other expressions, either for Complex, Vector types or exceptions.

## Exception Handling

Guided by my mentors, I changed the design of the try-catch operation in the ClangIR dialect to be like

```
cir.try {
 ...
 cir.yield
} catch [type #cir.global_view<RTTI> : !cir.ptr<!u8i>] {
 ...
 cir.yield
} unwind { // Can be another catch, unwind, cleanup or catch all too
 ...
 cir.yield
}
```

Also, I implemented support for rethrow and throw with sub-expression, for example, `throw _builtin_complex(1.0f, 2.0f)`.

Now we can compile C++ code with a try catch block that contains a function call, which may or may not throw exceptions.

# Future work

- Support Implicit conversion from integral to boolean vectors, which is a new feature recently
  implemented in the classic Clang CodeGen ([PR in Clang](https://github.com/llvm/llvm-project/pull/158369)).

- Some ComplexType built-in functions should be implemented later once the call convention is upstreamed, for example, `__builtin_cabs`, `__builtin_ccos`, ...etc.

- Support a mix of unwind and cleanup regions in exception handling, so after that, when compiling the following C++ code.

```cpp
try {
  S1 s1;
  S2 s2;
  foo();
  S3 s3;
}
catch (std::exception& e) {}
```

We will have IR representation like this 😋.

```
cir.try {
 cir.call @S1::S1()
 cir.call @S2::S2() cleanup("c1")
 cir.call @foo() cleanup("c1")
 cir.call @S3::S3() cleanup("c2")
 cir.call @S3::~S3()
 cir.call @S2::~S2()
 cir.call @S1::~S1()
 cir.yield
} cleanup("c2") {
 cir.call @S2::~S2()
 // Falls through to "c1"
 cir.yield  
} cleanup("c1") {
 cir.call @S1::~S1()
 // Yields to the catch handler or unwind
 cir.yield  
} catch [type #cir.global_view<RTTI>] {
 // Stays in this function
 cir.yield  
} unwind {
 // Continues looking for a catch handler in the caller's scope
 cir.resume 
}
```

# What I've learned

- Before Google Summer of Code, I had no experience with MLIR at all, but during the coding period, it was an incredible learning experience for me to work with my mentors on a codegen for an interesting language like C++ using MLIR.

- Reading ClangIR Codegen in the incubator, classic Clang codegen and upstreamed Codegen was a very interesting and knowledgeable experience for me.

- I was impressed by my knowledge near the end of Google Summer of Code when I found a crash in the original Clang codegen, and in almost one hour, I was able to find the exact reason for the crash and create a PR to fix it ([#160583](https://github.com/llvm/llvm-project/issues/160583)).

- I learned a lot about best practices on how to work with MLIR.

- I learned a lot about how to think and organize work from the upstreaming process itself and how my mentors organized it.

# Acknowledgements

I would like to thank my mentors, Bruno Cardoso Lopes, Andy Kaylor, and Erich Keane, for their guidance, code reviews, the interesting discussions and brainstorming, which not only helped me work on that project, but also improved my thought process on how to think in many situations 🙏.

I would also like to thank Anton Korobeynikov for offering regular office hours on GSoC guidelines and for providing helpful feedback on the midterm presentation 🙏.

I would also like to thank the LLVM Compiler Infrastructure and Google for supporting this work through GSoC 🙏.

# Contributions & PRs

- PR's in the main LLVM repo for upstreaming stuff or implementing features that will be backported to the ClangIR ([191 PR](https://github.com/llvm/llvm-project/pulls?q=is%3Apr+author%3AAmrDeveloper+label%3AClangIR+is%3Aclosed)).

- PR's in the ClangIR repo for implementing features or backporting work from the upstream ([75 PR](https://github.com/llvm/clangir/pulls?q=is%3Apr+author%3AAmrDeveloper+is%3Aclosed)).

- The fix to the classic clang codegen ([1 PR](https://github.com/llvm/llvm-project/pull/160609)).
