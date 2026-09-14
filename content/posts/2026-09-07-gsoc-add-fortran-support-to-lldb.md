---
author: "Iason Karaprodromidis"
date: "2026-09-07"
tags: ["GSoC", "lldb", "fortran", "debugging"]
title: "GSoC 2026 - Adding Fortran Support to LLDB"
---

# GSoC 2026 - Adding Fortran Support to LLDB (LLVM)

While Flang has matured, the developer tooling around it has lagged behind. In particular it lacks a dedicated debugger. LLDB lacks native Fortran support, it is only able to resolve basic breakpoints via raw line tables. This project adds support for Fortran in LLDB and lays the foundations on which a fully-featured Flang-backed debugger can be built.

Specifically, this work introduces native support for:
- Primitive types (`INTEGER`, `REAL`, `LOGICAL`, `COMPLEX`)
- Arrays
- Pointers
- Functions

## Current Status & Running the Code

Because the upstreaming process for a project of this scale is ongoing, not all features have been merged into the LLVM main branch yet. However, the complete, working implementation is available for anyone to test.

You can view the full set of changes, clone the code, and compile it yourself from my [personal branch](https://github.com/Iasonaskrpr/llvm-project/tree/Fortran-support-stable-branch).

To keep up to date with the ongoing integration of this project, you can follow the [tracking issue](https://github.com/llvm/llvm-project/issues/109119).

## High-Level Architecture

Adding a new language to LLDB requires teaching it how to extract, represent, and display the values from compiled binaries. This relies on three core components:

* **`DWARFASTParser`:** Parses compiler-generated DWARF debug information and extracts raw type metadata (e.g., symbol names, byte sizes, and layouts).
* **`TypeSystem`:** Represents the internal type model of the language within LLDB. It takes the metadata parsed from DWARF and constructs native LLDB type representations.
* **`Language Plugin`:** Registers Fortran with LLDB and supplies language-specific pretty-printers, value formatters, and synthetic children providers.

For more details on how to add a new language to LLDB, see the official [documentation](https://lldb.llvm.org/resources/addinglanguagesupport.html).

## RFC process 

When proposing any significant change to LLVM, it first needs to go through the [RFC process](https://llvm.org/docs/RFCProcess.html). In short, a proposal is written providing an overview of the change, the motivation behind it, the overall architecture of what will be changed, and open questions. It then receives feedback, and if consensus is reached, the change is accepted and can be upstreamed. 

Reaching consensus on [my RFC](https://discourse.llvm.org/t/rfc-add-native-support-for-fortran-in-lldb/91034) took approximately two months since it was a major new feature, but it taught me a lot about how to communicate an idea and interact with other developers. 

The most important decisions we reached in the RFC were:
- Figuring out where and how testing would be performed, including setting up buildbots (x86_64 / AArch64) to prevent bit-rot.
- Expression evaluation will eventually be supported using Flang, but later in the project's lifetime and outside the scope of GSoC.
- LLDB will not link directly to Flang at this stage due to Flang's demanding build requirements. For now, only a Fortran compiler is needed to run tests.
- Array support will be added using synthetic children, rather than making intrusive changes to LLDB's core infrastructure.
- Language-specific details like Fortran's case insensitivity will be handled inside the plugin via DWARF attributes (`DW_AT_identifier_case`) without polluting core LLDB.

## Testing and Upstreaming

For a large part of GSoC, development took place on my [personal LLVM fork](https://github.com/Iasonaskrpr/llvm-project) while waiting for community consensus on the RFC. Once consensus was reached, the focus shifted to breaking the work into small, incrementally testable pull requests that were easy for maintainers to review.

To ensure long-term stability without regressions, the implementation leverages LLDB's [lit-based testing infrastructure](https://llvm.org/docs/CommandGuide/lit.html) across two distinct levels:

* **Unit Tests (C++):** Added three test suites to test core components in isolation:
  * `TypeSystemFortranTests`: Verifies that `TypeSystemFortran` correctly instantiates, constructs, and queries Fortran types.
  * `DWARFASTParserFortranTests`: Tests that raw DWARF debug info is parsed accurately into internal types.
  * `FortranLanguageTests`: Validates language plugin registration and synthetic child provider hooks.

* **API Tests (Python / SB API):** End-to-end integration tests using LLDB’s [Scripting Bridge API](https://lldb.llvm.org/resources/sbapi.html). These compile sample Fortran programs, launch debug sessions, and verify that the plugin, DWARF parser, and type system communicate properly to inspect variables and print output.

For more details on LLDB testing standards, see the official [testing documentation](https://lldb.llvm.org/resources/test.html).
 
## Features added

### 1. Laying the Foundation

For API tests, LLDB needs to compile sample Fortran programs. This patch updated LLDB's test runner to automatically detect an installed Fortran compiler and conditionally enable the Fortran test suite.
[PR #208298](https://github.com/llvm/llvm-project/pull/208298)

Before adding actual language features, LLDB requires substantial boilerplate to satisfy its internal interfaces, specifically the `TypeSystem`, which demands numerous stub methods just to compile. To keep patches incremental and easy to review, this initial scaffolding was split into three minimal PRs that contain no active feature logic (except for registering the language plugin itself):

* **Language Plugin Registration:** Registers Fortran as a supported language within LLDB.  
  [PR #218014](https://github.com/llvm/llvm-project/pull/218014)
* **TypeSystem Stubs (`TypeSystemFortran`):** Adds the class skeleton and required interface overrides.  
  [PR #218016](https://github.com/llvm/llvm-project/pull/218016)
* **DWARF Parser Stubs (`DWARFASTParserFortran`):** Adds the parser skeleton to prepare for DWARF type parsing.  
  [PR #218024](https://github.com/llvm/llvm-project/pull/218024)

### 2. Primitive Types

Primitive types in Fortran consist of the INTEGER, REAL, LOGICAL and COMPLEX types and they are all described by the `DW_TAG_base_type` DWARF tag. 

#### Type System Implementation

`TypeSystemFortran` manages the lifetime and representation of all Fortran types in LLDB:

* **Memory Management & Deduplication:** For memory management without leaks, types are stored as unique pointers in a vector. To avoid redundant allocations, an LLVM `FoldingSet` uniques types so existing instances are reused whenever a matching type is requested.
* **Primitive Type Representation:** To represent basic primitive types (`INTEGER`, `REAL`, `LOGICAL`, `COMPLEX`), the type system tracks three essential properties:
  - Type name
  - Byte size
  - Type kind

[Add support for basic types in TypeSystemFortran PR #218269](https://github.com/llvm/llvm-project/pull/218269)

#### DWARFASTParser Implementation

`DWARFASTParserFortran` bridges the raw debug symbols generated by the compiler and LLDB's internal type system:

* **Tag Recognition:** Parses the `DW_TAG_base_type` and its children, collecting information about the type.
* **Scope Extraction:** Calculates the scope/context of the type.
* **Type Construction:** Forwards the extracted information to `TypeSystemFortran` to instantiate and cache the corresponding type.

### 3. Functions & Subroutines

#### Type System Implementation (Functions & Subroutines)

To represent Fortran functions and subroutines, `TypeSystemFortran` tracks three key elements:

* **Parameters:** Stores the name and type of each parameter, which are used to construct the function signature and resolve argument inspection at runtime.
* **Name:** Receives the base identifier from the DWARF parser and formats the complete Fortran signature (e.g., `INTEGER sample(INTEGER arg1)`).
* **Return Type:** Tracks the return type for functions (or absence thereof for subroutines) to build the signature and provide accurate type info.

#### DWARFASTParser Implementation (Functions & Subroutines)

When encountering a Fortran function or subroutine, `DWARFASTParserFortran` handles two main tasks:

* **Parsing Subprogram Information:** Identifies `DW_TAG_subprogram` (and Fortran subroutines) and extracts the symbol name, parameter metadata, and return type, passing them to `TypeSystemFortran` to instantiate the function type.
* **Calculating Frame Bounds:** Determines the start and end memory addresses (low/high PC) of the subprogram. This address range is essential for stack frame unwinding and allows `frame variable` to correctly locate and display local variables in the current scope.

### 4. Variable Printing

For variable values to be printed correctly, the TypeSystem provides information about how they should be displayed, such as their encoding and formatting, along with implementing `DumpTypeValue` to format and dump the value to a stream. 

With this addition, `frame variable` can print variables in a predictable way:

```text
(lldb) frame variable num_real
(REAL) num_real = 2.71828175

(lldb) frame variable num_int
(INTEGER) num_int = 152

(lldb) frame variable num_logical
(LOGICAL) num_logical = true

(lldb) frame variable z1
# TypeSystemFortran formats complex as (real, imaginary) to match how Fortran prints them 
(COMPLEX(KIND=8)) z1 = (3, 4)
```

With this addition API tests were also added, since the output could not be tested before.

### 5. Pointers

Pointers assosciated with non-array types are similar to C-like languages. So to handle them all that is needed is the pointee type. 

#### Type System Implementation

The TypeSystem creates a pointer type from the pointee type. It can also provide the pointee type from the pointer type and vice-versa.

#### DWARFASTParser Implementation 

The `DW_TAG_pointer_type` has a reference to the pointee type as a child, so the pointee type is recursively parsed using this reference and is forwarded to the TypeSystem.

#### Output

```text
(lldb) frame variable target_val
(INTEGER) _QFEtarget_val = 42

(lldb) frame variable int_ptr
(INTEGER *) _QFEint_ptr = 0x000055555563d1e8

(lldb) frame variable &target_val
(INTEGER *) &_QFEtarget_val = 0x000055555563d1e8

(lldb) frame variable func_ptr
(INTEGER <unnamed function>(INTEGER * ) *) _QFEfunc_ptr = 0x0000555555556740

(lldb) image lookup -a 0x0000555555556740
      Address: pointers[0x0000000000002740] (pointers.PT_LOAD[1]..text + 896)
      Summary: pointers`square at pointers.f90:24
```

Note on output: Variables appear with a mangled name because LLDB does not have access to the Flang demangler.

### 6. Explicit Arrays

Arrays in Fortran can have custom bounds (e.g., indexing from `-1` instead of `1`) and custom byte strides (such as creating a slice consisting of every second element). In addition, Fortran arrays are column-major, unlike C/C++ which is row-major. For example, in a 2D array, elements of each column are stored in contiguous memory locations, whereas in C/C++ the elements of each row are contiguous. Furthermore, in Fortran, partial subscripting (such as indexing only one dimension of a multi-dimensional array) is invalid, unlike in C where it yields a pointer to a sub-array.

### The array type structure

Below is a representation of how arrays are stored in the TypeSystem:

```text
+-----------------------------------------------------------+
| ArrayType                                                 |
|   - Element Type : CompilerType (e.g., INTEGER)           |
|   - Metadata     : Flags, Rank                            |
|   - Dimensions   : vector<ArrayShape>                     |
+-----------------------------------------------------------+
                        |
                        v (Per Dimension)
+-----------------------------------------------------------+
| ArrayShape                                                |
|   - Byte Stride   : Memory step between elements          |
|   - Element Count : Extent of this dimension              |
|   - Lower Bound   : ArrayBound                            |
|   - Upper Bound   : ArrayBound                            |
+-----------------------------------------------------------+
                        |
                        v
+-----------------------------------------------------------+
| ArrayBound                                                |
|   - Kind  : Explicit | Star (*) | Colon(:)                |
|   - Value : Bound index                                   |
+-----------------------------------------------------------+
```

**Note:**  Star describes assumed-size arrays and colon describes allocatable arrays.

#### TypeSystem Implementation

When executing `frame variable` for an array, LLDB resolves subscripts iteratively: it first resolves the base array, processes the first subscript, then resolves subsequent subscripts based on the previous result. Consequently, when a user queries `arr[1]` on a multi-dimensional array, LLDB should return a valid sub-array rather than an error. Because Fortran arrays are column-major, the elements of this sub-array are not contiguous in memory, but are instead separated by a byte stride. 
To handle this, a new sub-array type is created which strips the outermost dimension and scales the byte stride based on the extent and stride of the peeled dimension. This intermediate type is then returned, enabling LLDB to display the slice or continue stripping dimensions for chained subscripts. 

The diagram below illustrates this resolution process:

```text
Logical 2D Fortran Array: A(1:3, 1:2)
       Col 1      Col 2
     +--------+ +--------+
Row 1| A(1,1) | | A(1,2) |
Row 2| A(2,1) | | A(2,2) |  <-- User queries intermediate slice: A[2]
Row 3| A(3,1) | | A(3,2) |
     +--------+ +--------+

Physical Memory Layout (Column-Major):
Offset:   0x00     0x04     0x08        0x0C     0x10     0x14
Bytes:   [ A(1,1) | A(2,1) | A(3,1) ]  [ A(1,2) | A(2,2) | A(3,2) ]
                   ^                             ^
                   |                             |
                   +------ stride = 3 * 4B ------+
                           (12 bytes)

Intermediate Sub-Array Type created by LLDB for A[2]:
- Base Offset : 0x04  (idx * sizeof(elem))
- Stride      : 12 B  (count(dim 1) * sizeof(elem))
- Elements    : [ A(2,1), A(2,2) ]
```


##### Assumed-Size Arrays (*)

Assumed-size arrays use an asterisk (*) for their final dimension's upper bound, leaving the total element count unknown in debug metadata. While handling follows the standard path, it differs in two ways:

* **Unbounded Indexing:** Upper-bound checks are bypassed, allowing direct element access (e.g., arr[10]) without failing on an unknown extent.

* **No Trailing Sub-Array Display:** Because the final dimension lacks a known element count, LLDB cannot calculate child boundaries, meaning intermediate slices cannot list or display the sub-arrays that follow.

* **Note:** While Fortran source syntax uses parentheses for multi-dimensional indexing (e.g., `A(row, col)`), LLDB's frame variable expression path parser currently only supports chained C-style bracket notation (`A[row][col]`).

#### DWARFASTParser Implementation

A `DW_TAG_array_type` tag is emitted in DWARF, with its children consisting of the element type as well as `DW_TAG_subrange_type` tags, one for each subrange, which can contain the following information:

- Lower and upper bound
- Element count
- Byte stride

This information is collected and forwarded to TypeSystemFortran to create the array type.

#### Output

```text
(lldb) frame variable a
(INTEGER(2, 2)) a = {
  [1] = ([1] = 1, [2] = 3)
  [2] = ([1] = 2, [2] = 4)
}

(lldb) frame variable a[1]
(INTEGER(2)) a[1] = ([1] = 1, [2] = 3)

(lldb) frame variable a[1][2]
(INTEGER) a[1][2] = 3

(lldb) frame variable arr
(INTEGER(3, *)) arr =

(lldb) frame variable arr[1]
(INTEGER(*)) arr[1] =

(lldb) frame variable arr[1][2]
(INTEGER) arr[1][2] = 12
```

### 7. Dynamic Arrays

Dynamic arrays in Fortran are described by an array descriptor, which holds information about the byte stride, lower/upper bounds, and each subrange's element count. Because this information can change at runtime, it must be evaluated dynamically at the point of inspection.

Fortran compilers expose this metadata by emitting [DWARF expressions](https://dwarfstd.org/doc/DWARF5.pdf#page=44), which evaluate the target memory state to calculate the runtime address and value of each attribute.

The primary architectural challenge was that LLDB as a whole assumes array geometry is static. Root structures like ValueObject and TypeSystem assume that while the values of array elements change at runtime, the structure of the array (such as its bounds and element count) does not, and if it does there is no way to know what that structure is. This model works for C, but breaks for Fortran, where the array's true shape and element count can change during execution and  the debugger can know what the array's structure is.

Rather than making intrusive changes to core LLDB interfaces, a provider for synthetic children was used. Its job is to read the runtime descriptor in target memory and instantiate a new, concrete type containing the active bounds and strides. This cleanly accommodates Fortran's dynamic layouts while keeping the TypeSystem isolated from runtime state.

#### Automatic-arrays

For arrays whose properties depend on runtime variables (such as bounds determined by function arguments), compilers emit a hidden variable in the DWARF metadata containing the required values. Because this information resides in runtime memory, it is resolved by the synthetic children provider using the same mechanism as array descriptors.

#### Assumed-rank arrays

These arrays appear in functions, allowing callers to pass arrays of varying dimensions to the same routine. For these arrays, the compiler emits a single `DW_TAG_generic_subrange` instead of one subrange tag per dimension, along with a rank attribute (`DW_AT_rank`) that provides the actual rank. Using the DWARF expressions found in the generic subrange's children, the runtime properties of each dimension are determined. This resolution is handled by the synthetic children provider.

#### TypeSystem Implementation

The required DWARF expressions are stored directly within the array type, ensuring the `TypeSystem` does not process arrays containing runtime information. During `frame variable` evaluation, LLDB first attempts to retrieve a child from the `TypeSystem`. If the static type cannot provide one, the request falls back to the synthetic children provider. The provider resolves the runtime metadata into a concrete type and passes that resolved type back to the `TypeSystem` for standard processing.

#### DWARFASTParser implementation

The DWARF information for arrays can either be a DWARF expression, a reference to a variable or a number. The DWARFASTParser has to figure out what type of array it is and activate the appropriate flags. Its job is to inform the TypeSystem what to expect and forward all information.


### 8. Case-insensitivity

Fortran is a case insensitive language and LLDB did not have support for performing case-insensitive lookups. Compilers emit a `DW_AT_identifier_case` attribute which gives information regarding the casing. Using this attribute LLDB sets the correct appropriate casing.
For mixed-language executables, where one language is case-insensitive and the other isn't, LLDB defaults to being case-sensitive. 

### Next steps

Unfortunately, arrays proved to be a lot more complex and time-consuming than we originally anticipated, so the scope of the project changed. The following features will be implemented in the future:
- Strings
- Derived types

As discussed in the RFC, the ideal end-point of this project is to have an expression evaluator using Flang, rather than relying only on the `frame variable` command (which uses a language-agnostic parser called [DIL](https://discourse.llvm.org/t/rfc-data-inspection-language/69893)).

### Biggest struggle

This was my first professional project and also my first time collaborating with many people. The hardest thing for me was learning how to communicate effectively and finding my voice in a large community. Thankfully, my mentors helped me a lot with this, enabling me to become much more confident in group discussions.

### Learnings

I learned a lot about navigating a large codebase. By the end of the project, I was completing tasks much faster and finding information more easily. Just as importantly, I realized how vital clear communication is in a big project, and how much difference a supportive, helpful community can make.

### Acknowledgements

I want to say a big thank you to my mentors, Shivam Gupta and Tarun Prabhu. They were immensely helpful throughout this project and taught me a lot. I also want to thank the LLVM community—especially the LLDB team—for taking the time to give me feedback on the RFC for this project and helping me along the way.
