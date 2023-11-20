---
author: "David Spickett"
date: "2023-11-20"
tags: ["TableGen"]
title: "Tools for Learning LLVM TableGen"
---

[TableGen](https://github.com/llvm/llvm-project/tree/main/llvm/utils/TableGen)
is a language used within the LLVM project for generating all kinds of files,
when maintaining those files by hand would be very difficult.

For example, it is used to define all of the instructions that can be used on a
particular architecture. The information is defined once in TableGen and we can
produce many things based on it. C++ code, documentation, command line options,
etc.

TableGen has been around since
[before](https://github.com/llvm/llvm-project/commit/a6240f6b1a34f9238cbe8bc8c9b6376257236b0a)
the first official release of LLVM, over 20 years ago.

Today in the monorepo there are over a thousand TableGen source files totalling
over 500,000 lines of code. Making it the 5th most popular language in the
monorepo.

| Language     | files | blank  | comment   | code     |
| ------------ | ------| -------| --------- | -------- |
| C++          | 29642 | 958542 |  1870101  | 5544445  |
| C/C++ Header | 11844 | 316806 |   499845  | 1486165  |
| C            | 10535 | 259900 |  1603594  | 1011269  |
| Assembly     | 10694 | 478035 |  1222315  |  820236  |
| TableGen     |  1312 |  94112 |    83616  |  580289  |

(Counted from
[this commit](https://github.com/llvm/llvm-project/commit/ba24b814f2a20a136f0a7a0b492b6ad8a62114c6),
rest of table omitted)

With projects like MLIR
[embracing TableGen](https://mlir.llvm.org/docs/DefiningDialects/Operations/)
it’s only going to grow from here. So if you’re contributing to LLVM, you’re
going to encounter it eventually.

Which might be a problem, because TableGen only exists within LLVM. It doesn’t
have the massive array of resources that a language like C++ has.

On top of joining a new project you have this Domain Specific Language (DSL)
to learn. You didn’t come to LLVM to learn a DSL, you probably came here
to write a compiler.

I can’t say that this problem will ever be solved, but the situation isn’t as
bleak as it appears. There have been big improvements in TableGen tools
recently, which means you can put more of your energy into the goals that
brought you to LLVM in the first place.

# A Brief Introduction to TableGen

Imagine you wanted to represent the registers of an architecture. I’m going to
use Arm's AArch64 in particular here.

You could describe them in TableGen like this:
```
class Register<int _size, string _alias=""> {
   int size = _size;
   string alias = _alias;
}

// 64 bit general purpose registers are X<N>.
def X0: Register<8> {}
// Some have special alternate names.
def X29: Register<8, "frame pointer"> {}
// ...a lot more registers omitted.
```

By default the TableGen compiler `llvm-tblgen` produces what are called
“records”, which are shown below.

**Note:** There are other compilers e.g. `clang-tblgen` and `lldb-tblgen`.
The difference between them is the backends that they include. The language is
the same.

```
------------- Classes -----------------
class Register<int Register:_size = ?, string Register:_alias = ""> {
 int size = Register:_size;
 string alias = Register:_alias;
}
------------- Defs -----------------
def X0 {        // Register
 int size = 8;
 string alias = "";
}
def X29 {       // Register
 int size = 8;
 string alias = "frame pointer";
}
```

This is the intermediate representation of the TableGen compiler, akin to LLVM's
LLVM IR.

Unlike LLVM, TableGen’s “targets” are not instructions for CPUs but
instead whatever output format the selected “backend” produces. For example
there is one that generates C++ code that implements
[searching](https://godbolt.org/z/5c696j1f9) of data tables.

You might take your register definitions and produce C++ code to initialise them
in some kind of bootloader. Perhaps you also document it and produce a diagram
to illustrate it. With enough backends, you could do all that from the same
TableGen source code.

You would write these backends either in C++ within the TableGen compiler,
or you can use the compiler’s [JSON output](https://godbolt.org/z/vre845e77)
(`--dump-json`) and write them in any language with a JSON parser (e.g.
[Python](https://github.com/llvm/llvm-project/blob/main/llvm/utils/TableGen/jupyter/sql_query_backend.ipynb)).

# There is TableGen and There Are Things Built With TableGen

This is more a mindset than a tool. It’s summed up best by a quote from the
[documentation](https://llvm.org/docs/TableGen/index.html#tablegen-deficiencies):

> Despite being very generic, TableGen has some deficiencies that have been
> pointed out numerous times. The common theme is that, while TableGen allows
> you to build domain specific languages, the final languages that you create
> lack the power of other DSLs, which in turn increase considerably the size and
> complexity of TableGen files.
>
> At the same time, TableGen allows you to create virtually any meaning of the
> basic concepts via custom-made backends, which can pervert the original design
> and make it very hard for newcomers to understand the evil TableGen file.”

This means that you'll be tackling the language itself, and the things built
with it. Which are often more complicated than the language.

It’s like learning C++ and struggling to use [Boost](https://www.boost.org/).
Someone might say to you “Boost isn’t required, why not remove it and save
yourself the hassle?”. As someone new to C++, you might not be aware of the
boundary between the two of them.

Of course this doesn't help you too much if the project you want to contribute
to uses Boost. You're stuck dealing with both. In LLVM terms, the TableGen
language and the backends that consume it are a package deal.

I bring this up so that you can draw a distinction between not understanding
one or the other. Knowing which one is confusing you is a big advantage
to finding help.

For any given task there's probably one or two "things built with" you need to
understand and even then not entirely.

Don't think that the TableGen journey (for lack of a less grand term) ends with
understanding the many many uses of it. If you could get there it would be an
end of a sort, but you really don't need to and hardly anyone does. Instead
put your energy into the things that really interest you.

# Compiler Explorer

Of course we have TableGen in Compiler Explorer! Is a language even real if it’s
not in Compiler Explorer?

(Of course it is, but if you’re favourite language isn’t, Compiler Explorer has
[excellent documentation](https://github.com/compiler-explorer/compiler-explorer/blob/main/docs/AddingALanguage.md)
and friendly maintainers)

Compiler Explorer is a whole bunch of different versions of compilers for
different languages and different architectures that you can access with just a
browser tab.

It’s an incredible tool for learning, teaching, triaging, optimising and
[many more](https://www.youtube.com/watch?v=O5sEug_iaf4) things. I won’t go into
detail about it here, just a few things about TableGen’s inclusion.

The obvious thing is that `llvm-tblgen` doesn’t emit machine code (though a
hypothetical backend could) so there is no option to compile to binary or
execute code.

By default you will see the records printed as plain text. A backend can be
chosen by adding a compiler option manually or as an “Action” from the
“Overrides” menu.

It’s important to note that TableGen backends have very specific expectations of
what will be in the source code. Almost as if you had a C++ compiler that
wouldn’t compile for Arm unless it saw the symbol `arm_is_cool` somewhere in the
source code.

In the LLVM monorepo all the required classes are set up for you, but in
Compiler Explorer they are not. So if you want to experiment with an existing
backend I suggest providing stub implementations of the classes, or copying some
in from the LLVM project. Standard includes from `include/llvm/*.td` can also be
used.

Developing a backend within Compiler Explorer is not possible at this time, but
you can select the JSON backend and copy that JSON to give to local scripts.

Multi-file projects (“IDE mode”) also work as expected, so you can have your own
[include files](https://godbolt.org/z/4qhdoaMjE) if you like.

Finally, remember that Compiler Explorer examples can be shared. If you’re
asking or answering questions about TableGen, always include a Compiler Explorer
link if you can!

# Jupyter Notebooks

[Jupyter](https://jupyter.org/) creates interactive notebooks that combine text
and code into one document. With the ability to edit that code rerun it to
update the results in the notebook.

This is great for taking notes or building up large examples from small chunks
of code. Then you can export your notes as a notebook anyone can edit, or in
static formats like PDF or Markdown.

TableGen can be used in one of these notebooks via the
[TableGen Jupyter Kernel](https://github.com/llvm/llvm-project/tree/main/llvm/utils/TableGen/jupyter).
Install instructions are on the linked page and you can watch me talk more about
it [here](https://www.youtube.com/watch?v=Gf0FUiY2TRo).

**Note:** There’s also an [MLIR kernel](https://github.com/llvm/llvm-project/tree/main/mlir/utils/jupyter)
for Jupyter, amongst many, many others.

We’ve aimed to give the same experience as other languages, so I will focus not
on how to use a notebook, but instead on what we’ve been able to make with them.

## TableGen Tutorial

The first notebook is an
[introduction to TableGen](https://github.com/llvm/llvm-project/blob/main/llvm/utils/TableGen/jupyter/tablegen_tutorial_part_1.ipynb).
You can read it on GitHub or download for use in Jupyter.

The great thing about this is that if something in that document doesn’t make
sense to you, you can edit the examples and experiment. Maybe it glosses over a
part you find really interesting. You can add a new code and see what happens.

## How to Write a TableGen Backend

This notebook is different. The language is Python and instead of focusing on
TableGen source, it will show you how to write a TableGen backend.

The 2021 LLVM dev talk
[“How to write a TableGen backend”](https://www.youtube.com/watch?v=UP-LBRbvI_U)
by Min-Yih Hsu is the basis for this. The
[notebook](https://github.com/llvm/llvm-project/blob/main/llvm/utils/TableGen/jupyter/sql_query_backend.ipynb)
is in fact a Python port of Min’s own
[C++](https://github.com/mshockwave/SQLGen) implementation.

It shows you how to take the JSON output of `llvm-tblgen` and process it with
Python to create SQL queries.

What’s unique here is we now have the same content in multiple media forms and
multiple programming languages. Pick the ones that best suit you.

Referring back to “There is TableGen and There are Things Built With TableGen”
, the tutorial notebook is TableGen. The writing a backend notebook is “Things
Built With TableGen”.

## Limitations

The major limitation of the notebooks is that we have no output filtering. This
means if you do `include “llvm/Target/Target.td"` you will get about 320,000
lines of output (before you have added any of your own code). This is more than
a default notebook will accept from a kernel and when I removed that limit, the
browser tab crashed.

It’s not a problem in most cases and the possible solutions have big trade offs
so we are not rushing into fixing it. If it does affect you, please post your
feedback on the [tracking issue](https://github.com/llvm/llvm-project/issues/72856).

# TableGen Language Server

The MLIR project has implemented a server for the
[Language Server Protocol (LSP)](https://microsoft.github.io/language-server-protocol/).
Which supports TableGen and
[2 other languages](https://mlir.llvm.org/docs/Tools/MLIRLSP/) used within MLIR.

The language server protocol is an editor agnostic means of providing
information to an editor about the structure of a language and project. For
instance, where are the include files, where is the definition of this type? If
you have used an editor like Visual Studio Code, you’ve probably used this
without knowing it. “Go To Definition” is the classic use case.

It means you can open a project, go to the code you want to change and jump
directly to the relevant parts of the repository. With 500,000+ lines of
TableGen in the LLVM Project, that’s a lot of code you get to ignore!

# Setup

You will need a copy of the server binary `tblgen-lsp-server`. Which you can get
from the
[release package](https://github.com/llvm/llvm-project/releases) for your
platform, or you can build it yourself.

This is the minimal set of commands to build it yourself:
```
$ cmake -G Ninja ../llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="mlir"
$ ninja tblgen-lsp-server
```

You will find `tblgen-lsp-server` in `<build-dir>/bin/`.

The server reads a compilation database file, which is made for you when you
configure LLVM using CMake.

As long as your checkout of llvm-project includes
[this commit](https://github.com/llvm/llvm-project/commit/c4afeccdd235a282d200c450e06a730504a66a08)
the compilation database will include all TableGen files from all enabled
projects (previously it was MLIR only).

For example this configure will include information about TableGen files from
llvm, clang, mlir and lldb:
```
$ cmake -G Ninja ../llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;llvm;lldb;mlir"
```

This also applies to `-DLLVM_TARGETS_TO_BUILD=`. Enabling only one target will
mean the compilation database only has files relevant to that target.

**Note:** You do not need to build a project to include its TableGen files in
the compilation database. Configuring is all that’s needed.

Next, install the MLIR
[extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-mlir)
into Visual Studio Code (the language server can work with other but setup will
differ).

Then follow the setup instructions
[here](https://mlir.llvm.org/docs/Tools/MLIRLSP/#td---tablegen-files) to tell
the extension where the server and compilation database are.

## Example

Assuming you configured LLVM with the AArch64 target enabled (it is on by
default).

* Open the file `llvm/lib/Target/AArch64/AArch64.td`.
* Put your cursor on a use of the `SubtargetFeature` type.
* In the top menus select "Go" then "Go to Definition".
* You will be taken to `llvm/include/llvm/Target/Target.td`, where
  `SubtargetFeature` is defined.

## Limitations

The language server highlights an anti-pattern in the way some LLVM targets
(e.g. AArch64) use TableGen.

You may find yourself in a file that uses a class but does not define it, or
include any files which do. This is because this file is intended to be included
in another file, which does include a definition of that class.

Perhaps we can address this by improving the language server, or reorganising
the includes so we do not have these seemingly isolated files.

# Dump

What about printf? The best debugging tool of them all.

TableGen’s equivalent is
[dump](https://llvm.org/docs/TableGen/ProgRef.html#dump-print-messages-to-stderr),
and its companion `repr`.
```
class Register<int _size, string _alias=""> {
   int size = _size;
   string alias = _alias;
   dump "size is " # !repr(size) # " and alias is " # !repr(alias);
}

def X0: Register<8> {}
def X29: Register<8, "frame pointer"> {}
```

`dump` prints to `stderr`:
```
<source>:4:5: note: size is 8 and alias is ""
   dump "size is " # !repr(size) # " and alias is " # !repr(alias);
   ^
<source>:4:5: note: size is 8 and alias is "frame pointer"
   dump "size is " # !repr(size) # " and alias is " # !repr(alias);
   ^
```

Note: This was added
[recently](https://github.com/llvm/llvm-project/commit/411c4edeef076bd2e01b104fe095ba381600a3d3)
by Francesco Petrogalli and Adam Nemet. So you will need a recent build, or a
released version 18.0 or greater (which is unreleased at time of writing).

Of course you can try this
[on Compiler Explorer](https://godbolt.org/z/555n19qMK) right now!

# Find In Files

This is last because in an ideal world it would be the last option, but it’s
often not the least of them. Grep, ack, Find In Files, whatever you call it,
searching text is unreasonably effective if you have a little knowledge of the
language syntax.

Should I even mention such an obvious idea? Well obvious is in the eye of the
beholder, and for TableGen we have a special circumstance that makes it more
effective than usual.

In the LLVM project repo we have the vast majority of TableGen code in use today.
Want to know how to use a feature? It’s all there, albeit in a 500,000+ line
haystack, but you’d be surprised what a simple query can find.

Think about how you would write the thing you’re trying to find. If it’s a
class would it have template arguments or not and so would there be a `<` after
the name? If it’s an error message, what parts would be constant and what parts
substituted?

`class Foo has no attribute Bar` is much more likely to be created by
substituting the name of the class and attribute whereas `Expected end of line`
is likely a static string.

There are also tests for the compiler, most of which are in
[this folder](https://github.com/llvm/llvm-project/tree/main/llvm/test/TableGen).
Where you will find minimal examples for the language features. You could
narrow your search to that location.

# Conclusion

Learning TableGen doesn’t have to be scary. Don’t think that because it’s an
isolated DSL that it doesn’t have what you’ve come to expect from your
favourite languages.

Keep in mind that TableGen is also a tool, not a goal unto itself. If you can
achieve your goals with a limited but accurate understanding of TableGen and its
backends, that’s fine. Dive as deep as you need or want to.

On top of the tools, there’s an active community ready to answer your questions
on [Discord](https://discord.com/invite/xS7Z362) or the
[forums](https://discourse.llvm.org/).

If you find problems or want to contribute improvements please do so by opening
a GitHub [Issue](https://github.com/llvm/llvm-project/issues) or
[Pull Request](https://llvm.org/docs/Contributing.html).

Look at the other languages you use. Do they have these tools? Should they? They
might be the difference between frustration and your new favourite language.
