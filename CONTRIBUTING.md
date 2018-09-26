If you want to hack on the GTK+ project, you'll need to have the development
tools appropriate for your operating system, including:

 - Python 3.x
 - Meson
 - Ninja
 - Gettext (19.7 or newer)
 - a C99 compatible compiler

Up-to-date instructions about developing GNOME applications and libraries
can be found here:

 * https://developer.gnome.org

Information about using GitLab with GNOME can be found here:

 * https://wiki.gnome.org/GitLab

In order to get Git GTK+ installed on your system, you need to have the
required versions of all the GTK+ dependencies; typically, this means a
recent version of GLib, Cairo, Pango, and ATK, as well as the platform
specific dependencies for the windowing system you are using (Wayland, X11,
Windows, or macOS).

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

To compile the Git version of GTK+ on your system, you will need to
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
maintainers review your contribution. The [CODE-OWNERS](./docs/CODE-OWNERS)
document contains the list of core contributors to GTK and the areas for
which they are responsible.

### Commit messages

The expected format for git commit messages is as follows:

```plain
Short explanation of the commit

Longer explanation explaining exactly what's changed, whether any
external or private interfaces changed, what bugs were fixed (with bug
tracker reference if applicable) and so forth. Be concise but not too
brief.
```

 - Always add a brief description of the commit to the _first_ line of
 the commit and terminate by two newlines (it will work without the
 second newline, but that is not nice for the interfaces).

 - First line (the brief description) must only be one sentence and
 should start with a capital letter unless it starts with a lowercase
 symbol or identifier. Don't use a trailing period either. Don't exceed
 72 characters.

 - The main description (the body) is normal prose and should use normal
 punctuation and capital letters where appropriate. Conside the commit
 message as an email sent to the developers (or yourself, six months
 down the line) detailin **why** you changed something. There's no need
 to specify the **how**: the changes can be inlined.

 - When committing code on behalf of others use the `--author` option, e.g.
 `git commit -a --author "Joe Coder <joe@coder.org>"` and `--signoff`.

 - If your commit is addressing an issue, use the GitLab syntax to
 automatically close the issue on push:

```plain
Closes #1234
```

  or:

```plain
Fixes #1234
```

  or:

```plain
Closes https://gitlab.gnome.org/GNOME/gtk/issues/1234
```

### Access to the GTK repository

GTK+ is part of the GNOME infrastructure. At the current time, any
person with write access to the GNOME repository, can make changes to
GTK+. This is a good thing, in that it encourages many people to work
on GTK+, and progress can be made quickly. However, GTK+ is a fairly
large and complicated package that many other things depend on, so to
avoid unnecessary breakage, and to take advantage of the knowledge
about GTK+ that has been built up over the years, we'd like to ask
people committing to GTK+ to follow a few rules:

0. Ask first. If your changes are major, or could possibly break existing
 code, you should always ask. If your change is minor and you've
 been working on GTK+ for a while it probably isn't necessary
 to ask. But when in doubt, ask. Even if your change is correct,
 somebody may know a better way to do things.
 If you are making changes to GTK+, you should be subscribed
 to [gtk-devel-list](https://mail.gnome.org/mailman/listinfo/gtk-devel-list).
 This is a good place to ask about intended changes.
 `#gtk+` on GIMPNet (irc.gnome.org) is also a good place to find GTK+
 developers to discuss changes, but if you live outside of the EU/US time
 zones, email to gtk-devel-list is the most certain and preferred method.

0. Ask _first_.

0. Always write a meaningful commit message. Changes without a sufficient
 commit message will be reverted.
