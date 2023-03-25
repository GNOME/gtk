Title: Coordinate systems
Slug: gtk-coordinates

## Coordinate systems in GTK

Every widget in a window has its own coordinate system that it uses to place its
child widgets and to interpret events. Most of the time, this fact can be safely
ignored. The section will explain the details for the few cases when it is important.

## The box model

When it comes to rendering, GTK follows the CSS box model as far as practical.

![](https://www.w3.org/TR/css-box-3/images/box.png)

The CSS stylesheet that is in use determines the sizes (and appearance) of the
margin, border and padding areas for each widget. The size of the content area
is determined by GTKs layout algorithm using each widgets measure() and
size_allocate() vfuncs.

You can learn more about the CSS box model by reading the
[CSS specification](https://www.w3.org/TR/css-box-3/#box-model).

## Widgets

The content area in the CSS box model is the region that the widget considers its own.

The origin of the widgets coordinate system is the top left corner of the content area,
and its size is the widgets size. The size can be queried with [method@Gtk.Widget.get_width]
and [method@Gtk.Widget.get_height]. GTK allows general transformations to position
widgets (although most of the time, the transformation will be a simple translation).
The transform to go from one widgets coordinate system to another one can be obtained
with [method@Gtk.Widget.compute_transform].

When widget APIs expect positions or areas, they need to be expressed in this coordinate
system, typically called *widget coordinates*. GTK provides a number of APIs to translate
between different widgets' coordinate systems, such as [method@Gtk.Widget.compute_point]
or [method@Gtk.Widget.compute_bounds].

Another area that is occasionally relevant are the widgets *bounds*, which is the area
that a widgets rendering is typically confined to (technically, widgets can draw outside
of this area, unless clipping is enforced via the [property@Gtk.Widget:overflow] property).
In CSS terms, the bounds of a widget correspond to the border area.

During GTK's layout algorithm, a parent widget needs to assign size and position to its
child widgets, by calling [method@Gtk.Widget.allocate] or one of its variants. The size
that is passed to this function corresponds to the margin area of the child widget in
the CSS box model.

## Events

Some types of events that are coming from the windowing system have positions associated
with them (e.g. the pointer position). Such positions are expressed in *surface coordinates*
which have their origin at the top left corner of the surface.

When interpreting such positions with respect to widgets, you need to keep in mind that
there is an offset between the top left corner of the surface and the top left corner of
the topmost widget (typically either a [class@Gtk.Window] or [class@Gtk.Popover]). This
offset can be obtained with [method@Gtk.Native.get_surface_transform].

This translation is only necessary when dealing with raw events in the form of
[class@Gdk.Event] structs. Event controllers such as [class@Gtk.GestureClick] report
positions in the coordinate system of the widget they are attached to.
