---
author: Arthur Eubanks
date: "2021-03-26"
tags: ["llvm"]
title:  The New Pass Manager
---

# LLVM's New Pass Manager

## What is a pass manager?

A pass manager schedules transformation passes and analyses to be run on IR in a specific order. Passes can run on an entire module, a single function, or something more abstract such as a strongly connected component (SCC) in a call graph or a loop inside of a function. Scheduling can be simple, such as running a list of module passes, or running function passes on every function inside a module. Scheduling can also be more involved, such as making sure we visit SCCs in the call graph in the correct order.

A pass manager is also responsible for managing analysis results. Analyses (e.g. dominator tree) should be shared between passes whenever possible for efficiency reasons, since recomputing analyses can be expensive. To do so, the pass manager must cache results and recompute them when they are invalidated by transforms.

For testing purposes, we can add specific passes to a pass manager to test those passes. However, the typical use case is to run a predetermined pass pipeline. For example, `clang -O2` runs a predetermined set of passes on the input IR.

## What is LLVM's new pass manager?

LLVM currently has two separate pass managers: the legacy pass manager (legacy PM) and the new pass manager (new PM). When referring to "legacy PM" and "new PM", this includes all of the surrounding infrastructure, not just the entity that manages passes.

The legacy PM has been in use for a very long time and did its job fairly well. However, there were some missing features required for better optimization opportunities, most notably the ability to use function analysis results for arbitrary functions from the inliner. The specific motivating use case was that the inliner wanted to look at the profile data of callees recursively, especially in regards to deferred inlining where the inliner wants to look through simple "wrapper" functions. The legacy PM did not support retrieval of analyses for arbitrary functions in a CGSCC pass. A CGSCC pass runs on a specific strongly connected component (SCC) of the call graph. The pass manager makes sure we visit SCCs bottom-up so that callees are as optimized as possible when we get to their callers and callers have as precise information as possible. LLVM's inliner is a CGSCC pass due to being a bottom-up inliner. This major limitation of the legacy PM, along with other warts, prompted the desire for a new pass manager.

Currently the new PM applies only to the middle-end optimization pipeline working with LLVM IR. The backend codegen pipeline still works only with the legacy PM, mostly because most codegen passes don't work on LLVM IR, but rather machine IR (MIR), and nobody has yet put in the time to create the new PM infrastructure for MIR passes and to migrate all of the backends to use the new PM. Migrating to the new PM for the codegen pipeline likely won't unlock performance gains since there are almost no interprocedural codegen passes. However, it would clean up a lot of technical debt.
Design
With the legacy PM, each pass declares which analyses it requires and preserves, and the pass manager schedules those analyses as passes to be run if they aren't currently cached or have been invalidated. Declaring ahead of time which analyses a pass may need is unnecessary boilerplate, and a pass might not end up using all analyses in all cases. 

The new PM takes a different approach of completely separating analyses and normal passes. Rather than having the pass manager take care of analyses, a separate analysis manager is in charge of computing, caching, and invalidating analyses. Passes can simply request an analysis from the analysis manager, allowing for lazily computing analyses. In order for a pass to communicate that analyses have been invalidated, it returns which analyses it has preserved. The pass manager tells the analysis manager to handle invalidated cached analyses. This results in less boilerplate and better separation of concerns between passes and analyses.

Since the legacy PM modelled analyses as passes to be scheduled and run, we can't efficiently access analyses to arbitrary functions. For a function analysis, the corresponding analysis pass will only contain the info for the current function, which is created during the latest run of the analysis pass. We can manually create analyses for other functions, but they won't be cached anywhere, leading to lots of redundant work and unacceptable compile time regressions. Since analyses are handled by an analysis manager in the new PM, the analysis manager can cache arbitrary analyses for arbitrary functions.

To support CGSCC analyses, we need a key to cache analyses. For things like functions and loops, we have persistent data structures for those to use as keys. However, the legacy CGSCC pass manager only stored the functions in the current SCC in memory and did not have a persistent call graph data structure to use as keys to cache analyses. So we need to keep the whole graph in memory to have something to use as a key. And if we have a persistent call graph, we need to make sure it is up to date if passes change its structure. To avoid too much redundant work regenerating a potentially large but sparse graph, we need to incrementally update the graph. This is the reason behind the complexity of the CGSCC pass manager in the new PM.

Within an SCC, a transform might break a call graph cycle and split the SCC. One issue with the legacy CGSCC infrastructure is that it simply stores all the functions in the current SCC in an array, then iterates through the functions in that order without ever revisiting functions. Consider the following SCC containing two functions.

```
void foo() {
  bar();
}
void bar() {
  if (false) {
    foo();
  }
}
```

Say we first visit foo, then visit bar and remove the dead call.

```
void foo() {
  bar();
}
void bar() {}
```

We now want to revisit foo since we have better information, most notably that foo is in its own SCC. The legacy CGSCC pass manager would simply move on to the next part of the call graph. So as part of the new PM's incremental call graph update, if an SCC is split, we make sure to visit the newly split SCCs bottom-up. This may involve revisiting a function we have already visited, but that is intentional as to give passes a chance to observe more precise information.

When adding passes to the legacy pass manager, the nesting of different pass types is implicit. For example, adding function passes after a module pass implicitly creates a function pass manager over a contiguous list of function passes. This is fine in theory, although it can be a little confusing. And some pipelines want to run a CGSCC pass independently of a function pass that comes right after, rather than nesting the function pass into the CGSCC pass via a CGSCC pass manager. The new PM makes the nesting more explicit by only allowing pass managers to contain passes of the equivalent type. For example, a function pass manager can only contain function passes. To add a loop pass to a function pass manager, the loop pass must be wrapped in a loop-to-function adaptor to turn it into a function pass. The IR nesting in the new PM is module (-> CGSCC) -> function -> loop, where the CGSCC nesting is optional. Requiring the CGSCC nesting was considered to simplify things, but the extra runtime overhead of building the call graph and the extra code for proper nesting to run function passes was enough to make the CGSCC nesting optional.

The legacy pass manager relies on many global flags and registries. This is supported by macros generating functions and variables to initialize passes, and any users of the legacy pass manager must make sure to call a function to initialize these passes. But we need some way for a pass manager builder to be aware of all passes for testing purposes. The way the new PM does this is by having the pass manager builder include the definitions of all passes, then use a large mapping of pass IDs to pass constructors to create a function that parses a textual description of a pipeline and adds passes. Users of a pass manager builder can add plugins that register parsing callbacks to handle custom out-of-tree passes. Although there is a global list of functions, there is no mutable global state since each pass manager builder can parse pass pipelines without going through a global registry. Other options, like debugging the execution of a pass manager, are also specified via the constructor, and not through a global flag.

There has been a desire to parallelize LLVM passes for a long time. Although the pass manager infrastructure is not the only blocker, the legacy PM did have a couple of issues blocking parallelization.
At the call graph level, only sibling SCCs can be parallelized. Creating SCCs on demand makes it hard to find sibling SCCs. The new PM's computation of the entire call graph makes it easy to find sibling SCCs to parallelize SCC passes on.
Module analyses can be computed from function passes in the legacy PM. Some passes only use analyses if they are cached, so parallelization can cause non-determinism since a module analysis may or may not exist based on other parallel pipelines. The new PM only allows function passes to access cached module analyses and does not allow running them. This has the downside of needing to make sure that certain higher-level analyses are present before running a lower-level pipeline, e.g. making sure GlobalsAA has been computed before running a function pipeline.

## Making the new pass manager the default pass manager

Some major users of LLVM switched to using the new PM by default many years ago. There were some efforts upstream to make the new PM work for all use cases. For example, all Clang tests had been passing with the new PM for a while. However, a vast majority of LLVM tests were still only testing the legacy PM. `opt`, the program typically used to test passes, had syntax to run passes using the legacy PM, `opt -instcombine`, and syntax to run passes using the new PM, `opt -passes=instcombine`. The vast majority of tests used the legacy PM syntax, so if the new PM were to be switched on by default, most LLVM tests wouldn't be testing the new PM passes. (a good number of tests already manually ran against both)

To make tests using `opt` run against the new PM, we can either manually make them run twice, once against the legacy PM and once against the new PM, or we can automatically translate `opt -instcombine` to `opt -passes=instcombine` when the new PM is on by default. Rather than update every test, an `-enable-new-pm` option was added to `opt`, which translates the legacy syntax to the new syntax.

With this new option, we started discovering what features the legacy PM had that existing users of the new PM weren't concerned with. Turning this on locally of course initially caused many tests to fail. Many passes hadn't yet been ported to the new PM and some `opt` features didn't work with the new PM. We ported passes and features that made sense to port to the new PM, and pinned tests using legacy PM features that didn't make sense to port to the new PM.

Some of the more interesting issues with the new PM uncovered with `-enable-new-pm`:

* The `optnone` function attribute didn't cause optional passes to be skipped. Using the existing pass instrumentation framework, which calls callbacks before and after running a pass, and also allows passes to be skipped, this was a very simple pass instrumentation. However, some passes must be run to preserve correctness, so we ended up marking some passes as required.
* [Opt-bisect](https://llvm.org/docs/OptBisect.html) wasn't supported in the new PM. It is used for bisecting which pass in a pipeline causes a miscompile by skipping passes after a certain point. This similarly was fairly easily implemented via pass instrumentation. This similarly always runs required passes.
* Various target-specific tests were failing. Upon inspection, some passes that were expected to be run in something like the -O2 pipeline weren't being run. Some backend targets add custom passes into the default pipelines. Some of these passes are required for correctness, such as passes to lower target-specific intrinsics. The legacy PM had a way for a `TargetMachine` to inject passes into default pipelines via `TargetMachine::adjustPassManager()`. A new PM equivalent was introduced and the target-specific passes in the optimization pipeline were ported to the new PM. This wasn't previously an issue because existing users of the new PM were mostly concerned with x86, which didn't use this feature in the legacy PM.
* Some coroutine tests were asserting in the CGSCC infrastructure. It turns out that the new PM CGSCC infrastructure didn't support extracting parts of a function into another (aka outlining) in a CGSCC pass. There were some initial failed attempts at hacks to work around this issue, which didn't properly update the call graph and didn't handle recursion from newly outlined functions. Finally we came up with a solution that fit into the existing CGSCC infrastructure and properly kept the call graph valid, although coroutine-specific call graph transformations had to be accomodated.

## Improvements

Various projects/companies have already been using the new PM for performance reasons for many years. Separately, Chrome recently started using PGO and ThinLTO to make Chrome faster, each with noticeable performance wins. After the new PM was turned on by default in LLVM, Chrome followed suit and turned on the new PM, seeing 3-4% improvements in Speedometer 2.0 for Linux and Windows, on top of a 8-9MB size decrease. It's likely that better usage of profile information as well as better handling of larger ThinLTO call graphs lead to these improvements.

However, smaller applications with tiny hotspots likely won't see much benefit from the new PM since the improvements brought on by the new PM tend to be more relevant to large codebases.

Aside from user-facing improvements, this also helps LLVM's code health by standardizing on one of the two pass managers for the optimization. While we can't yet remove the legacy pass manager, we can start the deprecation of it, at least for the optimization pipeline. Then hopefully at some point we can start to remove parts of the optimization pipeline that are legacy PM-specific.

## What's next?

To begin the process of removing the use of the legacy PM in the optimization pipeline, we need to make sure that anything using the legacy PM has an alternative using the new PM. Just to list a couple: bugpoint, the LLVM C API, GPU divergence analysis.

As mentioned before, the codegen pipeline still only works with the legacy PM. Although there has been work to start making the codegen pipeline work with the new PM, it is still very far from being usable. This is a great entry point into LLVM, please ask on llvm-dev for more information if you're interested.

