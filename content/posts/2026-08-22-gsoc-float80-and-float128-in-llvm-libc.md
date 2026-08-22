---
author: "Sukumar Sawant"
date: "2026-08-22"
tags: ["GSoC", "libc", "math", "floating-point"]
title: "GSoC 2026: Float80 and Float128 Emulation in LLVM libc"
---


## Introduction and "WHY"?

`float80` and `float128` are the two extended-precision floating-point formats LLVM libc relies on. `float80` is the 80-bit x87 extended precision format, with 1 sign bit, 15 exponent bits, and a 64-bit significand carrying an explicit integer bit. `float128` is the IEEE 754 `binary128` ("quad precision") format, with 1 sign bit, 15 exponent bits, and 112 stored mantissa bits. Both carry considerably more precision than `double`, which is what makes them useful as the intermediate type in correctly-rounded math routines.

Both formats are already natively supported on several platforms, but not on all of them. For example:

* MSVC on Windows treats `long double` as a 64-bit `double` and has no native `__float128` type.
* AArch64 targets typically use a 128-bit `long double`, but lack the 80-bit extended precision format entirely.
* Clang on certain non-x86 targets may not enable `__float128` by default.

This matters because LLVM libc needs to build and test its high-precision math routines consistently across every target it supports, not just the ones where these types happen to exist natively. Where the compiler provides no native type, the templated implementations cannot be compiled at all.

The goal of this project was to lift that restriction by emulating both formats in software, so that the same templated implementations build and run everywhere, producing identical results whether the underlying type is native or emulated.

The exact goals of this project were to:

1. Add software emulation classes for the `float80` and `float128` formats, backed by LLVM libc's existing `DyadicFloat` arithmetic.
2. Provide the operator overloads and conversions needed for these classes to be used in place of the native types.
3. Integrate the emulated types into the existing floating-point support infrastructure, so that generic code recognizes and handles them like any other floating-point type.
4. Migrate the existing `float128` math functions to work correctly with the emulated type.

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


| Function / Family                                                                                     | PR                                                             |
|--------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------|
| `ceilf128`                                                                                              | [#207735](https://github.com/llvm/llvm-project/pull/207735)     |
| `floorf128`                                                                                             | [#216552](https://github.com/llvm/llvm-project/pull/216552)     |
| `truncf128`                                                                                             | [#216560](https://github.com/llvm/llvm-project/pull/216560)     |
| `roundf128`                                                                                             | [#216563](https://github.com/llvm/llvm-project/pull/216563)     |
| `roundevenf128`                                                                                          | [#216568](https://github.com/llvm/llvm-project/pull/216568)     |
| `nearbyintf128`                                                                                          | [#217025](https://github.com/llvm/llvm-project/pull/217025)     |
| `lrintf128`, `llrintf128`, `rintf128`                                                                   | [#217070](https://github.com/llvm/llvm-project/pull/217070)     |
| `lroundf128`, `llroundf128`                                                                             | [#216829](https://github.com/llvm/llvm-project/pull/216829)     |
| `fromfpf128`, `fromfpxf128`, `ufromfpf128`, `ufromfpxf128`                                              | [#216578](https://github.com/llvm/llvm-project/pull/216578)     |
| `fabsf128`, `copysignf128`                                                                              | [#217389](https://github.com/llvm/llvm-project/pull/217389)     |
| `fmaxf128`, `fminf128`                                                                                   | [#216575](https://github.com/llvm/llvm-project/pull/216575)     |
| `fmaximumf128`, `fminimumf128`                                                                           | [#217390](https://github.com/llvm/llvm-project/pull/217390)     |
| `fmaximum_numf128`, `fminimum_numf128`                                                                   | [#217409](https://github.com/llvm/llvm-project/pull/217409)     |
| `fmaximum_magf128`, `fminimum_magf128`                                                                   | [#217539](https://github.com/llvm/llvm-project/pull/217539)     |
| `fmaximum_mag_numf128`, `fminimum_mag_numf128`                                                           | [#217549](https://github.com/llvm/llvm-project/pull/217549)     |
| `fdimf128`                                                                                               | [#217567](https://github.com/llvm/llvm-project/pull/217567)     |
| `iscanonicalf128`, `isnanf128`, `issignalingf128`                                                       | [#217134](https://github.com/llvm/llvm-project/pull/217134)     |
| `nextafterf128`, `nextupf128`, `nextdownf128`                                                           | [#217575](https://github.com/llvm/llvm-project/pull/217575)     |
| `fmodf128`, `remainderf128`                                                                              | [#217578](https://github.com/llvm/llvm-project/pull/217578)     |
| `sqrtf128`                                                                                               | [#216500](https://github.com/llvm/llvm-project/pull/216500)     |
| `atan2f128`                                                                                              | [#216508](https://github.com/llvm/llvm-project/pull/216508)     |
| `bf16addf128`, `bf16subf128`, `bf16mulf128`, `bf16divf128`, `bf16fmaf128`                                |                                                                   |
| `f16addf128`, `f16subf128`, `f16mulf128`, `f16divf128`                                                   |                                                                   |
| `f16fmaf128`, `f16sqrtf128`                                                                               |                                                                   |
| `daddf128`, `dsubf128`, `dmulf128`, `ddivf128`                                                            |                                                                   |
| `dfmaf128`, `dsqrtf128`                                                                                   |                                                                   |
| `faddf128`, `fsubf128`, `fmulf128`, `fdivf128`                                                            |                                                                   |
| `ffmaf128`, `fsqrtf128`                                                                                   |                                                                   |
| `totalorderf128`, `totalordermagf128`                                                                     |                                                                   |
| `canonicalizef128`                                                                                         |                                                                   |
| `frexpf128`, `logbf128`, `ilogbf128`, `llogbf128`                                                          |                                                                   |
| `ldexpf128`, `scalbnf128`, `scalblnf128`                                                                   |                                                                   |
| `modff128`                                                                                                 |                                                                   |
| `nanf128`, `getpayloadf128`, `setpayloadf128`, `setpayloadsigf128`                                         |                                                                   |
| `remquof128`                                                                                               |                                                                   |

## Future Work

- Add explicit `float80` entrypoints.
- Compare performance of the emulated `float80`/`float128` types against native implementations, where native support is available.
## Acknowledgements

I’m grateful to Google Summer of Code and the LLVM organization for giving me this opportunity. I would like to sincerely thank my mentors, Tue Ly, Nicolas Celik, and Krishna Pandey, for their guidance, support, and valuable feedback throughout this project and making this a great learning experience.

I look forward to continuing my contributions to LLVM and the open-source community.
