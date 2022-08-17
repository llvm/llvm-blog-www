---
author: Kristof Beyls
date: "2022-08-17"
tags: ["relicensing", "foundation"]
title: August 2022 LLVM relicensing update & further suggestions for help
---

The last update on LLVM relicensing was [done about 8 months
ago](https://blog.llvm.org/posts/2021-11-18-relicensing-update/). Since then
we've made substantial progress, so I thought it's worthwhile to share another
update.

The TL;DR is:
* Out of the about 32 million LOC that were contributed under the old license,
  we've reduced the lines of code that aren't relicensed yet from 6% to only
  2%.
* 8 months ago, we were still searching for ways to contact 808 individuals who
  contributed to LLVM over the past 20 years. We managed to reduce that number
  to 421 individuals now.
* We've also reduced the number of companies or universities to get a relicensing
  agreement from from 133 to 122.

Read on to find out how we achieved that great progress and how you can help
with getting us closer to our end goal of having LLVM fully relicensed.

First of all, a big thank you to everyone who responded to the call for help in
the [November 2021 blog
post](https://blog.llvm.org/posts/2021-11-18-relicensing-update/) and the [2021
LLVM dev meeting presentation](https://www.youtube.com/watch?v=aAFDfhD-jDs).
This level of progress would not have been possible without your action!
    
Next to receiving more relicensing agreements, we also started exploring some
of the tactics described in [the previous
update](https://discourse.llvm.org/t/board-meeting-minutes-may-2022/63628)
under section "the end game", for the pieces of code that we end up not
receiving a relicensing agreement for, as described in the next sections.
   
## Threshold of originality
 
Remember that licenses exists because of copyright law -- the license is how
the copyright owners of the code in LLVM give rights to users and other
contributors to use their code in lots of useful and interesting ways.

This also means that if a piece of code is not protected by copyright law,
there is no need for it to be covered by a license.

Some pieces of code are not covered by copyright law.  For example, copyright
law contains a concept called ["Threshold of
originality"](https://en.wikipedia.org/wiki/Threshold_of_originality). It means
that a work needs to be "sufficiently original" for it to be considered to be
covered by copyright.  There could be a lot of different interpretations into
what it means for a code contribution to be sufficiently original for it to be
covered by copyright.  A threshold that is often used in open source projects
that use [contributor license agreements
(CLA)](https://en.wikipedia.org/wiki/Contributor_License_Agreement) is to
assume that any contribution that's 10 lines of code or less does not meet the
threshold of originality and therefore copyright does not apply. In [their May
2022 board
meeting](https://discourse.llvm.org/t/board-meeting-minutes-may-2022/63628),
the LLVM Foundation decided to make the same assumption for the relicensing
project: contributions of 10 lines of code or less are assumed to not be
covered by copyright.
Therefore, we don't need relicensing agreements for those.

Furthermore, there are a few commits that don't seem to meet the
"threshold-of-originality" even though they're changing/adding more than 10
lines. We also consider those to not needing a relicensing agreement. One
example is https://github.com/llvm/llvm-project/commit/cd13ef01a21e, which only
removes the full stop at the end of a few sentences.
    
## Code no longer present in Top-of-Trunk.

We started exploring which of the not-yet-relicensed code is still in the
current top-of-trunk code base. Some big contributions that weren't covered yet
are no longer there, such as for example the Microblaze and PIC16 backends. We
manually checked and marked commits contributing to just these backends as not
needing to be covered by relicensing agreements.

To help with finding more code that is no longer in the code base, I wrote a
simple heuristic script that searches in current top-of-trunk if lines from a
specific commit can still be found in the code base. If for a given commit, no
or very few lines can be found in current top-of-trunk, that's a strong
indication that that code is probably no longer there. That heuristic script
indicates that for about 5% of the not-yet-relicensed commits, the code does no
longer seem to be present. I still need to find time to manually verify the
code in these commits is indeed no longer in the code base.  This manual
verification would be something that someone could easily help me with. If
anyone reading this would like to volunteer for helping with that - please do
let me know!

## Next steps

In our quest to get nearer to 100% relicensing coverage, I believe the
following are the most impactful next steps to take:

- Continue accepting more relicensing agreements, from individuals and from
  corporations. An up-to-date list of who we still need to get agreements from
  is published as a [spreadsheet here](https://docs.google.com/spreadsheets/d/18_0Hog_eSwES8lKwf7WJal3yBwwcYfvPu1yCfZnTcek/edit?usp=sharing).
  - We found that it can help a lot when corporations can get a list of which
    commits they're agreeing to relicense. If you're progressing getting a
    corporation/company to sign, please do send an email to
    license-questions@llvm.org asking for the list of commits that the company
    may own copyright on.
- Go through the commits that look like they may no longer be in top-of-trunk,
  and verify that manually.

## How can you help?

- You could check if you know any individual still listed in
  [this up-to-date spreadsheet](https://docs.google.com/spreadsheets/d/18_0Hog_eSwES8lKwf7WJal3yBwwcYfvPu1yCfZnTcek/edit?usp=sharing).
  If you do know any of the people, please do reach out directly to them - I'm
  often the bottleneck when you share contact details with me and rely on me to
  reach out to them. If they do have any questions, I'm more than happy to try
  and answer them.
- You could check if you know who the right people are to contact at any of the
  remaining companies/corporations and talk with them directly or share their
  contact details with us at license-questions@llvm.org. They are also listed
  in the [same
spreadsheet](https://docs.google.com/spreadsheets/d/18_0Hog_eSwES8lKwf7WJal3yBwwcYfvPu1yCfZnTcek/edit?usp=sharing),
  on the sheet starting with "Corporations".

- If you'd be interested in helping to check if specific commits are still
  present in the current code base, please do let me know at
  license-questions@llvm.org.

