---
title: "GSoC 2026: Improving HLSL Support in clangd"
author: "Maria Fernanda Guimarães"
Mentors: Finn Plummer, Ashley Coleman
tags: ["GSoC", "clangd", "HLSL", "Clang"]
---

# GSoC 2026: Improving HLSL Support in clangd

HLSL (High-Level Shading Language) is a C++-like language used for GPU shader programming. Clang already parses HLSL and represents many of its constructs in the AST. `clangd`, however, reuses its C/C++ infrastructure to provide IDE features such as hover, code completion, diagnostics, and navigation. Clang may fully understand an HLSL construct, while `clangd` does not necessarily know how to expose that information to the user.

For example, HLSL has language-specific constructs such as semantic annotations, resource bindings, `out`/`inout` parameters, shader attributes, and vector and matrix swizzles. These constructs do not always have direct equivalents in standard C++, so the existing `clangd` infrastructure does not automatically provide the expected editor experience.

The goal of my Google Summer of Code 2026 project was to identify these gaps and improve HLSL support in `clangd`.

I first wanted to understand the current state of HLSL support, identify where the problems were coming from, and determine which solutions would fit best into the existing Clang and `clangd` architecture. The project followed this process:

```text
Gap Analysis
      |
      v
Investigation
      |
      v
Implementation Ideas
      |
      v
     RFC
      |
      v
Community Feedback
      |
      v
GitHub Issues
      |
      v
Pull Requests
```

## Finding the Gaps

The first phase of the project was a to do a gap analysis.

I created isolated HLSL test cases covering different categories of language constructs and tested them using several clangd features: hover, code completion, go-to-definition, and diagnostics.

When the behavior was unclear, I also inspected Clang's AST to determine whether the required information was already available to clangd.

The goal was to distinguish between different kinds of problems. Some constructs were already represented correctly in the AST but were not exposed by clangd. Others involved configuration, while some problems originated earlier in the compiler pipeline. This distinction was important because the solution depends heavily on where the gap actually exists.

For example, if Clang already has all the information needed in the AST, adding another HLSL-specific parser or compiler feature would be unnecessary. Instead, the appropriate solution may be to extend existing clangd infrastructure.

Full writeup: **[Gap Analysis](https://github.com/mafeguimaraes/hlsl-gap-analysis/blob/main/gap-analysis.md)**

## From Gaps to Solutions

After identifying the gaps, I investigated possible implementation approaches for each one. For every issue, I looked at:

* where the relevant information was represented in Clang;
* how clangd currently handled similar C/C++ constructs;
* why the existing infrastructure did not work for HLSL;
* whether the problem should be fixed in Clang or clangd;
* and what possible implementation approaches were available.

I documented these ideas before starting the implementation so the design could be discussed with my mentors and the LLVM community.

Full writeup: **[Implementation Ideas](https://github.com/mafeguimaraes/hlsl-gap-analysis/blob/main/implementation-ideias.md)**


## RFC and Community Feedback

After discussing the implementation ideas with my mentors, I consolidated the proposed solutions into an RFC and submitted it to the LLVM Discourse community.

**[RFC: Improving HLSL Support in clangd](https://discourse.llvm.org/t/rfc-improving-hlsl-support-in-clangd/91359)**

The RFC described the expected behavior, the observed gap, the root cause, and the proposed solution for each issue.

The discussion with the LLVM and DirectX communities was an important part of the project. Some of my initial ideas were refined based on the feedback I received.

For example, semantic completion after `:` initially considered suggesting HLSL semantics unconditionally. The discussion highlighted that not every identifier in that context is necessarily a semantic, so the implementation was refined to make semantic suggestions dependent on the typed prefix.

Another example was vector and matrix swizzle completion. The discussion raised concerns about automatically triggering completion after every `.`, which could become distracting during normal editing. The final implementation therefore made HLSL swizzle completion configurable.

## From RFC to Issues and Pull Requests

After the RFC discussion, I created a separate GitHub issue for each implementation item. Each issue documented the expected behavior, the current behavior, the root cause, and the proposed solution.

The 11 issues were divided into two main areas.

**Hover**
* Semantic annotations such as `SV_Target` and `SV_Position` - [issue #214808](https://github.com/llvm/llvm-project/issues/214808) / [PR #217725](https://github.com/llvm/llvm-project/pull/217725)
* Loop and branch control attributes such as `[unroll]` and `[loop]` - [issue #214067](https://github.com/llvm/llvm-project/issues/214067) / [PR #214318](https://github.com/llvm/llvm-project/pull/214318)
* `out` and `inout` parameter qualifiers - [issue #214071](https://github.com/llvm/llvm-project/issues/214071) / [PR #214883](https://github.com/llvm/llvm-project/pull/214883)
* Vector swizzle and matrix element access - [issue #212612](https://github.com/llvm/llvm-project/issues/212612) / [PR #212741](https://github.com/llvm/llvm-project/pull/212741)
* RootSignature hover - [issue #214790](https://github.com/llvm/llvm-project/issues/214790) / [PR #214955](https://github.com/llvm/llvm-project/pull/214955)
* `register(...)` hover ranges - [issue #212749](https://github.com/llvm/llvm-project/issues/212749) / [PR #212881](https://github.com/llvm/llvm-project/pull/212881)

**Code Completion**
* Attribute completion inside `[...]` - [issue #214792](https://github.com/llvm/llvm-project/issues/214792) / [PR #215353](https://github.com/llvm/llvm-project/pull/215353)
* `register(...)` completion - [issue #214798](https://github.com/llvm/llvm-project/issues/214798) / Draft PR (Depend on PR not merged yet)
* HLSL annotations after `:` - [issue #214801](https://github.com/llvm/llvm-project/issues/214801) / Draft PR (Depend on PR not merged yet)
* Vector swizzle completion - [issue #214804](https://github.com/llvm/llvm-project/issues/214804) / [PR #217381](https://github.com/llvm/llvm-project/pull/217381)
* Matrix swizzle completion - [issue #214805](https://github.com/llvm/llvm-project/issues/214805) /  Draft PR (Depend on PR not merged yet)

Each issue was then implemented as an independent pull request, making the changes easier to review and upstream.

All PRs: **[link](https://github.com/llvm/llvm-project/issues?q=is%3Apr%20author%3Amafeguimaraes%20label%3Aclangd)**

## Some Examples

One of the most interesting parts of the project was discovering that HLSL constructs are sometimes represented internally in ways that are different from how users wrote them.

For example, HLSL `out` and `inout` parameters are lowered internally to C++ reference types. Given:

```hlsl
void ApplyFog(out float4 color, inout float depth);
```

hovering over `color` previously exposed an internal representation such as `float &__restrict` instead of the HLSL type `out float4`.

The solution was to add a `getHLSLParamTypeAsWritten()` helper that allows clangd to recover the original HLSL type for display, independently of how the parameter is represented internally.

Another example was HLSL semantic annotations. Constructs such as:

```hlsl
float4 main() : SV_Target
```

were already represented by Clang, but hover did not display the semantic spelling correctly. Investigating the AST showed that the necessary information existed, but the relevant HLSL attribute did not provide the spelling expected by the generic hover infrastructure. This allowed the problem to be fixed at the appropriate layer rather than by adding an HLSL-specific workaround to the hover implementation.

## Multiple Entry Points

In addition to the original gaps, I investigated a larger architectural problem involving HLSL files containing multiple shader entry points. A single HLSL file can contain multiple entry points, for example:

```hlsl
[shader("vertex")]
void VSMain(...)
{
    ...
}

[shader("pixel")]
void PSMain(...)
{
    ...
}
```

This creates a challenge for clangd, which traditionally works with a single active compilation context for a file. Different HLSL entry points can require different compilation contexts, especially in legacy HLSL, where the shader stage and entry point are specified through compiler flags rather than source annotations.

### Modern HLSL

For modern HLSL, entry points can be annotated directly in the source, using `[shader("vertex")]`, `[shader("pixel")]`, and so on. In this case, I found that using an HLSL library target allows Clang to build a single AST containing the different entry points.

I implemented a fallback for HLSL files without an available compilation command that uses the library target automatically. This provides a generic compilation context that works well for modern HLSL files containing multiple entry points.

### Legacy HLSL

The legacy case is more difficult. When the entry point is selected through compiler flags, different entry points may require different compilation commands, and clangd cannot simply use one compilation command and expect all entry points to be represented correctly.

I investigated possible approaches and built a prototype for explicitly switching the active compilation context. The prototype allows clangd to discover the entry points available in a file and switch between their compilation contexts without requiring the user to close and reopen the file. The server-side mechanism was validated end to end using LSP communication.

Full writeup: **[Multiple Entry Point Investigation](https://github.com/mafeguimaraes/hlsl-gap-analysis/blob/main/multiple-entry-point.md)**

This work is not yet a complete upstream solution, but it provides a foundation for continuing the work after GSoC.

## Future Work

The main remaining area of work is the legacy multiple-entry-point problem. The prototype still needs to be developed into an upstreamable solution and paired with client-side UX for selecting the active entry point.

The investigation already provides pieces that can be reused in future work:

* entry-point discovery;
* compilation-context switching;
* the HLSL library-mode fallback;
* the investigation of clangd's compilation database behavior;
* and the LSP prototype used to validate the approach.

There are also smaller follow-up opportunities, such as richer RootSignature hover information, a default `files.associations` entry for `.hlsl` in `vscode-clangd`, and additional HLSL-specific configuration improvements.

## What I've Learned

Before Google Summer of Code, I had some experience working with LLVM, but I had never worked on clangd or HLSL support.

One of the most valuable things I learned was how to investigate a compiler tooling problem from the user's perspective and trace it back to its root cause. During the project, I often had to move between different layers of the compiler, from the LSP behavior in clangd, through its AST handling, down to parsing and semantic analysis in Clang.

I also learned how important it is to understand the existing architecture before implementing a fix. In several cases, the information required to support an HLSL feature was already available in Clang, and the right solution was to make existing infrastructure aware of it rather than introducing a separate HLSL-specific implementation.

The upstreaming process was another major part of the learning experience. Writing an RFC, discussing design alternatives with the LLVM community, incorporating feedback, creating focused issues, and eventually submitting reviewable pull requests taught me a lot about how large open-source projects are developed.

Finally, investigating multiple entry points showed me that not every problem can be solved by fixing a single missing feature. Some problems require understanding the interaction between the compiler, clangd, the compilation database, and the LSP itself.

Overall, GSoC gave me much more confidence navigating a large compiler codebase, debugging problems across different layers of the compiler, and contributing changes to a project as large as LLVM.

## Acknowledgments

I would like to thank my mentors, Finn Plummer and Ashley Coleman, for their guidance and support throughout this project. They always made time to talk to me, discuss my progress, answer my questions, and help me work through the challenges I encountered. I am grateful for all the feedback they gave on my ideas, and for taking the time to explain things, listen to my proposals, and help me find a way forward whenever I was unsure about something. Working with them was one of the most valuable parts of this experience, and I learned a great deal from the way they approached problems.

I would also like to thank the LLVM and DirectX communities for the discussions and feedback on the RFC and implementation ideas. Their input was an important part of refining the proposed solutions and understanding how these features should fit into clangd's existing architecture.

