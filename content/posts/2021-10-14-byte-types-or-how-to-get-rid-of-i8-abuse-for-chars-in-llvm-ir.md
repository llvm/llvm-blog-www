---
author: George Mitenkov
date: "2021-10-14"
tags: ["Clang", "llvm"]
title: Byte types, or how to get rid of i8 abuse for chars in LLVM IR
---

# Part 0, where we introduce our work

This year I participated in the Google Summer of Code (GSoC), working with Nuno
Lopes and Juneyoung Lee on fixing fundamental issues in LLVM. My project was
about making compiler-introduced load type punning correct and fixing associated
bugs that are reported by [Alive2 dashboard][alive2].

While our [proposal][proposal] was accepted for GSoC, our [initial RFC][rfc] on
the mailing list was received somewhat negatively and sparked many questions on
the current semantics around `ptrtoint`, `inttoptr`, pointer provenance and memory
in LLVM. In particular, the following was mentioned:

- Proposed semantics for the byte type seemed obscure and the explanation of the
  underlying issue of load type-punning was unclear.

- Adding a new type to LLVM IR seemed like a significant amount of work and many
  used this as a strong argument against introducing it.

- It was also pointed out that the load type-punning issue is only relevant to C,
  C++ or Rust, and changing Clang and LLVM IR just for these 3 frontends is not
  worth the effort.

Nevertheless, we proceeded with implementing a prototype. Our aim was to
introduce a new type to LLVM, fix any optimization issues/bugs and analyse the
performance regression. By the end of GSoC, we think that our results are very
promising and can be used as strong evidence in favour of a byte type being part
of LLVM IR. To clarify semantics of a byte type, and to demystify a common
disbelief that the proposed changes break everything or not needed at all, we
decided to write a blog post, describing the semantic inconsistencies that LLVM
has around pointers and memory, as well as our solution. We hope that these posts
will help us to make the LLVM community aware of the load type-punning issue and
why a new type can help to solve it.


# Part 1, where we argue whether the memory in LLVM IR is typed or not

Before describing the inconsistencies around pointers, provenance and
inttoptr/ptrtoint, let’s first establish whether the memory in LLVM IR is typed
or not. According to [LangRef][langref]:

> LLVM IR does not associate types with memory. The result type of a load
  merely indicates the size and alignment of the memory from which to load, as
  well as the interpretation of the value. The first operand type of a store
  similarly only indicates the size and alignment of the store.

Because the memory is untyped, any load/store of any type is equivalent to
converting the type to an integer (i.e. a bit sequence of some size and
alignment) and then loading the integer from memory or storing it to memory. In
the original RFC thread, Joshua Cranmer shared 2 examples of equivalent
functions under this semantics:

```llvm
define void @foo(ptr %mem, i8* %foo) {
    store i8* %foo, ptr %mem
}

define void @bar(ptr %mem, i8* %foo) {
    ; Assuming pointers are 64-bit
    %asint = ptrtoint i8* %foo to i64
    store i64 %asint, ptr %mem
}
```

Moreover, he also pointed out why this semantics is not sound. In LLVM, integers
are just a collection of bits, and any iN value can be replaced with any other
iN value given they have the same bit pattern. At the same time, two pointers
may have the same numerical value (when cast to integers) but cannot be replaced
with one another in general. This is because pointers also carry provenance
(i.e., information about which objects the pointer can refer to). If the
provenance of pointers is different, replacing one pointer with another and then
dereferencing it can lead to undefined behaviour.

Under the untyped memory model, we need to accept that every load/store has an
implicit `ptrtoint`/`inttoptr` attached to it. Hence, we lose provenance information
every time a pointer is stored to memory, as if we did an `inttoptr(ptrtoint x)`.
Thus, the following two functions (again thanks to Joshua Cranmer for examples)
are not semantically equivalent under this model:

```llvm
define i8* @foo(i8* %in) {
    ret i8* %in
}

define i8* @foo(i8* %in) {
    %mem = alloca i8* 
    store i8* %in, i8** %mem
    %out = load i8*, i8** %mem
    ret i8* %out
}
```

For equivalence, we must ensure that the load of a pointer recovers the
provenance data stored by the store of the pointer. But then the integer and the
pointer loads must be different which is not true if the memory is untyped.

Having untyped memory also affects the decision whether a pointer in a function
escapes or not. Recall that we say that a pointer escapes if it can be accessed
outside of the current function. If the pointer is stored to a global, then it is
evident that it can now be accessed globally, and therefore we consider it as escaped.
However, pointers that live on the stack of the current function and are not stored
to global memory may escape too. Consider the following function:

```llvm
  %q = alloca i32*
  store i32 0, i32* %p
  store i32* %p, i32** %q
  %q2 = bitcast i32** %q to i64*
  %p_as_int = load i64, i64* %q2
  %cmp = icmp i64 %p_as_int, 0x42
  br i1 %cmp, label %true, label %false
true: 
  call void @foo(i64 0x42)
  br label %false
false:
  %w = load i32, i32* %p
```

The catch here is that the pointer `%p` is implicitly cast to integer, and is passed
to some other function that may change the value stored at `%p` without alias
analysis noticing. To avoid this problem, we could consider all integer loads as
potential pointer carriers that have an implicit `ptrtoint` instruction attached to it.
This way alias analysis is aware of pointer escaping but has to be conservative on
every integer load, which is dreadful in terms of performance and invalidates certain
LLVM optimizations that do not consider loads as escape sites. An alternative is to
say that all pointer stores escape, which again has severe performance consequences
and again do not align with all LLVM optimizations.

To conclude our small introduction, it is evident that the current untyped memory
model is not enforced consistently. What is more disastrous is the ambiguity of
integers carrying pointers. In LLVM, certain optimizations assume that integer
load/stores have untyped semantics, whereas other optimizations (and frontends) take
provenance information into account. But how bad is this inconsistency?

# Part 2, where we question the possibility of implementing a memcpy in LLVM IR, as well as describe miscompilations due to lowering of unsigned char and compiler-introduced type punning

As we have already established, integers do not carry provenance in LLVM but
pointers do. Now suppose we have a memcpy implementation (a for loop that copies
data per-byte using `i8` in LLVM terms). If we are to copy a 64-bit pointer as
eight 8-bit integers, then what provenance does the result of the copy have? To
keep the provenance data one may say that we can allow integers to carry
provenance information. But this contradicts the fact that an integer is just a bit
pattern. Nicolai Hähnle, in his [blog post][post] describes this in greater detail,
as well as evaluates how this inconsistency can be fixed. His conclusion, in brief,
is that there is a limitation in the current expressiveness of LLVM IR, which has
surprising consequences for memcpy (and similar memory-related operations). To
deal with the limitation, we have to choose between:

1. Both pointers and integers carrying provenance

2. Pointers having provenance but not integers

3. Nothing having provenance

Unfortunately, there is no free lunch, and any option we pick invalidates some
LLVM optimizations.


So far we only talked about semantic inconsistencies in LLVM, but is this that
important for compiling code? The obvious answer is yes. For example, there are
miscompilations because integers that carry pointers can be treated as pure
integers by certain optimizations. Let us consider a simple optimization:

```
(p == q) ? p : q    ->   q
```

While it is clearly correct for “pure” integers, it is not correct for integers
carrying pointers. Suppose we have 2 arrays, `P` and `Q`. If `P` immediately
follows `Q` in memory, then `q = Q[4] = P[0] = p` (recall that taking the address
of one-past-the end element of an array is legal in C/C++ (unlike dereferencing it)
[C99 §6.5.6, p8][c99]. Hence, `p` and `q` have the same integer representation,
but their provenances are different which makes the optimization incorrect.
Can we exploit this optimization?

![img1](../../static/img/2021-10-14-byte-types-figure1.png)

Obviously the answer is yes (otherwise there would have been no blog post!),
and compiler-introduced type punning can help us with that. If we copy a pointer
byte per byte (e.g. with `memcpy`), the compiler can optimize this into a single
load/store pair. 

```llvm
%src8 = bitcast i8** %src to i8*
%dst8 = bitcast i8** %dst to i8*
call void @llvm.memcpy.p0i8.p0i8.i32(i8* %dst8, i8* %src8, i32 8, i1 false)
	=>
%src64 = bitcast i8** %src to i64*
%dst64 = bitcast i8** %dst to i64*
%addr = load i64, i64* %src64, align 1
store i64 %addr, i64* %dst64, align 1
```

The underlying issue is that while both C and C++ define `unsigned char` (or
`std::byte`) as handles to the raw bytes of objects, LLVM IR does not have a
similar type and uses integers for that. This means that compiler-introduced
type punning treats raw data copied with a memcpy as integers (e.g. i64 if
pointers are 64-bit wide). Since LLVM’s alias analyses do not take integer
operations into account, this type punning can lead to pointer escape without
compiler realising and lead to miscompilation. 

Note that the issue is not limited to `memcpy`, but also affects `memmove`,
`memcmp` and C++ functions that may be lowered to calls to `memcpy` or `memmove`
and subsequently to integer load/store pairs, such as
[`uninitialized_copy` in C++][https://llvm.godbolt.org/z/nGr6K4cnP].


Now that we have roughly outlined how we can get the desired miscompilation,
we describe an end-to-end example from the bug report [37469][bug]:

```cpp
// If we call store_10_to_p(p, p + 4) and p and p + 4 have the same address
// c1 == c2 in the loop, so arr = p_as_arr.
// Hence r = p, and *p should be 10. However, when compiled with -O3, *p is 1.
void store_10_to_p(int *p, int *q) {
    unsigned char p_as_arr[8];
    unsigned char q_as_arr[8];
    unsigned char arr[8];

    memcpy(p_as_arr, &p, sizeof(p));
    memcpy(q_as_arr, &q, sizeof(q));

    // Store p to arr.
    for (int i = 0; i < sizeof(q); ++i) {
        int c1 = p_as_arr[i];
        int c2 = q_as_arr[i];
        // Note that c1 == c2 is a comparison between integers (not pointers).
        if (c1 == c2) arr[i] = p_as_arr[i]; else arr[i] = q_as_arr[i];
    }

  // Now arr is equivalent to p_as_arr, which is p.
  int *r;
  memcpy(&r, arr, sizeof(r));
  // Now r is p.
  *p = 1;
  *r = 10;
}

int main() {
    int P[4], Q[4];
    printf("%p %p\n", P, &Q[4]);
    // If P immediately follows Q, store 10 to P[0].
    if ((uintptr_t)P == (uintptr_t)&Q[4]) {
        store_10_to_p(P, &Q[4]);
        printf("%d\n", P[0]);
    }
    return 0;
}
```

In this example, we have 2 arrays, `P` and `Q`. If `P` immediately follows `Q`
in memory, we store 1 and then 10 to `P[0]` and print the contents of `P[0]`.
Unsurprisingly, compiling with `-O0` produces a correct answer of 10. However,
compiling the same code with `-O3` produces 1! Let’s analyse the example in more
detail.

`P` immediately follows `Q`, and hence the integer representation of pointers
`p = P[0] and q = Q[4]` is the same. We first transform 64-bit integer
representations of pointers to arrays of chars by calling `memcpy`. Then, we proceed
by storing a pointer `p` into array arr by copying every byte of `p`. Note that
condition `(c1 == c2)` is always true since both `p` and `q` have the same integer
representation even though they point to different objects. Then, we copy the
contents of `arr` (which is a pointer `p`) to a new pointer `r`. Finally, we
store 1 to `p`, and 10 to `r` (which is `p`).

So why does this miscompilation happen? There are four main contributing factors
(read: LLVM optimizations), some of which we have already covered:

1. Instcombine has an optimization that, given 2 integers `x` and `y`, simplifies
   the expression `(x == y) ? x : y` into `y`. Therefore, in this case, the loop
   body becomes `arr[i] = q_as_arr[i];`.

2. Loop idiom recognizer sees the copy loop and replaces it with
   `memcpy(arr, q_as_arr, 8)`.

3. Instcombine replaces the `memcpy` calls with `i64` load/store pair.

4. Instcombine does the store forwarding of `q_as_arr` to `r` (as those are just
   integers now!). In the end, the optimizer thinks that `r` is `q`, so storing 10
   to `r` does not affect `p`.

To emphasize once again, LLVM IR does not have a universal union type like C
(`unsigned char`) or C++ (`std::byte`) have, and that can be used to access/copy
the raw data in memory. Instead, integers are used which makes it possible for
them to carry pointers. Therefore, we can conclude that either we have to:

- Change semantics and lower `unsigned char` and `std::byte` to something other
  than `i8` so that optimizations are aware that raw data (potentially a pointer)
  is loaded/stored.

- Keep semantics (i.e., the current lowering to integers) but change (or disable)
  unsound optimizations.
 

Abuse of integers as universal data holders also doesn’t fit the semantics of
poison. Suppose that we have a program that copies a struct:

```cpp
struct S {
    char s;
    // 3-byte padding
    int i;
};

int struct_cpy(struct S* p, struct S* q) {
    memcpy(s, q, sizeof(struct S);
}
```

The semantics of memcpy specify that the memory is copied as-is in bytes,
including the padding bits if necessary. However, if we widen the `memcpy` of
a struct to a big integer load (`i64` in this example), the poisonous padding
contaminates the copied value, and produces poison.


So far we have only presented C examples, however this issue is not limited to
this language. Ralf Jung came up with a [series of examples in Rust][rust-examples]
which show that optimizations like dead store elimination, integer "substitution"
(replacing `a` by `b` after an `a == b`), and provenance-based alias analysis,
are in conflict with each other: the current LLVM semantics are inconsistent and
can lead to miscompilations. He also stressed that this issue is not just a small
bug somewhere, it is a case of the implicit assumptions made by different
optimization passes or instructions being mutually incompatible.


# Part 3, in which we describe how NOT to fix LLVM

We hope that at this point the reader is aware of how serious the issue is and
that LLVM optimizations (especially when combined together) are not sound. Surely,
we need to try to solve that issue and the argument “but it does not occur that
often” should frighten us!

## Solution 0: Fix or disable optimizations

The first possible solution is to keep memory untyped, to get consistent
semantics by treating integers as potentially carrying pointers and to give up
unsound optimizations (or fix them). This approach has its own benefits. First,
it does not require much engineering effort - at least for disabling
optimizations. Secondly, all optimizations would accept the same semantics for
integers, and therefore would still be sound when combined together.

However, disabling optimizations can have a significant performance regression.
The first candidate to be given up would be GVN, which is not sound if integers
are treated as pointers. As many readers and developers can agree, this
optimization seems to be too important for performance to simply disable it.

Pros:

+ Low engineering effort.

+ LLVM IR type system and language constructs are untouched.

Cons:

- More pressure on the developers who have to ensure that the optimizations they write
  adhere to the semantics of integers escaping pointers.

- Performance regressions due to disabled optimizations like GVN and more conservative AA.

## Solution 1: Better alias analysis

Having examined the first solution, we may ask why do we need to disable
optimizations and make integers to be treated as pointers if we can simply
make alias analysis better? This is a great solution indeed: we keep the memory
untyped, we do not give up performance, we do not have to complicate semantics
and on top of that alias analysis works better than ever!

A single problem that we are left with is HOW to make the alias analysis better.
Let’s try to find a way:

1. Easy, just make alias analysis more conservative? Not really. While this solution
   leads to performance degradation as we would have more “potential escape” sites in
   the code and less optimizations can kick in, it can still be considered “better”
   because it will be sound. 

2. To avoid performance regressions be conservative only when needed? This is not a
   complete solution. Yes, we can explicitly mark potential type punning situations,
   such that alias analysis (and optimizers) can see and be conservative about.
   However, it is not clear what those situations would be and right now we would need
   to be conservative in every function that loads an argument pointer.

3. Then make alias analysis better without being conservative? That is a great idea but no.
   At the moment, there is no clear answer of how to do that (any suggestions are welcome).

Pros:

+ Only alias analysis code needs changes.

+ LLVM IR type system and language constructs are untouched.

+ LLVM optimizations are untouched (although it is debatable whether they would
  kick in if the analysis is too conservative).

Cons:

- High engineering cost for designing better alias analysis.

- Better alias analysis would not necessarily catch all corner cases.

## Solution 2: Explicit inttoptr/ptrtoint casts

So far we have seen that disabling optimizations is undesirable, and developing
a better alias analysis is not realistic. So let’s consider an alternative
(again, for untyped memory): make integer to pointer (and back to integer)
conversions explicit in IR. This way, it is always known that a pointer might
escape only with `inttoptr`/`ptrtoint` instructions (as mentioned by John McCall
on the mailing list).

However, there are cases when the type of the loaded value is not known explicitly
to Clang or to LLVM, e.g. when an `unsigned char` is loaded from memory in C.
Since we consider `inttoptr`/`ptrtoint` as only cases when pointers may escape, type
punning cases are not caught, which breaks alias analysis once again.

Pros:

+ Not a high engineering effort.

+ LLVM IR type system and language constructs are untouched.

Cons:

- Provenance information is lost when we have `inttoptr`/`ptrtoint` instructions.

- `inttoptr`/`ptrtoint` escape pointers and using them explicitly can be too
  conservative (i.e. lead to performance degradation).

- No guarantee that type punning cases are caught.

## Solution 3: Annotations and tags

Having examined the first 3 alternatives, we can see that we want something that:

1. does not make us disable optimizations like GVN

2. has a realistic implementation (unlike better alias analysis)

3. is stronger than explicit `ptrtoint`/`inttoptr`

4. as well as adheres to the same optimization restrictions as pointers and keeps
   provenance.

In the initial RFC thread, Madhur Amilkanthwar proposed a solution that seems
to match all the necessary conditions we have mentioned. In his proposal, he
suggested annotating types with attributes, metadata, or flags which optimizations
can use to do a better job. These tags would carry the semantic meaning for
the type. For example, lowering `memcpy` to integer load/store pair would also add
a tag that marked these instructions as potential escape sites (since we may be
loading/storing a pointer).

While this approach is similar to the one we propose later in this post, there is
a very important drawback: LLVM optimizers work with the assumption that attributes
can be discarded if the optimizer does not know how to handle them. However,
under this proposal discarding attributes becomes illegal.

Pros:

+ Satisfies necessary conditions to solve our problem.

Cons:

- Optimizations can drop attributes.

- IR becomes less readable.

- High engineering effort to enforce that attributes are preserved in
  every transformation and used by analyses. 


# Part 4, in which we introduce the byte type

## Motivation

Having examined the alternatives, we propose to consider LLVM memory as typed,
as well as to add a new type (called a byte type, or a “raw data” type)
that represents the raw memory in LLVM IR. We emphasize on the motivation
for this type once again:

1. LLVM IR needs a universal data holder type.

   Frontends load data from memory and do not necessarily know the type being loaded.
   When a `char` is loaded in C, it can be part of a pointer or an integer. As we
   have shown previously, representing it as an integer leads to miscompilations and
   does not fit the pointer provenance model. Moreover, the byte type enables us
   to express `memcpy` in IR as a copy of the raw data.


2. Optimizations for the universal holder type need to be correct for any LLVM IR type.

   Since the universal type can hold anything, optimizations that apply to this type must
   be correct for any data it stores: integers, pointers, etc. Currently, not all of them
   are correct for this universal type. An example is GVN, which is not correct for pointers
   in general because if two pointers compare equal, it does not mean they have the same
   provenance. As LLVM’s alias analysis uses provenance information, this information
   must be preserved.

This leads us to our proposal - a new type that represents raw data. While it requires
some work, it also allows us to keep all integer optimizations as aggressive as we want,
and only throttle down when encountering the byte type. Another benefit is that
loading/storing raw data from memory does not escape any pointer, while with the
integers as universal type you need to consider all stored pointers as escaped when
loading an integer. Introducing the byte type allows us to fix all the integer/pointer
cast bugs in LLVM IR.

Pros:

+ LLVM IR becomes more expressive.

+ The readability of IR does not change too much.

+ Backends and the optimizer can differentiate between integers and bytes
  (integers or pointers) to make integer optimizations more aggressive. This can
  be significant for some platforms (e.g., CHERI).

+ Only use bytes when frontends need it (e.g., C/C++ code generation for
  `unsigned char` or `std::byte`).

+ Alias analysis can be less conservative.

Cons:

- Reasonable engineering effort (but we created a prototype during GSoC 2021)

- Some optimizations need to be fixed to learn about the new type.

- IR gets a new type and a new instruction.

- Possible increase in compilation time.

## Byte type and bytecast semantics

As we have said, the byte type is a type that represents raw data. We propose
to have the same bit widths as used for integers, namely to have `b8`, `b32`,
`b64`, `b128`, etc. Moreover, we propose a new instruction to cast a byte type
to an integer or a pointer type, which we call `bytecast`. This extra cast is
needed because the byte is a universal type holder that can carry any type,
including an integer (the bytecast is simply a no-op or an `inttoptr`) or a pointer
(the bytecast is a `bitcast` or a `ptrtoint`). Moreover, bytes preserve provenance. 

We define the semantics as follows.

1. The byte type is allowed to be allocated, stored and loaded. `alloca`, `load`
   and `store` accept the byte type. 

   Examples:

   ```llvm
   %p = alloca bN* 		    ; allocate N bits in memory
   %w = load bN, bN* %q	    ; load N bits of raw memory
   store bN %w, bN* %p  	; store a N bits %w to %p
   ```

2. For type conversion, let’s denote the byte type carrying a type `T` as `byte (T)`,
   a pointer as `ptr`, and integer as `int`. We only allow conversions of types of
   the same bit width. Now, we define the semantics for type conversions as follows:
   converting any “allowed” type to the byte type is a no-op. Converting from a byte
   type to any “allowed” type may perform a type conversion. In our prototype,
   “allowed” types are integers and pointers only (or vectors of these types). To
   convert any other type (e.g. float or double) to byte, the source type first
   needs to be casted to an integer.

    - int -> byte:
    - ptr -> byte:
      Simple conversions that reinterpret an integer or a pointer as a sequence of
      bits are done with a `bitcast`, and are no-ops:
        
      ```llvm
      %b8 = bitcast i8 %i8 to b8
      %vb = bitcast <4 x iN> %vi to <b x N>
      %ptr = bitcast i8* %bptr to b64        ; assuming pointers are 64-bit wide
      ```

   The remaining conversions are done with a new `bytecast` instruction.

    - byte (int) -> int:
      If the byte carries an integer, the `bytecast` is a no-op.

      ```llvm
      ; %i = bitcast iN %b to iN
      %i = bytecast bN %b to iN
      ```

    - byte (int) -> ptr:
      If the byte carries an integer, the `bytecast` has the same semantics as `inttoptr`.

      ```llvm
      ; %ptr = inttoptr iN %b to i32*
      %ptr = bytecast bN %b to i32*
      ```

    - byte (ptr) -> int:
      If the byte carries a pointer, the `bytecast` has the same semantics as a `ptrtoint`.

      ```llvm
      ; %i = ptrtoint iN* %b to i64
      %i = bytecast b64 %b to i64
      ```

    - byte (ptr) -> ptr:
      If the byte carries a pointer, the `bytecast` is a no-op.

      ```llvm
      ; %ptr = bitcast iN* %b to i8*
      %ptr = bytecast b64 %b to i8*
      ```

3. No arithmetic or bitwise operations are allowed on the byte type.
   The main reasons for this are:

   - We defined the byte type as raw data, and it is not clear what arithmetic/bitwise
     operations would mean when applied to the raw bits in memory and how this would
     affect the provenance information.

   - Disallowing arithmetic/bitwise operations is aligned with how `std::byte` is
     defined in C++ (which roughly stems from the previous point).

   One might say that if chars are lowered to `b8`, and no arithmetic is allowed on bytes,
   the performance would degrade when the source code uses char arithmetic. However,
   that is not true. Currently, at `-O0` Clang promotes small bit width integers to
   32-bits to do arithmetic and  then truncates the result back. Consider the
   following C code:

   ```cpp
   unsigned char sum(unsigned char a, unsigned char b) { return a + b; }
   ```

   The current `-O0` and `-O3` lowerings produce the following IR:

   ```llvm
   ; -O0 version
   i8 @sum(i8 %a, i8 %b) {
       %1 = alloca i8
       %2 = alloca i8
       store i8 %a, i8* %1
       store i8 %b, i8* %2
       %3 = load i8, i8* %1
       %4 = zext i8 %3 to i32
       %5 = load i8, i8* %2
       %6 = zext i8 %5 to i32
       %7 = add nsw i32 %4, %6
       %8 = trunc i32 %7 to i8
       ret i8 %8
   }

   ; -O3 version
   i8 @sum(i8 %a, i8 %b) {
       %1 = add nsw i32 %a, %b
       ret i8 %1
   }
   ```

   With byte type we keep the same promotion pattern but with additional
   `bytecast`/`bitcast` operations. The fact that these operations apply to types of
   the same bit width is very important. It is common in LLVM codebase to check if
   instruction is a `zext`/`sext`/`trunc` and then assume that the destination type is
   an integer. If we allow `zext`/`sext`/`trunc` to produce bytes, all existing pattern
   matches would fail and would require fixing.

   The char addition example therefore becomes:

   ```llvm
   ; -O0 version
   b8 @sum(b8 %a, b8 %b) {
       %1 = alloca b8
       %2 = alloca b8
       store b8 %a, b8* %1
       store b8 %b, b8* %2
       %3 = load b8, b8* %1
       %cast1 = bytecast b8 %3 to i8
       %4 = zext i8 %cast1 to i32
       %5 = load b8, b8* %2
       %cast2 = bytecast b8 %5 to i8
       %6 = zext i8 %cast2 to i32
       %7 = add nsw i32 %4, %6
       %8 = trunc i32 %7 to i8
       %cast3 = bitcast i8 %8 to b8
       ret b8 %cast3
   }

   ; -O3 version
   b8 @sum(b8 %a, b8 %b) {
       %cast1 = bytecast b8 %a to i8
       %cast2 = bytecast b8 %b to i8
       %1 = add nsw i32 %cast1, %cast2
       %cast3 = bitcast i8 %1 to b8
       ret b8 %cast3
   }
   ```

   Since the semantics of `zext`/`sext`/`trunc` is preserved, and arithmetic
   is done over integer types, the optimizer picks up the pattern and creates
   an 8-bit wide addition. The surrounding casts simply reinterpret bits,
   and if `%a`, `%b` carry integers, the casts are no-ops and do not affect
   performance.

4. We allow performing comparisons, as we may potentially want to compare the ordering
   of the memory instances, check for null, etc. Comparison is also needed since
   `char` types are commonly compared. We define the following semantic of the byte type
   comparison.

   - If two byte types carry the same type, the result of the comparison is the result
     of the comparison of the values of the carried types.

   - If two byte types carry different types, we cast non-integral carried types to
     integers and return the result of the integer comparison.

## Preliminary results

We developed a [prototype][prototype] version of LLVM and clang during this year’s GSoC.
We changed about 2000 lines in LLVM and 100 in Clang (tests excluded). In particular:

- Both the byte type and `bytecast` instruction were introduced to LLVM IR. Currently,
  SelectionDAG uses a very basic lowering for them: byte is mapped to an integer,
  and `bytecast` is a no-op. Moreover, the code generation in Clang for C was adapted
  to produce bytes for `unsigned char`/`char` types and to add casts where necessary.

- The wrong type punning optimizations of `memcmp`, `memcpy`, `memmove` and `memset` were
  fixed. Moreover, all optimizations that were originally incompatible with a byte type
  (e.g. SROA, LoopVectorize, LoopIdiom, SLPVectorizer, GVN, etc.) were fixed to make sure
  that C programs can be compiled at any optimization level correctly.

- To test and evaluate our prototype, we used the ARM platform and
  [SPECrate2017 benchmark suite][spec2017]. In particular, we measured compile and
  execution times, as well as the size of binary files. We compared the performance
  of our prototype and vanilla LLVM, running experiments multiple times and taking
  the median value. Moreover, to quantify whether there was a regression or a
  speedup, we used ±1% error.

| Program          | Compile-time speedup, %    | Execution-time speedup, %    | Binary size increase, %           |
| ---------------- |---------------------------:|-----------------------------:|----------------------------------:|
| 500.perlbench_r. | 0.38%                      | -0.88%                       | -0.98%                            |
| 502.gcc_r.       | 0.37%                      | 0.02%                        | -2.23%                            |
| 505.mcf_r.       | -5.64%                     | -0.17%                       | -0.19%                            |
| 520.omnetpp_r.   | -0.08%                     | -0.46%                       | -1.01%                            |
| 523.xalancbmk_r. | 0.10%                      | -4.83%                       | -0.17%                            |
| 525.x264_r.      | 0.22%                      | -0.40%                       | -0.01%                            |
| 531.deepsjeng_r. | 0.56%                      | 0.26%                        | -0.01%                            |
| 541.leela_r.     | 0.02%                      | -0.01%                       | -0.01%                            |
| 557.xz_r.        | 0.19%                      | -0.91%                       | 1.84%                             |

We observed that in general, there was no compile-time slowdown on the benchmarks
and only 1 out of 9 programs showed a significant slowdown in compile time.
While the reasons for this slowdown are unknown, we can conclude that adding a new
type did not affect compile time considerably.

The size of binaries did not vary significantly, usually staying within ±1%
of the original LLVM trunk value. Again, only 1 program out of 9 showed a nearly 2%
increase in the size of the object file for unknown reasons.

Most importantly, the change in execution time of the programs mostly stayed within
±1%, and again only 1 out of 9 programs showed a 5% slowdown. We have tracked the
regression down to the vectorizer, and are currently looking for solutions.

## What is missing

Currently, there are a number of things that are missing in our prototype and
that we hope to address in the near future.

1. Clang has numerous test failures due to new code generation for `char` and
   `unsigned char`. These include simple test failures like checking for an
   `i8` instead of `b8`, but also have much more complicated cases which are
   hard to fix automatically:

   - Some target-specific intrinsics also use char in their type signature.
     We need to ensure type compatibility.

   - Currently all tests involving `i8` strings are wrong. Addressing the first point
     would help to avoid that issue, adding byte strings only where needed.

   - Some tests require insertion of `bytecast`/`bitcast` pairs: this makes rewriting
     FileCheck directives harder.

   We need to come up with a neat way of fixing all tests with the least number
   of conflicts (some inspiration can be taken from opaque pointers patches).

2. There are still pending performance issues, including execution time of `xalancbmk`,
   compile time of `mcf`, and the object file size of `xz`. We plan to investigate
   the causes of the regressions in these programs, aiming at bringing them down to 0%.

3. Introduce new optimizations for bytes and bytecasts to improve performance and make
   more optimizations byte-aware.

# Part 5, in which we draw conclusions

Having started from questioning whether memory in LLVM is typed or untyped, we
investigated subtle issues of LLVM and its semantics. We stressed that in C
`unsigned char` is a universal type - otherwise it is not possible to
implement `memcpy` in C. We then discussed the main problems and challenges that
are associated with the current lowering of `unsigned char` and `char`, the abuse of
integers as universal types in LLVM IR, and shared worrying miscompilation examples.

Having shown that the current LLVM IR is not expressive enough to solve type punning
problems, we described a possible set of solutions. In particular, we argued that
the best (in terms of effort, expressiveness and semantics) would be to consider memory
in LLVM to be typed, and to introduce a new type in LLVM IR that represents raw memory
data. We highlighted the semantics of the new type and the necessary changes in LLVM
IR semantics.

We presented a prototype we have implemented during the Google Summer of Code 2021.
Our evaluation showed that our solution does not have high engineering costs, has
comparable performance to vanilla LLVM, and solves the type punning problems.
 
We hope that now more LLVM developers understand our proposal and why LLVM needs
to have a byte type! For any questions, comments and suggestions feel free to
ping us (George, Nuno, Juneyoung) on the mailing list.


[alive2]: https://web.ist.utl.pt/nuno.lopes/alive2/
[bug]: https://bugs.llvm.org/show_bug.cgi?id=37469
[benchmarks]: https://docs.google.com/spreadsheets/d/1TyflQR0NTF2EUw4WERu2Di2od20sI1PIyXXEVbxrmBs/edit?usp=sharing
[c99]: http://www.open-std.org/JTC1/SC22/WG14/www/docs/n1256.pdf
[prototype]: https://github.com/georgemitenkov/llvm-project/commits/gsoc2021-dev
[langref]: https://llvm.org/docs/LangRef.html#pointer-aliasing-rules
[proposal]: https://docs.google.com/document/d/1C6WLgoqoDJCTTSFGK01X8sR2sEccXV5JMabvTHlpOGE/edit?usp=sharing
[post]: https://nhaehnle.blogspot.com/2021/06/can-memcpy-be-implemented-in-llvm-ir.html
[rfc]: https://lists.llvm.org/pipermail/llvm-dev/2021-June/150883.html
[rust-examples]: https://github.com/rust-lang/unsafe-code-guidelines/issues/286#issuecomment-860189806
[spec2017]: https://www.spec.org/cpu2017/results/