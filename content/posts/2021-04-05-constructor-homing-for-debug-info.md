---
author: Amy Huang
date: "2021-04-05"
tags: ["llvm"]
title:  Smaller debug info with constructor type homing
---

# Constructor type homing for debug info

## Background
Class type information is a large contributor to debug info size. Clang already has a few optimizations to reduce the size of class type information based on the assumption that debug info can be spread out over multiple compilation units. So, instead of emitting class type info in every compilation unit that references a class, we only really have to emit it in one place. (For all the other references, emitting the much smaller forward declaration of the class is sufficient.) As an example, one of the existing optimizations is vtable homing, where the type info for a dynamic C++ class is only emitted when its vtable is emitted.

## Constructor type homing
Constructor homing is a similar optimization that applies to almost all classes with constructors. It emits type information for a class wherever its constructor definition is emitted. Unlike with vtable homing, the type info for a class could be emitted more than once, but it has a large impact on debug info because it applies to a large percentage of classes. If all of a class's constructors are defined out of line, the class type information will only be emitted once. If there are constructors defined inline, the inline constructors, and therefore class type information, will be emitted in every compilation unit that calls a constructor.

Constructor homing assumes that if a user wants a class to have debug info, then that class was constructed somewhere in the program. This is a reasonable assumption to make, as classes viewed in the debugger probably exist in program memory, and any class that exists in memory must have been constructed.

Even though all classes have constructors, there are some types of classes that constructor homing doesn't apply to: trivial classes, aggregate classes, and classes with constexpr constructors. It's possible to create instances of these classes without emitting a constructor, so we can't guarantee that the debug info will be emitted. However, these types of classes tend to be fairly small, so we would probably see less of an improvement from using constructor homing on them.

Constructor homing can be enabled with `-Xclang -fuse-ctor-homing`. Eventually, the plan is to enable it by default in Clang so that it happens as part of `-fno-standalone-debug`. In terms of Clang's `-debug-info-kind=` flags, constructor homing is implemented as `-debug-info-kind=constructor`, one level below `-debug-info-kind=limited`.

## Size improvements
Emitting less class type info gives us a significant reduction in object file sizes. In a Chrome debug build on Linux (which uses split dwarf for debug info), .o and .dwo file sizes with constructor type homing are about 30% smaller (with a 20% overall reduction in build directory size). In a Clang debug build on Linux, .o file sizes are about 48% smaller (and the overall build directory is 38% smaller). On Windows, both Chrome and Clang had 37% smaller .obj files.

The smaller object file size also results in an improvement in link times and GDB load times. On Windows, linking Chrome with constructor homing is 6% faster, while linking Clang is 34% faster. On Linux there was no noticeable difference in link time in Chrome, but linking Clang is 25% faster.

Measured on my machine, without using `--gdb-index`, the GDB load time for Clang is about 2m30s without constructor homing, and 1 minute with. If `--gdb-index` is enabled, the GDB startup time is about a second regardless, and the binary size is about 30% smaller with constructor homing.

## Potential pitfalls
Ideally, constructor homing shouldn't change the debug info that's available when debugging, but there are some cases where it does. Even though this is undefined behavior in C++, it is possible to define a class with a non-trivial constructor and create an instance of it without calling the constructor (this is often done in C code, where there are no constructors):

```
Foo *p = malloc(sizeof(Foo));
p->someField = 1;
```

The constructor for Foo is never called, so its debug info is never emitted.

After enabling constructor type homing in Chrome, we discovered that there are a few classes in libc++ that avoid calling the constructor, and for various reasons, that would be difficult to change. To ensure that they still have debug info, there's a new attribute in Clang called `[[standalone_debug]]`. If a class has the attribute it will have the same debug info as if it were built with -fstandalone-debug. This can be used to get debug info for classes that otherwise would have had their type info omitted with constructor homing (or with any of the other debug info optimizations).

I've also looked into whether there are other common cases where constructor homing omits debug info. Manually comparing the debug info available in a Clang build showed some missing types. There were a few classes that were not used anywhere. There were also one or two pseudo-namespace classes that only had static methods and were therefore never constructed. Looking at diffs of the debug info is somewhat difficult given how many object files and types there are (most of the missing types I saw were because the type wasn't constructed in the particular binary or set of object files I compared), so there may have been other cases I missed.

# Summary
Constructor type homing is a new optimization that greatly reduces the size of debug info in object files. Currently it can be enabled with the cc1 flag `-fuse-ctor-homing`, and the plan is to enable it by default as part of `-fno-standalone-debug` in Clang. If you want to make your debug builds smaller, try adding `-Xclang -fuse-ctor-homing` to your build and let us know how much object file size it saves.

