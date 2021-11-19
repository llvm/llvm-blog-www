---
author: Kristof Beyls
date: "2021-11-18"
tags: ["relicensing", "foundation"]
title: LLVM relicensing update & call for help
---

In this blog post, I'd like to summarize the main points I talked about in the
relicensing update presentation at the 2021 LLVM developers meeting.

The very short summary is that we are currently in the long tail phase of
collecting relicensing agreements of past contributors. We already at the time
of this writing have more than 94% of older code relicensed.  We hope to crowd
source getting through the long tail to get as close to 100% as possible.

# Call for help

You can help by looking through [the list of remaining individuals and
corporations we need to get agreements
from](https://docs.google.com/spreadsheets/d/18_0Hog_eSwES8lKwf7WJal3yBwwcYfvPu1yCfZnTcek/edit?usp=sharing),
and reaching out to them, or letting us know at license-questions@llvm.org how
we can reach out to them. Also, if you'd happen to have any other information that you think could help us, please do let us know at license-questions@llvm.org.

More detailed guidance on how you can help are available at
<https://foundation.llvm.org/docs/relicensing_long_tail>.

In the rest of this blog post, I'm going to give a bit more historical
background and describe the current status in more detail.

# The LLVM relicensing effort: phases over time

The relicensing effort started some time ago. Let me first very briefly describe
the various phases before going in more detail. In the years leading up to 2015,
it became clear that there were some issues with the license LLVM had at the
time.

A different license was the answer to those issues. Between roughly 2015 and
2017 the focus was on deciding what different license would be best suited.

Once it was decided what the new license was going to be, we started working on
getting all code to be covered by this new license. That includes getting
agreement from all copyright holders of existing code to share their past
contributions under the new license. As you can see on the timeline, getting
those agreements is the current focus of the relicensing effort.
                  
Maybe we won't be able to get an agreement for every single past contribution.
In that case, we have a number of options to get to the point where we can claim
that all code in the LLVM project is covered by the new LLVM license. We call
this phase of relicensing "the end game".

<img>
<svg width=700 height=120>
  <line x1="0" y1="100" x2="700" y2="100" style="stroke:gray;stroke-width:2" />
  <circle cx="50" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="100" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="150" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="200" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="250" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="300" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="350" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="400" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="450" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="500" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="550" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="600" cy="100" r="5" stroke-width="0" fill="gray"/>
  <circle cx="650" cy="100" r="5" stroke-width="0" fill="gray"/>
  <text x="50"  y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2011</text>
  <text x="100" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2012</text>
  <text x="150" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2013</text>
  <text x="200" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2014</text>
  <text x="250" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2015</text>
  <text x="300" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2016</text>
  <text x="350" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2017</text>
  <text x="400" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2018</text>
  <text x="450" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2019</text>
  <text x="500" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2020</text>
  <text x="550" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2021</text>
  <text x="600" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2022</text>
  <text x="650" y="115" style="fill:gray;font-size: 12px;" text-anchor="middle">2023</text>
  <defs>
    <linearGradient id="grad1" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" style="stop-color:rgb(166, 191, 255);stop-opacity:0" />
      <stop offset="10%" style="stop-color:rgb(166, 191, 255);stop-opacity:1" />
      <stop offset="90%" style="stop-color:rgb(166, 191, 255);stop-opacity:1" />
      <stop offset="100%" style="stop-color:rgb(166, 191, 255);stop-opacity:0" />
    </linearGradient>
  </defs>
  <g class="fragment">
  <!-- First text block is 2011-2015-ish -->
  <rect x="75" y="40" width="175" height="50" fill="url(#grad1)"/>
  <!-- Middle of first text block is 2013 -->
  <text x="150" y="60" style="fill:black;font-size: 16px;" text-anchor="middle">
  <tspan x="160" dy="0em">Need for license</tspan>
  <tspan x="160" dy="1em">change emerges</tspan>
  </text>
  </g>
  <g class="fragment">
  <rect x="255" y="40" width="100" height="50" fill="url(#grad1)"/>
  <text x="325" y="60" style="fill:black;font-size: 16px;" text-anchor="middle">
    <tspan x="310" dy="0em">Deciding new</tspan>
    <tspan x="310" dy="1em">license</tspan>
  </text>
  </g>
  <g class="fragment">
  2018-now (2022?)〉 Get all code covered by new license.
  <rect x="350" y="40" width="275" height="50" fill="url(#grad1)"/>
  <text x="415" y="60" style="fill:black;font-size: 16px;" text-anchor="middle">
    <tspan x="490" dy="0em">Get all code covered</tspan>
    <tspan x="490" dy="1em"> by new license</tspan>
  </text>
  </g>
  <g class="fragment">
  <rect x="625" y="40" width="85" height="50" fill="url(#grad1)"/>
  <text x="625" y="60" style="fill:black;font-size: 16px;" text-anchor="middle">
    <tspan x="670" dy="0em">end</tspan>
    <tspan x="670" dy="1em">game</tspan>
  </text>
  </g>
</svg>
</img>

# Why relicense?

The old license consists of 3 components: the UIUC license that covered all the
code, the MIT license that additionally covered the code in run-time libraries,
and a few sentences on granting patent rights in the developer policy.

This caused
[the following 3 issues](https://lists.llvm.org/pipermail/llvm-dev/2015-October/091536.html):

1. Some were blocked from contributing because of the text in the patent section
  in the developer policy. The wording could be interpreted as requiring giving
  unnecessarily broad access to patent rights when contributing to LLVM. That
  made it impossible for some companies to contribute.

2. The run time libraries were dual licensed under the UIUC and MIT license; the
   rest of the code only under the UIUC license. Therefore, we could not easily
   move code to run time libraries from other parts. The reason run time
   libraries were dual licensed was to enable linking to run time library
   binaries without requiring attribution to LLVM.

3. The wording on patent rights in the developer policy was fuzzy and imprecise,
   leading to uncertainty over whether it did provide the intended protection.

# A new license

After exploring a range of options, it was decided that the best solution to solve these issues was to have all code licensed under the [Apache-2.0 with LLVM exception](https://foundation.llvm.org/relicensing/LICENSE.txt) license:

* Apache-2.0 contains well-understood patent granting which addresses the first
  and third issue.
            
* The LLVM exception is there for 2 reasons:

  - It removes a potential incompatibility with using LLVM code in combination
    with GPLv2 code.
            
  - It removes the requirement for developers using LLVM tools to tell the users
    of the binaries they produce that those binaries may contain some code
    originating from LLVM. Such a situation can easily arise when parts of the
    LLVM run-time libraries are linked in as part of the normal compilation
    process.

    The LLVM exception enables the run-time libraries to be covered by the exact
    same single license as the rest of the code base.  

# Getting all code covered by the new license

After a decision was made of what the new license should be, we started working
on having all code covered by it.
            
As a first step, we made sure that all new contributions were covered by the new
license. This happened after the 8.0 release branch was created. The 100k
commits since are covered by the new license.
            
The remaining task is to also get the earlier contributions covered by the new
license. This consists of about 300k commits totaling about 32 million lines of
contributed code.

<img>
<svg width=700 height=400 style="mix-blend-mode: difference;">
  <rect x="0" y="200" width="350" height="50" style="fill:rgb(0, 183, 255);stroke-width:0" />
  <rect x="350" y="200" width="350" height="50" style="fill:rgb(72, 255, 0);stroke-width:0" />
  <rect x="347" y="200" width="6" height="50" style="fill:gray;stroke-width:0"/>
  <text x="350" y="275" style="fill:rgb(255, 255, 255);font-size: 20px;"
  text-anchor="middle">2019/01/19</text>
  <text x="700" y="275" style="fill:rgb(255, 255, 255);font-size: 20px;"
  text-anchor="end">Today</text>
  <text x="0" y="275" style="fill:rgb(255, 255, 255);font-size: 20px;"
  text-anchor="start">2001/06/06</text>

  <g class="fragment">
    <circle cx="390" cy="47" r="5" stroke-width="0" fill="gray"/>
    <line x1="390" y1="47" x2="350" y2="200" style="stroke:gray;stroke-width:2" />
    <circle cx="350" cy="200" r="5" stroke-width="0" fill="gray"/>
    <foreignObject x="400" y="20" width="300" height="200">
    <div xmlns="http://www.w3.org/1999/xhtml"
    style="font-size:20px;text-align:left">
    New license installed after<br/>
    8.0 release branched.
    </div>
  </foreignObject>
  </g>

  <g class="fragment">
    <circle cx="420" cy="290" r="5" stroke-width="0" fill="gray"/>
    <line x1="420" y1="290" x2="420" y2="225" style="stroke:gray;stroke-width:2" />
    <circle cx="420" cy="225" r="5" stroke-width="0" fill="gray"/>
    <foreignObject x="400" y="300" width="300" height="200">
    <div xmlns="http://www.w3.org/1999/xhtml"
    style="font-size:20px;text-align:left">
    All code since is covered.
    (~100k commits so far)
    </div>
  </foreignObject>
  </g>

  <g class="fragment">
    <circle cx="40" cy="47" r="5" stroke-width="0" fill="gray"/>
    <line x1="40" y1="47" x2="40" y2="225" style="stroke:gray;stroke-width:2" />
    <circle cx="40" cy="225" r="5" stroke-width="0" fill="gray"/>
    <foreignObject x="50" y="20" width="300" height="200">
    <div xmlns="http://www.w3.org/1999/xhtml"
    style="font-size:20px;text-align:left">
    Most code before needs agreement from copyright owners.
    (~300k commits; ~32M LOC)
    </div>
  </foreignObject>
  </g>
</svg>
</img>

What needs to be done to get those earlier contributions covered?

The reason we need a license in the first place is copyright. Most code
contributions are covered by copyright. Which means that the person or company
owning the copyright has a lot of decision power over what can legally be done
with that code. By covering the code with a license, it becomes clear what
others are permitted to do with that code. If there isn't a license associated
with copyrighted code there isn't all that much useful that others can do with
it.

Basically, to get existing code to be covered by a new license, we need to find
who owns the copyright on it, and ask them to agree with offering their
copyrighted work under the new license.

The copyright owner can be either an individual, for example the person who
wrote the code originally; or a company, for example a company that employed the
person who wrote the code.

# Asking agreement from copyright owners

We started asking for their agreements. By examining the version control log of
the 300k-ish commits we need to get agreements for, we found that about 2800
different people or email addresses made a contribution.
    
We reached out to all of them and asked them two things. First, if any
corporation may own the copyright on any of their contributions. Secondly, if
they agree with relicensing the contributions they copyright own personally.
    
So far, we've heard of about 220 unique corporations potentially copyright
owning some past contribution. We also started reaching out to those, but have
not asked every single one just yet.

# Status as of November 2021

So after 2.5 years since we started asking -- where are we with getting
agreements to relicense?
            
The chart below summarize the current status. It shows a treemap where each
rectangle represents the contributions made by one person. The size of each
rectangle represents how many lines of code the person contributed.

![Treemap showing LLVM relicensing status](/img/relicensing2021/relicensing-status-treemap.svg)

When the rectangle is green, it means all their contributions are fully covered
by relicensing agreements.
            
When the rectangle is orange, it means we have not yet received such an
agreement. When the rectangle is orange with green stars, it means some
contributions by that person are covered and others not. This can happen for
example when the person has worked for multiple companies over time and only
some of those companies have agreed with the relicensing so far.
            
We already have over 94% of all contributed lines of code between 2001 and 2019
covered by a licensing agreement. We only have a good 5% of lines of code to go
still.
            
As you can see, most of the missing agreements are with "long tail"
contributors. By long tail contributors here we mean the many contributors who
contributed relatively fewer lines of code. We focussed on reaching out to the
bigger contributors first. To reach out well to the long tail of contributors,
we're hoping to get help from the wider LLVM community.

# Help wanted!

Please do consider helping us with reaching any of the people or corporations in
the long tail. Please have a look at [the up-to-date list of people and
corporations](https://docs.google.com/spreadsheets/d/18_0Hog_eSwES8lKwf7WJal3yBwwcYfvPu1yCfZnTcek/edit?usp=sharing)
we can use help with getting in touch with.
            
You can find more detailed guidance on how you can help on the [LLVM foundation
website's relicensing long tail
page](https://foundation.llvm.org/docs/relicensing_long_tail/).
            
If you do think you could help us with reaching out to someone on the list, or
you may have some other information that could help us, please do let us know by
emailing license-questions@llvm.org.

# Relicensing end game

We are currently in the phase of getting as many relicensing agreements as
possible. We do expect that we may not be able to get absolutely 100% of all
past contributions covered by an agreement. What can we do to achieve current
top-of-mainline to be fully covered by the new license?
            
We will need to decide on a contribution-by-contribution basis what the options
available are to achieve that goal. We have at least the following options.
            
* We can check if copyright even applies to the particular contribution. Very
  small contributions may not be covered by copyright, and hence may not need a
  license agreement.
            
* It may well be that code contributed a long time ago is no longer in the code
  base.
            
* If copyright does apply and the code is still in the code base, we can remove
  the contribution. Depending on whether current contributors and users still
  value the effect of that contribution, it may need to be reimplemented.