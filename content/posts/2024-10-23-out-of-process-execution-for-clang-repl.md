---
author: "Sahil Patidar"
date: "2024-10-27"
tags: ["GSoC", "clang-repl", "orc-jit"]
title: "GSoC 2024: Out-Of-Process Execution For Clang-Repl"
---

Hello! I'm Sahil Patidar, and this summer I had the exciting opportunity to
participate in Google Summer of Code (GSoC) 2024. My project revolved around
enhancing Clang-Repl by introducing Out-Of-Process Execution.

Mentors: Vassil Vassilev and Matheus Izvekov

## Project Background

Clang-Repl, part of the LLVM project, is a powerful interactive C++ interpreter using Just-In-Time (JIT) compilation. However, it faced two major issues: high resource consumption and instability. Running both Clang-Repl and JIT in the same process consumed excessive system resources, and any crash in user code would shut down the entire session.

To address these problems, **Out-Of-Process Execution** was introduced. By executing user code in a separate process, resource usage is reduced and crashes no longer affect the main session. This solution significantly enhances both the efficiency and stability of Clang-Repl, making it more reliable and suitable for a broader range of use cases, especially on resource-constrained systems.

## What We Accomplished

As part of my GSoC project, I've been focused on implementing out-of-process execution in Clang-Repl and enhancing the ORC JIT infrastructure to support this feature. Here is a breakdown of the key tasks and improvements I worked on:

### Out-Of-Process Execution Support for Clang-Repl

**PR**: [#110418](https://github.com/llvm/llvm-project/pull/110418)

One of the primary objectives of my project was to implement **out-of-process (OOP) execution** capabilities within Clang-Repl, enabling it to execute code in a separate, isolated process. This feature leverages **ORC JIT's remote execution capabilities** to enhance code execution flexibility by isolating runtime environments.

To enable OOP execution in Clang-Repl, I utilized the `llvm-jitlink-executor`, allowing Clang-Repl to offload code execution to a dedicated executor process. This setup introduces a layer of isolation between Clang-Repl’s main process and the code execution environment.

- **New Command-Line Flags**:

   To facilitate the out-of-process execution, I added two key command-line flags:

   - **`--oop-executor`**
      This flag starts a separate JIT executor process. The executor handles the actual code execution independently of the main Clang-Repl process.

   - **`--oop-executor-connect`**
      This flag establishes a communication link between Clang-Repl and the out-of-process executor. It allows Clang-Repl to transmit code to the executor and retrieve the results from the execution.

With these flags in place, Clang-Repl can utilize `llvm-jitlink-executor` to execute code in an isolated environment. This approach significantly enhances separation between the compilation and execution stages, increasing flexibility and ensuring a more secure and manageable execution process.

### Issues Encountered

- **Block Dependence Calculation in ObjectLinkingLayer**
   [Commit Link](https://github.com/llvm/llvm-project/commit/896dd322afcc1cf5dc4fa7375dedd55b59001eb4)

   **Code Example**
   ```cpp
   clang-repl> int f() {return 1;}
   clang-repl> int f1() {return f();}
   clang-repl> f1();
   error: disconnecting
   clang-repl> JIT session error: FD-transport disconnected
   JIT session error: disconnecting
   JIT session error: FD-transport disconnected
   JIT session error: Failed to materialize symbols: { (main, { __Z2fv }) }
   disconnecting
   ```

   During my work on `clang-repl`, I encountered an issue where the JIT session would crash during incremental compilation. The root cause was a bug in `ObjectLinkingLayer::computeBlockNonLocalDeps`. The problem arose from the way the worklist was built: it was being populated within the same loop that records immediate dependencies and dependants, which caused some blocks to be missed from the worklist. This bug was fixed by **Lang Hames**.

### ORC JIT Enhancements

As part of the OOP execution work, several improvements were made to ORC JIT, the underlying framework responsible for dynamic compilation and execution of code in Clang-Repl. These improvements target better handling of incremental execution, especially for Mach-O and ELF platforms, and ensuring that initializers are properly managed across different execution environments.

1. **Incremental Initializer Execution for Mach-O and ELF**
   **PRs**: [#97441](https://github.com/llvm/llvm-project/pull/97441), [#110406](https://github.com/llvm/llvm-project/pull/110406)

   In a typical JIT execution environment, the `dlopen` function is used to handle code mapping, reference counting, and initializer execution for dynamically loaded libraries. However, this approach is often too broad for interactive environments like Clang-Repl, where we only need to execute newly introduced initializers rather than reinitializing everything. To address this, I introduced the **`dlupdate`** function in the ORC runtime.

   The `dlupdate` function is a targeted solution that focuses solely on running new initializers added during a REPL session. Unlike `dlopen`, which handles a variety of tasks and can lead to unnecessary overhead, `dlupdate` only triggers the execution of newly registered initializers, avoiding redundant operations. This improvement is particularly beneficial in interactive settings like Clang-Repl, where code is frequently updated in small increments.

   By streamlining the execution of initializers, this change significantly improves the efficiency of Clang-Repl.

2. **Push-Request Model for ELF Initializers**
   **PR**: [#102846](https://github.com/llvm/llvm-project/pull/102846)

   A push-request model has been introduced to manage ELF initializers within the runtime state for each `JITDylib`, similar to how initializers are handled for Mach-O and COFF. Previously, ELF required a fresh request for initializers with each invocation of `dlopen`, but lacked mechanisms to register, deregister, or retain these initializers. This created issues during subsequent `dlopen` calls, as initializers were erased after the `rt_getInitializers` function was invoked, making further executions impossible.

   To resolve these issues, the following functions were introduced:

   - **`__orc_rt_elfnix_register_init_sections`**: Registers ELF initializers for the `JITDylib`.
   - **`__orc_rt_elfnix_register_jitdylib`**: Registers the `JITDylib` with the ELF runtime state.

   With the new push-request model, the management and tracking of initializers for each `JITDylib` state are now more efficient. By leveraging Mach-O’s `RecordSectionsTracker`, only newly registered initializers are executed, greatly improving efficiency and reliability when working with ELF targets in `clang-repl`.

   This update is crucial for enabling out-of-process execution in `clang-repl` on ELF platforms, offering a more effective approach to managing incremental execution.

### Additional Improvements

Beyond the main enhancements to Clang-Repl and ORC JIT, I also worked on several other improvements:

1. **Auto-loading Dynamic Libraries in ORC JIT.**

   **PR**: [#109913](https://github.com/llvm/llvm-project/pull/109913) (On-going)

   With this update, we’ve introduced a new feature to the ORC executor and controller: **automatic loading of dynamic libraries in the ORC JIT**. This enhancement enables efficient resolution of symbols from both loaded and unloaded libraries.

   - How It Works:
      - **Symbol Lookup:**
        When a lookup request is made, the system first attempts to resolve the symbol from already loaded libraries.
      - **Unloaded Libraries Scan:**
        If the symbol is not found in any loaded library, the system then scans the unloaded dynamic libraries to locate it.

   - Key Addition: **Global Bloom Filter**
     A significant improvement in this update is the introduction of a **Global Bloom Filter**. When a symbol cannot be resolved in the loaded libraries, the symbol tables from the scanned libraries are incorporated into this filter. If the symbol is still not found, the bloom filter’s result is returned to the controller, allowing it to skip checking for symbols that do not exist in the global table during future lookups.

   Additionally, the system tracks symbols that were previously thought to be present but are actually absent in both loaded and unloaded libraries. With these enhancements, symbol resolution is significantly faster, as the bloom filter helps prevent unnecessary lookups, thereby improving efficiency for both loaded and unloaded dynamic libraries.

2. **Refactor of `dlupdate` Function**
   **PR**: [#110491](https://github.com/llvm/llvm-project/pull/110491)

   This update simplifies the `dlupdate` function by removing the `mode` argument, streamlining the function's interface. The change enhances the clarity and usability of `dlupdate` by reducing unnecessary parameters, improving the overall maintainability of the code.


## Benchmarks: In-Process vs Out-of-Process Execution

- [Prime Finder](https://gist.github.com/SahilPatidar/4870bf9968b1b0cb3dabcff7281e6135)
- [Fibonacci Sequence](https://gist.github.com/SahilPatidar/2191963e59feb7dfa1314509340f95a1)
- [Matrix Multiplication](https://gist.github.com/SahilPatidar/1df9e219d0f8348bd126f1e01658b3fa)
- [Sorting Algorithms](https://gist.github.com/SahilPatidar/c814634b2f863fc167b8d16b573f88ec)


## Result

With these changes, `clang-repl` now supports out-of-process execution. We can run it using the following command:

```bash
clang-repl --oop-executor=path/to/llvm-jitlink-executor --orc-runtime=path/to/liborc_rt.a
```

## Future Work

- **Crash Recovery and Session Continuation :**
   Investigate and develop ways to enhance crash recovery so that if something goes wrong, the session can seamlessly resume without losing progress. This involves exploring options for an automatic process to restart the executor in the event of a crash.

- **Finalize Auto Library Loading in ORC JIT :**
   Wrap up the feature that automatically loads libraries in ORC JIT. This will streamline symbol resolution for both loaded and unloaded dynamic libraries by ensuring that any required dylibs containing symbol definitions are loaded as needed.


## Conclusion

With this project, **Clang-Repl** now supports **out-of-process execution** for both `ELF` and `Mach-O`, making it much more efficient and stable, especially on devices with limited resources.

In the future, I plan to work on automating library loading and improving ORC-JIT to make Clang-Repl's out-of-process execution even better.

## Acknowledgements

I would like to thank **Google Summer of Code (GSoC)** and the LLVM community for providing me with this amazing opportunity. Special thanks to my mentors, **Vassil Vassilev** and **Matheus Izvekov**, for their continuous support and guidance. I am also deeply grateful to **Lang Hames** for sharing their expertise on ORC-JIT and helping improve `clang-repl`. This experience has been a major step in my development, and I look forward to continuing my contributions to open source.

## Related Links

- [LLVM Repository](https://github.com/llvm/llvm-project)
- [Project Description](https://discourse.llvm.org/t/clang-out-of-process-execution-for-clang-repl/68225)
- [My GitHub Profile](https://github.com/SahilPatidar)