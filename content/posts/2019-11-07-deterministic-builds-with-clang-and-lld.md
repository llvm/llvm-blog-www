---
author: "Nico Weber"
date: "2019-11-07"
tags: ["lld","Clang"]
title: "Deterministic builds with clang and lld"
url: /2019/11/deterministic-builds-with-clang-and-lld.html
---

Deterministic builds can lower continuous integration costs and give you more confidence in your build and test process. This post outlines what it means for a build to be deterministic, the advantages of deterministic builds, and how to achieve them using LLVM tools.

**Note:** This article formats plain text like command line options `like so`. Key information (including some options) is highlighted <mark>like so</mark>.

# What is a deterministic build, and its advantages

A build is called *deterministic* or *reproducible* if running it twice produces exactly the same build outputs.

There are several degrees of build determinism that are increasingly useful but increasingly difficult to achieve:

1. *Basic* determinism: Doing a full build of the same source code in the same directory on the same machine produces exactly the same output every time, in the sense that a content hash of the final build artifacts and of all intermediate files does not change.
   - Once you have this, if all your builders are configured the same way (OS version, toolchain, build path, checkout path, …), they can share build artifacts, for example by using distcc.
   - This also allows local caching of test suite results keyed by a hash of test binary and test input files.
   - Illustrative example: `./build src out ; mv out out.old ; ./build src out ; diff -r out out.old`
2. *Incremental* basic determinism: Like basic determinism, but the output binaries also don’t change in partial rebuilds. In build systems that track file modification times to decide when to rebuild, this means for example that updating the modification time on a C++ source file (without doing any actual changes) and rebuilding will produce the same output as a full build.
   - This allows having build bots that don’t do full builds each time, while still allowing caching of compile artifacts and test results.
   - Illustrative example: `./build src out ; cp -r out out.old ; touch src/foo.c ; ./build src out ; diff -r out out.old`
3. *Local* determinism: Like incremental basic determinism, but builds are also independent of the name of the build directory. Builds of the same source code on the same machine produce exactly the same output every time, independent of the location of the source checkout directory or the build directory.
   - This allows machines to have several build directories at different locations but still share compile and test caches.
   - Illustrative example: `cp -r src src2 ; ./build src out ; ./build src2 out2 ; diff -r out out2`
4. *Universal* determinism: Like 3, but builds are also independent of the machine the build runs on. Everybody that checks out the project at a given revision into any directory and builds it following the build instructions ends up with exactly the same bits in the build output.
   - Since exact local OS and locally installed packages no longer matter, this allows devs to share compile and test caches with bots, without having to use difficult-to-setup containers.
   - It also allows easy verification of builds done by others to make sure output binaries haven’t been tampered with.
   - Illustrative example: `./build src out ; ssh remote ./build src out && scp remote:out out2 ; diff -r out out2`

# Plan of attack

To make sure that a deterministic build stays deterministic, you should set up a builder that verifies that your build is deterministic. Even if your build isn’t deterministic yet, you can set up a bot that verifies that some parts of your build are deterministic and then expand the checks over time.

For example, you could have a bot that does a full build in a fixed build directory, then moves the build artifacts out of the way, and does another full build, and once your compiles have basic determinism, add a step that checks that object files between the two builds directories are the same. You could even add incremental checking for specific subdirectories or build targets while you work towards full basic determinism.

Once your links are deterministic, check that binaries are identical as well. Once all your build steps are deterministic, compare all files in the two build directories.

Once your build has incremental determinism, do an incremental build for the first build and a full build for the second build. Once your build has local determinism, do the two builds at different build paths.

# Getting to basic determinism

Basic determinism needs tools (compiler, linker, etc) that are deterministic. Tools internally must not output things in hash table order, multi-threaded programs must not write output in the order threads finish, etc. All of LLVM’s tools have deterministic outputs when run with the right flags but not necessarily by default.

The C standard defines the predefined macros `__TIME__` and `__DATE__` that expand to the time a source file is compiled. Several compilers, including clang, also define the non-standard `__TIMESTAMP__`. This is inherently nondeterministic. You should not use these macros, and you can use `-Wdate-time` to make the compiler emit a warning when they are used.

If they are used in third-party code you don’t control, you can use <mark>-Wno-builtin-macro-redefined -D__DATE__= -D__TIME__= -D__TIMESTAMP__=</mark> to make them expand to nothing.

When targeting Windows, clang and clang-cl by default also embed the current time in a timestamp field in the output .obj file, because Microsoft’s link.exe in `/incremental` mode silently mislinks files if that field isn’t set correctly. If you don’t use link.exe’s `/incremental` flag, or if you link with lld-link, you should pass <mark>/Brepro</mark> to clang-cl to make it not write the current timestamp into its output.

Both link.exe and lld-link also write the current timestamp into output .dll or .exe files. To make them instead write a hash of the binary into this field, you can pass `/Brepro` to the linker as well. However, some tools, such as Windows 7’s app compatibility database, try to interpret that field as an actual timestamp and can get confused if it’s set to a hash of the binary. For this case, lld-link also offers a <mark>/timestamp:</mark> flag that you can give an explicit timestamp that’s written into the output. You could use this to for example write the time of the commit the code is built at instead of the current time to make it deterministic. (But see the footnote on embedding commit hashes below.)

Visual Studio’s assemblers ml.exe and ml64.exe also insist on writing the current time into their output. In situations like this, where you can’t easily fix the tool to write the right output in the first place, you need to write wrappers that fix up the file after the fact. As an example, <mark>[ml.py](https://cs.chromium.org/chromium/src/build/toolchain/win/ml.py)</mark> is the wrapper the Chromium project uses to make ml’s output deterministic.

macOS’s libtool and ld64 also insist on writing timestamps into their outputs. You can set the environment variable <mark>ZERO_AR_DATE</mark> to 1 in a wrapper to make their output deterministic, but that confuses lldb of older Xcode versions.

Gcc sometimes uses random numbers in certain symbol mangling situations. Clang does not do this, so there’s no need to pass `-frandom-seed` to clang.

It’s a good idea to make your build independent of environment variables as much as possible, so that accidental local changes in the environment don’t affect the build output. You should pass <mark>/X</mark> to clang-cl to make it ignore `%INCLUDE%` and explicitly pass system include directories via the <mark>-imsvc</mark> switch instead. Likewise, very new lld-link versions (LLVM 10 and newer, at the time of this writing still unreleased) understand the flag <mark>/lldignoreenv</mark> flag, which makes lld-link ignore the `%LIB%` environment variable; explicitly pass system library directories via <mark>/libpath:</mark>.

## Footnote on embedding git hashes into the binary

It might be tempting to embed the git commit hash or svn revision that a binary was built at into the binary’s `--version` output, or use the revision as a cache key to invalidate on-disk caches when the version changes.

This doesn’t affect your build’s determinism, but it does affect the hit rate if you’re using deterministic builds to cache test run results. If your binary embeds the current commit, it is guaranteed to change on every single commit, and you won’t be able to cache test results across commits. Even commits that just fix typos in comments, add non-code documentation, or that only affect code used by some but not all of your binaries will change every binary.

For cache invalidation, consider using something finer-grained, such as only the latest commit of the directory containing the cache handling code, or the hash of all source files containing the cache handling code.

For `--version` output, if your build is fully deterministic, the hash of the binary itself (and its dynamic library dependencies) can serve as a stable version identifier. You can keep a map of binary hash to all commit hashes that produce that binary somewhere.

Windows only: For the same reason, just using the timestamp of the latest commit as a `/timestamp:` might not be the best option. Rounding the timestamp of the latest commit to 6h (or similar) granularity is a possible approach for not having the timestamp change the binary on every commit, while still keeping the timestamp close to reality. For production builds, the symbol server key for binaries is a (executable size, timestamp) pair, so here having fairly granular timestamps is important to not map binaries from consecutive commits to the same symbol server key. Depending on how often you push production binaries to your symbol servers, you might want to use the timestamp of the latest commit as `/timestamp:` for official builds, or you might want to round to finer granularity than you do on dev builds.

# Getting to incremental determinism

Having deterministic incremental builds mostly requires having correct incremental builds, meaning that if a file is changed and the build reruns, everything that uses this file needs to be rebuilt.

This is very build system dependent, so this post can’t say much about it.

In general, every build step needs to correctly declare all the inputs it depends on.

Some tools, such as Visual Studio’s link.exe in `/incremental` mode, by design write a different output every time. Don’t use inherently incrementally non-deterministic tools like that if you care about build determinism.

The build should not depend on environment variables, since build systems usually don’t model dependencies on environment variables.

# Getting to local determinism

Making build outputs independent of the names of the checkout or build directory means that build outputs must not contain absolute paths, or relative paths that contain the name of either directory.

A possible way to arrange for that is to put all build directories into the checkout directory. For example, if your code is at `path/to/src`, then you could have “out” in your `.gitignore` and build directories at `path/to/src/out/debug`, `path/to/src/out/release`, and so on. The relative path from each build artifact to the source is with `../../` followed by the path of the source file in the source directory, which is identical for each build directory.

The C standard defines the predefined macro `__FILE__` that expands to the name of the current source file. Clang expands this to an absolute path if it is invoked with an absolute path (`clang -c /absolute/path/to/my/file.cc`), and to a relative path if it is invoked with a relative path (`clang ../../path/to/my/file.cc`). To make your build locally deterministic, <mark>pass relative paths to your .cc files to clang</mark>.

By default, clang will internally use absolute paths to refer to compiler-internal headers. Pass <mark>-no-canonical-prefixes</mark> to make clang use relative paths for these internal files.

Passing relative paths to clang makes clang expand `__FILE__` to a relative path, but paths in debug information are still absolute by default. Pass <mark>-fdebug-compilation-dir .</mark> to make paths in debug information relative to the build directory. (Before LLVM 9, this is an internal clang flag that must be used as `-Xclang -fdebug-compilation-dir -Xclang .`) When using clang’s integrated assembler (the default), <mark>-Wa,-fdebug-compilation-dir,.</mark> will do the same for object files created from assembly input. (For ml.exe / ml64.exe, see the script linked to from the “Basic determinism” section above.)

Using this means that debuggers won’t automatically find the source code belonging to your binary. At the moment, there’s no way to tell debuggers to resolve relative paths relative to the location of the binary ([DWARF proposal](http://dwarfstd.org/ShowIssue.php?issue=171130.2), [gdb patch](https://gnutoolchain-gerrit.osci.io/r/c/binutils-gdb/+/402)). See the end of this section for how to configure common debuggers to work correctly.

There are a few flags that try to make compilers produce relative paths in outputs even if the filename passed to the compiler is absolute (`-fdebug-prefix-map`, `-ffile-prefix-map`, `-fmacro-prefix-map`). <mark>Do not use these flags</mark>.

- They work by adding lhs=rhs replacement patterns, and the lhs must be an absolute path to remove the absolute path from the output. That means that while they make the compile output path-independent, they make the compile command itself path-dependent, which hinders distributed compile caching. With `-grecord-gcc-switches` or `-frecord-gcc-switches` the compile command is embedded in debug info or even the object file itself, so in that case the flags even break local determinism. (Both `-grecord-gcc-switches` and `-frecord-gcc-switches` default to false in clang.)
- They don’t affect the paths in dwo files when using fission; passing relative paths to the compiler is the only way to make these paths relative.

On Windows, it’s very unusual to have PDBs with relative paths. You can pass <mark>/pdbsourcepath:X:\fake\prefix</mark> to lld-link to make it resolve all relative paths in object files against a fixed absolute path to make sure your final PDBs contain absolute paths. Since the absolute path is against a fixed prefix, this doesn’t impair determinism. With this, both binaries and PDBs created by clang-cl and lld-link will be fully deterministic and build path independent.

Also on Windows, the linker by default puts the absolute path the to the generated PDB file in the output binary. Pass <mark>/pdbaltpath:%_PDB%</mark> when you pass `/debug` to make the linker write a relative path to the generated PDB file instead. If you have custom build steps that extract PDB names from binaries, you have to make sure these scripts work with relative paths. Microsoft’s tools (debuggers, ETW) work fine with this set in most situations, and you can add a symbol search path in the cases where they don’t (when the binaries are copied before being run).

## Getting debuggers to work well with locally deterministic builds

At the moment, no debugger offers an option to resolve relative paths in debug info against the directory the debugged binary is in.

Some debuggers (gdb, lldb) do try to resolve relative paths against the cwd, so a simple way to make debugging work is to cd into your build directory before debugging.

If you don’t want to require devs to cd into the build directory for debugging to work, you have to do debugger-specific configuration tweaks.

To make sure devs don’t miss this, you could have your custom init script set an env var and query if it’s set early during your test binary startup, and exit with a message like “Add `source /path/to/your/project/gdbinit` to your `~/.gdbinit`" if the environment variable isn’t set.

### gdb

`dir path/to/build/dir` tells gdb what directory to resolve relative paths against.

`show debug-file-directory` prints the list of directories gdb looks in for dwo files. Query that, append `:path/to/build/dir`, and call `set debug-file-directory` to add your build dir to that search path.

For an example, see [Chromium’s gdbinit](https://chromium.googlesource.com/chromium/src/+/master/tools/gdb/gdbinit#78) (which also does a few other unrelated things).

### lldb

`settings set target.source-map ../.. /absolute/path/to/build/dir` can map the “../..” prefix that all .cc files will refer to when using the setup described above with an absolute path. This requires Xcode 10.3 or newer; the lldb shipping with Xcode 10.1 has problems with this setup.

For an example, see [Chromium’s lldbinit](https://chromium.googlesource.com/chromium/src/+/master/tools/lldb/lldbinit.py).

### Visual Studio’s debugger and windbg

If you use the setup described above, `/PDBSourcePath:X:\fake\prefix` will combine with the `..\..\my\file.cc` relative paths to make your code appear at `X:\my\file.cc`. To make Windows debuggers find them, you have two options:

1. Run `subst X: C:\src\real\root` in cmd.exe before launching the debuggers to create a virtual drive that maps X: to the actual source location. Both windbg and Visual Studio will load code over X: this way.
2. Add `C:\src\real\root` to each debugger’s source search path.
   - [Windbg](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/source-window#source-path): Run `.srcpath+ C:\src\real\root`. You can also set this via the `_NT_SOURCE_PATH`  environment variable, or via  File->Source File Path (Ctrl+P). Or pass `-srcpath C:\src\real\root` when launching windbg from the command line.
   - Visual Studio: The IDE has a [“Debug Source Files” property](https://docs.microsoft.com/en-us/visualstudio/debugger/debug-source-files-common-properties-solution-property-pages-dialog-box?view=vs-2019). Add `C:\src\real\root` to “Directories containing source code” to Project->Properties (Alt+F7)->Common Properties->Debug Source Files->Directories containing source code.

Alternatively, you could pass the absolute path to the actual build directory to `/PDBSourcePath:` instead of something like `X:\fake\prefix`. That way, all PDBs have “correct” absolute paths in them, while your compile steps are still path-independent and can share a cache across machines. However, since executables contain a reference to the PDB hash, none of your binaries will be path-independent. This setup doesn’t require any debugger configuration, but it doesn’t allow your builds to be locally deterministic.

# Getting to universal determinism

By now, your build output is deterministic as long as everyone uses the same compiler, and linker binaries, and as long as everyone uses the version of the SDK and system libraries.

Making your build independent of that requires making sure that everyone automatically uses the same compiler, linker, and SDK.

This might seem like a lot of work, but in addition to build determinism this work also gives you cross builds (where you can e.g. build the Linux version of your product on a Windows host).

It also versions the compiler, linker, and SDK used within your code, which means you will be able to update all your bots and devs to new versions automatically (and if an update causes issues, it’s easy to revert it).

You need to store the currently-used compiler, linker, and SDK versions in a file in your source control repository, and from some kind of hook that runs after pulling the newest version of the source, download compiler, linker, and SDK of the right version from some kind of cloud storage service.

You then need to modify your build files to use <mark>--sysroot</mark> (Linux), <mark>-isysroot</mark> (macOS), <mark>-imsvc</mark> (Windows) to use these hermetic SDKs for builds. They need to be somewhere below your source root to not regress build directory name invariance.

You also want to make sure your build doesn’t depend on environment variables, as already mentioned in the “Getting to incremental determinism”, since environments between different machines can be very different and difficult to control.

Build steps shouldn’t embed the hostname of the current machine or the logged-in user name in the build output, or similar.

# Summary

This post explained what deterministic builds are, how build determinism spans a spectrum (local, fixed-build-dir-path-only to fully host-OS-independent) instead of just being binary, and how you can use LLVM’s tools to make your build deterministic. It also touched on techniques you can use to make your test caches more effective.

*Thanks to Elly Fong-Jones for helping edit and structure this post, and to Adrian McCarthy, Bob Haarman, Bruce Dawson, Dirk Pranke, Fumitoshi Ukai, Hans Wennborg, Kai Naschinski, Reid Kleckner, Rui Ueyama, and Takuto Ikuta for reading drafts and suggesting improvements.*
