---
author: "Abdullah Amin"
date: "2025-08-31"
tags: ["GSoC", "LLDB"]
title: "GSoC 2025: Rich Disassembler for LLDB"
---

Hello! I’m Abdullah Amin, and this summer I had the exciting opportunity to participate in Google Summer of Code (GSoC) 2025 with the LLVM Compiler Infrastructure project. My project focused on extending LLDB with a **Rich Disassembler** that annotates machine instructions with source-level variable information.  

**Mentors:** Adrian Prantl and Jonas Devlieghere  

---

## Project Background

LLDB is LLVM’s debugger, capable of source-level debugging across multiple platforms and architectures. While LLDB’s disassembler shows machine instructions, it doesn’t provide much insight into how variables from the original program map onto registers or memory.  

The goal of this project was to use DWARF debug information to enhance LLDB’s disassembly with **variable lifetime and location annotations**. This allows developers to better understand what each register contains at a given point in time, and how variables flow across instructions.  

For example, instead of just seeing register usage:

```bash
0x100000f80 <+32>: movq (%rbx,%r15,8), %rdi
```

…the rich disassembler can add context:

```bash
0x100000f80 <+32>: movq (%rbx,%r15,8), %rdi ; i = r15
```


This makes it much easier to reason about code, especially in optimized builds.

---

## What We Accomplished

Over the summer, I implemented a prototype that integrates DWARF variable location ranges into LLDB’s disassembly pipeline. The key accomplishments are:

- **DWARFExpressionEntry API**  
  Added a new helper (`GetExpressionEntryAtAddress`) to expose variable location ranges from DWARF debug info.  
  *PR: [#144238](https://github.com/llvm/llvm-project/pull/144238)*

- **Register-Resident Variable Annotations**  
  Updated the disassembler to annotate instructions when variables enter, change, or leave registers.  
  *PR: [#147460](https://github.com/llvm/llvm-project/pull/147460)*

- **Stateful Variable Tracking**  
  Extended `Disassembler::PrintInstructions()` to track live variable states across instructions, emitting transitions such as:  
  - `var = RDI` when a variable becomes live in a register  
  - `var = <undef>` when a variable goes out of scope  
  *PR: [#152887](https://github.com/llvm/llvm-project/pull/152887)*

- **Portable Tests**  
  Added new LLDB API tests under `lldb/test/API/functionalities/disassembler-variables/`. These use stable `.s` files (with original C seeds as comments) to generate `.o` files for disassembly. This ensures reliable, portable tests independent of compiler optimizations.  
  *PR: [#152887](https://github.com/llvm/llvm-project/pull/152887) [#155942](https://github.com/llvm/llvm-project/pull/155942) [#156026](https://github.com/llvm/llvm-project/pull/156026)*

Example test coverage includes:
- Function parameters passed in registers (integer, floating point, and mixed).  
- Variables live across function calls.  
- Loop-based register rotation.  
- Constants and undefined ranges.  

---
## How to use it

The annotations are available from LLDB’s disassembler. Enable them with:
```bash 
(lldb) disassemble --variable-annotations``` or ```(lldb) disassemble -v
```

You can also target a specific symbol:
```bash
(lldb) disassemble -n loop_reg_rotate --variable-annotations
``` 
or 
```bash
(lldb) disassemble -n loop_reg_rotate -v
```

### Example

C seed (kept minimal but forces interesting register reshuffles):

```c
__attribute__((noinline))
int loop_reg_rotate(int n, int seed) {
  volatile int acc = seed;    // keep as a named local
  int i = 0, j = 1, k = 2;    // extra pressure but not enough to spill

  for (int t = 0; t < n; ++t) {
    // Mix uses so the allocator may reshuffle regs for 'acc'
    acc = acc + i;
    asm volatile("" :: "r"(acc));      // pin 'acc' live here
    acc = acc ^ j;
    asm volatile("" :: "r"(acc));      // and here
    acc = acc + k;
    i ^= acc; j += acc; k ^= j;
  }

  asm volatile("" :: "r"(acc));
  return acc + i + j + k;
}
```

Disassembly with variable annotations (excerpt):
```bash
loop_reg_rotate.o`loop_reg_rotate:
0x0  <+0>:   pushq  %rbp                                        ; n = RDI, seed = RSI
0x1  <+1>:   movq   %rsp, %rbp
0x4  <+4>:   movl   %esi, -0x4(%rbp)
0x7  <+7>:   testl  %edi, %edi                                  ; j = 1, k = 2, t = 0, i = 0
0x9  <+9>:   jle    0x3d                      ; <+61> at loop_reg_rotate.c
0xb  <+11>:  xorl   %eax, %eax
0xd  <+13>:  movl   $0x1, %edx
0x12 <+18>:  movl   $0x2, %ecx
0x17 <+23>:  nopw   (%rax,%rax)                                 ; j = RDX, k = RCX, i = RAX, t = <undef>
0x20 <+32>:  addl   %eax, -0x4(%rbp)
0x23 <+35>:  movl   -0x4(%rbp), %esi
0x26 <+38>:  xorl   %edx, -0x4(%rbp)
0x29 <+41>:  movl   -0x4(%rbp), %esi
0x2c <+44>:  addl   %ecx, -0x4(%rbp)
0x2f <+47>:  xorl   -0x4(%rbp), %eax
0x32 <+50>:  addl   -0x4(%rbp), %edx
0x35 <+53>:  xorl   %edx, %ecx
0x37 <+55>:  decl   %edi
0x39 <+57>:  jne    0x20                      ; <+32> at loop_reg_rotate.c:8:9
0x3d <+61>:  movl   $0x2, %ecx                                    ; j = 1, k = 2, i = 0
0x42 <+66>:  movl   $0x1, %edx
0x47 <+71>:  xorl   %eax, %eax
0x49 <+73>:  movl   -0x4(%rbp), %esi                              ; j = <undef>, k = <undef>, i = <undef>
0x4c <+76>:  addl   %edx, %eax
0x4e <+78>:  addl   %ecx, %eax
0x50 <+80>:  addl   -0x4(%rbp), %eax
0x53 <+83>:  popq   %rbp
0x54 <+84>:  retq

```

In this example:

* Function params are annotated at entry (n = RDI, seed = RSI).
* Local temporaries (i, j, k) become live in specific registers and later go <undef> when they leave scope or change location.
* Only transitions are printed (start/change/end), keeping the output concise.

---
  
## Current State

- **Working prototype complete:**  
  Rich disassembly annotations are now functional for variables that reside fully in registers or constants.

- **Tested and validated:**  
  A comprehensive set of tests confirm correctness across multiple scenarios, including register rotation, constants, and live-across-call variables.

- **Upstreamed into LLVM:**  
  The core implementation, supporting infrastructure, and final refactoring/formatting changes have all been merged into the main LLVM repository. This means the feature is available in the latest development builds of LLDB.
 
---

## What’s Left to Do

One original goal of the project was to expose the rich disassembly annotations as **structured data** through LLDB’s scripting API, so that tooling can build on top of it.  

While the textual annotations and stateful tracking are complete, this structured API exposure remains future work. I plan to continue working on this beyond GSoC as a follow-up contribution.  

---

## Challenges and Learnings

- **DWARF complexity:** Navigating DWARF location expressions and ranges was challenging, but I gained a deep understanding of how debuggers map source variables to registers and memory.  
- **Testing portability:** Early attempts at hand-writing DWARF with `yaml2obj` proved too fragile. Switching to compiler-generated `.s` files provided stable and portable tests.  
- **Collaboration:** Working with my mentors taught me the value of incremental, reviewable patches and iterative design.  

---

## Conclusion

LLDB’s disassembler is a feature aimed at advanced programmers who need detailed insights into optimized machine code. With the new variable annotations, it becomes easier to understand how source-level variables map to registers and how their lifetimes evolve, bridging the gap between source code and raw assembly.  

Future work will focus on structured API exposure, enabling new tooling to build on these annotations.  

I am grateful to my mentors, **Adrian Prantl** and **Jonas Devlieghere**, for their guidance and support throughout the project, and to the LLVM community for reviewing and testing my work.  

---

## Related Links

- [PR #144238: Add DWARFExpressionEntry API](https://github.com/llvm/llvm-project/pull/144238)  
- [PR #147460: Annotate disassembly with register-resident variables](https://github.com/llvm/llvm-project/pull/147460)  
- [PR #152887: Stateful variable-location annotations](https://github.com/llvm/llvm-project/pull/152887)  
- [PR #155942: Fix workflow testing issues (part 1)](https://github.com/llvm/llvm-project/pull/155942)  
- [PR #156026: Fix workflow testing issues (part 2)](https://github.com/llvm/llvm-project/pull/156026)  
- [PR #156118: Final code formatting and refactoring](https://github.com/llvm/llvm-project/pull/156118)  
- [LLVM Repository](https://github.com/llvm/llvm-project)  
- [My GitHub Profile](https://github.com/UltimateForce21)  
  
