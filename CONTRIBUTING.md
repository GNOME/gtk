If you want to hack on the GTK+ project, you'll need to have the development
tools appropriate for your operating system, including:

	- Python (2.7 or 3.x)
	- Meson
	- Ninja
	- Gettext (19.7 or newer)
	- a C99 compatible compiler

Up-to-date instructions about developing GNOME applications and libraries
can be found here:

        https://developer.gnome.org

Information about using git with GNOME can be found here:

        https://wiki.gnome.org/Git

In order to get Git GTK+ installed on your system, you need to have the
required versions of all the GTK+ dependencies; typically, this means a
recent version of GLib, Cairo, Pango, and ATK.

**Note**: if you plan to push changes to back to the master repository and
have a GNOME account, you want to use the following instead:

```sh
$ git clone ssh://<username>@git.gnome.org/git/gtk+
```

To compile the Git version of GTK+ on your system, you will need to
configure your build using Meson:

```sh
$ meson _builddir .
```

For information about submitting patches and pushing changes to Git, see the
`README.md` and `README.commits` files. In particular, don't, under any
circumstances, push anything to Git before reading and understanding
`README.commmits`.
