---
author: "Mohammed Ashraf"
date: "2026-11-09"
tags: ["GSoC", "Clang", "bounds-safety", "C"]
title: "GSoC 2026: Upstreaming -fbounds-safety"
---

Hi, I'm Mohammed Ashraf. For Google Summer of Code 2026, I worked on upstreaming
parts of `-fbounds-safety` from Apple's Swift fork of LLVM into mainline Clang.
My mentors were [Yeoul Na](https://github.com/rapidsna),
[Dan Liew](https://github.com/delcypher) and
[Henrik G. Olsson](https://github.com/hnrklssn).

My work covered the parsing infrastructure for bounds attributes, shared
validation, and support for the `counted_by` family on function parameters.

## Why pointer bounds belong in the type

A C API often passes a pointer alongside a count:

```c
void process(int count, int *data);
```

The programmer knows that `count` describes the buffer behind `data`, but the
declaration does not express that relationship. Bounds annotations make it
explicit:

```c
#define __counted_by(n) __attribute__((counted_by(n)))

void process(int count, int *__counted_by(count) data);
```

Here, the annotation describes a pointer to a buffer containing at least
`count` elements. The related `sized_by` attribute expresses a size in bytes, and the
`_or_null` variants allow a null pointer independently of the count.

The full `-fbounds-safety` extension uses bounds information to check pointer
accesses and trap before an out-of-bounds access occurs. Its programming model
combines explicit annotations at interface boundaries with pointers that carry
bounds internally for local use. The
[design document](https://clang.llvm.org/docs/BoundsSafety/BoundsSafety.html)
explains how this supports incremental adoption while preserving C ABI
compatibility.

Throughout this post, **upstream** means
[`llvm/llvm-project`](https://github.com/llvm/llvm-project), and **downstream**
means the Swift LLVM fork,
[`swiftlang/llvm-project`](https://github.com/swiftlang/llvm-project).
Downstream provides the implementation being ported. My project contributes
the frontend foundations needed to bring more of it upstream; the parameter
patches described here do not implement the full runtime-checking model.

## Preparing the parser

Consider a struct where the count comes after the pointer:

```c
struct buffer {
  int *__counted_by(len) data;
  int len;
};
```

This is a motivating example for late parsing: when the parser reaches the
annotation, it has not yet seen `len`. It can save the attribute's tokens,
continue parsing the struct, and interpret the expression at the closing `}`,
when all the fields are known.

Clang already had machinery for delayed attribute parsing, but its C and C++
paths duplicated work. My
[#186033](https://github.com/llvm/llvm-project/pull/186033) removed the separate
C attribute-parsing functions and made C struct fields use the existing
`ParseLexedAttribute` and `ParseLexedAttributeList`, resolving
[#93263](https://github.com/llvm/llvm-project/issues/93263).
The follow-up,
[#206689](https://github.com/llvm/llvm-project/pull/206689), shared the token
replay machinery through `ParseLexedAttributeTokens`.

That machinery temporarily feeds saved tokens back into the parser and restores
the surrounding token stream afterward. Sharing it means changes to this
handling do not have to be maintained separately in the C and C++ paths.
The corresponding downstream ports are
[#12446](https://github.com/swiftlang/llvm-project/pull/12446) and
[#14170](https://github.com/swiftlang/llvm-project/pull/14170).

Replaying the tokens was only part of the problem. A bounds annotation can
belong to a particular pointer within a type. In an out parameter such as
`int *__counted_by(n) *result`, the count describes the inner `int *`. Clang
must preserve that position even when it cannot yet parse the count.

## How the late-parsing design changed

The late-parsing design was still evolving while I was implementing
function-parameter support. Yeoul led the infrastructure work, and I initially
built parameter support on the placeholder approach, including implementing
`LateParsedAttrType`. When the design changed to completing
`CountAttributedType` in place, I had to rework my parameter implementation
around the new model.

### Remembering a position

The first approach,
[#166491](https://github.com/llvm/llvm-project/pull/166491), recorded the
pointer level where an attribute appeared and applied the parsed attribute
there later.

Reviewers raised two concerns: a pointer level put
bounds-specific information into the general late-parsed attribute machinery,
and it could not describe every position inside a more complicated C++ type.
Even though this work targets C, the representation needed room to grow.

### Building a placeholder

The next approach placed a temporary type node exactly where the attribute
belonged. That node, `LateParsedAttrType`, wrapped the underlying type and
retained the late-parsed attribute. Once the count was available, a
`TreeTransform`, Clang's machinery for rebuilding AST structures, would replace
the placeholder with a `CountAttributedType`.

I implemented the placeholder node in
[#204125](https://github.com/llvm/llvm-project/pull/204125).
Adding a type reaches beyond its class definition: the AST context,
visitors, type locations, serialization, printing and other consumers all need
to account for it. The node was intended to be temporary and resolved before
later compiler stages needed a finished type.

Yeoul's proposed resolver,
[#212906](https://github.com/llvm/llvm-project/pull/212906), also introduced
a dependence bit to identify types containing a placeholder. I initially built
parameter support on that approach.

### Completing the real node in place

Review exposed costs in the placeholder design. Replacing a node required
rebuilding the types containing it, while tracking unresolved attributes added
another use of Clang's already complicated type-dependence machinery.

Review led to a simpler design: create the actual `CountAttributedType`
immediately and supply its count expression later. Yeoul implemented that
direction in [#223267](https://github.com/llvm/llvm-project/pull/223267).
The enclosing types can keep pointing to the same node throughout parsing:

<div style="overflow-x: auto;">

| Stage | Placeholder approach | Complete the node in place |
| --- | --- | --- |
| First encounter | Create a temporary wrapper | Create `CountAttributedType` with its count pending |
| Count becomes available | Replace the wrapper and rebuild enclosing types | Fill in the existing node |
| Track pending work | Propagate a dependence bit | Keep a list of pending attributes and their nodes |

</div>

This introduced a constraint on sharing type nodes. Clang normally reuses many
types through a process called uniquing. Two incomplete counted types can have
the same underlying pointer type but eventually refer to different counts.
Sharing them before those counts are known would make completing one change
the other. The incomplete nodes therefore remain separate, including after
completion.

This change directly affected my parameter late-parsing work. The earlier
implementation rebuilt parameter types to replace their placeholders. In the
revised prototype, the types stayed in place, and the parameter list became
responsible for completing their count expressions once all parameters had
been parsed. The checks on which declarations a count could reference remained
useful, but the machinery for resolving and applying the attributes had to
change.

The merged infrastructure is also separate from activating type-attribute
handling and late parsing, which remain in Yeoul's
[#233582](https://github.com/llvm/llvm-project/pull/233582) and
[#233583](https://github.com/llvm/llvm-project/pull/233583).

## Separating type checks from count checks

Delayed parsing naturally divides validation into two stages. When Clang builds
the type, it can check its shape: whether the annotation is attached to a
pointer or array, and whether its pointee or element type is permitted. Later,
when the relevant declarations are in scope, it can resolve and check the
count expression.

My [#227774](https://github.com/llvm/llvm-project/pull/227774) prepares the
first stage. It adds shared validation for the four attribute variants so
subsequent patches can use the same type checks for attributes parsed
immediately and attributes parsed later.

## Bounds attributes on function parameters

The following four PRs add support for bounds attributes on function parameters.
They depend on Yeoul's type-attribute handling in
[#233582](https://github.com/llvm/llvm-project/pull/233582).
The series is still awaiting integration into mainline Clang.

Parameter counts do not yet affect `-fsanitize=array-bounds` checks or
`__builtin_dynamic_object_size`.

### Basic parameters

[#234941](https://github.com/llvm/llvm-project/pull/234941) supports counts
that can be parsed where they are written:

```c
void process(int count, int *__counted_by(count) data);
```

Here, `count` has already been declared. The patch records the annotation in
the parameter's type and checks its argument. It shares the integer and simple
declaration-reference checks with fields, and checks that the referenced
declaration is a parameter.

Reversing the parameter order needs a further step:

```c
void process_later(int *__counted_by(count) data, int count);
```

The current stack diagnoses the undeclared `count` in this example. Parameter
late parsing is separate work.

### Array parameters

In C, an array parameter is adjusted to a pointer. That adjustment must preserve
an annotation written on the parameter:

```c
void process_array(int count, int data[] __counted_by(count));
```

[#234950](https://github.com/llvm/llvm-project/pull/234950) transfers the count
to the resulting pointer type, even when other type attributes follow the
count. It rejects an array parameter that combines an explicit array size with
a count annotation.

### Out parameters

An API can return a buffer and its length through pointers supplied by the
caller:

```c
void make_buffer(int *len, int *__counted_by(*len) *buf);
```

The count here describes the buffer pointer stored through `buf`; its value
comes from dereferencing `len`.
[#234963](https://github.com/llvm/llvm-project/pull/234963) supports this
relationship, allowing a count one pointer level below the parameter and a
count expression that dereferences an earlier parameter. Deeper nesting, or
counts on both the inner and outer pointers, remains rejected.

### Redeclarations

A function can be declared more than once, such as in a header and again in
its implementation. Those declarations must agree on which parameter provides
a pointer's count. The parameter names can differ:

```c
void process(int n, int *__counted_by(n) data);
void process(int count, int *__counted_by(count) buffer);
```

Both declarations describe the same relationship: the second parameter points
to a buffer whose element count is given by the first parameter. Renaming `n`
to `count` and `data` to `buffer` does not change that relationship.

Removing the count is a conflict in an ordinary user declaration:

```c
void drop(int n, int *__counted_by(n) p);
void drop(int n, int *p);
```

[#234981](https://github.com/llvm/llvm-project/pull/234981) checks these
relationships, including counts on out parameters. It also handles functions
declared through function typedefs or `__typeof__`: the counts must refer to
the new function's own parameters. The compatibility check has an exception
when either declaration comes from a system header.

## Status

<div style="overflow-x: auto;">

| Contribution | Upstream | Downstream mirror | Status |
| --- | --- | --- | --- |
| Unify `ParseLexedAttribute` | [#186033](https://github.com/llvm/llvm-project/pull/186033) | [#12446](https://github.com/swiftlang/llvm-project/pull/12446) | Both merged |
| Add `LateParsedAttrType` for the earlier design | [#204125](https://github.com/llvm/llvm-project/pull/204125) | — | Merged |
| Unify `ParseLexedAttributeTokens` | [#206689](https://github.com/llvm/llvm-project/pull/206689) | [#14170](https://github.com/swiftlang/llvm-project/pull/14170) | Both merged |
| Shared type-shape validation | [#227774](https://github.com/llvm/llvm-project/pull/227774) | — | Merged |
| Basic function parameters | [#234941](https://github.com/llvm/llvm-project/pull/234941) | — | Open draft |
| Array parameters | [#234950](https://github.com/llvm/llvm-project/pull/234950) | — | Open draft |
| Out parameters | [#234963](https://github.com/llvm/llvm-project/pull/234963) | — | Open draft |
| Redeclaration checks | [#234981](https://github.com/llvm/llvm-project/pull/234981) | — | Open draft |

</div>

## What comes next

The immediate goal is to finish review and land the parameter series. Parameter
late parsing follows, building on the type-attribute activation work so counts
can refer to parameters declared later in the list.

Beyond that are return types, local and global variables, and the remaining
`-fbounds-safety` attributes.

## Acknowledgements

Thank you to my mentors, [Yeoul Na](https://github.com/rapidsna),
[Dan Liew](https://github.com/delcypher) and
[Henrik G. Olsson](https://github.com/hnrklssn), for their guidance, reviews and
design discussions throughout the project.

Thanks also to the Clang reviewers whose feedback shaped these changes, and to
the downstream contributors whose implementation this work builds on. Thank you to
the LLVM Foundation for organizing LLVM's participation and to Google Summer
of Code for the opportunity to contribute.
