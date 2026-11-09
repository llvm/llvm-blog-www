---
author: "David Rivera (RiverDave)"
date: "2026-11-02"
tags: ["GSoC", "ClangIR", "MLIR", "CUDA", "HIP", "offloading"]
title: "GSoC 2026: Merging Host and Device ClangIR for Cross-Boundary CUDA/HIP Optimization"
---

Hi, I'm David Rivera, a third-year computer science undergrad at Ontario Tech University in Canada. For Google Summer of Code 2026 I worked on the [Host-Device CIR Combine Pipeline](https://discourse.llvm.org/t/gsoc-2026-host-device-cir-combine-pipeline-project-idea-discussion/89623), mentored by Konstantinos Parasyris (Dinos) and Joseph Huber.

I started contributing to ClangIR for the love of the game: I wanted to understand how compilers work underneath. GPU support in ClangIR was limited at the time, so I started working on it, and with the help of the maintainers I became a valuable contributor to the project, especially on the components related to GPU compilation.

This project started as Dinos's idea. He had a [work-in-progress merge pipeline](https://github.com/llvm/clangir/pull/2097) in the ClangIR incubator, and in January I added the driver-side support for CUDA on top of his branch. Before GSoC I also spent months landing CUDA and HIP support upstream ([tracking issue](https://github.com/llvm/llvm-project/issues/179278)), which this project depends on and which is like another mini GSoC project by itself.

GSoC took me from sending patches to coordinating with the community: contributing to an RFC, publishing our evaluations and prototypes, writing my first paper, which will appear at [LLVM-HPC at SC26](https://sc26.conference-program.com/presentation/?id=ws_llvmf112&sess=sess133), and attending my first LLVM Developers' Meeting. I also got to meet in person the maintainers I'd only interacted with through code review, which was a really cool experience.

TL;DR:

- We have a prototype that merges the host and device ClangIR modules of a CUDA or HIP translation unit into one module, runs passes that see both sides, and splits them back into the normal pipelines. We've measured real benefits with it.
- We implemented three optimizations on top of it: kernel argument constant propagation, launch bounds propagation, and alias information forwarded from host launch sites. Alias information provides by far the largest gains: a 1.67× geomean in GPU-region time on PolyBench/GPU on an NVIDIA A10, with caveats I go through below.
- The prototype isn't merged upstream yet. The foundations have landed, including [the container](https://github.com/llvm/llvm-project/pull/206576) and the work that followed [Dinos's RFC](https://discourse.llvm.org/t/rfc-clangir-making-cir-pipeline-boundaries-first-class-driver-artifacts/90998), we have a roadmap for the rest, and we'll be working on it in the coming months.

# Background

Clang compiles a CUDA or HIP translation unit several times: one host `cc1`, plus one device `cc1` per GPU architecture. There is never a single phase in the compiler where we have visibility of both the host launches and the kernels they launch, so co-optimizing them isn't possible.

Here's what that costs:

```cpp
__global__ void acc(float *out, const float *a, const float *b, int n) {
  int tid = blockIdx.x * blockDim.x + threadIdx.x;
  if (tid < n)
    out[tid] = a[tid] + b[tid];
}

int main() {
  // ...
  cudaMalloc((void **)&a, bytes);
  cudaMalloc((void **)&b, bytes);
  cudaMalloc((void **)&out, bytes);
  acc<<<(n + 255) / 256, 256>>>(out, a, b, n);
}
```

Anyone reading this can see that `out`, `a` and `b` are three separate buffers. The device compilation can't: it only has the kernel, so it has to assume `out` may overlap `a` or `b`.

We can't just compile everything once, because headers can branch on `__CUDA_ARCH__`, so parsing and Sema have to run once per side. What we can do is bring the two sides together after that step.

Giving the compiler one view of both sides isn't a new idea. The [Heterogeneous LLVM-IR Modules RFC](https://discourse.llvm.org/t/rfc-heterogeneous-llvm-ir-modules/56011) from 2020 sought the same visibility, but a conventional LLVM module has one target and one data layout. MLIR modules nest and each can carry its own, and [ClangIR](https://llvm.github.io/clangir/) (CIR) is the bridge that gets a Clang AST into MLIR, with LLVM and the target backends still downstream. Since CIR comes after Sema, each side contributes its own correct translation and we merge those.

## Getting CUDA and HIP through CIR first

None of this works if CIR can't compile the programs in the first place, so a lot of my time went into parity: calling conventions, kernel registration, NVPTX builtins, address spaces and function attributes. In August, across 21 [PolyBench/GPU](https://github.com/RiverDave/polybenchGpu) programs, CIR matched classic CodeGen at 0.998× runtime on an H100 and 1.008× on an MI300X, with about 3% more compile time ([raw data](https://github.com/RiverDave/polybench-results)). We wrote up this part of the work, how CUDA and HIP kernels lower through CIR, in the paper that will appear at SC26. 25 of my PRs merged upstream during the project, and they're listed at the end of this post.

# The merge pipeline

The prototype lives on my fork. With `--clangir-offload-merge`, the driver stops each compilation after CIR, merges the results, runs the cross-boundary passes, splits the module back apart and resumes each `cc1` from CIR:

```bash
clang -x cuda -fclangir --clangir-offload-merge --cuda-gpu-arch=sm_86 -c kernel.cu
clang -x hip  -fclangir --clangir-offload-merge --offload-arch=gfx942 -c kernel.hip
```

{{< figure src="/img/gsoc26-cir-offload-merge-driver.png" alt="Driver flow: host and device cc1 emit CIR, CIRMergeJobAction combines them and runs the cross-boundary passes, CIRSplitJobAction splits them, and each cc1 resumes from CIR">}}

Two new job actions, `CIRMergeJobAction` and `CIRSplitJobAction`, both map to a standalone tool, `cir-offload-merge`. You can also run the steps by hand:

```bash
cir-offload-merge -combine \
  -input=host.cir -input=device-sm86.cir \
  -targets=host-x86_64-unknown-linux-gnu,cuda-nvptx64-nvidia-cuda--sm_86 \
  -output=combined.cir
```

This works for CUDA and HIP, with one or several GPU architectures.

## The container

The merged module is an ordinary MLIR module with host and device modules nested inside it. Trimmed:

```mlir
module attributes {cir.offload.container} {
  module @host attributes {cir.offload.kind = #cir.offload_kind<host>} {
    // stubs + launch sites
  }
  module @device_cuda_nvptx64_nvidia_cuda__sm_86
      attributes {cir.offload.kind = #cir.offload_kind<device>} {
    // kernels, cc(ptx_kernel)
  }
}
```

My first version was a dedicated `cir.offload.container` op, because I wanted the verifier to enforce the hierarchy: one host module, then one or more device modules. In review of [#206576](https://github.com/llvm/llvm-project/pull/206576), Andy Kaylor asked whether we needed the op at all, or whether a module marked with an attribute could be the container. Looking at how the `gpu` dialect verifies `gpu.container_module`, I could keep the same invariants in `CIRDialect::verifyOperationAttribute` without introducing a new op, so that's what landed.

## Connecting launches to kernels

Being in the same module doesn't connect anything, because a launch never calls the kernel directly. It goes through a stub and the runtime:

```cpp
// acc<<<grid, block>>>(out, a, b, n) lowers to:
__cudaPushCallConfiguration(grid, block);
__device_stub__acc(out, a, b, n);

// and the stub, generated by Clang:
void __device_stub__acc(...) {
  void *args[] = {&out, ...};
  cudaLaunchKernel((void *)__device_stub__acc, grid, block, args);
}
```

The link between the stub and the kernel only exists at runtime, through `__cudaRegisterFunction`. So the first analysis I wrote, `KernelBindingTable`, recovers that mapping statically: each host stub, its device kernel in each architecture's module, and every launch site with its arguments and grid and block geometry. Every pass below is built on it. Reflecting on the implementation, we believe this mapping should be an operation in the IR rather than an analysis, which I come back to under future work.

## What made this harder than it looks

**Resuming from `.cir`.** The pipeline has to re-enter `cc1` with CIR as input and no `ASTContext`, so anything the later stages used to read from the AST has to live on the module. Dinos led the upstream work on this, starting from [an RFC](https://discourse.llvm.org/t/rfc-clangir-making-cir-pipeline-boundaries-first-class-driver-artifacts/90998) and tracked in [#227917](https://github.com/llvm/llvm-project/issues/227917). Lowering `.cir` input to objects landed in October ([#232506](https://github.com/llvm/llvm-project/pull/232506)), and stopping before ABI lowering is still open.

**One context for two targets.** Combining parses every input into a single `MLIRContext`, and CIR records are uniqued by name, so a different body for the same name on the device side is silently dropped. Valid code hits this: a host-only lambda shifts CIRGen's `anon.N` counter, so the names stop matching. I have a prototype fix that namespaces device records on [a separate branch](https://github.com/RiverDave/llvm-project/tree/gsoc/diag-1-kernel-arg-layout-check).

# The first passes, and what moved runtime

We started with a couple of passes that were partly a concept of how cross-boundary optimization could work:

| Pass | What it does | Runtime result |
| --- | --- | --- |
| Kernel argument constant propagation | Folds an argument into the kernel when every launch passes the same constant | Neutral on its own (1.0016× at 1× size, 0.9996× at 2×), but binaries were 11.8% smaller |
| Launch bounds propagation | Records the largest constant block size as `nvvm.maxntid`, like a user-written `__launch_bounds__` | 0.9998× geomean, and the SASS (NVIDIA's machine code) was byte-identical on all 21 programs |

We expected launch bounds to be the biggest early win, since PolyBench declares its block sizes statically. On this suite the hint is invariant, and the reason is useful. NVPTX has no register allocator in LLVM, so `ptxas` makes the real decisions, and `maxntid` only gives it room to use more registers per thread. These kernels don't spill registers to memory, so that room has nothing to relieve. The HeCBench kernels we forced by hand with stock `nvcc` were register-lean as well and showed the same result. We expect kernels with higher register pressure, the ones that actually spill, to benefit from this optimization, and we still need to widen our search for candidates like that. The pass only runs for NVIDIA today. Experimenting with AMD is something we still have to do, and we're aware we might see different results on AMD or on Intel targets, although Intel isn't something we support here.

So by early September the infrastructure worked and nothing was faster.

At our sync that week, Dinos suggested looking at aliasing: the host sees facts about buffers, and it could attach metadata like `noalias`. Andy had just added [the start of alias analysis](https://github.com/llvm/llvm-project/pull/214357) to CIR upstream.

After launch bounds, I wanted to know whether the fact could matter before writing a pass for it. I injected `__restrict` by hand on the kernel pointers of `2mm`, `3mm`, `gemm` and `gramschmidt` and compiled with classic CodeGen, with no CIR involved. I wrote the rule down first: if the SASS stayed identical on all 9 kernels, we wouldn't build the pass. It changed on all four programs, and kernel-region time came out about 2.3× faster on `2mm`, `3mm` and `gramschmidt` and 1.9× on `gemm`, with identical checksums. The remaining question was whether the compiler could prove what I'd written by hand.

# Launch-derived noalias

The `OffloadPointerFacts` pass ([#30](https://github.com/RiverDave/llvm-project/pull/30)) starts from the `acc` example at the top of this post. For each kernel, it uses the binding table to look at every visible launch and checks that:

- every launch is known,
- each pointer argument traces to a different live `cudaMalloc`,
- and the kernel can't reach those buffers any other way: no address-taken global, loaded pointer, escape or in-kernel allocation.

If all of that holds, the existing kernel parameters get `llvm.noalias`:

```mlir
cir.func @_Z3accPfPKfS1_i(%out: !cir.ptr<!cir.float> {llvm.noalias},
                          %a: !cir.ptr<!cir.float> {llvm.noalias},
                          %b: !cir.ptr<!cir.float> {llvm.noalias},
                          %n: !s32i) cc(ptx_kernel)
```

The proof is kernel-wide and conservative: a repeated allocation, an unprovable argument or an unprovable path through the body drops every annotation on that kernel.

My first version got this wrong, and review caught it. Dinos pointed out that I never proved the allocation reaches the launch. I only checked the order of stores, so this got `noalias`:

```cpp
float *A = B;
if (c)
  cudaMalloc(&A, n);   // skipped when !c
k<<<1, 1>>>(A, B);     // A == B
```

The allocation now has to dominate the launch. He also showed a pointer smuggled through a `uintptr_t` argument, and a `__noinline__` callee writing to a buffer through a device global. In the end, callees are checked with the same rules as the kernel, and later rounds of review from Dinos and Steffen Larsen turned the body check into an allow-list of pointer sources and made inline asm a rejection.

The pass only adds attributes, and unchanged LLVM passes do the optimizing. Here's PolyBench `gemm` at `-O3` for `sm_86`, trimmed, with index math and unrolling removed:

```llvm
; before: c may alias a or b
loop:
  %acc = phi float [ %c0, %pre ], [ %sum, %loop ]
  %va  = load float, ptr %a.ik
  %vb  = load float, ptr %b.kj
  %p   = fmul float (%alpha * %va), %vb
  %sum = fadd float %acc, %p
  store float %sum, ptr %c.ij      ; every iteration
  br i1 %done, label %exit, label %loop
```

```llvm
; after: noalias from the host
loop:
  %acc = phi float [ %c0, %pre ], [ %sum, %loop ]
  %va  = load float, ptr %a.ik
  %vb  = load float, ptr %b.kj
  %p   = fmul float (%alpha * %va), %vb
  %sum = fadd float %acc, %p
  br i1 %done, label %exit, label %loop
exit:
  store float %sum, ptr %c.ij      ; once
```

The sum was already in a register before the change. The store only stayed in the loop because the next load might read `c`. With the fact in place, LICM's store promotion sinks it, taking the kernel from 6 stores to 2. Turning that off with `-disable-licm-promotion` brings all 6 back. The NVPTX backend also gets to use the non-coherent load form: 10 of the 11 global loads become `ld.global.nc`, and the remaining one is `c`, which is written.

This pattern, accumulating inside a loop while alias uncertainty pins the memory access, is the one I kept seeing across the suite.

## Results

Pass on versus pass off on an NVIDIA A10 (`sm_86`), with everything else identical: 3 warmup runs plus 8 samples, serial, using the benchmark's own GPU-region timer. These were measured at `fbda065af219`, a revision that predates the final rounds of review. Validation passed in all 42 records across the two arms.

| Benchmark | Speedup | Benchmark | Speedup |
| --- | ---: | --- | ---: |
| `correlation` | **7.17×** | `gemm` | **1.90×** |
| `covariance` | **7.08×** | `atax` | **1.79×** |
| `bicg` | **2.37×** | `mvt` | **1.78×** |
| `2mm` | **2.28×** | `gesummv` | **1.29×** |
| `3mm` | **2.26×** | `syrk` | **1.07×** |
| `gramschmidt` | **2.24×** | `syr2k` | **1.03×** |
| `gemver` | **2.05×** | `adi` | **1.02×** |
| `doitgen` | **2.03×** |  |  |

The other six stay within about 1.5% of 1.00×, with `jacobi-2d` the lowest at 0.985×. Geomean over all 21 is **1.67×**, and compile time goes up by 0.006 s per benchmark. These are the numbers I posted [on Discourse](https://discourse.llvm.org/t/rfc-enable-clangir-build-by-default/91730/96) and on [#30](https://github.com/RiverDave/llvm-project/pull/30).

That table only covers GPU-region time. I also compared the whole merge pipeline against the same `-O3` CIR compilation without it, including wall time for the whole process:

| Problem size | GPU-region speedup | Wall-time speedup |
| --- | ---: | ---: |
| 1× | 1.72× | 1.19× |
| 2× | 1.68× | 1.24× |
| 4× | 1.64× | 1.25× |

The wall-time gain is smaller because GPU work is only part of the process. This run used an earlier revision of the pipeline at [`7a94fd56356d`](https://github.com/RiverDave/llvm-project/commit/7a94fd56356d) and covers 20 programs. The 4× run was timing-only on a different A10. [#30](https://github.com/RiverDave/llvm-project/pull/30) has the details.

Please read those numbers with these in mind:

- **The kernels were forced `static`.** If a kernel's host stub is externally visible, another object could launch it, so the pass correctly leaves it alone. I [patched the suite](https://github.com/RiverDave/polybenchGpu/tree/experiment/static-kernels-pr25) to make the kernels internal, which isn't what you'd get on the suite as shipped.
- **PolyBench is a starting point.** These are single-file programs with flat kernels and small call graphs. Future work is to experiment deeper on large applications, catch the transformations that could yield better benefits there, and measure compile time on them too.
- **7× isn't typical.** In the whole-pipeline run, `correlation` and `covariance` dropped to about 4.6× at 4× problem size.
- **One GPU, one vendor.** The pass only recognizes `cudaMalloc` today, so these are CUDA numbers. The pipeline runs for HIP, but this pass doesn't fire there on the final branch.

## HeCBench and other benchmarks

Forcing `static` on every kernel isn't an option for real code. Steffen contributed [kernel cloning helpers](https://github.com/RiverDave/llvm-project/pull/28) that create a specialized copy of a kernel for a chosen set of launches and always keep the original, since the runtime can still reach it by name. A [separate branch](https://github.com/RiverDave/llvm-project/tree/gsoc/aa-kernel-cloning) builds on them, so proven launches get an internal clone that carries the facts. That's crucial for not having to write `static` all the time, and it's what let us explore much more complex benchmarks. On the four unmodified PolyBench programs I ran, the gains mostly hold: `2mm` 2.26×, `3mm` 2.18×, `gemm` 1.81×, and `lu` 0.997× as a control.

On [HeCBench](https://github.com/ORNL/HeCBench), the pass fires much less often and the gains are smaller:

| Benchmark | Speedup | Note |
| --- | ---: | --- |
| `chemv` | 1.75× | Kernel pulled into the host TU with `-include`; binary grows from 25.7 KB to 38.1 KB |
| `softmax` | 1.05× | Stock source; loads become the read-only form |
| miniFE, CG iteration | 1.016× | `matvec` inlined by hand; only 1 of 11 kernels is cloned |

To see how far this could go, I also forced `noalias` on every kernel pointer in IR, locally with stand-in headers, which gives an upper bound. Across 1,152 HeCBench kernels, stores or loops went down in 11. The accumulate-through-a-buffer pattern is less common there: a source scan finds it in 24 of 1,427 kernels, against 24 of 47 in PolyBench. In Rodinia's 78 kernels, none lost a loop or a store from an innermost loop.

When the pass doesn't fire on real code, these are the main reasons:

| What blocks the pass | Where we saw it |
| --- | --- |
| Allocation and launch live in different functions | 17 of 46 failed allocation traces in one HeCBench survey, plus miniFE |
| Kernel defined in one file, declared in a header and launched from another | 41 of 164 HeCBench benchmarks, including `chemv` |
| `cudaMalloc` inside a nested scope, like an error-check macro | About 77 of 519 HeCBench benchmarks |
| Two buffers swapped between launches | 14 HeCBench benchmarks, by source search |
| Kernel passes a buffer pointer to a helper | LULESH and 6 other HeCBench benchmarks |
| Buffers owned by a framework object | darknet, where forcing `noalias` takes the average pooling kernel from 67 SASS stores to 1, but none of the 48 launch sites we checked can be traced |

So the PolyBench numbers don't carry over to real code today. The pattern is less common there, and when it's present, the pass often can't prove the facts.

# Limitations

- **Separate TUs hide definitions.** We merge one TU at a time, so a launch of a kernel defined in another TU only sees a declaration. The pipeline is also non-RDC only, so a device function calling a device function in another TU isn't covered either. Solving this needs visibility across translation units, which is an inherent problem in compilers, not one specific to this pipeline.
- **Tracing facts across the boundary is hard.** When the launch sits in a function and the allocation happens in its caller, we can't walk back and see every allocation, so the kernel gets rejected. A proper interprocedural framework would help more than another pass.
- **Specialization has a cost.** Cloning grows the binary, as `chemv` shows, so a cost model that decides when a clone is worth it would be great, and looking at what LLVM does for specialization is a good starting point.

# Status and future work

| Piece | Status |
| --- | --- |
| Container attribute and verifier | Upstream ([#206576](https://github.com/llvm/llvm-project/pull/206576)) |
| Emitting and resuming `.cir` through `cc1` | Upstream, stop point before ABI lowering still open |
| Driver integration, `cir-offload-merge`, binding table, passes | Fork, on [`gsoc/combine-cir`](https://github.com/RiverDave/llvm-project/tree/gsoc/combine-cir) |

I'm staying on this after GSoC, and I'm planning to upstream all of these findings. The bigger goal is a much more generic representation of GPU programs in CIR, one that works across different GPU targets and runtimes:

- **Launches connected through the call graph.** `KernelBindingTable` is a proof of concept, and it's one of the parts we likely need to reassess. MLIR has generic transformations like `symbol-dce` and `sccp` that dialects opt into through interfaces, and we could rely on them if a launch connected the call graph on the host to the kernel on the device. A higher-level launch op, similar to what the `gpu` dialect has, would make that edge explicit in the IR. This was also some of the feedback we got at the LLVM Developers' Meeting.
- **Generic runtime operations.** The noalias pass has to recognize `cudaMalloc`, and with today's shape we'd have to track every runtime's allocation, whether that's CUDA, HIP, liboffload or anything else. Ideally these operations would be generic in the CIR dialect, which is in part what the `gpu` dialect does.

This representation can also introduce new diagnostics, because it sees both sides of the program. A [small prototype](https://github.com/RiverDave/llvm-project/tree/gsoc/diag-1-kernel-arg-layout-check) warns when a kernel argument's layout differs between host and device.

I think the combine tool has a lot of potential, and conversations at the meeting with people from several important vendors in the space backed that up. I believe it's one of the reasons someone would want to use ClangIR in the first place. Steffen also continues to help upstream and on the fork, and Mariya Podchishchaeva mainly upstream.

# What I learned

The biggest surprise was how much alias analysis gave us. Initially our optimizations were a bit shallow: we went from constant propagation to launch bounds, and neither caused a noticeable speedup on PolyBench. I was sort of expecting aliasing to unlock something, since I've read a couple of LLVM's middle-end passes and it's one of the things that blocks many of them, but not at the level it did on PolyBench.

The other thing I really learned is how to evaluate whether a pass is worth chasing. What I mainly did was inject the information a pass could derive, either from the front end or into LLVM IR, and check whether later passes consume it and whether SASS ultimately benefits. That's far less work than the pass.

# Acknowledgements

Thanks to my mentors, Dinos Parasyris and Joseph Huber, and especially to Dinos, whose idea this was and who I've been working with on CUDA support in CIR since January. Thanks to Steffen Larsen (AMD), who was working on a similar staging layer, reached out, and ended up contributing [#27](https://github.com/RiverDave/llvm-project/pull/27), [#28](https://github.com/RiverDave/llvm-project/pull/28), [#31](https://github.com/RiverDave/llvm-project/pull/31) and [#32](https://github.com/RiverDave/llvm-project/pull/32) to the fork. Thanks to the ClangIR maintainers Bruno Cardoso Lopes, Andy Kaylor, Erich Keane and Henrich Lauko for their reviews, and to Mariya Podchishchaeva for her upstream work on CIR. Thanks also to Google and the LLVM community for making GSoC possible.

# Links

- Final work: [`gsoc/combine-cir`](https://github.com/RiverDave/llvm-project/tree/gsoc/combine-cir)
- Talk: *CUDA and HIP in ClangIR*, LLVM Offload Workshop, Santa Clara, October 2026
- Paper: [*GPU Device Compilation in ClangIR: Lowering CUDA and HIP Kernels Through the CIR Pipeline*](https://sc26.conference-program.com/presentation/?id=ws_llvmf112&sess=sess133), with Bruno Cardoso Lopes, Joseph Huber and Dinos Parasyris, LLVM-HPC 2026 at SC26, November 16
- Benchmark harness: [`polybench-gpu-audit`](https://github.com/RiverDave/polybench-gpu-audit)

Upstream PRs merged during the project:

| Area | PRs |
| --- | --- |
| Offload container | [#206576](https://github.com/llvm/llvm-project/pull/206576) |
| CUDA/HIP codegen | [#195381](https://github.com/llvm/llvm-project/pull/195381), [#195382](https://github.com/llvm/llvm-project/pull/195382), [#195391](https://github.com/llvm/llvm-project/pull/195391), [#199270](https://github.com/llvm/llvm-project/pull/199270), [#204663](https://github.com/llvm/llvm-project/pull/204663), [#215314](https://github.com/llvm/llvm-project/pull/215314), [#231390](https://github.com/llvm/llvm-project/pull/231390) |
| NVPTX builtins and attributes | [#195214](https://github.com/llvm/llvm-project/pull/195214), [#195217](https://github.com/llvm/llvm-project/pull/195217), [#214708](https://github.com/llvm/llvm-project/pull/214708), [#216447](https://github.com/llvm/llvm-project/pull/216447), [#221255](https://github.com/llvm/llvm-project/pull/221255), [#221261](https://github.com/llvm/llvm-project/pull/221261), [#221262](https://github.com/llvm/llvm-project/pull/221262) |
| Address spaces | [#196868](https://github.com/llvm/llvm-project/pull/196868), [#226649](https://github.com/llvm/llvm-project/pull/226649), [#226650](https://github.com/llvm/llvm-project/pull/226650) |
| Aliasing and floating point | [#221817](https://github.com/llvm/llvm-project/pull/221817), [#221818](https://github.com/llvm/llvm-project/pull/221818), [#226334](https://github.com/llvm/llvm-project/pull/226334) |
| Tooling | [#196245](https://github.com/llvm/llvm-project/pull/196245), [#200266](https://github.com/llvm/llvm-project/pull/200266), [#229565](https://github.com/llvm/llvm-project/pull/229565), [#229575](https://github.com/llvm/llvm-project/pull/229575) |

Fork PRs are on [RiverDave/llvm-project](https://github.com/RiverDave/llvm-project/pulls?q=is%3Apr+author%3ARiverDave).
