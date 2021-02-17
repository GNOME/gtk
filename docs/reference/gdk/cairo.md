----
Title: Cairo interaction
----

## Functions to support using cairo

[Cairo](http://cairographics.org) is a graphics library that supports vector
graphics and image compositing that can be used with GTK.

GDK does not wrap the Cairo API; instead it allows to create Cairo
drawing contexts which can be used to draw on [class@Gdk.Surface]s.

Additional functions allow use [struct@Gdk.Rectangle]s with Cairo
and to use [struct@Gdk.RGBA], `GdkPixbuf`, and [class@Gdk.Surface]
instances as sources for drawing operations.

For more information on Cairo, please see the
[Cairo API reference](https://www.cairographics.org/manual/).
