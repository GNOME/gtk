Title: Using GTK on Apple macOS
Slug: gtk-osx

The Apple macOS port of GTK is an implementation of GDK (and therefore GTK)
on top of the Quartz API.

Currently, the macOS port does not use any additional commandline options
or environment variables.

For up-to-date information on building, installation, and bundling, see the
[GTK website](https://www.gtk.org/docs/installations/macos).

## Menus

By default a GTK app shows an app menu and an edit menu.
To make menu actions work well with native windows, such as a file dialog,
the most common edit commands are treated special:

* `text.undo`
* `text.redo`
* `clipboard.cut`
* `clipboard.copy`
* `clipboard.paste`
* `selection.select-all`

Those actions map to their respective macOS counterparts.
The actions are enabled in GTK if the action is available on the focused widget
and is enabled.

To extend the macOS system menu, add application and window actions to the
application with [`Application.set_accels_for_action()`](method.Application.set_accels_for_action.html).
Those actions can then be activated from the menu.

## Content types

While GTK uses MIME types, macOS uses Unified Type descriptors.
GTK maps MIME to UTI types.

If you create a macOS app for your application, you can provide
custom UTI/MIME types mappings in the
[Information Property List](https://developer.apple.com/documentation/bundleresources/information_property_list)
for your application.
