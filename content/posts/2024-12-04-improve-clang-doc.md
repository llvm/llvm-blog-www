---
author: "Peter Chou"
date: "2024-12-04"
tags: ["GSoC", "Clang-Doc"]
title: "GSoC 2024: Improve Clang Doc"
---

Hi, my name is Peter, and this year I was involved in Google Summer of Code 2024. I worked on [improving the Clang-Doc documenation generator](https://discourse.llvm.org/t/improve-Clang-Doc-usability/76996)

Mentors: Petr Hosek and Paul Kirth

## Project Background

Clang-Doc is a documentation generator developed on top of libtooling, as an
alternative to Doxygen. Development started in 2018, and continued through 2019,
however it has since stalled. Currently, the tool can generate HTML, YAML, and markdown but the generated output has usability issues. This GSOC project was aimed to address the pain points regarding the output of the HTML, by adding support for various C++ constructs and reworking the css of the HTML output to be more user friendly.

## Work Done

The original scope of the project was to improve the output of Clang-Doc's generation. However during testing the tool was significantly slower than expected which made developing features for the tool impossible. 
Each compilation of the LLVM codebase was taking upwards of 10 hours on my local machine. Additionally the tool utilized a lot of memory and was prone to crashing with an out of memory error. Similar tools such as Doxygen and Hdoc ran in comparatively less time for the same codebase. This pointed to a significant bottleneck within Clang-Doc’s codepath when generating large scale software projects. Due to this the project scope quickly changed to improving the runtime of Clang-Doc so that it could run much faster. It was only during the latter half of the project did the scope change back to improving Clang-Doc’s generation.

### Added More Test Cases to Clang-Doc test suite


Clang-Doc previously had tests which did not test the full scope of the the HTML or Markdown output. I added more end-to-end tests to make sure that in the process of optimizing documentation generation we were not degrading the quality or functionality of the tool. 

In summary, I added four comprehensive tests which covered all features that we were not testing such as testing the generation for Enums, Namespace and Records for HTML and Markdown.

### Improve Clang-Doc’s performance by 1.58 times 

Internally, the way Clang-Doc works is by leveraging libtooling's ASTVisitor class to parse the source level declarations in each TU and serializing it into an internal format which gets deserialized later when we output the final format. 

Many experiments were conducted in order to identified the source of the bottleneck, initially I leverage windows prolifer however that was not fined grained enough to identified the true source of the 

Eventually, we were able to identify a major bottleneck in Clang-Doc's performance to doing redundant work when it was processing each declaration. We settled on a caching/memoization strategy to minimize the redundant work.

For example if we had a the following project: 

```cpp
//File: Base.cpp

class Base {}
```


```cpp
//File: A.cpp

class A : public Base {}
```

```cpp
//File: B.cpp

class B : public Base {}
```

In this case, the ASTVisitor invoked by Clang-Doc would visit the serialized Base class three times, once when it is parsing Base.cpp, another when its visiting A.cpp then B.cpp. This means any C++ project that heavily leverages inheritance would result in a lot of redundant work. 

The optimization ended up being a simple memoization dictionary which kept track of a list of declaration that Clang-Doc had visited. 

Here is a plot of the benchmarking numbers:

<div style="margin:0 auto;">
  <img src="/img/Clang-Doc-benchmark-numbers.png"><br/>
</div>


### Added Template Mustache HTML Backend

Clang-Doc originally used an ad-hoc method of generating HTML. I introduced a templating language as a way of reducing project complexity and reducing the ease of development. Two RFCs were made before arriving at the idea of introducing Mustache as a library. Originally the idea was to introduce a custom templating language, however upon further discussion it was decided that the complexity of designing and implementing a new templating language was too much.
A LLVM community member suggested using Mustache as templating language. 
Mustache was the ideal choice since it was very simple to implement, and has a well defined spec that fit what was needed for Clang-Doc’s use case. The feedback on the RFC was generally positive. While there was some resistance regarding the inclusion of an HTML support library in LLVM, this concern stemmed partly from a lack of awareness that HTML generation already occurs in several parts of LLVM. Additionally, the introduction of Mustache has the potential to simplify other HTML-related use cases.
In terms of engineering wins, this library was able to cut the direct down on HTML backend significantly dropping 500 lines of code compared to the original Clang-Doc HTML backend. This library was also designed for general purpose use around LLVM, since there are numerous places in LLVM where various tools generate html in its own way. Using the Mustache templating library would be a nice way to standardize the codebase. 

### Improve Clang-Doc HTML Output

The previous version of Clang-Doc’s output was a pretty minimal bare bones implementation. It had a sidebar that contained every single declaration within the project which created a large unnavigable UI. Typedef documentation was missing, plus method documentation was missing details such as whether or not the method was a const or virtual. There was no linking between other declarations in the project and there was no syntax highlighting on any language construct.

With the new Mustache changes an additional backend was added using the specifier (--format=mhtml). That addresses these issues. 

Below is a comparison of the same output between the two backends


<div style="margin:0 auto;">
  <img src="/img/Clang-Doc-old-html-output.png"><br/>
</div>

<div style="margin:0 auto;">
  <img src="/img/Clang-Doc-new-output.png"><br/>
</div>

You can also visit the output project on my personal github.io page link
[here](https://peterchou1.github.io/).

Note: this output is still a work in progress.

## Learning Insight

I've learned a lot in the past few months, thanks to GSOC I now have a much better idea of what it’s like to participate in a large open source project. I received a lot of feedback through PR’s, making RFC and collaborating with other GSOC members. I’d learned a lot about how to interact with the community and solicit feedback. I also learned a lot about instrumentation/profiling code having conducted many experiments in order to try to speed Clang-Doc up.

## Future Work

As my work concluded I was named as one of the maintainers of the project. In the future I plan to work on Clang-Doc until an MVP product can be generated and evaluated for the LLVM project. My remaining tasks include landing the Mustache support library and Clang-Doc’s Mustache backend, as well as gathering feedback from the LLVM community regarding Clang-Doc’s current output. Additionally, I intend to add test cases for the Mustache HTML backend to ensure its robustness and functionality.


## Conclusion

Overall the current state of Clang-Doc is much healthier than it was before. It now has much better test coverage across all its output, markdown, html, yaml. Whereas previously there were no e2e test cases that were not as comprehensive. The tool is significantly faster especially for large scale projects like LLVM making documentation generation and development a much better experience.
The tool also has a simplified HTML backend that will be much easier to work with compared to before leading to a faster velocity for development. 


## Acknowledgements

I’d like to thank my mentors, Paul and Petr for their invaluable input when I encounter issues with the project. This year has been tough for me mentally, and I’d like to thank my mentors for being accommodating with me. 
