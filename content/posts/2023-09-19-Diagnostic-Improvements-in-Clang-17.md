---
author: "Takuya Shimizu"
date: "2023-09-19"
tags: ["clang", "frontend", "gsoc"]
title: "Diagnostic Improvements in Clang 17"
---

## Introduction
In the last few months, I have been a part of an ongoing effort to improve Clang's diagnostic capabilities.
The newly released Clang 17 brings several of these improvements to the forefront.
This blog post aims to provide a comprehensive overview of these diagnostic enhancements.
We will employ simplified code examples and compare diagnostic outputs from Clang 16 and Clang 17 to illustrate how the latest updates can enhance the development experience for Clang users.

### Multi-line printing of code snippets
One of the most anticipated diagnostic features of Clang 17 is its support for multi-line printing of code snippets.
This marks a departure from the old single-line limit, which used to make it difficult to fully understand the context around a code issue.
This new feature improves the readability and comprehensibility of diagnostic messages by displaying a more complete view of the code in question.
Moreover, line numbers are now attached to the left of each line, allowing for quicker navigation and issue resolution.

```c++
int func(
  int a, int b, int& r);

void test(int *ptr) {
  func(3, 4, 5);
  func(3, 4);
}
```

Before:
```console
<source>:5:3: error: no matching function for call to 'func'
  func(3, 4, 5);
  ^~~~
<source>:1:5: note: candidate function not viable: expects an lvalue for 3rd argument
int func(
    ^
<source>:6:3: error: no matching function for call to 'func'
  func(3, 4);
  ^~~~
<source>:1:5: note: candidate function not viable: requires 3 arguments, but 2 were provided
int func(
    ^
```

After:
```console
<source>:5:3: error: no matching function for call to 'func'
    5 |   func(3, 4, 5);
      |   ^~~~
<source>:1:5: note: candidate function not viable: expects an lvalue for 3rd argument
    1 | int func(
      |     ^
    2 |   int a, int b, int& r);
      |                 ~~~~~~
<source>:6:3: error: no matching function for call to 'func'
    6 |   func(3, 4);
      |   ^~~~
<source>:1:5: note: candidate function not viable: requires 3 arguments, but 2 were provided
    1 | int func(
      |     ^
    2 |   int a, int b, int& r);
      |   ~~~~~~~~~~~~~~~~~~~~
```

In this example, the newly covered source ranges make it easier to understand why the overload candidate is invalid.

Commit: https://reviews.llvm.org/D147875 (Timm Bäder)

### Preprocessor-related diagnostics
- Clang warns on macro redefinitions. When the redefinition happens in assembly files, and the previous definition of the macro comes from the command line, the last definition is now diagnosed as coming from `<command line>` instead of `<built-in>`.

Assembly file:
```asm
#define MACRO 3
```

Clang invocation command:
```console
clang -DMACRO=1 file.S
```
Before:
```console
warning: 'MACRO' macro redefined [-Wmacro-redefined]
#define MACRO 3
        ^
<built-in>:362:9: note: previous definition is here
#define MACRO 1
        ^
```
After:
```console
warning: 'MACRO' macro redefined [-Wmacro-redefined]
    1 | #define MACRO 3
      |         ^
<command line>:1:9: note: previous definition is here
    1 | #define MACRO 1
      |         ^
```
Commit: https://reviews.llvm.org/D145397 (John Brawn)

<br>

- Clang 17 emits a warning on any compiler-builtin macro being undefined or redefined, some of which were just ignored in Clang 16.

```c++
#undef __cplusplus
```

Before: _No Warning_

After:
```console
<source>:1:8: warning: undefining builtin macro [-Wbuiltin-macro-redefined]
    1 | #undef __cplusplus
      |        ^
```

Redefinition of compiler builtin macros usually leads to unintended results because library headers often rely on these macros, and they do not
expect these macros to be modified by users.

Commit: https://reviews.llvm.org/D144654 (John Brawn)

<br>

- Clang 17 diagnoses unexpected tokens after a `#pragma clang|GCC diagnostic push|pop` directive.
```c++
#pragma clang diagnostic push ignore
```
Before: _No Warning_

After:
```console
<source>:1:31: warning: unexpected token in pragma diagnostic [-Wunknown-pragmas]
    1 | #pragma clang diagnostic push ignored
      |                               ^
```
Commit: https://github.com/llvm/llvm-project/commit/7ff507f1448bfdfcaa91d177d1f655dcb17557e7 (Aaron Ballman)

### Attribute related diagnostics
- Clang 17 generates notes and fix-its for `ifunc`/`alias` attributes which point to unmangled function names.
```c++
__attribute__((used)) static void *resolve_foo() { return 0; }

__attribute__((ifunc("resolve_foo"))) void foo();
```
Before:
```console
<source>:3:16: error: ifunc must point to a defined function
__attribute__((ifunc("resolve_foo"))) void foo();
               ^
```
After:
```console
<source>:3:16: error: ifunc must point to a defined function
    3 | __attribute__((ifunc("resolve_foo"))) void foo();
      |                ^
<source>:3:16: note: the function specified in an ifunc must refer to its mangled name
<source>:3:16: note: function by that name is mangled as "_ZL11resolve_foov"
    3 | __attribute__((ifunc("resolve_foo"))) void foo();
      |                ^~~~~~~~~~~~~~~~~~~~
      |                ifunc("_ZL11resolve_foov")
```

One needs to be aware of the C++ name mangling when using `ifunc` or `alias` attributes, but knowing the mangled name from a function signature isn't
an easy task for many people.
This change makes the error message highly understandable by suggesting that the `ifunc` needs to refer to the mangled name,
and it also makes this error more actionable by representing the mangled name.

Commit: https://reviews.llvm.org/D143803 (Dhruv Chawla)

<br>


- Clang 17 avoids duplicate warnings on unreachable ``[[fallthrough]];`` statements previously issued from `-Wunreachable-code` and `-Wunreachable-code-fallthrough` by prioritizing `-Wunreachable-code-fallthrough`.
```c++
void f(int n) {
  switch (n) {
    [[fallthrough]];
  case 1:;
  }
}
```

Clang invocation command:
```console
clang++ -Wunreachable file.cpp
```
Before:
```console
<source>:3:5: warning: code will never be executed [-Wunreachable-code]
    [[fallthrough]];
    ^~~~~~~~~~~~~~~~
<source>:3:5: warning: fallthrough annotation in unreachable code [-Wunreachable-code-fallthrough]
```
After:
```console
<source>:3:5: warning: fallthrough annotation in unreachable code [-Wunreachable-code-fallthrough]
    3 |     [[fallthrough]];
      |     ^
```

Commit: https://reviews.llvm.org/D145842 (Takuya Shimizu)

<br>

- Clang 17 correctly emits diagnostics for `unavailable` attributes that were ignored in Clang 16.
```c++
template <class _ValueType = int>
class __attribute__((unavailable)) polymorphic_allocator {};

void f() { polymorphic_allocator<void> a; }
```
Before:
No diagnostics

After:
```console
<source>:4:12: error: 'polymorphic_allocator<void>' is unavailable
    4 | void f() { polymorphic_allocator<void> a; }
      |            ^
<source>:2:36: note: 'polymorphic_allocator<void>' has been explicitly marked unavailable here
    2 | class __attribute__((unavailable)) polymorphic_allocator {};
      |                                    ^
```

Commit: https://reviews.llvm.org/D147495 (Shafik Yaghmour)

<br>

- Clang no longer emits `-Wunused-variable` warnings for variables declared with `__attribute__((cleanup(...)))` to match GCC's behavior.

```c++
void c(int *);
void f(void) { int __attribute__((cleanup(c))) X1 = 4; }
```

Before:
```console
<source>:2:48: warning: unused variable 'X1' [-Wunused-variable]
void f(void) { int __attribute__((cleanup(c))) X1 = 4; }
                                               ^
```

After: _No Warning_

`cleanup` attribute is used to write RAII in C.
Objects declared with this attribute are actually *used* as arguments to the function specified in `cleanup` attribute after its declaration,
and thus, it's considered better not to diagnose them as unused.

Commit: https://reviews.llvm.org/D152180 (Nathan Chancellor)

### `alignas` specifier

- Clang 16 modeled `alignas(type-id)` as `alignas(alignof(type-id))`.
Clang 17 fixes this modeling and thus fixes the wrong mention of `alignof` in diagnostics about `alignas` and `_Alignas`.

```c++
struct alignas(void) A {};
```

Before:
```console
<source>:1:16: error: invalid application of 'alignof' to an incomplete type 'void'
struct alignas(void) A {};
              ~^~~~~
```

After:
```console
<source>:1:16: error: invalid application of 'alignas' to an incomplete type 'void'
    1 | struct alignas(void) A {};
      |               ~^~~~~
```

Commit: https://reviews.llvm.org/D150528 (yronglin)

### Shadowings
- Clang 17 emits an error when lambda's captured variable shadows a template parameter.
```c++
auto h = [y = 0]<typename y>(y) { return 0; }
```

Before: _No Error_

After:
```console
<source>:1:11: error: declaration of 'y' shadows template parameter
    1 | auto h = [y = 0]<typename y>(y) { return 0; };
      |           ^
<source>:1:27: note: template parameter is declared here
    1 | auto h = [y = 0]<typename y>(y) { return 0; };
      |                           ^
```

Commit: https://reviews.llvm.org/D148712 (Mariya Podchishchaeva)

<br>

- Clang 17's `-Wshadow` diagnoses shadowings by static local variables.

```c++
int var;
void f() { static int var = 42; }
```

Before: _No Warning_

After:
```console
<source>:2:23: warning: declaration shadows a variable in the global namespace [-Wshadow]
    2 | void f() { static int var = 42; }
      |                       ^
<source>:1:5: note: previous declaration is here
    1 | int var;
      |     ^
```

Commit: https://reviews.llvm.org/D151214 (Takuya Shimizu)

### `-Wformat`

- Clang 17 diagnoses invalid use of scoped enumeration types in format strings, which is an Undefined Behavior.
Now it also emits a fix-it hint to suggest the use of `static_cast` to its underlying type to avoid the UB.

```c++
#include <limits.h>
#include <stdio.h>

enum class Foo : long {
  Bar = LONG_MAX,
};

int main() { printf("%ld", Foo::Bar); }
```
Before: _No Warning_

After:
```console
<source>:8:28: warning: format specifies type 'long' but the argument has type 'Foo' [-Wformat]
    8 | int main() { printf("%ld", Foo::Bar); }
      |                      ~~~   ^~~~~~~~
      |                            static_cast<long>( )
```

Commit: https://github.com/llvm/llvm-project-release-prs/commit/3632e2f5179a420ea8ab84e6ca33747ff6130fa2 (Aaron Ballman)

Commit: https://reviews.llvm.org/D153622 (Alex Brachet)

<br>

- Clang 17's `-Wformat` recognizes `%lb` and `%lB` as format specifiers.

```c
#include <cstdio>
int main() { printf("%lb %lB", 10L, 10L); }
```

Before:
```console
<source>:2:23: warning: length modifier 'l' results in undefined behavior or no effect with 'b' conversion specifier [-Wformat]
int main() { printf("%lb %lB", 10L, 10L); }
                     ~^~
<source>:2:27: warning: length modifier 'l' results in undefined behavior or no effect with 'B' conversion specifier [-Wformat]
int main() { printf("%lb %lB", 10L, 10L); }
                         ~^~
```

After: _No Warning_

`%b` and `%B` are new formats for printing binary representations of integers specified in the ISO C23 draft.
There are already several libc implementations available that support this format. (glibc >= 2.35, for example)

Clang 16 already recognizes `%b` and `%llb` as valid format specifiers but handles `%lb` as invalid.
Clang 17 recognizes `%lb` and `%lB` to avoid false positive warnings and to emit correct fix-it hints.

Commit: https://reviews.llvm.org/D148779 (Fangrui Song)

### Constexpr-related diagnostics
- Clang often prints the subexpression values of binary operators such as `==`, `||`, and `&&` in static assertion failures to help users
understand the cause of the failure.
Clang 17 stops printing subexpression values if the binary operator is `||` because it is evident that both subexpressions evaluate to `false` in that case.

- The error message for the failure of static assertion now points to the asserted expression instead of the `static_assert` token.

```c++
constexpr bool a = false;
constexpr bool b = false;
static_assert(a || b);
```
Before:
```console
<source>:3:1: error: static assertion failed due to requirement 'a || b'
static_assert(a || b);
^             ~~~~~~
<source>:3:17: note: expression evaluates to 'false || false'
static_assert(a || b);
              ~~^~~~
```

After:
```console
<source>:3:15: error: static assertion failed due to requirement 'a || b'
    3 | static_assert(a || b);
      |               ^~~~~~
```

Commit: https://reviews.llvm.org/D147745 (Jorge Pinto Sousa)

Commit: https://reviews.llvm.org/D146376 (Krishna Narayanan)

<br>

- Clang 17 diagnoses calls to a null function pointer in constexpr evaluation as such instead of just saying it is invalid.
```c++
constexpr int call(int (*F)()) {
    return F();
}
static_assert(call(nullptr));
```
Before:
```console
<source>:4:15: error: static assertion expression is not an integral constant expression
static_assert(call(nullptr));
              ^~~~~~~~~~~~~
<source>:2:12: note: subexpression not valid in a constant expression
    return F();
           ^
<source>:4:15: note: in call to 'call(nullptr)'
static_assert(call(nullptr));
              ^
```

After:
```console
<source>:4:15: error: static assertion expression is not an integral constant expression
    4 | static_assert(call(nullptr));
      |               ^~~~~~~~~~~~~
<source>:2:12: note: 'F' evaluates to a null function pointer
    2 |     return F();
      |            ^
<source>:4:15: note: in call to 'call(nullptr)'
    4 | static_assert(call(nullptr));
      |               ^~~~~~~~~~~~~
```

Commit: https://reviews.llvm.org/D145793 (Takuya Shimizu)

<br>

- Member function calls are displayed more true to the user-written code.

```c++
struct Foo {
  constexpr int div(int i) const { return 1 / i; }
};

constexpr Foo obj;
constexpr const Foo &ref = obj;
static_assert(ref.div(0));
```

Before:
```console
<source>:7:15: error: static assertion expression is not an integral constant expression
static_assert(ref.div(0));
              ^~~~~~~~~~
<source>:2:45: note: division by zero
  constexpr int div(int i) const { return 1 / i; }
                                            ^
<source>:7:19: note: in call to '&obj->div(0)'
static_assert(ref.div(0));
                  ^
```

After:
```console
<source>:7:15: error: static assertion expression is not an integral constant expression
    7 | static_assert(ref.div(0));
      |               ^~~~~~~~~~
<source>:2:45: note: division by zero
    2 |   constexpr int div(int i) const { return 1 / i; }
      |                                             ^ ~
<source>:7:15: note: in call to 'ref.div(0)'
    7 | static_assert(ref.div(0));
      |               ^~~~~~~~~~
```

Commit: https://reviews.llvm.org/D151720 (Takuya Shimizu)

<br>

- When a constexpr variable's constructor call leaves its subobject uninitialized, Clang 17 prints the uninitialized subobject's name instead of its type.

```c++
struct Foo {
  constexpr Foo() {}
  int val;
};
constexpr Foo ff;
```
Before:
```console
<source>:5:15: error: constexpr variable 'ff' must be initialized by a constant expression
constexpr Foo ff;
              ^~
<source>:5:15: note: subobject of type 'int' is not initialized
<source>:3:7: note: subobject declared here
  int val;
      ^
```

After:
```console
<source>:5:15: error: constexpr variable 'ff' must be initialized by a constant expression
    5 | constexpr Foo ff;
      |               ^~
<source>:5:15: note: subobject 'val' is not initialized
<source>:3:7: note: subobject declared here
    3 |   int val;
      |       ^
```

Commit: https://reviews.llvm.org/D146358 (Takuya Shimizu)

<br>

- Clang 17 diagnoses unused const variable template as "unused variable template" instead of "unused variable".

```c++
namespace {
template <typename T> constexpr double var_t = 0;
}
```

Before:
```console
<source>:2:40: warning: unused variable 'var_t' [-Wunused-const-variable]
template <typename T> constexpr double var_t = 0;
                                       ^
```
After:
```console
<source>:2:40: warning: unused variable template 'var_t' [-Wunused-template]
    2 | template <typename T> constexpr double var_t = 0;
      |                                        ^~~~~
```
Uninstantiated templates do not generate symbols, and thus, the meaning of _unused_ is broader than the usual
unused variables or functions.

For this reason, `-Wunused` omits `-Wunused-template`.
This change follows the rationale and leads to fewer unwanted `Wunused-const-variable` warnings.

Commit: https://reviews.llvm.org/D152796 (Takuya Shimizu)

## Acknowledgements
Special thanks are in order for Timm Bäder, my Google Summer of Code mentor, for his invaluable guidance and support throughout the project.

Further gratitude is extended to my regular reviewers: Aaron Ballman, Christopher Di Bella, and Shafik Yaghmour, for their insightful and constructive feedback
that greatly improved my codes.
