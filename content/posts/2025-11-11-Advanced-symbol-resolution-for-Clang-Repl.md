---
author: "Sahil Patidar"
date: "2025-11-11"
tags: ["GSoC", "clang-repl", "orc-jit"]
title: "GSoC 2025: Advanced symbol resolution for Clang-Repl"
---

## Introduction

Hello! I’m **Sahil Patidar**, and this summer I had the opportunity to participate in **Google Summer of Code (GSoC) 2025** with the **LLVM organization**.
My project focused on enhancing **ORC-JIT** and **Clang-Repl** by introducing a new feature for **advanced symbol resolution**, aimed at improving runtime symbol handling and flexibility.

**Mentors**: Vassil Vassilev, Aaron Jomy

## Overview of the Project

[Clang-Repl](https://clang.llvm.org/docs/ClangRepl.html) is an interactive C++ interpreter built on top of LLVM’s **ORC JIT**, enabling incremental compilation and execution.
However, when user code references symbols from external libraries, those libraries must currently be loaded manually. This happens because **ORC JIT** does not automatically resolve symbols from libraries that haven’t been loaded yet.

To overcome this limitation, my project introduces an **automatic library resolver** for unresolved symbols in **ORC JIT**, improving Clang-Repl’s runtime by making external symbol handling seamless and user-friendly.

## Project Goals

The main goal of my project was to design and implement a new **Library-Resolution API** for **ORC-JIT**.
This API acts as a **smart symbol resolver** — when ORC-JIT encounters an unresolved symbol, it can call this API to find **where** the symbol exists and **which library** provides it.

The next step is to **integrate this API into ORC-JIT**, so that **Clang-Repl** can automatically use it to handle missing symbols without requiring manual library loading.

## Library-Resolution: Smarter Symbol Resolver

During my GSoC project, one of the major components I worked on was **Library-Resolution** — an API we re-designed and re-implemented based on Cling’s original *library-resolver*.

In simple terms, **Library-Resolution** acts as a **smart library resolver**.
It doesn’t actually *load* libraries — instead, it finds **where** the missing symbols (unresolved references) can be found and provides their correct library paths.

This makes it a powerful helper when dealing with unresolved symbols during execution.


### How It Works

When system (Orc-JIT or any) encounters an **unresolved symbol**, system can call the resolver to find symbols that not found.
It scans through user-provided library paths, checks potential matches, and identifies the libraries that contain the missing symbols — all without directly loading them.

At the heart of this system is the **LibraryResolver**, which runs the resolution process by:

1. Scanning available libraries.
2. Filtering symbols efficiently using Bloom filters.
3. Matching unresolved symbols through a **SymbolQuery** tracker.

The result: symbols are mapped to their correct library paths, and system can continue execution seamlessly.


### Core Components Overview

Here’s a quick breakdown of the key components that make Library-Resolution work:

#### 1. LibraryResolver

The main coordinator that controls the entire flow — from scanning libraries to managing symbol lookups.
It ensures that unresolved symbols are systematically matched to libraries.

#### 2. LibraryScanner

Handles the actual scanning of directories and library paths.
It detects valid shared libraries and registers them with the **LibraryManager**.

* **LibraryScanHelper** → Keeps track of directories that need to be scanned.
* **LibrarySearchPath** → Represents a directory and its type (User/System) along with its current scan state.
* **PathResolver** → Normalizes and resolves file paths efficiently.
* **LibraryPathCache** → Stores already-resolved paths and symbolic links to prevent repeated filesystem checks.

#### 3. LibraryManager

Maintains metadata about all discovered libraries.
Each library is represented by a **LibraryInfo** object containing:

* Library path
* Load status (loaded or not)
* A **Bloom filter** for fast symbol existence checks


### Symbol Resolution Flow

Here’s how everything fits together in the resolution process:

1. **Start** → The process begins with a list of unresolved symbols, passed to the `LibraryResolver` to create a `SymbolQuery`.
2. **Scan** → The resolver scans user and system library paths to find new or unregistered libraries.
3. **Cache & Filter** → For each library, if it doesn’t yet have a Bloom filter, one is created to enable faster lookups in future scans.
4. **Match** → Each unresolved symbol is tested against these filters. If a likely match is found and verified, the symbol is linked to that library’s path.
5. **Repeat** → This continues until all symbols are resolved or no more valid libraries remain.
6. **Complete** → The results are returned through a completion callback.

## Summary of accomplished tasks

### ExecutorResolver:
[143654](https://github.com/llvm/llvm-project/pull/143654)
suggested by @lhames.we introduced a `DylibSymbolResolver` that helps resolve symbols for each loaded dylib.

Previously, we returned a DylibHandle to the controller. Now, we wrap the native handle inside `DylibSymbolResolver` and return a `ResolverHandle` instead. This makes the code cleaner and separates the symbol resolution logic from raw handle management.

with this changes this will help us to integrate LibraryResolver API using some future through new `AutoDylibResolver`.

### Library-Resolver API:
[#165360](https://github.com/llvm/llvm-project/pull/165360)

This is the main API we redesigned based on cling auto library-resolver. this api provide way to user to add search-path and ask for symbols to search and provide resolved library for each symbols.

### Additional Improvements

> **Note:** This post describes ongoing design work. The implementation is still under review and may evolve as feedback is incorporated.

During this phase, I worked on **value-printing support** for both **in-process** and **out-of-process** execution in `clang-repl`.
Previously, `clang-repl` only supported value printing in the in-process mode, and the same implementation couldn’t be directly applied to the out-of-process case.

#### Challenges

1. **Dependency on type information**

   The value-printing logic depends on the **type information of the JIT-compiled expression result** to correctly interpret and cast the result into a `Value` object.
   In the **in-process** setup, this is straightforward — process symbols such as `__clang_interpreter*`, which are used to capture the result at runtime along with its type information, reside within the controller. This allows the call to use pointer type information directly to interpret the resulting value correctly.
   However, in the **out-of-process** setup, those symbols exist in the controller, while the actual evaluated result resides in the executor. Even if we set up the runtime call in the executor, we still need to access the type information that remains on the controller side.

2. **No direct executor-to-controller calls**

   In the out-of-process model, we cannot directly invoke controller-side functions from the JIT-compiled code running in the executor. This limitation made the previous mechanism unsuitable.

#### Approach (Initial design — under review)

**Pull Request:** [#156649](https://github.com/llvm/llvm-project/pull/156649)

We **reimplemented the value-printing infrastructure** on top of the **ORC MemoryAccess** API.
The old design supported only in-process evaluation; the new design works in both **in-process** and **out-of-process** environments.

#### How it works

1. **Capturing the JIT Expression Result**

    With the new design, we adopted a different approach:
    we now take the **address of the evaluated JIT expression result** and pass it to a runtime call.

    ```cpp
    EXTERN_C void __clang_Interpreter_SendResultValue(void *Ctx, unsigned long long, void *);
    // In-process: Ctx is a pointer to the ValueResultManager object, 
    // allowing us to call ValueResultManager::deliverResult.

    EXTERN_C void __orc_rt_SendResultValue(unsigned long long, void *);
    // Out-of-process version.
    ```

    At the **AST level**, we perform the necessary transformations to insert this runtime call.
    When adding the runtime “sugar” call to the expression AST, the expression must be an **lvalue**, so we can safely cast it to a `void*` and pass it to the runtime helper.

    If the expression is already an **lvalue**, we simply cast and pass its address directly.
    If it’s an **rvalue**, we modify the AST to convert it into an lvalue, ensuring it can be handled uniformly by the runtime call added to the expression AST.


2. **Tracking results via `ValueResultManager`**

   We introduced a new helper, `ValueResultManager`, which tracks upcoming expression results using a unique **Expr ID → TypeInfo** mapping.
   The expression’s runtime call includes this ID, which is later returned to the controller-side handler when the executor reports back.

   **Executor runtime call example:**

   ```cpp
   RC_RT_JIT_DISPATCH_TAG(__orc_rt_SendResultValue_tag)

   ORC_RT_INTERFACE void __orc_rt_SendResultValue(uint64_t ResultId, void *V) {
     Error OptErr = Error::success();
     if (auto Err = WrapperFunction<SPSError(uint64_t, SPSExecutorAddr)>::call(
             JITDispatch(&__orc_rt_SendResultValue_tag), OptErr, ResultId,
             ExecutorAddr::fromPtr(V))) {
       cantFail(std::move(OptErr));
       cantFail(std::move(Err));
     }
     consumeError(std::move(OptErr));
   }
   ```

   **Controller-side runtime call for in-process:**

   ```cpp
   REPL_EXTERNAL_VISIBILITY void
   __clang_Interpreter_SendResultValue(void *Ctx, uint64_t Id, void *Addr) {
     static_cast<ValueResultManager *>(Ctx)->deliverResult(
         [](llvm::Error Err) { llvm::cantFail(std::move(Err)); }, Id,
         llvm::orc::ExecutorAddr::fromPtr(Addr));
   }
   ```

   This mechanism allows both in-process and out-of-process environments to deliver evaluated result addresses to `ValueResultManager::deliverResult`.

3. **Reading and formatting values**

   On receiving the address, the `ValueResultManager` calls into a `ValueReaderDispatcher`, which uses **MemoryAccess** to read values from memory using the address and type information.
   The evaluated data is then wrapped in a redesigned **`Value` class** and finally converted to a string via a **`ValueToString` printer**.

#### New `Value` class design

The new `Value` implementation is inspired by `APValue` and designed to handle both local and remote execution enviroment.

Key improvements:

* Supports array element store and access by index.
* Works safely in out-of-process mode — the old design, which assumed arrays lived in controller-side memory (invalid in remote execution).
* Provides a unified way to represent, store, and print evaluated results across environments.

### Other PRs
[166510](https://github.com/llvm/llvm-project/pull/166510)
[166147](https://github.com/llvm/llvm-project/pull/166147)


## Result: What the Library-Resolution API Enables

With these updates, we now have a **fully functional Library-Resolution API** that can dynamically **resolve missing symbols to their correct shared libraries** — all **without loading the libraries directly**.

This new system can **discover**, **query**, and **match** unresolved symbols across user and system paths, providing the exact library paths where those symbols exist. It serves as a powerful helper for ORC-JIT and Clang-Repl’s runtime infrastructure.

Here’s a short example showing how the API can be configured and used to trigger the resolution process:

```cpp
llvm::orc::LibraryResolver::Setup S =
    llvm::orc::LibraryResolver::Setup::create({});

// Define a callback that decides whether a library should be scanned
S.ShouldScanCall = [&](llvm::StringRef lib) -> bool { return true; };

// Create the driver that coordinates the resolution
Controller = llvm::orc::LibraryResolutionDriver::create(S);

// Add user and system library paths to be scanned
for (const auto &SP : SearchPaths)
  Controller->addScanPath(SP, llvm::orc::PathType::User);

// Prepare the symbols to be resolved
std::vector<std::string> Sym;
Sym.push_back(MangledName);

// Configure resolution policy
llvm::orc::SearchConfig Config;
Config.Policy = {
  {{llvm::orc::LibraryManager::LibState::Queried,  llvm::orc::PathType::User},
   {llvm::orc::LibraryManager::LibState::Unloaded, llvm::orc::PathType::User},
   {llvm::orc::LibraryManager::LibState::Queried,  llvm::orc::PathType::System},
   {llvm::orc::LibraryManager::LibState::Unloaded, llvm::orc::PathType::System}}};

Config.Options.FilterFlags =
    llvm::orc::SymbolEnumeratorOptions::IgnoreUndefined;

// Run the symbol resolution
Controller->resolveSymbols(
    Sym,
    [&](llvm::orc::LibraryResolver::SymbolQuery &Q) {
      if (auto S = Q.getResolvedLib(MangledName))
        Res = *S;
    },
    Config);
```

## Future Work

The next step will be to **continue development on the ORC-JIT side**, aligned with the ongoing evolution of the **Executor** layer.
Once the new Executor design stabilizes, we’ll **revisit the Library-Resolution API** and make any necessary adjustments for compatibility and cleaner integration.
This work will be done under the **guidance of Lang Hames**, ensuring it fits well within the evolving ORC architecture.

The next phase of this project focuses on **integrating the Library-Resolution API into ORC-JIT**.
Specifically, we plan to:

* **Introduce the AutoDylibResolver** — based on the groundwork implemented in the *ExecutorResolver* pull request.
  This component will allow ORC-JIT to automatically invoke the Library-Resolution API whenever it encounters an unresolved symbol.

* **Enable automatic library loading** in ORC-JIT.
  Once integrated, ORC-JIT (and tools like Clang-Repl) will be able to automatically locate and load the required libraries during runtime — removing the need for users to manually load them.

This next step will complete the feature chain — from symbol detection to automatic library resolution and loading — making Clang-Repl more user-friendly.

## Conclusion

Through this project, we now have a working **Library-Resolver API** implementation in **ORC-JIT**.
Moving forward, the focus will be on **integrating this API into ORC-JIT** to enable **automatic library loading** and further improve **Clang-Repl** and other projects that use ORC-JIT as their execution engine.

Thank you for being part of my **GSoC 2025** journey!

## Acknowledgements

I would like to express my heartfelt thanks to **Google Summer of Code (GSoC)** and **LLVM Org** for the amazing opportunity to work on this project.
A special thanks to my mentor **Vassil Vassilev** for his continuous guidance and support, and to **Lang Hames** for his valuable insights on ORC-JIT and Clang-Repl.

This experience has been truly rewarding, and I’m excited to keep contributing to the **LLVM** and open-source community in the future.

## Related Links

- [LLVM Repository](https://github.com/llvm/llvm-project)
- [Project Description](https://discourse.llvm.org/t/gsoc2025-advanced-symbol-resolution-and-reoptimization-for-clang-repl/84624/3)
- [My GitHub Profile](https://github.com/SahilPatidar)
