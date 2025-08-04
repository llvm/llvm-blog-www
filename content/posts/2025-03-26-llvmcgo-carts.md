---
author: "Rafael Andres Herrera Guaitero"
date: "2025-08-04"
tags: ["MLIR", "ARTS"]
title: "LLVMCGO25 - CARTS: Enabling Event-Driven Task and Data Block Compilation for Distributed HPC"
---

# LLVMCGO25 - CARTS: Enabling Event-Driven Task and Data Block Compilation for Distributed HPC

Hello everyone! I’m Rafael, a PhD candidate at the University of Delaware. I recently flew from Philadelphia to Las Vegas to attend the CGO conference, 
where I had the chance to present my project and soak in new ideas about HPC.

In this blog, I’ll dive into the project I discussed at the conference and share some personal insights and lessons I learned along the way. 
Although comments aren’t enabled here, I’d love to hear from you, feel free to reach out at (*rafaelhg at udel dot edu*) if you’re interested in collaborating, have questions, or just want to chat.

## Motivation: Why CARTS?

Modern High-Performance Computing (HPC) and AI/ML workloads are pushing our hardware and software to the limits. Some key challenges include:

- **Evolving Architectures:** Systems now have complex memory hierarchies that need smart utilization.
- **Hardware Heterogeneity:** With multi-core CPUs, GPUs, and specialized accelerators in the mix, resource management gets tricky.
- **Performance Pressure:** Large-scale systems demand efficient handling of concurrency, synchronization, and communication.

These challenges led to the creation of CARTS—a compiler framework that combines the flexibility of MLIR with the reliability of LLVM to optimize applications for distributed HPC environments.

## A Closer Look at ARTS and Its Inspirations

At the heart of CARTS is ARTS. Originally, ARTS stood for the **Abstract Runtime System**. 
I often get mixed up and mistakenly call it the **Asynchronous Runtime System**. To keep things light, 
we sometimes joke about it being the **Any Runtime System**. 

ARTS is inspired by the Codelet model, a concept I could talk about all day! 
The Codelet model breaks a computation into small, independent tasks (or "codelets") that can run as soon as their data dependencies are met. 
If you're curious to learn more about this model (or find it delightfully abstract), I suggest you visit our research group website 
at [CAPSL, University of Delaware](https://www.capsl.udel.edu/) and check out the [Codelet Model website](https://www.capsl.udel.edu/codelets.shtml#B4).

### What Does ARTS Do?

ARTS is designed to support fine-grained, event-driven task execution in distributed systems. Here’s a simple breakdown of some key concepts:

- **Event-Driven Tasks (EDTs):** These are the basic units of work that can be scheduled independently. Think of an EDT as a small, self-contained task that runs once all its required data is ready.
- **DataBlocks:** These represent memory regions holding the data needed by tasks. ARTS tracks these DataBlocks across distributed nodes so that tasks have quick and efficient access to the data they need.
- **Events:** These are signals that tell the system when a DataBlock is ready or when a task has finished. They help synchronize tasks without the need for heavy locks.
- **Epochs:** These act as synchronization boundaries. An epoch groups tasks together, ensuring that all tasks within the group finish before moving on to the next phase.

By modeling tasks, DataBlocks, events, and epochs explicitly, ARTS makes it easier to analyze and optimize how tasks are executed across large, distributed systems.

## The CARTS Compiler Pipeline

Building on ARTS, CARTS creates a task-centric compiler workflow. Here’s how it works:

### Clang/Polygeist: From C/OpenMP to MLIR

- **Conversion Process:** Using the Polygeist infrastructure, we translate C/OpenMP code into MLIR. This process handles multiple dialects (like OpenMP, SCF, Affine, and Arith).
- **Extended Support:** We’ve enhanced it to handle more OpenMP constructs, including OpenMP Tasks

### ARTS Dialect: Simplifying Concurrency

- **Custom Language Constructs:** The ARTS dialect converts high-level OpenMP tasks into a form that directly represents EDTs, DataBlocks, events, and epochs.
- **Easier Analysis:** This clear representation makes it simpler to analyze and optimize the code.

### Optimization and Transformation Passes

- **EDT Optimization:** We remove redundant tasks and optimize task structures—for example, turning a “parallel” task that contains only one subtask into a “sync” task.
- **DataBlock Management:** We analyze memory access patterns to decide which DataBlocks are needed and optimize their usage.
- **Event Handling and Classic Optimizations:** We allocate and manage events, applying techniques like dead code elimination and common subexpression elimination to clean up the code.

### Lowering to LLVM IR and Runtime Integration

- **Conversion to LLVM IR:** The ARTS-enhanced MLIR is converted into LLVM IR. This involves outlining EDT regions into functions and inserting ARTS API calls for task, DataBlock, epoch, and event management.
- **Seamless Integration:** The final binary runs on the ARTS runtime, which schedules tasks dynamically based on data readiness.

## Looking Ahead: Future Directions for CARTS

The journey with CARTS is just beginning. Here’s a glimpse of what’s next:

- **Comprehensive Benchmarking:** Testing the infrastructure with a variety of benchmarks to validate performance under diverse scenarios.
- **Expanded OpenMP Support:** Enhancing support for additional OpenMP constructs such as loops, barriers, and locks.
- **Advanced Transformation Passes:** Developing techniques like dependency pruning, task splitting/fusion, and affine transformations to further optimize task management and data locality.
- **Memory-Centric Optimizations:** Implementing strategies like cache-aware tiling, data partitioning, and optimized memory layouts to reduce cache misses and enhance data transfer efficiency.
- **Feedback-Directed Compilation:** Incorporating runtime profiling data to adapt optimizations dynamically based on actual workload and hardware behavior.
- **Domain-Specific Extensions:** Creating specialized operations for domains such as stencil computations and tensor operations to boost performance in targeted HPC applications.

##  Wrapping Up

Conferences like CGO are not just about technical presentations, they’re also about meeting people and sharing ideas. I really enjoyed the mix of technical sessions and informal conversations. 
One of my favorite moments was meeting a professor at the conference and joking about how we only seem to meet when we’re away from Newark. 
It’s these human connections, along with the valuable feedback on my work, that make attending such events worthwhile. Here are a few personal takeaways:

- **Invaluable Feedback:** Presenting work-in-progress at LLVM CGO workshops has taught me that constructive criticism is the fuel for innovation.
- **Community Spirit:** Reconnecting with fellow researchers, whether through formal sessions or casual hallway conversations, enriches both our professional and personal lives.
I encourage fellow PhD candidates and early-career researchers to take every opportunity to present your work,your ideas might not be 100% polished, but the community is there to help you refine them.

Presenting CARTS allowed me to share detailed technical insights, discuss the practical challenges of HPC, and even have a few laughs along the way. While the technical details might seem dense at times, I
 hope the mix of personal anecdotes and hands-on explanations makes the topic accessible and engaging.
If you’re interested in discussing more about ARTS, the Codelet model, or anything else related to HPC, please drop me an email at (*rafaelhg at udel dot edu*). I’d love to chat, collaborate, or simply hang out.

## Acknowledgements

- This work is supported by the US DOE Office of Science project “Advanced Memory to Support Artificial Intelligence for Science” at PNNL. PNNL is operated by Battelle Memorial Institute under Contract DEAC06-76RL01830.
- Thanks to the LLVM Foundation for the travel award that made attending the CGO conference possible.
