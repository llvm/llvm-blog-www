---
author: "QuillPusher (Saqib)"
date: "2023-12-31"
tags: ["clang-repl"]
title: "Another step forward towards interactive programming"
---

The [Compiler Research] team is pleased to announce the successful completion
of another round of internships focused on enhancements in interactive
programming, specifically in relation to the [Clang-REPL] component in LLVM. 

The Compiler Research team includes researchers located at Princeton University
and CERN. Our primary goal is best described as follows: 

>To establish a proficient workflow in LLVM, where [interactive development] in
>C++ is possible, and exploratory C++ becomes an accessible experience to a
>wider audience.

Following are some notable contributions by our interns this year.

## Yuquan Fu - Autocompletion in Clang-REPL

Clang-Repl allows developers to program in C++ interactively with a REPL
environment. However, it was missing the ability to suggest code completion or
auto-complete options for user input, which can be time-consuming and prone to
typing errors.

With this code completion system, users can either complete their input quickly
or see a list of valid completion candidates. The code completion is also
context-aware, providing semantically relevant results based on the current
position and input on the current line.

Mentors: Vassil Vassilev ([Princeton.edu]) & David Lange ([Princeton.edu])

Project Details: [Autocompletion in Clang-REPL]

Funding: Google Summer of Code 2023

### Example – avoiding tedious typing

```
clang-repl> struct WhateverMeaningfulLoooooooooongName{ int field;};
clang-repl> Wh<tab>
```

With code completion, hitting tab completes the entity name:

```
clang-repl>  WhateverMeaningfulLoooooooooongName
```

> For implementation details, please see the respective [slides] and the
> [blog].


## Anubhab Ghosh - WebAssembly Support for Clang-Repl

The Xeus Framework enables accessing Clang-REPL (an interpreter that JIT
compiles C++ code into native code) in a web browser, using Jupyter. However,
this shifts the computational load to the server.

A more scalable approach is to use WebAssembly. It allows sandboxed execution
of native (e.g. C/C++/Rust) programs compiled to an intermediate bytecode at
closer to native speeds. The idea is to run clang-repl within WebAssembly and
generate JIT-compiled WebAssembly code and execute it on the client side.

However, this comes with some challenges (e.g., code in WebAssembly is
immutable, which is unacceptable for JIT).

Solution: To address the code immutability issue, a new WebAssembly module is
created at each iteration of the REPL loop. Initially, a precompiled module
containing the Standard C/C++ libraries, LLVM, Clang, and wasm-ld is sent to
the browser, which runs the interpreter and compiles the user code.

Since we cannot call Interpreter::Execute() to execute the module (due to
JITLink reliance), the LLVM WebAssembly backend is used manually to produce an
object file. This file is then passed to the WebAssembly version of LLD
(wasm-ld) to turn it into a shared library which is written to the virtual file
system of Emscripten. The dynamic linking facilities of Emscripten can be used
to load this library.

Mentors: Vassil Vassilev ([Princeton.edu]) & Alexander Penev ([Uni-Plovdiv.bg])

Project Details: [WebAssembly Support for Clang-Repl]

Funding: Google Summer of Code 2023

### Example:

```
SDL_Init(SDL_INIT_VIDEO);
SDL_Window *window;
SDL_Rendered *renderer;
SDL_CreateWindowAndRenderer (300, 300, 0, &window, &renderer);
```

This should connect to a simple black canvas. Next, we can draw things into it.

```
SDL_SetRenderDrawColor(renderer, 0x80, 0x00, 0x00, 0xFF);
SDL_Rect rect3 = {.x = 20, .y = 20, .w = 150, .h = 100};
SDL_RenderFillRect(rendered, &rect3);
 
SDL_SetRenderDrawColor(renderer, 0x00, 0x80, 0x00, 0xFF);
SDL_Rect rect4 = {.x = 40, .y = 40, .w = 150, .h = 100};
SDL_RenderFillRect(rendered, &rect4);
 
SDL_RenderPresent(renderer);
```

The output should look something like this:

{{< figure src="/img/WebAssemblyExample.png" alt="Web Assembly Example" >}}


## Sunho Kim -  Re-optimization using JITLink

In order to support re-optimization, the JITLink API was extended by adding the
cross-architecture stub creation API. This API works in all platforms and
architectures that JITLink supports and through this we can create the
redirectable stubs by using JITLink.

Once the re-optimization API was developed, it was time to actually implement
re-optimization. A new layer was introduced to support re-optimization of IR
modules. There were many abstraction levels where redirection could be
implemented, but we ended up doing it at IR level since that brings a lot of
re-optimization techniques to be implemented easily by transforming IR
directly. From an API perspective, the most flexible abstraction level to do
this may be at the FrontEnd AST level.

Clang-Repl relies on LLJIT to do JIT-related tasks. Enabling re-optimization
for LLJIT also helped enable it in Clang-Repl. However, there were minor
challenges (e.g., mismatch in what clang-repl expects from how the runtime
executes the static initializers and how ELF orc runtime runs it). Possible
solutions for these are in discussion (e.g., adding a new dl function).
Nevertheless, we now have a real-world experimental environment where we can
test new re-optimization techniques and perform benchmarks to see if they are
useful.

Finally, based on the above infrastructure, profile guided optimization is now
possible (by transforming the IR module). There are still some enhancements
pending before the code is fully upstreamed, but the current code achieves
instrumentation on the orc-runtime side, which simplifies implementation by a
lot. 

Mentors: Vassil Vassilev ([Princeton.edu]) & Lang Hames/ lhames ([Apple])

Project Details: [Re-optimization using JITLink]

Funding: Google Summer of Code 2023

### Example: Doing the -O2 optimization if function was called more than 10 times

The following example builds a PassManager using the LLVM library and then runs
the optimization pipeline.

```
static Error reoptimizeTo02(ReOptimizeLayer &Parent, ReOptMaterializationUnitID MUID,
    unsigned Curverison, ResourceTrackerSP OldRT, ThreadSafeModule &TSM) {
      TSM.withModuleDo([&]{llvm::Module &M) {
         auto PassManager = buildPassManager();
         PassManager.run(M);
        });
        return Error::success();
}
ReOptLayer ->setReoptimizeFunc(reoptimizeTo02);
ReOptLayer ->setAddProfileFunc(reoptimizeIfCallFrequent);
```

For more examples, please see the [LLVM-JITLink-COFF-Example] repo.


## Krishna Narayanan - Tutorial development with clang-repl

Open Source documentation is often a neglected area in the software lifecycle.
Specifically, this project targeted helping contributors by documenting how
they can set up respective environments on their local machines to contribute
to the code and documentation of the respective project. These environments
were set up locally, tested and then the setup methodology was updated in the
relevant documentation. 

Besides other compiler research technologies, write-ups were also added to LLVM
(specifically the Clang-Repl documentation) as part of this project. Usage
examples were also added.

Mentors: Vassil Vassilev ([Princeton.edu]) & David Lange ([Princeton.edu])

Project Details: [Tutorial development with clang-repl]

Funding: Google Summer of Code 2023

### Example

```
// Classes and Structures
clang-repl> #include <iostream>
clang-repl> class Rectangle {int width, height; public: void set_values (int,int);\
clang-repl... int area() {return width*height;}};
clang-repl>  void Rectangle::set_values (int x, int y) { width = x;height = y;}
clang-repl> int main () { Rectangle rect;rect.set_values (3,4);\
clang-repl... std::cout << "area: " << rect.area() << std::endl;\
clang-repl... return 0;}
clang-repl> main();
area: 12
```



[Compiler Research]: https://compiler-research.org

[Clang-REPL]: https://clang.llvm.org/docs/ClangRepl.html

[interactive development]: https://compiler-research.org/interactive_cpp

[Princeton.edu]: https://www.princeton.edu/

[Uni-Plovdiv.bg]: https://uni-plovdiv.bg/en/

[Apple]: https://www.apple.com

[Autocompletion in Clang-REPL]: https://www.syntaxforge.net/clang-repl-cc/

[WebAssembly Support for Clang-Repl]: https://gist.github.com/argentite/c0852d3e178c4770a429f14291e83475

[Re-optimization using JITLink]: https://gist.github.com/sunho/bbbf7c415ea4e16d37bec5cea8adce5a

[LLVM-JITLink-COFF-Example]: https://github.com/sunho/LLVM-JITLink-COFF-Example

[Tutorial development with clang-repl]: https://github.com/Krishna-13-cyber/GSoC23-LLVM/blob/main/README.md

[slides]: https://compiler-research.org/assets/presentations/CaaS_Weekly_30_08_2023_Fred-Code_Completion_in_ClangRepl_GSoC.pdf

[blog]: https://compiler-research.org/blogs/gsoc23_ffu_experience_blog/

[Google Summer Of Code website]: https://summerofcode.withgoogle.com/archive/2023/organizations/llvm-compiler-infrastructure