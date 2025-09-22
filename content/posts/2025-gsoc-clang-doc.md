---
author: "Erick Velez (evelez7)"
date: "2025-08-28"
tags: ["GSoC", "clang-doc", "clang-tools-extra", "documentation"]
title: "GSoC 2025: Improving Core Clang-Doc Functionality"
---

I was selected as a contributor for GSoC 2025 under the project "Improving Core Clang-Doc Functionality" for LLVM.
My mentors for the project were Paul Kirth and Petr Hosek.

Clang-Doc is a tool in clang-tools-extra that generates documentation from Clang's AST and can output Markdown, HTML, YAML, and JSON.
The project started in 2018 but major development eventually slowed.
Recently, there have been efforts to get it back on track.

This year, the [GSoC project idea](https://discourse.llvm.org/t/improve-documentation-parsing-in-clang/84513) had a simple premise: improve core functionality.

## The Issues

The project idea proposed three main areas of focus to improve documentation quality.

1. C++ support
2. Doxygen comments
3. Markdown support

First, not all C++ constructs were supported, like friends or concepts.
Not supporting core C++ constructs in C++ documentation is not good.
Second, it's important that Doxygen command support is robust and that we can support as many as possible.
Lastly, having Markdown available to developers for documentation would be useful.
Markdown provides the power of expression in an area that is technically dense.
It can be used to highlight critical information and warnings.

### The Architecture

Here's a quick overview on Clang-Doc's architecture, which follows a map-reduce pattern:

1. Visit source declarations via Clang's ASTVisitor.
2. Serialize relevant source information into an Info (Clang-Doc's main data entity).
3. Once all source declarations are serialized, write them into bitcode, reduce, and read the reduced Infos.
4. Serialize Infos into the desired format.

<div style="margin:0 auto;">
  <img src="/img/gsoc-2025-clang-doc-architecture.png"><br/>
</div>

It seems fairly straightforward, but the architecture had a critical flaw.
If a new C++ construct needed to be supported, it would be visited and serialized, but then support would have to be added to each backend individually.
If you wanted to serialize something in YAML, you'd have to implement the Markdown logic separately.
This imposed a very high maintenance cost for extending basic functionality, even if you just wanted to add something simple.
It also easily led to generator disparity; a construct might be serialized in YAML, but not in Markdown.
Testing was also in an awkward spot because it was unclear what format would be used to verify if the documentation output was acceptable.

## The Good: Mustache

Last year's GSoC brought in great improvements that became the basis of my summer.
First, last year's GSoC contributor landed a large performance improvement.
I might not have been able to test Clang-Doc on Clang itself without it.

Another contribution that was essential to my summer is the [Mustache template engine](https://mustache.github.io/) implementation in LLVM.
Mustache templates allow Clang-Doc to shift away from manually generating HTML tags and eliminate high maintenance burdens.
Templates could also solve the feature parity problem by using JSON to feed templates.

# Building a JSON Backend

While familiarizing myself with the codebase during the Community Bonding Period, I quickly determined that implementing a JSON backend would be incredibly beneficial to the project and my summer plans.
A JSON backend presented two immediate benefits:

1. We could use it to feed our Mustache HTML templates and future template usage.
2. As the main feeder format, testing can be focused on the JSON output.

The existing Mustache backend in Clang-Doc already contained logic to create JSON documents, but they were immediately discarded when the templates were rendered.
This backend is extremely beneficial to Clang-Doc because it would completely eliminate any need for manual HTML tag generation, thus greatly reducing lines of code.
If the JSON and template rendering logic from the existing implementation were uncoupled, we could apply the same pattern to any format we'd want to support.
For example, Markdown generation would be a similar case to HTML where templates would be used to automate the creation of all markup.

<div style="margin:0 auto;">
  <img src="/img/gsoc-2025-clang-doc-template-backend.png"><br/>
</div>

This diagram models the architecture that Clang-Doc would follow given a unified JSON backend.
Note the similarities to Clang, where our frontend (the visitation/serialization) gathers all the information we need and emits an intermediate representation (JSON).
The JSON is then fed to the desired templates to produce our documentation, similar to how IR is used for different LLVM backends.
Following this pattern would reduce the logic maintenance to only the JSON generation; all the formatting for HTML, Markdown, etc. would exist in template files that are very simple to change.

Thus, I adapted the JSON logic from the Mustache backend and create a separate JSON backend.
I also added tests to ensure the C++ constructs that Clang-Doc already supported were properly serialized in JSON.
I didn't realize it at the time, but this would end up dramatically accelerating my pace of implementation.

## C++ Language Support and Testing

After landing the JSON generator in about a week, I returned to my proposed schedule by implementing support for C++ constructs like friends.
The new JSON generator allowed me to quickly implement and test these features because I didn't have to worry about HTML formatting or appearance.
I could work with the assumption that as long as the information was properly serialized into JSON, it would be able to be displayed well in HTML later.

Testing is an area that the JSON backend brought a lot of clarity to.
Clang-Doc didn't have a format where all the information we wanted, like ensuring we document that a variable is `const` or `volatile`, was validated.
At one time, YAML was meant to be that format, but it suffered from feature disparity since it wasn't relevant when something needed to be displayed in HTML.
I ended up adding 14 different test files over the course of the summer to ensure test coverage.

### Pull Requests
- [add tags to Mustache namespace template](https://github.com/llvm/llvm-project/pull/142045)
- [add a JSON generator](https://github.com/llvm/llvm-project/pull/142483)
- [add namespaces to JSON generator](https://github.com/llvm/llvm-project/pull/143209)
- [removed default label on some switches](https://github.com/llvm/llvm-project/pull/143919)
- [precommit](https://github.com/llvm/llvm-project/pull/144160) and [add support for concepts](https://github.com/llvm/llvm-project/pull/144430)
- [precommit](https://github.com/llvm/llvm-project/pull/145069) and [document global variables](https://github.com/llvm/llvm-project/pull/145070)
- [serialize isBuiltIn and IsTemplate](https://github.com/llvm/llvm-project/pull/146149)
- [precommit](https://github.com/llvm/llvm-project/pull/146164) and [serialize friends](https://github.com/llvm/llvm-project/pull/146165)

# Comments

## Groups and Order

Comments weren't ordered in Clang-Doc's HTML documentation.
They were just displayed in whatever order they were serialized in, which is the order that they're written in source.
This meant comments would be extremely difficult to read - you don't want to search for another parameter comment after reading the first one, even if they're expected to be written in order in source.

Funnily enough, Mustache made this a little more complicated.
The only logic operation that Mustache has to check if a field exists is an iteration like `{{#Fields}}`, but any header that denotes a comment section would be duplicated.

```html
{{#Fields}}
<h3>Field Header</h3>
  {{FieldInfo}}
{{/Fields}}
```

All of the logic to order them needs to be done in the serialization to JSON itself, so I overhauled our comment organization.
Previously, Clang-Doc's comments were organized exactly as in Clang's AST like the following:

- FullComment
  - BriefComment
    - ParagraphComment
      - TextComment
      - TextComment
  - BriefComment
    - ParagraphComment

Everything was unnecessarily nested under a FullComment, and TextComments were also unnecessarily nested.
Every non-verbatim comment's text was held in one ParagraphComment.
Since there was only one, we could reduce some boilerplate by directly mapping to the array of TextComments.

After the change, Clang-Doc's comments were structured like this:

- BriefComments
  - TextCommentArray
  - TextCommentArray
- ParagraphComments
  - TextCommentArray

Now, we can just iterate over every type of comment, which means iterating over every array.
This left our JSON documentation with a few more fields, since one is needed for every Doxygen command, but with easier identification of what comments exist in the documentation.
After this refactor was landed, I implemented support for the comments we had already supported and ones we didn't, like Doxygen code comments.

## Reaping the benefits of JSON

This was an area where a JSON backend once again accelerated my progress.
Without it, I would've written the same JSON logic but would've had to written tests to check for the comments in HTML.
This would've been incredibly cumbersome since I would've had to:

1. Add the appropriate templating language to allow the comments to render.
2. Add the correct HTML tags to allow the test to pass.

As I mentioned, comments weren't being generated the best in HTML anyways, so I could've run into more annoyances if I had to follow that workflow.

### Pull Requests
- [add namespace references to VarInfo](https://github.com/llvm/llvm-project/pull/146964)
- [fix ASan complaints from passing RepositoryURL as reference](https://github.com/llvm/llvm-project/pull/148923)
- [integrate JSON as the source for Mustache templates](https://github.com/llvm/llvm-project/pull/149589)
- [separate comments into categories](https://github.com/llvm/llvm-project/pull/149590)
- [enable comments in class templates](https://github.com/llvm/llvm-project/pull/149848)
- [remove nesting of text comments inside paragraphs](https://github.com/llvm/llvm-project/pull/150451)
- [generate comments for functions](https://github.com/llvm/llvm-project/pull/150570)
- [add param comments to comment template](https://github.com/llvm/llvm-project/pull/150571)
- [add return comments to comment template](https://github.com/llvm/llvm-project/pull/150647)
- [add code comments to comment template](https://github.com/llvm/llvm-project/pull/150648)

# Markdown
Markdown was the most speculative aspect of the project.
It wasn't clear whether we'd try to integrate a solution into Clang itself or whether we'd keep it in clang-tools-extra.

## A JavaScript Solution
The first option I explored was suggested by my mentor, which was a JavaScript library called [Markdown-Tag](https://github.com/MarketingPipeline/Markdown-Tag)
This would've been really convenient since all it requires is an HTML tag to enable rendering, so any comment text in a template can be easily rendered.
Unfortunately, it requires all HTML to be sanitized, which defeats the purpose of a ready-made solution for us.
We would have to parse any potential HTML in comments anyways.

## A Parser Solution
Without an out-of-the-box solution, we were left with implementing our own parser.
When I considered this in my proposal, I knew an in-tree parser would want to conform to the simplest possible standard.
Markdown has no official standard, so I opted for CommonMark conformance.

The summer ended without a complete solution since a couple weeks were spent researching whether or not this could be integrated directly in the Clang comment parser or whether we'd need to build our own solution or not.
You can see my initial draft [here](https://github.com/llvm/llvm-project/pull/155887).
I plan to continue working on this parser and landing it in Clang-Doc.

# Refactors, Name Mangling, and More!
During my summer, I would stumble into places where I would think "This could be better" and my mentors usually agreed.
Thus, there were a few patches where I dedicated time to general refactors to improve code reuse and hopefully make the lives of future contributors much easier than what I had to go through.
In fact, one of my best refactors was of the JSON generator that I wrote, which my mentor noted had a lot of areas for great code reuse.
Future me was extremely thankful for the easy-to-use functions I had added.
I also refactored some of the bitcode reader/writer code so that less copy-pasting would be involved in the future.

Another significant feature that I hadn't planned was name mangling.
Clang-Doc suffered from a bug where template specializations would be serialized to the same file as their described class because they had the same name.
The YAML backend avoided this problem because its filenames were SymbolIDs, but this meant that the lit tests would have to use regex to find the file for FileCheck.
Nasty.
In HTML, you ended up with a buggy HTML page from duplicate tags.
In JSON, you get a fatal error since two JSON documents can't live in the same file.
So I used `ItaniumMangleContext` to generate mangled names we could use for the filenames.

### Pull Requests
- [Serialize record files with mangled name](https://github.com/llvm/llvm-project/pull/148021)
- [refactor BitcodeReader::readSubBlock](https://github.com/llvm/llvm-project/pull/145835)
- [refactor JSONGenerator array usage](https://github.com/llvm/llvm-project/pull/145595)
- [refactor JSON for better Mustache compatibility](https://github.com/llvm/llvm-project/pull/149588)

# Overview
I implemented a new JSON generator that will serve as the basis for Clang-Doc's documentation generation.
This will vastly reduce overall lines of code and maintenance burdens.
I added a lot of tests to increase code coverage and ensure we are serializing all the information necessary for high-quality documentation.
I refactored our comment handling to streamline the logic that handles them and for better output in the HTML.
I also explored options for rendering Markdown and began an implemetation for a parser that I plan on working on in the future.
Along the way, I also did some refactoring to improve code reuse and improved maintenance burdens by reducing boilerplate code.

After my work this summer, Clang-Doc is nearly ready to switch to HTML generation via Mustache templates, which will be a huge milestone.
It is backed by the JSON generator which will allow for a much more flexible architecture that will change how we generate other documentation formats like our existing Markdown backend.
All of this was achieved with little to no performance loss.
I also hope that future contributors have an easier time than I did learning about and working with Clang-Doc.
The threshold for contributing was high due to a disjointed architecture.

I'm also very excited to present my work and showcase Clang-Doc at this year's LLVM Dev Meeting during a technical talk.
This is the first time I'll be presenting at a conference and I didn't expect to have the opportunity when I started at the beginning of the summer.

Over the summer, I addressed these issues:
- [template operator T() produces a bad name](https://github.com/llvm/llvm-project/issues/59812)
- [Add a JSON backend to clang-doc to better leverage mustache templates](https://github.com/llvm/llvm-project/issues/140094)
- [Reconsider use of enum InfoType::IT_default](https://github.com/llvm/llvm-project/issues/142888)
- [Add a JSON backend to clang-doc to better leverage mustache templates](https://github.com/llvm/llvm-project/issues/140094)

# Future Work
These are issues that I identified over the summer that I wasn't able to address but would benefit from community discussion and contribution.

## Doxygen Grouping

Doxygen has a very useful [grouping](https://www.doxygen.nl/manual/grouping.html) feature that allows structures to be grouped under a custom heading or on separate pages.
You can see it in [llvm::sys::path](https://llvm.org/doxygen/namespacellvm_1_1sys_1_1path.html).
We [opened up an issue](https://github.com/llvm/llvm-project/issues/151184) for Clang to track this issue, which ended up being a duplicate of [this issue](https://github.com/llvm/llvm-project/issues/123582).

There would most likely have to be some major changes to Clang's comment parsing and Clang's own parsing.
That's because a lot of the group opening tokens in Clang are free-floating, like so:

```cpp
/// @{

class Foo {};
```

That `@{` doesn't attach to a Decl; only comments directly above a declaration are attached to a Decl in the AST.
My mentors wisely advised that this would be too much to even consider this summer, and could probably be its own GSoC project.

## Cross-referencing

In Doxygen you can use the `@copydoc` command to copy the documentation from one entity to another.
Doxygen also displays where an entity is referenced, like where a function is invoked.
Clang-Doc currently has no support for this kind of behavior.

Clang-Doc would need a preprocessing step where any reference to another entity is identified and then resolved somewhere.
One of my mentors pointed out that it would be great to do during the reduction step where every Info is being visited anyways.
This actually wasn't something I had even considered in my proposal besides identifying that `@copydoc` wasn't supported by the comment parser.
It's a common feature of modern documentation, so hopefully someday soon Clang-Doc can acquire it.

# Acknowledgements
Thank you very much to my mentors Paul Kirth and Petr Hosek for guiding me and advising me in this project.
I learned so much from review feedback and our conversations.
I would not be presenting at the Dev Meeting if not for the encouragement you both gave.
