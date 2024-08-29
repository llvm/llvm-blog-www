---
author: "Andrew Kallai"
date: "2024-08-29"
tags: ["GSoC", "llvm-ir-dataset-utils", "ComPile", "analysis"]
title: "GSoC 2024: Statistical Analysis of LLVM-IR Compilation"
---

Welcome! My name is Andrew and I contributed to LLVM through the 2024 Google Summer of Code Program. My project is called [Statistical Analysis of LLVM-IR Compilation](https://summerofcode.withgoogle.com/programs/2024/projects/hquDyVBK). The objective of this project is to provide an analysis of the causes of unexpected compilation times.

# Background

In principle, an LLVM IR bitcode file, or module, contains IR features that determine the behavior of the compiler optimization pipeline. By varying these features, the optimization pipeline, opt, can perform better or worse. More specifically, optimizations succeed in less or more time; the user can wait for a microsecond or a few minutes. LLVM compiler developers constantly edit the pipeline, so the performance of these optimizations can vary by time (sometimes by seconds).

Having a large IR dataset such as ComPile (linked below) allows for testing the LLVM compilation pipeline on a varied sample of IR. The size of this sample is sufficient to determine outlying IR modules. By identifying and examining such files using utilities which are being added to the LLVM IR Dataset Utils Repo (linked below), the causes of unexpected compilation times can be determined. Developers can then modify and improve the compilation pipeline accordingly.

# Summary of Work

The utilities added in PR37 (linked below) are intended to write each IR module to a tar file corresponding to a programming language. This is designed to be a multithreaded process that is agnostic to the number of files per language or the type of language. Executing the scripts correctly results in a file containing index values. Each file written to the tar files is indexed by its location in the HF dataset. The first file index begins at 1, so for N files, there are IR files from file1.bc to fileN.bc. 

The Makefile from PR36 (linked below) is responsible for carrying out the data collection. This data includes text segment size, user CPU instruction counts during compile time, IR feature counts sourced from LLVM function_analysis_properties, and maximum relative time pass names and percentage counts. The data can be extracted in parallel or serially, and is stored in a CSV file.

Below is a modified version of code taken from the Makefile, which specifically illustrates what and how data is generated from the IR files:

```shell
#file$@.bc means fileX.bc, where is X is a positive integer.
#$(lang) is the directory name for the given IR language (e.g., rust)
perf stat --no-big-num -e instructions:u -o \
		$(lang)/perf_stat_files/file$@.txt \
		clang -O3 -c $(lang)/bc_files/file$@.bc \
		-o $(lang)/object_files/file$@.o

awk '/instructions/ {print $$1}' \
		$(lang)/perf_stat_files/file$@.txt

llvm-size $(lang)/object_files/file$@.o | awk 'NR==2 {print $$1}'

python3 -m llvm_ir_dataset_utils.compile_time_analysis_tools.write_ir_counts \
		$(lang)/bc_files/file$@.bc

clang -w -c -ftime-report $(lang)/bc_files/file$@.bc -o /dev/null 2>&1 | \
		awk '!/ignoring feature/' | awk 'NR==7 {print $$(NF-1) ", " $$NF}' | sed 's/%)//'
```

The last data collection command includes `clang -w -c -ftime-report $(lang)/bc_files/file$@.bc -o /dev/null`. The output from the command is large, but the part of interest is the first `Pass execution timing report`: 

```text
===-------------------------------------------------------------------------===
                          Pass execution timing report
===-------------------------------------------------------------------------===
  Total Execution Time: 2.2547 seconds (2.2552 wall clock)

   ---User Time---   --System Time--   --User+System--   ---Wall Time---  --- Name ---
   2.1722 ( 96.5%)   0.0019 ( 47.5%)   2.1741 ( 96.4%)   2.1745 ( 96.4%)  VerifierPass
   0.0726 (  3.2%)   0.0000 (  0.0%)   0.0726 (  3.2%)   0.0726 (  3.2%)  AlwaysInlinerPass
   0.0042 (  0.2%)   0.0015 ( 39.2%)   0.0058 (  0.3%)   0.0058 (  0.3%)  AnnotationRemarksPass
   0.0014 (  0.1%)   0.0005 ( 13.3%)   0.0019 (  0.1%)   0.0020 (  0.1%)  EntryExitInstrumenterPass
   0.0003 (  0.0%)   0.0000 (  0.0%)   0.0003 (  0.0%)   0.0003 (  0.0%)  CoroConditionalWrapper
   2.2507 (100.0%)   0.0039 (100.0%)   2.2547 (100.0%)   2.2552 (100.0%)  Total
```

A user can visually see the distribution of these passes by using a profiling tool for .json files. The .json file for a given bitcode file is obtained by `clang -c -ftime-trace <file>`.

The visualization of this output can be filtered to the passes of interest as in the following image:

{{< figure src="/img/file4504_pass_trace.png" alt="ftime-trace of C IR Outlier" >}}

# Current Status 

Currently, there are three PRs that require approval to be merged. There has been ongoing discussion on their contents, so few steps should be left to merge them.
In the current state, users of the dataset utils repository should be able to readily reproduce the quantitative results I had obtained for my midterm presentation graphs. Users can easily perform outlier analysis as well on the IR files (excluding Julia IR). Some of the results include the following:

Scatter Plot of C IR Files:

{{< figure src="/img/c_instvtext.png" alt="C IR Compiler Instruction Counts versus Text Segment Size" >}}

Table of outliers for C IR files:

{{< figure src="/img/c_outliers.png" alt="C IR Outlier Files" >}}

# Future Work

It was discussed in PR 37 to consolidate the tar file creation into the dataset file writer Python script. This is a feature I wish to implement in order to speed up the tar file creation process by having the bitcode files written from memory to the tar instead of from memory, to disk, to tar.

As mentioned, Julia IR was not analyzed. Modifying the scripts to include Julia IR results is desirable to make complete use of the dataset.
Adding additional documentation for demonstration-of-use purposes could help clarify ways to use the tools.


# Acknowledgements

I would like to thank my mentors Johannes Doerfert and Aiden Grossman for their constant support during and prior to the GSoC program. Additionally, I would like to acknowledge the work of the LLVM Foundation admins and the GSoC admins.

# Links
* [PR 38](https://github.com/llvm-ml/llvm-ir-dataset-utils/pull/38)

* [PR 37](https://github.com/llvm-ml/llvm-ir-dataset-utils/pull/37)

* [PR 36](https://github.com/llvm-ml/llvm-ir-dataset-utils/pull/36)

* [LLVM IR Dataset Utils Repo](https://github.com/llvm-ml/llvm-ir-dataset-utils)

* [Compile Dataset](https://huggingface.co/datasets/llvm-ml/ComPile)
