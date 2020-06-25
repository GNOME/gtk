# Migrating from GTK 3.x to GTK 4 {#gtk-migrating-3-to-4}

GTK 4 is a major new version of GTK that breaks both API and ABI
compared to GTK 3.x. Thankfully, most of the changes are not hard
to adapt to and there are a number of steps that you can take to
prepare your GTK 3.x application for the switch to GTK 4. After
that, there's a number of adjustments that you may have to do
when you actually switch your application to build against GTK 4.

## Preparation in GTK 3.x

The steps outlined in the following sections assume that your
application is working with GTK 3.24, which is the final stable
release of GTK 3.x. It includes all the necessary APIs and tools
to help you port your application to GTK 4. If you are using
an older version of GTK 3.x, you should first get your application
to build and work with the latest minor release in the 3.24 series.

### Do not use deprecated symbols

Over the years, a number of functions, and in some cases, entire
widgets have been deprecated. These deprecations are clearly spelled
out in the API reference, with hints about the recommended replacements.
The API reference for GTK 3 also includes an
[index](https://developer.gnome.org/gtk3/3.24/api-index-deprecated.html)
of all deprecated symbols.

To verify that your program does not use any deprecated symbols,
you can use defines to remove deprecated symbols from the header files,
as follows:
```
make CFLAGS+="-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
```

Note that some parts of our API, such as enumeration values, are
not well covered by the deprecation warnings. In most cases, using
them will require you to also use deprecated functions, which will
trigger warnings.

### Enable diagnostic warnings

Deprecations of properties and signals cannot be caught at compile
time, as both properties and signals are installed and used after
types have been instantiated. In order to catch deprecations and
changes in the run time components, you should use the
`G_ENABLE_DIAGNOSTIC` environment variable when running your
application, e.g.:
```
G_ENABLE_DIAGNOSTIC=1 ./your-app
```

### Do not use widget style properties

Style properties do not exist in GTK 4. You should stop using them in
your custom CSS and in your code.

### Review your window creation flags

GTK 4 removes the `GDK_WA_CURSOR` flag. Instead, just use
gdk_window_set_cursor() to set a cursor on the window after
creating it. GTK 4 also removes the `GDK_WA_VISUAL` flag, and
always uses an RGBA visual for windows. To prepare your code for
this, use `gdk_window_set_visual (gdk_screen_get_rgba_visual ())`
after creating your window. GTK 4 also removes the `GDK_WA_WMCLASS`
flag. If you need this X11-specific functionality, use XSetClassHint()
directly.

### Stop using direct access to GdkEvent structs

In GTK 4, event structs are opaque and immutable. Many fields already
have accessors in GTK 3, and you should use those to reduce the amount
of porting work you have to do at the time of the switch.

### Stop using gdk_pointer_warp()

Warping the pointer is disorienting and unfriendly to users.
GTK 4 does not support it. In special circumstances (such as when
implementing remote connection UIs) it can be necessary to
warp the pointer; in this case, use platform APIs such as
XWarpPointer() directly.

### Stop using non-RGBA visuals

GTK 4 always uses RGBA visuals for its windows; you should make
sure that your code works with such visuals. At the same time,
you should stop using GdkVisual APIs, since this object not longer
exists in GTK 4. Most of its APIs are deprecated already and not
useful when dealing with RGBA visuals.

### Stop using GtkBox padding, fill and expand child properties

GTK 4 removes these #GtkBox child properties, so you should stop using
them. You can replace GtkBox:padding using the #GtkWidget:margin properties
on your #GtkBox child widgets.

The fill child property can be replaced by setting appropriate values
for the #GtkWidget:halign and #GtkWidget:valign properties of the child
widgets. If you previously set the fill child property to %TRUE, you can
achieve the same effect by setting the halign or valign properties to
%GTK_ALIGN_FILL, depending on the parent box -- halign for a horizontal
box, valign for a vertical one.

\#GtkBox also uses the expand child property. It can be replaced by setting
#GtkWidget:hexpand or #GtkWidget:vexpand on the child widgets. To match the
old behavior of the #GtkBox's expand child property, you need to set
#GtkWidget:hexpand on the child widgets of a horizontal #GtkBox and
#GtkWidget:vexpand on the child widgets of a vertical #GtkBox.

Note that there's a subtle but important difference between #GtkBox's
expand and fill child properties and the ones in #GtkWidget: setting
#GtkWidget:hexpand or #GtkWidget:vexpand to %TRUE will propagate up
the widget hierarchy, so a pixel-perfect port might require you to reset
the expansion flags to %FALSE in a parent widget higher up the hierarchy.

### Stop using the state argument of GtkStyleContext getters

The getters in the GtkStyleContext API, such as
gtk_style_context_get_property(), gtk_style_context_get(),
or gtk_style_context_get_color() only accept the context's current
state for their state argument. You should update all callers to pass
the current state.

### Stop using gdk_pixbuf_get_from_window() and gdk_cairo_set_source_surface()

These functions are not supported in GTK 4. Instead, either use
backend-specific APIs, or render your widgets using
#GtkWidgetClass.snapshot() (once you are using GTK 4).

Stop using GtkButton's image-related API

The functions and properties related to automatically add a GtkImage
to a GtkButton, and using a GtkSetting to control its visibility, are
not supported in GTK 4. Instead, you can just pack a GtkImage inside
a GtkButton, and control its visibility like you would for any other
widget. If you only want to add a named icon to a GtkButton, you can
use gtk_button_new_from_icon_name().

### Stop using GtkWidget event signals

Event controllers and #GtkGestures replace event signals in GTK 4.
They have been backported to GTK 3.x so you can prepare for this change.

### Set a proper application ID

In GTK 4 we want the application's #GApplication 'application-id'
(and therefore the D-Bus name), the desktop file basename and Wayland's
xdg-shell app_id to match. In order to achieve this with GTK 3.x call
g_set_prgname() with the same application ID you passed to #GtkApplication.
Rename your desktop files to match the application ID if needed.

The call to g_set_prgname() can be removed once you fully migrated to GTK 4.

You should be aware that changing the application ID makes your
application appear as a new, different app to application installers.
You should consult the appstream documentation for best practices
around renaming applications.

### Stop using gtk_main() and related APIs

GTK 4 removes the gtk_main_ family of APIs. The recommended replacement
is GtkApplication, but you can also iterate the GLib mainloop directly,
using GMainContext APIs. The replacement for gtk_events_pending() is
g_main_context_pending(), the replacement for gtk_main_iteration() is
g_main_context_iteration().

### Reduce the use of gtk_widget_destroy()

GTK 4 introduces a gtk_window_destroy() api. While that is not available
in GTK 3, you can prepare for the switch by using gtk_widget_destroy()
only on toplevel windows, and replace all other uses with
gtk_container_remove() or g_object_unref().

### Reduce the use of generic container APIs</title>

GTK 4 removes gtk_container_add() and gtk_container_remove(). While there
is not always a replacement for gtk_container_remove() in GTK 3, you can
replace many uses of gtk_container_add() with equivalent container-specific
APIs such as gtk_box_pack_start() or gtk_grid_attach(), and thereby reduce
the amount of work you have to do at the time of the switch.

## Changes that need to be done at the time of the switch

This section outlines porting tasks that you need to tackle when
you get to the point that you actually build your application against
GTK 4. Making it possible to prepare for these in GTK 3 would
have been either impossible or impractical.

### Stop using GdkScreen

The GdkScreen object has been removed in GTK 4. Most of its APIs already
had replacements in GTK 3 and were deprecated, a few remaining replacements
have been added to GdkDisplay.

### Stop using the root window

The root window is an X11-centric concept that is no longer exposed in the
backend-neutral GDK API. If you need to interact with the X11 root window,
you can use gdk_x11_display_get_xrootwindow() to get its XID.

### Stop using GdkVisual

This object is not useful with current GTK drawing APIs and has been removed
without replacement.

### Stop using GdkDeviceManager

The GdkDeviceManager object has been removed in GTK 4. Most of its APIs already
had replacements in GTK 3 and were deprecated in favor of GdkSeat.

### Adapt to GdkWindow API changes

GdkWindow has been renamed to GdkSurface.

In GTK 4, the two roles of a standalone toplevel window and of a popup
that is placed relative to a parent window have been separated out into
two interfaces, #GdkToplevel and #GdkPopup. Surfaces implementing these
interfaces are created with gdk_surface_new_toplevel() and
gdk_surface_new_popup(), respectively, and they are presented on screen
using gdk_toplevel_present() and gdk_popup_present(). The present()
functions take parameters in the form of an auxiliary layout struct,
#GdkPopupLayout or #GdkToplevelLayout. If your code is dealing directly
with surfaces, you may have to change it to call the API in these
interfaces, depending on whether the surface you are dealing with
is a toplevel or a popup.

As part of this reorganization, X11-only concepts such as sticky or
keep-below have been removed. If you need to use them on your X11 windows,
you will have to set the corresponding X11 properties (as specified in the
EWMH) yourself. Subsurfaces are only supported with the Wayland backend,
using gdk_wayland_surface_new_subsurface(). Native and foreign subwindows
are no longer supported. These concepts were complicating the code and
could not be supported across backends.

gdk_window_reparent() is no longer available.

A number of minor API cleanups have happened in GdkSurface
as well. For example, gdk_surface_input_shape_combine_region()
has been renamed to gdk_surface_set_input_region(), and
gdk_surface_begin_resize_drag() has been renamed to
gdk_toplevel_begin_resize().

### The "iconified" window state has been renamed to "minimized"

The %GDK_SURFACE_STATE_ICONIFIED value of the
#GdkSurfaceState enumeration is now %GDK_SURFACE_STATE_MINIMIZED.

The #GdkWindow functions <function>gdk_window_iconify()</function>
and <function>gdk_window_deiconify()</function> have been renamed to
gdk_toplevel_minimize() and gdk_toplevel_present(), respectively.

The behavior of the minimization and unminimization operations have
not been changed, and they still require support from the underlying
windowing system.

### Adapt to GdkEvent API changes

Direct access to GdkEvent structs is no longer possible in GTK 4.
GdkEvent is now a strictly read-only type, and you can no longer
change any of its fields, or construct new events. All event fields
have accessors that you will have to use.

Event compression is always enabled in GTK 4. If you need to see
the uncoalesced motion history, use gdk_motion_event_get_history()
on the latest motion event.

### Stop using grabs

GTK 4 no longer provides the gdk_device_grab() or gdk_seat_grab()
apis. If you need to dismiss a popup when the user clicks outside
(the most common use for grabs), you can use the GdkPopup
#GdkPopup:autohide property instead. GtkPopover also has a
#GtkPopover:autohide property for this. If you need to prevent
the user from interacting with a window while a dialog is open,
use the #GtkWindow:modal property of the dialog.

### Adapt to coordinate API changes

A number of coordinate APIs in GTK 3 had _double variants: 
gdk_device_get_position(), gdk_device_get_surface_at_position(),
gdk_surface_get_device_position(). These have been changed to use
doubles, and the _double variants have been removed. Update your
code accordingly.

Any APIs that deal with global (or root) coordinates have been
removed in GTK 4, since not all backends support them. You should
replace your use of such APIs with surface-relative equivalents.
Examples of this are gdk_surface_get_origin(), gdk_surface_move()
or gdk_event_get_root_coords().

### Adapt to GdkKeymap API changes

GdkKeymap no longer exists as an independent object.

If you need access to keymap state, it is now exposed as properties
on the #GdkDevice representing the keyboard: #GdkDevice:direction,
#GdkDevice:has-bidi-layouts, #GdkDevice:caps-lock-state,
#GdkDevice:num-lock-state, #GdkDevice:scroll-lock-state and
#GdkDevice:modifier-state. To obtain the keyboard device, you can use
`gdk_seat_get_keyboard (gdk_display_get_default_seat (display)`.

If you need access to translated keys for event handling, #GdkEvent
now includes all of the translated key state, including consumed
modifiers, group and shift level, so there should be no need to
manually call gdk_keymap_translate_keyboard_state() (which has
been removed).

If you need to do forward or backward mapping between key codes
and key values, use gdk_display_map_keycode() and gdk_display_map_keyval(),
which are the replacements for gdk_keymap_get_entries_for_keycode()
and gdk_keymap_get_entries_for_keyval().

### Adapt to changes in keyboard modifier handling

GTK 3 has the idea that use of modifiers may differ between different
platforms, and has a #GdkModifierIntent api to let platforms provide
hint about how modifiers are expected to be used. It also promoted
the use of <Primary> instead of <Control> to specify accelerators that
adapt to platform conventions.

In GTK 4, the meaning of modifiers has been fixed, and backends are
expected to map the platform conventions to the existing modifiers.
The expected use of modifiers in GTK 4 is:

GDK_CONTROL_MASK
 : Primary accelerators
 GDK_ALT_MASK
 :  Mnemonics
GDK_SHIFT_MASK
 : Extending selections
GDK_CONTROL_MASK
 : Modifying selections
GDK_CONTROL_MASK|GDK_ALT_MASK
 : Prevent text input

Consequently, #GdkModifierIntent and related APIs have been removed,
and <Control> is preferred over <Primary> in accelerators.

A related change is that GTK 4 no longer supports the use of archaic
X11 'real' modifiers with the names Mod1,..., Mod5, and %GDK_MOD1_MASK
has been renamed to %GDK_ALT_MASK.

### Stop using gtk_get_current_... APIs

The function gtk_get_current_event() and its variants have been
replaced by equivalent event controller APIs:
gtk_event_controller_get_current_event(), etc.

### Convert your ui files

A number of the changes outlined below affect .ui files. The
gtk4-builder-tool simplify command can perform many of the
necessary changes automatically, when called with the --3to4
option. You should always review the resulting changes.

### Adapt to event controller API changes

A few changes to the event controller and #GtkGesture APIs
did not make it back to GTK 3, and have to be taken into account
when moving to GTK 4. One is that the #GtkEventControllerMotion::enter
and #GtkEventControllerMotion::leave signals have gained new arguments.
Another is that #GtkGestureMultiPress has been renamed to #GtkGestureClick,
and has lost its area property. A #GtkEventControllerFocus has been
split off from #GtkEventcontrollerKey.

### Focus handling changes

The semantics of the #GtkWidget:can-focus property have changed.
In GTK 3, this property only meant that the widget itself would not
accept keyboard input, but its children still might (in the case of
containers). In GTK 4, if :can-focus is %FALSE, the focus cannot enter
the widget or any of its descendents, and the default value has changed
from %FALSE to %TRUE. In addition, there is a #GtkWidget:focusable
property, which controls whether an individual widget can receive
the input focus.

The feature to automatically keep the focus widget scrolled into view
with gtk_container_set_focus_vadjustment() has been removed together with
GtkContainer, and is provided by scrollable widgets instead. In the common
case that the scrollable is a #GtkViewport, use #GtkViewport:scroll-to-focus.

### Use the new apis for keyboard shortcuts

The APIs for keyboard shortcuts and accelerators have changed in GTK 4.

Instead of GtkAccelGroup, you now use a #GtkShortcutController with global
scope, and instead of GtkBindingSet, you now use gtk_widget_class_add_shortcut(),
gtk_widget_class_add_binding() and its variants. In both cases, you probably
want to add actions that can be triggered by your shortcuts.

There is no direct replacement for loading and saving accelerators with
GtkAccelMap. But since #GtkShortcutController implements #GListModel and
both #GtkShortcutTrigger and #GtkShortcutAction can be serialized to
strings, it is relatively easy to implement saving and loading yourself. 

### Stop using GtkEventBox

GtkEventBox is no longer needed and has been removed.
All widgets receive all events.

### Stop using GtkButtonBox

GtkButtonBox has been removed. Use a GtkBox instead.

### Adapt to GtkBox API changes

The GtkBox pack-start and -end methods have been replaced by gtk_box_prepend()
and gtk_box_append(). You can also reorder box children as necessary.

### Adapt to GtkHeaderBar and GtkActionBar API changes

The gtk_header_bar_set_show_close_button() function has been renamed to
the more accurate name gtk_header_bar_set_show_title_buttons(). The
corresponding getter and the property itself have also been renamed.

The gtk_header_bar_set_custom_title() function has been renamed to
the more accurate name gtk_header_bar_set_title_widget(). The
corresponding getter and the property itself have also been renamed.

The gtk_header_bar_set_title() function has been removed along with its
corresponding getter and the property. By default #GtkHeaderBar shows
the title of the window, so if you were setting the title of the header
bar, consider setting the window title instead. If you need to show a
title that's different from the window title, use the
#GtkHeaderBar:title-widget property to add a #GtkLabel as shown in the
example in #GtkHeaderBar documentation.

The gtk_header_bar_set_subtitle() function has been removed along with
its corresponding getter and the property. The old "subtitle" behavior
can be replicated by setting the #GtkHeaderBar:title-widget property to
a #GtkBox with two labels inside, with the title label matching the
example in #GtkHeaderBar documentation, and the subtitle label being
similar, but with "subtitle" style class instead of "title".

The gtk_header_bar_set_has_subtitle() function has been removed along
with its corresponding getter and the property. Its behavior can be
replicated by setting the #GtkHeaderBar:title-widget property to a
#GtkStack with #GtkStack:vhomogeneous property set to %TRUE and two
pages, each with a #GtkBox with title and subtitle as described above.

The ::pack-type child properties of GtkHeaderBar and GtkActionBar have
been removed. If you need to programmatically place children, use the
pack_start() and pack_end() APIs. In ui files, use the type attribute
on the child element.

gtk4-builder-tool can help with this conversion, with the --3to4 option
of the simplify command.

### Adapt to GtkStack, GtkAssistant and GtkNotebook API changes

The child properties of GtkStack, GtkAssistant and GtkNotebook have been
converted into child meta objects.
Instead of gtk_container_child_set (stack, child, …), you can now use
g_object_set (gtk_stack_get_page (stack, child), …). In .ui files, the
GtkStackPage objects must be created explicitly, and take the child widget
as property. GtkNotebook and GtkAssistant are similar.

gtk4-builder-tool can help with this conversion, with the --3to4 option
of the simplify command.

### Adapt to GtkBin removal

The abstract base class GtkBin for single-child containers has been
removed. The former subclasses are now derived directly from GtkWidget,
and have a "child" property for their child widget. To add a child, use
the setter for the "child" property (e.g. gtk_frame_set_child()) instead
of gtk_container_add(). Adding a child in a ui file with <child> still works.

The affected classes are:

- GtkAspectFrame
- GtkButton (and subclasses)
- GtkComboBox
- GtkFlowBoxChild
- GtkFrame
- GtkListBoxRow
- GtkOverlay
- GtkPopover
- GtkRevealer
- GtkScrolledWindow
- GtkSearchBar
- GtkViewport
- GtkWindow (and subclasses)

If you have custom widgets that were derived from GtkBin, you should
port them to derive from GtkWidget. Notable vfuncs that you will have
to implement include dispose() (to unparent your child), compute_expand()
(if you want your container to propagate expand flags) and
get_request_mode() (if you want your container to support height-for-width.

You may also want to implement the GtkBuildable interface, to support
adding children with <child> in ui files.

### Adapt to GtkContainer removal

The abstract base class GtkContainer for general containers has been
removed. The former subclasses are now derived directly from GtkWidget,
and have class-specific add() and remove() functions.
The most noticable change is the use of gtk_box_append() or gtk_box_prepend()
instead of gtk_container_add() for adding children to GtkBox, and the change
to use container-specific remove functions, such as gtk_stack_remove() instead
of gtk_container_remove(). Adding a child in a ui file with <child> still works.

The affected classes are:

- GtkActionBar
- GtkBox (and subclasses)
- GtkExpander
- GtkFixed
- GtkFlowBox
- GtkGrid
- GtkHeaderBar
- GtkIconView
- GtkInfoBar
- GtkListBox
- GtkNotebook
- GtkPaned
- GtkStack
- GtkTextView
- GtkTreeView

Without GtkContainer, there are no longer facilities for defining and
using child properties. If you have custom widgets using child properties,
they will have to be converted either to layout properties provided
by a layout manager (if they are layout-related), or handled in some
other way. One possibility is to use child meta objects, as seen with
GtkAssistantPage, GtkStackPage and the like.

### Stop using GtkContainer::border-width

GTK 4 has removed the #GtkContainer::border-width property (together
with the rest of GtkContainer). Use other means to influence the spacing
of your containers, such as the CSS margin and padding properties on child
widgets.

### Adapt to gtk_widget_destroy() removal

The function gtk_widget_destroy() has been removed. To explicitly destroy
a toplevel window, use gtk_window_destroy(). To destroy a widget that is
part of a hierarchy, remove it from its parent using a container-specific
remove api, such as gtk_box_remove() or gtk_stack_remove(). To destroy
a freestanding non-toplevel widget, use g_object_unref() to drop your
reference.

### Adapt to coordinate API changes

A number of APIs that are accepting or returning coordinates have
been changed from ints to doubles: gtk_widget_translate_coordinates(),
gtk_fixed_put(), gtk_fixed_move(). This change is mostly transparent,
except for cases where out parameters are involved: you need to
pass double* now, instead of int*.

### Adapt to GtkStyleContext API changes

The getters in the GtkStyleContext API, such as
gtk_style_context_get_property(), gtk_style_context_get(),
or gtk_style_context_get_color() have lost their state argument,
and always use the context's current state. Update all callers
to omit the state argument.

The most commonly used GtkStyleContext API, gtk_style_context_add_class(),
has been moved to GtkWidget as gtk_widget_add_css_class(), as have the
corresponding gtk_style_context_remove_class() and
gtk_style_context_has_class() APIs.

### Adapt to GtkCssProvider API changes

In GTK 4, the various #GtkCssProvider load functions have lost their
#GError argument. If you want to handle CSS loading errors, use the
#GtkCssProvider::parsing-error signal instead. gtk_css_provider_get_named()
has been replaced by gtk_css_provider_load_named().

### Stop using GtkShadowType and GtkRelief properties

The shadow-type properties in GtkScrolledWindow, GtkViewport,
and GtkFrame, as well as the relief properties in GtkButton
and its subclasses have been removed. GtkScrolledWindow, GtkButton
and GtkMenuButton have instead gained a boolean has-frame
property.

### Adapt to GtkWidget's size request changes

GTK 3 used five different virtual functions in GtkWidget to
implement size requisition, namely the gtk_widget_get_preferred_width()
family of functions. To simplify widget implementations, GTK 4 uses
only one virtual function, GtkWidgetClass::measure() that widgets
have to implement. gtk_widget_measure() replaces the various
gtk_widget_get_preferred_ functions for querying sizes.

### Adapt to GtkWidget's size allocation changes

The #GtkWidget.size_allocate() vfunc takes the baseline as an argument
now, so you no longer need to call gtk_widget_get_allocated_baseline()
to get it.

The ::size-allocate signal has been removed, since it is easy
to misuse. If you need to learn about sizing changes of custom
drawing widgets, use the #GtkDrawingArea::resize or #GtkGLArea::resize
signals.

### Switch to GtkWidget's children APIs

In GTK 4, any widget can have children (and GtkContainer is gone).
There is new API to navigate the widget tree, for use in widget
implementations: gtk_widget_get_first_child(), gtk_widget_get_last_child(),
gtk_widget_get_next_sibling(), gtk_widget_get_prev_sibling().

### Don't use -gtk-gradient in your CSS

GTK now supports standard CSS syntax for both linear and radial
gradients, just use those.

### Don't use -gtk-icon-effect in your CSS

GTK now supports a more versatile -gtk-icon-filter instead. Replace
-gtk-icon-effect: dim; with -gtk-icon-filter: opacity(0.5); and
-gtk-icon-effect: hilight; with -gtk-icon-filter: brightness(1.2);.

### Don't use -gtk-icon-theme in your CSS

GTK now uses the current icon theme, without a way to change this.

### Don't use -gtk-outline-...-radius in your CSS

These non-standard properties have been removed from GTK
CSS. Just use regular border radius.

### Adapt to drawing model changes

This area has seen the most radical changes in the transition from GTK 3
to GTK 4. Widgets no longer use a draw() function to render their contents
to a cairo surface. Instead, they have a snapshot() function that creates
one or more GskRenderNodes to represent their content. Third-party widgets
that use a draw() function or a #GtkWidget::draw signal handler for custom
drawing will need to be converted to use gtk_snapshot_append_cairo().

The auxiliary #GtkSnapshot object has APIs to help with creating render
nodes.

If you are using a #GtkDrawingArea for custom drawing, you need to switch
to using gtk_drawing_area_set_draw_func() to set a draw function instead
of connecting a handler to the #GtkWidget::draw signal.

### Stop using APIs to query GdkSurfaces

A number of APIs for querying special-purpose windows have been removed,
since these windows are no longer publically available:
gtk_tree_view_get_bin_window(), gtk_viewport_get_bin_window(),
gtk_viewport_get_view_window().

### Widgets are now visible by default

The default value of #GtkWidget:visible in GTK 4 is %TRUE, so you no
longer need to explicitly show all your widgets. On the flip side, you
need to hide widgets that are not meant to be visible from the start.
The only widgets that still need to be explicitly shown are toplevel
windows, dialogs and popovers.

A convenient way to remove unnecessary property assignments like this
from ui files it run the command `gtk4-builder-tool simplify --replace`
on them.

The function gtk_widget_show_all(), the #GtkWidget:no-show-all property
and its getter and setter have been removed in  GTK 4, so you should stop
using them.

### Adapt to changes in animated hiding and showing of widgets

Widgets that appear and disappear with an animation, such as GtkPopover,
GtkInfoBar, GtkRevealer no longer use gtk_widget_show() and gtk_widget_hide()
for this, but have gained dedicated APIs for this purpose that you should
use.

### Stop passing commandline arguments to gtk_init

The gtk_init() and gtk_init_check() functions no longer accept commandline
arguments. Just call them without arguments. Other initialization functions
that were purely related to commandline argument handling, such as
gtk_parse_args() and gtk_get_option_group(), are gone. The APIs to
initialize GDK separately are also gone, but it is very unlikely
that you are affected by that.

### GdkPixbuf is deemphasized

A number of #GdkPixbuf-based APIs have been removed. The available replacements
are either using #GIcon, or the newly introduced #GdkTexture or #GdkPaintable
classes instead. If you are dealing with pixbufs, you can use
gdk_texture_new_for_pixbuf() to convert them to texture objects where needed.

### GtkWidget event signals are removed

Event controllers and #GtkGestures have already been introduced in GTK 3 to handle
input for many cases. In GTK 4, the traditional widget signals for handling input,
such as #GtkWidget::motion-event or #GtkWidget::event have been removed.

### Invalidation handling has changed

Only gtk_widget_queue_draw() is left to mark a widget as needing redraw.
Variations like gtk_widget_queue_draw_rectangle() or gtk_widget_queue_draw_region()
are no longer available.

### Stop using GtkWidget::draw

The #GtkWidget::draw signal has been removed. Widgets need to implement the
#GtkWidgetClass.snapshot() function now. Connecting draw signal handlers is
no longer possible. If you want to keep using cairo for drawing, use
gtk_snaphot_append_cairo().

### Window content observation has changed

Observing widget contents and widget size is now done by using the
#GtkWidgetPaintable object instead of connecting to widget signals.

### Monitor handling has changed

Instead of a monitor number, #GdkMonitor is now used throughout. 
gdk_display_get_monitors() returns the list of monitors that can be queried
or observed for monitors to pass to APIs like gtk_window_fullscreen_on_monitor().

### Adapt to cursor API changes

Use the new gtk_widget_set_cursor() function to set cursors, instead of
setting the cursor on the underlying window directly. This is necessary
because most widgets don't have their own window anymore, turning any
such calls into global cursor changes.

For creating standard cursors, gdk_cursor_new_for_display() has been removed,
you have to use cursor names instead of GdkCursorType. For creating custom cursors,
use gdk_cursor_new_from_texture(). The ability to get cursor images has been removed.

### Adapt to icon size API changes

Instead of the existing extensible set of symbolic icon sizes, GTK now only
supports normal and large icons with the #GtkIconSize enumeration. The actual sizes
can be defined by themes via the CSS property -gtk-icon-size.

GtkImage setters like gtk_image_set_from_icon_name() no longer take a #GtkIconSize
argument. You can use the separate gtk_image_set_icon_size() setter if you need
to override the icon size.

The :stock-size property of GtkCellRendererPixbuf has been renamed to
#GtkCellRendererPixbuf:icon-size.

### Adapt to changes in the GtkAssistant API

The :has-padding property is gone, and GtkAssistant no longer adds padding
to pages. You can easily do that yourself.

### Adapt to changes in the API of GtkEntry, GtkSearchEntry and GtkSpinButton

The GtkEditable interface has been made more useful, and the core functionality of
GtkEntry has been broken out as a GtkText widget. GtkEntry, GtkSearchEntry,
GtkSpinButton and the new GtkPasswordEntry now use a GtkText widget internally
and implement GtkEditable. In particular, this means that it is no longer
possible to use GtkEntry API such as gtk_entry_grab_focus_without_selecting()
on a search entry.

Use GtkEditable API for editable functionality, and widget-specific APIs for
things that go beyond the common interface. For password entries, use
GtkPasswordEntry. As an example, gtk_spin_button_set_max_width_chars()
has been removed in favor of gtk_editable_set_max_width_chars().

### Adapt to changes in GtkOverlay API

The GtkOverlay :pass-through child property has been replaced by the
#GtkWidget:can-target property. Note that they have the opposite sense:
pass-through == !can-target.

### Use GtkFixed instead of GtkLayout

Since GtkScrolledWindow can deal with widgets that do not implement
the GtkScrollable interface by automatically wrapping them into a
GtkViewport, GtkLayout is redundant, and has been removed in favor
of the existing GtkFixed container widget.

### Adapt to search entry changes

The way search entries are connected to global events has changed;
gtk_search_entry_handle_event() has been dropped and replaced by
gtk_search_entry_set_key_capture_widget() and
gtk_event_controller_key_forward().

### Stop using gtk_window_activate_default()

The handling of default widgets has been changed, and activating
the default now works by calling gtk_widget_activate_default()
on the widget that caused the activation. If you have a custom widget
that wants to override the default handling, you can provide an
implementation of the default.activate action in your widgets' action
groups.

### Stop setting ::has-default and ::has-focus in .ui files

The special handling for the ::has-default and ::has-focus properties
has been removed. If you want to define the initial focus or the
the default widget in a .ui file, set the ::default-widget or
::focus-widget properties of the toplevel window.

### Stop using the GtkWidget::display-changed signal

To track the current display, use the #GtkWidget::root property instead.

### GtkPopover::modal has been renamed to autohide

The modal property has been renamed to autohide.
gtk-builder-tool can assist with the rename in ui files.

### gtk_widget_get_surface has been removed

gtk_widget_get_surface() has been removed.
Use gtk_native_get_surface() in combination with
gtk_widget_get_native() instead.

### gtk_widget_is_toplevel has been removed

gtk_widget_is_toplevel() has been removed.
Use GTK_IS_ROOT, GTK_IS_NATIVE or GTK_IS_WINDOW
instead, as appropriate.

### gtk_widget_get_toplevel has been removed

gtk_widget_get_toplevel() has been removed.
Use gtk_widget_get_root() or gtk_widget_get_native()
instead, as appropriate.

### GtkEntryBuffer ::deleted-text has changed

To allow signal handlers to access the deleted text before it
has been deleted #GtkEntryBuffer::deleted-text has changed from
%G_SIGNAL_RUN_FIRST to %G_SIGNAL_RUN_LAST. The default handler
removes the text from the #GtkEntryBuffer.

To adapt existing code, use g_signal_connect_after() or
%G_CONNECT_AFTER when using g_signal_connect_data() or
g_signal_connect_object().

### GtkMenu, GtkMenuBar and GtkMenuItem are gone

These widgets were heavily relying on X11-centric concepts such as
override-redirect windows and grabs, and were hard to adjust to other
windowing systems.

Menus can already be replaced using GtkPopoverMenu in GTK 3. Additionally,
GTK 4 introduces GtkPopoverMenuBar to replace menubars. These new widgets
can only be constructed from menu models, so the porting effort involves
switching to menu models and actions.

Tabular menus were rarely used and complicated the menu code,
so they have not been brought over to #GtkPopoverMenu. If you need
complex layout in menu-like popups, consider directly using a
#GtkPopover instead.

Since menus are gone, GtkMenuButton also lost its ability to show menus,
and needs to be used with popovers in GTK 4.

### GtkToolbar has been removed

Toolbars were using outdated concepts such as requiring special toolitem
widgets. Toolbars should be replaced by using a GtkBox with regular widgets
instead and the "toolbar" style class.

### GtkAspectFrame is no longer a frame

GtkAspectFrame is no longer derived from GtkFrame and does not
place a label and frame around its child anymore. It still lets
you control the aspect ratio of its child.

### Stop using custom tooltip windows

Tooltips no longer use GtkWindows in GTK 4, and it is no longer
possible to provide a custom window for tooltips. Replacing the content
of the tooltip with a custom widget is still possible, with
gtk_tooltip_set_custom().

### Switch to the new Drag-and-Drop api

The source-side Drag-and-Drop apis in GTK 4 have been changed to use an event
controller, #GtkDragSource. Instead of calling gtk_drag_source_set()
and connecting to #GtkWidget signals, you create a #GtkDragSource object,
attach it to the widget with gtk_widget_add_controller(), and connect
to #GtkDragSource signals. Instead of calling gtk_drag_begin() on a widget
to start a drag manually, call gdk_drag_begin().
The ::drag-data-get signal has been replaced by the #GtkDragSource::prepare
signal, which returns a #GdkContentProvider for the drag operation.

The destination-side Drag-and-Drop  apis in GTK 4 have also been changed
to use an event controller, #GtkDropTarget. Instead of calling
gtk_drag_dest_set() and connecting to #GtkWidget signals, you create
a #GtkDropTarget object, attach it to the widget with
gtk_widget_add_controller(), and connect to #GtkDropTarget signals.
The ::drag-motion signal has been renamed to #GtkDropTarget::accept, and
instead of ::drag-data-received, you need to use async read methods on the
#GdkDrop object, such as gdk_drop_read_async() or gdk_drop_read_value_async().

### Adapt to GtkIconTheme API changes

gtk_icon_theme_lookup_icon() returns a #GtkIconPaintable object now, instead
of a #GtkIconInfo. It always returns a paintable in the requested size, and
never fails. A number of no-longer-relevant lookup flags and API variants
have been removed.

### Update to GtkFileChooser API changes

GtkFileChooser moved to a GFile-based API. If you need to convert a
path or a URI, use g_file_new_for_path(), g_file_new_for_commandline_arg(),
or g_file_new_for_uri(); similarly, if you need to get a path or a URI
from a GFile, use g_file_get_path(), or g_file_get_uri(). With the
removal or path and URI-based functions, the "local-only" property has
been removed; GFile can be used to access non-local as well as local
resources.

The GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER action has been removed. Use
%GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, instead. If a new folder is needed,
the user can create one.

The "confirm-overwrite" signal, and the "do-overwrite-confirmation"
property have been removed from GtkFileChooser. The file chooser widgets
will automatically handle the confirmation of overwriting a file when
using GTK_FILE_CHOOSER_ACTION_SAVE.

GtkFileChooser does not support a custom extra widget any more. If you
need to add extra widgets, use gtk_file_chooser_add_choice() instead.

GtkFileChooser does not support a custom preview widget any more. If
you need to show a custom preview, you can create your own GtkDialog
with a GtkFileChooserWidget and your own preview widget that you
update whenever the #GtkFileChooser::selection-changed signal is
emitted.

### Stop using blocking dialog functions

GtkDialog, GtkNativeDialog, and GtkPrintOperation removed their
blocking API using nested main loops. Nested main loops present
re-entrancy issues and other hard to debug issues when coupled
with other event sources (IPC, accessibility, network operations)
that are not under the toolkit or the application developer's
control. Additionally, "stop-the-world" functions do not fit
the event-driven programming model of GTK.

You can replace calls to <function>gtk_dialog_run()</function>
by specifying that the #GtkDialog must be modal using
gtk_window_set_modal() or the %GTK_DIALOG_MODAL flag, and
connecting to the #GtkDialog::response signal.
