---
author: "Thomas Fransham"
date: "2024-11-23"
tags: ["GSoC", "llvm.org"]
title: "GSoC 2024: Adding LLVM and Clang plugin support for windows"
---

Hello everyone! My name is Thomas and for GSOC I’ve been working on adding plugin support for LLVM and Clang to windows, which mainly involved implementing proper support for building LLVM and Clang as shared libraries (known as DLLs on Windows, dylibs on macOS, or DSOs/SOs on most other Unices)  on Windows.

## Background
The LLVM CMake buildsystem had some existing support for building LLVM as a shared library on Windows, but it suffers from two limitations when trying to make code symbol visibility for DLLs work like Linux.
Most of the environments that LLVM works on use ELF as the object and executable file format. Windows, however, uses PE as its executable file format and COFF as its object file format. This difference is important to highlight as it impacts how dynamic libraries operate in these environments. The ELF (and MachO) based targets implicitly export symbols across the module boundary, but they can be explicitly controlled via the GNU attribute applied to the symbol: `__attribute__((__visibility__(“...”)))`. For PE/COFF environments, two different attributes are required. Symbols meant to be exposed to other modules are decorated with `__declspec(dllexport)` and symbols which are imported from other modules are decorated with `__declspec(dllimport)`. Additionally, the PE format maintains a list of symbols which are public and assigns them a numerical identity known as the ordinal. This is represented in the file format as a 16-bit field, limiting the number of exported symbols to 64K.

In order to support DLL builds on MinGW, a [python script](https://github.com/llvm/llvm-project/blob/main/llvm/utils/extract_symbols.py) would scan the object files generated during the build and extract the symbol names from them. In order to remain under the 64K limit, the symbols would be filtered by pattern matching. The final set would then be used to create an import library that the consumer could use. This technique not only potentially over-exported symbols, introduced a secondary source of truth for the code, but also relied on the linker to generate fix up thunks as the compiler could not determine if a symbol originated from a shared library or static library. This would add additional overhead on a function call that went through this import library as it would no longer be a simple indirect call. Such a thunk was also not possible for data symbols such as static fields of classes except for MinGW which uses a custom [runtime fixup](https://lists.llvm.org/pipermail/llvm-dev/2021-June/150866.html).

## What We Did
Some initial work I did was update the LLVM CMake build system to be able to build a LLVM DLL and use clang-cl's [/Zc:dllexportInlines-](https://blog.llvm.org/2018/11/30-faster-windows-builds-with-clang-cl_14.html). Inline declared class methods by default are not compiled unless used, but when the `__declspec(dllexport)` attribute is applied to a class all its methods are compiled and exported by the compiler even if not used. This option negates this behaviour, preventing inline methods from being compiled and exported. This avoids emitting these methods in every translation unit including the declaration, greatly reducing compile times for DLL builds. More importantly, it almost halves the number of symbols exported for LLVM to 28k and Clang DLL to 20k. The cost of this improvement is that DLLs built with this option cannot be consumed by code being built with MSVC as that code expects these methods to be available externally. There is a Microsoft Developer Community [issue](https://developercommunity.visualstudio.com/t/implement-zcdllexportinlines-aka-fvisibility-inlin/374754) to add it to MSVC, please consider voting for it so that it may be considered by Microsoft for addition to the MSVC toolchain.

Another major thing I worked on was extending a Clang tooling based tool [ids](https://github.com/compnerd/ids) that Saleem Abdulrasool created to automate adding symbol visibility macros to classes, global functions and variables in public LLVM and Clang headers. I made its file processing multi-threaded and added a config file system to make it simple to add exclusions for different headers and directories when adding macro annotations. I also changed it to automatically add a include the header that defines the visibility macros when code is annotated by macros in a file.

I managed to get plugins for Clang and LLVM working including passing the LLVM and Clang test suite when building with clang-cl. Some of the changes to support this have already merged LLVM and Clang or are waiting in open PRs, but it will take some time to get all the changes merged across the whole LLVM and Clang codebase.
The greatly reduced install size from using a non statically linked build of LLVM tools and Clang could also help with [current limitation](https://github.com/llvm/llvm-project/issues/101994) of the installer used for the official distribution on windows that forced the number of targets included in the official distribution to be limited. It would shrink from over 2GB to close to 500MB.

## Future Work
Some of the next steps after all the symbol visibility changes have been merged in to LLVM and Clang would be to use the `ids` tool annotate new classes and functions added to either be integrated in to the PR LLVM pre-submit action to generate symbol visibility macros for new classes and functions added or alternative something like gn syncbot that runs as an after commit action along.
A build bot will also later need to be set up to make sure the windows shared library builds continue to build and function correctly.
Clang still has some weak areas when it comes to tracking the source location for some code constructs in its AST like explicit function template instantiation that `ids` could benefit from being fixed.

## Acknowledgements
I'd like to thank Tom Stellards for doing a lot of the initial work I reused and built on top of. My mentors Saleem Abdulrasool and Vassil Vassilev.
## Links
[Github issue for current progress adding plugin and LLVM_BUILD_LLVM_DYLIB support for Windows](https://github.com/llvm/llvm-project/issues/109483)
Previous discussion of [Supporting LLVM_BUILD_LLVM_DYLIB on Windows](https://discourse.llvm.org/t/supporting-llvm-build-llvm-dylib-on-windows/58891) 



