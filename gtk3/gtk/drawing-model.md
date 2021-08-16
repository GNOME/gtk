Title: The GTK Drawing Model

## Overview of the drawing model

This chapter describes the GTK drawing model in detail. If you are
interested in the procedure which GTK follows to draw its widgets and
windows, you should read this chapter; this will be useful to know if you
decide to implement your own widgets. This chapter will also clarify the
reasons behind the ways certain things are done in GTK; for example, why you
cannot change the background color of all widgets with the same method.

### Windows and events

Programs that run in a windowing system generally create rectangular regions
in the screen called windows. Traditional windowing systems do not
automatically save the graphical content of windows, and instead ask client
programs to repaint those windows whenever it is needed. For example, if a
window that is stacked below other windows gets raised to the top, then a
client program has to repaint the area that was previously obscured. When
the windowing system asks a client program to redraw part of a window, it
sends an exposure event to the program for that window.

Here, "windows" means "rectangular regions with automatic clipping", instead
of "toplevel application windows". Most windowing systems support nested
windows, where the contents of child windows get clipped by the boundaries
of their parents. Although GTK and GDK in particular may run on a windowing
system with no such notion of nested windows, GDK presents the illusion of
being under such a system. A toplevel window may contain many subwindows and
sub-subwindows, for example, one for the menu bar, one for the document
area, one for each scrollbar, and one for the status bar. In addition,
controls that receive user input, such as clickable buttons, are likely to
have their own subwindows as well.

In practice, most windows in modern GTK application are client-side
constructs. Only few windows (in particular toplevel windows) are native,
which means that they represent a window from the underlying windowing
system on which GTK is running. For example, on X11 it corresponds to a
`Window`; on Windows, it corresponds to a `HANDLE`.

Generally, the drawing cycle begins when GTK receives an exposure event from
the underlying windowing system: if the user drags a window over another
one, the windowing system will tell the underlying window that it needs to
repaint itself. The drawing cycle can also be initiated when a widget itself
decides that it needs to update its display. For example, when the user
types a character in a `GtkEntry` widget, the entry asks GTK to queue a
redraw operation for itself.

The windowing system generates events for native windows. The GDK interface
to the windowing system translates such native events into
[`struct@Gdk.Event`] structures and sends them on to the GTK layer. In turn,
the GTK layer finds the widget that corresponds to a particular
[`class@Gdk.Window`] and emits the corresponding event signals on that
widget.

The following sections describe how GTK decides which widgets need to be
repainted in response to such events, and how widgets work internally in
terms of the resources they use from the windowing system.

### The frame clock

All GTK applications are mainloop-driven, which means that most of the time
the app is idle inside a loop that just waits for something to happen and
then calls out to the right place when it does. On top of this GTK has a
frame clock that gives a “pulse” to the application. This clock beats at a
steady rate, which is tied to the framerate of the output (this is synced to
the monitor via the window manager/compositor). The clock has several
phases:

- Events
- Update
- Layout
- Paint

The phases happens in this order and we will always run each phase through
before going back to the start.

The *Events* phase is a long stretch of time between each redraw where we
get input events from the user and other events (like e.g. network I/O).
Some events, like mouse motion are compressed so that we only get a single
mouse motion event per clock cycle.

Once the *Events* phase is over we pause all external events and run the
redraw loop. First is the *Update* phase, where all animations are run to
calculate the new state based on the estimated time the next frame will be
visible (available via the frame clock). This often involves geometry
changes which drives the next phase, *Layout*. If there are any changes in
widget size requirements we calculate a new layout for the widget hierarchy
(i.e. we assign sizes and positions). Then we go to the *Paint* phase where
we redraw the regions of the window that need redrawing.

If nothing requires the *Update*/*Layout*/*Paint* phases we will stay in the
*Events* phase forever, as we don’t want to redraw if nothing changes. Each
phase can request further processing in the following phases (e.g. the
*Update* phase will cause there to be layout work, and layout changes cause
repaints).

There are multiple ways to drive the clock, at the lowest level you can
request a particular phase with [`method@Gdk.FrameClock.request_phase`]
which will schedule a clock beat as needed so that it eventually reaches the
requested phase. However, in practice most things happen at higher levels:

- If you are doing an animation, you can use
  [`method@Gtk.Widget.add_tick_callback`] which will cause a regular beating
  of the clock with a callback in the *Update* phase until you stop the
  tick.
- If some state changes that causes the size of your widget to change you
  call [`method@Gtk.Widget.queue_resize`] which will request a *Layout*
  phase and mark your widget as needing relayout.
- If some state changes so you need to redraw some area of your widget you
  use the normal [`method@Gtk.Widget.queue_draw`] set of functions. These
  will request a *Paint* phase and mark the region as needing redraw.

There are also a lot of implicit triggers of these from the CSS layer (which
does animations, resizes and repaints as needed).

Hierarchical drawing

During the Paint phase we will send a single expose event to the toplevel
window. The event handler will create a cairo context for the window and
emit a [`signal@Gtk.Widget::draw`] signal on it, which will propagate down
the entire widget hierarchy in back-to-front order, using the clipping and
transform of the Cairo context. This lets each widget draw its content at
the right place and time, correctly handling things like partial
transparencies and overlapping widgets.

When generating the event, GDK also sets up double buffering to avoid the
flickering that would result from each widget drawing itself in turn. the
section called [“Double buffering”](#double-buffering) describes the double
buffering mechanism in detail.

Normally, there is only a single Cairo context which is used in the entire
repaint, rather than one per `GdkWindow`. This means you have to respect
(and not reset) existing clip and transformations set on it.

Most widgets, including those that create their own `GdkWindow`s have a
transparent background, so they draw on top of whatever widgets are below
them. This was not the case in GTK 2 where the theme set the background of
most widgets to the default background color. (In fact, transparent
`GdkWindow`s used to be impossible.)

The whole rendering hierarchy is captured in the call stack, rather than
having multiple separate draw emissions, so you can use effects like e.g.
`cairo_push/pop_group()` which will affect all the widgets below you in the
hierarchy. This makes it possible to have e.g. partially transparent
containers.

### Scrolling

Traditionally, GTK has used self-copy operations to implement scrolling with
native windows. With transparent backgrounds, this no longer works. Instead,
we just mark the entire affected area for repainting when these operations
are used. This allows (partially) transparent backgrounds, and it also more
closely models modern hardware where self-copy operations are problematic
(they break the rendering pipeline).

Since the above causes some overhead, we introduce a caching mechanism.
Containers that scroll a lot (`GtkViewport`, `GtkTextView`, `GtkTreeView`,
etc) allocate an offscreen image during scrolling and render their children
to it (which is possible since drawing is fully hierarchical). The offscreen
image is a bit larger than the visible area, so most of the time when
scrolling it just needs to draw the offscreen in a different position. This
matches contemporary graphics hardware much better, as well as allowing
efficient transparent backgrounds. In order for this to work such containers
need to detect when child widgets are redrawn so that it can update the
offscreen. This can be done with the new
[`method@Gdk.Window.set_invalidate_handler`] function.

### Double buffering

If each of the drawing calls made by each subwidget's draw handler were sent
directly to the windowing system, flicker could result. This is because
areas may get redrawn repeatedly: the background, then decorative frames,
then text labels, etc. To avoid flicker, GTK employs a double buffering
system at the GDK level. Widgets normally don't know that they are drawing
to an off-screen buffer; they just issue their normal drawing commands, and
the buffer gets sent to the windowing system when all drawing operations are
done.

Two basic functions in GDK form the core of the double-buffering mechanism:
[`method@Gdk.Window.begin_paint_region`] and
[`method@Gdk.Window.end_paint`]. The first function tells a `GdkWindow` to
create a temporary off-screen buffer for drawing. All subsequent drawing
operations to this window get automatically redirected to that buffer. The
second function actually paints the buffer onto the on-screen window, and
frees the buffer.

### Automatic double buffering

It would be inconvenient for all widgets to call
`gdk_window_begin_paint_region()` and `gdk_window_end_paint()` at the
beginning and end of their draw handlers.

To make this easier, GTK normally calls `gdk_window_begin_paint_region()`
before emitting the `GtkWidget::draw` signal, and then it calls
`gdk_window_end_paint()` after the signal has been emitted. This is
convenient for most widgets, as they do not need to worry about creating
their own temporary drawing buffers or about calling those functions.

However, some widgets may prefer to disable this kind of automatic double
buffering and do things on their own. To do this, call the
[`method@Gtk.Widget.set_double_buffered`] function in your widget's
constructor. Double buffering can only be turned off for widgets that have a
native window.

#### Disabling automatic double buffering

```c
static void
my_widget_init (MyWidget *widget)
{
  ...

  gtk_widget_set_double_buffered (widget, FALSE);

  ...
}
```

When is it convenient to disable double buffering? Generally, this is the
case only if your widget gets drawn in such a way that the different drawing
operations do not overlap each other. For example, this may be the case for
a simple image viewer: it can just draw the image in a single operation.
This would not be the case with a word processor, since it will need to draw
and over-draw the page's background, then the background for highlighted
text, and then the text itself.

Even if you turn off double buffering on a widget, you can still call
`gdk_window_begin_paint_region()` and `gdk_window_end_paint()` by hand to
use temporary drawing buffers.

### App-paintable widgets

Generally, applications use the pre-defined widgets in GTK and they do not
draw extra things on top of them (the exception being [class@Gtk.DrawingArea]).
However, applications may sometimes find it convenient to draw directly on
certain widgets like toplevel windows or event boxes. When this is the case,
GTK needs to be told not to overwrite your drawing afterwards, when the
window gets to drawing its default contents.

[class@Gtk.Window] and [class@Gtk.EventBox] are the two widgets that allow
turning off drawing of default contents by calling
[`method@Gtk.Widget.set_app_paintable`]. If you call this function, they
will not draw their contents and let you do it instead.

Since the `GtkWidget::draw` signal runs user-connected handlers before the
widget's default handler, what usually happens is this:

- Your own draw handler gets run. It paints something on the window or the
  event box.
- The widget's default draw handler gets run. If
  `gtk_widget_set_app_paintable()` has not been called to turn off widget
  drawing (this is the default), your drawing will be overwritten. An app
  paintable widget will not draw its default contents however and preserve
  your drawing instead.
- The draw handler for the parent class gets run. Since both `GtkWindow` and
  `GtkEventBox` are descendants of [class@Gtk.Container], their no-window
  children will be asked to draw themselves recursively, as described in the
  section called [“Hierarchical drawing”](#hierarchical-drawing).

**Summary of app-paintable widgets.** Call `gtk_widget_set_app_paintable()`
if you intend to draw your own content directly on a `GtkWindow` and
`GtkEventBox`. You seldom need to draw on top of other widgets, and
`GtkDrawingArea` ignores this flag, as it is intended to be drawn on.
