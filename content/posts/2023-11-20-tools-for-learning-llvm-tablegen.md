---
author: "David Spickett"
date: "2023-11-20"
tags: ["TableGen"]
title: "Tools for Learning LLVM TableGen"
---

[TableGen](https://github.com/llvm/llvm-project/tree/main/llvm/utils/TableGen)
is a language used within the LLVM project for generating a variety of files,
when manual maintenance would be very difficult.

For example, it is used to define all of the instructions that can be used on a
particular architecture. The information is defined in TableGen and we can
produce many things based on that single source file. C++ code, documentation,
command line options, and so on.

TableGen has been in existence since
[before](https://github.com/llvm/llvm-project/commit/a6240f6b1a34f9238cbe8bc8c9b6376257236b0a)
the first official release of LLVM, over 20 years ago.

Today in the [LLVM project repository](https://github.com/llvm/llvm-project) there are
over a thousand TableGen source files totalling over 500,000 lines of code.
Making it the 5th most popular language in the repository.

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

With projects such as MLIR
[embracing TableGen](https://mlir.llvm.org/docs/DefiningDialects/Operations/)
it is only going to grow. So if you are contributing to LLVM, you will encounter
it at some point.

Which might be a problem as TableGen only exists within LLVM. Unlike a language
such as C++, TableGen does not have a large array of resources.

So, as well as joining a new project, you also now need to learn a new
Domain Specific Language (DSL). You did not come to LLVM to learn a DSL, you
probably came here to write a compiler.

I cannnot say when this problem might be solved, but the situation is not as
bleak as it appears. There have been big improvements in TableGen tools
recently, which means you can put more of your energy into the goals that
brought you to LLVM in the first place.

# A Brief Introduction to TableGen

Imagine you wanted to represent the registers of an architecture. I am going to
use Arm's AArch64 in particular here.

You could describe them in TableGen as:
```bash
$ cat register.td

class Register<int _size, string _alias=""> {
  int size = _size;
  string alias = _alias;
}

// 64 bit general purpose registers are X<N>.
def X0: Register<8> {}
// Some have special alternate names.
def X29: Register<8, "frame pointer"> {}
// Some registers omitted...
```

By default, the TableGen compiler `llvm-tblgen` creates “records” - which are
shown below.

```bash
$ ./bin/llvm-tblgen register.td

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

This is the intermediate representation (IR) of the TableGen compiler, similar
to LLVM's "LLVM IR".

When using LLVM you would select a "target" which is the processor architecture
you want to generate instructions for. TableGen's equivalent is a "backend".
These backends do not generate instructions, but instead output a format for
that backend's specific use case.

For example, there is a backend that generates C++ code for 
[searching](https://godbolt.org/z/5c696j1f9) data tables. Other examples are 
C header files and [reStructuredText](https://docutils.sourceforge.io/rst.html)
documentation.

```
                       TableGen source
                               |
 +--llvm-tblgen----------------|------------------------+
 |                             v                        |
 |              +----- Expanded records ----+           |
 |              |                           |           |
 |              v                           v           |
 | +-------------------------+    +-------------------+ |
 | | --gen-searchable-tables |    | Other backends... | |
 | +-------------------------+    +-------------------+ |
 |              |                           |           |
 +--------------|---------------------------|-----------+
                v                           v
    .inc file with C++ code       Other output formats...
    for table searching.
```

The main compiler is `llvm-tblgen`, but there are others specific to
sub-projects of LLVM. For example `clang-tblgen` and `lldb-tblgen`. The only
difference is the backends included in each one, the language is the same.

You might take your register definitions and produce C++ code to initialise them
in some kind of bootloader. Perhaps you also document it and produce a diagram
of the process. With enough backends, you could do all that from the same
TableGen source code.

You would write these backends either in C++ within the TableGen compiler,
or as an external backend using the compiler's
[JSON output](https://godbolt.org/z/vre845e77) (`--dump-json`). So you can use
any language with a JSON parser (such as
[Python](https://github.com/llvm/llvm-project/blob/main/llvm/utils/TableGen/jupyter/sql_query_backend.ipynb)).

# There is TableGen and There Are Things Built With TableGen

This is more a mindset than a tool. It is summed up best by a quote from the
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

This means that you will be tackling TableGen, and things built with TableGen.
Which are often more complicated than the language.

It is like learning C++ and struggling to use [Boost](https://www.boost.org/).
Someone might say to you, “Boost is not required, why not remove it and save
yourself the hassle?”. As someone new to C++, you might not be aware of the
boundary between the two of them.

Of course this does not help you too much if the project you want to contribute
to uses Boost. You are stuck dealing with both. In LLVM terms, the TableGen
language and the backends that consume it are a package deal.

I mention this so that you can draw a distinction between not understanding
one or the other. Knowing which one is confusing you is a big advantage
to finding help.

For any task there are probably one or two "things built with TableGen" that you
need to understand and even then, not entirely.

Do not think that your TableGen journey must end with understanding all the ways
it is used. That is possible, but it is not rquired, and hardly anyone learns
everything. Instead put your energy into the things that really interest you.

# Compiler Explorer

Of course we have TableGen in Compiler Explorer! Is a language even real if it is
not in Compiler Explorer?

(Of course it is, but if your favourite language is not there, Compiler Explorer
has [excellent documentation](https://github.com/compiler-explorer/compiler-explorer/blob/main/docs/AddingALanguage.md)
and friendly maintainers)

Compiler Explorer is a whole bunch of different versions of compilers for
different languages and different architectures that you can access with just a
browser tab.

It is an incredible tool for learning, teaching, triaging, optimising and
[many more](https://www.youtube.com/watch?v=O5sEug_iaf4) things. I will not go into
detail about it here, just a few things about TableGen's inclusion.

The obvious thing is that `llvm-tblgen` does not emit instructions (though a
hypothetical backend could) so there is no option to compile to binary or
execute code.

By default, records printed as plain text. You can choose a backend by adding a
compiler option, or by opening the "Overrides" menu and selecting an "Action".

It is important to note that TableGen backends have very specific expectations of
what will be in the source code. As if you had a C++ compiler that
would not compile for Arm unless it saw `arm_is_cool` somewhere in the
source code.

In the LLVM repository all the required classes are set up for you, but in
Compiler Explorer they are not. So, if you would like to experiment with an
existing backend, I suggest you provide stub implementations of the classes, or
copy some from the LLVM project repository. You can also use standard includes
from `include/llvm/*.td`.

It is not possible at this time to develop a backend within Compiler Explorer,
but you can select the JSON backend and copy that JSON to give to local scripts.

Multi-file projects (“IDE mode”) also work as expected, so, if you would like,
you can have your own [include files](https://godbolt.org/z/4qhdoaMjE).

Finally, remember that you can share Compiler Explorer examples. If you are
asking or answering questions about TableGen, always include a Compiler Explorer
link if you can!

# Jupyter Notebooks

[Jupyter](https://jupyter.org/) creates interactive notebooks. A notebook is a
single document which contains text, code and the results of running that code.
This enables you to edit the code and rerun it to update the results in the
notebook.

This is great for taking notes or building up large examples from small chunks
of code. You can export the document as a notebook that anyone can edit, or
in noninteractive formats such as PDF or Markdown.

TableGen can be used in notebooks by using the TableGen Jupyter Kernel.
Installation instructions are available [here](https://github.com/llvm/llvm-project/tree/main/llvm/utils/TableGen/jupyter) and you can watch me talk more about it
[here](https://www.youtube.com/watch?v=Gf0FUiY2TRo).

**Note:** There is also an 
[MLIR kernel](https://github.com/llvm/llvm-project/tree/main/mlir/utils/jupyter)
for Jupyter, along with many others.

We have aimed to give the same experience as other languages, so I will focus
not on how to use a notebook, but instead on what we have been able to make with
them.

## TableGen Tutorial Notebook

This notebook is an introduction to TableGen. You can read it on
[GitHub](https://github.com/llvm/llvm-project/blob/main/llvm/utils/TableGen/jupyter/tablegen_tutorial_part_1.ipynb),
or [download](https://raw.githubusercontent.com/llvm/llvm-project/main/llvm/utils/TableGen/jupyter/tablegen_tutorial_part_1.ipynb)
it and read it in Jupyter.

When using Jupyter, you can edit the document to add your own examples or expand
the ones that you find interesting.

## "How to Write a TableGen Backend" Notebook

This notebook uses Python instead of TableGen, and it shows you how to write a
backend.

The 2021 EU LLVM Developer's Meeting talk
[“How to write a TableGen backend”](https://www.youtube.com/watch?v=UP-LBRbvI_U)
by Min-Yih Hsu is the basis for this. The
[notebook](https://github.com/llvm/llvm-project/blob/main/llvm/utils/TableGen/jupyter/sql_query_backend.ipynb)
is in fact a Python port of Min's own
[C++](https://github.com/mshockwave/SQLGen) implementation.

It shows you how to take the JSON output of `llvm-tblgen` and process it with
Python to create SQL queries.

What is unique here is we now have the same content in multiple media forms and
multiple programming languages. Choose the ones that suit you best.

Referring back to “There is TableGen and There are Things Built With TableGen”
, the tutorial notebook is TableGen. The writing a backend notebook is “Things
Built With TableGen”.

## Limitations

The major limitation of the notebooks is that we have no output filtering. This
means if you do `include “llvm/Target/Target.td"` you will get about 320,000
lines of output (before you have added any of your own code). This is more than
a default notebook accepts from a kernel and when I removed that limit, the
browser tab crashed.

This is not a problem in most cases and the possible solutions have big
trade-offs, so we are not going to rush a fix. If it does affect you, please add your
feedback to the
[tracking issue](https://github.com/llvm/llvm-project/issues/72856).

# TableGen Language Server

The MLIR project has implemented a server for the
[Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
(LSP). Which supports TableGen and
[2 other languages](https://mlir.llvm.org/docs/Tools/MLIRLSP/) used within MLIR.

The language server protocol provides information to compatible editors about
the structure of a language and project. For example, where are the included
files? Where is the definition of a particular type?

If you have used a LSP compatible editor (such as Visual Studio Code), you have
probably used a language server without knowing. “Go To Definition” is the
most common feature they provide.

The Language Server Protocol allows you to open a project, go to the code you
want to change and jump from there directly to the other relevant parts of the
repository. With 500,000+ lines of TableGen in the LLVM project, that is a lot of
code you get to ignore!

# Setup

You will need a copy of the server binary `tblgen-lsp-server`. Which you can get
from the
[release package](https://github.com/llvm/llvm-project/releases) for your
platform, or you can build it yourself.

This is how to build it yourself:
```bash
$ cmake -G Ninja <path-to>/llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="mlir"
$ ninja tblgen-lsp-server
```

Having run those commands, `tblgen-lsp-server` is found in `<build-dir>/bin/`.

The server reads a compilation database file `tablegen_compile_commands.yml`,
which is made for you when you configure LLVM using CMake.

This serves a similar purpose to the `compile_commands.json` file generated when using
`CMAKE_EXPORT_COMPILE_COMMANDS`, but the two files are not related.

As long as your checkout of llvm-project includes
[this commit](https://github.com/llvm/llvm-project/commit/c4afeccdd235a282d200c450e06a730504a66a08)
the compilation database includes TableGen files from all enabled
projects (prior to that commit it was MLIR only).

For example this configure command includes information about TableGen files from the
LLVM, Clang, MLIR and LLDB subprojects:

```bash
$ cmake -G Ninja <path-to>/llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;llvm;lldb;mlir"
```

This also applies to `-DLLVM_TARGETS_TO_BUILD=`. Enabling only one target means
that the compilation database only has files relevant to that target.

**Note:** You do not need to build a project to include its TableGen files in
the compilation database. Configuring is all that is needed.

Next, install the MLIR
[extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-mlir)
into Visual Studio Code (the language server works with other editors but their
setup will be different).

Then follow the setup instructions
[here](https://mlir.llvm.org/docs/Tools/MLIRLSP/#td---tablegen-files) to tell
the extension where the server and compilation database are.

## Example

This example assumes you have configured LLVM with the `AArch64` target enabled.
(It is enabled by default)

* Open the file `llvm/lib/Target/AArch64/AArch64.td`.
* Put your cursor on a use of the `SubtargetFeature` type.
* In the menu bar, select "Go" then "Go to Definition".
* This takes you to `llvm/include/llvm/Target/Target.td`, where
  `SubtargetFeature` is defined.

## Limitations

The language server highlights an anti-pattern in the way some LLVM targets
such as AArch64 use TableGen.

You may find yourself in a file that uses a class but does not define it or
include any files which define it. This is because this file is intended to be
included in another file, which does include a definition of that class.

```
example.td:
  class Example {}

uses_example.td:
  def example: Example {}

main.td:
  include "example.td"
  include "uses_example.td"
```

The example above shows this anti-pattern:

* The file `example.td` defines the class `Example`.
* `uses_example.td` uses the class `Example`, but does not include `example.td`.
* `main.td` includes both `example.td` and `uses_example.td`.
* `main.td` is the file that is compiled.
* When you are in `uses_example.td`, the language server does not know where
  `Example` is defined,
* When you are in `main.td`, the language server does know where `Example` is
  defined.

Perhaps we can address this by improving the language server, or reorganising
the includes so we do not have files that appear to be isolated.

# Dump

What about `printf`? The best debugging tool of them all.

TableGen's equivalent is
[dump](https://llvm.org/docs/TableGen/ProgRef.html#dump-print-messages-to-stderr),
and its companion `repr`.
```
def op;
class A {
  string A = "some text";
  dag X =(op op);
}
def a : A;

dump "The Value of a is: \n" # !repr(a);
```

`dump` prints to `stderr`:
```
<source>:8:1: note: The Value of a is:
a {	// A
  string A = "some text";
  dag X = (op op);
}

dump "The Value of a is: \n" # !repr(a);
^
```

This was added
[recently](https://github.com/llvm/llvm-project/commit/411c4edeef076bd2e01b104fe095ba381600a3d3).
So you will need a recent build, or a released version 18.0 or newer (which is unreleased
at time of writing).

Of course you can try this
[on Compiler Explorer](https://godbolt.org/z/Ta6jb19hr) right now!

# Assertions

An assertion checks that a condition is true at a specific point in your
program. An assertion consists of:
* The keyword `assert`.
* A condition (usually a call to one of the
  [bang operators](https://llvm.org/docs/TableGen/ProgRef.html#bang-operators)).
* A message.

If the condition is false, a compiler error is generated with the message you
provided.

For example, the code below checks that you have not tried to make a register
with a size that is less than 0.

```
class Register<int _size> {
  assert !gt(_size, 0),
       "Register size must be > 0, not " # _size # "." ;
  int size = _size;
}

def X0: Register<8> {}
def X1: Register<-8> {}
```
([Try this on Compiler Explorer](https://godbolt.org/z/e4GzvhEeh))

The register `X0` has `_size=8`, so the condition `!gt(_size, 0)` (which would
be  `_size > 0` in C syntax) is true and therefore no error is generated.

The register `X1` has `_size=-8`, so the condition is false and an error is
generated. The compiler output is shown below:
```
<source>:2:11: error: assertion failed
   assert !gt(_size, 0),
          ^
note: Register size must be > 0, not -8.
```

While learning new code it is helpful to add your own assertions to check your
assumptions. In addition, adding assertions to code written to be used by other
people is a good way to stop them using it incorrectly. Unlike documentation,
you cannot miss an assertion error.

# Find In Files

This is last because in an ideal world it would be the last option, but it is
often not the least of the options. Grep, ack, Find In Files, whatever you call it,
searching text is unreasonably effective if you have a little knowledge of the
language syntax.

Why should I mention such an obvious idea? Well, obvious is subjective, and
there is a special situation that makes it more effective than usual.

In the LLVM project repository we have the vast majority of TableGen code in use today.
Would you like to know how to use a particular feature? It is all there,
somewhere in 500,000+ lines of source code. You would be surprised by what a
simple query can find despite that.

Think about the thing you are trying to find. What do you think its source
code would look like? If it is a class would it have template arguments or not
and so would there be a `<` after the name? If it is an error message, what parts
would be constant and what parts would be inserted into a template message?

`Expected end of line` is likely to be a static string so you can search for the
message itself. In contrast, `class Foo has no attribute Bar` is more likely to
be created by substituting in the name of the class and attribute. So a good
search term for this would be `has no attribute`. 

There are also tests for the compiler, most of which are in
[this folder](https://github.com/llvm/llvm-project/tree/main/llvm/test/TableGen).
This folder contains minimal examples for the language features. Try narrowing 
your search to this location.

# Conclusion

Learning TableGen does not have to be scary. Do not think that because it is an
isolated DSL that it does not have what you have come to expect from your
favourite languages.

Keep in mind that TableGen is also a tool, not a goal in itself. If you can
achieve your goals with a limited but accurate understanding of TableGen and its
backends, that is great. Learn as much as you want or need.

In addition to the tools, there is an active community ready to answer your
questions on [Discord](https://discord.com/invite/xS7Z362) or the
[forums](https://discourse.llvm.org/).

If you find problems or want to contribute improvements please do so. Open a
GitHub [Issue](https://github.com/llvm/llvm-project/issues) or
[Pull Request](https://llvm.org/docs/Contributing.html).

Look at the other languages you use. Do they have these tools? Should they? They
might be the difference between frustration and your new favourite language.

# Acknowledgements

TODO!
