---
author: "Dominic Stöcker"
date: "2026-08-21"
tags: ["GSoC", "Clang", "API Notes", "Swift", "C++"]
title: "GSoC 2026: Extending Clang API Notes for C++: Overload-Specific Annotations for Functions and Methods"
---

Hi! I’m Dominic Stöcker, and during Google Summer of Code 2026 I worked on extending Clang API Notes for C++ with overload-specific annotations for functions and methods.

My mentors were [Gábor Horváth](https://github.com/Xazax-hun), [John Hui](https://github.com/j-hui), and [Egor Zhdan](https://github.com/egorzhdan).

This project was originally accepted as a small, 8-week project. As the implementation scope grew, it was expanded to a large, 12-week project.

I was interested in this project because it connects Clang's semantic analysis with Swift interoperability. What made the project especially interesting to me was that a concise user-facing YAML extension had to be connected to Clang’s internal model of declarations, types, overloads, and source-level type spelling.

The goal of the project was to let API Notes target individual C++ function and method overloads. The work covered the public YAML design, parsing, binary serialization, declaration lookup, Sema integration, type normalization, diagnostics, and member-function object qualifiers.

## Background

API Notes allow additional information to be attached to declarations without modifying their original source headers. They are used in particular to refine how C, Objective-C, and C++ APIs are exposed to Swift.

Previously, an API Notes entry could identify a C++ function or method primarily by name. This is not sufficient when several overloads share that name:

```cpp
struct Widget {
  void setValue(int);
  void setValue(double);
};
```

A name-only entry for `setValue` can apply to the whole overload set, but it cannot assign different annotations to `setValue(int)` and `setValue(double)`.

The main challenge was therefore not only to select individual overloads, but to do so without changing the behavior of existing name-only API Notes files.

## Selecting C++ Overloads

The design keeps the existing `Functions`, `Tags`, and `Methods` API Notes structure. The declaration name remains the primary lookup key, while an optional `Where` block narrows the candidate set.

```yaml
Tags:
  - Name: Widget
    Methods:
      - Name: setValue
        Where:
          Parameters:
            - int
        SwiftName: setIntValue(_:)

      - Name: setValue
        Where:
          Parameters:
            - double
        SwiftName: setDoubleValue(_:)
```

This API Notes file uses the `Where.Parameters` selector to apply separate `SwiftName` annotations to the `int`- and `double`-parameter overloads of `setValue`.

The same selector is available for overloaded global functions:

```yaml
Functions:
  - Name: makeWidget
    Where:
      Parameters:
        - int
    SwiftName: makeWidgetFromInt(_:)

  - Name: makeWidget
    Where:
      Parameters:
        - double
    SwiftName: makeWidgetFromDouble(_:)
```

Name-only entries preserve their current behavior:

```yaml
Tags:
  - Name: Widget
    Methods:
      - Name: setValue
        Availability: nonswift
```

This entry still applies to every method named `setValue` in `Widget`. Omitting `Where.Parameters` leaves the parameter list unconstrained. Adding `Where.Parameters` narrows the entry to a specific explicit parameter list.

An explicitly empty parameter list has a different meaning:

```yaml
Methods:
  - Name: build
    Where:
      Parameters: []
    SwiftName: buildWithoutArguments()
```

`Where.Parameters: []` selects a declaration with no explicit source parameters. This differs from the name-only case above, which does not constrain parameters at all.

The parameter selector describes the complete explicit parameter list, so parameter count and order are significant.

## Matching C++ Types

Matching parameter types requires more than comparing raw strings.

Consider a declaration that uses an alias:

```cpp
using Count = int;

struct Counter {
  void setCount(Count);
};
```

An API Notes author may write a selector using either `Count` or `int`. Both are useful, but they do not express exactly the same intent.

The implemented lookup first tries the written or sugared type from the declaration. If no matching entry exists, it can fall back to an appropriately desugared representation.

This gives an alias-specific selector precedence when both forms exist, while still allowing an underlying-type selector to work as a fallback.

The implementation also normalizes spelling differences that should not affect overload identity. This includes insignificant whitespace, spacing around punctuation such as pointers, references, template commas, and template closers, and narrow spelling cases such as `unsigned` versus `unsigned int`.

Selector-only nullability is stripped during selector formation because API Notes can add or replace nullability independently. This includes nested nullability in pointer, array, and function-pointer type components.

Top-level `const` and `volatile` also need special handling:

```cpp
void process(int);
void process(const int);
```

These do not declare different C++ overloads. The selector representation therefore removes top-level qualifiers in cases where they do not contribute to overload identity, including the supported by-value and top-level pointer cases.

Because top-level `const` is stripped in this position, the second `process` declaration is treated as a redeclaration of the first rather than as a separate overload.

The goal is not to implement complete semantic type equivalence. Instead, the matching policy preserves useful source-level distinctions while normalizing differences that should not select separate overloads.

## Matching the Implicit Object

Explicit parameters are not enough to distinguish every C++ method overload.

Member functions can differ through qualifiers on their implicit object parameter:

```cpp
struct Builder {
  Result build() &;
  Result build() &&;
  Result build() const &;
};
```

All three methods have the same name and no explicit parameters. `Where.Parameters: []` alone cannot distinguish them.

The selector model therefore includes an `Object` constraint:

```yaml
Tags:
  - Name: Builder
    Methods:
      - Name: build
        Where:
          Parameters: []
        Availability: nonswift

      - Name: build
        Where:
          Parameters: []
          Object:
            Ref: lvalue
        SwiftName: buildFromLValue()

      - Name: build
        Where:
          Parameters: []
          Object:
            Ref: rvalue
        SwiftName: buildFromRValue()

      - Name: build
        Where:
          Parameters: []
          Object:
            Const: true
            Ref: lvalue
        SwiftName: buildFromConstLValue()
```

The first entry omits `Where.Object`, so it can still match all `build()` overloads with no explicit parameters. The following entries add `Where.Object` to distinguish `const`, `volatile`, and reference qualification on the implicit C++ object.

As with `Where.Parameters`, omitted properties remain unconstrained, while present properties narrow the candidate set.

Representing these qualifiers under `Object` follows the C++ language model more closely than treating the receiver as an ordinary parameter at a special position. These properties belong to the implicit object parameter, not to the method’s explicit parameter list.

Static methods and non-member functions do not have an implicit object parameter. Applying an `Object` constraint to such declarations is therefore invalid and can be diagnosed.

## Implementation and Results

Supporting overload-aware API Notes required changes throughout the existing pipeline:

```text
YAML
  -> API Notes model
  -> binary serialization
  -> declaration lookup
  -> overload-specific filtering
  -> Sema applies API Notes effects
```

The implementation was split into focused patches.

The first changes added [YAML parsing and data-model support](https://github.com/llvm/llvm-project/pull/203227) so that multiple entries with the same declaration name but different selectors could be represented.

The next part extended the [binary API Notes format](https://github.com/llvm/llvm-project/pull/204147). Overload-specific entries must remain distinct when API Notes are compiled and later loaded again. The serialization also preserves the difference between an omitted parameter constraint and an explicitly empty parameter list.

The [Sema integration](https://github.com/llvm/llvm-project/pull/205307) first finds API Notes by declaration context and name, then uses the declaration’s explicit parameter types to select the overload-specific entry and apply its effects.

Legacy name-only lookup remains unchanged. An explicit selector, however, does not silently fall back to an unrelated broad match when it fails. Such a fallback could apply an annotation intended for one overload to another declaration with the same name.

Additional work adds [diagnostics for malformed, duplicate, and unmatched selectors](https://github.com/llvm/llvm-project/pull/209408). The [normalization patch](https://github.com/llvm/llvm-project/pull/213043) refines selector lookup for aliases, nullability, qualifiers, and supported spelling differences, while the [object-qualifier patch](https://github.com/llvm/llvm-project/pull/216148) adds `Where.Object` matching. A small [diagnostics follow-up](https://github.com/llvm/llvm-project/pull/216272) makes generic `-Wapinotes` warnings visible for system headers, which matters because API Notes are commonly attached to SDK headers. Tests cover the parser, serialization, Sema lookup, normalization behavior, aliases, default arguments, static methods, zero-parameter declarations, and object-qualified member functions.

The result is an overload-aware selector model that supports both global functions and C++ methods while preserving existing API Notes behavior.

## Future Template Design

The future template design work explored how the overload selector model might eventually extend from concrete functions and methods to C++ templates. Template matching was not implemented in the current patches, but the design work helped identify which parts of the selector model should remain extensible.

Ordinary `Where.Parameters` matching works for concrete function parameter types. Templates add several harder questions. One selector might need to identify a function template or one of its specializations, such as `f<int>`. Another selector might need to match an ordinary function overload whose parameter type contains a class template specialization or dependent template parameter.

For example, dependent types can refer to template parameters directly, through nested dependent names, or as part of a larger type:

```cpp
template <typename T> void f(const T &);
template <typename Container> void f(const typename Container::value_type &);
template <typename Key, typename Value>
void f(const std::pair<Key, Value> &);
```

Relying only on source names such as `T`, `Container`, `Key`, or `Value` would be fragile because template parameter names can differ across redeclarations and library implementations. The design notes therefore explored structural template-parameter identity using depth and index, matching concrete template arguments for specializations such as `f<int>`, and a possible mapping from stable template-parameter identities to readable names so dependent types such as `const T &` can still be written clearly.

One possible direction was to assign stable identities to template parameters in API Notes, using depth and index, and then map those identities to readable local names used in dependent parameter spellings such as `const T &`. This would avoid relying on redeclaration-local source names while still keeping API Notes readable. The [template design document](https://github.com/StoeckOverflow/gsoc-2026-clang-api-notes-overloads/blob/main/ideas/format_v2_templates.md) contains illustrative future syntax for these ideas. That syntax is rejected by the current API Notes implementation.

## Reflections

This project gave me practical experience working across several parts of Clang, from YAML parsing and API Notes serialization to declaration lookup, type representation, diagnostics, and Sema integration. One of the main lessons was that preserving existing API Notes behavior was as important as adding the new selector support. Name-only notes still need to match as they did before, while overload-specific notes need clear rules for matching and diagnostics. It also taught me how valuable small, focused upstream patches are: splitting the parser, serialization, Sema, diagnostics, normalization, and object-qualifier work made each part easier to review, test, and revise.

The project also made me more interested in continuing to work on Clang, especially in areas where C++ language support and Swift interoperability meet.

## Acknowledgements

I would like to thank my mentors, Gábor Horváth, John Hui, and Egor Zhdan, for their thoughtful reviews, engaging design discussions, and support throughout Google Summer of Code.

Their feedback helped preserve compatibility with existing API Notes while extending the model to support overload matching, type normalization, and C++ object qualifiers.

## Links

* [GSoC final report](https://github.com/StoeckOverflow/gsoc-2026-clang-api-notes-overloads/blob/main/blog/report.md)
* [Project design document](https://github.com/StoeckOverflow/gsoc-2026-clang-api-notes-overloads/blob/main/ideas/format_v2.md)
* [Development diary](https://github.com/StoeckOverflow/gsoc-2026-clang-api-notes-overloads/blob/main/blog/diary.md)
* [Development repository](https://github.com/StoeckOverflow/llvm-project)
* Patch series:
  * [Parse `Where.Parameters` for C++ functions and methods](https://github.com/llvm/llvm-project/pull/203227)
  * [Serialize overload parameter selectors](https://github.com/llvm/llvm-project/pull/204147)
  * [Apply overload-aware API Notes in Sema](https://github.com/llvm/llvm-project/pull/205307)
  * [Add diagnostics and integration coverage](https://github.com/llvm/llvm-project/pull/209408)
  * [Normalize parameter types during selector lookup](https://github.com/llvm/llvm-project/pull/213043)
  * [Match C++ member-function object qualifiers](https://github.com/llvm/llvm-project/pull/216148)
  * [Show API Notes warnings in system headers](https://github.com/llvm/llvm-project/pull/216272)
* [Template-matching design notes](https://github.com/StoeckOverflow/gsoc-2026-clang-api-notes-overloads/blob/main/ideas/format_v2_templates.md)
* [Template specialization selector design branch](https://github.com/StoeckOverflow/llvm-project/tree/apinotes-where-parameters-template-design)
