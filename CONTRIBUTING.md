# Contribution guidelines

Thank you for considering contributing to the GTK project!

These guidelines are meant for new contributors, regardless of their level
of proficiency; following them allows the maintainers of the GTK project to
more effectively evaluate your contribution, and provide prompt feedback to
you. Additionally, by following these guidelines you clearly communicate
that you respect the time and effort that the people developing GTK put into
managing the project.

GTK is a complex free software GUI toolkit, and it would not exist without
contributions from the free and open source software community. There are
many things that we value:

 - bug reporting and fixing
 - documentation and examples
 - tests
 - new features

Please, do not use the issue tracker for support questions. If you have
questions on how to use GTK effectively, you can use:

 - the `#gtk` IRC channel on irc.gnome.org
 - the [gtk](https://mail.gnome.org/mailman/listinfo/gtk-list) mailing list,
   for general questions on GTK
 - the [gtk-app-devel](https://mail.gnome.org/mailman/listinfo/gtk-app-devel-list)
   mailing list, for questions on application development with GTK
 - the [gtk-devel](https://mail.gnome.org/mailman/listinfo/gtk-devel-list)
   mailing list, for questions on developing GTK itself

You can also look at the GTK tag on [Stack
Overflow](https://stackoverflow.com/questions/tagged/gtk).

The issue tracker is meant to be used for actionable issues only.

## How to report bugs

### Security issues

You should not open a new issue for security related questions.

When in doubt, send an email to the [security](mailto:security@gnome.org)
mailing list.

### Bug reports

If you're reporting a bug make sure to list:

 0. which version of GTK are you using?
 0. which operating system are you using?
 0. the necessary steps to reproduce the issue
 0. the expected outcome
 0. a description of the behavior; screenshots are also welcome
 0. a small, self-contained example exhibiting the behavior; if this
    is not available, try reproducing the issue using the GTK examples
    or interactive tests

If the issue includes a crash, you should also include:

 0. the eventual warnings printed on the terminal
 0. a backtrace, obtained with tools such as GDB or LLDB

For small issues, such as:

 - spelling/grammar fixes in the documentation
 - typo correction
 - comment clean ups
 - changes to metadata files (CI, `.gitignore`)
 - build system changes
 - source tree clean ups and reorganizations

You should directly open a merge request instead of filing a new issue.

### Features and enhancements

Feature discussion can be open ended and require high bandwidth channels; if
you are proposing a new feature on the issue tracker, make sure to make
an actionable proposal, and list:

 0. what you're trying to achieve
 0. prior art, in other toolkits or applications
 0. design and theming changes

If you're proposing the integration of new features it helps to have
multiple applications using shared or similar code, especially if they have
iterated over it various times.

Each feature should also come fully documented, and with tests.

## Your first contribution

### Prerequisites

If you want to contribute to the GTK project, you will need to have the
development tools appropriate for your operating system, including:

 - Python 3.x
 - Meson
 - Ninja
 - Gettext (19.7 or newer)
 - a [C99 compatible compiler](https://wiki.gnome.org/Projects/GLib/CompilerRequirements)

Up-to-date instructions about developing GNOME applications and libraries
can be found on [the GNOME Developer Center](https://developer.gnome.org).

The GTK project uses GitLab for code hosting and for tracking issues. More
information about using GitLab can be found [on the GNOME
wiki](https://wiki.gnome.org/GitLab).

### Dependencies

In order to get GTK from Git installed on your system, you need to have the
required versions of all the software dependencies required by GTK; typically,
this means a recent version of GLib, Cairo, Pango, and ATK, as well as the
platform-specific dependencies for the windowing system you are using (Wayland,
X11, Windows, or macOS).

The core dependencies for GTK are:

 - [GLib, GObject, and GIO](https://gitlab.gnome.org/GNOME/glib)
 - [Cairo](http://cairographics.org)
 - [Pango](https://gitlab.gnome.org/GNOME/pango)
 - [GdkPixbuf](https://gitlab.gnome.org/GNOME/gdk-pixbuf)
 - [Epoxy](https://github.com/anholt/libepoxy)
 - [ATK](https://gitlab.gnome.org/GNOME/atk)
 - [Graphene](https://github.com/ebassi/graphene)

GTK will attempt to download and build some of these dependencies if it
cannot find them on your system.

Additionally, you may want to look at projects that create a development
environment for you, like [jhbuild](https://wiki.gnome.org/HowDoI/Jhbuild)
and [gvsbuild](https://github.com/wingtk/gvsbuild).

### Getting started

You should start by forking the GTK repository from the GitLab web UI, and
cloning from your fork:

```sh
$ git clone https://gitlab.gnome.org/yourusername/gtk.git
$ cd gtk
```

**Note**: if you plan to push changes to back to the main repository and
have a GNOME account, you can skip the fork, and use the following instead:

```sh
$ git clone git@gitlab.gnome.org:GNOME/gtk.git
$ cd gtk
```

To compile the Git version of GTK on your system, you will need to
configure your build using Meson:

```sh
$ meson _builddir .
$ cd _builddir
$ ninja
```

Typically, you should work on your own branch:

```sh
$ git checkout -b your-branch
```

Once you've finished working on the bug fix or feature, push the branch
to the Git repository and open a new merge request, to let the GTK
maintainers review your contribution.

### Code reviews

Each contribution is reviewed by the core developers of the GTK project.

The [CODE-OWNERS](./docs/CODE-OWNERS) document contains the list of core
contributors to GTK and the areas for which they are responsible; you
should ensure to receive their review and signoff on your changes.

### Commit messages

The expected format for git commit messages is as follows:

```plain
Short explanation of the commit

Longer explanation explaining exactly what's changed, whether any
external or private interfaces changed, what bugs were fixed (with bug
tracker reference if applicable) and so forth. Be concise but not too
brief.

Closes #1234
```

 - Always add a brief description of the commit to the _first_ line of
 the commit and terminate by two newlines (it will work without the
 second newline, but that is not nice for the interfaces).

 - First line (the brief description) must only be one sentence and
 should start with a capital letter unless it starts with a lowercase
 symbol or identifier. Don't use a trailing period either. Don't exceed
 72 characters.

 - The main description (the body) is normal prose and should use normal
 punctuation and capital letters where appropriate. Consider the commit
 message as an email sent to the developers (or yourself, six months
 down the line) detailing **why** you changed something. There's no need
 to specify the **how**: the changes can be inlined.

 - When committing code on behalf of others use the `--author` option, e.g.
 `git commit -a --author "Joe Coder <joe@coder.org>"` and `--signoff`.

 - If your commit is addressing an issue, use the
 [GitLab syntax](https://docs.gitlab.com/ce/user/project/issues/automatic_issue_closing.html)
 to automatically close the issue when merging the commit with the upstream
 repository:

```plain
Closes #1234
Fixes #1234
Closes: https://gitlab.gnome.org/GNOME/gtk/issues/1234
```

 - If you have a merge request with multiple commits and none of them
 completely fixes an issue, you should add a reference to the issue in
 the commit message, e.g. `Bug: #1234`, and use the automatic issue
 closing syntax in the description of the merge request.

### Commit access to the GTK repository

GTK is part of the GNOME infrastructure. At the current time, any
person with write access to the GNOME repository can merge changes to
GTK. This is a good thing, in that it encourages many people to work
on GTK, and progress can be made quickly. However, GTK is a fairly
large and complicated project on which many other things depend, so to
avoid unnecessary breakage, and to take advantage of the knowledge
about GTK that has been built up over the years, we'd like to ask
people committing to GTK to follow a few rules:

0. Ask first. If your changes are major, or could possibly break existing
   code, you should always ask. If your change is minor and you've been
   working on GTK for a while it probably isn't necessary to ask. But when
   in doubt, ask. Even if your change is correct, somebody may know a
   better way to do things. If you are making changes to GTK, you should
   be subscribed to the [gtk-devel](https://mail.gnome.org/mailman/listinfo/gtk-devel-list)
   mailing list; this is a good place to ask about intended changes.
   The `#gtk` IRC channel on irc.gnome.org is also a good place to find GTK
   developers to discuss changes, but if you live outside of the EU/US time
   zones, an email to the gtk-devel mailing list is the most certain and
   preferred method.

0. Ask _first_.

0. Always write a meaningful commit message. Changes without a sufficient
   commit message will be reverted.

0. Never push to the `master` branch, or any stable branches, directly; you
   should always go through a merge request, to ensure that the code is
   tested on the CI infrastructure at the very least. A merge request is
   also the proper place to get a comprehensive code review from the core
   developers of GTK.

If you have been contributing to GTK for a while and you don't have commit
access to the repository, you may ask to obtain it following the [GNOME account
process](https://wiki.gnome.org/AccountsTeam/NewAccounts).
