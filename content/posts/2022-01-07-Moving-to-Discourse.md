---
author: Tanya Lattner 
date: "2022-01-07"
tags: ["community", "infrastructure", "foundation"]
title: "Improving LLVM Infrastructure - Part 1: Mailing lists"
---
When the LLVM Project was open sourced in 2003, it was a small project with a small community. The tools selected to support the project were chosen during a different time and a different situation. 

Now, almost 18 years later, the project has grown tremendously and those infrastructure choices are not necessarily the right choices today.  According to a recent [article](https://www.phoronix.com/scan.php?page=news_item&px=LLVM-Record-Growth-2021), the LLVM community experienced record growth in 2021 with 1400 contributors to the LLVM Project. This is incredible growth!  While there is a very real cost to change, it is clear that we need to invest in modernizing these tools to support the current and further growth of LLVM.

Recently, the LLVM Project moved its source code repository and bug tracker to GitHub in an effort to grow the LLVM community and make contributing to the project easier. While these changes were neither quick or easy, the benefits to the project outweigh the time and efforts to make the move. The growth in contributors since the move to GitHub is just one metric showing the gains from the change.

This year, the LLVM Foundation, which provides the majority of the LLVM Project infrastructure, will be spearheading the effort to evaluate and identify improvements for the remaining tools and software the project utilizes.  We will be looking at mailing lists, chat servers, code review tools, and other parts of our infrastructure. This is part 1 in our blog post series describing the upcoming changes (if any) to LLVM infrastructure. 

**Project Communication - Mailing Lists**

Mailman and IRC have been the primary means of communication for the LLVM Project for the last 18 years. As the project has grown, we added [mailing lists](http://lists.llvm.org) for subprojects like Clang, but the main list (llvm-dev) has remained unchanged. This list is meant to cover all of LLVM which includes everything from mid-level optimizations, to back-end optimizations, to the numerous LLVM backends. It has been the catchall for any topic not covered by another list. Due to the growth in LLVM and the size of the project, this list is very high traffic and requires a lot of work to filter messages that one may be interested in. It is increasingly clear that a mailing list may not be the best form of communication in the lack of features to make it easy to filter, respond and track topics of interest.

In 2019, LLVM started to experiment with [Discourse](https://www.discourse.org) as an alternative to its mailing lists. Discourse is an open source discussion platform built with an amazing list of modern features, such as the following:
- Fully supported Email interface - Discourse supports the ability to interact through email if you do not like to use the web or app interfaces.
- Categories and subcategories - This breaks the LLVM project into smaller pieces and users can subscribe to topics that they care most about versus having to do their own filtering. 
- Single Account Sign-On - Users can use their existing GitHub account to use Discourse and get access to all categories.
- Dynamic notifications - Immediately notifies users when they are tagged, get a new reply, or sends email when you are offline.
- Simple - Discourse uses a flat forum where replies are dynamically loaded and flow down the page in a line.
- Mobile support - App available for iOS and Android, but you can also just use the web interface.
- Better Moderation - The community has the ability to self moderate through flagging and spreads the work across a larger group of moderators.
- Enhanced spam blocking - Out of the box spam filtering that eliminates obvious spam before it gets to moderators.
- Safety - It has an enhanced trust system that allows the community to build natural immunity from trolls, bad actors, and spammers, and also reinforces positive behavior through likes and badges.

The majority of the community was in favor of the move when the move to Discourse was discussed extensively on the LLVM mailing lists.  This provides the features mentioned above in addition to a more modern communication. We did hear of one feature some would miss compared to Mailman: the ability to reply to someone directly through email. However, while it may not be ideal for some, we feel this is a worthwhile tradeoff to gain the other benefits, e.g. better safety for LLVM developers and users in general.

**Moving to Discourse**

As a consequence of the above discussion, we are planning to move most mailing lists to Discourse.  We are excited that it will bring many new features to help improve communication for existing contributors and users, and also make the project more accessible to newcomers as most are familiar with using forums such as Discourse.

However, while Discourse is a great solution for community discussions, it is not clear if it is a good place to host commit messages and post-commit review.   Therefore, the commit lists won’t be part of the initial migration to Discourse - they will remain on mailing lists for now.

The plan to move to Discourse will involve the following:
- January 7-9 - Re-configure the existing LLVM Discourse to the new category/subcategory structure (see below)
- January 10-20 (sometime during these 2 weeks) - The LLVM mailing list archives are migrated to Discourse and it is sanity checked by volunteers of the LLVM community. This sanity check can take a week or more. 
- Feb 1 - The mailing lists (except commits) are put into read only mode and all users must migrate to use Discourse.
- Feb 1-4 (approx) - The final merge is done from Mailman to Discourse. **We encourage all LLVM community members to start using Discourse on Jan 10th to minimize any disruption once the mailing lists become read only and the final messages are merged to Discourse.** However, please be assured that all mail will be merged to Discourse and you will be able to continue any threads started there once that is completed. 

The mailman archives on the LLVM server may eventually be removed, but there is no final decision or deadline on this yet. 

**Mapping from Mailing Lists to Discourse Categories**

The existing LLVM Discourse will be modified to have the following categories/subcategories:

* Announcements

* Clang Frontend
  * Using Clang
  * Building Clang
  * Static Analyzer
  * clangd

* Subprojects
  * LLD Linker
  * Flang Fortran Frontend
  * LLDB Debugger

* Code Generation
  * Common Infrastructure
  * AArch64
  * ARM
  * Mips
  * PowerPC
  * RISCV
  * WebAssembly
  * X86
  * Other

* IR & Optimizations
  * Loop Optimizations

* Community
  * Women in Compilers and Tools
  * Job Postings
  * US Developer Meeting
  * EuroLLVM
  * Google Summer of Code
  * Community.o
  * LLVM Foundation

* Project Infrastructure
  * Release Testers
  * Website
  * Documentation
  * GitHub
  * Code Review
  * Discord
  * Mailing Lists and Forums
  * IRC
  * Infrastructure Working Group
  * Policies
  * LLVM Dev List Archives

* Runtimes
  * C
  * C++
  * OpenCL
  * OpenMP
  * Sanitizers

* Incubator
  * CIRCT
  * mlir-npcomp

* MLIR
  * Announcements
  * Newsletter
  * TCP-WG 
  
  
**The Mailman archives will be mapped as follows:**

| Mailing lists         | category in Discourse |
|-----------------------|--------------------|
| All-commits           | no migration at the moment |
| Bugs-admin            | no migration at the moment |
| cfe-commits           | no migration at the moment |
| cfe-dev               | Clang Frontend |
| cfe-users             | Clang Frontend/Using Clang |
| clangd-dev            | Clang Frontend/clangd |
| devmtg-organizers     | Obsolete |
| Docs                  | Obsolete |
| eurollvm-organizers   | Obsolete |
| flang-commits         | no migration at the moment |
| flang-dev             | Subprojects/Flang Fortran Frontend |
| gsoc                  | Obsolete |
| libc-commits          | no migration at the moment |
| libc-dev              | Runtimes/C |
| Libclc-dev            | Runtimes/OpenCL |
| libcxx-bugs           | no migration at the moment |
| libcxx-commits        | no migration at the moment |
| libcxx-dev            | Runtimes/C++ |
| lldb-commits          | no migration at the moment |
| lldb-dev              | Subprojects/lldb |
| llvm-admin            | no migration at the moment |
| llvm-announce         | Announce |
| llvm-branch-commits   | no migration at the moment |
| llvm-bugs             | no migration at the moment |
| llvm-commits          | no migration at the moment |
| llvm-dev              | Project Infrastructure/LLVM Dev List Archives |
| llvm-devmeeting       | Community/US Developer Meeting |
| llvm-foundation       | Community/LLVM Foundation |
| Mlir-commits          | no migration at the moment |
| Openmp-commits        | no migration at the moment |
| Openmp-dev            | Runtimes/OpenMP |
| Parallel_libs-commits | no migration at the moment |
| Parallel_libs-dev     | Runtimes/C++ |
| Release-testers       | Project Infrastructure/Release Testers |
| Test-list             | Obsolete |
| vmkit-commits         | Obsolete |
| WiCT                  | Community/Women in Compilers and Tools |
| www-scripts           | Obsolete |


For help with migrating to Discourse, please see the [User Guide](https://llvm.org/docs/DiscourseMigrationGuide.html) and discuss on the [LLVM Discourse forums](https://llvm.discourse.group). 

