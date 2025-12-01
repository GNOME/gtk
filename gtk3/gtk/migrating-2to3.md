Title: Migrating from GTK 2.x to GTK 3

# Migrating from GTK 2.x to GTK 3

GTK 3 is a major new version of GTK that breaks both API and ABI compared to
GTK 2.x, which has remained API- and ABI-stable for a long time. Thankfully,
most of the changes are not hard to adapt to and there are a number of steps
that you can take to prepare your GTK 2.x application for the switch to
GTK 3. After that, there's a small number of adjustments that you may have to
do when you actually switch your application to build against GTK 3.

## Preparations in GTK 2.x

The steps outlined in the following sections assume that your application is
working with GTK 2.24, which is the final stable release of GTK 2.x. It
includes all the necessary APIs and tools to help you port your application
to GTK 3. If you are still using an older version of GTK 2.x, you should
first get your application to build and work with 2.24.

### Do not include individual headers

With GTK 2.x it was common to include just the header files for a few
widgets that your application was using, which could lead to problems with
missing definitions, etc. GTK 3 tightens the rules about which header files
you are allowed to include directly. The allowed header files are are:

- `gtk/gtk.h`, for GTK
- `gtk/gtkx.h`, for the X-specfic widgets `GtkSocket` and `GtkPlug`
- `gtk/gtkunixprint.h`, for low-level, UNIX-specific printing functions
- `gdk/gdk.h`, for GDK
- `gdk/gdkx.h`, for GDK functions that are X11-specific
- `gdk/gdkwin32.h`, for GDK functions that are Windows-specific

(these relative paths are assuming that you are using the include paths that
are specified in the `gtk+-2.0.pc` file, as returned by `pkg-config --cflags
gtk+-2.0.pc`.)

To check that your application only includes the allowed headers, you can
use defines to disable inclusion of individual headers, as follows:

    make CFLAGS+="-DGTK_DISABLE_SINGLE_INCLUDES"

## Do not use deprecated symbols

Over the years, a number of functions, and in some cases, entire widgets
have been deprecated. These deprecations are clearly spelled out in the API
reference, with hints about the recommended replacements. The API reference
for GTK 2 also includes an index of all deprecated symbols.

To verify that your program does not use any deprecated symbols, you can use
defines to remove deprecated symbols from the header files, as follows:

    make CFLAGS+="-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"

Note that some parts of our API, such as enumeration values, are not well
covered by the deprecation warnings. In most cases, using them will require
you to also use deprecated functions, which will trigger warnings. But some
things, like the `GTK_DIALOG_NO_SEPARATOR` flag that has disappeared in GTK
3, may not.

### Use accessor functions instead of direct access

GTK 3 removes many implementation details and struct members from its public headers.

To ensure that your application does not have problems with this, you define
the preprocessor symbol `GSEAL_ENABLE` while building your application
against GTK 2.x. This will make the compiler catch all uses of direct access
to struct fields so that you can go through them one by one and replace them
with a call to an accessor function instead.

    make CFLAGS+="-DGSEAL_ENABLE"

While it may be painful to convert, this helps us keep API and ABI
compatibility when we change internal interfaces. As a quick example, when
adding `GSEAL_ENABLE`, if you see an error like:

    error: 'GtkToggleButton' has no member named 'active'

this means that you are accessing the public structure of `GtkToggleButton`
directly, perhaps with some code like:

```c
static void
on_toggled (GtkToggleButton *button)
{
  if (button->active)
    frob_active ();
  else
    frob_inactive ();
}
```

In most cases, this can easily be replaced with the correct accessor method.
The main rule is that if you have code like the above which accesses the
"active" field of a `GtkToggleButton`, then the accessor method becomes
`gtk_toggle_button_get_active()`:

```c
static void
on_toggled (GtkToggleButton *button)
{
  if (gtk_toggle_button_get_active (button))
    frob_active ();
  else
    frob_inactive ();
}
```

In the case of setting field members directly, there's usually a
corresponding setter method.

### Replace `GDK_<keyname>` with `GDK_KEY_<keyname>`

Key constants have gained a `_KEY_` infix. For example, `GDK_a` is now
`GDK_KEY_a`. In GTK 2, the old names continue to be available. In GTK 3
however, the old names will require an explicit include of the
`gdkkeysyms-compat.h` header.

### Use GIO for launching applications

The `gdk_spawn` family of functions has been deprecated in GDK 2.24 and
removed from GDK 3. Various replacements exist; the best replacement depends
on the circumstances:

- If you are opening a document or URI by launching a command like
  `firefox http://my-favourite-website.com` or `gnome-open ghelp:epiphany`,
  it is best to just use [`func@Gtk.show_uri_on_window`]; as an added benefit,
  your application will henceforth respect the users preference for what
  application to use and correctly open links in sandboxed applications.

- If you are launching a regular, installed application that has a desktop
  file, it is best to use GIOs GAppInfo with a suitable launch context:

  ```c
  GAppInfo *info;
  GAppLaunchContext *context;
  GError *error = NULL;

  info = (GAppInfo*) g_desktop_app_info_new ("epiphany.desktop");
  context = (GAppLaunchContext*) gdk_display_get_app_launch_context (display);
  g_app_info_launch (info, NULL, context, &error);

  if (error)
    {
      g_warning ("Failed to launch epiphany: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (info);
  g_object_unref (context);
  ```

  Remember that you have to include `gio/gdesktopappinfo.h` and use the
  `gio-unix-2.0` pkg-config file when using [`ctor@GioUnix.DesktopAppInfo.new`].

- If you are launching a custom commandline, you can still use [`method@Gio.AppInfo.launch`]
  with a [`iface@Gio.AppInfo`] that is constructed with [`func@Gio.AppInfo.create_from_commandline`],
  or you can use the more lowlevel `g_spawn` family of functions (e.g.
  [`func@GLib.spawn_command_line_async`], and pass `DISPLAY` in the environment.
  [`method@Gdk.Screen.make_display_name`] can be used to find the right value for
  the `DISPLAY` environment variable.

### Use cairo for drawing

In GTK 3, the GDK drawing API (which closely mimics the X drawing API, which
is itself modeled after PostScript) has been removed. All drawing in GTK 3
is done via [Cairo](https://cairographics.org).

The `GdkGC` and `GdkImage` objects, as well as all the functions using them,
are gone. This includes the `gdk_draw` family of functions like
`gdk_draw_rectangle()` and `gdk_draw_drawable()`. As `GdkGC` is roughly
equivalent to `cairo_t` and `GdkImage` was used for drawing images to
`GdkWindows`, which Cairo supports automatically, a transition is usually
straightforward.

The following examples show a few common drawing idioms used by applications
that have been ported to use cairo and how the code was replaced.

#### Drawing pixbufs

Drawing a pixbuf onto a drawable used to be done like this:

```c
gdk_draw_pixbuf (window,
                 gtk_widget_get_style (widget)->black_gc,
                 pixbuf,
                 0, 0
                 x, y,
                 gdk_pixbuf_get_width (pixbuf),
                 gdk_pixbuf_get_height (pixbuf),
                 GDK_RGB_DITHER_NORMAL,
                 0, 0);
```

Doing the same thing with cairo:

```c
cairo_t *cr = gdk_cairo_create (window);
gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
cairo_paint (cr);
cairo_destroy (cr);
```

Note that very similar code can be used when porting code using `GdkPixmap`
to `cairo_surface_t` by calling `cairo_set_source_surface()` instead of
`gdk_cairo_set_source_pixbuf()`.

#### Tiled pixmaps

Tiled pixmaps are often used for drawing backgrounds. Old code looked
something like this:

```c
GdkGCValues gc_values;
GdkGC *gc;

/* setup */
gc = gtk_widget_get_style (widget)->black_gc;
gdk_gc_set_tile (gc, pixmap);
gdk_gc_set_fill (gc, GDK_TILED);
gdk_gc_set_ts_origin (gc, x_origin, y_origin);

/* use */
gdk_draw_rectangle (window, gc, TRUE, 0, 0, width, height);

/* restore */
gdk_gc_set_tile (gc, NULL);
gdk_gc_set_fill (gc, GDK_SOLID);
gdk_gc_set_ts_origin (gc, 0, 0);
```

The equivalent cairo code to draw a tiled surface looks like this:

```c
cairo_t *cr;
cairo_surface_t *surface;

surface = ...
cr = gdk_cairo_create (window);
cairo_set_source_surface (cr, surface, x_origin, y_origin);
cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
cairo_rectangle (cr, 0, 0, width, height);
cairo_fill (cr);
cairo_destroy (cr);
```

The `surface` here can be either an image surface or a X surface, and can
either be created on the spot or kept around for caching purposes. Another
alternative is to use pixbufs instead of surfaces with
`gdk_cairo_set_source_pixbuf()` instead of `cairo_set_source_surface()`.

#### Clipped layouts

Drawing layouts clipped is often used to avoid overdraw or to allow drawing
selections. Code would have looked like this:

```
GdkGC *gc;

/* setup */
gc = gtk_widget_get_style (widget)->text_gc[state];
gdk_gc_set_clip_rectangle (gc, &area);

/* use */
gdk_draw_layout (drawable, gc, x, y, layout);

/* restore */
gdk_gc_set_clip_rectangle (gc, NULL);
```

With Cairo, the same effect can be achieved using:

```c
GtkStyleContext *context;
GtkStateFlags flags;
GdkRGBA rgba;
cairo_t *cr;

cr = gdk_cairo_create (drawable);

/* clip */
gdk_cairo_rectangle (cr, &area);
cairo_clip (cr);

/* set the correct source color */
context = gtk_widget_get_style_context (widget));
state = gtk_widget_get_state_flags (widget);
gtk_style_context_get_color (context, state, &rgba);
gdk_cairo_set_source_rgba (cr, &rgba);

/* draw the text */
cairo_move_to (cr, x, y);
pango_cairo_show_layout (cr, layout);
cairo_destroy (cr);
```

Clipping using `cairo_clip()` is of course not restricted to text rendering
and can be used everywhere where GC clips were used. And using
`gdk_cairo_set_source_color()` with style colors should be used in all the
places where a style’s GC was used to achieve a particular color.

#### What should you be aware of?

**No more stippling.** Stippling is the usage of a bi-level mask, called a
`GdkBitmap`. It was often used to achieve a checkerboard effect. You can use
`cairo_mask()` to achieve this effect. To get a checkerbox mask, you can use
code like this:

```c
static cairo_pattern_t *
gtk_color_button_get_checkered (void)
{
  /* need to respect pixman's stride being a multiple of 4 */
  static unsigned char data[8] = {
    0xFF, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0x00, 0x00
  };
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_A8,
                                                 2, 2,
                                                 4);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_surface_destroy (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

  return pattern;
}
```

Note that stippling looks very outdated in UIs, and is rarely used in modern
applications. All properties that made use of stippling have been removed
from GTK 3. Most prominently, stippling is absent from text rendering, in
particular `GtkTextTag`.

**Using the target also as source or mask.** The `gdk_draw_drawable()`
function allowed using the same drawable as source and target. This was
often used to achieve a scrolling effect. Cairo does not allow this yet. You
can however use `cairo_push_group()` to get a different intermediate target
that you can copy to. So you can replace this code:

```c
gdk_draw_drawable (pixmap,
                   gc,
                   pixmap,
                   area.x + dx, area.y + dy,
                   area.x, area.y,
                   area.width, area.height);
```

By using this code:

```c
cairo_t *cr = cairo_create (surface);

/* clipping restricts the intermediate surface's size, so it's a good idea
 * to use it. */
gdk_cairo_rectangle (cr, &area);
cairo_clip (cr);

/* Now push a group to change the target */
cairo_push_group (cr);
cairo_set_source_surface (cr, surface, dx, dy);
cairo_paint (cr);

/* Now copy the intermediate target back */
cairo_pop_group_to_source (cr);
cairo_paint (cr);
cairo_destroy (cr);
```

The surface here can be either an image surface or a X surface, and can
either be created on the spot or kept around for caching purposes. Another
alternative is to use pixbufs instead of surfaces with
`gdk_cairo_set_source_pixbuf()` instead of `cairo_set_source_surface()`. The
Cairo developers plan to add self-copies in the future to allow exactly this
effect, so you might want to keep up on Cairo development to be able to
change your code.

**Using pango_cairo_show_layout() instead of
gdk_draw_layout_with_colors().** GDK provided a way to ignore the color
attributes of text and use a hardcoded text color with the
`gdk_draw_layout_with_colors()` function. This is often used to draw text
shadows or selections. Pango’s Cairo support does not yet provide this
functionality. If you use Pango layouts that change colors, the easiest way
to achieve a similar effect is using `pango_cairo_layout_path()` and
`cairo_fill()` instead of `gdk_draw_layout_with_colors()`. Note that this
results in a slightly uglier-looking text, as subpixel anti-aliasing is not
supported.

## Changes that need to be done at the time of the switch

This section outlines porting tasks that you need to tackle when you get to
the point that you actually build your application against GTK 3. Making it
possible to prepare for these in GTK 2.24 would have been either impossible
or impractical.

### Replace size_request by get_preferred_width/height

The request-phase of the traditional GTK geometry management has been
replaced by a more flexible height-for-width system, which is described in
detail in the API documentation (see the section called “Height-for-width
Geometry Management” in the [`class@Gtk.Widget`] description). As a
consequence, the `::size-request` signal and virtual function have been
removed from the GtkWidget class. The replacement for `size_request()` can
take several levels of sophistication:

- As a minimal replacement to keep current functionality, you can simply
  implement the [`vfunc@Gtk.Widget.get_preferred_width`] and
  [`vfunc@Gtk.Widget.get_preferred_height`] virtual functions by calling
  your existing `size_request()` function. So you go from:

```c
static void
my_widget_class_init (MyWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  /* ... */

  widget_class->size_request = my_widget_size_request;

  /* ... */
}
```

to something that looks more like this:

```c
static void
my_widget_get_preferred_width (GtkWidget *widget,
                               gint      *minimal_width,
                               gint      *natural_width)
{
  GtkRequisition requisition;

  my_widget_size_request (widget, &requisition);

  *minimal_width = *natural_width = requisition.width;
}

static void
my_widget_get_preferred_height (GtkWidget *widget,
                                gint      *minimal_height,
                                gint      *natural_height)
{
  GtkRequisition requisition;

  my_widget_size_request (widget, &requisition);

  *minimal_height = *natural_height = requisition.height;
}

/* ... */

static void
my_widget_class_init (MyWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  /* ... */

  widget_class->get_preferred_width = my_widget_get_preferred_width;
  widget_class->get_preferred_height = my_widget_get_preferred_height;

  /* ... */
}
```

- Sometimes you can make things a little more streamlined by replacing your
  existing `size_request()` implementation by one that takes an orientation
  parameter:

```c
static void
my_widget_get_preferred_size (GtkWidget      *widget,
                              GtkOrientation  orientation,
                               gint          *minimal_size,
                               gint          *natural_size)
{

  /* do things that are common for both orientations ... */

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* do stuff that only applies to width... */

      *minimal_size = *natural_size = ...
    }
  else
   {
      /* do stuff that only applies to height... */

      *minimal_size = *natural_size = ...
   }
}

static void
my_widget_get_preferred_width (GtkWidget *widget,
                               gint      *minimal_width,
                               gint      *natural_width)
{
  my_widget_get_preferred_size (widget,
                                GTK_ORIENTATION_HORIZONTAL,
                                minimal_width,
                                natural_width);
}

static void
my_widget_get_preferred_height (GtkWidget *widget,
                                gint      *minimal_height,
                                gint      *natural_height)
{
  my_widget_get_preferred_size (widget,
                                GTK_ORIENTATION_VERTICAL,
                                minimal_height,
                                natural_height);
}
```

- If your widget can cope with a small size, but would appreciate getting
  some more space (a common example would be that it contains ellipsizable
  labels), you can do that by making your
  [`vfunc@Gtk.Widget.get_preferred_width`] / [`vfunc@GtkWidget.get_preferred_height`]
  functions return a smaller value for minimal than for natural. For minimal,
  you probably want to return the same value that your `size_request()`
  function returned before (since `size_request()` was defined as returning
  the minimal size a widget can work with). A simple way to obtain good values
  for natural, in the case of containers, is to use [`method@Gtk.Widget.get_preferred_width`]
  and [`method@Gtk.Widget.get_preferred_height`] on the children of the
  container, as in the following example:

```c
static void
gtk_fixed_get_preferred_height (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  GtkFixed *fixed = GTK_FIXED (widget);
  GtkFixedPrivate *priv = fixed->priv;
  GtkFixedChild *child;
  GList *children;
  gint child_min, child_nat;

  *minimum = 0;
  *natural = 0;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      gtk_widget_get_preferred_height (child->widget, &child_min, &child_nat);

      *minimum = MAX (*minimum, child->y + child_min);
      *natural = MAX (*natural, child->y + child_nat);
    }
}
```

- Note that the [`vfunc@Gtk.Widget.get_preferred_width`] /
  [`vfunc@Gtk.Widget.get_preferred_height`] functions only allow you to deal
  with one dimension at a time. If your `size_request()` handler is doing
  things that involve both width and height at the same time (e.g. limiting
  the aspect ratio), you will have to implement
  [`vfunc@Gtk.Widget.get_preferred_height_for_width`] and
  [`vfunc@Gtk.Widget.get_preferred_width_for_height`].

- To make full use of the new capabilities of the height-for-width geometry
  management, you need to additionally implement the
  [`vfunc@Gtk.Widget.get_preferred_height_for_width`] and
  [`vfunc@Gtk.Widget.get_preferred_width_for_height`]. For details on these
  functions, see the section called “Height-for-width Geometry Management”
  in the [`class@Gtk.Widget`] documentation.

### Replace `GdkRegion` with `cairo_region_t`

Starting with version 1.10, cairo provides a region API that is equivalent
to the GDK region API (which was itself copied from the X server).
Therefore, the region API has been removed in GTK 3.

Porting your application to the cairo region API should be a straight
find-and-replace task. Please refer to the following table:

| GDK                            | Cairo                                                              |
|--------------------------------|--------------------------------------------------------------------|
| `GdkRegion`                    | `cairo_region_t`                                                   |
| `GdkRectangle`                 | `cairo_rectangle_int_t`                                            |
| `gdk_rectangle_intersect()`    | this function is still available                                   |
| `gdk_rectangle_union()`        | this function is still available                                   |
| `gdk_region_new()`             | `cairo_region_create()`                                            |
| `gdk_region_copy()`            | `cairo_region_copy()`                                              |
| `gdk_region_destroy()`         | `cairo_region_destroy()`                                           |
| `gdk_region_rectangle()`       | `cairo_region_create_rectangle()`                                  |
| `gdk_region_get_clipbox()`     | `cairo_region_get_extents()`                                       |
| `gdk_region_get_rectangles()`  | `cairo_region_num_rectangles()` and `cairo_region_get_rectangle()` |
| `gdk_region_empty()`           | `cairo_region_is_empty()`                                          |
| `gdk_region_equal()`           | `cairo_region_equal()`                                             |
| `gdk_region_point_in()`        | `cairo_region_contains_point()`                                    |
| `gdk_region_rect_in()`         | `cairo_region_contains_rectangle()`                                |
| `gdk_region_offset()`          | `cairo_region_translate()`                                         |
| `gdk_region_union_with_rect()` | `cairo_region_union_rectangle()`                                   |
| `gdk_region_intersect()`       | `cairo_region_intersect()`                                         |
| `gdk_region_union()`           | `cairo_region_union()`                                             |
| `gdk_region_subtract()`        | `cairo_region_subtract()`                                          |
| `gdk_region_xor()`             | `cairo_region_xor()`                                               |
| `gdk_region_shrink()`          | no replacement                                                     |
| `gdk_region_polygon()`         | no replacement, use Cairo paths instead                            |

### Replace GdkPixmap by cairo surfaces

The GdkPixmap object and related functions have been removed. In the
Cairo-centric world of GTK 3, Cairo surfaces take over the role of pixmaps.

One place where pixmaps were commonly used is to create custom cursors:

```c
GdkCursor *cursor;
GdkPixmap *pixmap;
cairo_t *cr;
GdkColor fg = { 0, 0, 0, 0 };

pixmap = gdk_pixmap_new (NULL, 1, 1, 1);

cr = gdk_cairo_create (pixmap);
cairo_rectangle (cr, 0, 0, 1, 1);
cairo_fill (cr);
cairo_destroy (cr);

cursor = gdk_cursor_new_from_pixmap (pixmap, pixmap, &fg, &fg, 0, 0);

g_object_unref (pixmap);
```

The same can be achieved without pixmaps, by drawing onto an image surface:

```
GdkCursor *cursor;
cairo_surface_t *s;
cairo_t *cr;
GdkPixbuf *pixbuf;

s = cairo_image_surface_create (CAIRO_FORMAT_A1, 3, 3);
cr = cairo_create (s);
cairo_arc (cr, 1.5, 1.5, 1.5, 0, 2 * M_PI);
cairo_fill (cr);
cairo_destroy (cr);

pixbuf = gdk_pixbuf_get_from_surface (s,
                                      0, 0,
                                      3, 3);

cairo_surface_destroy (s);

cursor = gdk_cursor_new_from_pixbuf (display, pixbuf, 0, 0);

g_object_unref (pixbuf);
```

### Replace `GdkColormap` with `GdkVisual`

For drawing with Cairo, it is not necessary to allocate colors, and a
[`class@Gdk.Visual`] provides enough information for Cairo to handle colors
in 'native' surfaces. Therefore, `GdkColormap` and related functions have
been removed in GTK 3, and visuals are used instead. The colormap-handling
functions of `GtkWidget` (`gtk_widget_set_colormap()`, etc) have been
removed and [`method@Gtk.Widget.set_visual`] has been added.

#### Translucent windows

You might have a screen-changed handler like the following to set up a
translucent window with an alpha-channel:

```c
static void
on_alpha_screen_changed (GtkWidget *widget,
                         GdkScreen *old_screen,
                         GtkWidget *label)
{
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GdkColormap *colormap = gdk_screen_get_rgba_colormap (screen);

  if (colormap == NULL)
    colormap = gdk_screen_get_default_colormap (screen);

  gtk_widget_set_colormap (widget, colormap);
}
```

With visuals instead of colormaps, this will look as follows:

```c
static void
on_alpha_screen_changed (GtkWindow *window,
                         GdkScreen *old_screen,
                         GtkWidget *label)
{
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (window));
  GdkVisual *visual = gdk_screen_get_rgba_visual (screen);

  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  gtk_widget_set_visual (window, visual);
}
```

### `GdkDrawable` is gone

`GdkDrawable` has been removed in GTK 3, together with `GdkPixmap` and
`GdkImage`. The only remaining drawable class is [`class@Gdk.Window`]. For
dealing with image data, you should use a `cairo_surface_t` or a
[`class@GdkPixbuf.Pixbuf`].

`GdkDrawable` functions that are useful with windows have been replaced by
corresponding `GdkWindow` functions:

| GDK 2.x                             | GDK 3                                                  |
|-------------------------------------|--------------------------------------------------------|
| `gdk_drawable_get_visual()`         | `gdk_window_get_visual()`                              |
| `gdk_drawable_get_size()`           | `gdk_window_get_width()` and `gdk_window_get_height()` |
| `gdk_pixbuf_get_from_drawable()`    | `gdk_pixbuf_get_from_window()`                         |
| `gdk_drawable_get_clip_region()`    | `gdk_window_get_clip_region()`                         |
| `gdk_drawable_get_visible_region()` | `gdk_window_get_visible_region()`                      |

### Event filtering

If your application uses the low-level event filtering facilities in GDK,
there are some changes you need to be aware of.

The special-purpose `GdkEventClient` events and the
`gdk_add_client_message_filter()` and
`gdk_display_add_client_message_filter()` functions have been removed.
Receiving X11 ClientMessage events is still possible, using the general
[`method@Gdk.Window.add_filter`] API. A client message filter like:

```c
static GdkFilterReturn
message_filter (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
  XClientMessageEvent *evt = (XClientMessageEvent *)xevent;

  /* do something with evt ... */
}

/* ... */

message_type = gdk_atom_intern ("MANAGER", FALSE);
gdk_display_add_client_message_filter (display, message_type, message_filter, NULL);
```

then looks like this:

```c
static GdkFilterReturn
event_filter (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
  XClientMessageEvent *evt;
  GdkAtom message_type;

  if (((XEvent *)xevent)->type != ClientMessage)
    return GDK_FILTER_CONTINUE;

  evt = (XClientMessageEvent *)xevent;
  message_type = XInternAtom (evt->display, "MANAGER", FALSE);

  if (evt->message_type != message_type)
    return GDK_FILTER_CONTINUE;

  /* do something with evt ... */
}

/* ... */

gdk_window_add_filter (NULL, message_filter, NULL);
```

One advantage of using an event filter is that you can actually remove the
filter when you don't need it anymore, using
[`method@Gdk.Window.remove_filter`].

The other difference to be aware of when working with event filters in GTK 3
is that GDK now uses the XInput 2 extension by default when available. That
means that your application does not receive core X11 key or button events.
Instead, all input events are delivered as `XIDeviceEvent`s. As a short-term
workaround for this, you can force your application to not use XI2, with
[`func@Gdk.disable_multidevice`]. In the long term, you probably want to rewrite
your event filter to deal with `XIDeviceEvent`s.

### Backend-specific code

In GTK 2.x, GDK could only be compiled for one backend at a time, and the
`GDK_WINDOWING_X11` or `GDK_WINDOWING_WIN32` macros could be used to find
out which one you are dealing with:

```c
#ifdef GDK_WINDOWING_X11
      if (timestamp != GDK_CURRENT_TIME)
        gdk_x11_window_set_user_time (gdk_window, timestamp);
#endif
#ifdef GDK_WINDOWING_WIN32
        /* ... win32 specific code ... */
#endif
```

In GTK 3, GDK can be built with multiple backends, and currently used
backend has to be determined at runtime, typically using type-check macros
on a [`class@Gdk.Display`] or [`class@Gdk.Window`]. You still need to use
the `GDK_WINDOWING_*` macros to only compile code referring to supported
backends:

```c
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_DISPLAY (display))
        {
          if (timestamp != GDK_CURRENT_TIME)
            gdk_x11_window_set_user_time (gdk_window, timestamp);
        }
      else
#endif
#ifdef GDK_WINDOWING_WIN32
      if (GDK_IS_WIN32_DISPLAY (display))
        {
          /* ... win32 specific code ... */
        }
      else
#endif
       {
         g_warning ("Unsupported GDK backend");
       }
```

If you used the `pkg-config` variable target to conditionally build part of your project depending on the GDK backend, for instance like this:

    AM_CONDITIONAL(BUILD_X11, test `$PKG_CONFIG --variable=target gtk+-2.0` = "x11")

then you should now use the M4 macro provided by GTK itself:

    GTK_CHECK_BACKEND([x11], [3.0.2], [have_x11=yes], [have_x11=no])
    AM_CONDITIONAL(BUILD_x11, [test "x$have_x11" = "xyes"])

### `GtkPlug` and `GtkSocket`

The `GtkPlug` and `GtkSocket` widgets are now X11-specific, and you have to
include the `<gtk/gtkx.h>` header to use them. The previous section about
proper handling of backend-specific code applies, if you care about other
backends.

### The `GtkWidget::draw` signal

The `GtkWidget` “expose-event” signal has been replaced by a new
[`signal@Gtk.Widget::draw`] signal, which takes a `cairo_t` instead of an
expose event. The Cairo context is being set up so that the origin at (0, 0)
coincides with the upper left corner of the widget, and is properly clipped.

In other words, the Cairo context of the draw signal is set up in 'widget
coordinates', which is different from traditional expose event handlers,
which always assume 'window coordinates'.

The widget is expected to draw itself with its allocated size, which is
available via the new [`method@Gtk.Widget.get_allocated_width`] and
[`method@Gtk.Widget.get_allocated_height`] functions. It is not necessary to check
for `gtk_widget_is_drawable()`, since GTK already does this check before
emitting the “draw” signal.

There are some special considerations for widgets with multiple windows.
Expose events are window-specific, and widgets with multiple windows could
expect to get an expose event for each window that needs to be redrawn.
Therefore, multi-window expose event handlers typically look like this:

```c
if (event->window == widget->window1)
  {
     /* ... draw window1 ... */
  }
else if (event->window == widget->window2)
  {
     /* ... draw window2 ... */
  }
```

In contrast, the “draw” signal handler may have to draw multiple windows in
one call. GTK has a convenience function [`func@Gtk.cairo_should_draw_window`]
that can be used to find out if a window needs to be drawn. With that, the
example above would look like this (note that the 'else' is gone):

```c
if (gtk_cairo_should_draw_window (cr, widget->window1)
  {
     /* ... draw window1 ... */
  }

if (gtk_cairo_should_draw_window (cr, widget->window2)
  {
     /* ... draw window2 ... */
  }
```

Another convenience function that can help when implementing `::draw` for
multi-window widgets is [`func@Gtk.cairo_transform_to_window`], which
transforms a cairo context from widget-relative coordinates to
window-relative coordinates. You may want to use `cairo_save()` and
`cairo_restore()` when modifying the cairo context in your draw function.

All `GtkStyle` drawing functions (`gtk_paint_box()`, etc) have been changed
to take a `cairo_t` instead of a window and a clip area. `::draw`
implementations will usually just use the Cairo context that has been passed
in for this.

#### A simple `::draw` function

```c
gboolean
gtk_arrow_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkStyleContext *context;
  gint x, y;
  gint width, height;
  gint extent;

  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  extent = MIN (width - 2 * PAD, height - 2 * PAD);
  x = PAD;
  y = PAD;

  gtk_render_arrow (context, rc, G_PI / 2, x, y, extent);
}
```

### GtkProgressBar orientation

In GTK 2.x, `GtkProgressBar` and `GtkCellRendererProgress` were using the `GtkProgressBarOrientation` enumeration to specify their orientation and direction. In GTK 3, both the widget and the cell renderer implement [`iface@Gtk.Orientable`], and have an additional 'inverted' property to determine their direction. Therefore, a call to `gtk_progress_bar_set_orientation()` needs to be replaced by a pair of calls to [`method@Gtk.Orientable.set_orientation`] and [`method@Gtk.ProgressBar.set_inverted`]. The following values correspond:

<table class="table" border="1">
<colgroup>
<col class="1">
<col class="2">
<col class="3">
</colgroup>
<thead>
<tr>
<th>GTK 2.x</th>
<th colspan="2">GTK 3</th>
</tr>
<tr>
<th>`GtkProgressBarOrientation`</th>
<th>`GtkOrientation`</th>
<th>inverted</th>
</tr>
</thead>
<tbody>
<tr>
<td>`GTK_PROGRESS_LEFT_TO_RIGHT`</td>
<td>`GTK_ORIENTATION_HORIZONTAL`</td>
<td>`FALSE`</td>
</tr>
<tr>
<td>`GTK_PROGRESS_RIGHT_TO_LEFT`</td>
<td>`GTK_ORIENTATION_HORIZONTAL`</td>
<td>`TRUE`</td>
</tr>
<tr>
<td>`GTK_PROGRESS_TOP_TO_BOTTOM`</td>
<td>`GTK_ORIENTATION_VERTICAL`</td>
<td>`FALSE`</td>
</tr>
<tr>
<td>`GTK_PROGRESS_BOTTOM_TO_TOP`</td>
<td>`GTK_ORIENTATION_VERTICAL`</td>
<td>`TRUE`</td>
</tr>
</tbody>
</table>

### Check your expand and fill flags

The behaviour of expanding widgets has changed slightly in GTK 3, compared
to GTK 2.x. It is now 'inherited', i.e. a container that has an expanding
child is considered expanding itself. This is often the desired behaviour.
In places where you don't want this to happen, setting the container
explicity as not expanding will stop the expand flag of the child from being
inherited. See [`method@Gtk.Widget.set_hexpand`] and
[`method@Gtk.Widget.set_vexpand`].

If you experience sizing problems with widgets in ported code, carefully
check the `GtkBox` "expand" and "fill" child properties of your boxes.

### Scrolling changes

The default values for the “hscrollbar-policy” and “vscrollbar-policy”
properties have been changed from 'never' to 'automatic'. If your
application was relying on the default value, you will have to set it
explicitly.

The `::set-scroll-adjustments` signal on `GtkWidget` has been replaced by
the [`iface@Gtk.Scrollable`] interface which must be implemented by a widget
that wants to be placed in a [`class@Gtk.ScrolledWindow`]. Instead of
emitting `::set-scroll-adjustments`, the scrolled window simply sets the
[`property@Gtk.Scrollable:hadjustment`] and
[`property@Gtk.Scrollable:vadjustment`] properties.

### `GtkObject` is gone

`GtkObject` has been removed in GTK 3. Its remaining functionality, the
`::destroy` signal, has been moved to `GtkWidget`. If you have non-widget
classes that are directly derived from `GtkObject`, you have to make them
derive from [`class@GObject.InitiallyUnowned`]; or, if you don't need the
floating functionality, `GObject`. If you have widgets that override the
destroy class handler, you have to adjust your `class_init` function, since
destroy is now a member of `GtkWidgetClass`:

```c
static void
your_widget_class_init (YourWidgetClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

  object_class->destroy = your_widget_destroy;

  /* ... */
}
```

becomes

```
static void
your_widget_class_init (YourWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->destroy = your_widget_destroy;

  /* ... */
}
```

In the unlikely case that you have a non-widget class that is derived from
`GtkObject` and makes use of the "destroy" functionality, you have to
implement `::destroy` yourself.

If your program used functions like `gtk_object_get()` or
`gtk_object_set()`, these can be replaced directly with `g_object_get()` or
`g_object_set()`. In fact, most every `gtk_object_*` function can be
replaced with the corresponding `g_object_*` function, even in GTK 2 code.
The one exception to this rule is `gtk_object_destroy()`, which can be
replaced with `gtk_widget_destroy()`, again in both GTK 2 and GTK 3.

### GtkEntryCompletion signal parameters

The “match-selected” and “cursor-on-match” signals were erroneously given
the internal filter model instead of the users model. This oversight has
been fixed in GTK 3; if you have handlers for these signals, they will
likely need slight adjustments.

### Resize grips

The resize grip functionality has been moved from [`class@Gtk.Statusbar`] to
[`class@Gtk.Window`]. Any window can now have resize grips, regardless
whether it has a statusbar or not. The functions
`gtk_statusbar_set_has_resize_grip()` and
`gtk_statusbar_get_has_resize_grip()` have disappeared, and instead there
are now [`method@Gtk.Window.set_has_resize_grip`] and
[`method@Gtk.Window.get_has_resize_grip`].

In more recent versions of GTK 3, the resize grip functionality has been
removed entirely, in favor of invisible resize borders around the window.
When updating to newer versions of GTK 3, you should simply remove all code
dealing with resize grips.

### Prevent mixed linkage

Linking against GTK 2.x and GTK 3 in the same process is problematic and can
lead to hard-to-diagnose crashes. The `gtk_init()` function in both GTK 2.24
and in GTK 3 tries to detect this situation and abort with a diagnostic
message, but this check is not 100% reliable (e.g. if the problematic
linking happens only in loadable modules).

Direct linking of your application against both versions of GTK is easy to
avoid; the problem gets harder when your application is using libraries that
are themselves linked against some version of GTK. In that case, you have to
verify that you are using a version of the library that is linked against
GTK 3.

If you are using packages provided by a distributor, it is likely that
parallel installable versions of the library exist for GTK 2.x and GTK 3,
e.g for vte, check for vte3; for webkitgtk look for webkitgtk3, and so on.

### Install GTK modules in the right place

Some software packages install loadable GTK modules such as theme engines,
gdk-pixbuf loaders or input methods. Since GTK 3 is parallel-installable
with GTK 2.x, the two GTK versions have separate locations for their
loadable modules. The location for GTK 2.x is `libdir/gtk-2.0` (and its
subdirectories), for GTK 3 the location is `libdir/gtk-3.0` (and its
subdirectories).

For some kinds of modules, namely input methods and pixbuf loaders, GTK
keeps a cache file with extra information about the modules. For GTK 2.x,
these cache files are located in `sysconfdir/gtk-2.0`. For GTK 3, they have
been moved to `libdir/gtk-3.0/3.0.0/`. The commands that create these cache
files have been renamed with a `-3` suffix to make them parallel-installable.

Note that GTK modules often link against libgtk, libgdk-pixbuf, etc. If that
is the case for your module, you have to be careful to link the GTK 2.x
version of your module against the 2.x version of the libraries, and the GTK
3 version against hte 3.x versions. Loading a module linked against libgtk
2.x into an application using GTK 3 will lead to unhappiness and must be
avoided.
