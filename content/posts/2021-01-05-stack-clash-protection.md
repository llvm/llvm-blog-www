---
author: Serge Guelton, Sylvestre Ledru, Josh Stone
date: "2021-01-30T10:00:00Z"
tags: ["Clang", "llvm"]
title: Bringing Stack Clash Protection to Clang / X86 — the Open Source Way
---

# Context

Stack clash is an attack that dates back to 2017, when the Qualys Research Team
released an advisory with a [joint blogpost][QualysAdvisory]. It basically
exploits large stack allocation (greater than `PAGE_SIZE`) that can lead to
stack read/write *not* triggering the [stack guard page][StackGuardPage] allocated by the Linux
Kernel.

Shortly after the advisory got released, GCC provided a [couter-measure][GCCStackClashProtection] activated by `-fstack-clash-protection` that
basically consists in splitting large allocation in chunks of `PAGE_SIZE`,
with a probe in each chunk to trigger the kernel stack guard page.

This has been a major security difference between GCC and Clang since then. It
has even been identified as a blocker by Fedora to move from GCC to Clang as the
compiler for some projects that already made the move upstream, leading to [extra
maintenance][FedoraClangGCC] for packagers.

Support for this flag [landed in Clang in 2020][CLANGStackClashProtection],
only for X86, SystemZ and PowerPC. Its implementation is a result of a fruitful
collaboration between LLVM, Firefox and Rust developpers.


Rust already had a countermeasure implemented in the form of a runtime call to
perform the stack probing. With LLVM catching up, using a more leightweight
approach got [investigated in Rust][RustStackProbe].


# Countermeasure Description

The Clang implementation for X86 is derived from the GCC implementation, with a
few distinctions. The core ideas are:

1. thanks to X86 calling convention, we get a free probe at each call site,
   which means that each function starts with a probed stack

2. when probing the stack in the function prologue, we don't probe the tail of
   the allocation. Stated otherwise, if the stack size is `PAGE_SIZE + PAGE_SIZE/2`,
   we want to probe only once. This is important to limit the number
   of probes: if the stack size is lower than `PAGE_SIZE` no probe is needed

3. because a signal can interrupt the execution flow any time, at no point
   should we have two stack allocations (lower than `PAGE_SIZE`) without a probe
   in between.


The probing strategy for stack allocation varies based on the size of the stack
allocation. If it's smaller than `PAGE_SIZE`, thanks to (2) no probing is
needing. If it's below a small multiple of ``PAGE_SIZE``, then the probing loop
can be unrolled. Otherwise a probing loop alternates stack allocation of
`PAGE_SIZE` bytes and probe, starting with the allocation thanks to (1).

As side effect of (2) is that when performing a dynamic allocation, we need to
probe *before* updating the stack, otherwise we got a hole in the protection.
This probe cannot be done after the stack update, even with an offset, because
of (3). Otherwise we end up with a bug as this one found in [GCC][GCCProbeBug]

The following scheme attempts to summarize the allocation and probing
interaction between static and dynamic allocations:

         + ----- <- ------------ <- ------------- <- ------------ +
         |                                                        |
    [free probe] -> [page alloc] -> [alloc probe] -> [tail alloc] + -> [dyn probe] -> [page alloc] -> [dyn probe] -> [tail alloc] +
                                                                  |                                                               |
                                                                  + <- ----------- <- ------------ <- ----------- <- ------------ +


# Validation with Firefox

Firefox provides an amazing test bench to evaluate the impact of compiler
changes. Indeed, with more than 12MLOC of C/C++ and 3MLOC of Rust built using
PGO/LTO and [XLTO][XLTO], most of the important cases are covered.

Moreover, Firefox being supported on a large set of operating system and
architectures, it was a great way to test the Stack Clash protection on various
set of configurations.

The work is detailed in the [bug 1588710][bug1588710].


## Functional Testing

To make sure that Firefox would perform as expected, we leveraged the huge test
suite to verify that the product would still work as expected with this option.

We used the [`try auto`][ml-test], a new command which will run the most
appropriate set of tests for such kind of changes during the development phase.
Then, once the patch landed into Mozilla-central (Firefox nightly), the whole
test suite is executed, presenting about 29 days of machine time for about 9000
tasks.

Thanks to this infrastructure, we have identified an [issue with
`alloca(0)`][alloca] generating buggy machine code.
Fortunately, the [fix][bug47657] was already in the trunk version of LLVM.
We cherry-picked the fix in our custom Clang build which addressed our issue.


## Performance Testing

Over the years, Mozilla has developed a few tools to evaluate performance
impact of changes, from micro-benchmark to page loads. These tools have been key
to improve Firefox overall performances but also evaluate the impact of the [move
to Clang][move-clang] on all platforms done a couple years ago.

The usual procedure to evaluate performances improvements/regressions is to:

1. Run two builds with benchmarks. One without the patch, one with it.

2. Leverage the tooling to rerun the benchmark (usually 5 to 20 times) to limit
   the noise.

3. Compare the various benchmark to see if significant regressions can be
   identified.

In the context of this project, we run the usual benchmarks sensitive to C++
changes and we haven't identified any [regression][Perfherder] in term of
performances.


## Current status

Firefox nightly on Linux is now compiled with the stack-clash-option from
January 8th 2021. We have not detected any regressions since it landed.
If everything goes well, this change should ship with Firefox 86 (planned for
mid February 2021).

# Validation With Rust

Rust has long supported the callback style of the LLVM `probe-stack` attribute,
using the function `__rust_probestack` defined in its own compiler builtins
library. In Rust's spirit of safety, this attribute is added to *all* functions,
letting LLVM sort out which actually need probing. However, forcing such a call
into every function with a large stack frame is not ideal for performance,
especially for those cases that could use just a few unrolled probes inline.
Furthermore, Rust only has this callback implemented for its Tier 1 (most
supported) targets, namely i686 and x86_64, leaving other architectures without
protection so far. Therefore, letting LLVM generate inline stack probes is
beneficial both for the performance of avoiding a call and for the increased
architecture support.

Since the Rust compiler is written in Rust itself, with stack probing enabled by
default, it makes a great functional test for any new code generation feature.
The compiler is bootstrapped in stages, first building with a prior version,
then rebuilding with the result of that first stage. Codegen issues are often
revealed if the compiler crashes during that rebuild, and experiments with
inline stack probes were no different, leading to fixes in
[D82867](https://reviews.llvm.org/D82867) and
[D90216](https://reviews.llvm.org/D90216). Both of these were simple errors that
were not apparent in existing FileCheck tests, showing the importance of
actually executing generated code.

An [issue][RustStackAlign] also led to the realization that there was a more
general bug impacting both GCC and LLVM implementation of
`-fstack-clash-protector`, leading to a new patch set on the LLVM side.
Essentially, the observed behavior is the following:

Alignment requirement behave similarly to allocation with respect to the stack:
they (may) make it grow. For instance the stack allocation for an `char
foo[4096] __attribute__((aligned(2048)));` is done through:

    and     rsp, -2048
    sub     rsp, 6024

Both `and` and the `sub` actually update the stack! To take that effect into
account, the LLVM patch considers the `and rsp, -2048` as a `sub rsp, 2048`
when computing the probing distance, which means considering the worst
case scenario.

For future work on the Rust side, inline stack probes will replace
`__rust_probestack` on i686 and x86_64 soon in [Rust
pr77885](https://github.com/rust-lang/rust/pull/77885), and that will include
[perf results](https://perf.rust-lang.org/) to monitor the effect. After that,
additional architectures can be functionally tested and enabled for inline stack
probes as well, increasing the reach of Rust's memory safety.

# Validation with a Binary Tracer

None of the above validation validates the security aspect of the protection. To
have more confidence on the actual probing scheme implementation, we implemented
a binary tracer based on the (awesome) [QBDI](https://github.com/QBDI/QBDI)
Dynamic Binary Instrumentation framework. This Proof Of Concept (POC) is
available on GitHub:
[stack-clash-tracer](https://github.com/serge-sans-paille/stack-clash-tracer)

This tool instruments all stack allocation and memory access of a running
binary, logs them and checks that no stack allocation is greater than
`PAGE_SIZE` and that we get an actual probing between two allocations.

Here is a sample session that showcases large stack allocation issues:

    $ cat main.c
    #include <alloca.h>
    #include <string.h>
    int main(int argc, char**argv) {
      char buffer[5000];
      strcpy(buffer, argv[0]);
      char* dynbuffer = alloca(argc * 1000);
      strcpy(dynbuffer, argv[0]);
      return buffer[argc] + dynbuffer[argc];
    }
    $ gcc main.c -o main
    $ LD_PRELOAD=./libstack_clash_tracer.so ./main 1
    [sct][error] stack allocation is too big (5024)
    $ LD_PRELOAD=./libstack_clash_tracer.so ./main 1 2 3 4 5
    [sct][error] stack allocation is too big (5024)
    [sct][error] stack allocation is too big (6016)

The same code, compiled with `-fstack-clash-protection`, is safer (apart from
the stupid use of `strcpy`, that is)

    $ gcc main.c -fstack-clash-protection -o main
    $ LD_PRELOAD=./libstack_clash_tracer.so ./main 1
    $ LD_PRELOAD=./libstack_clash_tracer.so ./main 1 2 3 4 5

Small bonus of this compiler-independent approach: we can verify both GCC and
Clang implementation `:-)`

    $ clang main.c -fstack-clash-protection -o main
    $ LD_PRELOAD=./libstack_clash_tracer.so ./main 1
    $ LD_PRELOAD=./libstack_clash_tracer.so ./main 1 2 3 4 5

To come back on the Firefox test case, before we landed the change, we could
see:


    $ LD_PRELOAD=./libstack_clash_tracer.so firefox-bin
    [sct][error] stack allocation is too big (4168)

Once Firefox nightly shipped with stack clash protection, this warning
disappears.

# Conclusion

Aside from the technical aspects of the countermeasure, it is interesting to
note that its Clang implementation was derived from the GCC implementation, but
led to an issue being reported in the GCC codebase. The Clang-generated code got
validated by Firefox People, tested by Rust people who reported several bugs,
some impacting both Clang and GCC implementation, the circle is complete!


# References

[QualysAdvisory]: https://blog.qualys.com/vulnerabilities-research/2017/06/19/the-stack-clash

[StackGuardPage]: https://lkml.org/lkml/2017/6/22/345

[GCCStackClashProtection]: https://gcc.gnu.org/legacy-ml/gcc-patches/2017-07/msg00556.html

[CLANGStackClashProtection]: https://reviews.llvm.org/D68720

[RustStackProbe]: https://github.com/rust-lang/rust/pull/77885

[FedoraClangGCC]: https://pagure.io/fesco/issue/2020

[GCCProbeBug]: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97928

[RustStackAlign]: https://github.com/rust-lang/rust/issues/70143

[XLTO]: https://blog.llvm.org/2019/09/closing-gap-cross-language-lto-between.html

[ml-test]: https://hacks.mozilla.org/2020/07/testing-firefox-more-efficiently-with-machine-learning/

[Perfherder]: https://treeherder.mozilla.org/perfherder/comparesubtest?originalProject=try&newProject=try&newRevision=62108fa48bd15fe01f1a0f1ffab133af9b4207cc&originalSignature=1904137&newSignature=1904137&framework=10&originalRevision=a47c98b909b61035dae2e1e00883f2ade0fef129

[bug1588710]:  https://bugzilla.mozilla.org/show_bug.cgi?id=1588710

[alloca]: https://bugzilla.mozilla.org/show_bug.cgi?id=1588710#c40

[bug47657]: https://bugs.llvm.org/show_bug.cgi?id=47657

[move-clang]: https://blog.mozilla.org/nfroyd/2018/05/29/when-implementation-monoculture-right-thing/
