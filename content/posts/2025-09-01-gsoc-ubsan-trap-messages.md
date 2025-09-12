---
author: "Anthony Tran"
date: "2025-09-01"
tags: ["GSoC", "Clang", "CodeGen"]
title: "GSoC 2025: Usability Improvements for the Undefined Behavior Sanitizer"
---


## Introduction


My name is Anthony and I had the pleasure of working on improving the Undefined Behavior Sanitizer this Google Summer of Code 2025. My mentors were Dan Liew and Michael Buch.


## Background


Undefined Behavior Sanitizer (UBSan) is a tool for detecting a subset of the undefined behaviors in the C, C++, and Objective-C languages at runtime. This project focused mainly on the trapping variant of UBSan, which is evoked through `-fsanitize-trap=<...>` along with `-fsanitize=<...>`. Trapping UBSan is a lighter-weight version of UBSan because upon detection of undefined behavior a trap instruction is executed rather than calling into a runtime library to handle the undefined behavior. This makes it more appealing for kernel, embedded, and production hardening use cases. For cases of undefined behavior that can be detected by UBSan, check out the [official clang documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html).

An issue with trapping UBSan prior to my work was that it was much harder to debug undefined behavior when it is detected when compared to the non-trapping mode. To illustrate this consider this C program that reads integers from the command line arguments and adds them.


```
#include <stdlib.h>
#include <stdio.h>
int add(int a, int b) {
    return a + b;
}
int main(int argc, const char** argv) {
    if (argc < 3)
        return 1;
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    int result = add(a, b);
    printf("Added %d + %d = %d\n", a, b, result);
    return 0;
}
```


If this program is compiled and executed using UBSan with its userspace runtime it provides helpful output diagnosing the problem and also allows execution to continue.


```
$ bin/clang -fsanitize=undefined add.c -g -o add && ./add 2147483647 1
add.c:5:13: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior add.c:5:13 
Added 2147483647 + 1 = -2147483648
```


In contrast when using UBSan in trapping mode the program immediately terminates when undefined behavior is detected as shown below.


```
$ clang -fsanitize=undefined -fsanitize-trap=undefined add.c -g -o add && ./add 2147483647 1
[1]    54357 trace trap  ./add 2147483647 1
```


This is the expected behavior of trapping mode but how should a developer debug what happened when a trap is hit? If we attach a debugger and run the example program this is the output LLDB shows.


```
$ lldb ./add -- 2147483647 1    
(lldb) target create "./add"
(lldb) settings set -- target.run-args  "2147483647" "1"
(lldb) r
Process 17347 launched: 'add' (arm64)
Process 17347 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BREAKPOINT (code=1, subcode=0x100003d3c)
    frame #0: 0x0000000100003d3c add`add(a=2147483647, b=1) at add.c:5:13
   2    #include <stdio.h>
   3   
   4    int add(int a, int b) {
-> 5        return a + b;
   6    }
   7   
   8    int main(int argc, const char** argv) {
(lldb) bt
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BREAKPOINT (code=1, subcode=0x100003d3c)
  * frame #0: 0x0000000100003d3c add`add(a=2147483647, b=1) at add.c:5:13
    frame #1: 0x0000000100003eec add`main(argc=3, argv=0x000000016fdff110) at add.c:13:18
    frame #2: 0x00000001842bab98 dyld`start + 6076
(lldb) dis -p
add`add:
->  0x100003d3c <+40>: brk    #0x5500
    0x100003d40 <+44>: ldr    w0, [sp, #0x4]
    0x100003d44 <+48>: add    sp, sp, #0x10
    0x100003d48 <+52>: ret   
```


We can see that a `brk` instruction was hit while handling the `a + b` expression but there is no good explanation of what happened. `brk` is the trap instruction on arm64 but it is not particularly clear this has anything to do with UBSan. For this toy example we can speculate that integer overflow occurred because the program was built with trapping UBSan and the trap was hit while handling `a + b`. However, in real programs built with trapping UBSan and potentially other hardening mechanisms it is often far less obvious what happened.


For this particular example. The information that this is an integer overflow UBSan check is actually there but it is not very obvious. On x86_64 and arm64 the reason for trapping is actually [encoded in the operand to the trap instruction](https://maskray.me/blog/2023-01-29-all-about-undefined-behavior-sanitizer ). In this case the `#0x5500` immediate to the brk instruction encodes that this is a UBSan trap for integer overflow. The UBSan immediate is encoded as `('U' << 8) + SanitizerHandler` where [`SanitizerHandler` is the enum value from the `SanitizerHandler` enum inside Clang’s internals](https://github.com/llvm/llvm-project/blob/96195e7d44613e272475c90df187678036f21966/clang/lib/CodeGen/SanitizerHandler.h#L78).


As we can see the debugging experience with UBSan traps is not ideal and improving this was the primary goal of the GSoC project.




## Human readable descriptions of UBSan traps in LLDB


During my GSoC project I implemented support for displaying human readable descriptions of UBSan traps in LLDB to improve the debugging experience.


### Let the debugger handle most of the work


An alternative to this approach would be to teach debuggers (e.g. LLDB) to decode the trap reason encoded in trap instructions in the debugger. However, this approach wasn’t taken for several reasons:


* Using the trap reason encoded in trap instructions only works for x86_64 and arm64. The approach that I used works for all targets where debug info is supported (many more).

* Relying on decoding the trap reason encoded in the trap instruction creates a tight coupling between the compiler and the debugger because if the encoding ever changes:
  * The debugger would need to be changed to adapt to the new encoding.

  * Older versions of the debugger would fail to work with binaries using the new encoding.
  * New versions of the debugger would fail to work with binaries using the old encoding.
* In contrast, encoding the trap reason as a string in the debug info is a much looser coupling because the compiler is free to change the trap reason without changes to the debugger.



### Encoding the trap reason in the debug info


The approach I took is based on how `__builtin_verbose_trap` encodes its message into debug info [11] [12], a feature which was implemented in the past for [libc++ hardening](https://discourse.llvm.org/t/rfc-hardening-in-libc/73925). The core idea is that the trap reason string gets encoded directly in the trap's debug information.

To accomplish this, we needed to find a place to "stuff" the string in the DWARF DIE tree. Using a `DW_TAG_subprogram` was deemed the most straightforward and space-efficient location. This means we create a synthetic `DISubprogram` which is not a real function in the compiled program; it exists only in the debug info as a container. While the string could have been placed elsewhere, for reasons outside the scope of this blog post, it resides on this fake function DIE, with the trap reason encoded in the `DW_TAG_subprogram`'s name. For a deeper dive into this design decision, you can see [15](https://github.com/llvm/llvm-project/pull/145967#issuecomment-3054319138).

Let's look at the LLVM IR of the previous example to see how this is implemented:

```
$ clang -fsanitize=undefined -fsanitize-trap=undefined  add.c -g -o - -o - -S -emit-llvm -fsanitize-debug-trap-reasons=basic
; Function Attrs: noinline nounwind optnone ssp uwtable(sync)
define i32 @add(i32 noundef %a, i32 noundef %b) #0 !dbg !17 !func_sanitize !22 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
    #dbg_declare(ptr %a.addr, !23, !DIExpression(), !24)
  store i32 %b, ptr %b.addr, align 4
    #dbg_declare(ptr %b.addr, !25, !DIExpression(), !26)
  %0 = load i32, ptr %a.addr, align 4, !dbg !27
  %1 = load i32, ptr %b.addr, align 4, !dbg !28
  %2 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %0, i32 %1), !dbg !29, !nosanitize !21
  %3 = extractvalue { i32, i1 } %2, 0, !dbg !29, !nosanitize !21
  %4 = extractvalue { i32, i1 } %2, 1, !dbg !29, !nosanitize !21
  %5 = xor i1 %4, true, !dbg !29, !nosanitize !21
  br i1 %5, label %cont, label %trap, !dbg !29, !prof !30, !nosanitize !21
trap:                                             ; preds = %entry
  call void @llvm.ubsantrap(i8 0) #4, !dbg !31, !nosanitize !21
  unreachable, !dbg !31, !nosanitize !21
cont:                                             ; preds = %entry
  ret i32 %3, !dbg !34
}
;...
!29 = !DILocation(line: 5, column: 13, scope: !17)
!30 = !{!"branch_weights", i32 1048575, i32 1}
!31 = !DILocation(line: 0, scope: !32, inlinedAt: !29)
!32 = distinct !DISubprogram(name: "__clang_trap_msg$Undefined Behavior Sanitizer$Integer addition overflowed", scope: !2, file: !2, type: !33, flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: !14)
```

The debug metadata for the `@llvm.ubsantrap` call is `!31`.  That `DILocation` has the scope of the `DISubprogram` assigned to `!32` which is the artificial function which encodes the trap category. This function's name is formatted as `__clang_trap_msg$<Category>$<TrapMessage>` to encode the trap category (`Undefined Behavior Sanitizer`) and the specific message (`Integer addition overflowed`). This function does not actually exist in the compiled program. It only exists in the debug info as a convenient (albeit hacky) way to describe the reason for trapping. When a trap is hit in the debugger, the debugger retrieves this string from the debug info and shows it as the reason for trapping.

Note that the `DILocation` for `!31` has `inlinedAt:` which tells us that the trap was inlined from `!32` into the location at `!29` which is the location of the `a + b` expression in the `add` function. 


I implemented this change on this [PR](https://github.com/llvm/llvm-project/pull/145967).


### Debug info size changes


One concern that a reviewer had was the debug info size difference. This was one of the motivations for putting this feature under the new `-fsanitize-debug-trap-reasons` flag because initially (prior to code review), my mentors and I planned to have the trap feature flag accompany the `-fsanitize-trap=` flag. Although the `-fsanitize-debug-trap-reasons` flag is on by default (so long as trapping UBSan is enabled), having the trap reason feature under a flag allows users to opt-out by using the `-fno-sanitize-debug-trap-reasons` flag. 

Using bloaty, I tested a release build of clang with the `-fsanitize-debug-trap-reasons` flag enabled, and one with it disabled (`-fno-sanitize-debug-trap-reasons`). We found that the size difference was negligible; results are below.


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


Note it is likely the code size difference is negligible because because in optimized builds trap instructions in a function get merged together which causes the additional debug info my patch adds to be dropped. 

An increase of size would likely be the result of extra bytes per-UBSan trap in debug_info. It would also be contingent on the number of traps emitted since a new `DW_TAG_subprogram` DIE is emitted for each trap with this new feature. [A later comparison on a larger code base ("Big Google Binary")](https://github.com/llvm/llvm-project/pull/154618#issuecomment-3225724300), actually found a rather significant size increase of about 18% with trap reasons enabled. Future work may involve looking into why this is happening, and how such drastic size increases can be reduced.


### Displaying the trap reason in the debugger


With the support in the compiler for encoding the trap reasons for UBSan implemented I then turned my attention to displaying these in the LLDB debugger.


In this particular case nothing new needs to be implemented in LLDB because the `VerboseTrapFrameRecognizer` in LLDB which was implemented for `__builtin_verbose_trap` is general enough that it already supports any artificial function in the debug info of the form `__clang_trap_msg$<Category>$<TrapMessage>`.


So if we take the running example and run it under LLDB, its output now looks like.


```
$ clang -fsanitize=undefined -fsanitize-trap=undefined add.c -g -o add
$ lldb ./add -- 2147483647 1
(lldb) target create "add"
(lldb) settings set -- target.run-args  "2147483647" "1"
(lldb) r
Process 81705 launched: '/add' (arm64)
Process 81705 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = Undefined Behavior Sanitizer: Integer addition overflowed
    frame #1: 0x0000000100003d3c add`add(a=2147483647, b=1) at add.c:5:13
   2    #include <stdio.h>
   3   
   4    int add(int a, int b) {
-> 5        return a + b;
   6    }
   7   
   8    int main(int argc, const char** argv) {
(lldb) bt
* thread #1, queue = 'com.apple.main-thread', stop reason = Undefined Behavior Sanitizer: Integer addition overflowed
    frame #0: 0x0000000100003d3c add`__clang_trap_msg$Undefined Behavior Sanitizer$Integer addition overflowed at add.c:0 [inlined]
  * frame #1: 0x0000000100003d3c add`add(a=2147483647, b=1) at add.c:5:13
    frame #2: 0x0000000100003eec add`main(argc=3, argv=0x000000016fdff110) at add.c:13:18
    frame #3: 0x00000001842bab98 dyld`start + 6076
(lldb) dis -p
add`__clang_trap_msg$Undefined Behavior Sanitizer$Integer addition overflowed:
->  0x100003d3c <+40>: brk    #0x5500
    0x100003d40 <+44>: ldr    w0, [sp, #0x4]
    0x100003d44 <+48>: add    sp, sp, #0x10
    0x100003d48 <+52>: ret   
```


Notice that:


The stop reason now shows as `Undefined Behavior Sanitizer: Integer addition overflowed`. Previously no helpful stop reason was shown.
We are stopped with `frame #1` selected and the artificial frame (`frame #0`) is present in the backtrace. LLDB does this so stopping is not shown in the artificial function which would be confusing.
The `dis -pc` output claims we are inside the artificial function. This is an artifact of the implementation that is a little confusing but worth the trade-off.


So for this portion of my GSoC project the only thing I need to do was added a test case to ensure LLDB behaved appropriately. This was done on [this PR](https://github.com/llvm/llvm-project/pull/151231).




## RFC: Add a warning when `-fsanitize=` is passed without associated `-fsanitize-trap=`


The next part of my GSoC project was to post an RFC and implement a sketch fix for a usability problem with trapping UBSan.


Currently, clang does not warn about cases where `-fsanitize-trap=` does nothing (silent no-op), particularly [in the case where `-fsanitize-trap=` is passed without `-fsanitize=`](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html#id5):


Ex:


`$ clang -fsanitize-trap=undefined foo.c`


Emits no warning, even though `-fsanitize-trap=undefined` is not doing anything here.


We thought it would be more user-friendly to add a warning for such cases, but due to some [initial community pushback](https://discourse.llvm.org/t/clang-gsoc-2025-usability-improvements-for-trapping-undefined-behavior-sanitizer/84568/11), it was decided that an RFC should be opened. I ended up writing a [sketch patch that emitted a warning for such cases](https://github.com/llvm/llvm-project/pull/147997).


Ex:


`$ clang -fsanitize-trap=undefined foo.c`


Would now emit:


`warning: -fsanitize-trap=undefined has no effect because the "undefined" sanitizer is disabled; consider passing "-fsanitize=undefined" to enable the sanitizer`


Unfortunately, we found that the emission of such warnings could become exceedingly complicated and a point of contention due to the existence of sanitizer groups, subgroups, and individual sanitizers. Determining the correct behavior for various cases, historical precedence with no-ops, interference with current build systems, prioritization of existing build systems over the user experience, and compatibility with gcc led to the end of [the RFC](https://discourse.llvm.org/t/rfc-emit-a-warning-when-fsanitize-trap-is-passed-without-associated-fsanitize/87893).


## Expand upon the hard-coded strings in `-fsanitize-debug-trap-reasons` to be more specific

There were two initial design decisions to pick from here. Either I could:

**(a)** Use some string formatting, such as LLVM's formatvariadic or raw_ostream. For some implementation context, the function in which the trap messages were generated was called in a function called `EmitTrapCheck`, so the idea was to pass down extra information from earlier in the call stack before `EmitTrapCheck` was called.

or 

**(b)** Extend clang's diagnostic system to accomodate trap reasons. This is explained further below.

Due to the time it took to complete the first two tasks, I chose the first option. I deemed the second option to be a large commitment that I wouldn't have been able to do by the end of the GSoC coding period. Additionally, I was unsure if building on top of the diagnostics subsystem would be approved since diagnostics were originally intended to emit messages within the command line, not debug info. 

After taking some time to investigate possible cases where extra information in trap messages could be useful, I put up [a PR](https://github.com/llvm/llvm-project/pull/153845). The patch was admittedly quite messy, so to take a cleaner approach that was more aligned with clang's frontend, one of my mentors, Dan, ended up following through with option (b) to [create the extension to the clang diagnostics subsystem to work with trap messages](https://github.com/llvm/llvm-project/pull/154618). By extending the diagnostics subsystem, it allows us to leverage the powerful string formatting engine of the diagnostics system. 

However, this doesn't mean that the effort used to write proper hard-coded trap messages under `-fsanitize-debug-trap-reasons` was thrown away. Rather as of [Dan's patch](https://github.com/llvm/llvm-project/pull/154618), the flag now has two options: `-fsanitize-debug-trap-reasons=basic` for the hard-coded trap messages and `-fsanitize-debug-trap-reasons=detailed` for the detailed trap messages which utilize the trap reasons diagnostics API. This was done in case users did not want to deal with the larger binary sizes that came with the trap reasons.


## What I've Learned


Before I started this GSoC, I barely even knew how to build clang and LLVM or use git in a large open-source project. My mentors showed me the ropes on a lot of things (particularly how to properly use git and build and configure clang), and I came out of this summer knowing a lot more of how to get my changes properly reviewed and upstreamed. I was also able to get a firmer understanding of the Undefined Behavior Sanitizer, better C++ programming practices, and the LLVM codebase. 


## Work to Do


As stated prior, some research needs to be conducted to figure out how size increase can be minimalized.

Also stated previously, the diagnostics extension for trap messages has been [upstreamed by Dan](https://github.com/llvm/llvm-project/pull/154618). As of right now, only signed and unsigned overflow for addition, subtraction, and multiplication are being used by this system. I plan to integrate what I found on my [abandoned PR](https://github.com/llvm/llvm-project/pull/153845) by building on top of what Dan has already done. This will be done after the GSoC coding period.


There is [an issue](https://github.com/llvm/llvm-project/issues/150707) where trap messages are not emitted in cases where they should be due to a null check. The purpose of the null check was to prevent a nullptr dereference that occurred in the debug-info prologue. This is a known issue to which there isn't a concrete solution as of current.


## Conclusion


I want to give a special thanks to my mentors, Dan and Michael, for being there for me the whole way. They helped a lot with guiding me through git, the LLVM code base, and even this blog post. I appreciate their commitment to the project and their patience with me. 

I'm incredibly grateful that I was able to work on this project and I wouldn't have traded it for anything else. Being a beginner to both LLVM and open-source, I have to admit I was overwhelmed at first, but slowly, along with their help, I was able to gain at least a semblance of understanding of how things worked. I could not have asked for a better set of mentors, so again, a huge thanks to them. I want to also extend my gratitude to the LLVM Foundation for this opportunity.


I've had a lot of fun with this project and I hope to contribute more to LLVM, or in open-source in general, in the future.

## Landed PRs
https://github.com/llvm/llvm-project/commits?author=anthonyhatran

## External Links

[1] https://github.com/llvm/llvm-project/pull/145967

[2] https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html#id5

[3] https://discourse.llvm.org/t/clang-gsoc-2025-usability-improvements-for-trapping-undefined-behavior-sanitizer/84568/11

[4] https://github.com/llvm/llvm-project/pull/147997

[5] https://discourse.llvm.org/t/rfc-emit-a-warning-when-fsanitize-trap-is-passed-without-associated-fsanitize/87893

[6] https://github.com/llvm/llvm-project/pull/154618

[7] https://github.com/llvm/llvm-project/pull/153845

[8] https://github.com/llvm/llvm-project/issues/150707

[9] https://maskray.me/blog/2023-01-29-all-about-undefined-behavior-sanitizer 

[10] https://github.com/llvm/llvm-project/pull/151231 

[11] https://discourse.llvm.org/t/rfc-adding-builtin-verbose-trap-string-literal/75845

[12] https://github.com/llvm/llvm-project/pull/79230

[13] https://discourse.llvm.org/t/rfc-hardening-in-libc/73925

[14] https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html

[15] https://github.com/llvm/llvm-project/pull/145967#issuecomment-3054319138