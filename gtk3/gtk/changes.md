Title: Changes in GTK 3

# Changes in GTK 3

GTK 3 has seen considerable development over multiple stable releases. While
we try to maintain compatibility as far as possible, some minor adjustments
may be necessary when moving an application from one release of GTK 3 to
the next.

The following sections list noteworthy changes in GTK 3.x release that may
or may not require applications to be updated. 

## Changes in GTK 3.2

The accessible implementations for GTK widgets have been integrated into
libgtk itself, and the gail module does not exist anymore. This change
should not affect applications very much.

## Changes in GTK 3.4

Scroll events have been separated from button events, and smooth scrolling
has been added with a separate event mask. Widgets now need to have either
`GDK_SCROLL_MASK` or `GDK_SMOOTH_SCROLL_MASK` in their event mask to receive
scroll events. In addition, the `GdkScrollDirection` enumeration has gained
a new member, `GDK_SCROLL_SMOOTH`, so switch statements will have to be
amended to cover this case.

GTK now uses `<Primary>` instead of `<Control>` in keyboard accelerators,
for improved cross-platform handling. This should not affect applications,
unless they parse or create these accelerator manually.

The tacit assumption that the <kbd>Alt</kbd> key corresponds to the `MOD1`
modifier under X11 is now a hard requirement.

The Beagle search backend for the file chooser has been dropped.
[Tracker](https://gnome.pages.gitlab.gnome.org/tracker/) is the only
supported search backend on Linux now.

`GtkNotebook` has been changed to destroy its action widgets when it gets
destroyed itself. If your application is using action widgets in notebooks,
you may have to adjust your code to take this into account.

`GtkApplication` no longer uses the `gtk_` mainloop wrappers, so it is no
longer possible to use `gtk_main_quit()` to stop it.

The `-uninstalled` variants of the pkg-config files have been dropped.

Excessive dependencies have been culled from `Requires:` lines in the
pkg-config files. Dependent modules may have to declare dependencies that
there were getting 'for free' in the past.

## Changes in GTK 3.6

The accessibility bridge code that exports accessible objects on the bus is
now used by default; atk-bridge has been converted into a library that GTK
links against. To void the linking, pass --without-atk-bridge when
configuring GTK.

GDK threading support has been deprecated. It is recommended to use
`g_idle_add()`, `g_main_context_invoke()` and similar funtions to make all GTK
calls from the main thread.

GTK now follows the [XDG Base
Directory](https://specifications.freedesktop.org/basedir-spec/latest/)
specification for user configuration and data files. In detail,

- `$XDG_CONFIG_HOME/gtk-3.0/custom-papers` is the new location for
  `$HOME/.gtk-custom-papers`
- `$XDG_CONFIG_HOME/gtk-3.0/bookmarks` is the new location for
  `$HOME/.gtk-bookmarks`
- `$XDG_DATA_HOME/themes` is preferred over `$HOME/.themes`
- `$XDG_DATA_HOME/icons` is preferred over `$HOME/.icons`.

Existing files from the old location will still be read if the new location
does not exist.

`$HOME/.gtk-3.0` is no longer in the default module load path. If you want
to load modules from there, add it to the `GTK_PATH` environment variable.

## Changes in GTK 3.8

`GtkIconInfo` has changed from being a boxed type to a `GObject`. This is
technically an ABI change, but basically all existing code will keep working
if its used as a boxed type, and it is not possible to instantiate
`GtkIconInfo`s outside GTK, so this is not expected to be a big problem.

## Changes in GTK 3.10

GDK has been changed to allow only a single "screen" per display. Only the
X11 backend had multiple screens before, and multi-screen setups (not
multi-monitor!) are very rare nowadays. If you really need multiple X
screens, open them as separate displays.

The behavior of the [`GtkBox:expand`] child property has been changed to
never propagate up. Previously, this was happening inconsistently. If you
want the expand to propagate, use the GtkWidget h/vexpand properties. If
you experience sizing problems with widgets in ported code, carefully check
the expand and fill flags of your boxes.

`GtkBin` no longer provides default implementations for
`get_height_for_width`, subclasses now have to provide their own
implementation if they need height-for-width functionality.

Widget state propagation has been changed. Historically, all of active,
prelight, selected, insensitive, inconsistent and backdrop have been
propagated to children. This has now been restricted to just the insensitive
and backdrop states. This mostly affects theming.

The way widget drawing happens has changed. Earlier versions handled one
expose event per `GdkWindow`, each with a separate `cairo_t`. Now we only
handle the expose event on the toplevel and reuse the same `cairo_t` (with
the right translation and clipping) for the entire widget hierarchy,
recursing down via the [`signal@Gtk.Widget::draw`] signal. Having all
rendering in the same call tree allows effects like opacity and offscreen
rendering of entire widget sub-hierarchies. Generally this should not
require any changes in widgets, but code looking at e.g. the current expose
event may see different behavior than before.

The GTK scrolling implementation has changed. [`method@Gdk.Window.scroll`]
and [`method@Gdk.Window.move_region`] no longer copy the region on the
window, but rather invalidate the entire scrolled region. This is slightly
slower, but allowed us to implement a offscreen surface scrolling method
which better fits modern hardware. Most scrolling widgets in GTK have been
converted to use this model for scrolling, but external widgets implementing
scrolling using `GdkWindow` may see some slowdown.

## Changes in GTK 3.12

`GtkWidget` had a hack where if opacity is 0.999 we set up an opacity group
when rendering the widget. This is no longer needed in 3.10, and `GtkStack`
doesn't use it anymore. It has been removed in 3.12. `GdStack` is using it,
so applications should be ported from `GdStack` to `GtkStack` in 3.12.

`GtkHeaderBar` in 3.10 was not ordering its pack-end children in the right
way. This has been fixed in 3.12. Applications which pack multiple widgets
at the end of a headerbar will have to be updated.

[`method@Gtk.TextView.add_child_in_window`] has changed behaviour a bit. It
now always positions the child in buffer coordinates, where it used to
inconsistently scroll with the buffer but then go reposition to a
window-relative position on redraw.

A number of container widgets have been made more compliant with the uniform
CSS rendering model by making them render backgrounds and borders. This may
require some adjustments in applications that were making assumptions about
containers never rendering backgrounds.

## Changes in GTK 3.14

A new state, `GTK_STATE_FLAG_CHECKED`, has been added for checked states of
radio and check buttons and menuitems. Applications that are using GTK
styles without widgets will need adjustments.

Adwaita is now the default theme on all platforms.

The icon theme code has become a little pickier about sizes and is not
automatically scaling icons beyond the limits defined in the icon theme
unless explicitly asked to do so with `GTK_ICON_LOOKUP_FORCE_SIZE`.

GTK now includes an interactive debugger which can be activated with the
keyboard shortcuts <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd> or
<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>I</kbd>. If these shortcuts interfere
with application keybindings, they can be disabled with the
`enable-inspector-keybinding` setting in the `org.gtk.Settings.Debug`
GSettings schema.

Most widgets have been ported to use the new gesture framework internally
for event handling. Traditional event handlers in derived widgets are still
being called.

Using GTK under X11 without the X Render extension has been observed to be
problematic. This combination is using code paths in Cairo and graphics
drivers which are rarely tested and likely buggy.

`GtkTextView` is now using a pixel-cache internally, and is drawing a
background underneath the text. This can cause problems for applications
which assumed that they could draw things below and above the text by
chaining up in the `::draw` implementation of their `GtkTextView` subclass.
As a short-term workaround, you can make the application apply a custom
theme to the text view with a transparent background. For a proper fix, use
the new `::draw_layer` vfunc.

## Changes in GTK 3.16

GTK now includes an OpenGL rendering widget. To support GL on various
platforms, GTK uses [libepoxy](https://github.com/anholt/libepoxy).

GTK no longer uses `gtk-update-icon-cache` during its build. The
`--enable-gtk2-dependency` configure option has been removed.

The introspection annotations for the x and y parameters of
`GtkMenuPositionFunc` have been corrected from 'out' to 'inout'. If you are
using such a function from language-bindings, this may require adjustments.

The lookup order for actions that are activated via keyboard accelerators
has been changed to start at the currently focused widget. If your
application is making use fo nested action groups via
`gtk_widget_insert_action_group()`, you may want to check that this change
does not upset your accelerators.

The `GtkScrollable` interface has gained a new vfunc, `get_border()`, that
is used to position overshoot and undershoot indications that are drawn over
the content by `GtkScrolledWindow`. Unless your scrollable has non-scrolling
parts similar to treeview headers, there is no need to implement this vfunc.

The `GtkSearchEntry` widget has gained a number of new signals that are
emitted when certain key sequences are seen. In particular, it now handles
the <kbd>Esc</kbd> key and emits `::stop-search`. Applications that expect
to handle <kbd>Esc</kbd> themselves will need to be updated.

## Changes in GTK 3.18

The GtkListBox model support that was introduced in 3.16 has been changed to
no longer call `gtk_widget_show_all()` on rows created by the
`create_widget_func`. You need to manage the visibility of child widgets
yourself in your `create_widget_func`.

The alpha component of foreground colors that are applied to
`GtkCellRendererText` is no longer ignored. If you don't want your text to be
translucent, use opaque colors.

## Changes in GTK 3.20

The way theming works in GTK has been reworked fundamentally, to implement
many more CSS features and make themes more expressive. As a result, custom
CSS that is shipped with applications and third-party themes will need
adjustments. Widgets now use element names much more than style classes;
type names are no longer used in style matching. Every widget now documents
the element names it has and the style classes it uses. The GTK inspector
can also help with finding this information.

GTK now uses internal subobjects (also known as gadgets) for allocating and
drawing widget parts. Applications that subclass GTK widgets may see
warnings if they override the `size_allocate` vfunc and don't chain up. The
proper way to subclass is to chain up in `size_allocate`. If you do not want
to do that for some reason, you have to override the `draw` vfunc as well.

Several fixes for window sizing and window placement with client-side
decorations may affect applications that are saving and restoring window
sizes. The recommended best practice for this which is known to work with
client-side and server-side decorations and with older and newer versions of
GTK is to use `gtk_window_get_size()` to save window sizes and
`gtk_window_set_default_size()` to restore it. See
<https://wiki.gnome.org/HowDoI/SaveWindowState> for a detailed example.

Geometry handling in `GtkWindow` has been removed. If you are using the
functions `gtk_window_resize_to_geometry()`,
`gtk_window_set_default_geometry()`, `gtk_window_parse_geometry()` or
`gtk_window_set_geometry_hints()`, you may need to make some changes to your
code.

`GtkDrawingArea` used to implicitly render the theme background before
calling the `::draw` handler. This is no longer the case. If you rely on
having a theme-provided background, call `gtk_render_background()` from your
`::draw` handler.

The `GtkFileChooser` interface prerequisite changed from `GtkWidget` to
`GObject`, allowing non-widget implementations of this interface. This is a
minor change in ABI, as applications are no longer guaranteed that a
`GtkFileChooser` also supports all `GtkWidget` methods. However, all
previously existing implementations still derive from `GtkWidget`, so no
existing code should break.

The way in which `GtkLevelBar` determines the offset to apply was a bit
inconsistent in the past; this has been fixed. Applications that are using
custom offsets should double-check that their levels look as expected.

## Changes in GTK 3.22

The CSS parser has gotten a bit more selective in what it accepts as valid
values for the `font:` shorthand property. Following the CSS specification,
at least a size and a family name are required now. If you want to change an
individual facet of the font, like the weight, use the individual CSS
properties: `font-weight`, `font-size`, `font-family`, etc.

The CSS parser now warns about some more constructs that are not according
to the CSS spec, such as gradients with a single color stop. Instead, you
can just use `image(<color>)`.

The “ignore-hidden” property has not been working properly for a long time,
and we've not documented it as broken and deprecated. The recommended
alternative for reserving space of widgets that are not currently shown in
the UI is to use a `GtkStack` (with some 'filler' widget, e.g. an empty
`GtkBox`, in another page).

`GtkHeaderBar` now respects the hexpand property for its custom title widget
and its packed children. This change may inadvertently cause the layout of
those children to change, if they unintentionally had hexpand set before.

The behavior of the expand flag in `GtkTable`'s `GtkAttachOptions` has been
changed to (again) match the behavior in `GtkBox` and in GTK 2.x. These
options don't cause the table itself to expand.

The way `GtkPopover` behaved during a call to `gtk_widget_hide()` violated
some of the internal assumptions GTK makes about widget visibility.
`gtk_popover_popup()` and `gtk_popover_popdown()` have been introduced to
show or hide the popover with a transition, while `gtk_widget_show()` and
`gtk_widget_hide()` on a GtkPopover now work the same way they do on any
other widget and immediately hide (or show) the popover.
