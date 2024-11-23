Title: Using GTK on Apple macOS
Slug: gtk-osx

The Apple macOS port of GTK is an implementation of GDK (and therefore GTK)
on top of the Quartz API.

Currently, the macOS port does not use any additional commandline options
or environment variables.

For up-to-date information on building, installation, and bundling, see the
[GTK website](https://www.gtk.org/docs/installations/macos).

## Content types

While GTK uses MIME types, macOS uses Unified Type descriptors.
GTK maps MIME to UTI types.

If you create a macOS app for your application, you can provide
custom UTI/MIME types mappings in the
[Information Property List](https://developer.apple.com/documentation/bundleresources/information_property_list)
for your application.
