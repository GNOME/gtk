GTK Documentation site
======================

This branch of the GTK repository is used to generate the content of the
[docs.gtk.org](https://docs.gtk.org) website.

Contents
--------

```
.
├── atk
│   ├── atk
│   └── atspi2
├── glib
│   ├── gio
│   ├── glib
│   └── gobject
├── gtk3
│   ├── gdk
│   └── gtk
├── static
└── subprojects
    └── gi-docgen.wrap
```

The landing page is store inside the [`static`](./static) directory.

The [`atk`](./atk) directory contains the introspection data and gi-docgen
project files for ATK and AT-SPI2.

The [`gtk3`](./gtk3) directory contains the introspection data and gi-docgen
project files for GTK3 and GDK3.

The [`subprojects`](./subprojects) directory contains a Meson subproject for
gi-docgen.

How does this work
------------------

The CI pipeline for the main development branch of GTK builds the API
references for the following projects:

 - gtk
 - pango
 - gdk-pixbuf

Similarly, the CI pipeline for the main development branch of GLib builds
the API references for the following projects:

 - glib
 - glib-unix
 - glib-win32
 - gmodule
 - gobject
 - gio
 - gio-unix
 - gio-win32
 - girepository

The generated documentation is stored as an artifact inside GitLab.
Additionally, the CI pipeline will use a pipeline trigger for the
`docs-gtk-org` branch (the branch that contains the `README` file you are
currently reading).

The CI pipeline for the `docs-gtk-org` branch will:

 - download the build artifacts
 - extract the various API references
 - run gi-docgen on the introspection data of the following projects:
   - atk
   - gtk3
 - publish the static landing page
 - publish all the API references

Notes
-----

The token for the pipeline trigger is stored in the `PAGES_TRIGGER_TOKEN`
environment variable, which is exposed to the CI pipelines of the GTK
project.

Only the `docs-gtk-org` branch can publish the contents of the docs.gtk.org
website.
