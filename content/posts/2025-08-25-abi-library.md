---
author: "Narayan Sreekumar (vortex73)"
date: "2025-08-25"
tags: ["GSoC", "abi", "codegen", "sysv"]
title: "GSoC 2025: Introducing an ABI Lowering Library"
---

# Introduction

In this post I'm going to outline details about a new ABI lowering library I've been developing for LLVM as part of GSoC 2025!. The aim was to extract the ABI logic from clang and create a reusable library that any LLVM frontend can use for correct C interoperability.

# Get the Code
Let's get the obvious out of the way first. You can find the implementation in the LLVM repository under `llvm/lib/ABI/` with Clang integration in `clang/lib/CodeGen/CGCall.cpp`. A list of all the major components can be found scattered across multiple files in [this PR](https://github.com/llvm/llvm-project/pull/140112).

# The Problem We're Solving

At the start of the program, I wrote about the [fundamental gap in LLVM's target abstraction](https://vortex73.github.io/rendered/GSOC_BLOG1.html). The promise is simple: frontends emit LLVM IR, and LLVM handles everything else. But this promise completely breaks down when it comes to Application Binary Interface (ABI) lowering. Every LLVM frontend that wants C interoperability has to reimplement thousands of lines of target-specific ABI logic.

Here's what that looks like in practice:
```cpp
struct Point { double x, y; };
struct Point add_points(struct Point a, struct Point b);
```
Seems innocent enough, right? But generating correct LLVM IR for this requires knowing:

- Does x86-64 pass this in registers or memory?
- What about ARM? PowerPC? WebAssembly?
- Should it be scalarized to `(double, double, double, double)`?
- Or does it need a hidden return parameter?

The answer depends on subtle ABI rules that are target-specific, constantly evolving, and absolutely critical to get right. Miss one detail and you get silent memory corruption that only shows up in release builds.

As I outlined in my earlier blog post, LLVM's type system simply can't express all the information needed for correct ABI decisions. Two otherwise identical structs with different explicit alignment attributes have different ABIs. `__int128` and `_BitInt(128)` look similar but follow completely different rules.

# The Design

<div style="margin:0 auto;">
  <img src="/img/abi_flow.png"><br/>
</div>

## Independent ABI Type System

At the heart of the library is `llvm::abi::Type`, a type system designed specifically for ABI decisions:

```cpp
class Type {
protected:
  TypeKind Kind;
  TypeSize SizeInBits;
  Align ABIAlignment;

public:
  TypeKind getKind() const { return Kind; }
  TypeSize getSizeInBits() const { return SizeInBits; }
  Align getAlignment() const { return ABIAlignment; }

  bool isInteger() const { return Kind == TypeKind::Integer; }
  bool isStruct() const { return Kind == TypeKind::Struct; }
  // ... other predicates that matter for ABI
};
```
This isn't just another type system, in that, it's carefully designed to capture exactly the information that ABI rules care about:

```cpp
class IntegerType : public Type {
private:
  bool IsSigned;
  bool IsBoolean;
  bool IsBitInt;        // Crucially different from __int128!
  bool IsPromotable;    // For C integer promotion rules

public:
  IntegerType(uint64_t BitWidth, Align Align, bool Signed,
              bool IsBool = false, bool BitInt = false,
              bool IsPromotableInt = false);
};
```

## Frontend-to-ABI Mapping

The QualTypeMapper class handles the complex job of converting frontend types to ABI types. Here's how it tackles C++ inheritance:

```cpp
const llvm::abi::StructType *
QualTypeMapper::convertCXXRecordType(const CXXRecordDecl *RD,
                                     bool canPassInRegs) {
  const ASTRecordLayout &Layout = ASTCtx.getASTRecordLayout(RD);
  SmallVector<llvm::abi::FieldInfo, 16> Fields;
  SmallVector<llvm::abi::FieldInfo, 8> BaseClasses;
  SmallVector<llvm::abi::FieldInfo, 8> VirtualBaseClasses;

  // Handle vtable pointer for polymorphic classes
  if (RD->isPolymorphic()) {
    const llvm::abi::Type *VtablePointer =
        createPointerTypeForPointee(ASTCtx.VoidPtrTy);
    Fields.emplace_back(VtablePointer, 0);
  }

  // Process base classes with proper offset calculation
  for (const auto &Base : RD->bases()) {
    const llvm::abi::Type *BaseType = convertType(Base.getType());
    uint64_t BaseOffset = Layout.getBaseClassOffset(
        Base.getType()->castAs<RecordType>()->getAsCXXRecordDecl()
    ).getQuantity() * 8;

    if (Base.isVirtual())
      VirtualBaseClasses.emplace_back(BaseType, BaseOffset);
    else
      BaseClasses.emplace_back(BaseType, BaseOffset);
  }

  // ... field processing and final struct creation
}
```

This was significantly more complex than I initially anticipated. C++ object layout involves vtables, base class subobjects, virtual inheritance, and all sorts of edge cases that need to be preserved for correct ABI decisions.

## Target-Specific Classification

Each target implements the ABIInfo interface. Here's the complete BPF implementation:

```cpp
class BPFABIInfo : public ABIInfo {
private:
  TypeBuilder &TB;

public:
  ABIArgInfo classifyArgumentType(const Type *ArgTy) const {
    if (isAggregateType(ArgTy)) {
      auto SizeInBits = ArgTy->getSizeInBits().getFixedValue();
      if (SizeInBits == 0)
        return ABIArgInfo::getIgnore();

      if (SizeInBits <= 128) {
        const Type *CoerceTy;
        if (SizeInBits <= 64) {
          auto AlignedBits = alignTo(SizeInBits, 8);
          CoerceTy = TB.getIntegerType(AlignedBits, Align(8), false);
        } else {
          // Two 64-bit registers for larger aggregates
          const Type *RegTy = TB.getIntegerType(64, Align(8), false);
          CoerceTy = TB.getArrayType(RegTy, 2);
        }
        return ABIArgInfo::getDirect(CoerceTy);
      }

      return ABIArgInfo::getIndirect(ArgTy->getAlignment().value());
    }

    // Handle integer promotion elegantly
    if (const auto *IntTy = dyn_cast<IntegerType>(ArgTy)) {
      if (IntTy->isPromotableIntegerType())
        return ABIArgInfo::getExtend(ArgTy);
    }

    return ABIArgInfo::getDirect();
  }
};

```
Compare this to the old approach where BPF ABI logic would be scattered across multiple files, mixed with Clang-specific assumptions!

# Results

The library and the new typesystem are now successfully integrated into Clang, as part of the PR, and are enabled for BPF and X86-64 Linux targets. Here's what we achieved:
 ## Clean Architecture

The three-layer separation is working beautifully. Frontend concerns, ABI classification, and IR generation are now properly separated:

```cpp
// Integration point in Clang
if (CGM.shouldUseLLVMABI()) {
  SmallVector<const llvm::abi::Type *, 8> MappedArgTypes;
  for (CanQualType ArgType : argTypes)
    MappedArgTypes.push_back(getMapper().convertType(ArgType));

  tempFI.reset(llvm::abi::ABIFunctionInfo::create(
      CC, getMapper().convertType(resultType), MappedArgTypes));

  CGM.fetchABIInfo(getTypeBuilder()).computeInfo(*tempFI);
} else {
  CGM.getABIInfo().computeInfo(*FI);  // Legacy path
}
```

## Performance Considerations Addressed

My earlier blog post worried about the overhead of "an additional type system." The caching strategy handles this elegantly:

```cpp
const llvm::abi::Type *QualTypeMapper::convertType(QualType QT) {
  QT = QT.getCanonicalType().getUnqualifiedType();

  auto It = TypeCache.find(QT);
  if (It != TypeCache.end())
    return It->second;  // Cache hit - no recomputation

  const llvm::abi::Type *Result = /* conversion logic */;

  if (Result)
    TypeCache[QT] = Result;
  return Result;
}
```

Combined with `BumpPtrAllocator` for type storage, the performance impact is minimal in practice.

<div style="margin:0 auto;">
  <img src="/img/abi_library_benchmarks.png"><br/>
</div>

The results are encouraging. Most compilation stages show essentially no performance difference (well within measurement noise). The 0.20% regression in the final Clang binary size is expected - we've added new code to the codebase. But the actual compilation performance impact is negligible.

# Future Work

There's still plenty to explore:

## Extended Target Support

Currently supporting BPF and X86-64 SysV, but the architecture makes adding ARM, Windows calling conventions, and other targets straightforward.

## Cross-Frontend Compatibility

The real test will be when other frontends start using the library. We need to ensure that all frontends generate identical calling conventions for the same C function signature.


## Better Integration

There are still some rough edges in the Clang integration that could be smoothed out. And other LLVM projects could benefit from adopting the library.

# Acknowledgements

This work wouldn't have been possible without my amazing mentors, Nikita Popov and Maksim Levental, who provided invaluable guidance throughout the project. The LLVM community's feedback on the [original RFC](https://discourse.llvm.org/t/rfc-an-abi-lowering-library-for-llvm/84495) was instrumental in shaping the design.

Special thanks to everyone who reviewed the code, provided feedback, and helped navigate all the ABI corner cases. The architecture only works because it's built on decades of accumulated ABI knowledge that was already present in LLVM and Clang.

Looking back at my precursor blog post from earlier this year, I'm amazed at how much the design evolved during implementation. What started as a relatively straightforward "extract Clang's ABI code" became a much more ambitious architectural rework. But the result is something that's genuinely useful for the entire LLVM ecosystem.
