---
author: "Narayan Sreekumar (vortex73)"
date: "2025-11-03"
tags: ["GSoC", "abi", "codegen", "sysv"]
title: "GSoC 2025: Introducing an ABI Lowering Library"
---

# Introduction

In this post I'm going to outline details about a new ABI lowering library I've been developing for LLVM as part of GSoC 2025! The aim was to extract the ABI logic from Clang and create a reusable library that any LLVM frontend can use for correct C interoperability.

# The Problem We're Solving

At the start of the program, I wrote about the [fundamental gap in LLVM's target abstraction](https://vortex73.github.io/rendered/GSOC_BLOG1.html). The promise is simple: frontends emit LLVM IR, and LLVM handles everything else. But this promise completely breaks down when it comes to Application Binary Interface (ABI) lowering. Every LLVM frontend that wants C interoperability has to reimplement thousands of lines of target-specific ABI logic.

Here's what that looks like in practice:
```cpp
struct Point { float x, y; };
struct Point add_points(struct Point a, struct Point b);
```

Seems innocent enough, right? But generating correct LLVM IR for this requires knowing:
- Are the struct arguments passed in registers or memory?
- If in registers, what register class is used?
- Are multiple values packed into a single register?
- Is the struct returned in registers or using a hidden return parameter?

The answer depends on subtle ABI rules that are target-specific, constantly evolving, and absolutely critical to get right. Miss one detail and you get silent memory corruption.

[This godbolt link](https://clang.godbolt.org/z/P4fMj7hjY) shows the same simple struct using six different calling conventions across six different targets. And crucially: a frontend generating IR needs to know ALL of this before it can emit the right function signature.

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

It contains **more information than LLVM IR types** (which for instance doesn't distinguish between `__int128` and `_BitInt(128)`, both just `i128`), but **less information than frontend types** like Clang's QualType (which carry parsing context, sugar, and other frontend-specific concerns that don't matter for calling conventions).
```cpp
class IntegerType : public Type {
private:
  bool IsSigned;
  bool IsBitInt;        // Crucially different from __int128!

public:
  IntegerType(uint64_t BitWidth, Align Align, bool Signed,
              bool BitInt = false);
};
```

## Frontend-to-ABI Mapping

The `QualTypeMapper` class handles the job of converting Clang frontend types to ABI types.

**The ABI library is primarily intended to handle the C ABI.** The C type system is relatively simple, and as such the type mapping from frontend types to ABI types is straightforward : integers map to `IntegerType`, pointers map to `PointerType`, and structs map to `StructType` with their fields and offsets preserved.

However, Clang also needs support for the C++ ABI, and the type mapping for this case is significantly more complicated. C++ object layout involves vtables, base class subobjects, virtual inheritance, and all sorts of edge cases that need to be preserved for correct ABI decisions. Here's an excerpt showing how `QualTypeMapper` tackles C++ inheritance:
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
Other frontends that only need C interoperability will have a much simpler mapping task.

## Target-Specific Classification

Each target implements the ABIInfo interface. I'll show the BPF implementation here since it's one of the simplest ABIs in LLVM, the classification logic fits in about 50 lines of code with straightforward rules: small aggregates go in registers, larger ones are passed indirectly.

Its worth noting that most real-world ABIs are not *this* simple - for instance targets like X86-64 are significantly more complex.
```cpp
class BPFABIInfo : public ABIInfo {
private:
  TypeBuilder &TB;

public:
  BPFABIInfo(TypeBuilder &TypeBuilder) : TB(TypeBuilder) {}

  ABIArgInfo classifyArgumentType(const Type *ArgTy) const {
    if (isAggregateTypeForABI(ArgTy)) {
      auto SizeInBits = ArgTy->getSizeInBits().getFixedValue();
      if (SizeInBits == 0)
        return ABIArgInfo::getIgnore();

      if (SizeInBits <= 128) {
        const Type *CoerceTy;
        if (SizeInBits <= 64) {
          auto AlignedBits = alignTo(SizeInBits, 8);
          CoerceTy = TB.getIntegerType(AlignedBits, Align(8), false);
        } else {
          const Type *RegTy = TB.getIntegerType(64, Align(8), false);
          CoerceTy = TB.getArrayType(RegTy, 2, 128);
        }
        return ABIArgInfo::getDirect(CoerceTy);
      }

      return ABIArgInfo::getIndirect(ArgTy->getAlignment().value());
    }

    if (const auto *IntTy = dyn_cast<IntegerType>(ArgTy)) {
      auto BitWidth = IntTy->getSizeInBits().getFixedValue();
      if (IntTy->isBitInt() && BitWidth > 128)
        return ABIArgInfo::getIndirect(ArgTy->getAlignment().value());

      if (isPromotableInteger(IntTy))
        return ABIArgInfo::getExtend(ArgTy);
    }
    return ABIArgInfo::getDirect();
  }
};
```

The key difference is that the ABI classification logic itself is **completely independent of Clang**. Any LLVM frontend can use it by implementing a mapper from their types to `llvm::abi::Type`. The library then performs ABI classification and outputs `llvm::abi::ABIFunctionInfo` with all the lowering decisions.

For Clang specifically, the `ABITypeMapper` converts those `llvm::abi::Type` results back into `llvm::Type` and populates `clang::CGFunctionInfo`, which then continues through the normal IR generation pipeline.
# Results
The library and the new type system are implemented and working in the [PR #140112](https://github.com/llvm/llvm-project/pull/140112), currently enabled for BPF and X86-64 Linux targets. You can find the implementation under `llvm/lib/ABI/` with Clang integration in `clang/lib/CodeGen/CGCall.cpp`. Here's what we've achieved so far:

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

The results are encouraging. Most compilation stages show essentially no performance difference (well within measurement noise). The 0.20% regression in the final Clang build times is expected - we've added new code to the codebase. But the actual compilation performance impact is negligible.

# Future Work

There's still plenty to explore:

## Upstreaming the progress so far...

The work is being upstreamed to LLVM in stages, starting with [PR #158329](https://github.com/llvm/llvm-project/pull/158329). This involves addressing reviewer feedback, ensuring compatibility with existing code, and validating that the new system produces identical results to the current implementation for all supported targets.

## Extended Target Support

Currently the ABI library supports the BPF and X86-64 SysV ABIs, but the architecture makes adding ARM, Windows calling conventions, and other targets straightforward.

## Cross-Frontend Compatibility

The real test will be when other frontends start using the library. We need to ensure that all frontends generate identical calling conventions for the same C function signature.


## Better Integration

There are still some rough edges in the Clang integration that could be smoothed out. And other LLVM projects could benefit from adopting the library.

# Acknowledgements

This work wouldn't have been possible without my amazing mentors, Nikita Popov and Maksim Levental, who provided invaluable guidance throughout the project. The LLVM community's feedback on the [original RFC](https://discourse.llvm.org/t/rfc-an-abi-lowering-library-for-llvm/84495) was instrumental in shaping the design.

Special thanks to everyone who reviewed the code, provided feedback, and helped navigate all the ABI corner cases. The architecture only works because it's built on decades of accumulated ABI knowledge that was already present in LLVM and Clang.

Looking back at my precursor blog post from earlier this year, I'm amazed at how much the design evolved during implementation. What started as a relatively straightforward "extract Clang's ABI code" became a much more ambitious architectural rework. But the result is something that's genuinely useful for the entire LLVM ecosystem.
