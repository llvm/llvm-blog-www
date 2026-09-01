---
author: "Sukumar Sawant"
date: "2026-08-22"
tags: ["GSoC", "libc", "math", "floating-point"]
title: "GSoC 2026: Enable float80/float128 on Unsupported Targets for LLVM libc"
---

## Introduction

Hello LLVM Community! I am Sukumar Sawant, Computer Science student at Mumbai University (VIT Mumbai), India. I had the pleasure of working on the project on introducing `float80` and `float128` (emulated) in LLVM libc as part of Google Summer of Code 2026 along with my mentors Tue Ly, Nicolas Celik and Krishna Pandey.

## "Why"?

`float80` and `float128` are the two extended-precision floating-point formats LLVM libc supports. `float80` is the 80-bit x87 extended precision format, with 1 sign bit, 15 exponent bits, and a 64-bit significand carrying an explicit integer bit. `float128` is the IEEE 754 `binary128` ("quad precision") format, with 1 sign bit, 15 exponent bits, and 112 stored mantissa bits.

Both formats are already natively supported on several platforms, but not on all of them. For example:

* MSVC on Windows treats `long double` as a 64-bit `double` and has no native `__float128` type.
* AArch64 targets typically use a 128-bit `long double`, but lack the 80-bit extended precision format entirely.
* Clang on certain non-x86 targets may not enable `__float128` by default.

This matters because LLVM libc needs to build and test its high-precision math routines consistently across every target it supports, not just the ones where these types happen to exist natively. Where the compiler provides no native type, the templated implementations cannot be compiled at all.

The goal of this project was to lift that restriction by emulating both formats in software, so that the same templated implementations build and run everywhere, producing identical results whether the underlying type is native or emulated.

The exact goals of this project were to:

1. Add software emulation classes for the `float80` and `float128` formats.
2. Provide the operator overloads and conversions needed for these classes to be used in place of the native types.
3. Integrate the emulated types into the existing floating-point support infrastructure.
4. Modify the existing `float128` math functions to work correctly with the emulated type.

## Interesting Challenges

### 1. A `bit_cast` compile error that only showed on Windows

`Float128` stores its value in a `UInt128`, which on Windows, without a native 128-bit integer type, is `BigInt`. LLVM-libc's `cpp::bit_cast` requires the target type to be trivially constructible and trivially copyable, which was causing the compiler error in this case.

```cpp
// This implementation of bit_cast requires trivially-constructible To, to avoid
// UB in the implementation.
template <typename To, typename From>
LIBC_INLINE static constexpr cpp::enable_if_t<
    (sizeof(To) == sizeof(From)) &&
        cpp::is_trivially_constructible<To>::value &&
        cpp::is_trivially_copyable<To>::value &&
        cpp::is_trivially_copyable<From>::value,
    To>
bit_cast(const From &from) {
  // ...
}
```
```sh
D:\a\llvm-project\llvm-project\libc\src/__support/FPUtil\FPBits.h(863,12): error: no matching function for call to 'bit_cast'
  863 |     return cpp::bit_cast<T>(UP::bits);
      |            ^~~~~~~~~~~~~~~~
...
D:\a\llvm-project\llvm-project\libc\test\src\math\smoke\float128_test.cpp(21,12): note: in instantiation of function template specialization '__llvm_libc_23_0_0_git::fputil::Float128::Float128<double>' requested here
   21 |   Float128 a(1.0), b(1.0), c(2.0);
      |            ^
```

The culprit was `BigInt` being zero-initialized in `libc/src/__support/big_int.h`.

```cpp
template <size_t Bits, bool Signed, typename WordType = uint64_t>
struct BigInt {
  // ...
  cpp::array<WordType, WORD_COUNT> val{}; // zero initialized.
  // ...
};
```

The default member initializer was enough to make the condition fail.

The fix was to drop the zero initialization from the declaration and push it into the constructors that actually need it
([#206277](https://github.com/llvm/llvm-project/pull/206277)).

With the member initializer gone, the defaulted constructor is genuinely trivial, both traits become true, and `bit_cast` accepts the type, which fixed the Windows build.

### 2. The ABI issue with emulated types

A native `float128` uses different registers than the emulated types like `Float128`, which under the hood uses a `UInt128` container.
While testing LLVM-libc with the CORE-MATH project's test suite, the `bfloat16` (also emulated) functions consistently failed. This was because CORE-MATH uses the native `__bf16` type provided by the compiler, which uses different registers than the emulated [BFloat16](https://github.com/llvm/llvm-project/pull/144463) type previously implemented in LLVM libc.
So we tested our `float128` functions with CORE-MATH and modified them to use the emulation internally for calculations, and the native `float128` when available for parameters/returns, so that in the end the registers used for parameters/returns remain the same and the ABI mismatch is prevented.

The work below on modifying the `float128` functions already uses this idea, making the `float128` functions ABI compatible.

## Work done

- `Float128` emulation type was added to LLVM libc [#200565](https://github.com/llvm/llvm-project/pull/200565).
- `DyadicFloat<128>` was renamed to `DFloat128` [#200907](https://github.com/llvm/llvm-project/pull/200907).
- `BigInt` was made trivially constructible, needed for the emulated types [#206277](https://github.com/llvm/llvm-project/pull/206277).
- `Float80` emulation type was added to LLVM libc [#214447](https://github.com/llvm/llvm-project/pull/214447), along with its operator overloads [#214493](https://github.com/llvm/llvm-project/pull/214493).
- A float128-to-integer conversion UB bug was fixed [#211593](https://github.com/llvm/llvm-project/pull/211593).
- Conversion tests between emulated and native `f128` were added [#213422](https://github.com/llvm/llvm-project/pull/213422).
- `add_sub` was fixed for emulated types [#217344](https://github.com/llvm/llvm-project/pull/217344).
- A `NEED_MPFR_F128` argument was added to `add_fp_unittest` [#215657](https://github.com/llvm/llvm-project/pull/215657).
- `LIBC_TYPES_HAS_FLOAT128` was replaced with `LIBC_TYPES_HAS_NATIVE_FLOAT128` for clarity (NFC) [#215605](https://github.com/llvm/llvm-project/pull/215605).
- Existing `float128` math functions were migrated to use the emulated type, one function (or function family) at a time, tracked under [#216576](https://github.com/llvm/llvm-project/issues/216576) (see table below).


| Function / Family                                          | PR                                                          |
|------------------------------------------------------------|-------------------------------------------------------------|
| `ceilf128`                                                 | [#207735](https://github.com/llvm/llvm-project/pull/207735) |
| `floorf128`                                                | [#216552](https://github.com/llvm/llvm-project/pull/216552) |
| `truncf128`                                                | [#216560](https://github.com/llvm/llvm-project/pull/216560) |
| `roundf128`                                                | [#216563](https://github.com/llvm/llvm-project/pull/216563) |
| `roundevenf128`                                            | [#216568](https://github.com/llvm/llvm-project/pull/216568) |
| `nearbyintf128`                                            | [#217025](https://github.com/llvm/llvm-project/pull/217025) |
| `lrintf128`, `llrintf128`, `rintf128`                      | [#217070](https://github.com/llvm/llvm-project/pull/217070) |
| `lroundf128`, `llroundf128`                                | [#216829](https://github.com/llvm/llvm-project/pull/216829) |
| `fromfpf128`, `fromfpxf128`, `ufromfpf128`, `ufromfpxf128` | [#216578](https://github.com/llvm/llvm-project/pull/216578) |
| `fabsf128`, `copysignf128`                                 | [#217389](https://github.com/llvm/llvm-project/pull/217389) |
| `fmaxf128`, `fminf128`                                     | [#216575](https://github.com/llvm/llvm-project/pull/216575) |
| `fmaximumf128`, `fminimumf128`                             | [#217390](https://github.com/llvm/llvm-project/pull/217390) |
| `fmaximum_numf128`, `fminimum_numf128`                     | [#217409](https://github.com/llvm/llvm-project/pull/217409) |
| `fmaximum_magf128`, `fminimum_magf128`                     | [#217539](https://github.com/llvm/llvm-project/pull/217539) |
| `fmaximum_mag_numf128`, `fminimum_mag_numf128`             | [#217549](https://github.com/llvm/llvm-project/pull/217549) |
| `fdimf128`                                                 | [#217567](https://github.com/llvm/llvm-project/pull/217567) |
| `iscanonicalf128`, `isnanf128`, `issignalingf128`          | [#217134](https://github.com/llvm/llvm-project/pull/217134) |
| `nextafterf128`, `nextupf128`, `nextdownf128`              | [#217575](https://github.com/llvm/llvm-project/pull/217575) |
| `fmodf128`, `remainderf128`                                | [#217578](https://github.com/llvm/llvm-project/pull/217578) |
| `sqrtf128`                                                 | [#216500](https://github.com/llvm/llvm-project/pull/216500) |
| `atan2f128`                                                | [#216508](https://github.com/llvm/llvm-project/pull/216508) |

## Conclusion

- We would now be able to use, test and build the `Float128` and `Float80` types even on platforms where they are not natively available.
- The `float128` functions are now enabled on targets like Windows with MSVC, macOS, and GPU targets.

## What did I learn?

One of the most important things I learned was more about floating-point formats, which I'd never have had a chance to go through or play with otherwise. My mentors did let me try new things and learn from them, which helped me discover missing areas or fix any patches I encountered during the project. The fun part is that I also made my first stacked PR during this period. I was also able to get a firmer understanding of emulated types, better C++ programming practices, and the LLVM libc codebase. I came out of this summer knowing a lot more about floating-point formats and C++ than I did before.

## Future Work

- Complete modification of the remaining `float128` functions.
- Add explicit `float80` entrypoints.
- Compare performance of the emulated `float80`/`float128` types against native implementations, where native support is available.

## References

- The previous work on `BFloat16` was quite helpful in designing the current `Float128` and `Float80`: [`bfloat16.h`](https://github.com/llvm/llvm-project/blob/c689c165ba2723285258975a14cd562d41d059ab/libc/src/__support/FPUtil/bfloat16.h#L27-L28)
- [CORE-MATH](https://gitlab.inria.fr/core-math/core-math/-/blob/4acb80ff2b6fc614023a1ada2110bf7625d3cf8c/src/binaryb16/support/check_exhaustive.c#L42-L43), used to confirm the ABI issue
- [C23 draft standard (N3220)](https://www.open-std.org/JTC1/SC22/wg14/www/docs/n3220.pdf), used for conversions and handling undefined behavior
- *Handbook of Floating-Point Arithmetic*, by Jean-Michel Muller et al.

## Acknowledgements

I would like to sincerely thank my mentors, Tue Ly, Nicolas Celik, and Krishna Pandey, for their guidance, support, and valuable feedback throughout this project and making this a great learning experience. I’m grateful to Google Summer of Code admins and the LLVM organization for giving me this opportunity.

I look forward to continuing my contributions to LLVM and the open-source community.
