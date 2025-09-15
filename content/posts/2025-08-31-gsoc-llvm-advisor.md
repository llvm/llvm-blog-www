---
author: "Miguel Cárdenas"
date: "2025-08-31"
tags: ["GSoC", "optimization", "visualization", "tooling", "offloading"]
title: "Making LLVM Compilation Data Accessible: A Unified Visualization Tool for Compiler Artifacts"
---

This summer, I had the incredible opportunity to work on an LLVM project through Google Summer of Code 2025. As an undergraduate student passionate about compilers, I set out to tackle a problem that many LLVM developers face daily: the overwhelming nature of compilation data scattered across different flags, output formats, and locations.

## The Problem: Information Overload

LLVM generates a wealth of valuable data during compilation—optimization remarks, profiling information, timing data, and more. However, this information is often difficult to process and analyze, especially for developers new to LLVM or those working with offloading toolchains. The typical workflow involves:

- Searching through thousands of remarks using grep
- Manually correlating data across compilation units  
- Trial-and-error debugging without data-driven insights
- Wrestling with different output formats and flags

This fragmented approach makes it particularly challenging for newcomers to understand what's happening during compilation and how to optimize their code effectively.

## Project Goals

The goal was to create a unified infrastructure that organizes, visualizes, and analyzes compilation data for the LLVM toolchain. Specifically, I aimed to:

- Make compiler data accessible to everyone, not just experts
- Reduce the frustration of searching through compilation artifacts
- Enable data-driven optimization decisions
- Create a foundation for advanced analysis tools
- Make offloading development more approachable

## What I Built

Over the 12-week program, I developed a working prototype called **LLVM Advisor** that addresses these challenges through several key components:

### Compiler Wrapper Tool
I created an easy-to-use wrapper (similar to ccache) that automatically collects all relevant compilation artifacts without requiring users to modify their build systems. The wrapper intelligently parses and organizes data into a structured format.

### Storage Layer
The tool uses a JSON-based storage format that supports:
- Project-wide linking of related files
- Incremental updates and differential views
- Cross-compilation unit correlation

### Web-Based Visualization Interface
The heart of the project is an intuitive dashboard that provides:
- **Overview Dashboard**: Key metrics, summaries, and aggregated findings at a glance
- **Source Code Viewer**: Interactive code browser with inline remarks and diagnostics
- **Performance Timeline**: Detailed analysis of compilation phases and bottlenecks

### Analysis Capabilities
The tool offers several analysis features:
- Cross-unit analysis views
- Performance tracking across builds
- Optimization effectiveness monitoring
- Pattern recognition in compilation issues

## Key Achievements

Throughout the program, I made steady progress on both the technical implementation and community engagement:

**Weeks 1-2**: Fixed a critical issue in the NVLINK wrapper that prevented collecting remarks for NVIDIA targets (submitted as PR [#145365](https://github.com/llvm/llvm-project/pull/145365)) and implemented the initial wrapper.

**Weeks 3-4**: Developed the remarks parser and created the first version of the web interface with source viewing capabilities.

**Weeks 5-6**: Designed the compiler wrapper architecture and storage layer, with extensive discussions with mentors about effective data presentation.

**Weeks 7-8**: Presented the project to the LLVM community and improved the web server foundation.

**Weeks 9-12**: Completed the source viewer, implemented the performance timeline, and expanded parsing support for various file types.

**Final Week**: Deployed a live demo and completed documentation.

## Technical Highlights

One of the most challenging aspects was designing a flexible data pipeline that could correlate scattered information from various compiler outputs **without any dependency**. The solution involved:

- **Smart Parsing**: Robust regex patterns and parsers for different LLVM output formats
- **Data Correlation**: Algorithms to link remarks, timing data, and source locations across compilation units  
- **Incremental Processing**: Efficient handling of large codebases with differential updates
- **Intuitive UI**: A clean interface that presents complex multi-layered data without overwhelming users

## Community Impact

This project represents more than just a tool—it's a step toward making LLVM more accessible. As someone from Latin America, where compiler topics aren't commonly discussed, I'm passionate about lowering barriers to entry in the LLVM community. 

The tool could be valuable for:
- **New Contributors**: Providing visual guidance for understanding optimization behavior
- **Experienced Developers**: Offering data-driven insights for performance tuning
- **Educational Use**: Helping students visualize what compilers actually do

## Try It Yourself

You can explore a live demo of LLVM Advisor at: https://llvm-advisor.onrender.com/

The complete implementation is available in PR [#147451](https://github.com/llvm/llvm-project/pull/147451), and you can see the development timeline at: https://time.graphics/line/999689/

## Looking Forward

This project lays a strong foundation with many exciting opportunities ahead:

- **LLM Integration**: Leveraging Large Language Models to provide automated insights and recommendations
- **Enhanced Analytics**: More sophisticated performance heuristics and pattern detection
- **Community Features**: Sharing optimization patterns and best practices across the community
- **Extended Platform Support**: Broader coverage of LLVM's diverse target architectures

## Personal Reflections

Working on this project through GSoC has been transformative. I've gained deep technical knowledge in LLVM's remark system, full-stack development, and build system integration. More importantly, it's confirmed my passion for making compilers accessible to everyone.

My goal is to continue as a maintainer and help grow the LLVM community, particularly in underrepresented regions. I believe that by making compiler technology more approachable, we can unlock innovation from developers around the world.

## Acknowledgments

I want to thank the LLVM community and my mentors for their guidance throughout this journey. The collaborative spirit and technical excellence of the LLVM project continues to inspire me, and I look forward to contributing to this amazing ecosystem for years to come.

As we often say in the compiler community: LLVM is the place for hackers, nerds, and anyone who likes what they're doing—because no one with common sense takes a glance at compilers at first, but once you get in, you're always willing to learn more and experiment more. We are the alchemists of new days, and projects like this help ensure that alchemy remains accessible to all.

## Gallery

<div class="figure">
<img src="/img/2025-08-31-gsoc-llvm-advisor-figure1.png"
     alt="dashboard"
     style="width:100%">
<p>Figure 1: dashboard</p>
</div>

<div class="figure">
<img src="/img/2025-08-31-gsoc-llvm-advisor-figure2.png"
     alt="performance"
     style="width:100%">
<p>Figure 2: performance</p>
</div>
