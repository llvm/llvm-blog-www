---
author: "Anthony Tran (anthonyhatran)"
date: "2025-09-01"
tags: ["GSoC", "Clang", "CodeGen"]
title: "GSoC 2025: Usability Improvements for the Undefined Behavior Sanitizer"
---

## Introduction

Hi everyone, my name is Anthony and I had the pleasure of working on improving the Undefined Behavior Sanitizer this Google Summer of Code 2025. My mentors were Dan Liew and Michael Buch.

## Background

Undefined Behavior Sanitizer (UBSan) is an undefined behavior checker which detects some of the undefined behaviors in C, C++, and Objective-C languages at runtime. This project focused mainly on trapping UBSan, which is evoked through `-fsanitize-trap=<...>` along with `-fsanitize=<...>`. Trapping UBSan is a lighter-weight version of runtime UBSan because upon detection of undefined behavior a trap instruction is executed rather than calling into a runtime library to handle the undefined behavior. This makes it more appealing for kernel, embedded, and production hardening use cases.

TODO: Show example of normal userspace UBSan and how it outputs the problem
TODO: show example of trapping UBSan. Both with and without the debugger. This will let you illustrate the problem of it being hard to understand what happened, even in the debugger. This gives you the motivation for your work which is currently missing.

## Human readable descriptions of UBSan traps in LLDB

During my GSoC project I implemented support for displaying human readable descriptions of UBSan traps in LLDB to improve the debugging experience.

The approach used is based on how `__builtin_verbose_trap` is implemented.

This was done by inserting a fake frame into debug-info, which is formatted like so: `__clang_trap_msg$<Category>$<TrapMessage>`. This specific format
is recognized and can be encoded by LLDB, which resembles what is used for `__builtin_verbose_trap`. 

In both cases, the compiler encodes the trap's context by emitting the trap instruction inside an artificial function with a specially-formatted name (e.g., `__clang_trap_msg$<Category>$<Message>`). When the trap occurs, LLDB's Verbose Trap StackFrame Recognizer identifies this special name in the debug info and uses the extracted category and message to generate the user-friendly stop reason. By adopting this existing protocol, this feature ensures robust and consistent behavior within the debugger.

Take for example this erroneous program, `foo.c`:

```
#include <limits.h>

int main() { return INT_MAX + 1; }
```

When run with these flags before my change:
`$ clang -fsanitize=signed-integer-overflow -fsanitize-trap=signed-integer-overflow foo.c`

Debug info would not provide a stopping reason, leaving the user possibly confused. 

After the changes, this line is emitted in LLVM IR:
`!18 = distinct !DISubprogram(name: "__clang_trap_msg$Undefined Behavior Sanitizer$signed integer addition overflow in '2147483647 + 1'", scope: !1, file: !1, type: !19, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: !0)`

Which looks something like this in LLDB: 

`stop reason = Undefined Behavior Sanitizer: signed integer addition overflow in '2147483647 + 1'`

Previously, the program would stop execution, but the user would not know why. With the new feature, the stop reason is apparent. 

The `-fsanitize-debug-trap-reasons` flag [1] enables trap messages for UBSan, which provides context for stop reasons in trapping UBSan.

One concern that a reviewer had was the debug info size difference. Using bloaty, I tested a release build of clang with the `-fsanitize-debug-trap-reasons` flag enabled, and one with it disabled (`-fno-sanitize-debug-trap-reasons`). Results are below.
```
    FILE SIZE        VM SIZE    
 --------------  -------------- 
  +0.3% +6.01Mi  +0.3% +6.01Mi    ,__debug_info
  +2.0% +2.26Mi  [ = ]       0    [Unmapped]
  +1.2% +1.35Mi  +1.2% +1.35Mi    ,__apple_names
  +0.0% +1.01Mi  +0.0% +1.01Mi    ,__debug_str
  +0.8%  +636Ki  +0.8%  +635Ki    ,__debug_line
  +0.4%  +161Ki  +0.4%  +161Ki    ,__debug_ranges
  +0.4% +47.9Ki  +0.4% +47.9Ki    ,__debug_abbrev
  +0.0%     +14  +0.0%     +14    ,__apple_types
  [ = ]       0  +0.0%      +8    ,__common
  [ = ]       0  +7.1%      +4    ,__thread_bss
  -0.0%      -4  -0.0%      -4    ,__const
  -0.0% -1.27Ki  -0.0% -1.27Ki    ,__cstring
  +0.2% +11.5Mi  +0.1% +9.19Mi    TOTAL
  ```


## RFC: Add a warning when `-fsanitize=` is passed without associated `-fsanitize-trap=`

Currently, clang does not warn about cases where `-fsanitize-trap=` does nothing (silent no-op), particularly in the case where `-fsanitize-trap=` is passed without `-fsanitize=` [2]:

Ex:
`$ clang -fsanitize-trap=undefined foo.c`

Emits no warning, even though `-fsanitize-trap=undefined` is not doing anything here.


We thought it would be more user-friendly to add a warning for such cases, but due to some initial community pushback [3], it was decided that an RFC should be opened. I ended up writing a sketch patch that emitted a warning for such cases [4]. 

Ex:
`$ clang -fsanitize-trap=undefined foo.c`

Would now emit:
`warning: -fsanitize-trap=undefined has no effect because the "undefined" sanitizer is disabled; consider passing "-fsanitize=undefined" to enable the sanitizer`

Unfortunately, we found that the emission of such warnings could become exceedingly complicated and a point of contention due to the the existence of sanitizer groups, subgroups, and individual sanitizers. Determining the correct behavior for various cases, historical precedence with no-ops, interference with current build systems, prioritization of existing build systems over the user experience, and compatibility with gcc led to the end of the RFC [5].

## Expand upon the hard-coded strings in `-fsanitize-debug-trap-reasons` to be more specific

One of my mentors, Dan, created an extension to the clang diagnostics subsystem to work with trap messages [6]. By extending the diagnostics subsystem, it allows us to leverage the powerful semantics that the diagnostics system offers. We still use what was implemented in the first task as sort of a fallback, meaning that if no additional context was needed for the trap message, then the default hard-coded string will be used instead.

## What I've Learned

Before I started this GSoC, I barely even knew how to build clang and LLVM or use git in a large open-source project. My mentors showed me the ropes on a lot of things, and I came out of this summer knowing a lot more of how to get my changes properly reviewed and upstreamed.

## Future Work

The diagnostics extension for trap messages has been recently upstreamed by Dan [6]. As of right now, only signed and unsigned overflow for addition, subtraction, and multiplication is being used by this system. I've investigated some use cases outside of signed and unsigned overflow, and I plan to implement that within the next week(s). [stub, since I'll probably upstream my changes soon so this part will change]

There is also an issue [8] where trap messages are not emitted in cases where they should be due to a null check. The purpose of the null check was to prevent a nullptr dereference that occurred in debug-info prologue. This is a known issue to which there isn't a concrete solution as of current.

## Conclusion

I want to give a special thanks to my mentors, Dan and Michael, for being there for me the whole way. I appreciate their commitment to the project and their patience with me. I'm incredibly grateful that I was able to work on this project and I wouldn't have traded it for anything else. Being a beginner to both LLVM and open-source, I have to admit I was overwhelmed at first, but slowly, along with their help, I was able to gain at least a semblance of understanding of how things worked. I could not have asked for a better set of mentors, so again, a huge thanks to them. I want to also extend my gratitude to the LLVM Foundation for this opportunity.

I've had a lot of fun with this project and I hope to contribute more to LLVM, or in open-source in general, in the future.

## External Links

[1] https://github.com/llvm/llvm-project/pull/145967
[2] https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html#id5
[3] https://discourse.llvm.org/t/clang-gsoc-2025-usability-improvements-for-trapping-undefined-behavior-sanitizer/84568/11
[4] https://github.com/llvm/llvm-project/pull/147997
[5] https://discourse.llvm.org/t/rfc-emit-a-warning-when-fsanitize-trap-is-passed-without-associated-fsanitize/87893
[6] https://github.com/llvm/llvm-project/pull/154618
[7] https://github.com/llvm/llvm-project/pull/153845
[8] https://github.com/llvm/llvm-project/issues/150707