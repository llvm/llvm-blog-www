---
title: "About"
date: 2020-06-17T22:30:32-07:00
menu: "about" #Display this page on the nav menu
weight: "1" #Right-most nav item
meta: "false" #Do not display tags or categories
---

Welcome to the LLVM Blog!

To create a new post on this blog start by cloning the repository:

```sh
git clone https://github.com/llvm/llvm-blog-www.git
```

Content on the blog is located in the `content/posts` directory. There is a
template file located in `content/template.md` that can be used as a starting
point. It contains the meta data that the author of each blog needs to fill out.
The naming convention used for the posts is: `YYYY-MM-DD-name-of-blog.md`. 

Once the blog contents have been filled in, the file should be placed under the
`content/post` directory. You can then use hugo to build the blogs and start a
webserver on your local machine to view the content:

```sh
hugo server -D
```

This should create output similar to:

```
Start building sites …
hugo v0.96.0+extended darwin/amd64 BuildDate=unknown

                   | EN
-------------------+------
  Pages            | 406
  Paginator pages  |  69
  Non-page files   |   0
  Static files     |  46
  Processed images |   0
  Aliases          | 170
  Sitemaps         |   1
  Cleaned          |   0

Built in 2291 ms
Watching for changes in /tmp/llvm-blog-www/{content,static,themes}
Watching for config changes in /tmp/llvm-blog-www/config.toml, /tmp/llvm-blog-www/themes/hugo-kiera/config.toml
Environment: "development"
Serving pages from memory
Running in Fast Render Mode. For full rebuilds on change: hugo server --disableFastRender
Web Server is available at http://localhost:1313/ (bind address 127.0.0.1)
Press Ctrl+C to stop

```

Note the second last line that specifies the Web Server that is now running. You
can direct your browser to this page to view the contents. Note that this will
build *all* of the blogs, including yours, so you can see how it will appear on
the official LLVM website.


Once you are happy with the content of your blog post, and checked that it
renders properly the final step is to issue a pull request to merge it into the
main branch of the llvm-blog-www repository.



