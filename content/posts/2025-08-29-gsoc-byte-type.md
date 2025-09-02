---
author: "Pedro Lobo"
date: "2025-09-08"
tags: ["GSoC", "clang", "optimizations", "IR"]
title: "GSoC 2025 - Byte Type: Supporting Raw Data Copies in the LLVM IR"
---

This summer I participated in GSoC under the LLVM Compiler Infrastructure.
The goal of the project was to add a new byte type to the LLVM IR, capable of representing raw memory values.
This new addition enables the native implementation of memory-related intrinsics in the IR, including `memcpy`, `memmove` and `memcmp`, fixes existing unsound transformations and enables new optimizations, all with a minimal performance impact.

# Background

One of LLVM's longstanding problems is the absence of a type capable of representing raw memory values.
Currently, memory loads of raw bytes are performed through an appropriately sized integer type.
However, integers are incapable of representing an arbitrary memory value.
Firstly, they do not retain pointer provenance information, rendering them unable to fully specify the value of a pointer.
Secondly, loading memory values containing `poison` bits through an integer type taints the loaded value, as integer values are either `poison` or have a fully-defined value, with no way to represent individual `poison` bits.

Source languages such as C[^1] and C++[^2] provide proper types to inspect and manipulate raw memory.
These include `char`, `signed char` and `unsigned char`.
C++17 introduced the `std::byte` type, which offers similar raw memory access capabilities, but does not support arithmetic operations.
Currently, Clang lowers these types to the `i8` integer type, which does not accurately model their raw memory access semantics, motivating miscompilations such as the one reported in [bug report 37469](https://bugs.llvm.org/show_bug.cgi?id=37469).

The absence of a similar type in the LLVM IR hinders the implementation of memory-related intrinsics such as `memcpy`, `memmove` and `memcmp`, and introduces additional friction when loading and converting memory values to other types, leading to implicit conversions that are hard to identify and reason about.
The two core problems stemming from the absence of a proper type to access and manipulate raw memory, directly addressed by the byte type and explored throughout the remainder of this section, are summarized as follows:

1. Integers do not track provenance, rendering them incapable of representing a pointer.
2. Loads through integer types spread `poison` values, which taints the load result if the loaded values contain at least one `poison` bit (as occurs with padded values).

## Pointer Provenance

According to the [LLVM Language Reference](https://llvm.org/docs/LangRef.html), pointers track provenance, which is _the ability to perform memory accesses through the pointer, in the sense of the pointer aliasing rules_.
The main goal of tracking pointer provenance is to simplify alias analysis, yielding more precise results, which enables high-level optimizations.

Integers, unlike pointers, do not capture provenance information, being solely characterized by their numerical value.
Therefore, loading a pointer through an integer type discards the pointer's provenance.
This is problematic as such loads can cause pointer escapes that go unnoticed by alias analysis.
Once alias analysis is compromised, simple optimizations that rely on the absence of aliasing become invalid, compromising the correctness of the whole compilation process.

Currently, Alive2 defines the result of loading a pointer value through an integer type as `poison`.
This implies that loads through integer types fail to accurately recreate the original memory value, hindering pointer copies via integer types.
In the following example, storing a pointer to memory and loading it through the `i64` type yields `poison`, invalidating the transformation.

```llvm
define ptr @src(ptr %ptr, ptr %v) {
  store ptr %v, ptr %ptr
  %l = load ptr, ptr %ptr
  ret ptr %l
}

define ptr @tgt(ptr %ptr, ptr %v) {
  store ptr %v, ptr %ptr
  %l = load i64, ptr %ptr      ; poison
  %c = inttoptr i64 %l to ptr  ; poison
  ret ptr %c                   ; poison
}
```

## Undefined Behavior

LLVM’s `poison` value is used to represent unspecified values, such as padding bits.
Loading such memory values through an integer type propagates `poison` values, as integer types are either `poison` or have a fully-defined value, not providing enough granularity to represent individual `poison` bits.
This hinders the copy of padded values.

Moreover, this lack of granularity can lead to subtle issues that are often overlooked.
The [LLVM Language Reference](https://llvm.org/docs/LangRef.html#bitcast-to-instruction) defines the `bitcast` instruction as a _no-op cast because no bits change with this conversion_.
Nonetheless, while scalar types are either `poison` or have a fully-defined value, vector types in LLVM track `poison` values on a per-lane basis.
This introduces potential pitfalls when casting vector types to non-vector types, as the cast operation can inadvertently taint non-`poison` lanes.
In the following example, considering the first lane of `%v` to be `poison`, the result of casting the vector to an `i64` value is `poison`, regardless of the value of the second lane.

```llvm
define i64 @ub(ptr %ptr) {
  %v = load <2 x i32>, ptr %ptr       ; <i32 poison, i32 42>
  %c = bitcast <2 x i32> %v to i64    ; i64 poison
  ret i64 %c
}
```

Although covered by the [Language Reference](https://llvm.org/docs/LangRef.html#bitcast-to-instruction) ("_the [bitcast] conversion is done as if the value had been stored to memory and read back as [the destination type]_"), this duality in the value representation between vector and scalar types integer constitutes a corner case that is not widely contemplated and often unnecessarily introduces undefined behavior.

# Implementing the Byte Type

Back in 2021, a [GSoC project with a similar goal](https://github.com/georgemitenkov/GSoC-2021), produced a working prototype of the byte type.
This prototype introduced the byte type to the IR, lowered C and C++'s raw memory access types to the byte type and implemented some optimizations over the new type.

The current project began by porting these patches to the latest version of LLVM, adapting the code to support the newly introduced opaque pointers.
As the work progressed and new challenges emerged, the original proposal was iteratively refined.
The implementation of the byte type in LLVM and Alive2 can be found [here](https://github.com/pedroclobo/llvm-project/tree/byte-type) and [here](https://github.com/pedroclobo/alive2/tree/byte-type), respectively.

## Byte Type

The byte type is a first-class single-value type, with the same size and alignment as the equivalently sized integer type.
Memory loads through the byte type yield the value's raw representation, without introducing any implicit casts.
This allows the byte type to represent both pointer and non-pointer values.

Additionally, the byte type is equipped with the necessary granularity to represent `poison` values at the bit-level, such that loads of padded values through the byte type do not taint the loaded value.
As a consequence, a `bitcast` between vector and scalar byte types preserves the raw byte value.
In the following example, a `poison` lane does not taint the cast result, unlike with equivalently sized integer types.

```llvm
define b64 @f(ptr %ptr) {
  %v = load <2 x b32>, ptr %ptr
  %c = bitcast <2 x b32> %v to b64
  ret b64 %c
}
```

These two properties of the byte type directly addressed the aforementioned problems, enabling the implementation of a user-defined `memcpy` in the IR, as shown in the following example.
In a similar manner, a native implementation of `memmove` can be achieved.

```llvm
define ptr @my_memcpy(ptr %dst, ptr %src, i64 %n) {
entry:
  br label %for.cond
for.cond:
  %i = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp ult i64 %i, %n
  br i1 %cmp, label %for.body, label %for.end
for.body:
  %arrayidx = getelementptr inbounds b8, ptr %src, i64 %i
  %byte = load b8, ptr %arrayidx
  %arrayidx1 = getelementptr inbounds b8, ptr %dst, i64 %i
  store b8 %byte, ptr %arrayidx1
  %inc = add i64 %i, 1
  br label %for.cond
for.end:
  ret ptr %dst
}
```

The newly implemented type also fixes existing optimizations.
Previously, InstCombine lowered small calls to `memcpy` and `memmove` into integer load/store pairs.
Due to the aforementioned reasons, this lowering is unsound.
By using byte load/store pairs instead, the transformation, as shown in the following example, is now valid.

```llvm
define void @my_memcpy(ptr %dst, ptr %src) {
  call void @llvm.memcpy(ptr %dst, ptr %src, i64 8)
  ret void
}

define void @my_memmove(ptr %dst, ptr %src) {
  call void @llvm.memmove(ptr %dst, ptr %src, i64 8)
  ret void
}

define void @my_memcpy(ptr %dst, ptr %src) {
  %l = load b64, ptr %src
  store b64 %l, ptr %dst
  ret void
}

define void @my_memmove(ptr %d, ptr %s) {
  %l = load b64, ptr %s
  store b64 %l, ptr %d
  ret void
}
```

SROA performs a similar transformation, lowering `memcpy` calls to integer load/store pairs.
Similarly, this optimization pass was changed to use byte load/store pairs, as depicted in the following example.

```llvm
define void @src(ptr %a, ptr %b) {
  %mem = alloca i8
  call void @llvm.memcpy(ptr %mem, ptr %a, i32 1)
  call void @llvm.memcpy(ptr %a, ptr %mem, i32 1)
  ret void
}

define void @tgt(ptr %a, ptr %b) {
  %mem.copyload = load b8, ptr %a
  store b8 %mem.copyload, ptr %a
  ret void
}
```

## Bytecast Instruction

Byte values can be reinterpreted as values of other primitive types.
This is achieved through the `bytecast` instruction.
This cast instruction comes in two flavors, either allowing or disallowing type punning.
Considering that a byte might hold a pointer or a non-pointer value, the `bytecast` follows the following semantics:

- A vanilla `bytecast`, distinguished by the absence of the `exact` flag, is used to cast a byte to any other primitive type, allowing type punning. More precisely,
  - If the type of the value held by the byte matches the destination type of the cast, it is a no-op.
  - Otherwise, the cast operand undergoes a conversion to the destination type, converting pointers to non-pointer values and vice-versa, respectively wrapping a `ptrtoint` or `inttoptr` cast.

- A `bytecast` with the `exact` flag succeeds if both the type of the value held by the byte and the destination type are either both pointer or non-pointer types. More specifically,
  - If the type of the value held by the byte matches the destination type of the cast, it is a no-op.
  - Otherwise, the result is `poison`, preventing type punning between pointer and non-pointer values.

The `exact` version of the `bytecast` mimics the reinterpretation of a value, as if it had been stored in memory and loaded back through the cast destination type.
This is aligned with the semantics adopted by the `bitcast` instruction, which "_is done as if the value had been stored to memory and read back as [the destination type]_"", enabling store-to-load forwarding optimizations, such as the one depicted in the next example.

```llvm
define i8 @src(b8 %x) {
  %a = alloca b8
  store b8 %x, ptr %a
  %v = load i8, ptr %a
  ret i8 %v
}

define i8 @tgt(b8 %x) {
  %cast = bytecast exact b8 %x to i8
  ret i8 %cast
}
```

## Memcmp Lowering

The standard version of the `bytecast` enables the implementation of `memcmp` in the IR.
Currently, calls to `memcmp` of small sizes are lowered to integer loads, followed by a subtraction, comparing the two loaded values.
Due to the aforementioned problems, this lowering is unsound.
Loading the two memory values as bytes is insufficient as comparisons between bytes are undefined, as to avoid overloading the IR by supporting comparisons between pointers and provenance-unaware values.
To that end, the version of the `bytecast` which performs type punning is used, forcefully converting possible pointer values into their integer representation.
The two values, then converted to integers, can be compared as before.
The following example depicts the previous and new lowerings of a `memcmp` of 1 byte.

```llvm
define i32 @before(ptr %p, ptr %q) {
  %lhsc = load i8, ptr %p
  %lhsv = zext i8 %lhsc to i32
  %rhsc = load i8, ptr %q
  %rhsv = zext i8 %rhsc to i32
  %chardiff = sub i32 %lhsv, %rhsv
  ret i32 %chardiff
}

define i32 @after(ptr %p, ptr %q) {
  %lhsb = load b8, ptr %p
  %lhsc = bytecast b8 %lhsb to i8
  %lhsv = zext i8 %lhsc to i32
  %rhsb = load b8, ptr %q
  %rhsc = bytecast b8 %rhsb to i8
  %rhsv = zext i8 %rhsc to i32
  %chardiff = sub i32 %lhsv, %rhsv
  ret i32 %chardiff
}
```

## Load Widening

A common optimization performed by LLVM is to widen memory loads when lowering calls to `memcmp`.
The previously proposed lowering falls short in the presence of such optimizations.
Whilst using a larger byte type to load the memory value preserves its raw value, the `bytecast` to an integer type yields `poison` if any of the loaded bits are `poison`.
This is problematic as the remaining bits added by the widened load could assume any value or even be uninitialized.
As such, when performing load widening, the following lowering, depicted in the next example, is performed.
The `!uninit_is_nondet`, proposed in [the RFC proposing uninitialized memory loads to return `poison`](https://discourse.llvm.org/t/rfc-load-instruction-uninitialized-memory-semantics/67481), converts any `poison` bits to a non-deterministic value, preventing the `bytecast` to an integer type from yielding `poison`.

```llvm
define i32 @src(ptr %x, ptr %y) {
  %call = tail call i32 @memcmp(
   ptr %x, ptr %y, i64 2)
  ret i32 %call
}

define i32 @tgt(ptr %x, ptr %y) {
  %1 = load b16, ptr %x, !uninit_is_nondet
  %2 = load b16, ptr %y, !uninit_is_nondet
  %3 = bytecast b16 %1 to i16
  %4 = bytecast b16 %2 to i16
  %5 = call i16 @llvm.bswap.i16(i16 %3)
  %6 = call i16 @llvm.bswap.i16(i16 %4)
  %7 = zext i16 %5 to i32
  %8 = zext i16 %6 to i32
  %9 = sub i32 %7, %8
  ret i32 %9
}
```

## Casts, Bitwise and Arithmetic Operations

Values of other primitive types can be cast to the byte type using the `bitcast` instruction, as shown in the following example.

```llvm
%1 = bitcast i8 %val to b8
%2 = bitcast i64 %val to b64
%3 = bitcast ptr to b64 ; assuming pointers to be 64 bits wide
%4 = bitcast <8 x i8> to <8 x b8>
```

Furthermore, bytes can also be truncated, enabling store-to-load forwarding optimizations, such as the one presented in the next example.
Performing an exact `bytecast` to `i32`, followed by a `trunc` to `i8` and a `bitcast` to `b8` would be unsound, as if any of the unobserved bits of the byte value were `poison`, the `bytecast` would yield `poison`, invalidating the transformation.

```llvm
define b8 @src(b32 %x) {
  %a = alloca b32
  store b32 %x, ptr %a
  %v = load b8, ptr %a
  ret b8 %v
}

define b8 @tgt(b32 %x) {
  %trunc = trunc b32 %x to b8
  ret b8 %trunc
}
```

Due to the cumbersome semantics of performing arithmetic on provenance-aware values, arithmetic operations on the byte type are disallowed.
Bitwise binary operations are also disallowed, with the exception of logical shift right.
This instruction enables store-to-load forwarding optimization with offsets, such as the one performed in the following example.
To rule out sub-byte accesses, its use is restricted to shift amounts that are multiples of 8.

```llvm
define i8 @src(b32 %x) {
  %a = alloca b32
  %gep = getelementptr i8, ptr %a, i64 2
  store b32 %x, ptr %a
  %v = load i8, ptr %gep
  ret i8 %v
}

define i8 @tgt(b32 %x) {
  %shift = lshr b32 %x, 16
  %trunc = trunc b32 %shift to b8
  %cast = bytecast exact b8 to i8
  ret i8 %cast
}
```

## Value Coercion Optimizations

Some optimization passes perform transformations that are unsound under the premise that type punning is disallowed.
Such an optimization pass is GVN, which performs value coercion in order to eliminate redundant loads.
Currently, a class of optimization where a pointer load is coerced to a non-pointer value or a non-pointer load is coerced to a pointer value [is reported as unsound by Alive2](https://github.com/llvm/llvm-project/issues/124461).

The following example illustrates one such optimization, in which GVN replaces the pointer load at `%v3` by a phi node, merging the pointer load at `%v2` with the coerced value at `%1`, resulting from an `inttoptr` cast.
If the value stored in memory is a pointer, the source function returns the pointer value, while, in the target function, the load at `%v1` returns `poison`.

```llvm
declare void @use(...) readonly

define ptr @src(ptr %p, i1 %cond) {
  br i1 %cond, label %bb1, label %bb2
bb1:
  %v1 = load i64, ptr %p
  call void @use(i64 %v1)
  %1 = inttoptr i64 %v1 to ptr
  br label %merge
bb2:
  %v2 = load ptr, ptr %p
  call void @use(ptr %v2)
  br label %merge
merge:
  %v3 = load ptr, ptr %p
  ret ptr %v3
}

define ptr @tgt(ptr %p, i1 %cond) {
  br i1 %cond, label %bb1, label %bb2
bb1:
  %v1 = load i64, ptr %p
  call void @use(i64 %v1)
  %1 = inttoptr i64 %v1 to ptr
  br label %merge
bb2:
  %v2 = load ptr, ptr %p
  call void @use(ptr %v2)
  br label %merge
merge:
  %v3 = phi ptr [ %v2, %bb2 ], [ %1, %bb1 ]
  ret ptr %v3
}
```

The byte type can be leveraged to avoid the implicit type punning that hinders this kind of optimizations, as depicted in the following example.
Since the byte type can represent both pointer and non-pointer values, the loads at `%v1` and `%v2` can instead be performed using the byte type.
The `bytecast` instruction is then used to convert the byte into the desired type.
As the load through the byte type accurately models the loaded value, avoiding implicit casts, the `bytecast`, yields the pointer stored in memory.
This value can then be used to replace the load at `%v3`.

```llvm
declare void @use(...) readonly

define ptr @src(ptr %p, i1 %cond) {
  br i1 %cond, label %bb1, label %bb2
bb1:
  %v1 = load i64, ptr %p
  call void @use(i64 %v1)
  %1 = inttoptr i64 %v1 to ptr
  br label %merge
bb2:
  %v2 = load ptr, ptr %p
  call void @use(ptr %v2)
  br label %merge
merge:
  %v3 = load ptr, ptr %p
  ret ptr %v3
}

define ptr @tgt(ptr %p, i1 %cond) {
  %load = load b64, ptr %p
  br i1 %cond, label %bb1, label %bb2
bb1:
  %v1 = bytecast exact b64 %load to i64
  call void @use(i64 %v1)
  %1 = bytecast exact b64 %load to ptr
  br label %merge
bb2:
  %v2 = bytecast exact b64 %load to ptr
  call void @use(ptr %v2)
  br label %merge
merge:
  %v3 = phi ptr [ %v2, %bb2 ], [ %1, %bb1 ]
  ret ptr %v3
}
```

## Other Optimizations

Additional optimizations were also implemented.
While these do not affect program correctness, they do contribute to performance improvements.
Some of them include cast pair eliminations and combining of load and `bytecast` pairs with a single use, depicted in the following examples.

```llvm
define b32 @src_float(b32 %b) {
  %1 = bytecast exact b32 %b to float
  %2 = bitcast float %1 to b32
  ret b32 %2
}

define i8 @src_int(i8 %i) {
  %b = bitcast i8 %i to b8
  %c = bytecast exact b8 %1 to i8
  ret i8 %c
}

define b32 @tgt_float(b32 %b) {
  ret b32 %b
}

define i8 @tgt_int(i8 %i) {
  ret i8 %i
}
```

```llvm
define i8 @src(ptr %p) {
  %b = load b8, ptr %p
  %c = bytecast exact b8 %b to i8
  ret i8 %c
}

define i8 @tgt(ptr %p) {
  %i = load i8, ptr %p
  ret i8 %i
}
```

## Clang

Given the raw memory access capabilities of the byte type, Clang was altered to lower C and C++'s raw memory access types were lowered to the byte type.
These include `char`, `signed char`, `unsigned char` and `std::byte`.
The new lowerings are depicted in the next example.

```c
void foo(
  unsigned char arg1,
  char arg2,
  signed char arg3,
  std::byte arg4
);
```

```llvm
void @foo(
  b8 zeroext %arg1,
  b8 signext %arg2,
  b8 signext %arg3,
  b8 zeroext %arg4
);
```

Additionally, code generation was updated to insert missing `bytecast` instructions where integer values were previously expected, such as in arithmetic and comparison operations involving character types.
The next example depicts an example function in C, adding two `char` values, and the corresponding lowering to LLVM IR as performed by Clang.

```c
char sum(char a, char b) {
  return a + b;
}
```

```llvm
define b8 @sum(b8 %a, b8 %b) {
  %conv = bytecast exact b8 %a to i8
  %conv1 = sext i8 %conv to i32
  %conv2 = bytecast exact b8 %b to i8
  %conv3 = sext i8 %conv2 to i32
  %add = add nsw i32 %conv1, %conv3
  %conv4 = trunc i32 %add to i8
  %res = bitcast i8 %conv4 to b8
  ret b8 %res
}
```

## Summary

In summary, the byte type contributes with the following changes/additions to the IR:

- **Raw memory representation:** Optimization passes can use the byte type to represent raw memory values, avoiding the introduction of implicit casts and treating both pointer and non-pointer values uniformly.

- **Bit-level `poison` representation:** The byte type provides the necessary granularity to represent individual `poison` bits, providing greater flexibility than integer types, which either have a fully-defined value or are tainted by `poison` bits.

- **`bitcast` instruction:** This instruction allows conversions from other primitive types to equivalently-sized byte types. Casts between vector and scalar byte types do not taint the cast result in the presence of `poison` lanes, as occurs with integer types.

- **`bytecast` instruction:** This instruction enables the conversion of byte values to other primitive types. The standard version of the cast performs type punning, reinterpreting pointers as integers and vice-versa. The `exact` flag disallows type punning by returning `poison` if the type of the value held by the byte does not match the cast destination type.

- **`trunc` and `lshr` instructions:** The `trunc` and `lshr` instructions accept byte operands, behaving similarly to their integer counterparts. The latter only accepts shift amounts that are multiples of 8, ruling out sub-byte accesses.

# Results

## Benchmarks

The implementation was evaluated using the Phoronix Test Suite automated benchmarking tool, from which a set of 20 C/C++ applications, listed below, were selected.

| **Benchmark**    | **Version** | **LoC**   | **Description**                               |
| ---------------- | ----------- | --------- | --------------------------------------------- |
| aircrack-ng      | 1.7         | 66,988    | Tool suite to test WiFi/WLAN network security |
| botan            | 2.17.3      | 147,832   | C++ library for cryptographic operations      |
| compress-7zip    | 24.05       | 247,211   | File archiving tool based on the 7-Zip format |
| compress-pbzip2  | 1.1.13      | 10,187    | Parallel implementation of bzip2              |
| compress-zstd    | 1.5.4       | 90,489    | Lossless compression tool using Zstandard     |
| draco            | 1.5.6       | 50,007    | 3D mesh and point cloud compressing library   |
| espeak           | 1.51        | 45,192    | Compact open-source speech synthesizer        |
| ffmpeg           | 7.0         | 1,291,957 | Audio and video processing framework          |
| fftw             | 3.3.10      | 264,128   | Library for computing FFTs                    |
| graphics-magick  | 1.3.43      | 267,450   | Toolkit for image editing and conversion      |
| luajit           | 2.1-git     | 68,833    | JIT-compiler of the Lua programming language  |
| ngspice          | 34          | 527,637   | Open-source circuit simulator                 |
| openssl          | 3.3         | 597,713   | Implementation of SSL/TLS                     |
| redis            | 7.0.4       | 178,014   | In-memory data store                          |
| rnnoise          | 0.2         | 146,693   | Neural network for audio noise reduction      |
| scimark2         | 2.0         | 800       | Scientific computing suite written in ANSI C  |
| sqlite-speedtest | 3.30        | 250,607   | Program for executing SQLite database tests   |
| stockfish        | 17          | 11,054    | Advanced open-source chess engine             |
| tjbench          | 2.1.0       | 57,438    | JPEG encoding and decoding tool               |
| z3               | 4.14.1      | 512,002   | SMT solver and theorem prover                 |

All programs were compiled with the `-O3` pipeline on an AMD EPYC 9554P 64-Core CPU.
In order to minimize result variance, turbo boost, hyperthreading, and ASLR were disabled, the performance governor was used, and core pinning was applied.
The plots, depicted below, display the compile time, object size, peak memory usage (maximum redisent set size) and run-time performance differences between the implementation and upstream LLVM.
The results reveal that the addition of the byte type had a minimal impact on all of the addressed performance metrics.
Each result is averaged over three runs. The run-time results represent the average regression percentage across all tests of each benchmark.

<div style="margin:0 auto;">
  <img src="/img/byte-type-plots.svg"><br/>
</div>

The following plots show per-function assembly size distributions and differences, indicating that the addition of the `byte` type results in minor changes to the generated code, with the largest observed shift being approximately 5%.
Each subplot includes the net byte size change and the percentage of functions with differing assembly, disregarding non-semantic differences such as varying jump and call target addresses.

<div style="margin:0 auto;">
  <img src="/img/byte-type-asm-size.svg"><br/>
  <img src="/img/byte-type-asm-diff.svg"><br/>
</div>

## Alive2

### LLVM Test Suite

The byte type was implemented in Alive2, enabling the verification of both the reworked and newly added optimizations.
Accessing both the correctness of the implementation and the broader impact of introducing the byte type into the IR, Alive2 was run over the LLVM test suite.
Several previously unsound optimizations, which were addressed by the byte type, were identified in the tests listed below.

| **Test**                        | **Reason**                           |
| ------------------------------- | ------------------------------------ |
| ExpandMemCmp/AArch64/memcmp.ll  | `memcmp` to integer load/store pairs |
| ExpandMemCmp/X86/bcmp.ll        | `bcmp` to integer load/store pairs   |
| ExpandMemCmp/X86/memcmp-x32.ll  | `memcmp` to integer load/store pairs |
| ExpandMemCmp/X86/memcmp.ll      | `memcmp` to integer load/store pairs |
| GVN/metadata.ll                 | Unsound pointer coercions            |
| GVN/pr24397.ll                  | Unsound pointer coercions            |
| InstCombine/bcmp-1.ll           | `bcmp` to integer load/store pairs   |
| InstCombine/memcmp-1.ll         | `memcmp` to integer load/store pairs |
| InstCombine/memcpy-to-load.ll   | `memcpy` to integer load/store pairs |
| PhaseOrdering/swap-promotion.ll | `memcpy` to integer load/store pairs |
| SROA/alignment.ll               | `memcpy` to integer load/store pairs |

It is worth noting that some additional tests containing unsound optimizations were addressed.
However, Alive2 did not report them as unsound, due to the presence of unsupported features, such as multiple address spaces.
Moreover, the `ExpandMemCmp` tests continue to be flagged as unsound by Alive2.
This is because the required `!uninit_is_nondet` metadata has not yet been upstreamed and therefore remains absent in `memcmp` load widenings optimizations.

### Single File Programs

The `alivecc` tool was used to verify the compilation of two single-file C programs, both compiled with the `-O2` optimization level.
The results are presented below.

- [`bzip2`](https://people.csail.mit.edu/smcc/projects/single-file-programs/bzip2.c): No differences were detected during verification.
- [`sqlite3`](https://raw.githubusercontent.com/azadkuh/sqlite-amalgamation/refs/heads/master/sqlite3.c): Two optimizations previously flagged as unsound by Alive2 were fixed. These occurred in the `sqlite3WhereOkOnePass` and `dup8bytes` functions. The reduced IR reveals that these were caused by lowerings of `memcpy` to integer load/store pairs.

# Future Work

After modifying Clang to lower the `char`, `unsigned char` and `signed char` types to the byte type, approximately 1800 Clang regression tests began failing.
Over the course of the project, the number of failing tests was gradually reduced and, currently, around 100 regression tests are still failing.
LLVM is a fast-moving codebase, and due to the sheer number of Clang tests affected by the introduction of the byte type, maintaining a clean test suite constitutes a continuous effort.

The benchmarks were run on an x86-64 system.
However, LLVM also supports other popular architectures such as AArch64 and RISC-V, which may require additional performance evaluation.

Furthermore, the patches do not include any additions to the Language Reference.

# Conclusion

The addition of the byte type to the IR solves one of the long lasting problems in LLVM, with a minimal performance impact.
Optimization passes can now safely represent and manipulate raw memory values, fixing existing optimizations, and setting up a solid foundation for new, previously inexpressible optimizations.

Participating in GSoC was both a great honor and a tremendous learning opportunity.
Over the course of this project, I've learned a lot about compilers, optimizations and LLVM.
It was also a valuable opportunity to get in touch with the LLVM community and contribute through the following pull requests:

- [[InstCombine] Fold `(x == A) || (x & -Pow2) == A + 1` into range check](https://github.com/llvm/llvm-project/pull/153842)
- [[ADT] Add signed and unsigned mulExtended to APInt](https://github.com/llvm/llvm-project/pull/153399)
- [[Headers][X86] Allow pmuludq/pmuldq to be used in constexpr](https://github.com/llvm/llvm-project/pull/153293)
- [[LangRef] Fix `ptrtoaddr` code block](https://github.com/llvm/llvm-project/pull/152927)
- [[clang][x86] Add C/C++ and 32/64-bit test coverage to constexpr tests](https://github.com/llvm/llvm-project/pull/152478)
- [[Headers][X86] Allow AVX512 reduction intrinsics to be used in constexpr](https://github.com/llvm/llvm-project/pull/152363)
- [[InstCombine] Support offsets in `memset` to load forwarding](https://github.com/llvm/llvm-project/pull/151924)
- [[ConstantFolding] Merge constant gep `inrange` attributes](https://github.com/llvm/llvm-project/pull/150546)
- [[InstCombine] Propagate neg `nsw` when folding `abs(-x)` to `abs(x)`](https://github.com/llvm/llvm-project/pull/150460)
- [[LV] Peek through bitcasts when performing CSE](https://github.com/llvm/llvm-project/pull/146856)

I would like to thank my mentor, Nuno Lopes, for his guidance and support.
Not only did his experience and expertise help me get through some of the most challenging parts of the project, but his presence also made the whole process genuinely enjoyable.
I also believe few people in the world could guide me so well through the Alive2 codebase!

I would also like to thank George Mitenkov, who laid the groundwork by developing the [original prototype introducing the byte type](https://github.com/georgemitenkov/llvm-project/commits/gsoc2021-dev).
Not only did he accomplish quite a lot in a single summer, but he also wrote a [phenomenal write-up](https://gist.github.com/georgemitenkov/3def898b8845c2cc161bd216cbbdb81f), which greatly contributed to my understanding of the problem.

[^1]: _Values stored in non-bit-field objects of any other object type consist of n x `CHAR_BIT` bits, where `n` is the size of an object of that type, in bytes. The value may be copied into an object of type `unsigned char [n]` (e.g., by `memcpy`); the resulting set of bytes is called the object representation of the value._ (C99 ISO Standard, 6.2.6.1.4)

[^2]: _The underlying bytes making up the object can be copied into an array of `char`, `unsigned char`, or `std::byte`. If the content of that array is copied back into the object, the object shall subsequently hold its original value._ (C++20 ISO Standard, 6.9.2)
