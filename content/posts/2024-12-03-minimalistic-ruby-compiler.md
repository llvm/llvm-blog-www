---
author: "Alex Denisov, Amir Rajan"
date: "2024-12-09"
tags: ["llvm", "mlir", "ruby", "dragonruby"]
title: "Lightstorm: minimalistic Ruby compiler"
---

Some time ago I was talking about an [ahead-of-time Ruby compiler](https://www.youtube.com/watch?v=NfMX-dFMSr0).
We started the project with certain goals and hypotheses in mind, and while the original compiler is at nearly 90% completion, there are still those other 90% that needs to be done.

In the meantime, we decided to strip it down to a bare minimum and implement just enough features to validate the hypotheses.

Just like the original compiler we use MLIR to bridge the gap between Ruby VM's bytecode and the codegen, but instead of targeting LLVM IR directly, we go through EmitC dialect and targeting C language, as it significantly simplifies OS/CPU support. We go into a bit more details later.

The source code of our minimalistic Ruby compiler is here: https://github.com/dragonruby/lightstorm.

The rest of the article covers why we decided to build it, how we approached the problem, and what we discovered along the way.

## Motivation and the use case

Our use case is pretty straightforward: we are building [a cross-platform game engine that's indie-focused, productive, and easy to use](https://dragonruby.org). The game engine itself is written in a mix of C and Ruby, but the main user-interface is the Ruby itself.

As soon as the game development is done and the game is ready for "deployment," the code is more or less static and so we asked ourselves if we can pre-compile it into machine code to make it run faster.

But given all the dynamism of Ruby, why would a compilation make it faster?

So here comes our hypothesis. But first let's look at some high-level implementation details to see where the hypothesis comes from.

## Compilers vs Interpreters

While a language itself cannot be strictly qualified as compiled or interpreted, the typical implementations certainly are.

In case of a "compiled" language the compiler would take the whole program, analyze it and produce the machine code targeting a specific hardware (real or virtual), while an interpreter would take the program and execute it right away, one "instruction" at a time.

_The definition above is a bit handwavy: zoom our far enough and everything is a compiler, zoom in close enough and everything is an interpreter. But you've got the gist._

Most Ruby implementations are interpreter-based, and in our case we are using [mruby](https://mruby.org).

The mruby interpreter is a lightweight register-based virtual machine (VM).

Let's look at a concrete example. The following piece of code:

```ruby
42 + 15
```

is converted into the following VM bytecode, consisting of various operations (ops for short):

```asm
LOADI R1 42
LOADI R2 15
ADD R1 R2
HALT
```

The VM's interpreter loop looks as follows (pseudocode):

```c
dispatch_next:
  Op op = bytecode.next_op();
  switch (op.opcode) {
    case LOADI: {
      vstack.store(op.dest, mrb_int(op.literal));
      goto dispatch_next;
    }
    case ADD: {
      mrb_value lhs = vstack.load(op.lhs);
      mrb_value rhs = vstack.load(op.rhs);
      vstack.store(op.dest, mrb_add(lhs, rhs));
      goto dispatch_next;
    }
    // More ops...
    case HALT: goto halt_vm;
  }

halt_vm:
  // ...
```

For each bytecode operation the VM will jump/branch into the right opcode handler, and then will branch back to the beginning of the dispatch loop.
In the meantime, each opcode handler would use a virtual stack (confusingly located on the heap) to store intermediate results.

If we unroll the above bytecode manually, then the code can look like this:

```c
  goto loadi_1;

loadi_1:
  // LOADI R1 42
  mrb_value t1 = mrb_int(42);
  vstack.store(1, t1);
  goto loadi_2;

loadi_2:
  // LOADI R2 42
  mrb_value t2 = mrb_int(15);
  vstack.store(2, t2);
  goto add;

add:
  // ADD R1 R2
  mrb_value lhs = vstack.load(1);
  mrb_value rhs = vstack.load(2);
  mrb_value t3 = mrb_add(lhs, rhs);
  vstack.store(1, t3);
  goto halt;

halt:
  // shutdown VM
```

Many items in this example can be eliminated: specifically, we can avoid load/stores from/to the heap, and we can safely eliminate `goto`s/branches:

```c
  mrb_value t1 = mrb_int(42);
  mrb_value t2 = mrb_int(15);;
  mrb_value t3 = mrb_add(t1, t2);
  vstack.store(1, t3);
  goto halt;

halt:
  // shutdown VM
```

So here goes our hypothesis:

> ### Hypothesis
> By precompiling/unrolling the VM dispatch loop we can eliminate many load/stores and branches along with branch mispredictions, this should improve the performance of the end program.

We can also try to apply some optimizations based on the high-level bytecode analysis, but due to the Ruby's dynamism the static optimization surface is limited.

## Approach

As mentioned in the beginning, building a full-fledged AOT compiler is a laborious task which requires time and has certain constraints.

For the minimalistic version we decided to change/relax some of the constraints as follows:

 - the compiled code must be compatible with the existing ecosystem/runtime
 - the existing runtime must not require any changes
 - the supported language features should be easily "representable" in C

Unlike the original compiler, we are not targeting machine code directly, but C instead.
This eliminates a lot of complexity, but it also means that we only support a subset of the language (e.g., blocks and exceptions are missing at the moment).

This is obviously not ideal, but it serves important purpose - **our goal at this point is to validate the hypothesis**.

A classical compilation pipeline looks as follows:

![Classical compilation pipeline](/img/ruby-compiler/compilation-pipeline.png)


To build a compiler one needs to implement the conversions from the raw source file all the way down to machine code and the language runtime library.
Since we are targeting the existing implementation, we have the benefit of reusing the frontend (parsing + AST) and the runtime library.

Still, we need to implement the conversion from AST to the machine code.
And this is where the power of MLIR kicks in: we built a custom dialect ([Rite](https://github.com/DragonRuby/lightstorm/blob/3ed0077af589ba51b98954bba8869daf58e22b9e/include/lightstorm/dialect/rite.td)) which represents mruby VM's bytecode, and then use a number of builtin dialects (`cf`, `func`, `arith`, `emitc`) to convert our IR into C code.

At this point, we can just use clang to compile/link the code together with the existing runtime and that's it.

The final compilation pipeline looks as follows:

![Lightstorm compilation pipeline](/img/ruby-compiler/lightstorm-compilation-pipeline.png)

> With the benefit of MLIR we are able to build a funtional compiler in just a couple of thousands lines of code!

Now let's look at how it performs.

## Some numbers

Benchmarking is hard, so take these numbers with a grain of salt.

We run various (micro)-benchmarks showing results in the range of 1% to 1200% speedups, but we are sticking to the [aobench](https://openbenchmarking.org/test/pts/aobench) as it is very close to the game-dev workloads we are targeting.

mruby also uses [aobench](https://github.com/mruby/mruby/blob/e05dbf7bbc6d2fbecfd7d4a46418fbe4421bc160/benchmark/bm_ao_render.rb) as part of its benchmark suite, though we slightly modified it to replace `Number.each` blocks with explicit `while` loops.

Next we used excellent [simple-kpc](https://github.com/lunacookies/simple-kpc) library to capture CPU counters on Apple M1 CPU, namely we collect total cycles, total instructions count, branches, branch mispredictions, and load/stores (`FIXED_CYCLES`, `FIXED_INSTRUCTIONS`, `INST_BRANCH`, `BRANCH_MISPRED_NONSPEC`, and `INST_LDST` respectively).

Naturally, we also collect the total execution time.

All the benchmarks compare vanilla bytecode interpretation against the "unrolled" compiled version.

We are using mruby 3.0. While it's not the most recent version at the time of writing, it was the most recent version at the time we've build the compiler.

The following chart shows the results of our measurements.
The three versions we compare are the baseline on the left, compiled version without optimizations in the middle, and the compiled version plus [simple escape analysis](https://github.com/DragonRuby/lightstorm/blob/09042d2026f9251ff32ec079d99bf8ca2e5a6337/lib/optimizations/optimizations.cpp#L14) and common subexpression elimination (CSE) on the right side.

_The raw data and the formulas are [here](https://docs.google.com/spreadsheets/d/1KQdZjll0cOjWhfPO3KM-FN-OciUW53IecGOtTY6aYJM/edit?usp=sharing)._

![Benchmarks](/img/ruby-compiler/benchmark.png)

With all the current optimizations in place both the number of cycles and the total execution time went down by roughly ~30%.

We are able to eliminate ~17% of branches and ~28% of load/stores, while the branch misses were cut in half with ~55% decrease.

The numbers look promising, although the amount of load/stores and branches will certainly go up as we implement all the language features due to the way blocks and exceptions are handled.

On the other hand, we didn't touch the runtime implementation which together with LTO will enable some more improvements due to more inlining.

## Where to next?

As mentioned in the beginning, some parts of the engine itself are written in C with and some of them are purely due to performance reasons. We are looking into replacing those critical pieces with compiled Ruby. While we may still pay performance penalty, we hope that ease of maintenance will be worthwile.

In the meantime, do not hesitate to give it a shot, and if you have any questions reach out to [Alex](https://lowlevelbits.org/about/) or [Amir](https://amirrajan.net)!

## Some links

 - the compiler: https://github.com/DragonRuby/lightstorm
 - the game engine: https://dragonruby.org
 - our discord: https://discord.dragonruby.org
