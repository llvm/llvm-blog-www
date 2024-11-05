---
author: "Chaitanya Shahare"
date: "2024-11-05"
tags: ["GSoC", "llvm.org"]
title: "GSoC 2024: LLVM.org Website Redesign"
---

## Introduction

Hello! I’m Chaitanya Shahare, and as part of my Google Summer of Code (GSoC) 2024, I contributed to the LLVM project by redesigning the LLVM.org website. My
project, titled _Improving LLVM.org Website Look and Feel_, focused on creating
a modern, accessible, and efficient user experience for the LLVM community.
Working with Hugo as a static site generator, I aimed to streamline content
organization, enhance site flexibility, and make the content management process
more accessible for contributors.

## Project Goals

The objectives of the project were clear but ambitious:

- **Create a Modular, Reusable Theme**: Build a responsive, accessible Hugo-based theme that could simplify the process of updating content while maintaining a clean and modern aesthetic.
- **Improve Content Structure and Navigation**: Simplify the navigation and presentation of content, making it easier for users to discover LLVM resources, community projects, and ongoing initiatives.
- **Configurable Content Management**: Use YAML data files to separate data and layout, making it straightforward for contributors to manage and update website content.

## Project Work and Achievements

#### 1. Building the Hugo-Based Theme

- Created a theme with reusable components for flexible layout customization.
- Designed a **responsive navbar** with support for submenus, a theme switcher for light/dark mode, and an extendable homepage template.
- Implemented key site features, like a **configurable footer** and modular components, to ensure a cohesive look and easy navigation across all devices.

#### 2. YAML Data-Driven Pages

- Developed pages for subprojects, GSoC projects, LLVM Developer Meetings, and Publications using YAML data files. This approach allows easy additions without complex HTML edits.
- Developed **custom shortcodes** to standardize content display elements, including:
  - **Sub Project Cards**: Reusable components for projects and highlights.
  - **Table of Contents**: Generated table of contents based on headers for easy navigation.
  - **Event and Publication Shortcodes**: Simplified creating pages with uniform data for conferences, talks, and publication details.

#### 3. Setting up the Repository Structure and Configurations

- Split the codebase into two repositories:
  - **Main Website Repository**: `www-new`, which contains the core site setup.
  - **Hugo Theme Repository**: `www-template`, designed as a standalone Hugo theme, making it easy to extend the new layout to other LLVM subprojects (e.g., Clang).
- Documented theme configuration and provided guidelines on setting up new content and adding new pages, which makes it user-friendly for future contributors.

#### 4. Comprehensive Documentation and Community Engagement

- Created detailed documentation covering theme installation, customization, content updates, and shortcodes.
- Set up a staging environment for testing and gathering community feedback, allowing contributors to interact with the new design and provide insights.

[![New LLVM.org Website Screenshot](/img/llvm-org-website-redesign-2024-11-05.png)](https://www-new.llvm.org/)

## Challenges and Learnings

Developing a modular, reusable theme for an extensive website like LLVM.org came with its own set of challenges. Ensuring that the Hugo theme was both customizable and accessible involved extensive testing across devices and getting feedback from LLVM community members. I also gained hands-on experience with designing user-friendly navigation and adapting UI components based on community needs.

## Future Work

There are a few areas left for further development, which would add even more flexibility and usability to the site:

- **Expanding Content Pages**: Continue adding dev meeting pages.
- **Implement CI for Link Validation**: Add continuous integration to check for broken links and ensure build stability.
- **Clang Website**: Apply this reusable theme structure to create a website specifically for Clang.
- **Engaging Contributors**: Facilitate community contributions to content updates, ensuring a continuously updated and resourceful website for all LLVM users.

## Acknowledgements

I am sincerely grateful to my mentors, [Tanya Lattner](https://github.com/tlattner) and [Vassil Vassilev](https://github.com/vgvassilev), for their incredible guidance and support throughout the project. Special thanks to the LLVM Foundation and the GSoC program for this opportunity.

## Related Links

- **New Website**: [www-new.llvm.org](https://www-new.llvm.org/)
- **GitHub Repositories**:
  - [LLVM Website Repo](https://github.com/llvm/www-new)
  - [LLVM Theme Repo](https://github.com/llvm/www-template)
- **Detailed Project Diary**: [GSoC Blog Series](https://blog.chaitanyashahare.com/series/gsoc/)

This project has been a rewarding experience, and I’m excited to see the LLVM community benefit from a refreshed, modernized website. Thank you for reading and for the opportunity to contribute to this impactful open-source project!
