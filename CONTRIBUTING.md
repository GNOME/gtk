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

```ssh
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

**Note**: For information about submitting patches and pushing changes
to Git, see the `README.md` and `README.commits` files. In particular,
don't, under any circumstances, push anything to Git before reading and
understanding `README.commmits`.

Typically, you should work on your own branch:

```sh
$ git checkout -b your-branch
```

Once you've finished working on the bug fix or feature, push the branch
to the Git repository and open a new merge request, to let the GTK
maintainers review your contribution.
