---
author: "Krishna Pandey"
date: "2025-09-29"
tags: ["GSoC", "libc", "math", "c++23"]
title: "GSoC 2025: Bfloat16 in LLVM libc"
---


## Introduction
BFloat16 is a 16-bit floating-point format, introduced by Google and standardized in C++23 as `std::bfloat16_t`. It uses 1 sign bit, 8 exponent bits (the same as `float`), and 7 mantissa bits. This configuration allows BFloat16 to represent a much wider dynamic range than IEEE `binary16` (~3×10^38 values compared to 65,504), though with lower precision. BFloat16 has become popular in AI and machine learning use-cases where it offers significant performance advantages over IEEE `binary32` while preserving its approximate dynamic range.

The goal of this project was to implement the BFloat16 type in LLVM libc along with the basic math functions like `fabsbf16`, `fmaxbf16`, etc.  
We also want all functions to be generic (platform-independent) and correctly rounded for all rounding modes.

## What was done

- BFloat16 type was added to LLVM libc (`libc/src/__support/FPUtil/bfloat16.h`) [#144463](https://github.com/llvm/llvm-project/pull/144463).
- All 70 expected basic math functions for `bfloat16` were implemented, using a generic approach that supports all libc supported architectures (ARM, RISC-V, GPUs, x86, Darwin) (see table below).
- Implemented two additional basic math functions: `iscanonicalbf16` and `issignalingbf16`.
- Implemented higher math functions: `sqrtbf16` [#156654](https://github.com/llvm/llvm-project/pull/156654) and `log_bf16` [#157811](https://github.com/llvm/llvm-project/pull/157811) (open).
- Comparison operations for the `FPBits` class were added [#144983](https://github.com/llvm/llvm-project/pull/144983).

| Basic Math Function                                                                                                                                            | PR                                                          |
|----------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------|
| `fabsbf16`                                                                                                                                                     | [#148398](https://github.com/llvm/llvm-project/pull/148398) |
| `ceilbf16`, `floorbf16`, `roundbf16`, `roundevenbf16`, `truncbf16`                                                                                             | [#152352](https://github.com/llvm/llvm-project/pull/152352) |
| `bf16add`, `bf16addf`, `bf16addl`, `bf16addf128`, `bf16sub`, `bf16subf`, `bf16subl`, `bf16subf128`                                                             | [#152774](https://github.com/llvm/llvm-project/pull/152774) |
| `fmaxbf16`, `fminbf16`                                                                                                                                         | [#152782](https://github.com/llvm/llvm-project/pull/152782) |
| `bf16mul`, `bf16mulf`, `bf16mull`, `bf16mulf128`                                                                                                               | [#152847](https://github.com/llvm/llvm-project/pull/152847) |
| `fmaximumbf16`, `fmaximum_magbf16`, `fmaximum_mag_numbf16`, `fmaximum_numbf16`, `fminimumbf16`, `fminimum_magbf16`, `fminimum_mag_numbf16`, `fminimum_numbf16` | [#152881](https://github.com/llvm/llvm-project/pull/152881) |
| `bf16div`, `bf16divf`, `bf16divl`, `bf16divf128`                                                                                                               | [#153191](https://github.com/llvm/llvm-project/pull/153191) |
| `bf16fma`, `bf16fmaf`, `bf16fmal`, `bf16fmaf128`                                                                                                               | [#153231](https://github.com/llvm/llvm-project/pull/153231) |
| `llrintbf16`, `llroundbf16`, `lrintbf16`, `lroundbf16`, `nearbyintbf16`, `rintbf16`                                                                            | [#153882](https://github.com/llvm/llvm-project/pull/153882) |
| `fromfpbf16`, `fromfpxbf16`, `ufromfpbf16`, `ufromfpxbf16`                                                                                                     | [#153992](https://github.com/llvm/llvm-project/pull/153992) |
| `nextafterbf16`, `nextdownbf16`, `nexttowardbf16`, `nextupbf16`                                                                                                | [#153993](https://github.com/llvm/llvm-project/pull/153993) |
| `getpayloadbf16`, `setpayloadbf16`, `setpayloadsigbf16`                                                                                                        | [#153994](https://github.com/llvm/llvm-project/pull/153994) |
| `nanbf16`                                                                                                                                                      | [#153995](https://github.com/llvm/llvm-project/pull/153995) |
| `frexpbf16`, `ilogbbf16`, `ldexpbf16`, `llogbbf16`, `logbbf16`                                                                                                 | [#154427](https://github.com/llvm/llvm-project/pull/154427) |
| `modfbf16`, `remainderbf16`, `remquobf16`                                                                                                                      | [#154652](https://github.com/llvm/llvm-project/pull/154652) |
| `canonicalizebf16`, `iscanonicalbf16`, `issignalingbf16`, `copysignbf16`, `fdimbf16`                                                                           | [#155567](https://github.com/llvm/llvm-project/pull/155567) |
| `totalorderbf16`, `totalordermagbf16`                                                                                                                          | [#155568](https://github.com/llvm/llvm-project/pull/155568) |
| `scalbnbf16`, `scalblnbf16`                                                                                                                                    | [#155569](https://github.com/llvm/llvm-project/pull/155569) |
| `fmodbf16`                                                                                                                                                     | [#155575](https://github.com/llvm/llvm-project/pull/155575) |

The implementation status can be viewed at the libc `math.h` header implementation [status page](https://libc.llvm.org/headers/math/index.html), which is updated regularly.

## What was not done

- The implementation used a generic approach and did not rely on the `__bf16` compiler intrinsic, as it is not available in all compilers versions. Our goal is to ensure that the type is supported by all compilers and versions supported by [LLVM libc](https://libc.llvm.org/compiler_support.html).
- Hardware optimizations provided by Intel's [AVX-512_BF16](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#avx512techs=AVX512_BF16) were not utilized. These instructions only support round-to-nearest-even mode, always flush output denormals to zero, and treat input denormals as zero, which does not align with our goal. See [VCVTNE2PS2BF16 instruction description](https://www.felixcloutier.com/x86/vcvtne2ps2bf16#description).
- ARMv9 [SVE instructions](https://developer.arm.com/documentation/ddi0602/2021-12/SVE-Instructions/) were not utilized, as they are relatively new and not yet widely supported.
- Not all higher math functions were implemented due to time constraints.

## Future Work
- Implement the remaining higher math functions.
- Perform performance comparisons with other libc implementations once their `bfloat16` support is available and also with the [CORE-MATH](https://core-math.gitlabpages.inria.fr/) project.
- Update the test suite when the `mpfr_get_bfloat16` function becomes available.

## Acknowledgements
I would like to thank my mentors, Tue Ly and Nicolas Celik, for their invaluable guidance and support throughout this project. The project wouldn't have been possible without them. I am also grateful to the LLVM Foundation and the GSoC admins for giving me this opportunity.
