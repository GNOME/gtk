/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <math.h>

#include <gobject/gvaluecollector.h>
#include <gobject/gobjectnotifyqueue.c>
#include <cairo-gobject.h>

#include "gtkcontainer.h"
#include "gtkaccelmapprivate.h"
#include "gtkaccelgroupprivate.h"
#include "gtkclipboard.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkselectionprivate.h"
#include "gtksettingsprivate.h"
#include "gtksizegroup-private.h"
#include "gtksizerequestcacheprivate.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkaccessible.h"
#include "gtktooltipprivate.h"
#include "gtkinvisible.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssprovider.h"
#include "gtkmodifierstyle.h"
#include "gtkversion.h"
#include "gtkdebug.h"
#include "gtkplug.h"
#include "gtktypebuiltins.h"
#include "a11y/gtkwidgetaccessible.h"
#include "gtkapplicationprivate.h"
#include "gtkgestureprivate.h"

/* for the use of round() */
#include "fallback-c89.c"

/**
 * SECTION:gtkwidget
 * @Short_description: Base class for all widgets
 * @Title: GtkWidget
 *
 * GtkWidget is the base class all widgets in GTK+ derive from. It manages the
 * widget lifecycle, states and style.
 *
 * # Height-for-width Geometry Management # {#geometry-management}
 *
 * GTK+ uses a height-for-width (and width-for-height) geometry management
 * system. Height-for-width means that a widget can change how much
 * vertical space it needs, depending on the amount of horizontal space
 * that it is given (and similar for width-for-height). The most common
 * example is a label that reflows to fill up the available width, wraps
 * to fewer lines, and therefore needs less height.
 *
 * Height-for-width geometry management is implemented in GTK+ by way
 * of five virtual methods:
 *
 * - #GtkWidgetClass.get_request_mode()
 * - #GtkWidgetClass.get_preferred_width()
 * - #GtkWidgetClass.get_preferred_height()
 * - #GtkWidgetClass.get_preferred_height_for_width()
 * - #GtkWidgetClass.get_preferred_width_for_height()
 * - #GtkWidgetClass.get_preferred_height_and_baseline_for_width()
 *
 * There are some important things to keep in mind when implementing
 * height-for-width and when using it in container implementations.
 *
 * The geometry management system will query a widget hierarchy in
 * only one orientation at a time. When widgets are initially queried
 * for their minimum sizes it is generally done in two initial passes
 * in the #GtkSizeRequestMode chosen by the toplevel.
 *
 * For example, when queried in the normal
 * %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH mode:
 * First, the default minimum and natural width for each widget
 * in the interface will be computed using gtk_widget_get_preferred_width().
 * Because the preferred widths for each container depend on the preferred
 * widths of their children, this information propagates up the hierarchy,
 * and finally a minimum and natural width is determined for the entire
 * toplevel. Next, the toplevel will use the minimum width to query for the
 * minimum height contextual to that width using
 * gtk_widget_get_preferred_height_for_width(), which will also be a highly
 * recursive operation. The minimum height for the minimum width is normally
 * used to set the minimum size constraint on the toplevel
 * (unless gtk_window_set_geometry_hints() is explicitly used instead).
 *
 * After the toplevel window has initially requested its size in both
 * dimensions it can go on to allocate itself a reasonable size (or a size
 * previously specified with gtk_window_set_default_size()). During the
 * recursive allocation process it’s important to note that request cycles
 * will be recursively executed while container widgets allocate their children.
 * Each container widget, once allocated a size, will go on to first share the
 * space in one orientation among its children and then request each child's
 * height for its target allocated width or its width for allocated height,
 * depending. In this way a #GtkWidget will typically be requested its size
 * a number of times before actually being allocated a size. The size a
 * widget is finally allocated can of course differ from the size it has
 * requested. For this reason, #GtkWidget caches a  small number of results
 * to avoid re-querying for the same sizes in one allocation cycle.
 *
 * See
 * [GtkContainer’s geometry management section][container-geometry-management]
 * to learn more about how height-for-width allocations are performed
 * by container widgets.
 *
 * If a widget does move content around to intelligently use up the
 * allocated size then it must support the request in both
 * #GtkSizeRequestModes even if the widget in question only
 * trades sizes in a single orientation.
 *
 * For instance, a #GtkLabel that does height-for-width word wrapping
 * will not expect to have #GtkWidgetClass.get_preferred_height() called
 * because that call is specific to a width-for-height request. In this
 * case the label must return the height required for its own minimum
 * possible width. By following this rule any widget that handles
 * height-for-width or width-for-height requests will always be allocated
 * at least enough space to fit its own content.
 *
 * Here are some examples of how a %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH widget
 * generally deals with width-for-height requests, for #GtkWidgetClass.get_preferred_height()
 * it will do:
 *
 * |[<!-- language="C" -->
 * static void
 * foo_widget_get_preferred_height (GtkWidget *widget,
 *                                  gint *min_height,
 *                                  gint *nat_height)
 * {
 *    if (i_am_in_height_for_width_mode)
 *      {
 *        gint min_width, nat_width;
 *
 *        GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget,
 *                                                            &min_width,
 *                                                            &nat_width);
 *        GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width
 *                                                           (widget,
 *                                                            min_width,
 *                                                            min_height,
 *                                                            nat_height);
 *      }
 *    else
 *      {
 *         ... some widgets do both. For instance, if a GtkLabel is
 *         rotated to 90 degrees it will return the minimum and
 *         natural height for the rotated label here.
 *      }
 * }
 * ]|
 *
 * And in #GtkWidgetClass.get_preferred_width_for_height() it will simply return
 * the minimum and natural width:
 * |[<!-- language="C" -->
 * static void
 * foo_widget_get_preferred_width_for_height (GtkWidget *widget,
 *                                            gint for_height,
 *                                            gint *min_width,
 *                                            gint *nat_width)
 * {
 *    if (i_am_in_height_for_width_mode)
 *      {
 *        GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget,
 *                                                            min_width,
 *                                                            nat_width);
 *      }
 *    else
 *      {
 *         ... again if a widget is sometimes operating in
 *         width-for-height mode (like a rotated GtkLabel) it can go
 *         ahead and do its real width for height calculation here.
 *      }
 * }
 * ]|
 *
 * Often a widget needs to get its own request during size request or
 * allocation. For example, when computing height it may need to also
 * compute width. Or when deciding how to use an allocation, the widget
 * may need to know its natural size. In these cases, the widget should
 * be careful to call its virtual methods directly, like this:
 *
 * |[<!-- language="C" -->
 * GTK_WIDGET_GET_CLASS(widget)->get_preferred_width (widget,
 *                                                    &min,
 *                                                    &natural);
 * ]|
 *
 * It will not work to use the wrapper functions, such as
 * gtk_widget_get_preferred_width() inside your own size request
 * implementation. These return a request adjusted by #GtkSizeGroup
 * and by the #GtkWidgetClass.adjust_size_request() virtual method. If a
 * widget used the wrappers inside its virtual method implementations,
 * then the adjustments (such as widget margins) would be applied
 * twice. GTK+ therefore does not allow this and will warn if you try
 * to do it.
 *
 * Of course if you are getting the size request for
 * another widget, such as a child of a
 * container, you must use the wrapper APIs.
 * Otherwise, you would not properly consider widget margins,
 * #GtkSizeGroup, and so forth.
 *
 * Since 3.10 Gtk+ also supports baseline vertical alignment of widgets. This
 * means that widgets are positioned such that the typographical baseline of
 * widgets in the same row are aligned. This happens if a widget supports baselines,
 * has a vertical alignment of %GTK_ALIGN_BASELINE, and is inside a container
 * that supports baselines and has a natural “row” that it aligns to the baseline,
 * or a baseline assigned to it by the grandparent.
 *
 * Baseline alignment support for a widget is done by the #GtkWidgetClass.get_preferred_height_and_baseline_for_width()
 * virtual function. It allows you to report a baseline in combination with the
 * minimum and natural height. If there is no baseline you can return -1 to indicate
 * this. The default implementation of this virtual function calls into the
 * #GtkWidgetClass.get_preferred_height() and #GtkWidgetClass.get_preferred_height_for_width(),
 * so if baselines are not supported it doesn’t need to be implemented.
 *
 * If a widget ends up baseline aligned it will be allocated all the space in the parent
 * as if it was %GTK_ALIGN_FILL, but the selected baseline can be found via gtk_widget_get_allocated_baseline().
 * If this has a value other than -1 you need to align the widget such that the baseline
 * appears at the position.
 *
 * # Style Properties
 *
 * #GtkWidget introduces “style
 * properties” - these are basically object properties that are stored
 * not on the object, but in the style object associated to the widget. Style
 * properties are set in [resource files][gtk3-Resource-Files].
 * This mechanism is used for configuring such things as the location of the
 * scrollbar arrows through the theme, giving theme authors more control over the
 * look of applications without the need to write a theme engine in C.
 *
 * Use gtk_widget_class_install_style_property() to install style properties for
 * a widget class, gtk_widget_class_find_style_property() or
 * gtk_widget_class_list_style_properties() to get information about existing
 * style properties and gtk_widget_style_get_property(), gtk_widget_style_get() or
 * gtk_widget_style_get_valist() to obtain the value of a style property.
 *
 * # GtkWidget as GtkBuildable
 *
 * The GtkWidget implementation of the GtkBuildable interface supports a
 * custom <accelerator> element, which has attributes named ”key”, ”modifiers”
 * and ”signal” and allows to specify accelerators.
 *
 * An example of a UI definition fragment specifying an accelerator:
 * |[
 * <object class="GtkButton">
 *   <accelerator key="q" modifiers="GDK_CONTROL_MASK" signal="clicked"/>
 * </object>
 * ]|
 *
 * In addition to accelerators, GtkWidget also support a custom <accessible>
 * element, which supports actions and relations. Properties on the accessible
 * implementation of an object can be set by accessing the internal child
 * “accessible” of a #GtkWidget.
 *
 * An example of a UI definition fragment specifying an accessible:
 * |[
 * <object class="GtkButton" id="label1"/>
 *   <property name="label">I am a Label for a Button</property>
 * </object>
 * <object class="GtkButton" id="button1">
 *   <accessibility>
 *     <action action_name="click" translatable="yes">Click the button.</action>
 *     <relation target="label1" type="labelled-by"/>
 *   </accessibility>
 *   <child internal-child="accessible">
 *     <object class="AtkObject" id="a11y-button1">
 *       <property name="accessible-name">Clickable Button</property>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * Finally, GtkWidget allows style information such as style classes to
 * be associated with widgets, using the custom <style> element:
 * |[
 * <object class="GtkButton" id="button1">
 *   <style>
 *     <class name="my-special-button-class"/>
 *     <class name="dark-button"/>
 *   </style>
 * </object>
 * ]|
 *
 * # Building composite widgets from template XML ## {#composite-templates}
 *
 * GtkWidget exposes some facilities to automate the proceedure
 * of creating composite widgets using #GtkBuilder interface description
 * language.
 *
 * To create composite widgets with #GtkBuilder XML, one must associate
 * the interface description with the widget class at class initialization
 * time using gtk_widget_class_set_template().
 *
 * The interface description semantics expected in composite template descriptions
 * is slightly different from regulare #GtkBuilder XML.
 *
 * Unlike regular interface descriptions, gtk_widget_class_set_template() will
 * expect a <template> tag as a direct child of the toplevel <interface>
 * tag. The <template> tag must specify the “class” attribute which must be
 * the type name of the widget. Optionally, the “parent” attribute may be
 * specified to specify the direct parent type of the widget type, this is
 * ignored by the GtkBuilder but required for Glade to introspect what kind
 * of properties and internal children exist for a given type when the actual
 * type does not exist.
 *
 * The XML which is contained inside the <template> tag behaves as if it were
 * added to the <object> tag defining @widget itself. You may set properties
 * on @widget by inserting <property> tags into the <template> tag, and also
 * add <child> tags to add children and extend @widget in the normal way you
 * would with <object> tags.
 *
 * Additionally, <object> tags can also be added before and after the initial
 * <template> tag in the normal way, allowing one to define auxilary objects
 * which might be referenced by other widgets declared as children of the
 * <template> tag.
 *
 * An example of a GtkBuilder Template Definition:
 * |[
 * <interface>
 *   <template class="FooWidget" parent="GtkBox">
 *     <property name="orientation">GTK_ORIENTATION_HORIZONTAL</property>
 *     <property name="spacing">4</property>
 *     <child>
 *       <object class="GtkButton" id="hello_button">
 *         <property name="label">Hello World</property>
 *         <signal name="clicked" handler="hello_button_clicked" object="FooWidget" swapped="yes"/>
 *       </object>
 *     </child>
 *     <child>
 *       <object class="GtkButton" id="goodbye_button">
 *         <property name="label">Goodbye World</property>
 *       </object>
 *     </child>
 *   </template>
 * </interface>
 * ]|
 *
 * Typically, you'll place the template fragment into a file that is
 * bundled with your project, using #GResource. In order to load the
 * template, you need to call gtk_widget_class_set_template_from_resource()
 * from the class initialization of your #GtkWidget type:
 *
 * |[<!-- language="C" -->
 * static void
 * foo_widget_class_init (FooWidgetClass *klass)
 * {
 *   // ...
 *
 *   gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
 *                                                "/com/example/ui/foowidget.ui");
 * }
 * ]|
 *
 * You will also need to call gtk_widget_init_template() from the instance
 * initialization function:
 *
 * |[<!-- language="C" -->
 * static void
 * foo_widget_init (FooWidget *self)
 * {
 *   // ...
 *   gtk_widget_init_template (GTK_WIDGET (self));
 * }
 * ]|
 *
 * You can access widgets defined in the template using the
 * gtk_widget_get_template_child() function, but you will typically declare
 * a pointer in the instance private data structure of your type using the same
 * name as the widget in the template definition, and call
 * gtk_widget_class_bind_template_child_private() with that name, e.g.
 *
 * |[<!-- language="C" -->
 * typedef struct {
 *   GtkWidget *hello_button;
 *   GtkWidget *goodbye_button;
 * } FooWidgetPrivate;
 *
 * G_DEFINE_TYPE_WITH_PRIVATE (FooWidget, foo_widget, GTK_TYPE_BOX)
 *
 * static void
 * foo_widget_class_init (FooWidgetClass *klass)
 * {
 *   // ...
 *   gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
 *                                                "/com/example/ui/foowidget.ui");
 *   gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
 *                                                 FooWidget, hello_button);
 *   gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
 *                                                 FooWidget, goodbye_button);
 * }
 * ]|
 *
 * You can also use gtk_widget_class_bind_template_callback() to connect a signal
 * callback defined in the template with a function visible in the scope of the
 * class, e.g.
 *
 * |[<!-- language="C" -->
 * // the signal handler has the instance and user data swapped
 * // because of the swapped="yes" attribute in the template XML
 * static void
 * hello_button_clicked (FooWidget *self,
 *                       GtkButton *button)
 * {
 *   g_print ("Hello, world!\n");
 * }
 *
 * static void
 * foo_widget_class_init (FooWidgetClass *klass)
 * {
 *   // ...
 *   gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
 *                                                "/com/example/ui/foowidget.ui");
 *   gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), hello_button_clicked);
 * }
 * ]|
 */

#define GTK_STATE_FLAGS_DO_PROPAGATE (GTK_STATE_FLAG_INSENSITIVE|GTK_STATE_FLAG_BACKDROP)

#define WIDGET_CLASS(w)	 GTK_WIDGET_GET_CLASS (w)

#define GTK_STATE_FLAGS_BITS 12

typedef struct {
  gchar               *name;           /* Name of the template automatic child */
  gboolean             internal_child; /* Whether the automatic widget should be exported as an <internal-child> */
  gssize               offset;         /* Instance private data offset where to set the automatic child (or 0) */
} AutomaticChildClass;

typedef struct {
  gchar     *callback_name;
  GCallback  callback_symbol;
} CallbackSymbol;

typedef struct {
  GBytes               *data;
  GSList               *children;
  GSList               *callbacks;
  GtkBuilderConnectFunc connect_func;
  gpointer              connect_data;
  GDestroyNotify        destroy_notify;
} GtkWidgetTemplate;

typedef struct {
  GtkEventController *controller;
  guint evmask_notify_id;
  guint grab_notify_id;
  guint sequence_state_changed_id;
} EventControllerData;

struct _GtkWidgetPrivate
{
  /* The state of the widget. Needs to be able to hold all GtkStateFlags bits
   * (defined in "gtkenums.h").
   */
  guint state_flags : GTK_STATE_FLAGS_BITS;

  guint direction             : 2;

  guint in_destruction        : 1;
  guint toplevel              : 1;
  guint anchored              : 1;
  guint composite_child       : 1;
  guint no_window             : 1;
  guint realized              : 1;
  guint mapped                : 1;
  guint visible               : 1;
  guint sensitive             : 1;
  guint can_focus             : 1;
  guint has_focus             : 1;
  guint can_default           : 1;
  guint has_default           : 1;
  guint receives_default      : 1;
  guint has_grab              : 1;
  guint shadowed              : 1;
  guint style_update_pending  : 1;
  guint app_paintable         : 1;
  guint double_buffered       : 1;
  guint redraw_on_alloc       : 1;
  guint no_show_all           : 1;
  guint child_visible         : 1;
  guint multidevice           : 1;
  guint has_shape_mask        : 1;
  guint in_reparent           : 1;

  /* Queue-resize related flags */
  guint alloc_needed          : 1;

  /* Expand-related flags */
  guint need_compute_expand   : 1; /* Need to recompute computed_[hv]_expand */
  guint computed_hexpand      : 1; /* computed results (composite of child flags) */
  guint computed_vexpand      : 1;
  guint hexpand               : 1; /* application-forced expand */
  guint vexpand               : 1;
  guint hexpand_set           : 1; /* whether to use application-forced  */
  guint vexpand_set           : 1; /* instead of computing from children */

  /* SizeGroup related flags */
  guint have_size_groups      : 1;

  guint8 alpha;
  guint8 user_alpha;

  /* The widget's name. If the widget does not have a name
   * (the name is NULL), then its name (as returned by
   * "gtk_widget_get_name") is its class's name.
   * Among other things, the widget name is used to determine
   * the style to use for a widget.
   */
  gchar *name;

  /* The list of attached windows to this widget.
   * We keep a list in order to call reset_style to all of them,
   * recursively. */
  GList *attached_windows; 

  /* The style for the widget. The style contains the
   * colors the widget should be drawn in for each state
   * along with graphics contexts used to draw with and
   * the font to use for text.
   */
  GtkStyle *style;
  GtkStyleContext *context;

  /* Widget's path for styling */
  GtkWidgetPath *path;

  /* The widget's allocated size */
  GtkAllocation allocation;
  gint allocated_baseline;
  GtkAllocation clip;

  /* The widget's requested sizes */
  SizeRequestCache requests;

  /* actions attached to this or any parent widget */
  GtkActionMuxer *muxer;

  /* The widget's window or its parent window if it does
   * not have a window. (Which will be indicated by the
   * no_window field being set).
   */
  GdkWindow *window;
  GList *registered_windows;

  /* The widget's parent */
  GtkWidget *parent;

  /* Animations and other things to update on clock ticks */
  GList *tick_callbacks;
  guint clock_tick_id;

  /* A hash by GType key, containing hash tables by widget name
   */
  GHashTable *auto_children;

#ifdef G_ENABLE_DEBUG
  /* Number of gtk_widget_push_verify_invariants () */
  guint verifying_invariants_count;
#endif /* G_ENABLE_DEBUG */

  GList *event_controllers;
};

struct _GtkWidgetClassPrivate
{
  GtkWidgetTemplate *template;
  GType accessible_type;
  AtkRole accessible_role;
};

enum {
  DESTROY,
  SHOW,
  HIDE,
  MAP,
  UNMAP,
  REALIZE,
  UNREALIZE,
  SIZE_ALLOCATE,
  STATE_FLAGS_CHANGED,
  STATE_CHANGED,
  PARENT_SET,
  HIERARCHY_CHANGED,
  STYLE_SET,
  DIRECTION_CHANGED,
  GRAB_NOTIFY,
  CHILD_NOTIFY,
  DRAW,
  MNEMONIC_ACTIVATE,
  GRAB_FOCUS,
  FOCUS,
  MOVE_FOCUS,
  KEYNAV_FAILED,
  EVENT,
  EVENT_AFTER,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SCROLL_EVENT,
  MOTION_NOTIFY_EVENT,
  DELETE_EVENT,
  DESTROY_EVENT,
  KEY_PRESS_EVENT,
  KEY_RELEASE_EVENT,
  ENTER_NOTIFY_EVENT,
  LEAVE_NOTIFY_EVENT,
  CONFIGURE_EVENT,
  FOCUS_IN_EVENT,
  FOCUS_OUT_EVENT,
  MAP_EVENT,
  UNMAP_EVENT,
  PROPERTY_NOTIFY_EVENT,
  SELECTION_CLEAR_EVENT,
  SELECTION_REQUEST_EVENT,
  SELECTION_NOTIFY_EVENT,
  SELECTION_GET,
  SELECTION_RECEIVED,
  PROXIMITY_IN_EVENT,
  PROXIMITY_OUT_EVENT,
  VISIBILITY_NOTIFY_EVENT,
  WINDOW_STATE_EVENT,
  DAMAGE_EVENT,
  GRAB_BROKEN_EVENT,
  DRAG_BEGIN,
  DRAG_END,
  DRAG_DATA_DELETE,
  DRAG_LEAVE,
  DRAG_MOTION,
  DRAG_DROP,
  DRAG_DATA_GET,
  DRAG_DATA_RECEIVED,
  POPUP_MENU,
  SHOW_HELP,
  ACCEL_CLOSURES_CHANGED,
  SCREEN_CHANGED,
  CAN_ACTIVATE_ACCEL,
  COMPOSITED_CHANGED,
  QUERY_TOOLTIP,
  DRAG_FAILED,
  STYLE_UPDATED,
  TOUCH_EVENT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_PARENT,
  PROP_WIDTH_REQUEST,
  PROP_HEIGHT_REQUEST,
  PROP_VISIBLE,
  PROP_SENSITIVE,
  PROP_APP_PAINTABLE,
  PROP_CAN_FOCUS,
  PROP_HAS_FOCUS,
  PROP_IS_FOCUS,
  PROP_CAN_DEFAULT,
  PROP_HAS_DEFAULT,
  PROP_RECEIVES_DEFAULT,
  PROP_COMPOSITE_CHILD,
  PROP_STYLE,
  PROP_EVENTS,
  PROP_NO_SHOW_ALL,
  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_MARKUP,
  PROP_TOOLTIP_TEXT,
  PROP_WINDOW,
  PROP_OPACITY,
  PROP_DOUBLE_BUFFERED,
  PROP_HALIGN,
  PROP_VALIGN,
  PROP_MARGIN_LEFT,
  PROP_MARGIN_RIGHT,
  PROP_MARGIN_START,
  PROP_MARGIN_END,
  PROP_MARGIN_TOP,
  PROP_MARGIN_BOTTOM,
  PROP_MARGIN,
  PROP_HEXPAND,
  PROP_VEXPAND,
  PROP_HEXPAND_SET,
  PROP_VEXPAND_SET,
  PROP_EXPAND,
  PROP_SCALE_FACTOR
};

typedef	struct	_GtkStateData	 GtkStateData;

struct _GtkStateData
{
  guint         flags_to_set;
  guint         flags_to_unset;
};

/* --- prototypes --- */
static void	gtk_widget_base_class_init	(gpointer            g_class);
static void	gtk_widget_class_init		(GtkWidgetClass     *klass);
static void	gtk_widget_base_class_finalize	(GtkWidgetClass     *klass);
static void	gtk_widget_init			(GtkWidget          *widget);
static void	gtk_widget_set_property		 (GObject           *object,
						  guint              prop_id,
						  const GValue      *value,
						  GParamSpec        *pspec);
static void	gtk_widget_get_property		 (GObject           *object,
						  guint              prop_id,
						  GValue            *value,
						  GParamSpec        *pspec);
static void	gtk_widget_constructed           (GObject	    *object);
static void	gtk_widget_dispose		 (GObject	    *object);
static void	gtk_widget_real_destroy		 (GtkWidget	    *object);
static void	gtk_widget_finalize		 (GObject	    *object);
static void	gtk_widget_real_show		 (GtkWidget	    *widget);
static void	gtk_widget_real_hide		 (GtkWidget	    *widget);
static void	gtk_widget_real_map		 (GtkWidget	    *widget);
static void	gtk_widget_real_unmap		 (GtkWidget	    *widget);
static void	gtk_widget_real_realize		 (GtkWidget	    *widget);
static void	gtk_widget_real_unrealize	 (GtkWidget	    *widget);
static void	gtk_widget_real_size_allocate	 (GtkWidget	    *widget,
                                                  GtkAllocation	    *allocation);
static void	gtk_widget_real_style_set        (GtkWidget         *widget,
                                                  GtkStyle          *previous_style);
static void	gtk_widget_real_direction_changed(GtkWidget         *widget,
                                                  GtkTextDirection   previous_direction);

static void	gtk_widget_real_grab_focus	 (GtkWidget         *focus_widget);
static gboolean gtk_widget_real_query_tooltip    (GtkWidget         *widget,
						  gint               x,
						  gint               y,
						  gboolean           keyboard_tip,
						  GtkTooltip        *tooltip);
static void     gtk_widget_real_style_updated    (GtkWidget         *widget);
static gboolean gtk_widget_real_show_help        (GtkWidget         *widget,
                                                  GtkWidgetHelpType  help_type);
static gboolean _gtk_widget_run_controllers      (GtkWidget           *widget,
                                                  const GdkEvent      *event,
                                                  GtkPropagationPhase  phase);

static void	gtk_widget_dispatch_child_properties_changed	(GtkWidget        *object,
								 guint             n_pspecs,
								 GParamSpec      **pspecs);
static gboolean         gtk_widget_real_button_event            (GtkWidget        *widget,
                                                                 GdkEventButton   *event);
static gboolean         gtk_widget_real_motion_event            (GtkWidget        *widget,
                                                                 GdkEventMotion   *event);
static gboolean		gtk_widget_real_key_press_event   	(GtkWidget        *widget,
								 GdkEventKey      *event);
static gboolean		gtk_widget_real_key_release_event 	(GtkWidget        *widget,
								 GdkEventKey      *event);
static gboolean		gtk_widget_real_focus_in_event   	 (GtkWidget       *widget,
								  GdkEventFocus   *event);
static gboolean		gtk_widget_real_focus_out_event   	(GtkWidget        *widget,
								 GdkEventFocus    *event);
static gboolean         gtk_widget_real_touch_event             (GtkWidget        *widget,
                                                                 GdkEventTouch    *event);
static gboolean         gtk_widget_real_grab_broken_event       (GtkWidget          *widget,
                                                                 GdkEventGrabBroken *event);
static gboolean		gtk_widget_real_focus			(GtkWidget        *widget,
								 GtkDirectionType  direction);
static void             gtk_widget_real_move_focus              (GtkWidget        *widget,
                                                                 GtkDirectionType  direction);
static gboolean		gtk_widget_real_keynav_failed		(GtkWidget        *widget,
								 GtkDirectionType  direction);
#ifdef G_ENABLE_DEBUG
static void             gtk_widget_verify_invariants            (GtkWidget        *widget);
static void             gtk_widget_push_verify_invariants       (GtkWidget        *widget);
static void             gtk_widget_pop_verify_invariants        (GtkWidget        *widget);
#else
#define                 gtk_widget_verify_invariants(widget)
#define                 gtk_widget_push_verify_invariants(widget)
#define                 gtk_widget_pop_verify_invariants(widget)
#endif
static PangoContext*	gtk_widget_peek_pango_context		(GtkWidget	  *widget);
static void     	gtk_widget_update_pango_context		(GtkWidget	  *widget);
static void		gtk_widget_propagate_state		(GtkWidget	  *widget,
								 GtkStateData 	  *data);
static void             gtk_widget_update_alpha                 (GtkWidget        *widget);

static gint		gtk_widget_event_internal		(GtkWidget	  *widget,
								 GdkEvent	  *event);
static gboolean		gtk_widget_real_mnemonic_activate	(GtkWidget	  *widget,
								 gboolean	   group_cycling);
static void             gtk_widget_real_get_width               (GtkWidget        *widget,
                                                                 gint             *minimum_size,
                                                                 gint             *natural_size);
static void             gtk_widget_real_get_height              (GtkWidget        *widget,
                                                                 gint             *minimum_size,
                                                                 gint             *natural_size);
static void             gtk_widget_real_get_height_for_width    (GtkWidget        *widget,
                                                                 gint              width,
                                                                 gint             *minimum_height,
                                                                 gint             *natural_height);
static void             gtk_widget_real_get_width_for_height    (GtkWidget        *widget,
                                                                 gint              height,
                                                                 gint             *minimum_width,
                                                                 gint             *natural_width);
static void             gtk_widget_real_state_flags_changed     (GtkWidget        *widget,
                                                                 GtkStateFlags     old_state);
static void             gtk_widget_real_queue_draw_region       (GtkWidget         *widget,
								 const cairo_region_t *region);
static const GtkWidgetAuxInfo* _gtk_widget_get_aux_info_or_defaults (GtkWidget *widget);
static GtkWidgetAuxInfo* gtk_widget_get_aux_info                (GtkWidget        *widget,
                                                                 gboolean          create);
static void		gtk_widget_aux_info_destroy		(GtkWidgetAuxInfo *aux_info);
static AtkObject*	gtk_widget_real_get_accessible		(GtkWidget	  *widget);
static void		gtk_widget_accessible_interface_init	(AtkImplementorIface *iface);
static AtkObject*	gtk_widget_ref_accessible		(AtkImplementor *implementor);
static void             gtk_widget_invalidate_widget_windows    (GtkWidget        *widget,
								 cairo_region_t        *region);
static GdkScreen *      gtk_widget_get_screen_unchecked         (GtkWidget        *widget);
static gboolean         gtk_widget_real_can_activate_accel      (GtkWidget *widget,
                                                                 guint      signal_id);

static void             gtk_widget_real_set_has_tooltip         (GtkWidget *widget,
								 gboolean   has_tooltip,
								 gboolean   force);
static void             gtk_widget_buildable_interface_init     (GtkBuildableIface *iface);
static void             gtk_widget_buildable_set_name           (GtkBuildable     *buildable,
                                                                 const gchar      *name);
static const gchar *    gtk_widget_buildable_get_name           (GtkBuildable     *buildable);
static GObject *        gtk_widget_buildable_get_internal_child (GtkBuildable *buildable,
								 GtkBuilder   *builder,
								 const gchar  *childname);
static void             gtk_widget_buildable_set_buildable_property (GtkBuildable     *buildable,
								     GtkBuilder       *builder,
								     const gchar      *name,
								     const GValue     *value);
static gboolean         gtk_widget_buildable_custom_tag_start   (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 GMarkupParser    *parser,
                                                                 gpointer         *data);
static void             gtk_widget_buildable_custom_finished    (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 gpointer          data);
static void             gtk_widget_buildable_parser_finished    (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder);

static GtkSizeRequestMode gtk_widget_real_get_request_mode      (GtkWidget         *widget);
static void             gtk_widget_real_get_width               (GtkWidget         *widget,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size);
static void             gtk_widget_real_get_height              (GtkWidget         *widget,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size);

static void             gtk_widget_queue_tooltip_query          (GtkWidget *widget);


static void             gtk_widget_real_adjust_size_request     (GtkWidget         *widget,
                                                                 GtkOrientation     orientation,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size);
static void             gtk_widget_real_adjust_baseline_request (GtkWidget         *widget,
								 gint              *minimum_baseline,
								 gint              *natural_baseline);
static void             gtk_widget_real_adjust_size_allocation  (GtkWidget         *widget,
                                                                 GtkOrientation     orientation,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size,
                                                                 gint              *allocated_pos,
                                                                 gint              *allocated_size);
static void             gtk_widget_real_adjust_baseline_allocation (GtkWidget         *widget,
								    gint              *baseline);

/* --- functions dealing with template data structures --- */
static AutomaticChildClass  *template_child_class_new          (const gchar          *name,
                                                                gboolean              internal_child,
                                                                gssize                offset);
static void                  template_child_class_free         (AutomaticChildClass  *child_class);
static CallbackSymbol       *callback_symbol_new                (const gchar          *name,
								 GCallback             callback);
static void                  callback_symbol_free               (CallbackSymbol       *callback);
static void                  template_data_free                 (GtkWidgetTemplate    *template_data);
static GHashTable           *get_auto_child_hash                (GtkWidget            *widget,
								 GType                 type,
								 gboolean              create);
static gboolean              setup_template_child              (GtkWidgetTemplate    *template_data,
                                                                GType                 class_type,
                                                                AutomaticChildClass  *child_class,
                                                                GtkWidget            *widget,
                                                                GtkBuilder           *builder);

static void gtk_widget_set_usize_internal (GtkWidget          *widget,
					   gint                width,
					   gint                height,
					   GtkQueueResizeFlags flags);

static void gtk_widget_add_events_internal (GtkWidget *widget,
                                            GdkDevice *device,
                                            gint       events);
static void gtk_widget_set_device_enabled_internal (GtkWidget *widget,
                                                    GdkDevice *device,
                                                    gboolean   recurse,
                                                    gboolean   enabled);

static void gtk_widget_on_frame_clock_update (GdkFrameClock *frame_clock,
                                              GtkWidget     *widget);

static gboolean event_window_is_still_viewable (GdkEvent *event);
static void gtk_cairo_set_event_window (cairo_t        *cr,
					GdkWindow *window);
static void gtk_cairo_set_event (cairo_t        *cr,
				 GdkEventExpose *event);

static void gtk_widget_update_input_shape (GtkWidget *widget);

/* --- variables --- */
static gint             GtkWidget_private_offset = 0;
static gpointer         gtk_widget_parent_class = NULL;
static guint            widget_signals[LAST_SIGNAL] = { 0 };
static guint            composite_child_stack = 0;
static GtkTextDirection gtk_default_direction = GTK_TEXT_DIR_LTR;
static GParamSpecPool  *style_property_spec_pool = NULL;

static GQuark		quark_property_parser = 0;
static GQuark		quark_aux_info = 0;
static GQuark		quark_accel_path = 0;
static GQuark		quark_accel_closures = 0;
static GQuark		quark_event_mask = 0;
static GQuark           quark_device_event_mask = 0;
static GQuark		quark_parent_window = 0;
static GQuark		quark_shape_info = 0;
static GQuark		quark_input_shape_info = 0;
static GQuark		quark_pango_context = 0;
static GQuark		quark_accessible_object = 0;
static GQuark		quark_mnemonic_labels = 0;
static GQuark		quark_tooltip_markup = 0;
static GQuark		quark_has_tooltip = 0;
static GQuark		quark_tooltip_window = 0;
static GQuark		quark_visual = 0;
static GQuark           quark_modifier_style = 0;
static GQuark           quark_enabled_devices = 0;
static GQuark           quark_size_groups = 0;
GParamSpecPool         *_gtk_widget_child_property_pool = NULL;
GObjectNotifyContext   *_gtk_widget_child_property_notify_context = NULL;

/* --- functions --- */
GType
gtk_widget_get_type (void)
{
  static GType widget_type = 0;

  if (G_UNLIKELY (widget_type == 0))
    {
      const GTypeInfo widget_info =
      {
	sizeof (GtkWidgetClass),
	gtk_widget_base_class_init,
	(GBaseFinalizeFunc) gtk_widget_base_class_finalize,
	(GClassInitFunc) gtk_widget_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_init */
	sizeof (GtkWidget),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_widget_init,
	NULL,		/* value_table */
      };

      const GInterfaceInfo accessibility_info =
      {
	(GInterfaceInitFunc) gtk_widget_accessible_interface_init,
	(GInterfaceFinalizeFunc) NULL,
	NULL /* interface data */
      };

      const GInterfaceInfo buildable_info =
      {
	(GInterfaceInitFunc) gtk_widget_buildable_interface_init,
	(GInterfaceFinalizeFunc) NULL,
	NULL /* interface data */
      };

      widget_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, "GtkWidget",
                                            &widget_info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_class_private (widget_type, sizeof (GtkWidgetClassPrivate));

      GtkWidget_private_offset =
        g_type_add_instance_private (widget_type, sizeof (GtkWidgetPrivate));

      g_type_add_interface_static (widget_type, ATK_TYPE_IMPLEMENTOR,
                                   &accessibility_info) ;
      g_type_add_interface_static (widget_type, GTK_TYPE_BUILDABLE,
                                   &buildable_info) ;
    }

  return widget_type;
}

static inline gpointer
gtk_widget_get_instance_private (GtkWidget *self)
{
  return (G_STRUCT_MEMBER_P (self, GtkWidget_private_offset));
}

static void
gtk_widget_base_class_init (gpointer g_class)
{
  GtkWidgetClass *klass = g_class;

  klass->priv = G_TYPE_CLASS_GET_PRIVATE (g_class, GTK_TYPE_WIDGET, GtkWidgetClassPrivate);
  klass->priv->template = NULL;
}

static void
child_property_notify_dispatcher (GObject     *object,
				  guint        n_pspecs,
				  GParamSpec **pspecs)
{
  GTK_WIDGET_GET_CLASS (object)->dispatch_child_properties_changed (GTK_WIDGET (object), n_pspecs, pspecs);
}

/* We guard against the draw signal callbacks modifying the state of the
 * cairo context by surounding it with save/restore.
 * Maybe we should also cairo_new_path() just to be sure?
 */
static void
gtk_widget_draw_marshaller (GClosure     *closure,
                            GValue       *return_value,
                            guint         n_param_values,
                            const GValue *param_values,
                            gpointer      invocation_hint,
                            gpointer      marshal_data)
{
  cairo_t *cr = g_value_get_boxed (&param_values[1]);

  cairo_save (cr);

  _gtk_marshal_BOOLEAN__BOXED (closure,
                               return_value,
                               n_param_values,
                               param_values,
                               invocation_hint,
                               marshal_data);


  cairo_restore (cr);
}

static void
gtk_widget_draw_marshallerv (GClosure     *closure,
			     GValue       *return_value,
			     gpointer      instance,
			     va_list       args,
			     gpointer      marshal_data,
			     int           n_params,
			     GType        *param_types)
{
  cairo_t *cr;
  va_list args_copy;

  G_VA_COPY (args_copy, args);
  cr = va_arg (args_copy, gpointer);

  cairo_save (cr);

  _gtk_marshal_BOOLEAN__BOXEDv (closure,
				return_value,
				instance,
				args,
				marshal_data,
				n_params,
				param_types);


  cairo_restore (cr);

  va_end (args_copy);
}

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  static GObjectNotifyContext cpn_context = { 0, NULL, NULL };
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  g_type_class_adjust_private_offset (klass, &GtkWidget_private_offset);
  gtk_widget_parent_class = g_type_class_peek_parent (klass);

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");
  quark_aux_info = g_quark_from_static_string ("gtk-aux-info");
  quark_accel_path = g_quark_from_static_string ("gtk-accel-path");
  quark_accel_closures = g_quark_from_static_string ("gtk-accel-closures");
  quark_event_mask = g_quark_from_static_string ("gtk-event-mask");
  quark_device_event_mask = g_quark_from_static_string ("gtk-device-event-mask");
  quark_parent_window = g_quark_from_static_string ("gtk-parent-window");
  quark_shape_info = g_quark_from_static_string ("gtk-shape-info");
  quark_input_shape_info = g_quark_from_static_string ("gtk-input-shape-info");
  quark_pango_context = g_quark_from_static_string ("gtk-pango-context");
  quark_accessible_object = g_quark_from_static_string ("gtk-accessible-object");
  quark_mnemonic_labels = g_quark_from_static_string ("gtk-mnemonic-labels");
  quark_tooltip_markup = g_quark_from_static_string ("gtk-tooltip-markup");
  quark_has_tooltip = g_quark_from_static_string ("gtk-has-tooltip");
  quark_tooltip_window = g_quark_from_static_string ("gtk-tooltip-window");
  quark_visual = g_quark_from_static_string ("gtk-widget-visual");
  quark_modifier_style = g_quark_from_static_string ("gtk-widget-modifier-style");
  quark_enabled_devices = g_quark_from_static_string ("gtk-widget-enabled-devices");
  quark_size_groups = g_quark_from_static_string ("gtk-widget-size-groups");

  style_property_spec_pool = g_param_spec_pool_new (FALSE);
  _gtk_widget_child_property_pool = g_param_spec_pool_new (TRUE);
  cpn_context.quark_notify_queue = g_quark_from_static_string ("GtkWidget-child-property-notify-queue");
  cpn_context.dispatcher = child_property_notify_dispatcher;
  _gtk_widget_child_property_notify_context = &cpn_context;

  gobject_class->constructed = gtk_widget_constructed;
  gobject_class->dispose = gtk_widget_dispose;
  gobject_class->finalize = gtk_widget_finalize;
  gobject_class->set_property = gtk_widget_set_property;
  gobject_class->get_property = gtk_widget_get_property;

  klass->destroy = gtk_widget_real_destroy;

  klass->activate_signal = 0;
  klass->dispatch_child_properties_changed = gtk_widget_dispatch_child_properties_changed;
  klass->show = gtk_widget_real_show;
  klass->show_all = gtk_widget_show;
  klass->hide = gtk_widget_real_hide;
  klass->map = gtk_widget_real_map;
  klass->unmap = gtk_widget_real_unmap;
  klass->realize = gtk_widget_real_realize;
  klass->unrealize = gtk_widget_real_unrealize;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->get_request_mode = gtk_widget_real_get_request_mode;
  klass->get_preferred_width = gtk_widget_real_get_width;
  klass->get_preferred_height = gtk_widget_real_get_height;
  klass->get_preferred_width_for_height = gtk_widget_real_get_width_for_height;
  klass->get_preferred_height_for_width = gtk_widget_real_get_height_for_width;
  klass->get_preferred_height_and_baseline_for_width = NULL;
  klass->state_changed = NULL;
  klass->state_flags_changed = gtk_widget_real_state_flags_changed;
  klass->parent_set = NULL;
  klass->hierarchy_changed = NULL;
  klass->style_set = gtk_widget_real_style_set;
  klass->direction_changed = gtk_widget_real_direction_changed;
  klass->grab_notify = NULL;
  klass->child_notify = NULL;
  klass->draw = NULL;
  klass->mnemonic_activate = gtk_widget_real_mnemonic_activate;
  klass->grab_focus = gtk_widget_real_grab_focus;
  klass->focus = gtk_widget_real_focus;
  klass->move_focus = gtk_widget_real_move_focus;
  klass->keynav_failed = gtk_widget_real_keynav_failed;
  klass->event = NULL;
  klass->button_press_event = gtk_widget_real_button_event;
  klass->button_release_event = gtk_widget_real_button_event;
  klass->motion_notify_event = gtk_widget_real_motion_event;
  klass->touch_event = gtk_widget_real_touch_event;
  klass->delete_event = NULL;
  klass->destroy_event = NULL;
  klass->key_press_event = gtk_widget_real_key_press_event;
  klass->key_release_event = gtk_widget_real_key_release_event;
  klass->enter_notify_event = NULL;
  klass->leave_notify_event = NULL;
  klass->configure_event = NULL;
  klass->focus_in_event = gtk_widget_real_focus_in_event;
  klass->focus_out_event = gtk_widget_real_focus_out_event;
  klass->map_event = NULL;
  klass->unmap_event = NULL;
  klass->window_state_event = NULL;
  klass->property_notify_event = _gtk_selection_property_notify;
  klass->selection_clear_event = _gtk_selection_clear;
  klass->selection_request_event = _gtk_selection_request;
  klass->selection_notify_event = _gtk_selection_notify;
  klass->selection_received = NULL;
  klass->proximity_in_event = NULL;
  klass->proximity_out_event = NULL;
  klass->drag_begin = NULL;
  klass->drag_end = NULL;
  klass->drag_data_delete = NULL;
  klass->drag_leave = NULL;
  klass->drag_motion = NULL;
  klass->drag_drop = NULL;
  klass->drag_data_received = NULL;
  klass->screen_changed = NULL;
  klass->can_activate_accel = gtk_widget_real_can_activate_accel;
  klass->grab_broken_event = gtk_widget_real_grab_broken_event;
  klass->query_tooltip = gtk_widget_real_query_tooltip;
  klass->style_updated = gtk_widget_real_style_updated;

  klass->show_help = gtk_widget_real_show_help;

  /* Accessibility support */
  klass->priv->accessible_type = GTK_TYPE_ACCESSIBLE;
  klass->priv->accessible_role = ATK_ROLE_INVALID;
  klass->get_accessible = gtk_widget_real_get_accessible;

  klass->adjust_size_request = gtk_widget_real_adjust_size_request;
  klass->adjust_baseline_request = gtk_widget_real_adjust_baseline_request;
  klass->adjust_size_allocation = gtk_widget_real_adjust_size_allocation;
  klass->adjust_baseline_allocation = gtk_widget_real_adjust_baseline_allocation;
  klass->queue_draw_region = gtk_widget_real_queue_draw_region;

  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
 							P_("Widget name"),
							P_("The name of the widget"),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_PARENT,
				   g_param_spec_object ("parent",
							P_("Parent widget"),
							P_("The parent widget of this widget. Must be a Container widget"),
							GTK_TYPE_CONTAINER,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_WIDTH_REQUEST,
				   g_param_spec_int ("width-request",
 						     P_("Width request"),
 						     P_("Override for width request of the widget, or -1 if natural request should be used"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_HEIGHT_REQUEST,
				   g_param_spec_int ("height-request",
 						     P_("Height request"),
 						     P_("Override for height request of the widget, or -1 if natural request should be used"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
 							 P_("Visible"),
 							 P_("Whether the widget is visible"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_SENSITIVE,
				   g_param_spec_boolean ("sensitive",
 							 P_("Sensitive"),
 							 P_("Whether the widget responds to input"),
 							 TRUE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_APP_PAINTABLE,
				   g_param_spec_boolean ("app-paintable",
 							 P_("Application paintable"),
 							 P_("Whether the application will paint directly on the widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_CAN_FOCUS,
				   g_param_spec_boolean ("can-focus",
 							 P_("Can focus"),
 							 P_("Whether the widget can accept the input focus"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_HAS_FOCUS,
				   g_param_spec_boolean ("has-focus",
 							 P_("Has focus"),
 							 P_("Whether the widget has the input focus"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_IS_FOCUS,
				   g_param_spec_boolean ("is-focus",
 							 P_("Is focus"),
 							 P_("Whether the widget is the focus widget within the toplevel"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_CAN_DEFAULT,
				   g_param_spec_boolean ("can-default",
 							 P_("Can default"),
 							 P_("Whether the widget can be the default widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_HAS_DEFAULT,
				   g_param_spec_boolean ("has-default",
 							 P_("Has default"),
 							 P_("Whether the widget is the default widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_RECEIVES_DEFAULT,
				   g_param_spec_boolean ("receives-default",
 							 P_("Receives default"),
 							 P_("If TRUE, the widget will receive the default action when it is focused"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_COMPOSITE_CHILD,
				   g_param_spec_boolean ("composite-child",
 							 P_("Composite child"),
 							 P_("Whether the widget is part of a composite widget"),
 							 FALSE,
 							 GTK_PARAM_READABLE));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  g_object_class_install_property (gobject_class,
				   PROP_STYLE,
				   g_param_spec_object ("style",
 							P_("Style"),
 							P_("The style of the widget, which contains information about how it will look (colors etc)"),
 							GTK_TYPE_STYLE,
 							GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));

G_GNUC_END_IGNORE_DEPRECATIONS

  g_object_class_install_property (gobject_class,
				   PROP_EVENTS,
				   g_param_spec_flags ("events",
 						       P_("Events"),
 						       P_("The event mask that decides what kind of GdkEvents this widget gets"),
 						       GDK_TYPE_EVENT_MASK,
 						       GDK_STRUCTURE_MASK,
 						       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
				   PROP_NO_SHOW_ALL,
				   g_param_spec_boolean ("no-show-all",
 							 P_("No show all"),
 							 P_("Whether gtk_widget_show_all() should not affect this widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

/**
 * GtkWidget:has-tooltip:
 *
 * Enables or disables the emission of #GtkWidget::query-tooltip on @widget.
 * A value of %TRUE indicates that @widget can have a tooltip, in this case
 * the widget will be queried using #GtkWidget::query-tooltip to determine
 * whether it will provide a tooltip or not.
 *
 * Note that setting this property to %TRUE for the first time will change
 * the event masks of the GdkWindows of this widget to include leave-notify
 * and motion-notify events.  This cannot and will not be undone when the
 * property is set to %FALSE again.
 *
 * Since: 2.12
 */
  g_object_class_install_property (gobject_class,
				   PROP_HAS_TOOLTIP,
				   g_param_spec_boolean ("has-tooltip",
 							 P_("Has tooltip"),
 							 P_("Whether this widget has a tooltip"),
 							 FALSE,
 							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkWidget:tooltip-text:
   *
   * Sets the text of tooltip to be the given string.
   *
   * Also see gtk_tooltip_set_text().
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL: #GtkWidget:has-tooltip
   * will automatically be set to %TRUE and there will be taken care of
   * #GtkWidget::query-tooltip in the default signal handler.
   *
   * Note that if both #GtkWidget:tooltip-text and #GtkWidget:tooltip-markup
   * are set, the last one wins.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_TEXT,
                                   g_param_spec_string ("tooltip-text",
                                                        P_("Tooltip Text"),
                                                        P_("The contents of the tooltip for this widget"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkWidget:tooltip-markup:
   *
   * Sets the text of tooltip to be the given string, which is marked up
   * with the [Pango text markup language][PangoMarkupFormat].
   * Also see gtk_tooltip_set_markup().
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL: #GtkWidget:has-tooltip
   * will automatically be set to %TRUE and there will be taken care of
   * #GtkWidget::query-tooltip in the default signal handler.
   *
   * Note that if both #GtkWidget:tooltip-text and #GtkWidget:tooltip-markup
   * are set, the last one wins.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
				   PROP_TOOLTIP_MARKUP,
				   g_param_spec_string ("tooltip-markup",
 							P_("Tooltip markup"),
							P_("The contents of the tooltip for this widget"),
							NULL,
							GTK_PARAM_READWRITE));

  /**
   * GtkWidget:window:
   *
   * The widget's window if it is realized, %NULL otherwise.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
				   PROP_WINDOW,
				   g_param_spec_object ("window",
 							P_("Window"),
							P_("The widget's window if it is realized"),
							GDK_TYPE_WINDOW,
							GTK_PARAM_READABLE));

  /**
   * GtkWidget:double-buffered:
   *
   * Whether the widget is double buffered.
   *
   * Since: 2.18
   *
   * Deprecated: 3.14: Widgets should not use this property.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DOUBLE_BUFFERED,
                                   g_param_spec_boolean ("double-buffered",
                                                         P_("Double Buffered"),
                                                         P_("Whether the widget is double buffered"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED));

  /**
   * GtkWidget:halign:
   *
   * How to distribute horizontal space if widget gets extra space, see #GtkAlign
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HALIGN,
                                   g_param_spec_enum ("halign",
                                                      P_("Horizontal Alignment"),
                                                      P_("How to position in extra horizontal space"),
                                                      GTK_TYPE_ALIGN,
                                                      GTK_ALIGN_FILL,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:valign:
   *
   * How to distribute vertical space if widget gets extra space, see #GtkAlign
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VALIGN,
                                   g_param_spec_enum ("valign",
                                                      P_("Vertical Alignment"),
                                                      P_("How to position in extra vertical space"),
                                                      GTK_TYPE_ALIGN,
                                                      GTK_ALIGN_FILL,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:margin-left:
   *
   * Margin on left side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Deprecated: 3.12: Use #GtkWidget:margin-start instead.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_LEFT,
                                   g_param_spec_int ("margin-left",
                                                     P_("Margin on Left"),
                                                     P_("Pixels of extra space on the left side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED));

  /**
   * GtkWidget:margin-right:
   *
   * Margin on right side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Deprecated: 3.12: Use #GtkWidget:margin-end instead.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_RIGHT,
                                   g_param_spec_int ("margin-right",
                                                     P_("Margin on Right"),
                                                     P_("Pixels of extra space on the right side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED));

  /**
   * GtkWidget:margin-start:
   *
   * Margin on start of widget, horizontally. This property supports
   * left-to-right and right-to-left text directions.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_START,
                                   g_param_spec_int ("margin-start",
                                                     P_("Margin on Start"),
                                                     P_("Pixels of extra space on the start"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:margin-end:
   *
   * Margin on end of widget, horizontally. This property supports
   * left-to-right and right-to-left text directions.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_END,
                                   g_param_spec_int ("margin-end",
                                                     P_("Margin on End"),
                                                     P_("Pixels of extra space on the end"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:margin-top:
   *
   * Margin on top side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_TOP,
                                   g_param_spec_int ("margin-top",
                                                     P_("Margin on Top"),
                                                     P_("Pixels of extra space on the top side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:margin-bottom:
   *
   * Margin on bottom side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_BOTTOM,
                                   g_param_spec_int ("margin-bottom",
                                                     P_("Margin on Bottom"),
                                                     P_("Pixels of extra space on the bottom side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:margin:
   *
   * Sets all four sides' margin at once. If read, returns max
   * margin on any side.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN,
                                   g_param_spec_int ("margin",
                                                     P_("All Margins"),
                                                     P_("Pixels of extra space on all four sides"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWidget::destroy:
   * @object: the object which received the signal
   *
   * Signals that all holders of a reference to the widget should release
   * the reference that they hold. May result in finalization of the widget
   * if all references are released.
   *
   * This signal is not suitable for saving widget state.
   */
  widget_signals[DESTROY] =
    g_signal_new (I_("destroy"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (GtkWidgetClass, destroy),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget:hexpand:
   *
   * Whether to expand horizontally. See gtk_widget_set_hexpand().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HEXPAND,
                                   g_param_spec_boolean ("hexpand",
                                                         P_("Horizontal Expand"),
                                                         P_("Whether widget wants more horizontal space"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:hexpand-set:
   *
   * Whether to use the #GtkWidget:hexpand property. See gtk_widget_get_hexpand_set().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HEXPAND_SET,
                                   g_param_spec_boolean ("hexpand-set",
                                                         P_("Horizontal Expand Set"),
                                                         P_("Whether to use the hexpand property"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:vexpand:
   *
   * Whether to expand vertically. See gtk_widget_set_vexpand().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VEXPAND,
                                   g_param_spec_boolean ("vexpand",
                                                         P_("Vertical Expand"),
                                                         P_("Whether widget wants more vertical space"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:vexpand-set:
   *
   * Whether to use the #GtkWidget:vexpand property. See gtk_widget_get_vexpand_set().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VEXPAND_SET,
                                   g_param_spec_boolean ("vexpand-set",
                                                         P_("Vertical Expand Set"),
                                                         P_("Whether to use the vexpand property"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:expand:
   *
   * Whether to expand in both directions. Setting this sets both #GtkWidget:hexpand and #GtkWidget:vexpand
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_EXPAND,
                                   g_param_spec_boolean ("expand",
                                                         P_("Expand Both"),
                                                         P_("Whether widget wants to expand in both directions"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget:opacity:
   *
   * The requested opacity of the widget. See gtk_widget_set_opacity() for
   * more details about window opacity.
   *
   * Before 3.8 this was only available in GtkWindow
   *
   * Since: 3.8
   */
  g_object_class_install_property (gobject_class,
				   PROP_OPACITY,
				   g_param_spec_double ("opacity",
							P_("Opacity for Widget"),
							P_("The opacity of the widget, from 0 to 1"),
							0.0,
							1.0,
							1.0,
							GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkWidget:scale-factor:
   *
   * The scale factor of the widget. See gtk_widget_get_scale_factor() for
   * more details about widget scaling.
   *
   * Since: 3.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SCALE_FACTOR,
                                   g_param_spec_int ("scale-factor",
                                                     P_("Scale factor"),
                                                     P_("The scaling factor of the window"),
                                                     1,
                                                     G_MAXINT,
                                                     1,
                                                     GTK_PARAM_READABLE));

  /**
   * GtkWidget::show:
   * @widget: the object which received the signal.
   *
   * The ::show signal is emitted when @widget is shown, for example with
   * gtk_widget_show().
   */
  widget_signals[SHOW] =
    g_signal_new (I_("show"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, show),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::hide:
   * @widget: the object which received the signal.
   *
   * The ::hide signal is emitted when @widget is hidden, for example with
   * gtk_widget_hide().
   */
  widget_signals[HIDE] =
    g_signal_new (I_("hide"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, hide),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::map:
   * @widget: the object which received the signal.
   *
   * The ::map signal is emitted when @widget is going to be mapped, that is
   * when the widget is visible (which is controlled with
   * gtk_widget_set_visible()) and all its parents up to the toplevel widget
   * are also visible. Once the map has occurred, #GtkWidget::map-event will
   * be emitted.
   *
   * The ::map signal can be used to determine whether a widget will be drawn,
   * for instance it can resume an animation that was stopped during the
   * emission of #GtkWidget::unmap.
   */
  widget_signals[MAP] =
    g_signal_new (I_("map"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, map),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unmap:
   * @widget: the object which received the signal.
   *
   * The ::unmap signal is emitted when @widget is going to be unmapped, which
   * means that either it or any of its parents up to the toplevel widget have
   * been set as hidden.
   *
   * As ::unmap indicates that a widget will not be shown any longer, it can be
   * used to, for example, stop an animation on the widget.
   */
  widget_signals[UNMAP] =
    g_signal_new (I_("unmap"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unmap),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::realize:
   * @widget: the object which received the signal.
   *
   * The ::realize signal is emitted when @widget is associated with a
   * #GdkWindow, which means that gtk_widget_realize() has been called or the
   * widget has been mapped (that is, it is going to be drawn).
   */
  widget_signals[REALIZE] =
    g_signal_new (I_("realize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, realize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unrealize:
   * @widget: the object which received the signal.
   *
   * The ::unrealize signal is emitted when the #GdkWindow associated with
   * @widget is destroyed, which means that gtk_widget_unrealize() has been
   * called or the widget has been unmapped (that is, it is going to be
   * hidden).
   */
  widget_signals[UNREALIZE] =
    g_signal_new (I_("unrealize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unrealize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::size-allocate:
   * @widget: the object which received the signal.
   * @allocation: (type Gtk.Allocation): the region which has been
   *   allocated to the widget.
   */
  widget_signals[SIZE_ALLOCATE] =
    g_signal_new (I_("size-allocate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, size_allocate),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::state-changed:
   * @widget: the object which received the signal.
   * @state: the previous state
   *
   * The ::state-changed signal is emitted when the widget state changes.
   * See gtk_widget_get_state().
   *
   * Deprecated: 3.0: Use #GtkWidget::state-flags-changed instead.
   */
  widget_signals[STATE_CHANGED] =
    g_signal_new (I_("state-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_DEPRECATED,
		  G_STRUCT_OFFSET (GtkWidgetClass, state_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_STATE_TYPE);

  /**
   * GtkWidget::state-flags-changed:
   * @widget: the object which received the signal.
   * @flags: The previous state flags.
   *
   * The ::state-flags-changed signal is emitted when the widget state
   * changes, see gtk_widget_get_state_flags().
   *
   * Since: 3.0
   */
  widget_signals[STATE_FLAGS_CHANGED] =
    g_signal_new (I_("state-flags-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, state_flags_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__FLAGS,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_STATE_FLAGS);

  /**
   * GtkWidget::parent-set:
   * @widget: the object on which the signal is emitted
   * @old_parent: (allow-none): the previous parent, or %NULL if the widget
   *   just got its initial parent.
   *
   * The ::parent-set signal is emitted when a new parent
   * has been set on a widget.
   */
  widget_signals[PARENT_SET] =
    g_signal_new (I_("parent-set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, parent_set),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  /**
   * GtkWidget::hierarchy-changed:
   * @widget: the object on which the signal is emitted
   * @previous_toplevel: (allow-none): the previous toplevel ancestor, or %NULL
   *   if the widget was previously unanchored
   *
   * The ::hierarchy-changed signal is emitted when the
   * anchored state of a widget changes. A widget is
   * “anchored” when its toplevel
   * ancestor is a #GtkWindow. This signal is emitted when
   * a widget changes from un-anchored to anchored or vice-versa.
   */
  widget_signals[HIERARCHY_CHANGED] =
    g_signal_new (I_("hierarchy-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, hierarchy_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  /**
   * GtkWidget::style-set:
   * @widget: the object on which the signal is emitted
   * @previous_style: (allow-none): the previous style, or %NULL if the widget
   *   just got its initial style
   *
   * The ::style-set signal is emitted when a new style has been set
   * on a widget. Note that style-modifying functions like
   * gtk_widget_modify_base() also cause this signal to be emitted.
   *
   * Note that this signal is emitted for changes to the deprecated
   * #GtkStyle. To track changes to the #GtkStyleContext associated
   * with a widget, use the #GtkWidget::style-updated signal.
   *
   * Deprecated:3.0: Use the #GtkWidget::style-updated signal
   */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  widget_signals[STYLE_SET] =
    g_signal_new (I_("style-set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_DEPRECATED,
		  G_STRUCT_OFFSET (GtkWidgetClass, style_set),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_STYLE);

G_GNUC_END_IGNORE_DEPRECATIONS

  /**
   * GtkWidget::style-updated:
   * @widget: the object on which the signal is emitted
   *
   * The ::style-updated signal is emitted when the #GtkStyleContext
   * of a widget is changed. Note that style-modifying functions like
   * gtk_widget_override_color() also cause this signal to be emitted.
   *
   * Since: 3.0
   */
  widget_signals[STYLE_UPDATED] =
    g_signal_new (I_("style-updated"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, style_updated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::direction-changed:
   * @widget: the object on which the signal is emitted
   * @previous_direction: the previous text direction of @widget
   *
   * The ::direction-changed signal is emitted when the text direction
   * of a widget changes.
   */
  widget_signals[DIRECTION_CHANGED] =
    g_signal_new (I_("direction-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, direction_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TEXT_DIRECTION);

  /**
   * GtkWidget::grab-notify:
   * @widget: the object which received the signal
   * @was_grabbed: %FALSE if the widget becomes shadowed, %TRUE
   *               if it becomes unshadowed
   *
   * The ::grab-notify signal is emitted when a widget becomes
   * shadowed by a GTK+ grab (not a pointer or keyboard grab) on
   * another widget, or when it becomes unshadowed due to a grab
   * being removed.
   *
   * A widget is shadowed by a gtk_grab_add() when the topmost
   * grab widget in the grab stack of its window group is not
   * its ancestor.
   */
  widget_signals[GRAB_NOTIFY] =
    g_signal_new (I_("grab-notify"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, grab_notify),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkWidget::child-notify:
   * @widget: the object which received the signal
   * @child_property: the #GParamSpec of the changed child property
   *
   * The ::child-notify signal is emitted for each
   * [child property][child-properties]  that has
   * changed on an object. The signal's detail holds the property name.
   */
  widget_signals[CHILD_NOTIFY] =
    g_signal_new (I_("child-notify"),
		   G_TYPE_FROM_CLASS (gobject_class),
		   G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS,
		   G_STRUCT_OFFSET (GtkWidgetClass, child_notify),
		   NULL, NULL,
		   g_cclosure_marshal_VOID__PARAM,
		   G_TYPE_NONE, 1,
		   G_TYPE_PARAM);

  /**
   * GtkWidget::draw:
   * @widget: the object which received the signal
   * @cr: the cairo context to draw to
   *
   * This signal is emitted when a widget is supposed to render itself.
   * The @widget's top left corner must be painted at the origin of
   * the passed in context and be sized to the values returned by
   * gtk_widget_get_allocated_width() and
   * gtk_widget_get_allocated_height().
   *
   * Signal handlers connected to this signal can modify the cairo
   * context passed as @cr in any way they like and don't need to
   * restore it. The signal emission takes care of calling cairo_save()
   * before and cairo_restore() after invoking the handler.
   *
   * The signal handler will get a @cr with a clip region already set to the
   * widget's dirty region, i.e. to the area that needs repainting.  Complicated
   * widgets that want to avoid redrawing themselves completely can get the full
   * extents of the clip region with gdk_cairo_get_clip_rectangle(), or they can
   * get a finer-grained representation of the dirty region with
   * cairo_copy_clip_rectangle_list().
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   % %FALSE to propagate the event further.
   *
   * Since: 3.0
   */
  widget_signals[DRAW] =
    g_signal_new (I_("draw"),
		   G_TYPE_FROM_CLASS (gobject_class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET (GtkWidgetClass, draw),
                   _gtk_boolean_handled_accumulator, NULL,
                   gtk_widget_draw_marshaller,
		   G_TYPE_BOOLEAN, 1,
		   CAIRO_GOBJECT_TYPE_CONTEXT);
  g_signal_set_va_marshaller (widget_signals[DRAW], G_TYPE_FROM_CLASS (klass),
                              gtk_widget_draw_marshallerv);

  /**
   * GtkWidget::mnemonic-activate:
   * @widget: the object which received the signal.
   * @arg1:
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   * %FALSE to propagate the event further.
   */
  widget_signals[MNEMONIC_ACTIVATE] =
    g_signal_new (I_("mnemonic-activate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, mnemonic_activate),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkWidget::grab-focus:
   * @widget: the object which received the signal.
   */
  widget_signals[GRAB_FOCUS] =
    g_signal_new (I_("grab-focus"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, grab_focus),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::focus:
   * @widget: the object which received the signal.
   * @direction:
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event. %FALSE to propagate the event further.
   */
  widget_signals[FOCUS] =
    g_signal_new (I_("focus"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__ENUM,
		  G_TYPE_BOOLEAN, 1,
		  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::move-focus:
   * @widget: the object which received the signal.
   * @direction:
   */
  widget_signals[MOVE_FOCUS] =
    g_signal_new (I_("move-focus"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWidgetClass, move_focus),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::keynav-failed:
   * @widget: the object which received the signal
   * @direction: the direction of movement
   *
   * Gets emitted if keyboard navigation fails.
   * See gtk_widget_keynav_failed() for details.
   *
   * Returns: %TRUE if stopping keyboard navigation is fine, %FALSE
   *          if the emitting widget should try to handle the keyboard
   *          navigation attempt in its parent container(s).
   *
   * Since: 2.12
   **/
  widget_signals[KEYNAV_FAILED] =
    g_signal_new (I_("keynav-failed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, keynav_failed),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::event:
   * @widget: the object which received the signal.
   * @event: the #GdkEvent which triggered this signal
   *
   * The GTK+ main loop will emit three signals for each GDK event delivered
   * to a widget: one generic ::event signal, another, more specific,
   * signal that matches the type of event delivered (e.g.
   * #GtkWidget::key-press-event) and finally a generic
   * #GtkWidget::event-after signal.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event
   * and to cancel the emission of the second specific ::event signal.
   *   %FALSE to propagate the event further and to allow the emission of
   *   the second signal. The ::event-after signal is emitted regardless of
   *   the return value.
   */
  widget_signals[EVENT] =
    g_signal_new (I_("event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::event-after:
   * @widget: the object which received the signal.
   * @event: the #GdkEvent which triggered this signal
   *
   * After the emission of the #GtkWidget::event signal and (optionally)
   * the second more specific signal, ::event-after will be emitted
   * regardless of the previous two signals handlers return values.
   *
   */
  widget_signals[EVENT_AFTER] =
    g_signal_new (I_("event-after"),
		  G_TYPE_FROM_CLASS (klass),
		  0,
		  0,
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[EVENT_AFTER], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXEDv);

  /**
   * GtkWidget::button-press-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventButton): the #GdkEventButton which triggered
   *   this signal.
   *
   * The ::button-press-event signal will be emitted when a button
   * (typically from a mouse) is pressed.
   *
   * To receive this signal, the #GdkWindow associated to the
   * widget needs to enable the #GDK_BUTTON_PRESS_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[BUTTON_PRESS_EVENT] =
    g_signal_new (I_("button-press-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, button_press_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[BUTTON_PRESS_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::button-release-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventButton): the #GdkEventButton which triggered
   *   this signal.
   *
   * The ::button-release-event signal will be emitted when a button
   * (typically from a mouse) is released.
   *
   * To receive this signal, the #GdkWindow associated to the
   * widget needs to enable the #GDK_BUTTON_RELEASE_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new (I_("button-release-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, button_release_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[BUTTON_RELEASE_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  widget_signals[TOUCH_EVENT] =
    g_signal_new (I_("touch-event"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, touch_event),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[TOUCH_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::scroll-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventScroll): the #GdkEventScroll which triggered
   *   this signal.
   *
   * The ::scroll-event signal is emitted when a button in the 4 to 7
   * range is pressed. Wheel mice are usually configured to generate
   * button press events for buttons 4 and 5 when the wheel is turned.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_SCROLL_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[SCROLL_EVENT] =
    g_signal_new (I_("scroll-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, scroll_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[SCROLL_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::motion-notify-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventMotion): the #GdkEventMotion which triggered
   *   this signal.
   *
   * The ::motion-notify-event signal is emitted when the pointer moves
   * over the widget's #GdkWindow.
   *
   * To receive this signal, the #GdkWindow associated to the widget
   * needs to enable the #GDK_POINTER_MOTION_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new (I_("motion-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, motion_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[MOTION_NOTIFY_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::composited-changed:
   * @widget: the object on which the signal is emitted
   *
   * The ::composited-changed signal is emitted when the composited
   * status of @widgets screen changes.
   * See gdk_screen_is_composited().
   */
  widget_signals[COMPOSITED_CHANGED] =
    g_signal_new (I_("composited-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, composited_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::delete-event:
   * @widget: the object which received the signal
   * @event: the event which triggered this signal
   *
   * The ::delete-event signal is emitted if a user requests that
   * a toplevel window is closed. The default handler for this signal
   * destroys the window. Connecting gtk_widget_hide_on_delete() to
   * this signal will cause the window to be hidden instead, so that
   * it can later be shown again without reconstructing it.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[DELETE_EVENT] =
    g_signal_new (I_("delete-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, delete_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[DELETE_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::destroy-event:
   * @widget: the object which received the signal.
   * @event: the event which triggered this signal
   *
   * The ::destroy-event signal is emitted when a #GdkWindow is destroyed.
   * You rarely get this signal, because most widgets disconnect themselves
   * from their window before they destroy it, so no widget owns the
   * window at destroy time.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[DESTROY_EVENT] =
    g_signal_new (I_("destroy-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, destroy_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[DESTROY_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::key-press-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventKey): the #GdkEventKey which triggered this signal.
   *
   * The ::key-press-event signal is emitted when a key is pressed. The signal
   * emission will reoccur at the key-repeat rate when the key is kept pressed.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_KEY_PRESS_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[KEY_PRESS_EVENT] =
    g_signal_new (I_("key-press-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, key_press_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[KEY_PRESS_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::key-release-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventKey): the #GdkEventKey which triggered this signal.
   *
   * The ::key-release-event signal is emitted when a key is released.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_KEY_RELEASE_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[KEY_RELEASE_EVENT] =
    g_signal_new (I_("key-release-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, key_release_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[KEY_RELEASE_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::enter-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventCrossing): the #GdkEventCrossing which triggered
   *   this signal.
   *
   * The ::enter-notify-event will be emitted when the pointer enters
   * the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_ENTER_NOTIFY_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[ENTER_NOTIFY_EVENT] =
    g_signal_new (I_("enter-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, enter_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[ENTER_NOTIFY_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::leave-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventCrossing): the #GdkEventCrossing which triggered
   *   this signal.
   *
   * The ::leave-notify-event will be emitted when the pointer leaves
   * the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_LEAVE_NOTIFY_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[LEAVE_NOTIFY_EVENT] =
    g_signal_new (I_("leave-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, leave_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[LEAVE_NOTIFY_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::configure-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventConfigure): the #GdkEventConfigure which triggered
   *   this signal.
   *
   * The ::configure-event signal will be emitted when the size, position or
   * stacking of the @widget's window has changed.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[CONFIGURE_EVENT] =
    g_signal_new (I_("configure-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, configure_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[CONFIGURE_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::focus-in-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventFocus): the #GdkEventFocus which triggered
   *   this signal.
   *
   * The ::focus-in-event signal will be emitted when the keyboard focus
   * enters the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_FOCUS_CHANGE_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[FOCUS_IN_EVENT] =
    g_signal_new (I_("focus-in-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus_in_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[FOCUS_IN_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::focus-out-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventFocus): the #GdkEventFocus which triggered this
   *   signal.
   *
   * The ::focus-out-event signal will be emitted when the keyboard focus
   * leaves the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_FOCUS_CHANGE_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[FOCUS_OUT_EVENT] =
    g_signal_new (I_("focus-out-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus_out_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[FOCUS_OUT_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::map-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventAny): the #GdkEventAny which triggered this signal.
   *
   * The ::map-event signal will be emitted when the @widget's window is
   * mapped. A window is mapped when it becomes visible on the screen.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[MAP_EVENT] =
    g_signal_new (I_("map-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, map_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[MAP_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::unmap-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventAny): the #GdkEventAny which triggered this signal
   *
   * The ::unmap-event signal will be emitted when the @widget's window is
   * unmapped. A window is unmapped when it becomes invisible on the screen.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[UNMAP_EVENT] =
    g_signal_new (I_("unmap-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unmap_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[UNMAP_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::property-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventProperty): the #GdkEventProperty which triggered
   *   this signal.
   *
   * The ::property-notify-event signal will be emitted when a property on
   * the @widget's window has been changed or deleted.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_PROPERTY_CHANGE_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[PROPERTY_NOTIFY_EVENT] =
    g_signal_new (I_("property-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, property_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[PROPERTY_NOTIFY_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::selection-clear-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventSelection): the #GdkEventSelection which triggered
   *   this signal.
   *
   * The ::selection-clear-event signal will be emitted when the
   * the @widget's window has lost ownership of a selection.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[SELECTION_CLEAR_EVENT] =
    g_signal_new (I_("selection-clear-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_clear_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[SELECTION_CLEAR_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::selection-request-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventSelection): the #GdkEventSelection which triggered
   *   this signal.
   *
   * The ::selection-request-event signal will be emitted when
   * another client requests ownership of the selection owned by
   * the @widget's window.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[SELECTION_REQUEST_EVENT] =
    g_signal_new (I_("selection-request-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_request_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[SELECTION_REQUEST_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::selection-notify-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventSelection):
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event. %FALSE to propagate the event further.
   */
  widget_signals[SELECTION_NOTIFY_EVENT] =
    g_signal_new (I_("selection-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[SELECTION_NOTIFY_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::selection-received:
   * @widget: the object which received the signal.
   * @data:
   * @time:
   */
  widget_signals[SELECTION_RECEIVED] =
    g_signal_new (I_("selection-received"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_UINT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT);

  /**
   * GtkWidget::selection-get:
   * @widget: the object which received the signal.
   * @data:
   * @info:
   * @time:
   */
  widget_signals[SELECTION_GET] =
    g_signal_new (I_("selection-get"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_get),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_UINT_UINT,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::proximity-in-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventProximity): the #GdkEventProximity which triggered
   *   this signal.
   *
   * To receive this signal the #GdkWindow associated to the widget needs
   * to enable the #GDK_PROXIMITY_IN_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[PROXIMITY_IN_EVENT] =
    g_signal_new (I_("proximity-in-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, proximity_in_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[PROXIMITY_IN_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::proximity-out-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventProximity): the #GdkEventProximity which triggered
   *   this signal.
   *
   * To receive this signal the #GdkWindow associated to the widget needs
   * to enable the #GDK_PROXIMITY_OUT_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[PROXIMITY_OUT_EVENT] =
    g_signal_new (I_("proximity-out-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, proximity_out_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[PROXIMITY_OUT_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::drag-leave:
   * @widget: the object which received the signal.
   * @context: the drag context
   * @time: the timestamp of the motion event
   *
   * The ::drag-leave signal is emitted on the drop site when the cursor
   * leaves the widget. A typical reason to connect to this signal is to
   * undo things done in #GtkWidget::drag-motion, e.g. undo highlighting
   * with gtk_drag_unhighlight().
   *
   *
   * Likewise, the #GtkWidget::drag-leave signal is also emitted before the 
   * ::drag-drop signal, for instance to allow cleaning up of a preview item  
   * created in the #GtkWidget::drag-motion signal handler.
   */
  widget_signals[DRAG_LEAVE] =
    g_signal_new (I_("drag-leave"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_leave),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_UINT,
		  G_TYPE_NONE, 2,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-begin:
   * @widget: the object which received the signal
   * @context: the drag context
   *
   * The ::drag-begin signal is emitted on the drag source when a drag is
   * started. A typical reason to connect to this signal is to set up a
   * custom drag icon with e.g. gtk_drag_source_set_icon_pixbuf().
   *
   * Note that some widgets set up a drag icon in the default handler of
   * this signal, so you may have to use g_signal_connect_after() to
   * override what the default handler did.
   */
  widget_signals[DRAG_BEGIN] =
    g_signal_new (I_("drag-begin"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_begin),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-end:
   * @widget: the object which received the signal
   * @context: the drag context
   *
   * The ::drag-end signal is emitted on the drag source when a drag is
   * finished.  A typical reason to connect to this signal is to undo
   * things done in #GtkWidget::drag-begin.
   */
  widget_signals[DRAG_END] =
    g_signal_new (I_("drag-end"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_end),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-data-delete:
   * @widget: the object which received the signal
   * @context: the drag context
   *
   * The ::drag-data-delete signal is emitted on the drag source when a drag
   * with the action %GDK_ACTION_MOVE is successfully completed. The signal
   * handler is responsible for deleting the data that has been dropped. What
   * "delete" means depends on the context of the drag operation.
   */
  widget_signals[DRAG_DATA_DELETE] =
    g_signal_new (I_("drag-data-delete"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_delete),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-failed:
   * @widget: the object which received the signal
   * @context: the drag context
   * @result: the result of the drag operation
   *
   * The ::drag-failed signal is emitted on the drag source when a drag has
   * failed. The signal handler may hook custom code to handle a failed DND
   * operation based on the type of error, it returns %TRUE is the failure has
   * been already handled (not showing the default "drag operation failed"
   * animation), otherwise it returns %FALSE.
   *
   * Returns: %TRUE if the failed drag operation has been already handled.
   *
   * Since: 2.12
   */
  widget_signals[DRAG_FAILED] =
    g_signal_new (I_("drag-failed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_failed),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_ENUM,
		  G_TYPE_BOOLEAN, 2,
		  GDK_TYPE_DRAG_CONTEXT,
		  GTK_TYPE_DRAG_RESULT);

  /**
   * GtkWidget::drag-motion:
   * @widget: the object which received the signal
   * @context: the drag context
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   * @time: the timestamp of the motion event
   *
   * The ::drag-motion signal is emitted on the drop site when the user
   * moves the cursor over the widget during a drag. The signal handler
   * must determine whether the cursor position is in a drop zone or not.
   * If it is not in a drop zone, it returns %FALSE and no further processing
   * is necessary. Otherwise, the handler returns %TRUE. In this case, the
   * handler is responsible for providing the necessary information for
   * displaying feedback to the user, by calling gdk_drag_status().
   *
   * If the decision whether the drop will be accepted or rejected can't be
   * made based solely on the cursor position and the type of the data, the
   * handler may inspect the dragged data by calling gtk_drag_get_data() and
   * defer the gdk_drag_status() call to the #GtkWidget::drag-data-received
   * handler. Note that you cannot not pass #GTK_DEST_DEFAULT_DROP,
   * #GTK_DEST_DEFAULT_MOTION or #GTK_DEST_DEFAULT_ALL to gtk_drag_dest_set()
   * when using the drag-motion signal that way.
   *
   * Also note that there is no drag-enter signal. The drag receiver has to
   * keep track of whether he has received any drag-motion signals since the
   * last #GtkWidget::drag-leave and if not, treat the drag-motion signal as
   * an "enter" signal. Upon an "enter", the handler will typically highlight
   * the drop site with gtk_drag_highlight().
   * |[<!-- language="C" -->
   * static void
   * drag_motion (GtkWidget      *widget,
   *              GdkDragContext *context,
   *              gint            x,
   *              gint            y,
   *              guint           time)
   * {
   *   GdkAtom target;
   *
   *   PrivateData *private_data = GET_PRIVATE_DATA (widget);
   *
   *   if (!private_data->drag_highlight)
   *    {
   *      private_data->drag_highlight = 1;
   *      gtk_drag_highlight (widget);
   *    }
   *
   *   target = gtk_drag_dest_find_target (widget, context, NULL);
   *   if (target == GDK_NONE)
   *     gdk_drag_status (context, 0, time);
   *   else
   *    {
   *      private_data->pending_status
   *         = gdk_drag_context_get_suggested_action (context);
   *      gtk_drag_get_data (widget, context, target, time);
   *    }
   *
   *   return TRUE;
   * }
   *
   * static void
   * drag_data_received (GtkWidget        *widget,
   *                     GdkDragContext   *context,
   *                     gint              x,
   *                     gint              y,
   *                     GtkSelectionData *selection_data,
   *                     guint             info,
   *                     guint             time)
   * {
   *   PrivateData *private_data = GET_PRIVATE_DATA (widget);
   *
   *   if (private_data->suggested_action)
   *    {
   *      private_data->suggested_action = 0;
   *
   *      // We are getting this data due to a request in drag_motion,
   *      // rather than due to a request in drag_drop, so we are just
   *      // supposed to call gdk_drag_status(), not actually paste in
   *      // the data.
   *
   *      str = gtk_selection_data_get_text (selection_data);
   *      if (!data_is_acceptable (str))
   *        gdk_drag_status (context, 0, time);
   *      else
   *        gdk_drag_status (context,
   *                         private_data->suggested_action,
   *                         time);
   *    }
   *   else
   *    {
   *      // accept the drop
   *    }
   * }
   * ]|
   *
   * Returns: whether the cursor position is in a drop zone
   */
  widget_signals[DRAG_MOTION] =
    g_signal_new (I_("drag-motion"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_motion),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT_UINT,
		  G_TYPE_BOOLEAN, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-drop:
   * @widget: the object which received the signal
   * @context: the drag context
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   * @time: the timestamp of the motion event
   *
   * The ::drag-drop signal is emitted on the drop site when the user drops
   * the data onto the widget. The signal handler must determine whether
   * the cursor position is in a drop zone or not. If it is not in a drop
   * zone, it returns %FALSE and no further processing is necessary.
   * Otherwise, the handler returns %TRUE. In this case, the handler must
   * ensure that gtk_drag_finish() is called to let the source know that
   * the drop is done. The call to gtk_drag_finish() can be done either
   * directly or in a #GtkWidget::drag-data-received handler which gets
   * triggered by calling gtk_drag_get_data() to receive the data for one
   * or more of the supported targets.
   *
   * Returns: whether the cursor position is in a drop zone
   */
  widget_signals[DRAG_DROP] =
    g_signal_new (I_("drag-drop"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_drop),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT_UINT,
		  G_TYPE_BOOLEAN, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-data-get:
   * @widget: the object which received the signal
   * @context: the drag context
   * @data: the #GtkSelectionData to be filled with the dragged data
   * @info: the info that has been registered with the target in the
   *        #GtkTargetList
   * @time: the timestamp at which the data was requested
   *
   * The ::drag-data-get signal is emitted on the drag source when the drop
   * site requests the data which is dragged. It is the responsibility of
   * the signal handler to fill @data with the data in the format which
   * is indicated by @info. See gtk_selection_data_set() and
   * gtk_selection_data_set_text().
   */
  widget_signals[DRAG_DATA_GET] =
    g_signal_new (I_("drag-data-get"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_get),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_BOXED_UINT_UINT,
		  G_TYPE_NONE, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-data-received:
   * @widget: the object which received the signal
   * @context: the drag context
   * @x: where the drop happened
   * @y: where the drop happened
   * @data: the received data
   * @info: the info that has been registered with the target in the
   *        #GtkTargetList
   * @time: the timestamp at which the data was received
   *
   * The ::drag-data-received signal is emitted on the drop site when the
   * dragged data has been received. If the data was received in order to
   * determine whether the drop will be accepted, the handler is expected
   * to call gdk_drag_status() and not finish the drag.
   * If the data was received in response to a #GtkWidget::drag-drop signal
   * (and this is the last target to be received), the handler for this
   * signal is expected to process the received data and then call
   * gtk_drag_finish(), setting the @success parameter depending on
   * whether the data was processed successfully.
   *
   * Applications must create some means to determine why the signal was emitted 
   * and therefore whether to call gdk_drag_status() or gtk_drag_finish(). 
   *
   * The handler may inspect the selected action with
   * gdk_drag_context_get_selected_action() before calling
   * gtk_drag_finish(), e.g. to implement %GDK_ACTION_ASK as
   * shown in the following example:
   * |[<!-- language="C" -->
   * void
   * drag_data_received (GtkWidget          *widget,
   *                     GdkDragContext     *context,
   *                     gint                x,
   *                     gint                y,
   *                     GtkSelectionData   *data,
   *                     guint               info,
   *                     guint               time)
   * {
   *   if ((data->length >= 0) && (data->format == 8))
   *     {
   *       GdkDragAction action;
   *
   *       // handle data here
   *
   *       action = gdk_drag_context_get_selected_action (context);
   *       if (action == GDK_ACTION_ASK)
   *         {
   *           GtkWidget *dialog;
   *           gint response;
   *
   *           dialog = gtk_message_dialog_new (NULL,
   *                                            GTK_DIALOG_MODAL |
   *                                            GTK_DIALOG_DESTROY_WITH_PARENT,
   *                                            GTK_MESSAGE_INFO,
   *                                            GTK_BUTTONS_YES_NO,
   *                                            "Move the data ?\n");
   *           response = gtk_dialog_run (GTK_DIALOG (dialog));
   *           gtk_widget_destroy (dialog);
   *
   *           if (response == GTK_RESPONSE_YES)
   *             action = GDK_ACTION_MOVE;
   *           else
   *             action = GDK_ACTION_COPY;
   *          }
   *
   *       gtk_drag_finish (context, TRUE, action == GDK_ACTION_MOVE, time);
   *     }
   *   else
   *     gtk_drag_finish (context, FALSE, FALSE, time);
   *  }
   * ]|
   */
  widget_signals[DRAG_DATA_RECEIVED] =
    g_signal_new (I_("drag-data-received"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT_INT_BOXED_UINT_UINT,
		  G_TYPE_NONE, 6,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::visibility-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventVisibility): the #GdkEventVisibility which
   *   triggered this signal.
   *
   * The ::visibility-notify-event will be emitted when the @widget's
   * window is obscured or unobscured.
   *
   * To receive this signal the #GdkWindow associated to the widget needs
   * to enable the #GDK_VISIBILITY_NOTIFY_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   *
   * Deprecated: 3.12: Modern composited windowing systems with pervasive
   *     transparency make it impossible to track the visibility of a window
   *     reliably, so this signal can not be guaranteed to provide useful
   *     information.
   */
  widget_signals[VISIBILITY_NOTIFY_EVENT] =
    g_signal_new (I_("visibility-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
		  G_STRUCT_OFFSET (GtkWidgetClass, visibility_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::window-state-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventWindowState): the #GdkEventWindowState which
   *   triggered this signal.
   *
   * The ::window-state-event will be emitted when the state of the
   * toplevel window associated to the @widget changes.
   *
   * To receive this signal the #GdkWindow associated to the widget
   * needs to enable the #GDK_STRUCTURE_MASK mask. GDK will enable
   * this mask automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the
   *   event. %FALSE to propagate the event further.
   */
  widget_signals[WINDOW_STATE_EVENT] =
    g_signal_new (I_("window-state-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, window_state_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[WINDOW_STATE_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::damage-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventExpose): the #GdkEventExpose event
   *
   * Emitted when a redirected window belonging to @widget gets drawn into.
   * The region/area members of the event shows what area of the redirected
   * drawable was drawn into.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   *
   * Since: 2.14
   */
  widget_signals[DAMAGE_EVENT] =
    g_signal_new (I_("damage-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, damage_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[DAMAGE_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

/**
   * GtkWidget::grab-broken-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventGrabBroken): the #GdkEventGrabBroken event
   *
   * Emitted when a pointer or keyboard grab on a window belonging
   * to @widget gets broken.
   *
   * On X11, this happens when the grab window becomes unviewable
   * (i.e. it or one of its ancestors is unmapped), or if the same
   * application grabs the pointer or keyboard again.
   *
   * Returns: %TRUE to stop other handlers from being invoked for
   *   the event. %FALSE to propagate the event further.
   *
   * Since: 2.8
   */
  widget_signals[GRAB_BROKEN_EVENT] =
    g_signal_new (I_("grab-broken-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, grab_broken_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[GRAB_BROKEN_EVENT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__BOXEDv);

  /**
   * GtkWidget::query-tooltip:
   * @widget: the object which received the signal
   * @x: the x coordinate of the cursor position where the request has
   *     been emitted, relative to @widget's left side
   * @y: the y coordinate of the cursor position where the request has
   *     been emitted, relative to @widget's top
   * @keyboard_mode: %TRUE if the tooltip was trigged using the keyboard
   * @tooltip: a #GtkTooltip
   *
   * Emitted when #GtkWidget:has-tooltip is %TRUE and the hover timeout
   * has expired with the cursor hovering "above" @widget; or emitted when @widget got
   * focus in keyboard mode.
   *
   * Using the given coordinates, the signal handler should determine
   * whether a tooltip should be shown for @widget. If this is the case
   * %TRUE should be returned, %FALSE otherwise.  Note that if
   * @keyboard_mode is %TRUE, the values of @x and @y are undefined and
   * should not be used.
   *
   * The signal handler is free to manipulate @tooltip with the therefore
   * destined function calls.
   *
   * Returns: %TRUE if @tooltip should be shown right now, %FALSE otherwise.
   *
   * Since: 2.12
   */
  widget_signals[QUERY_TOOLTIP] =
    g_signal_new (I_("query-tooltip"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, query_tooltip),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__INT_INT_BOOLEAN_OBJECT,
		  G_TYPE_BOOLEAN, 4,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN,
		  GTK_TYPE_TOOLTIP);

  /**
   * GtkWidget::popup-menu:
   * @widget: the object which received the signal
   *
   * This signal gets emitted whenever a widget should pop up a context
   * menu. This usually happens through the standard key binding mechanism;
   * by pressing a certain key while a widget is focused, the user can cause
   * the widget to pop up a menu.  For example, the #GtkEntry widget creates
   * a menu with clipboard commands. See the
   * [Popup Menu Migration Checklist][checklist-popup-menu]
   * for an example of how to use this signal.
   *
   * Returns: %TRUE if a menu was activated
   */
  widget_signals[POPUP_MENU] =
    g_signal_new (I_("popup-menu"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, popup_menu),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  /**
   * GtkWidget::show-help:
   * @widget: the object which received the signal.
   * @help_type:
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   * %FALSE to propagate the event further.
   */
  widget_signals[SHOW_HELP] =
    g_signal_new (I_("show-help"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, show_help),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__ENUM,
		  G_TYPE_BOOLEAN, 1,
		  GTK_TYPE_WIDGET_HELP_TYPE);

  /**
   * GtkWidget::accel-closures-changed:
   * @widget: the object which received the signal.
   */
  widget_signals[ACCEL_CLOSURES_CHANGED] =
    g_signal_new (I_("accel-closures-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  0,
		  0,
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::screen-changed:
   * @widget: the object on which the signal is emitted
   * @previous_screen: (allow-none): the previous screen, or %NULL if the
   *   widget was not associated with a screen before
   *
   * The ::screen-changed signal gets emitted when the
   * screen of a widget has changed.
   */
  widget_signals[SCREEN_CHANGED] =
    g_signal_new (I_("screen-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, screen_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_SCREEN);

  /**
   * GtkWidget::can-activate-accel:
   * @widget: the object which received the signal
   * @signal_id: the ID of a signal installed on @widget
   *
   * Determines whether an accelerator that activates the signal
   * identified by @signal_id can currently be activated.
   * This signal is present to allow applications and derived
   * widgets to override the default #GtkWidget handling
   * for determining whether an accelerator can be activated.
   *
   * Returns: %TRUE if the signal can be activated.
   */
  widget_signals[CAN_ACTIVATE_ACCEL] =
     g_signal_new (I_("can-activate-accel"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, can_activate_accel),
                  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__UINT,
                  G_TYPE_BOOLEAN, 1, G_TYPE_UINT);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F10, GDK_SHIFT_MASK,
                                "popup-menu", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Menu, 0,
                                "popup-menu", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F1, GDK_CONTROL_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_TOOLTIP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_F1, GDK_CONTROL_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_TOOLTIP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F1, GDK_SHIFT_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_WHATS_THIS);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_F1, GDK_SHIFT_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_WHATS_THIS);

  /**
   * GtkWidget:interior-focus:
   *
   * The "interior-focus" style property defines whether
   * to draw the focus indicator inside widgets.
   *
   * Deprecated: 3.14: use the outline CSS properties instead.
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boolean ("interior-focus",
								 P_("Interior Focus"),
								 P_("Whether to draw the focus indicator inside widgets"),
								 TRUE,
								 GTK_PARAM_READABLE | G_PARAM_DEPRECATED));
  /**
   * GtkWidget:focus-line-width:
   *
   * The "focus-line-width" style property defines the width,
   * in pixels, of the focus indicator line
   *
   * Deprecated: 3.14: use the outline-width and padding CSS properties instead.
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_int ("focus-line-width",
							     P_("Focus linewidth"),
							     P_("Width, in pixels, of the focus indicator line"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE | G_PARAM_DEPRECATED));
  /**
   * GtkWidget:focus-line-pattern:
   *
   * The "focus-line-pattern" style property defines the dash pattern used to
   * draw the focus indicator. The character values are interpreted as pixel
   * widths of alternating on and off segments of the line.
   *
   * Deprecated: 3.14: use the outline-style CSS property instead.
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_string ("focus-line-pattern",
								P_("Focus line dash pattern"),
								P_("Dash pattern used to draw the focus indicator. The character values are interpreted as pixel widths of alternating on and off segments of the line."),
								"\1\1",
								GTK_PARAM_READABLE | G_PARAM_DEPRECATED));
  /**
   * GtkWidget:focus-padding:
   *
   * The "focus-padding" style property defines the width, in pixels,
   * between focus indicator and the widget 'box'.
   *
   * Deprecated: 3.14: use the padding CSS properties instead.
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_int ("focus-padding",
							     P_("Focus padding"),
							     P_("Width, in pixels, between focus indicator and the widget 'box'"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE | G_PARAM_DEPRECATED));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("cursor-color",
							       P_("Cursor color"),
							       P_("Color with which to draw insertion cursor"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("secondary-cursor-color",
							       P_("Secondary cursor color"),
							       P_("Color with which to draw the secondary insertion cursor when editing mixed right-to-left and left-to-right text"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_float ("cursor-aspect-ratio",
							       P_("Cursor line aspect ratio"),
							       P_("Aspect ratio with which to draw insertion cursor"),
							       0.0, 1.0, 0.04,
							       GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_boolean ("window-dragging",
                                                                 P_("Window dragging"),
                                                                 P_("Whether windows can be dragged and maximized by clicking on empty areas"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /**
   * GtkWidget:link-color:
   *
   * The "link-color" style property defines the color of unvisited links.
   *
   * Since: 2.10
   *
   * Deprecated: 3.12: Links now use a separate state flags for selecting
   *     different theming, this style property is ignored
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("link-color",
							       P_("Unvisited Link Color"),
							       P_("Color of unvisited links"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkWidget:visited-link-color:
   *
   * The "visited-link-color" style property defines the color of visited links.
   *
   * Since: 2.10
   *
   * Deprecated: 3.12: Links now use a separate state flags for selecting
   *     different theming, this style property is ignored
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("visited-link-color",
							       P_("Visited Link Color"),
							       P_("Color of visited links"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE|G_PARAM_DEPRECATED));
G_GNUC_END_IGNORE_DEPRECATIONS

  /**
   * GtkWidget:wide-separators:
   *
   * The "wide-separators" style property defines whether separators have
   * configurable width and should be drawn using a box instead of a line.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_boolean ("wide-separators",
                                                                 P_("Wide Separators"),
                                                                 P_("Whether separators have configurable width and should be drawn using a box instead of a line"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkWidget:separator-width:
   *
   * The "separator-width" style property defines the width of separators.
   * This property only takes effect if the "wide-separators" style property is %TRUE.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("separator-width",
                                                             P_("Separator Width"),
                                                             P_("The width of separators if wide-separators is TRUE"),
                                                             0, G_MAXINT, 0,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkWidget:separator-height:
   *
   * The "separator-height" style property defines the height of separators.
   * This property only takes effect if the "wide-separators" style property is %TRUE.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("separator-height",
                                                             P_("Separator Height"),
                                                             P_("The height of separators if \"wide-separators\" is TRUE"),
                                                             0, G_MAXINT, 0,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkWidget:scroll-arrow-hlength:
   *
   * The "scroll-arrow-hlength" style property defines the length of
   * horizontal scroll arrows.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("scroll-arrow-hlength",
                                                             P_("Horizontal Scroll Arrow Length"),
                                                             P_("The length of horizontal scroll arrows"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkWidget:scroll-arrow-vlength:
   *
   * The "scroll-arrow-vlength" style property defines the length of
   * vertical scroll arrows.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("scroll-arrow-vlength",
                                                             P_("Vertical Scroll Arrow Length"),
                                                             P_("The length of vertical scroll arrows"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("text-handle-width",
                                                             P_("Width of text selection handles"),
                                                             P_("Width of text selection handles"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("text-handle-height",
                                                             P_("Height of text selection handles"),
                                                             P_("Height of text selection handles"),
                                                             1, G_MAXINT, 20,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_set_accessible_type (klass, GTK_TYPE_WIDGET_ACCESSIBLE);
}

static void
gtk_widget_base_class_finalize (GtkWidgetClass *klass)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (style_property_spec_pool, G_OBJECT_CLASS_TYPE (klass));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (style_property_spec_pool, pspec);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);

  template_data_free (klass->priv->template);
}

static void
gtk_widget_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);

  switch (prop_id)
    {
      gboolean tmp;
      gchar *tooltip_markup;
      const gchar *tooltip_text;
      GtkWindow *tooltip_window;

    case PROP_NAME:
      gtk_widget_set_name (widget, g_value_get_string (value));
      break;
    case PROP_PARENT:
      gtk_container_add (GTK_CONTAINER (g_value_get_object (value)), widget);
      break;
    case PROP_WIDTH_REQUEST:
      gtk_widget_set_usize_internal (widget, g_value_get_int (value), -2, 0);
      break;
    case PROP_HEIGHT_REQUEST:
      gtk_widget_set_usize_internal (widget, -2, g_value_get_int (value), 0);
      break;
    case PROP_VISIBLE:
      gtk_widget_set_visible (widget, g_value_get_boolean (value));
      break;
    case PROP_SENSITIVE:
      gtk_widget_set_sensitive (widget, g_value_get_boolean (value));
      break;
    case PROP_APP_PAINTABLE:
      gtk_widget_set_app_paintable (widget, g_value_get_boolean (value));
      break;
    case PROP_CAN_FOCUS:
      gtk_widget_set_can_focus (widget, g_value_get_boolean (value));
      break;
    case PROP_HAS_FOCUS:
      if (g_value_get_boolean (value))
	gtk_widget_grab_focus (widget);
      break;
    case PROP_IS_FOCUS:
      if (g_value_get_boolean (value))
	gtk_widget_grab_focus (widget);
      break;
    case PROP_CAN_DEFAULT:
      gtk_widget_set_can_default (widget, g_value_get_boolean (value));
      break;
    case PROP_HAS_DEFAULT:
      if (g_value_get_boolean (value))
	gtk_widget_grab_default (widget);
      break;
    case PROP_RECEIVES_DEFAULT:
      gtk_widget_set_receives_default (widget, g_value_get_boolean (value));
      break;
    case PROP_STYLE:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_set_style (widget, g_value_get_object (value));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_EVENTS:
      if (!gtk_widget_get_realized (widget) && gtk_widget_get_has_window (widget))
	gtk_widget_set_events (widget, g_value_get_flags (value));
      break;
    case PROP_NO_SHOW_ALL:
      gtk_widget_set_no_show_all (widget, g_value_get_boolean (value));
      break;
    case PROP_HAS_TOOLTIP:
      gtk_widget_real_set_has_tooltip (widget,
				       g_value_get_boolean (value), FALSE);
      break;
    case PROP_TOOLTIP_MARKUP:
      tooltip_window = g_object_get_qdata (object, quark_tooltip_window);
      tooltip_markup = g_value_dup_string (value);

      /* Treat an empty string as a NULL string,
       * because an empty string would be useless for a tooltip:
       */
      if (tooltip_markup && (strlen (tooltip_markup) == 0))
        {
	  g_free (tooltip_markup);
          tooltip_markup = NULL;
        }

      g_object_set_qdata_full (object, quark_tooltip_markup,
			       tooltip_markup, g_free);

      tmp = (tooltip_window != NULL || tooltip_markup != NULL);
      gtk_widget_real_set_has_tooltip (widget, tmp, FALSE);
      if (gtk_widget_get_visible (widget))
        gtk_widget_queue_tooltip_query (widget);
      break;
    case PROP_TOOLTIP_TEXT:
      tooltip_window = g_object_get_qdata (object, quark_tooltip_window);

      tooltip_text = g_value_get_string (value);

      /* Treat an empty string as a NULL string,
       * because an empty string would be useless for a tooltip:
       */
      if (tooltip_text && (strlen (tooltip_text) == 0))
        tooltip_text = NULL;

      tooltip_markup = tooltip_text ? g_markup_escape_text (tooltip_text, -1) : NULL;

      g_object_set_qdata_full (object, quark_tooltip_markup,
                               tooltip_markup, g_free);

      tmp = (tooltip_window != NULL || tooltip_markup != NULL);
      gtk_widget_real_set_has_tooltip (widget, tmp, FALSE);
      if (gtk_widget_get_visible (widget))
        gtk_widget_queue_tooltip_query (widget);
      break;
    case PROP_DOUBLE_BUFFERED:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_widget_set_double_buffered (widget, g_value_get_boolean (value));
      G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    case PROP_HALIGN:
      gtk_widget_set_halign (widget, g_value_get_enum (value));
      break;
    case PROP_VALIGN:
      gtk_widget_set_valign (widget, g_value_get_enum (value));
      break;
    case PROP_MARGIN_LEFT:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_set_margin_left (widget, g_value_get_int (value));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_MARGIN_RIGHT:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_set_margin_right (widget, g_value_get_int (value));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_MARGIN_START:
      gtk_widget_set_margin_start (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_END:
      gtk_widget_set_margin_end (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_TOP:
      gtk_widget_set_margin_top (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_BOTTOM:
      gtk_widget_set_margin_bottom (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN:
      g_object_freeze_notify (G_OBJECT (widget));
      gtk_widget_set_margin_start (widget, g_value_get_int (value));
      gtk_widget_set_margin_end (widget, g_value_get_int (value));
      gtk_widget_set_margin_top (widget, g_value_get_int (value));
      gtk_widget_set_margin_bottom (widget, g_value_get_int (value));
      g_object_thaw_notify (G_OBJECT (widget));
      break;
    case PROP_HEXPAND:
      gtk_widget_set_hexpand (widget, g_value_get_boolean (value));
      break;
    case PROP_HEXPAND_SET:
      gtk_widget_set_hexpand_set (widget, g_value_get_boolean (value));
      break;
    case PROP_VEXPAND:
      gtk_widget_set_vexpand (widget, g_value_get_boolean (value));
      break;
    case PROP_VEXPAND_SET:
      gtk_widget_set_vexpand_set (widget, g_value_get_boolean (value));
      break;
    case PROP_EXPAND:
      g_object_freeze_notify (G_OBJECT (widget));
      gtk_widget_set_hexpand (widget, g_value_get_boolean (value));
      gtk_widget_set_vexpand (widget, g_value_get_boolean (value));
      g_object_thaw_notify (G_OBJECT (widget));
      break;
    case PROP_OPACITY:
      gtk_widget_set_opacity (widget, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;

  switch (prop_id)
    {
      gpointer *eventp;

    case PROP_NAME:
      if (priv->name)
	g_value_set_string (value, priv->name);
      else
	g_value_set_static_string (value, "");
      break;
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    case PROP_WIDTH_REQUEST:
      {
        int w;
        gtk_widget_get_size_request (widget, &w, NULL);
        g_value_set_int (value, w);
      }
      break;
    case PROP_HEIGHT_REQUEST:
      {
        int h;
        gtk_widget_get_size_request (widget, NULL, &h);
        g_value_set_int (value, h);
      }
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, gtk_widget_get_visible (widget));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, gtk_widget_get_sensitive (widget));
      break;
    case PROP_APP_PAINTABLE:
      g_value_set_boolean (value, gtk_widget_get_app_paintable (widget));
      break;
    case PROP_CAN_FOCUS:
      g_value_set_boolean (value, gtk_widget_get_can_focus (widget));
      break;
    case PROP_HAS_FOCUS:
      g_value_set_boolean (value, gtk_widget_has_focus (widget));
      break;
    case PROP_IS_FOCUS:
      g_value_set_boolean (value, gtk_widget_is_focus (widget));
      break;
    case PROP_CAN_DEFAULT:
      g_value_set_boolean (value, gtk_widget_get_can_default (widget));
      break;
    case PROP_HAS_DEFAULT:
      g_value_set_boolean (value, gtk_widget_has_default (widget));
      break;
    case PROP_RECEIVES_DEFAULT:
      g_value_set_boolean (value, gtk_widget_get_receives_default (widget));
      break;
    case PROP_COMPOSITE_CHILD:
      g_value_set_boolean (value, widget->priv->composite_child);
      break;
    case PROP_STYLE:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_object (value, gtk_widget_get_style (widget));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_EVENTS:
      eventp = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
      g_value_set_flags (value, GPOINTER_TO_INT (eventp));
      break;
    case PROP_NO_SHOW_ALL:
      g_value_set_boolean (value, gtk_widget_get_no_show_all (widget));
      break;
    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, GPOINTER_TO_UINT (g_object_get_qdata (object, quark_has_tooltip)));
      break;
    case PROP_TOOLTIP_TEXT:
      {
        gchar *escaped = g_object_get_qdata (object, quark_tooltip_markup);
        gchar *text = NULL;

        if (escaped && !pango_parse_markup (escaped, -1, 0, NULL, &text, NULL, NULL))
          g_assert (NULL == text); /* text should still be NULL in case of markup errors */

        g_value_take_string (value, text);
      }
      break;
    case PROP_TOOLTIP_MARKUP:
      g_value_set_string (value, g_object_get_qdata (object, quark_tooltip_markup));
      break;
    case PROP_WINDOW:
      g_value_set_object (value, gtk_widget_get_window (widget));
      break;
    case PROP_DOUBLE_BUFFERED:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_boolean (value, gtk_widget_get_double_buffered (widget));
      G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    case PROP_HALIGN:
      g_value_set_enum (value, gtk_widget_get_halign (widget));
      break;
    case PROP_VALIGN:
      g_value_set_enum (value, gtk_widget_get_valign_with_baseline (widget));
      break;
    case PROP_MARGIN_LEFT:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_int (value, gtk_widget_get_margin_left (widget));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_MARGIN_RIGHT:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_int (value, gtk_widget_get_margin_right (widget));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_MARGIN_START:
      g_value_set_int (value, gtk_widget_get_margin_start (widget));
      break;
    case PROP_MARGIN_END:
      g_value_set_int (value, gtk_widget_get_margin_end (widget));
      break;
    case PROP_MARGIN_TOP:
      g_value_set_int (value, gtk_widget_get_margin_top (widget));
      break;
    case PROP_MARGIN_BOTTOM:
      g_value_set_int (value, gtk_widget_get_margin_bottom (widget));
      break;
    case PROP_MARGIN:
      {
        GtkWidgetAuxInfo *aux_info = gtk_widget_get_aux_info (widget, FALSE);
        if (aux_info == NULL)
          {
            g_value_set_int (value, 0);
          }
        else
          {
            g_value_set_int (value, MAX (MAX (aux_info->margin.left,
                                              aux_info->margin.right),
                                         MAX (aux_info->margin.top,
                                              aux_info->margin.bottom)));
          }
      }
      break;
    case PROP_HEXPAND:
      g_value_set_boolean (value, gtk_widget_get_hexpand (widget));
      break;
    case PROP_HEXPAND_SET:
      g_value_set_boolean (value, gtk_widget_get_hexpand_set (widget));
      break;
    case PROP_VEXPAND:
      g_value_set_boolean (value, gtk_widget_get_vexpand (widget));
      break;
    case PROP_VEXPAND_SET:
      g_value_set_boolean (value, gtk_widget_get_vexpand_set (widget));
      break;
    case PROP_EXPAND:
      g_value_set_boolean (value,
                           gtk_widget_get_hexpand (widget) &&
                           gtk_widget_get_vexpand (widget));
      break;
    case PROP_OPACITY:
      g_value_set_double (value, gtk_widget_get_opacity (widget));
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_int (value, gtk_widget_get_scale_factor (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_widget_emulate_press (GtkWidget      *widget,
                           const GdkEvent *event)
{
  GtkWidget *event_widget, *next_child, *parent;
  GdkEvent *press;

  event_widget = gtk_get_event_widget ((GdkEvent *) event);

  if (event_widget == widget)
    return;

  if (event->type == GDK_TOUCH_BEGIN ||
      event->type == GDK_TOUCH_UPDATE ||
      event->type == GDK_TOUCH_END)
    {
      press = gdk_event_copy (event);
      press->type = GDK_TOUCH_BEGIN;
    }
  else if (event->type == GDK_BUTTON_PRESS ||
           event->type == GDK_BUTTON_RELEASE)
    {
      press = gdk_event_copy (event);
      press->type = GDK_BUTTON_PRESS;
    }
  else if (event->type == GDK_MOTION_NOTIFY)
    {
      press = gdk_event_new (GDK_BUTTON_PRESS);
      press->button.window = g_object_ref (event->motion.window);
      press->button.time = event->motion.time;
      press->button.x = event->motion.x;
      press->button.y = event->motion.y;
      press->button.x_root = event->motion.x_root;
      press->button.y_root = event->motion.y_root;
      press->button.state = event->motion.state;

      press->button.axes = g_memdup (event->motion.axes,
                                     sizeof (gdouble) *
                                     gdk_device_get_n_axes (event->motion.device));

      if (event->motion.state & GDK_BUTTON3_MASK)
        press->button.button = 3;
      else if (event->motion.state & GDK_BUTTON2_MASK)
        press->button.button = 2;
      else
        {
          if ((event->motion.state & GDK_BUTTON1_MASK) == 0)
            g_critical ("Guessing button number 1 on generated button press event");

          press->button.button = 1;
        }

      gdk_event_set_device (press, gdk_event_get_device (event));
      gdk_event_set_source_device (press, gdk_event_get_source_device (event));
    }
  else
    return;

  press->any.send_event = TRUE;
  next_child = event_widget;
  parent = gtk_widget_get_parent (next_child);

  while (parent != widget)
    {
      next_child = parent;
      parent = gtk_widget_get_parent (parent);
    }

  /* Perform propagation state starting from the next child in the chain */
  if (!_gtk_propagate_captured_event (event_widget, press, next_child))
    gtk_propagate_event (event_widget, press);

  gdk_event_free (press);
}

static const GdkEvent *
_gtk_widget_get_last_event (GtkWidget        *widget,
                            GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = widget->priv;
  EventControllerData *data;
  const GdkEvent *event;
  GList *l;

  for (l = priv->event_controllers; l; l = l->next)
    {
      data = l->data;

      if (!GTK_IS_GESTURE (data->controller))
        continue;

      event = gtk_gesture_get_last_event (GTK_GESTURE (data->controller),
                                          sequence);
      if (event)
        return event;
    }

  return NULL;
}

static gboolean
_gtk_widget_get_emulating_sequence (GtkWidget         *widget,
                                    GdkEventSequence  *sequence,
                                    GdkEventSequence **sequence_out)
{
  GtkWidgetPrivate *priv = widget->priv;
  GList *l;

  *sequence_out = sequence;

  if (sequence)
    {
      const GdkEvent *last_event;

      last_event = _gtk_widget_get_last_event (widget, sequence);

      if (last_event &&
          (last_event->type == GDK_TOUCH_BEGIN ||
           last_event->type == GDK_TOUCH_UPDATE ||
           last_event->type == GDK_TOUCH_END) &&
          last_event->touch.emulating_pointer)
        return TRUE;
    }
  else
    {
      /* For a NULL(pointer) sequence, find the pointer emulating one */
      for (l = priv->event_controllers; l; l = l->next)
        {
          EventControllerData *data = l->data;

          if (!GTK_IS_GESTURE (data->controller))
            continue;

          if (_gtk_gesture_get_pointer_emulating_sequence (GTK_GESTURE (data->controller),
                                                           sequence_out))
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gtk_widget_needs_press_emulation (GtkWidget        *widget,
                                  GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = widget->priv;
  gboolean sequence_press_handled = FALSE;
  GList *l;

  /* Check whether there is any remaining gesture in
   * the capture phase that handled the press event
   */
  for (l = priv->event_controllers; l; l = l->next)
    {
      EventControllerData *data;
      GtkPropagationPhase phase;
      GtkGesture *gesture;

      data = l->data;
      phase = gtk_event_controller_get_propagation_phase (data->controller);

      if (phase != GTK_PHASE_CAPTURE)
        continue;
      if (!GTK_IS_GESTURE (data->controller))
        continue;

      gesture = GTK_GESTURE (data->controller);
      sequence_press_handled |=
        (gtk_gesture_handles_sequence (gesture, sequence) &&
         _gtk_gesture_handled_sequence_press (gesture, sequence));
    }

  return !sequence_press_handled;
}

static gint
_gtk_widget_set_sequence_state_internal (GtkWidget             *widget,
                                         GdkEventSequence      *sequence,
                                         GtkEventSequenceState  state,
                                         GtkGesture            *emitter)
{
  gboolean emulates_pointer, sequence_handled = FALSE;
  GtkWidgetPrivate *priv = widget->priv;
  const GdkEvent *mimic_event;
  GList *group = NULL, *l;
  GdkEventSequence *seq;
  gint n_handled = 0;

  if (!priv->event_controllers && state != GTK_EVENT_SEQUENCE_CLAIMED)
    return TRUE;

  if (emitter)
    group = gtk_gesture_get_group (emitter);

  emulates_pointer = _gtk_widget_get_emulating_sequence (widget, sequence, &seq);
  mimic_event = _gtk_widget_get_last_event (widget, seq);

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventSequenceState gesture_state;
      EventControllerData *data;
      GtkGesture *gesture;
      gboolean retval;

      seq = sequence;
      data = l->data;
      gesture_state = state;

      if (!GTK_IS_GESTURE (data->controller))
        continue;

      gesture = GTK_GESTURE (data->controller);

      if (gesture == emitter)
        {
          sequence_handled |=
            _gtk_gesture_handled_sequence_press (gesture, sequence);
          n_handled++;
          continue;
        }

      if (seq && emulates_pointer &&
          !gtk_gesture_handles_sequence (gesture, seq))
        seq = NULL;

      if (group && !g_list_find (group, data->controller))
        {
          /* If a group is provided, ensure only gestures pertaining to the group
           * get a "claimed" state, all other claiming gestures must deny the sequence.
           */
          if (gesture_state == GTK_EVENT_SEQUENCE_CLAIMED &&
              gtk_gesture_get_sequence_state (gesture, sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
            gesture_state = GTK_EVENT_SEQUENCE_DENIED;
          else
            continue;
        }
      else if (!group &&
               gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_CLAIMED)
        continue;

      g_signal_handler_block (data->controller, data->sequence_state_changed_id);
      retval = gtk_gesture_set_sequence_state (gesture, seq, gesture_state);
      g_signal_handler_unblock (data->controller, data->sequence_state_changed_id);

      if (retval || gesture == emitter)
        {
          sequence_handled |=
            _gtk_gesture_handled_sequence_press (gesture, seq);
          n_handled++;
        }
    }

  /* If the sequence goes denied, check whether this is a controller attached
   * to the capture phase, that additionally handled the button/touch press (ie.
   * it was consumed), the corresponding press will be emulated for widgets
   * beneath, so the widgets beneath get a coherent stream of events from now on.
   */
  if (n_handled > 0 && sequence_handled &&
      state == GTK_EVENT_SEQUENCE_DENIED &&
      gtk_widget_needs_press_emulation (widget, sequence))
    _gtk_widget_emulate_press (widget, mimic_event);

  g_list_free (group);

  return n_handled;
}

static gboolean
_gtk_widget_cancel_sequence (GtkWidget        *widget,
                             GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = widget->priv;
  gboolean emulates_pointer;
  gboolean handled = FALSE;
  GdkEventSequence *seq;
  GList *l;

  emulates_pointer = _gtk_widget_get_emulating_sequence (widget, sequence, &seq);

  for (l = priv->event_controllers; l; l = l->next)
    {
      EventControllerData *data;
      GtkGesture *gesture;

      seq = sequence;
      data = l->data;

      if (!GTK_IS_GESTURE (data->controller))
        continue;

      gesture = GTK_GESTURE (data->controller);

      if (seq && emulates_pointer &&
          !gtk_gesture_handles_sequence (gesture, seq))
        seq = NULL;

      if (!gtk_gesture_handles_sequence (gesture, seq))
        continue;

      handled |= _gtk_gesture_cancel_sequence (gesture, seq);
    }

  return handled;
}

static void
gtk_widget_init (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  widget->priv = gtk_widget_get_instance_private (widget); 
  priv = widget->priv;

  priv->child_visible = TRUE;
  priv->name = NULL;
  priv->allocation.x = -1;
  priv->allocation.y = -1;
  priv->allocation.width = 1;
  priv->allocation.height = 1;
  priv->user_alpha = 255;
  priv->alpha = 255;
  priv->window = NULL;
  priv->parent = NULL;

  priv->sensitive = TRUE;
  priv->composite_child = composite_child_stack != 0;
  priv->double_buffered = TRUE;
  priv->redraw_on_alloc = TRUE;
  priv->alloc_needed = TRUE;
   
  switch (gtk_widget_get_direction (widget))
    {
    case GTK_TEXT_DIR_LTR:
      priv->state_flags = GTK_STATE_FLAG_DIR_LTR;
      break;

    case GTK_TEXT_DIR_RTL:
      priv->state_flags = GTK_STATE_FLAG_DIR_RTL;
      break;

    case GTK_TEXT_DIR_NONE:
    default:
      g_assert_not_reached ();
      break;
    }


  /* this will be set to TRUE if the widget gets a child or if the
   * expand flag is set on the widget, but until one of those happen
   * we know the expand is already properly FALSE.
   *
   * We really want to default FALSE here to avoid computing expand
   * all over the place while initially building a widget tree.
   */
  priv->need_compute_expand = FALSE;

  _gtk_size_request_cache_init (&priv->requests);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  priv->style = gtk_widget_get_default_style ();
  G_GNUC_END_IGNORE_DEPRECATIONS;
  g_object_ref (priv->style);
}


static void
gtk_widget_dispatch_child_properties_changed (GtkWidget   *widget,
					      guint        n_pspecs,
					      GParamSpec **pspecs)
{
  GtkWidgetPrivate *priv = widget->priv;
  GtkWidget *container = priv->parent;
  guint i;

  for (i = 0; widget->priv->parent == container && i < n_pspecs; i++)
    g_signal_emit (widget, widget_signals[CHILD_NOTIFY], g_quark_from_string (pspecs[i]->name), pspecs[i]);
}

/**
 * gtk_widget_freeze_child_notify:
 * @widget: a #GtkWidget
 *
 * Stops emission of #GtkWidget::child-notify signals on @widget. The
 * signals are queued until gtk_widget_thaw_child_notify() is called
 * on @widget.
 *
 * This is the analogue of g_object_freeze_notify() for child properties.
 **/
void
gtk_widget_freeze_child_notify (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!G_OBJECT (widget)->ref_count)
    return;

  g_object_ref (widget);
  g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);
  g_object_unref (widget);
}

/**
 * gtk_widget_child_notify:
 * @widget: a #GtkWidget
 * @child_property: the name of a child property installed on the
 *                  class of @widget’s parent
 *
 * Emits a #GtkWidget::child-notify signal for the
 * [child property][child-properties] @child_property
 * on @widget.
 *
 * This is the analogue of g_object_notify() for child properties.
 *
 * Also see gtk_container_child_notify().
 */
void
gtk_widget_child_notify (GtkWidget    *widget,
                         const gchar  *child_property)
{
  if (widget->priv->parent == NULL)
    return;

  gtk_container_child_notify (GTK_CONTAINER (widget->priv->parent), widget, child_property);
}

/**
 * gtk_widget_thaw_child_notify:
 * @widget: a #GtkWidget
 *
 * Reverts the effect of a previous call to gtk_widget_freeze_child_notify().
 * This causes all queued #GtkWidget::child-notify signals on @widget to be
 * emitted.
 */
void
gtk_widget_thaw_child_notify (GtkWidget *widget)
{
  GObjectNotifyQueue *nqueue;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!G_OBJECT (widget)->ref_count)
    return;

  g_object_ref (widget);
  nqueue = g_object_notify_queue_from_object (G_OBJECT (widget), _gtk_widget_child_property_notify_context);
  if (!nqueue || !nqueue->freeze_count)
    g_warning (G_STRLOC ": child-property-changed notification for %s(%p) is not frozen",
	       G_OBJECT_TYPE_NAME (widget), widget);
  else
    g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);
  g_object_unref (widget);
}


/**
 * gtk_widget_new:
 * @type: type ID of the widget to create
 * @first_property_name: name of first property to set
 * @...: value of first property, followed by more properties,
 *     %NULL-terminated
 *
 * This is a convenience function for creating a widget and setting
 * its properties in one go. For example you might write:
 * `gtk_widget_new (GTK_TYPE_LABEL, "label", "Hello World", "xalign",
 * 0.0, NULL)` to create a left-aligned label. Equivalent to
 * g_object_new(), but returns a widget so you don’t have to
 * cast the object yourself.
 *
 * Returns: a new #GtkWidget of type @widget_type
 **/
GtkWidget*
gtk_widget_new (GType        type,
		const gchar *first_property_name,
		...)
{
  GtkWidget *widget;
  va_list var_args;

  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), NULL);

  va_start (var_args, first_property_name);
  widget = (GtkWidget *)g_object_new_valist (type, first_property_name, var_args);
  va_end (var_args);

  return widget;
}

static inline void
gtk_widget_queue_draw_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;
  GtkWidget *parent;

  parent = priv->parent;
  if (parent && gtk_widget_is_drawable (parent))
    gtk_widget_queue_draw_area (parent,
				priv->clip.x,
				priv->clip.y,
				priv->clip.width,
				priv->clip.height);
}

/**
 * gtk_widget_unparent:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 * Should be called by implementations of the remove method
 * on #GtkContainer, to dissociate a child from the container.
 **/
void
gtk_widget_unparent (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GObjectNotifyQueue *nqueue;
  GtkWidget *toplevel;
  GtkWidget *old_parent;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (priv->parent == NULL)
    return;

  /* keep this function in sync with gtk_menu_detach() */

  gtk_widget_push_verify_invariants (widget);

  g_object_freeze_notify (G_OBJECT (widget));
  nqueue = g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);

  toplevel = gtk_widget_get_toplevel (widget);
  if (gtk_widget_is_toplevel (toplevel))
    _gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);

  if (gtk_container_get_focus_child (GTK_CONTAINER (priv->parent)) == widget)
    gtk_container_set_focus_child (GTK_CONTAINER (priv->parent), NULL);

  gtk_widget_queue_draw_child (widget);

  /* Reset the width and height here, to force reallocation if we
   * get added back to a new parent. This won't work if our new
   * allocation is smaller than 1x1 and we actually want a size of 1x1...
   * (would 0x0 be OK here?)
   */
  priv->allocation.width = 1;
  priv->allocation.height = 1;

  if (gtk_widget_get_realized (widget))
    {
      if (priv->in_reparent)
	gtk_widget_unmap (widget);
      else
	gtk_widget_unrealize (widget);
    }

  /* If we are unanchoring the child, we save around the toplevel
   * to emit hierarchy changed
   */
  if (priv->parent->priv->anchored)
    g_object_ref (toplevel);
  else
    toplevel = NULL;

  /* Removing a widget from a container restores the child visible
   * flag to the default state, so it doesn't affect the child
   * in the next parent.
   */
  priv->child_visible = TRUE;

  old_parent = priv->parent;
  priv->parent = NULL;

  /* parent may no longer expand if the removed
   * child was expand=TRUE and could therefore
   * be forcing it to.
   */
  if (gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (old_parent);
    }

  /* Unset BACKDROP since we are no longer inside a toplevel window */
  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
  if (priv->context)
    gtk_style_context_set_parent (priv->context, NULL);

  _gtk_widget_update_parent_muxer (widget);

  g_signal_emit (widget, widget_signals[PARENT_SET], 0, old_parent);
  if (toplevel)
    {
      _gtk_widget_propagate_hierarchy_changed (widget, toplevel);
      g_object_unref (toplevel);
    }

  /* Now that the parent pointer is nullified and the hierarchy-changed
   * already passed, go ahead and unset the parent window, if we are unparenting
   * an embeded GtkWindow the window will become toplevel again and hierarchy-changed
   * will fire again for the new subhierarchy.
   */
  gtk_widget_set_parent_window (widget, NULL);

  g_object_notify (G_OBJECT (widget), "parent");
  g_object_thaw_notify (G_OBJECT (widget));
  if (!priv->parent)
    g_object_notify_queue_clear (G_OBJECT (widget), nqueue);
  g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_destroy:
 * @widget: a #GtkWidget
 *
 * Destroys a widget.
 *
 * When a widget is
 * destroyed, it will break any references it holds to other objects.
 * If the widget is inside a container, the widget will be removed
 * from the container. If the widget is a toplevel (derived from
 * #GtkWindow), it will be removed from the list of toplevels, and the
 * reference GTK+ holds to it will be removed. Removing a
 * widget from its container or the list of toplevels results in the
 * widget being finalized, unless you’ve added additional references
 * to the widget with g_object_ref().
 *
 * In most cases, only toplevel widgets (windows) require explicit
 * destruction, because when you destroy a toplevel its children will
 * be destroyed as well.
 **/
void
gtk_widget_destroy (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!widget->priv->in_destruction)
    g_object_run_dispose (G_OBJECT (widget));
}

/**
 * gtk_widget_destroyed:
 * @widget: a #GtkWidget
 * @widget_pointer: (inout) (transfer none): address of a variable that contains @widget
 *
 * This function sets *@widget_pointer to %NULL if @widget_pointer !=
 * %NULL.  It’s intended to be used as a callback connected to the
 * “destroy” signal of a widget. You connect gtk_widget_destroyed()
 * as a signal handler, and pass the address of your widget variable
 * as user data. Then when the widget is destroyed, the variable will
 * be set to %NULL. Useful for example to avoid multiple copies
 * of the same dialog.
 **/
void
gtk_widget_destroyed (GtkWidget      *widget,
		      GtkWidget      **widget_pointer)
{
  /* Don't make any assumptions about the
   *  value of widget!
   *  Even check widget_pointer.
   */
  if (widget_pointer)
    *widget_pointer = NULL;
}

/**
 * gtk_widget_show:
 * @widget: a #GtkWidget
 *
 * Flags a widget to be displayed. Any widget that isn’t shown will
 * not appear on the screen. If you want to show all the widgets in a
 * container, it’s easier to call gtk_widget_show_all() on the
 * container, instead of individually showing the widgets.
 *
 * Remember that you have to show the containers containing a widget,
 * in addition to the widget itself, before it will appear onscreen.
 *
 * When a toplevel container is shown, it is immediately realized and
 * mapped; other shown widgets are realized and mapped when their
 * toplevel container is realized and mapped.
 **/
void
gtk_widget_show (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_get_visible (widget))
    {
      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      if (!gtk_widget_is_toplevel (widget))
        gtk_widget_queue_resize (widget);

      /* see comment in set_parent() for why this should and can be
       * conditional
       */
      if (widget->priv->need_compute_expand ||
          widget->priv->computed_hexpand ||
          widget->priv->computed_vexpand)
        {
          if (widget->priv->parent != NULL)
            gtk_widget_queue_compute_expand (widget->priv->parent);
        }

      g_signal_emit (widget, widget_signals[SHOW], 0);
      g_object_notify (G_OBJECT (widget), "visible");

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_show (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (!gtk_widget_get_visible (widget))
    {
      priv->visible = TRUE;

      if (priv->parent &&
	  gtk_widget_get_mapped (priv->parent) &&
          gtk_widget_get_child_visible (widget) &&
	  !gtk_widget_get_mapped (widget))
	gtk_widget_map (widget);
    }
}

static void
gtk_widget_show_map_callback (GtkWidget *widget, GdkEvent *event, gint *flag)
{
  *flag = TRUE;
  g_signal_handlers_disconnect_by_func (widget,
					gtk_widget_show_map_callback,
					flag);
}

/**
 * gtk_widget_show_now:
 * @widget: a #GtkWidget
 *
 * Shows a widget. If the widget is an unmapped toplevel widget
 * (i.e. a #GtkWindow that has not yet been shown), enter the main
 * loop and wait for the window to actually be mapped. Be careful;
 * because the main loop is running, anything can happen during
 * this function.
 **/
void
gtk_widget_show_now (GtkWidget *widget)
{
  gint flag = FALSE;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* make sure we will get event */
  if (!gtk_widget_get_mapped (widget) &&
      gtk_widget_is_toplevel (widget))
    {
      gtk_widget_show (widget);

      g_signal_connect (widget, "map-event",
			G_CALLBACK (gtk_widget_show_map_callback),
			&flag);

      while (!flag)
	gtk_main_iteration ();
    }
  else
    gtk_widget_show (widget);
}

/**
 * gtk_widget_hide:
 * @widget: a #GtkWidget
 *
 * Reverses the effects of gtk_widget_show(), causing the widget to be
 * hidden (invisible to the user).
 **/
void
gtk_widget_hide (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_visible (widget))
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      if (toplevel != widget && gtk_widget_is_toplevel (toplevel))
        _gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);

      /* a parent may now be expand=FALSE since we're hidden. */
      if (widget->priv->need_compute_expand ||
          widget->priv->computed_hexpand ||
          widget->priv->computed_vexpand)
        {
          gtk_widget_queue_compute_expand (widget);
        }

      g_signal_emit (widget, widget_signals[HIDE], 0);
      if (!gtk_widget_is_toplevel (widget))
	gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "visible");

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_hide (GtkWidget *widget)
{
  if (gtk_widget_get_visible (widget))
    {
      widget->priv->visible = FALSE;

      if (gtk_widget_get_mapped (widget))
	gtk_widget_unmap (widget);
    }
}

/**
 * gtk_widget_hide_on_delete:
 * @widget: a #GtkWidget
 *
 * Utility function; intended to be connected to the #GtkWidget::delete-event
 * signal on a #GtkWindow. The function calls gtk_widget_hide() on its
 * argument, then returns %TRUE. If connected to ::delete-event, the
 * result is that clicking the close button for a window (on the
 * window frame, top right corner usually) will hide but not destroy
 * the window. By default, GTK+ destroys windows when ::delete-event
 * is received.
 *
 * Returns: %TRUE
 **/
gboolean
gtk_widget_hide_on_delete (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_widget_hide (widget);

  return TRUE;
}

/**
 * gtk_widget_show_all:
 * @widget: a #GtkWidget
 *
 * Recursively shows a widget, and any child widgets (if the widget is
 * a container).
 **/
void
gtk_widget_show_all (GtkWidget *widget)
{
  GtkWidgetClass *class;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_no_show_all (widget))
    return;

  class = GTK_WIDGET_GET_CLASS (widget);

  if (class->show_all)
    class->show_all (widget);
}

/**
 * gtk_widget_map:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations. Causes
 * a widget to be mapped if it isn’t already.
 **/
void
gtk_widget_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_visible (widget));
  g_return_if_fail (gtk_widget_get_child_visible (widget));

  priv = widget->priv;

  if (!gtk_widget_get_mapped (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      if (!gtk_widget_get_realized (widget))
        gtk_widget_realize (widget);

      g_signal_emit (widget, widget_signals[MAP], 0);

      if (!gtk_widget_get_has_window (widget))
        gdk_window_invalidate_rect (priv->window, &priv->clip, FALSE);

      if (widget->priv->context)
        _gtk_style_context_update_animating (widget->priv->context);

      gtk_widget_pop_verify_invariants (widget);
    }
}

/**
 * gtk_widget_unmap:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations. Causes
 * a widget to be unmapped if it’s currently mapped.
 **/
void
gtk_widget_unmap (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (gtk_widget_get_mapped (widget))
    {
      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      if (!gtk_widget_get_has_window (widget))
	gdk_window_invalidate_rect (priv->window, &priv->clip, FALSE);
      _gtk_tooltip_hide (widget);

      if (widget->priv->context)
        _gtk_style_context_update_animating (widget->priv->context);

      g_signal_emit (widget, widget_signals[UNMAP], 0);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
_gtk_widget_enable_device_events (GtkWidget *widget)
{
  GHashTable *device_events;
  GHashTableIter iter;
  gpointer key, value;

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (!device_events)
    return;

  g_hash_table_iter_init (&iter, device_events);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDevice *device;
      GdkEventMask event_mask;

      device = key;
      event_mask = GPOINTER_TO_UINT (value);
      gtk_widget_add_events_internal (widget, device, event_mask);
    }
}

static GList *
get_widget_windows (GtkWidget *widget)
{
  GList *window_list, *last, *l, *children, *ret;

  if (gtk_widget_get_has_window (widget))
    window_list = g_list_prepend (NULL, gtk_widget_get_window (widget));
  else
    window_list = gdk_window_peek_children (gtk_widget_get_window (widget));

  last = g_list_last (window_list);
  ret = NULL;

  for (l = window_list; l; l = l->next)
    {
      GtkWidget *window_widget = NULL;

      gdk_window_get_user_data (l->data, (gpointer *) &window_widget);

      if (widget != window_widget)
        continue;

      ret = g_list_prepend (ret, l->data);
      children = gdk_window_peek_children (GDK_WINDOW (l->data));

      if (children)
        {
          last = g_list_concat (last, children);
          last = g_list_last (last);
        }
    }

  g_list_free (window_list);

  return ret;
}

static void
device_enable_foreach (GtkWidget *widget,
                       gpointer   user_data)
{
  GdkDevice *device = user_data;
  gtk_widget_set_device_enabled_internal (widget, device, TRUE, TRUE);
}

static void
device_disable_foreach (GtkWidget *widget,
                        gpointer   user_data)
{
  GdkDevice *device = user_data;
  gtk_widget_set_device_enabled_internal (widget, device, TRUE, FALSE);
}

static void
gtk_widget_set_device_enabled_internal (GtkWidget *widget,
                                        GdkDevice *device,
                                        gboolean   recurse,
                                        gboolean   enabled)
{
  GList *window_list, *l;

  window_list = get_widget_windows (widget);

  for (l = window_list; l; l = l->next)
    {
      GdkEventMask events = 0;
      GdkWindow *window;

      window = l->data;

      if (enabled)
        events = gdk_window_get_events (window);

      gdk_window_set_device_events (window, device, events);
    }

  if (recurse && GTK_IS_CONTAINER (widget))
    {
      if (enabled)
        gtk_container_forall (GTK_CONTAINER (widget), device_enable_foreach, device);
      else
        gtk_container_forall (GTK_CONTAINER (widget), device_disable_foreach, device);
    }

  g_list_free (window_list);
}

static void
gtk_widget_update_devices_mask (GtkWidget *widget,
                                gboolean   recurse)
{
  GList *enabled_devices, *l;

  enabled_devices = g_object_get_qdata (G_OBJECT (widget), quark_enabled_devices);

  for (l = enabled_devices; l; l = l->next)
    gtk_widget_set_device_enabled_internal (widget, GDK_DEVICE (l->data), recurse, TRUE);
}

typedef struct _GtkTickCallbackInfo GtkTickCallbackInfo;

struct _GtkTickCallbackInfo
{
  guint refcount;

  guint id;
  GtkTickCallback callback;
  gpointer user_data;
  GDestroyNotify notify;

  guint destroyed : 1;
};

static void
ref_tick_callback_info (GtkTickCallbackInfo *info)
{
  info->refcount++;
}

static void
unref_tick_callback_info (GtkWidget           *widget,
                          GtkTickCallbackInfo *info,
                          GList               *link)
{
  GtkWidgetPrivate *priv = widget->priv;

  info->refcount--;
  if (info->refcount == 0)
    {
      priv->tick_callbacks = g_list_delete_link (priv->tick_callbacks, link);
      if (info->notify)
        info->notify (info->user_data);
      g_slice_free (GtkTickCallbackInfo, info);
    }

  if (priv->tick_callbacks == NULL && priv->clock_tick_id)
    {
      GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (widget);
      g_signal_handler_disconnect (frame_clock, priv->clock_tick_id);
      priv->clock_tick_id = 0;
      gdk_frame_clock_end_updating (frame_clock);
    }
}

static void
destroy_tick_callback_info (GtkWidget           *widget,
                            GtkTickCallbackInfo *info,
                            GList               *link)
{
  if (!info->destroyed)
    {
      info->destroyed = TRUE;
      unref_tick_callback_info (widget, info, link);
    }
}

static void
gtk_widget_on_frame_clock_update (GdkFrameClock *frame_clock,
                                  GtkWidget     *widget)
{
  GtkWidgetPrivate *priv = widget->priv;
  GList *l;

  g_object_ref (widget);

  for (l = priv->tick_callbacks; l;)
    {
      GtkTickCallbackInfo *info = l->data;
      GList *next;

      ref_tick_callback_info (info);
      if (!info->destroyed)
        {
          if (info->callback (widget,
                              frame_clock,
                              info->user_data) == G_SOURCE_REMOVE)
            {
              destroy_tick_callback_info (widget, info, l);
            }
        }

      next = l->next;
      unref_tick_callback_info (widget, info, l);
      l = next;
    }

  g_object_unref (widget);
}

static guint tick_callback_id;

/**
 * gtk_widget_add_tick_callback:
 * @widget: a #GtkWidget
 * @callback: function to call for updating animations
 * @user_data: data to pass to @callback
 * @notify: function to call to free @user_data when the callback is removed.
 *
 * Queues an animation frame update and adds a callback to be called
 * before each frame. Until the tick callback is removed, it will be
 * called frequently (usually at the frame rate of the output device
 * or as quickly as the application can be repainted, whichever is
 * slower). For this reason, is most suitable for handling graphics
 * that change every frame or every few frames. The tick callback does
 * not automatically imply a relayout or repaint. If you want a
 * repaint or relayout, and aren’t changing widget properties that
 * would trigger that (for example, changing the text of a #GtkLabel),
 * then you will have to call gtk_widget_queue_resize() or
 * gtk_widget_queue_draw_area() yourself.
 *
 * gdk_frame_clock_get_frame_time() should generally be used for timing
 * continuous animations and
 * gdk_frame_timings_get_predicted_presentation_time() if you are
 * trying to display isolated frames at particular times.
 *
 * This is a more convenient alternative to connecting directly to the
 * #GdkFrameClock::update signal of #GdkFrameClock, since you don't
 * have to worry about when a #GdkFrameClock is assigned to a widget.
 *
 * Returns: an id for the connection of this callback. Remove the callback
 *     by passing it to gtk_widget_remove_tick_callback()
 *
 * Since: 3.8
 */
guint
gtk_widget_add_tick_callback (GtkWidget       *widget,
                              GtkTickCallback  callback,
                              gpointer         user_data,
                              GDestroyNotify   notify)
{
  GtkWidgetPrivate *priv;
  GtkTickCallbackInfo *info;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  priv = widget->priv;

  if (priv->realized && !priv->clock_tick_id)
    {
      GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (widget);
      priv->clock_tick_id = g_signal_connect (frame_clock, "update",
                                              G_CALLBACK (gtk_widget_on_frame_clock_update),
                                              widget);
      gdk_frame_clock_begin_updating (frame_clock);
    }

  info = g_slice_new0 (GtkTickCallbackInfo);

  info->refcount = 1;
  info->id = ++tick_callback_id;
  info->callback = callback;
  info->user_data = user_data;
  info->notify = notify;

  priv->tick_callbacks = g_list_prepend (priv->tick_callbacks,
                                         info);

  return info->id;
}

/**
 * gtk_widget_remove_tick_callback:
 * @widget: a #GtkWidget
 * @id: an id returned by gtk_widget_add_tick_callback()
 *
 * Removes a tick callback previously registered with
 * gtk_widget_add_tick_callback().
 *
 * Since: 3.8
 */
void
gtk_widget_remove_tick_callback (GtkWidget *widget,
                                 guint      id)
{
  GtkWidgetPrivate *priv;
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  for (l = priv->tick_callbacks; l; l = l->next)
    {
      GtkTickCallbackInfo *info = l->data;
      if (info->id == id)
        {
          destroy_tick_callback_info (widget, info, l);
          return;
        }
    }
}

gboolean
gtk_widget_has_tick_callback (GtkWidget *widget)
{
  return widget->priv->tick_callbacks != NULL;
}

static void
gtk_widget_connect_frame_clock (GtkWidget     *widget,
                                GdkFrameClock *frame_clock)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (GTK_IS_CONTAINER (widget))
    _gtk_container_maybe_start_idle_sizer (GTK_CONTAINER (widget));

  if (priv->tick_callbacks != NULL && !priv->clock_tick_id)
    {
      priv->clock_tick_id = g_signal_connect (frame_clock, "update",
                                              G_CALLBACK (gtk_widget_on_frame_clock_update),
                                              widget);
      gdk_frame_clock_begin_updating (frame_clock);
    }

  if (priv->context)
    gtk_style_context_set_frame_clock (priv->context, frame_clock);
}

static void
gtk_widget_disconnect_frame_clock (GtkWidget     *widget,
                                   GdkFrameClock *frame_clock)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (GTK_IS_CONTAINER (widget))
    _gtk_container_stop_idle_sizer (GTK_CONTAINER (widget));

  if (priv->clock_tick_id)
    {
      g_signal_handler_disconnect (frame_clock, priv->clock_tick_id);
      priv->clock_tick_id = 0;
      gdk_frame_clock_end_updating (frame_clock);
    }

  if (priv->context)
    gtk_style_context_set_frame_clock (priv->context, NULL);
}

/**
 * gtk_widget_realize:
 * @widget: a #GtkWidget
 *
 * Creates the GDK (windowing system) resources associated with a
 * widget.  For example, @widget->window will be created when a widget
 * is realized.  Normally realization happens implicitly; if you show
 * a widget and all its parent containers, then the widget will be
 * realized and mapped automatically.
 *
 * Realizing a widget requires all
 * the widget’s parent widgets to be realized; calling
 * gtk_widget_realize() realizes the widget’s parents in addition to
 * @widget itself. If a widget is not yet inside a toplevel window
 * when you realize it, bad things will happen.
 *
 * This function is primarily used in widget implementations, and
 * isn’t very useful otherwise. Many times when you think you might
 * need it, a better approach is to connect to a signal that will be
 * called after the widget is realized automatically, such as
 * #GtkWidget::draw. Or simply g_signal_connect () to the
 * #GtkWidget::realize signal.
 **/
void
gtk_widget_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  cairo_region_t *region;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->priv->anchored ||
		    GTK_IS_INVISIBLE (widget));

  priv = widget->priv;

  if (!gtk_widget_get_realized (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      /*
	if (GTK_IS_CONTAINER (widget) && gtk_widget_get_has_window (widget))
	  g_message ("gtk_widget_realize(%s)", G_OBJECT_TYPE_NAME (widget));
      */

      if (priv->parent == NULL &&
          !gtk_widget_is_toplevel (widget))
        g_warning ("Calling gtk_widget_realize() on a widget that isn't "
                   "inside a toplevel window is not going to work very well. "
                   "Widgets must be inside a toplevel container before realizing them.");

      if (priv->parent && !gtk_widget_get_realized (priv->parent))
	gtk_widget_realize (priv->parent);

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_ensure_style (widget);
      G_GNUC_END_IGNORE_DEPRECATIONS;

      if (priv->style_update_pending)
        g_signal_emit (widget, widget_signals[STYLE_UPDATED], 0);

      g_signal_emit (widget, widget_signals[REALIZE], 0);

      gtk_widget_real_set_has_tooltip (widget,
				       GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (widget), quark_has_tooltip)),
				       TRUE);

      if (priv->has_shape_mask)
	{
	  region = g_object_get_qdata (G_OBJECT (widget), quark_shape_info);
	  gdk_window_shape_combine_region (priv->window, region, 0, 0);
	}

      gtk_widget_update_input_shape (widget);

      if (priv->multidevice)
        gdk_window_set_support_multidevice (priv->window, TRUE);

      _gtk_widget_enable_device_events (widget);
      gtk_widget_update_devices_mask (widget, TRUE);

      gtk_widget_update_alpha (widget);

      if (priv->context)
	gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));
      gtk_widget_connect_frame_clock (widget,
                                      gtk_widget_get_frame_clock (widget));

      gtk_widget_pop_verify_invariants (widget);
    }
}

/**
 * gtk_widget_unrealize:
 * @widget: a #GtkWidget
 *
 * This function is only useful in widget implementations.
 * Causes a widget to be unrealized (frees all GDK resources
 * associated with the widget, such as @widget->window).
 **/
void
gtk_widget_unrealize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (widget);
  gtk_widget_push_verify_invariants (widget);

  if (widget->priv->has_shape_mask)
    gtk_widget_shape_combine_region (widget, NULL);

  if (g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info))
    gtk_widget_input_shape_combine_region (widget, NULL);

  if (gtk_widget_get_realized (widget))
    {
      if (widget->priv->mapped)
        gtk_widget_unmap (widget);

      gtk_widget_disconnect_frame_clock (widget,
                                         gtk_widget_get_frame_clock (widget));

      g_signal_emit (widget, widget_signals[UNREALIZE], 0);
      g_assert (!widget->priv->mapped);
      gtk_widget_set_realized (widget, FALSE);
    }

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

/*****************************************
 * Draw queueing.
 *****************************************/

static void
gtk_widget_real_queue_draw_region (GtkWidget         *widget,
				   const cairo_region_t *region)
{
  GtkWidgetPrivate *priv = widget->priv;

  gdk_window_invalidate_region (priv->window, region, TRUE);
}

/**
 * gtk_widget_queue_draw_region:
 * @widget: a #GtkWidget
 * @region: region to draw
 *
 * Invalidates the area of @widget defined by @region by calling
 * gdk_window_invalidate_region() on the widget’s window and all its
 * child windows. Once the main loop becomes idle (after the current
 * batch of events has been processed, roughly), the window will
 * receive expose events for the union of all regions that have been
 * invalidated.
 *
 * Normally you would only use this function in widget
 * implementations. You might also use it to schedule a redraw of a
 * #GtkDrawingArea or some portion thereof.
 *
 * Since: 3.0
 **/
void
gtk_widget_queue_draw_region (GtkWidget            *widget,
                              const cairo_region_t *region)
{
  GtkWidget *w;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_get_realized (widget))
    return;

  /* Just return if the widget or one of its ancestors isn't mapped */
  for (w = widget; w != NULL; w = w->priv->parent)
    if (!gtk_widget_get_mapped (w))
      return;

  WIDGET_CLASS (widget)->queue_draw_region (widget, region);
}

/**
 * gtk_widget_queue_draw_area:
 * @widget: a #GtkWidget
 * @x: x coordinate of upper-left corner of rectangle to redraw
 * @y: y coordinate of upper-left corner of rectangle to redraw
 * @width: width of region to draw
 * @height: height of region to draw
 *
 * Convenience function that calls gtk_widget_queue_draw_region() on
 * the region created from the given coordinates.
 *
 * The region here is specified in widget coordinates.
 * Widget coordinates are a bit odd; for historical reasons, they are
 * defined as @widget->window coordinates for widgets that return %TRUE for
 * gtk_widget_get_has_window(), and are relative to @widget->allocation.x,
 * @widget->allocation.y otherwise.
 *
 * @width or @height may be 0, in this case this function does
 * nothing. Negative values for @width and @height are not allowed.
 */
void
gtk_widget_queue_draw_area (GtkWidget *widget,
			    gint       x,
			    gint       y,
			    gint       width,
			    gint       height)
{
  GdkRectangle rect;
  cairo_region_t *region;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  if (width == 0 || height == 0)
    return;

  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  region = cairo_region_create_rectangle (&rect);
  gtk_widget_queue_draw_region (widget, region);
  cairo_region_destroy (region);
}

/**
 * gtk_widget_queue_draw:
 * @widget: a #GtkWidget
 *
 * Equivalent to calling gtk_widget_queue_draw_area() for the
 * entire area of a widget.
 **/
void
gtk_widget_queue_draw (GtkWidget *widget)
{
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_get_clip (widget, &rect);

  if (!gtk_widget_get_has_window (widget))
    gtk_widget_queue_draw_area (widget,
                                rect.x, rect.y, rect.width, rect.height);
  else
    gtk_widget_queue_draw_area (widget,
                                0, 0, rect.width, rect.height);
}

/**
 * gtk_widget_queue_resize:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 * Flags a widget to have its size renegotiated; should
 * be called when a widget for some reason has a new size request.
 * For example, when you change the text in a #GtkLabel, #GtkLabel
 * queues a resize to ensure there’s enough space for the new text.
 *
 * Note that you cannot call gtk_widget_queue_resize() on a widget
 * from inside its implementation of the GtkWidgetClass::size_allocate 
 * virtual method. Calls to gtk_widget_queue_resize() from inside
 * GtkWidgetClass::size_allocate will be silently ignored.
 **/
void
gtk_widget_queue_resize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_realized (widget))
    gtk_widget_queue_draw (widget);

  _gtk_size_group_queue_resize (widget, 0);
}

/**
 * gtk_widget_queue_resize_no_redraw:
 * @widget: a #GtkWidget
 *
 * This function works like gtk_widget_queue_resize(),
 * except that the widget is not invalidated.
 *
 * Since: 2.4
 **/
void
gtk_widget_queue_resize_no_redraw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  _gtk_size_group_queue_resize (widget, 0);
}

/**
 * gtk_widget_get_frame_clock:
 * @widget: a #GtkWidget
 *
 * Obtains the frame clock for a widget. The frame clock is a global
 * “ticker” that can be used to drive animations and repaints.  The
 * most common reason to get the frame clock is to call
 * gdk_frame_clock_get_frame_time(), in order to get a time to use for
 * animating. For example you might record the start of the animation
 * with an initial value from gdk_frame_clock_get_frame_time(), and
 * then update the animation by calling
 * gdk_frame_clock_get_frame_time() again during each repaint.
 *
 * gdk_frame_clock_request_phase() will result in a new frame on the
 * clock, but won’t necessarily repaint any widgets. To repaint a
 * widget, you have to use gtk_widget_queue_draw() which invalidates
 * the widget (thus scheduling it to receive a draw on the next
 * frame). gtk_widget_queue_draw() will also end up requesting a frame
 * on the appropriate frame clock.
 *
 * A widget’s frame clock will not change while the widget is
 * mapped. Reparenting a widget (which implies a temporary unmap) can
 * change the widget’s frame clock.
 *
 * Unrealized widgets do not have a frame clock.
 *
 * Returns: (transfer none): a #GdkFrameClock (or #NULL if widget is unrealized)
 *
 * Since: 3.8
 */
GdkFrameClock*
gtk_widget_get_frame_clock (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (widget->priv->realized)
    {
      /* We use gtk_widget_get_toplevel() here to make it explicit that
       * the frame clock is a property of the toplevel that a widget
       * is anchored to; gdk_window_get_toplevel() will go up the
       * hierarchy anyways, but should squash any funny business with
       * reparenting windows and widgets.
       */
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
      GdkWindow *window = gtk_widget_get_window (toplevel);
      g_assert (window != NULL);

      return gdk_window_get_frame_clock (window);
    }
  else
    {
      return NULL;
    }
}

/**
 * gtk_widget_size_request:
 * @widget: a #GtkWidget
 * @requisition: (out): a #GtkRequisition to be filled in
 *
 * This function is typically used when implementing a #GtkContainer
 * subclass.  Obtains the preferred size of a widget. The container
 * uses this information to arrange its child widgets and decide what
 * size allocations to give them with gtk_widget_size_allocate().
 *
 * You can also call this function from an application, with some
 * caveats. Most notably, getting a size request requires the widget
 * to be associated with a screen, because font information may be
 * needed. Multihead-aware applications should keep this in mind.
 *
 * Also remember that the size request is not necessarily the size
 * a widget will actually be allocated.
 *
 * Deprecated: 3.0: Use gtk_widget_get_preferred_size() instead.
 **/
void
gtk_widget_size_request (GtkWidget	*widget,
			 GtkRequisition *requisition)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_get_preferred_size (widget, requisition, NULL);
}

/**
 * gtk_widget_get_child_requisition:
 * @widget: a #GtkWidget
 * @requisition: (out): a #GtkRequisition to be filled in
 *
 * This function is only for use in widget implementations. Obtains
 * @widget->requisition, unless someone has forced a particular
 * geometry on the widget (e.g. with gtk_widget_set_size_request()),
 * in which case it returns that geometry instead of the widget's
 * requisition.
 *
 * This function differs from gtk_widget_size_request() in that
 * it retrieves the last size request value from @widget->requisition,
 * while gtk_widget_size_request() actually calls the "size_request" method
 * on @widget to compute the size request and fill in @widget->requisition,
 * and only then returns @widget->requisition.
 *
 * Because this function does not call the “size_request” method, it
 * can only be used when you know that @widget->requisition is
 * up-to-date, that is, gtk_widget_size_request() has been called
 * since the last time a resize was queued. In general, only container
 * implementations have this information; applications should use
 * gtk_widget_size_request().
 *
 *
 * Deprecated: 3.0: Use gtk_widget_get_preferred_size() instead.
 **/
void
gtk_widget_get_child_requisition (GtkWidget	 *widget,
				  GtkRequisition *requisition)
{
  gtk_widget_get_preferred_size (widget, requisition, NULL);
}

static gboolean
invalidate_predicate (GdkWindow *window,
		      gpointer   data)
{
  gpointer user_data;

  gdk_window_get_user_data (window, &user_data);

  return (user_data == data);
}

/* Invalidate @region in widget->window and all children
 * of widget->window owned by widget. @region is in the
 * same coordinates as widget->allocation and will be
 * modified by this call.
 */
static void
gtk_widget_invalidate_widget_windows (GtkWidget *widget,
				      cairo_region_t *region)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (!gtk_widget_get_realized (widget))
    return;

  if (gtk_widget_get_has_window (widget) && priv->parent)
    {
      int x, y;

      gdk_window_get_position (priv->window, &x, &y);
      cairo_region_translate (region, -x, -y);
    }

  gdk_window_invalidate_maybe_recurse (priv->window, region,
				       invalidate_predicate, widget);
}

/**
 * gtk_widget_size_allocate_with_baseline:
 * @widget: a #GtkWidget
 * @allocation: position and size to be allocated to @widget
 * @baseline: The baseline of the child, or -1
 *
 * This function is only used by #GtkContainer subclasses, to assign a size,
 * position and (optionally) baseline to their child widgets.
 *
 * In this function, the allocation and baseline may be adjusted. It
 * will be forced to a 1x1 minimum size, and the
 * adjust_size_allocation virtual and adjust_baseline_allocation
 * methods on the child will be used to adjust the allocation and
 * baseline. Standard adjustments include removing the widget's
 * margins, and applying the widget’s #GtkWidget:halign and
 * #GtkWidget:valign properties.
 *
 * If the child widget does not have a valign of %GTK_ALIGN_BASELINE the
 * baseline argument is ignored and -1 is used instead.
 *
 * Since: 3.10
 **/
void
gtk_widget_size_allocate_with_baseline (GtkWidget     *widget,
					GtkAllocation *allocation,
					gint           baseline)
{
  GtkWidgetPrivate *priv;
  GdkRectangle real_allocation;
  GdkRectangle old_allocation, old_clip;
  GdkRectangle adjusted_allocation;
  gboolean alloc_needed;
  gboolean size_changed;
  gboolean baseline_changed;
  gboolean position_changed;
  gint natural_width, natural_height, dummy;
  gint min_width, min_height;
  gint old_baseline;

  priv = widget->priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!priv->visible && !gtk_widget_is_toplevel (widget))
    return;

  gtk_widget_push_verify_invariants (widget);

#ifdef G_ENABLE_DEBUG
  if (gtk_get_debug_flags () & GTK_DEBUG_GEOMETRY)
    {
      gint depth;
      GtkWidget *parent;
      const gchar *name;

      depth = 0;
      parent = widget;
      while (parent)
	{
	  depth++;
	  parent = gtk_widget_get_parent (parent);
	}

      name = g_type_name (G_OBJECT_TYPE (G_OBJECT (widget)));
      g_print ("gtk_widget_size_allocate: %*s%s %d %d %d %d",
	       2 * depth, " ", name,
	       allocation->x, allocation->y,
	       allocation->width, allocation->height);
      if (baseline != -1)
	g_print (" baseline: %d", baseline);
      g_print ("\n");
    }
#endif /* G_ENABLE_DEBUG */

  /* Never pass a baseline to a child unless it requested it.
     This means containers don't have to manually check for this. */
  if (baseline != -1 &&
      (gtk_widget_get_valign_with_baseline (widget) != GTK_ALIGN_BASELINE ||
       !_gtk_widget_has_baseline_support (widget)))
    baseline = -1;

  alloc_needed = priv->alloc_needed;
  /* Preserve request/allocate ordering */
  priv->alloc_needed = FALSE;

  old_allocation = priv->allocation;
  old_clip = priv->clip;
  old_baseline = priv->allocated_baseline;
  real_allocation = *allocation;

  adjusted_allocation = real_allocation;
  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      /* Go ahead and request the height for allocated width, note that the internals
       * of get_height_for_width will internally limit the for_size to natural size
       * when aligning implicitly.
       */
      gtk_widget_get_preferred_width (widget, &min_width, &natural_width);
      gtk_widget_get_preferred_height_for_width (widget, real_allocation.width, &min_height, &natural_height);
    }
  else
    {
      /* Go ahead and request the width for allocated height, note that the internals
       * of get_width_for_height will internally limit the for_size to natural size
       * when aligning implicitly.
       */
      gtk_widget_get_preferred_height (widget, &min_height, &natural_height);
      gtk_widget_get_preferred_width_for_height (widget, real_allocation.height, &min_width, &natural_width);
    }

#ifdef G_ENABLE_DEBUG
  if (gtk_get_debug_flags () & GTK_DEBUG_GEOMETRY)
    {
      if ((min_width > real_allocation.width || min_height > real_allocation.height) &&
          !GTK_IS_SCROLLABLE (widget))
        g_warning ("gtk_widget_size_allocate(): attempt to underallocate %s%s %s %p. "
                   "Allocation is %dx%d, but minimum required size is %dx%d.",
                   priv->parent ? G_OBJECT_TYPE_NAME (priv->parent) : "", priv->parent ? "'s child" : "toplevel",
                   G_OBJECT_TYPE_NAME (widget), widget,
                   real_allocation.width, real_allocation.height,
                   min_width, min_height);
    }
#endif
  /* Now that we have the right natural height and width, go ahead and remove any margins from the
   * allocated sizes and possibly limit them to the natural sizes */
  GTK_WIDGET_GET_CLASS (widget)->adjust_size_allocation (widget,
							 GTK_ORIENTATION_HORIZONTAL,
							 &dummy,
							 &natural_width,
							 &adjusted_allocation.x,
							 &adjusted_allocation.width);
  GTK_WIDGET_GET_CLASS (widget)->adjust_size_allocation (widget,
							 GTK_ORIENTATION_VERTICAL,
							 &dummy,
							 &natural_height,
							 &adjusted_allocation.y,
							 &adjusted_allocation.height);
  if (baseline >= 0)
    GTK_WIDGET_GET_CLASS (widget)->adjust_baseline_allocation (widget,
							       &baseline);

  if (adjusted_allocation.x < real_allocation.x ||
      adjusted_allocation.y < real_allocation.y ||
      (adjusted_allocation.x + adjusted_allocation.width) >
      (real_allocation.x + real_allocation.width) ||
      (adjusted_allocation.y + adjusted_allocation.height >
       real_allocation.y + real_allocation.height))
    {
      g_warning ("%s %p attempted to adjust its size allocation from %d,%d %dx%d to %d,%d %dx%d. adjust_size_allocation must keep allocation inside original bounds",
                 G_OBJECT_TYPE_NAME (widget), widget,
                 real_allocation.x, real_allocation.y, real_allocation.width, real_allocation.height,
                 adjusted_allocation.x, adjusted_allocation.y, adjusted_allocation.width, adjusted_allocation.height);
      adjusted_allocation = real_allocation; /* veto it */
    }
  else
    {
      real_allocation = adjusted_allocation;
    }

  if (real_allocation.width < 0 || real_allocation.height < 0)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate widget with width %d and height %d",
		 real_allocation.width,
		 real_allocation.height);
    }

  real_allocation.width = MAX (real_allocation.width, 1);
  real_allocation.height = MAX (real_allocation.height, 1);

  baseline_changed = old_baseline != baseline;
  size_changed = (old_allocation.width != real_allocation.width ||
		  old_allocation.height != real_allocation.height);
  position_changed = (old_allocation.x != real_allocation.x ||
		      old_allocation.y != real_allocation.y);

  if (!alloc_needed && !size_changed && !position_changed && !baseline_changed)
    goto out;

  priv->allocated_baseline = baseline;
  g_signal_emit (widget, widget_signals[SIZE_ALLOCATE], 0, &real_allocation);

  /* Size allocation is god... after consulting god, no further requests or allocations are needed */
  priv->alloc_needed = FALSE;

  size_changed |= (old_clip.width != priv->clip.width ||
                   old_clip.height != priv->clip.height);
  position_changed |= (old_clip.x != priv->clip.x ||
                      old_clip.y != priv->clip.y);

  if (gtk_widget_get_mapped (widget) && priv->redraw_on_alloc)
    {
      if (!gtk_widget_get_has_window (widget) && position_changed)
	{
	  /* Invalidate union(old_clip,priv->clip) in priv->window
	   */
	  cairo_region_t *invalidate = cairo_region_create_rectangle (&priv->clip);
	  cairo_region_union_rectangle (invalidate, &old_clip);

	  gdk_window_invalidate_region (priv->window, invalidate, FALSE);
	  cairo_region_destroy (invalidate);
	}

      if (size_changed || baseline_changed)
	{
          /* Invalidate union(old_clip,priv->clip) in priv->window and descendents owned by widget
           */
          cairo_region_t *invalidate = cairo_region_create_rectangle (&priv->clip);
          cairo_region_union_rectangle (invalidate, &old_clip);

          gtk_widget_invalidate_widget_windows (widget, invalidate);
          cairo_region_destroy (invalidate);
	}
    }

  if ((size_changed || position_changed || baseline_changed) && priv->parent &&
      gtk_widget_get_realized (priv->parent) && _gtk_container_get_reallocate_redraws (GTK_CONTAINER (priv->parent)))
    {
      cairo_region_t *invalidate = cairo_region_create_rectangle (&priv->parent->priv->clip);
      gtk_widget_invalidate_widget_windows (priv->parent, invalidate);
      cairo_region_destroy (invalidate);
    }

out:
  gtk_widget_pop_verify_invariants (widget);
}


/**
 * gtk_widget_size_allocate:
 * @widget: a #GtkWidget
 * @allocation: position and size to be allocated to @widget
 *
 * This function is only used by #GtkContainer subclasses, to assign a size
 * and position to their child widgets.
 *
 * In this function, the allocation may be adjusted. It will be forced
 * to a 1x1 minimum size, and the adjust_size_allocation virtual
 * method on the child will be used to adjust the allocation. Standard
 * adjustments include removing the widget’s margins, and applying the
 * widget’s #GtkWidget:halign and #GtkWidget:valign properties.
 *
 * For baseline support in containers you need to use gtk_widget_size_allocate_with_baseline()
 * instead.
 **/
void
gtk_widget_size_allocate (GtkWidget	*widget,
			  GtkAllocation *allocation)
{
  gtk_widget_size_allocate_with_baseline (widget, allocation, -1);
}

/**
 * gtk_widget_common_ancestor:
 * @widget_a: a #GtkWidget
 * @widget_b: a #GtkWidget
 *
 * Find the common ancestor of @widget_a and @widget_b that
 * is closest to the two widgets.
 *
 * Returns: the closest common ancestor of @widget_a and
 *   @widget_b or %NULL if @widget_a and @widget_b do not
 *   share a common ancestor.
 **/
static GtkWidget *
gtk_widget_common_ancestor (GtkWidget *widget_a,
			    GtkWidget *widget_b)
{
  GtkWidget *parent_a;
  GtkWidget *parent_b;
  gint depth_a = 0;
  gint depth_b = 0;

  parent_a = widget_a;
  while (parent_a->priv->parent)
    {
      parent_a = parent_a->priv->parent;
      depth_a++;
    }

  parent_b = widget_b;
  while (parent_b->priv->parent)
    {
      parent_b = parent_b->priv->parent;
      depth_b++;
    }

  if (parent_a != parent_b)
    return NULL;

  while (depth_a > depth_b)
    {
      widget_a = widget_a->priv->parent;
      depth_a--;
    }

  while (depth_b > depth_a)
    {
      widget_b = widget_b->priv->parent;
      depth_b--;
    }

  while (widget_a != widget_b)
    {
      widget_a = widget_a->priv->parent;
      widget_b = widget_b->priv->parent;
    }

  return widget_a;
}

/**
 * gtk_widget_translate_coordinates:
 * @src_widget:  a #GtkWidget
 * @dest_widget: a #GtkWidget
 * @src_x: X position relative to @src_widget
 * @src_y: Y position relative to @src_widget
 * @dest_x: (out): location to store X position relative to @dest_widget
 * @dest_y: (out): location to store Y position relative to @dest_widget
 *
 * Translate coordinates relative to @src_widget’s allocation to coordinates
 * relative to @dest_widget’s allocations. In order to perform this
 * operation, both widgets must be realized, and must share a common
 * toplevel.
 *
 * Returns: %FALSE if either widget was not realized, or there
 *   was no common ancestor. In this case, nothing is stored in
 *   *@dest_x and *@dest_y. Otherwise %TRUE.
 **/
gboolean
gtk_widget_translate_coordinates (GtkWidget  *src_widget,
				  GtkWidget  *dest_widget,
				  gint        src_x,
				  gint        src_y,
				  gint       *dest_x,
				  gint       *dest_y)
{
  GtkWidgetPrivate *src_priv = src_widget->priv;
  GtkWidgetPrivate *dest_priv = dest_widget->priv;
  GtkWidget *ancestor;
  GdkWindow *window;
  GList *dest_list = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (src_widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (dest_widget), FALSE);

  ancestor = gtk_widget_common_ancestor (src_widget, dest_widget);
  if (!ancestor || !gtk_widget_get_realized (src_widget) || !gtk_widget_get_realized (dest_widget))
    return FALSE;

  /* Translate from allocation relative to window relative */
  if (gtk_widget_get_has_window (src_widget) && src_priv->parent)
    {
      gint wx, wy;
      gdk_window_get_position (src_priv->window, &wx, &wy);

      src_x -= wx - src_priv->allocation.x;
      src_y -= wy - src_priv->allocation.y;
    }
  else
    {
      src_x += src_priv->allocation.x;
      src_y += src_priv->allocation.y;
    }

  /* Translate to the common ancestor */
  window = src_priv->window;
  while (window != ancestor->priv->window)
    {
      gdouble dx, dy;

      gdk_window_coords_to_parent (window, src_x, src_y, &dx, &dy);

      src_x = dx;
      src_y = dy;

      window = gdk_window_get_effective_parent (window);

      if (!window)		/* Handle GtkHandleBox */
	return FALSE;
    }

  /* And back */
  window = dest_priv->window;
  while (window != ancestor->priv->window)
    {
      dest_list = g_list_prepend (dest_list, window);

      window = gdk_window_get_effective_parent (window);

      if (!window)		/* Handle GtkHandleBox */
        {
          g_list_free (dest_list);
          return FALSE;
        }
    }

  while (dest_list)
    {
      gdouble dx, dy;

      gdk_window_coords_from_parent (dest_list->data, src_x, src_y, &dx, &dy);

      src_x = dx;
      src_y = dy;

      dest_list = g_list_remove (dest_list, dest_list->data);
    }

  /* Translate from window relative to allocation relative */
  if (gtk_widget_get_has_window (dest_widget) && dest_priv->parent)
    {
      gint wx, wy;
      gdk_window_get_position (dest_priv->window, &wx, &wy);

      src_x += wx - dest_priv->allocation.x;
      src_y += wy - dest_priv->allocation.y;
    }
  else
    {
      src_x -= dest_priv->allocation.x;
      src_y -= dest_priv->allocation.y;
    }

  if (dest_x)
    *dest_x = src_x;
  if (dest_y)
    *dest_y = src_y;

  return TRUE;
}

static void
gtk_widget_real_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv = widget->priv;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
     {
	gdk_window_move_resize (priv->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);
     }
}

/* translate initial/final into start/end */
static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    default:
      return align;
    }
}

static void
adjust_for_align (GtkAlign  align,
                  gint     *natural_size,
                  gint     *allocated_pos,
                  gint     *allocated_size)
{
  switch (align)
    {
    case GTK_ALIGN_BASELINE:
    case GTK_ALIGN_FILL:
      /* change nothing */
      break;
    case GTK_ALIGN_START:
      /* keep *allocated_pos where it is */
      *allocated_size = MIN (*allocated_size, *natural_size);
      break;
    case GTK_ALIGN_END:
      if (*allocated_size > *natural_size)
	{
	  *allocated_pos += (*allocated_size - *natural_size);
	  *allocated_size = *natural_size;
	}
      break;
    case GTK_ALIGN_CENTER:
      if (*allocated_size > *natural_size)
	{
	  *allocated_pos += (*allocated_size - *natural_size) / 2;
	  *allocated_size = MIN (*allocated_size, *natural_size);
	}
      break;
    }
}

static void
adjust_for_margin(gint               start_margin,
                  gint               end_margin,
                  gint              *minimum_size,
                  gint              *natural_size,
                  gint              *allocated_pos,
                  gint              *allocated_size)
{
  *minimum_size -= (start_margin + end_margin);
  *natural_size -= (start_margin + end_margin);
  *allocated_pos += start_margin;
  *allocated_size -= (start_margin + end_margin);
}

static void
gtk_widget_real_adjust_size_allocation (GtkWidget         *widget,
                                        GtkOrientation     orientation,
                                        gint              *minimum_size,
                                        gint              *natural_size,
                                        gint              *allocated_pos,
                                        gint              *allocated_size)
{
  const GtkWidgetAuxInfo *aux_info;

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      adjust_for_margin (aux_info->margin.left,
                         aux_info->margin.right,
                         minimum_size, natural_size,
                         allocated_pos, allocated_size);
      adjust_for_align (effective_align (aux_info->halign, gtk_widget_get_direction (widget)),
                        natural_size, allocated_pos, allocated_size);
    }
  else
    {
      adjust_for_margin (aux_info->margin.top,
                         aux_info->margin.bottom,
                         minimum_size, natural_size,
                         allocated_pos, allocated_size);
      adjust_for_align (effective_align (aux_info->valign, GTK_TEXT_DIR_NONE),
                        natural_size, allocated_pos, allocated_size);
    }
}

static void
gtk_widget_real_adjust_baseline_allocation (GtkWidget         *widget,
					    gint              *baseline)
{
  const GtkWidgetAuxInfo *aux_info;

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (*baseline >= 0)
    *baseline -= aux_info->margin.top;
}

static gboolean
gtk_widget_real_can_activate_accel (GtkWidget *widget,
                                    guint      signal_id)
{
  GtkWidgetPrivate *priv = widget->priv;

  /* widgets must be onscreen for accels to take effect */
  return gtk_widget_is_sensitive (widget) &&
         gtk_widget_is_drawable (widget) &&
         gdk_window_is_viewable (priv->window);
}

/**
 * gtk_widget_can_activate_accel:
 * @widget: a #GtkWidget
 * @signal_id: the ID of a signal installed on @widget
 *
 * Determines whether an accelerator that activates the signal
 * identified by @signal_id can currently be activated.
 * This is done by emitting the #GtkWidget::can-activate-accel
 * signal on @widget; if the signal isn’t overridden by a
 * handler or in a derived widget, then the default check is
 * that the widget must be sensitive, and the widget and all
 * its ancestors mapped.
 *
 * Returns: %TRUE if the accelerator can be activated.
 *
 * Since: 2.4
 **/
gboolean
gtk_widget_can_activate_accel (GtkWidget *widget,
                               guint      signal_id)
{
  gboolean can_activate = FALSE;
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_signal_emit (widget, widget_signals[CAN_ACTIVATE_ACCEL], 0, signal_id, &can_activate);
  return can_activate;
}

typedef struct {
  GClosure   closure;
  guint      signal_id;
} AccelClosure;

static void
closure_accel_activate (GClosure     *closure,
			GValue       *return_value,
			guint         n_param_values,
			const GValue *param_values,
			gpointer      invocation_hint,
			gpointer      marshal_data)
{
  AccelClosure *aclosure = (AccelClosure*) closure;
  gboolean can_activate = gtk_widget_can_activate_accel (closure->data, aclosure->signal_id);

  if (can_activate)
    g_signal_emit (closure->data, aclosure->signal_id, 0);

  /* whether accelerator was handled */
  g_value_set_boolean (return_value, can_activate);
}

static void
closures_destroy (gpointer data)
{
  GSList *slist, *closures = data;

  for (slist = closures; slist; slist = slist->next)
    {
      g_closure_invalidate (slist->data);
      g_closure_unref (slist->data);
    }
  g_slist_free (closures);
}

static GClosure*
widget_new_accel_closure (GtkWidget *widget,
			  guint      signal_id)
{
  AccelClosure *aclosure;
  GClosure *closure = NULL;
  GSList *slist, *closures;

  closures = g_object_steal_qdata (G_OBJECT (widget), quark_accel_closures);
  for (slist = closures; slist; slist = slist->next)
    if (!gtk_accel_group_from_accel_closure (slist->data))
      {
	/* reuse this closure */
	closure = slist->data;
	break;
      }
  if (!closure)
    {
      closure = g_closure_new_object (sizeof (AccelClosure), G_OBJECT (widget));
      closures = g_slist_prepend (closures, g_closure_ref (closure));
      g_closure_sink (closure);
      g_closure_set_marshal (closure, closure_accel_activate);
    }
  g_object_set_qdata_full (G_OBJECT (widget), quark_accel_closures, closures, closures_destroy);

  aclosure = (AccelClosure*) closure;
  g_assert (closure->data == widget);
  g_assert (closure->marshal == closure_accel_activate);
  aclosure->signal_id = signal_id;

  return closure;
}

/**
 * gtk_widget_add_accelerator:
 * @widget:       widget to install an accelerator on
 * @accel_signal: widget signal to emit on accelerator activation
 * @accel_group:  accel group for this widget, added to its toplevel
 * @accel_key:    GDK keyval of the accelerator
 * @accel_mods:   modifier key combination of the accelerator
 * @accel_flags:  flag accelerators, e.g. %GTK_ACCEL_VISIBLE
 *
 * Installs an accelerator for this @widget in @accel_group that causes
 * @accel_signal to be emitted if the accelerator is activated.
 * The @accel_group needs to be added to the widget’s toplevel via
 * gtk_window_add_accel_group(), and the signal must be of type %G_SIGNAL_ACTION.
 * Accelerators added through this function are not user changeable during
 * runtime. If you want to support accelerators that can be changed by the
 * user, use gtk_accel_map_add_entry() and gtk_widget_set_accel_path() or
 * gtk_menu_item_set_accel_path() instead.
 */
void
gtk_widget_add_accelerator (GtkWidget      *widget,
			    const gchar    *accel_signal,
			    GtkAccelGroup  *accel_group,
			    guint           accel_key,
			    GdkModifierType accel_mods,
			    GtkAccelFlags   accel_flags)
{
  GClosure *closure;
  GSignalQuery query;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (accel_signal != NULL);
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  g_signal_query (g_signal_lookup (accel_signal, G_OBJECT_TYPE (widget)), &query);
  if (!query.signal_id ||
      !(query.signal_flags & G_SIGNAL_ACTION) ||
      query.return_type != G_TYPE_NONE ||
      query.n_params)
    {
      /* hmm, should be elaborate enough */
      g_warning (G_STRLOC ": widget `%s' has no activatable signal \"%s\" without arguments",
		 G_OBJECT_TYPE_NAME (widget), accel_signal);
      return;
    }

  closure = widget_new_accel_closure (widget, query.signal_id);

  g_object_ref (widget);

  /* install the accelerator. since we don't map this onto an accel_path,
   * the accelerator will automatically be locked.
   */
  gtk_accel_group_connect (accel_group,
			   accel_key,
			   accel_mods,
			   accel_flags | GTK_ACCEL_LOCKED,
			   closure);

  g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);

  g_object_unref (widget);
}

/**
 * gtk_widget_remove_accelerator:
 * @widget:       widget to install an accelerator on
 * @accel_group:  accel group for this widget
 * @accel_key:    GDK keyval of the accelerator
 * @accel_mods:   modifier key combination of the accelerator
 *
 * Removes an accelerator from @widget, previously installed with
 * gtk_widget_add_accelerator().
 *
 * Returns: whether an accelerator was installed and could be removed
 */
gboolean
gtk_widget_remove_accelerator (GtkWidget      *widget,
			       GtkAccelGroup  *accel_group,
			       guint           accel_key,
			       GdkModifierType accel_mods)
{
  GtkAccelGroupEntry *ag_entry;
  GList *slist, *clist;
  guint n;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), FALSE);

  ag_entry = gtk_accel_group_query (accel_group, accel_key, accel_mods, &n);
  clist = gtk_widget_list_accel_closures (widget);
  for (slist = clist; slist; slist = slist->next)
    {
      guint i;

      for (i = 0; i < n; i++)
	if (slist->data == (gpointer) ag_entry[i].closure)
	  {
	    gboolean is_removed = gtk_accel_group_disconnect (accel_group, slist->data);

	    g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);

	    g_list_free (clist);

	    return is_removed;
	  }
    }
  g_list_free (clist);

  g_warning (G_STRLOC ": no accelerator (%u,%u) installed in accel group (%p) for %s (%p)",
	     accel_key, accel_mods, accel_group,
	     G_OBJECT_TYPE_NAME (widget), widget);

  return FALSE;
}

/**
 * gtk_widget_list_accel_closures:
 * @widget:  widget to list accelerator closures for
 *
 * Lists the closures used by @widget for accelerator group connections
 * with gtk_accel_group_connect_by_path() or gtk_accel_group_connect().
 * The closures can be used to monitor accelerator changes on @widget,
 * by connecting to the @GtkAccelGroup::accel-changed signal of the
 * #GtkAccelGroup of a closure which can be found out with
 * gtk_accel_group_from_accel_closure().
 *
 * Returns: (transfer container) (element-type GClosure):
 *     a newly allocated #GList of closures
 */
GList*
gtk_widget_list_accel_closures (GtkWidget *widget)
{
  GSList *slist;
  GList *clist = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (slist = g_object_get_qdata (G_OBJECT (widget), quark_accel_closures); slist; slist = slist->next)
    if (gtk_accel_group_from_accel_closure (slist->data))
      clist = g_list_prepend (clist, slist->data);
  return clist;
}

typedef struct {
  GQuark         path_quark;
  GtkAccelGroup *accel_group;
  GClosure      *closure;
} AccelPath;

static void
destroy_accel_path (gpointer data)
{
  AccelPath *apath = data;

  gtk_accel_group_disconnect (apath->accel_group, apath->closure);

  /* closures_destroy takes care of unrefing the closure */
  g_object_unref (apath->accel_group);

  g_slice_free (AccelPath, apath);
}


/**
 * gtk_widget_set_accel_path:
 * @widget: a #GtkWidget
 * @accel_path: (allow-none): path used to look up the accelerator
 * @accel_group: (allow-none): a #GtkAccelGroup.
 *
 * Given an accelerator group, @accel_group, and an accelerator path,
 * @accel_path, sets up an accelerator in @accel_group so whenever the
 * key binding that is defined for @accel_path is pressed, @widget
 * will be activated.  This removes any accelerators (for any
 * accelerator group) installed by previous calls to
 * gtk_widget_set_accel_path(). Associating accelerators with
 * paths allows them to be modified by the user and the modifications
 * to be saved for future use. (See gtk_accel_map_save().)
 *
 * This function is a low level function that would most likely
 * be used by a menu creation system like #GtkUIManager. If you
 * use #GtkUIManager, setting up accelerator paths will be done
 * automatically.
 *
 * Even when you you aren’t using #GtkUIManager, if you only want to
 * set up accelerators on menu items gtk_menu_item_set_accel_path()
 * provides a somewhat more convenient interface.
 *
 * Note that @accel_path string will be stored in a #GQuark. Therefore, if you
 * pass a static string, you can save some memory by interning it first with
 * g_intern_static_string().
 **/
void
gtk_widget_set_accel_path (GtkWidget     *widget,
			   const gchar   *accel_path,
			   GtkAccelGroup *accel_group)
{
  AccelPath *apath;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_GET_CLASS (widget)->activate_signal != 0);

  if (accel_path)
    {
      g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
      g_return_if_fail (_gtk_accel_path_is_valid (accel_path));

      gtk_accel_map_add_entry (accel_path, 0, 0);
      apath = g_slice_new (AccelPath);
      apath->accel_group = g_object_ref (accel_group);
      apath->path_quark = g_quark_from_string (accel_path);
      apath->closure = widget_new_accel_closure (widget, GTK_WIDGET_GET_CLASS (widget)->activate_signal);
    }
  else
    apath = NULL;

  /* also removes possible old settings */
  g_object_set_qdata_full (G_OBJECT (widget), quark_accel_path, apath, destroy_accel_path);

  if (apath)
    gtk_accel_group_connect_by_path (apath->accel_group, g_quark_to_string (apath->path_quark), apath->closure);

  g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);
}

const gchar*
_gtk_widget_get_accel_path (GtkWidget *widget,
			    gboolean  *locked)
{
  AccelPath *apath;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  apath = g_object_get_qdata (G_OBJECT (widget), quark_accel_path);
  if (locked)
    *locked = apath ? gtk_accel_group_get_is_locked (apath->accel_group) : TRUE;
  return apath ? g_quark_to_string (apath->path_quark) : NULL;
}

/**
 * gtk_widget_mnemonic_activate:
 * @widget: a #GtkWidget
 * @group_cycling:  %TRUE if there are other widgets with the same mnemonic
 *
 * Emits the #GtkWidget::mnemonic-activate signal.
 *
 * The default handler for this signal activates the @widget if
 * @group_cycling is %FALSE, and just grabs the focus if @group_cycling
 * is %TRUE.
 *
 * Returns: %TRUE if the signal has been handled
 */
gboolean
gtk_widget_mnemonic_activate (GtkWidget *widget,
                              gboolean   group_cycling)
{
  gboolean handled;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  group_cycling = group_cycling != FALSE;
  if (!gtk_widget_is_sensitive (widget))
    handled = TRUE;
  else
    g_signal_emit (widget,
		   widget_signals[MNEMONIC_ACTIVATE],
		   0,
		   group_cycling,
		   &handled);
  return handled;
}

static gboolean
gtk_widget_real_mnemonic_activate (GtkWidget *widget,
                                   gboolean   group_cycling)
{
  if (!group_cycling && GTK_WIDGET_GET_CLASS (widget)->activate_signal)
    gtk_widget_activate (widget);
  else if (gtk_widget_get_can_focus (widget))
    gtk_widget_grab_focus (widget);
  else
    {
      g_warning ("widget `%s' isn't suitable for mnemonic activation",
		 G_OBJECT_TYPE_NAME (widget));
      gtk_widget_error_bell (widget);
    }
  return TRUE;
}

static const cairo_user_data_key_t event_window_key;

GdkWindow *
_gtk_cairo_get_event_window (cairo_t *cr)
{
  g_return_val_if_fail (cr != NULL, NULL);

  return cairo_get_user_data (cr, &event_window_key);
}

static void
gtk_cairo_set_event_window (cairo_t        *cr,
			    GdkWindow *event_window)
{
  cairo_set_user_data (cr, &event_window_key, event_window, NULL);
}

static const cairo_user_data_key_t event_key;

GdkEventExpose *
_gtk_cairo_get_event (cairo_t *cr)
{
  g_return_val_if_fail (cr != NULL, NULL);

  return cairo_get_user_data (cr, &event_key);
}

static void
gtk_cairo_set_event (cairo_t        *cr,
		     GdkEventExpose *event)
{
  cairo_set_user_data (cr, &event_key, event, NULL);
}

/**
 * gtk_cairo_should_draw_window:
 * @cr: a cairo context
 * @window: the window to check. @window may not be an input-only
 *          window.
 *
 * This function is supposed to be called in #GtkWidget::draw
 * implementations for widgets that support multiple windows.
 * @cr must be untransformed from invoking of the draw function.
 * This function will return %TRUE if the contents of the given
 * @window are supposed to be drawn and %FALSE otherwise. Note
 * that when the drawing was not initiated by the windowing
 * system this function will return %TRUE for all windows, so
 * you need to draw the bottommost window first. Also, do not
 * use “else if” statements to check which window should be drawn.
 *
 * Returns: %TRUE if @window should be drawn
 *
 * Since: 3.0
 **/
gboolean
gtk_cairo_should_draw_window (cairo_t *cr,
                              GdkWindow *window)
{
  GdkWindow *event_window;

  g_return_val_if_fail (cr != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  event_window = _gtk_cairo_get_event_window (cr);

  return event_window == NULL ||
    event_window == window;
}

static void
_gtk_widget_draw_internal (GtkWidget *widget,
                           cairo_t   *cr,
                           gboolean   clip_to_size,
			   GdkWindow *window)
{
  GdkWindow *tmp_event_window;

  if (!gtk_widget_is_drawable (widget))
    return;

  tmp_event_window = _gtk_cairo_get_event_window (cr);
  gtk_cairo_set_event_window (cr, window);

  if (clip_to_size)
    {
      cairo_rectangle (cr,
                       widget->priv->clip.x - widget->priv->allocation.x,
                       widget->priv->clip.y - widget->priv->allocation.y,
                       widget->priv->clip.width,
                       widget->priv->clip.height);
      cairo_clip (cr);
    }

  if (gdk_cairo_get_clip_rectangle (cr, NULL))
    {
      gboolean result;

      gdk_window_mark_paint_from_clip (window, cr);

      g_signal_emit (widget, widget_signals[DRAW],
                     0, cr,
                     &result);

#ifdef G_ENABLE_DEBUG
      if (G_UNLIKELY (gtk_get_debug_flags () & GTK_DEBUG_BASELINES))
	{
	  gint baseline = gtk_widget_get_allocated_baseline (widget);
	  gint width = gtk_widget_get_allocated_width (widget);

	  if (baseline != -1)
	    {
	      cairo_save (cr);
	      cairo_new_path (cr);
	      cairo_move_to (cr, 0, baseline+0.5);
	      cairo_line_to (cr, width, baseline+0.5);
	      cairo_set_line_width (cr, 1.0);
	      cairo_set_source_rgba (cr, 1.0, 0, 0, 0.25);
	      cairo_stroke (cr);
	      cairo_restore (cr);
	    }
	}
#endif

      if (cairo_status (cr) &&
          _gtk_cairo_get_event_window (cr))
        {
          /* We check the event so we only warn about internal GTK calls.
           * Errors might come from PDF streams having write failures and
           * we don't want to spam stderr in that case.
           * We do want to catch errors from
           */
          g_warning ("drawing failure for widget `%s': %s",
                     G_OBJECT_TYPE_NAME (widget),
                     cairo_status_to_string (cairo_status (cr)));
        }
    }

  gtk_cairo_set_event_window (cr, tmp_event_window);
}

/* Emit draw() on the widget that owns window,
   and on any child windows that also belong
   to the widget. */
static void
_gtk_widget_draw_windows (GdkWindow *window,
			  cairo_t *cr,
			  int window_x,
			  int window_y)
{
  cairo_pattern_t *pattern;
  gboolean do_clip;
  GtkWidget *widget = NULL;
  GList *children, *l;
  GdkRectangle current_clip, window_clip;
  int x, y;

  if (!gdk_window_is_viewable (window))
    return;

  window_clip.x = window_x;
  window_clip.y = window_y;
  window_clip.width = gdk_window_get_width (window);
  window_clip.height = gdk_window_get_height (window);

  /* Cairo paths are fixed point 24.8, but gdk supports 32bit window
     sizes, so we can't feed window_clip to e.g. cairo_rectangle()
     directly. Instead, we pre-clip the window clip to the existing
     clip regions in full 32bit precision and feed that to cairo. */
  if (!gdk_cairo_get_clip_rectangle (cr, &current_clip) ||
      !gdk_rectangle_intersect (&window_clip, &current_clip, &window_clip))
    return;

  cairo_save (cr);
  cairo_rectangle (cr,
                   window_clip.x, window_clip.y,
                   window_clip.width, window_clip.height);
  cairo_clip (cr);

  cairo_translate (cr, window_x, window_y);

  if (gdk_cairo_get_clip_rectangle (cr, NULL))
    {
      gdk_window_get_user_data (window, (gpointer *) &widget);

      /* Only clear bg if double bufferer. This is what we used
	 to do before, where begin_paint() did the clearing. */
      pattern = gdk_window_get_background_pattern (window);
      if (pattern != NULL &&
	  widget->priv->double_buffered)
	{
	  cairo_save (cr);
	  cairo_set_source (cr, pattern);
	  cairo_paint (cr);
	  cairo_restore (cr);
	}

      do_clip = _gtk_widget_get_translation_to_window (widget, window,
						       &x, &y);
      cairo_save (cr);
      cairo_translate (cr, -x, -y);
      _gtk_widget_draw_internal (widget, cr, do_clip, window);
      cairo_restore (cr);

      children = gdk_window_get_children_with_user_data (window, widget);
      for (l = children; l != NULL; l = l->next)
	{
	  GdkWindow *child_window = l->data;
	  GdkWindowType type;
	  int wx, wy;

	  if (!gdk_window_is_visible (child_window) ||
	      gdk_window_is_input_only (child_window))
	    continue;

	  type = gdk_window_get_window_type (child_window);
	  if (type == GDK_WINDOW_OFFSCREEN ||
	      type == GDK_WINDOW_FOREIGN)
	    continue;

	  gdk_window_get_position (child_window, &wx, &wy);
	  _gtk_widget_draw_windows (child_window, cr, wx, wy);
	}
      g_list_free (children);
    }

  cairo_restore (cr);
}

void
_gtk_widget_draw (GtkWidget *widget,
		  cairo_t   *cr)
{
  GdkWindow *window, *child_window;
  GList *children, *l;
  int wx, wy;
  gboolean push_group;
  GdkWindowType type;

  /* We get expose events only on native windows, so the draw
   * implementation has to walk the entire widget hierarchy, except
   * that it stops at native subwindows while we're in an expose
   * event (_gtk_cairo_get_event () != NULL).
   *
   * However, we need to properly clip drawing into child windows
   * to avoid drawing outside if widgets use e.g. cairo_paint(), so
   * we traverse over GdkWindows as well as GtkWidgets.
   *
   * In order to be able to have opacity groups for entire widgets
   * that consists of multiple windows we collect all the windows
   * that belongs to a widget and draw them in one go. This means
   * we may somewhat reorder GdkWindows when we paint them, but
   * thats not generally a problem, as if you want a guaranteed
   * order you generally use a windowed widget where you control
   * the window hierarchy.
   */

  cairo_save (cr);

  push_group =
    widget->priv->alpha != 255 &&
    (!gtk_widget_is_toplevel (widget) ||
     gtk_widget_get_visual (widget) == gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget)));

  if (push_group)
    cairo_push_group (cr);

  window = gtk_widget_get_window (widget);
  if (gtk_widget_get_has_window (widget))
    {
      /* The widget will be completely contained in its window, so just
       * expose that (and any child window belonging to the widget) */
      _gtk_widget_draw_windows (window, cr, 0, 0);
    }
  else
    {
      /* The widget draws in its parent window, so we send a draw() for
       * that. */
      _gtk_widget_draw_internal (widget, cr, TRUE, window);

      /* But, it may also have child windows in the parent which we should
       * draw (after having drawn on the parent) */
      children = gdk_window_get_children_with_user_data (window, widget);
      for (l = children; l != NULL; l = l->next)
	{
	  child_window = l->data;

	  if (!gdk_window_is_visible (child_window) ||
	      gdk_window_is_input_only (child_window))
	    continue;

	  type = gdk_window_get_window_type (child_window);
	  if (type == GDK_WINDOW_OFFSCREEN ||
	      type == GDK_WINDOW_FOREIGN)
	    continue;

	  gdk_window_get_position (child_window, &wx, &wy);
	  _gtk_widget_draw_windows (child_window, cr,
				    wx - widget->priv->allocation.x,
				    wy - widget->priv->allocation.y);
	}
      g_list_free (children);
    }

  if (push_group)
    {
      cairo_pop_group_to_source (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_paint_with_alpha (cr, widget->priv->alpha / 255.0);
    }

  cairo_restore (cr);
}


/**
 * gtk_widget_draw:
 * @widget: the widget to draw. It must be drawable (see
 *   gtk_widget_is_drawable()) and a size must have been allocated.
 * @cr: a cairo context to draw to
 *
 * Draws @widget to @cr. The top left corner of the widget will be
 * drawn to the currently set origin point of @cr.
 *
 * You should pass a cairo context as @cr argument that is in an
 * original state. Otherwise the resulting drawing is undefined. For
 * example changing the operator using cairo_set_operator() or the
 * line width using cairo_set_line_width() might have unwanted side
 * effects.
 * You may however change the context’s transform matrix - like with
 * cairo_scale(), cairo_translate() or cairo_set_matrix() and clip
 * region with cairo_clip() prior to calling this function. Also, it
 * is fine to modify the context with cairo_save() and
 * cairo_push_group() prior to calling this function.
 *
 * Note that special-purpose widgets may contain special code for
 * rendering to the screen and might appear differently on screen
 * and when rendered using gtk_widget_draw().
 *
 * Since: 3.0
 **/
void
gtk_widget_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!widget->priv->alloc_needed);
  g_return_if_fail (cr != NULL);

  _gtk_widget_draw (widget, cr);
}

static gboolean
gtk_widget_real_button_event (GtkWidget      *widget,
                              GdkEventButton *event)
{
  return _gtk_widget_run_controllers (widget, (GdkEvent *) event,
                                      GTK_PHASE_BUBBLE);
}

static gboolean
gtk_widget_real_motion_event (GtkWidget      *widget,
                              GdkEventMotion *event)
{
  return _gtk_widget_run_controllers (widget, (GdkEvent *) event,
                                      GTK_PHASE_BUBBLE);
}

static gboolean
gtk_widget_real_key_press_event (GtkWidget         *widget,
				 GdkEventKey       *event)
{
  return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

static gboolean
gtk_widget_real_key_release_event (GtkWidget         *widget,
				   GdkEventKey       *event)
{
  return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

static gboolean
gtk_widget_real_focus_in_event (GtkWidget     *widget,
                                GdkEventFocus *event)
{
  gtk_widget_queue_draw (widget);

  return FALSE;
}

static gboolean
gtk_widget_real_focus_out_event (GtkWidget     *widget,
                                 GdkEventFocus *event)
{
  gtk_widget_queue_draw (widget);

  return FALSE;
}

static gboolean
gtk_widget_real_touch_event (GtkWidget     *widget,
                             GdkEventTouch *event)
{
  GdkEvent *bevent;
  gboolean return_val;
  gint signum;

  if (!event->emulating_pointer)
    return _gtk_widget_run_controllers (widget, (GdkEvent*) event,
                                        GTK_PHASE_BUBBLE);

  if (event->type == GDK_TOUCH_BEGIN ||
      event->type == GDK_TOUCH_END)
    {
      GdkEventType type;

      if (event->type == GDK_TOUCH_BEGIN)
        {
          type = GDK_BUTTON_PRESS;
          signum = BUTTON_PRESS_EVENT;
        }
      else
        {
          type = GDK_BUTTON_RELEASE;
          signum = BUTTON_RELEASE_EVENT;
        }
      bevent = gdk_event_new (type);
      bevent->any.window = g_object_ref (event->window);
      bevent->any.send_event = FALSE;
      bevent->button.time = event->time;
      bevent->button.state = event->state;
      bevent->button.button = 1;
      bevent->button.x_root = event->x_root;
      bevent->button.y_root = event->y_root;
      bevent->button.x = event->x;
      bevent->button.y = event->y;
      bevent->button.device = event->device;
      bevent->button.axes = g_memdup (event->axes,
                                      sizeof (gdouble) * gdk_device_get_n_axes (event->device));
      gdk_event_set_source_device (bevent, gdk_event_get_source_device ((GdkEvent*)event));

      if (event->type == GDK_TOUCH_END)
        bevent->button.state |= GDK_BUTTON1_MASK;
    }
  else if (event->type == GDK_TOUCH_UPDATE)
    {
      signum = MOTION_NOTIFY_EVENT;
      bevent = gdk_event_new (GDK_MOTION_NOTIFY);
      bevent->any.window = g_object_ref (event->window);
      bevent->any.send_event = FALSE;
      bevent->motion.time = event->time;
      bevent->motion.state = event->state | GDK_BUTTON1_MASK;
      bevent->motion.x_root = event->x_root;
      bevent->motion.y_root = event->y_root;
      bevent->motion.x = event->x;
      bevent->motion.y = event->y;
      bevent->motion.device = event->device;
      bevent->motion.is_hint = FALSE;
      bevent->motion.axes = g_memdup (event->axes,
                                      sizeof (gdouble) * gdk_device_get_n_axes (event->device));
      gdk_event_set_source_device (bevent, gdk_event_get_source_device ((GdkEvent*)event));
    }
  else
    return FALSE;

  g_signal_emit (widget, widget_signals[signum], 0, bevent, &return_val);

  gdk_event_free (bevent);

  return return_val;
}

static gboolean
gtk_widget_real_grab_broken_event (GtkWidget          *widget,
                                   GdkEventGrabBroken *event)
{
  return _gtk_widget_run_controllers (widget, (GdkEvent*) event,
                                      GTK_PHASE_BUBBLE);
}

#define WIDGET_REALIZED_FOR_EVENT(widget, event) \
     (event->type == GDK_FOCUS_CHANGE || gtk_widget_get_realized(widget))

/**
 * gtk_widget_event:
 * @widget: a #GtkWidget
 * @event: a #GdkEvent
 *
 * Rarely-used function. This function is used to emit
 * the event signals on a widget (those signals should never
 * be emitted without using this function to do so).
 * If you want to synthesize an event though, don’t use this function;
 * instead, use gtk_main_do_event() so the event will behave as if
 * it were in the event queue. Don’t synthesize expose events; instead,
 * use gdk_window_invalidate_rect() to invalidate a region of the
 * window.
 *
 * Returns: return from the event signal emission (%TRUE if
 *               the event was handled)
 **/
gboolean
gtk_widget_event (GtkWidget *widget,
		  GdkEvent  *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  if (event->type == GDK_EXPOSE)
    {
      g_warning ("Events of type GDK_EXPOSE cannot be synthesized. To get "
		 "the same effect, call gdk_window_invalidate_rect/region(), "
		 "followed by gdk_window_process_updates().");
      return TRUE;
    }

  return gtk_widget_event_internal (widget, event);
}

void
_gtk_widget_set_captured_event_handler (GtkWidget               *widget,
                                        GtkCapturedEventHandler  callback)
{
  g_object_set_data (G_OBJECT (widget), "captured-event-handler", callback);
}

static GdkEventMask
_gtk_widget_get_controllers_evmask (GtkWidget *widget)
{
  EventControllerData *data;
  GdkEventMask evmask = 0;
  GtkWidgetPrivate *priv;
  GList *l;

  priv = widget->priv;

  for (l = priv->event_controllers; l; l = l->next)
    {
      data = l->data;
      if (data->controller)
        evmask |= gtk_event_controller_get_event_mask (GTK_EVENT_CONTROLLER (data->controller));
    }

  return evmask;
}

static gboolean
_gtk_widget_run_controllers (GtkWidget           *widget,
                             const GdkEvent      *event,
                             GtkPropagationPhase  phase)
{
  EventControllerData *data;
  gboolean handled = FALSE;
  GtkWidgetPrivate *priv;
  GList *l;

  priv = widget->priv;
  g_object_ref (widget);

  l = priv->event_controllers;
  while (l != NULL)
    {
      GList *next = l->next;

      data = l->data;

      if (data->controller == NULL)
        {
          priv->event_controllers = g_list_delete_link (priv->event_controllers, l);
          g_free (data);
        }
      else
        {
          GtkPropagationPhase controller_phase;

          controller_phase = gtk_event_controller_get_propagation_phase (data->controller);

          if (controller_phase == phase)
            handled |= gtk_event_controller_handle_event (data->controller, event);
        }

      l = next;
    }

  g_object_unref (widget);

  return handled;
}

gboolean
_gtk_widget_captured_event (GtkWidget *widget,
                            GdkEvent  *event)
{
  gboolean return_val = FALSE;
  GtkCapturedEventHandler handler;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  if (event->type == GDK_EXPOSE)
    {
      g_warning ("Events of type GDK_EXPOSE cannot be synthesized. To get "
		 "the same effect, call gdk_window_invalidate_rect/region(), "
		 "followed by gdk_window_process_updates().");
      return TRUE;
    }

  if (!event_window_is_still_viewable (event))
    return TRUE;

  return_val = _gtk_widget_run_controllers (widget, event, GTK_PHASE_CAPTURE);

  handler = g_object_get_data (G_OBJECT (widget), "captured-event-handler");
  if (!handler)
    return return_val;

  g_object_ref (widget);

  return_val |= handler (widget, event);
  return_val |= !WIDGET_REALIZED_FOR_EVENT (widget, event);

  /* The widget that was originally to receive the event
   * handles motion hints, but the capturing widget might
   * not, so ensure we get further motion events.
   */
  if (return_val &&
      event->type == GDK_MOTION_NOTIFY &&
      event->motion.is_hint &&
      (gdk_window_get_events (event->any.window) &
       GDK_POINTER_MOTION_HINT_MASK) != 0)
    gdk_event_request_motions (&event->motion);

  g_object_unref (widget);

  return return_val;
}

/* Returns TRUE if a translation should be done */
gboolean
_gtk_widget_get_translation_to_window (GtkWidget      *widget,
				       GdkWindow      *window,
				       int            *x,
				       int            *y)
{
  GdkWindow *w, *widget_window;

  if (!gtk_widget_get_has_window (widget))
    {
      *x = -widget->priv->allocation.x;
      *y = -widget->priv->allocation.y;
    }
  else
    {
      *x = 0;
      *y = 0;
    }

  widget_window = gtk_widget_get_window (widget);

  for (w = window; w && w != widget_window; w = gdk_window_get_parent (w))
    {
      int wx, wy;
      gdk_window_get_position (w, &wx, &wy);
      *x += wx;
      *y += wy;
    }

  if (w == NULL)
    {
      *x = 0;
      *y = 0;
      return FALSE;
    }

  return TRUE;
}


/**
 * gtk_cairo_transform_to_window:
 * @cr: the cairo context to transform
 * @widget: the widget the context is currently centered for
 * @window: the window to transform the context to
 *
 * Transforms the given cairo context @cr that from @widget-relative
 * coordinates to @window-relative coordinates.
 * If the @widget’s window is not an ancestor of @window, no
 * modification will be applied.
 *
 * This is the inverse to the transformation GTK applies when
 * preparing an expose event to be emitted with the #GtkWidget::draw
 * signal. It is intended to help porting multiwindow widgets from
 * GTK+ 2 to the rendering architecture of GTK+ 3.
 *
 * Since: 3.0
 **/
void
gtk_cairo_transform_to_window (cairo_t   *cr,
                               GtkWidget *widget,
                               GdkWindow *window)
{
  int x, y;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (_gtk_widget_get_translation_to_window (widget, window, &x, &y))
    cairo_translate (cr, x, y);
}

/**
 * gtk_widget_send_expose:
 * @widget: a #GtkWidget
 * @event: a expose #GdkEvent
 *
 * Very rarely-used function. This function is used to emit
 * an expose event on a widget. This function is not normally used
 * directly. The only time it is used is when propagating an expose
 * event to a windowless child widget (gtk_widget_get_has_window() is %FALSE),
 * and that is normally done using gtk_container_propagate_draw().
 *
 * If you want to force an area of a window to be redrawn,
 * use gdk_window_invalidate_rect() or gdk_window_invalidate_region().
 * To cause the redraw to be done immediately, follow that call
 * with a call to gdk_window_process_updates().
 *
 * Returns: return from the event signal emission (%TRUE if
 *               the event was handled)
 **/
gint
gtk_widget_send_expose (GtkWidget *widget,
			GdkEvent  *event)
{
  gboolean result = FALSE;
  cairo_t *cr;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (gtk_widget_get_realized (widget), TRUE);
  g_return_val_if_fail (event != NULL, TRUE);
  g_return_val_if_fail (event->type == GDK_EXPOSE, TRUE);

  cr = gdk_cairo_create (event->expose.window);
  gdk_cairo_region (cr, event->expose.region);
  cairo_clip (cr);

  gtk_cairo_set_event (cr, &event->expose);

  if (event->expose.window == widget->priv->window)
    _gtk_widget_draw (widget, cr);
  else
    _gtk_widget_draw_windows (event->expose.window, cr, 0, 0);

  gtk_cairo_set_event (cr, NULL);

  cairo_destroy (cr);

  return result;
}

static gboolean
event_window_is_still_viewable (GdkEvent *event)
{
  /* Check that we think the event's window is viewable before
   * delivering the event, to prevent suprises. We do this here
   * at the last moment, since the event may have been queued
   * up behind other events, held over a recursive main loop, etc.
   */
  switch (event->type)
    {
    case GDK_EXPOSE:
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_KEY_PRESS:
    case GDK_ENTER_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_SCROLL:
      return event->any.window && gdk_window_is_viewable (event->any.window);

#if 0
    /* The following events are the second half of paired events;
     * we always deliver them to deal with widgets that clean up
     * on the second half.
     */
    case GDK_BUTTON_RELEASE:
    case GDK_KEY_RELEASE:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROXIMITY_OUT:
#endif

    default:
      /* Remaining events would make sense on an not-viewable window,
       * or don't have an associated window.
       */
      return TRUE;
    }
}

static gint
gtk_widget_event_internal (GtkWidget *widget,
			   GdkEvent  *event)
{
  gboolean return_val = FALSE, handled;

  /* We check only once for is-still-visible; if someone
   * hides the window in on of the signals on the widget,
   * they are responsible for returning TRUE to terminate
   * handling.
   */
  if (!event_window_is_still_viewable (event))
    return TRUE;

  g_object_ref (widget);

  if (widget == gtk_get_event_widget (event))
    return_val |= _gtk_widget_run_controllers (widget, event, GTK_PHASE_TARGET);

  g_signal_emit (widget, widget_signals[EVENT], 0, event, &handled);
  return_val |= handled | !WIDGET_REALIZED_FOR_EVENT (widget, event);
  if (!return_val)
    {
      gint signal_num;

      switch (event->type)
	{
	case GDK_EXPOSE:
	case GDK_NOTHING:
	  signal_num = -1;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	  signal_num = BUTTON_PRESS_EVENT;
          break;
        case GDK_TOUCH_BEGIN:
        case GDK_TOUCH_UPDATE:
        case GDK_TOUCH_END:
        case GDK_TOUCH_CANCEL:
	  signal_num = TOUCH_EVENT;
	  break;
	case GDK_SCROLL:
	  signal_num = SCROLL_EVENT;
	  break;
	case GDK_BUTTON_RELEASE:
	  signal_num = BUTTON_RELEASE_EVENT;
	  break;
	case GDK_MOTION_NOTIFY:
	  signal_num = MOTION_NOTIFY_EVENT;
	  break;
	case GDK_DELETE:
	  signal_num = DELETE_EVENT;
	  break;
	case GDK_DESTROY:
	  signal_num = DESTROY_EVENT;
	  _gtk_tooltip_hide (widget);
	  break;
	case GDK_KEY_PRESS:
	  signal_num = KEY_PRESS_EVENT;
	  break;
	case GDK_KEY_RELEASE:
	  signal_num = KEY_RELEASE_EVENT;
	  break;
	case GDK_ENTER_NOTIFY:
	  signal_num = ENTER_NOTIFY_EVENT;
	  break;
	case GDK_LEAVE_NOTIFY:
	  signal_num = LEAVE_NOTIFY_EVENT;
	  break;
	case GDK_FOCUS_CHANGE:
	  signal_num = event->focus_change.in ? FOCUS_IN_EVENT : FOCUS_OUT_EVENT;
	  if (event->focus_change.in)
	    _gtk_tooltip_focus_in (widget);
	  else
	    _gtk_tooltip_focus_out (widget);
	  break;
	case GDK_CONFIGURE:
	  signal_num = CONFIGURE_EVENT;
	  break;
	case GDK_MAP:
	  signal_num = MAP_EVENT;
	  break;
	case GDK_UNMAP:
	  signal_num = UNMAP_EVENT;
	  break;
	case GDK_WINDOW_STATE:
	  signal_num = WINDOW_STATE_EVENT;
	  break;
	case GDK_PROPERTY_NOTIFY:
	  signal_num = PROPERTY_NOTIFY_EVENT;
	  break;
	case GDK_SELECTION_CLEAR:
	  signal_num = SELECTION_CLEAR_EVENT;
	  break;
	case GDK_SELECTION_REQUEST:
	  signal_num = SELECTION_REQUEST_EVENT;
	  break;
	case GDK_SELECTION_NOTIFY:
	  signal_num = SELECTION_NOTIFY_EVENT;
	  break;
	case GDK_PROXIMITY_IN:
	  signal_num = PROXIMITY_IN_EVENT;
	  break;
	case GDK_PROXIMITY_OUT:
	  signal_num = PROXIMITY_OUT_EVENT;
	  break;
	case GDK_VISIBILITY_NOTIFY:
	  signal_num = VISIBILITY_NOTIFY_EVENT;
	  break;
	case GDK_GRAB_BROKEN:
	  signal_num = GRAB_BROKEN_EVENT;
	  break;
	case GDK_DAMAGE:
	  signal_num = DAMAGE_EVENT;
	  break;
	default:
	  g_warning ("gtk_widget_event(): unhandled event type: %d", event->type);
	  signal_num = -1;
	  break;
	}
      if (signal_num != -1)
        {
	  g_signal_emit (widget, widget_signals[signal_num], 0, event, &handled);
          return_val |= handled;
        }
    }
  if (WIDGET_REALIZED_FOR_EVENT (widget, event))
    g_signal_emit (widget, widget_signals[EVENT_AFTER], 0, event);
  else
    return_val = TRUE;

  g_object_unref (widget);

  return return_val;
}

/**
 * gtk_widget_activate:
 * @widget: a #GtkWidget that’s activatable
 *
 * For widgets that can be “activated” (buttons, menu items, etc.)
 * this function activates them. Activation is what happens when you
 * press Enter on a widget during key navigation. If @widget isn't
 * activatable, the function returns %FALSE.
 *
 * Returns: %TRUE if the widget was activatable
 **/
gboolean
gtk_widget_activate (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (WIDGET_CLASS (widget)->activate_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      g_signal_emit (widget, WIDGET_CLASS (widget)->activate_signal, 0);

      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_widget_reparent_subwindows (GtkWidget *widget,
				GdkWindow *new_window)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (!gtk_widget_get_has_window (widget))
    {
      GList *children = gdk_window_get_children (priv->window);
      GList *tmp_list;

      for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	{
	  GdkWindow *window = tmp_list->data;
	  gpointer child;

	  gdk_window_get_user_data (window, &child);
	  while (child && child != widget)
	    child = ((GtkWidget*) child)->priv->parent;

	  if (child)
	    gdk_window_reparent (window, new_window, 0, 0);
	}

      g_list_free (children);
    }
  else
   {
     GdkWindow *parent;
     GList *tmp_list, *children;

     parent = gdk_window_get_parent (priv->window);

     if (parent == NULL)
       gdk_window_reparent (priv->window, new_window, 0, 0);
     else
       {
	 children = gdk_window_get_children (parent);

	 for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	   {
	     GdkWindow *window = tmp_list->data;
	     gpointer child;

	     gdk_window_get_user_data (window, &child);

	     if (child == widget)
	       gdk_window_reparent (window, new_window, 0, 0);
	   }

	 g_list_free (children);
       }
   }
}

static void
gtk_widget_reparent_fixup_child (GtkWidget *widget,
				 gpointer   client_data)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (client_data != NULL);

  if (!gtk_widget_get_has_window (widget))
    {
      if (priv->window)
	g_object_unref (priv->window);
      priv->window = (GdkWindow*) client_data;
      if (priv->window)
	g_object_ref (priv->window);

      if (GTK_IS_CONTAINER (widget))
        gtk_container_forall (GTK_CONTAINER (widget),
                              gtk_widget_reparent_fixup_child,
                              client_data);
    }
}

/**
 * gtk_widget_reparent:
 * @widget: a #GtkWidget
 * @new_parent: a #GtkContainer to move the widget into
 *
 * Moves a widget from one #GtkContainer to another, handling reference
 * count issues to avoid destroying the widget.
 *
 * Deprecated: 3.14: Use gtk_container_remove() and gtk_container_add().
 **/
void
gtk_widget_reparent (GtkWidget *widget,
		     GtkWidget *new_parent)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_CONTAINER (new_parent));
  priv = widget->priv;
  g_return_if_fail (priv->parent != NULL);

  if (priv->parent != new_parent)
    {
      /* First try to see if we can get away without unrealizing
       * the widget as we reparent it. if so we set a flag so
       * that gtk_widget_unparent doesn't unrealize widget
       */
      if (gtk_widget_get_realized (widget) && gtk_widget_get_realized (new_parent))
	priv->in_reparent = TRUE;

      g_object_ref (widget);
      gtk_container_remove (GTK_CONTAINER (priv->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (widget);

      if (priv->in_reparent)
	{
          priv->in_reparent = FALSE;

	  gtk_widget_reparent_subwindows (widget, gtk_widget_get_parent_window (widget));
	  gtk_widget_reparent_fixup_child (widget,
					   gtk_widget_get_parent_window (widget));
	}

      g_object_notify (G_OBJECT (widget), "parent");
    }
}

/**
 * gtk_widget_intersect:
 * @widget: a #GtkWidget
 * @area: a rectangle
 * @intersection: (nullable): rectangle to store intersection of @widget and @area
 *
 * Computes the intersection of a @widget’s area and @area, storing
 * the intersection in @intersection, and returns %TRUE if there was
 * an intersection.  @intersection may be %NULL if you’re only
 * interested in whether there was an intersection.
 *
 * Returns: %TRUE if there was an intersection
 **/
gboolean
gtk_widget_intersect (GtkWidget	         *widget,
		      const GdkRectangle *area,
		      GdkRectangle       *intersection)
{
  GtkWidgetPrivate *priv;
  GdkRectangle *dest;
  GdkRectangle tmp;
  gint return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (area != NULL, FALSE);

  priv = widget->priv;

  if (intersection)
    dest = intersection;
  else
    dest = &tmp;

  return_val = gdk_rectangle_intersect (&priv->allocation, area, dest);

  if (return_val && intersection && gtk_widget_get_has_window (widget))
    {
      intersection->x -= priv->allocation.x;
      intersection->y -= priv->allocation.y;
    }

  return return_val;
}

/**
 * gtk_widget_region_intersect:
 * @widget: a #GtkWidget
 * @region: a #cairo_region_t, in the same coordinate system as
 *          @widget->allocation. That is, relative to @widget->window
 *          for widgets which return %FALSE from gtk_widget_get_has_window();
 *          relative to the parent window of @widget->window otherwise.
 *
 * Computes the intersection of a @widget’s area and @region, returning
 * the intersection. The result may be empty, use cairo_region_is_empty() to
 * check.
 *
 * Returns: A newly allocated region holding the intersection of @widget
 *     and @region.
 *
 * Deprecated: 3.14: Use gtk_widget_get_allocation() and
 *     cairo_region_intersect_rectangle() to get the same behavior.
 */
cairo_region_t *
gtk_widget_region_intersect (GtkWidget       *widget,
			     const cairo_region_t *region)
{
  GdkRectangle rect;
  cairo_region_t *dest;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (region != NULL, NULL);

  gtk_widget_get_allocation (widget, &rect);

  dest = cairo_region_create_rectangle (&rect);

  cairo_region_intersect (dest, region);

  return dest;
}

/**
 * _gtk_widget_grab_notify:
 * @widget: a #GtkWidget
 * @was_grabbed: whether a grab is now in effect
 *
 * Emits the #GtkWidget::grab-notify signal on @widget.
 *
 * Since: 2.6
 **/
void
_gtk_widget_grab_notify (GtkWidget *widget,
			 gboolean   was_grabbed)
{
  g_signal_emit (widget, widget_signals[GRAB_NOTIFY], 0, was_grabbed);
}

/**
 * gtk_widget_grab_focus:
 * @widget: a #GtkWidget
 *
 * Causes @widget to have the keyboard focus for the #GtkWindow it's
 * inside. @widget must be a focusable widget, such as a #GtkEntry;
 * something like #GtkFrame won’t work.
 *
 * More precisely, it must have the %GTK_CAN_FOCUS flag set. Use
 * gtk_widget_set_can_focus() to modify that flag.
 *
 * The widget also needs to be realized and mapped. This is indicated by the
 * related signals. Grabbing the focus immediately after creating the widget
 * will likely fail and cause critical warnings.
 **/
void
gtk_widget_grab_focus (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_is_sensitive (widget))
    return;

  g_object_ref (widget);
  g_signal_emit (widget, widget_signals[GRAB_FOCUS], 0);
  g_object_notify (G_OBJECT (widget), "has-focus");
  g_object_unref (widget);
}

static void
reset_focus_recurse (GtkWidget *widget,
		     gpointer   data)
{
  if (GTK_IS_CONTAINER (widget))
    {
      GtkContainer *container;

      container = GTK_CONTAINER (widget);
      gtk_container_set_focus_child (container, NULL);

      gtk_container_foreach (container,
			     reset_focus_recurse,
			     NULL);
    }
}

static void
gtk_widget_real_grab_focus (GtkWidget *focus_widget)
{
  if (gtk_widget_get_can_focus (focus_widget))
    {
      GtkWidget *toplevel;
      GtkWidget *widget;

      /* clear the current focus setting, break if the current widget
       * is the focus widget's parent, since containers above that will
       * be set by the next loop.
       */
      toplevel = gtk_widget_get_toplevel (focus_widget);
      if (gtk_widget_is_toplevel (toplevel) && GTK_IS_WINDOW (toplevel))
	{
          widget = gtk_window_get_focus (GTK_WINDOW (toplevel));

	  if (widget == focus_widget)
	    {
	      /* We call _gtk_window_internal_set_focus() here so that the
	       * toplevel window can request the focus if necessary.
	       * This is needed when the toplevel is a GtkPlug
	       */
	      if (!gtk_widget_has_focus (widget))
		_gtk_window_internal_set_focus (GTK_WINDOW (toplevel), focus_widget);

	      return;
	    }

	  if (widget)
	    {
	      GtkWidget *common_ancestor = gtk_widget_common_ancestor (widget, focus_widget);

	      if (widget != common_ancestor)
		{
		  while (widget->priv->parent)
		    {
		      widget = widget->priv->parent;
		      gtk_container_set_focus_child (GTK_CONTAINER (widget), NULL);
		      if (widget == common_ancestor)
		        break;
		    }
		}
	    }
	}
      else if (toplevel != focus_widget)
	{
	  /* gtk_widget_grab_focus() operates on a tree without window...
	   * actually, this is very questionable behaviour.
	   */

	  gtk_container_foreach (GTK_CONTAINER (toplevel),
				 reset_focus_recurse,
				 NULL);
	}

      /* now propagate the new focus up the widget tree and finally
       * set it on the window
       */
      widget = focus_widget;
      while (widget->priv->parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (widget->priv->parent), widget);
	  widget = widget->priv->parent;
	}
      if (GTK_IS_WINDOW (widget))
	_gtk_window_internal_set_focus (GTK_WINDOW (widget), focus_widget);
    }
}

static gboolean
gtk_widget_real_query_tooltip (GtkWidget  *widget,
			       gint        x,
			       gint        y,
			       gboolean    keyboard_tip,
			       GtkTooltip *tooltip)
{
  gchar *tooltip_markup;
  gboolean has_tooltip;

  tooltip_markup = g_object_get_qdata (G_OBJECT (widget), quark_tooltip_markup);
  has_tooltip = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (widget), quark_has_tooltip));

  if (has_tooltip && tooltip_markup)
    {
      gtk_tooltip_set_markup (tooltip, tooltip_markup);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_widget_real_state_flags_changed (GtkWidget     *widget,
                                     GtkStateFlags  old_state)
{
  gtk_widget_update_pango_context (widget);
}

static void
gtk_widget_real_style_updated (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  gtk_widget_update_pango_context (widget);
  gtk_widget_update_alpha (widget);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  if (priv->style != NULL &&
      priv->style != gtk_widget_get_default_style ())
    {
      /* Trigger ::style-set for old
       * widgets not listening to this
       */
      g_signal_emit (widget,
                     widget_signals[STYLE_SET],
                     0,
                     widget->priv->style);
    }
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (widget->priv->context)
    {
      const GtkBitmask *changes = _gtk_style_context_get_changes (widget->priv->context);

      if (gtk_widget_get_realized (widget) &&
          gtk_widget_get_has_window (widget) &&
          !gtk_widget_get_app_paintable (widget))
        gtk_style_context_set_background (widget->priv->context,
                                          widget->priv->window);

      if (widget->priv->anchored)
        {
          static GtkBitmask *affects_size = NULL;

          if (G_UNLIKELY (affects_size == NULL))
            affects_size = _gtk_css_style_property_get_mask_affecting (GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_CLIP);

          if (changes == NULL || _gtk_bitmask_intersects (changes, affects_size))
            gtk_widget_queue_resize (widget);
          else
            gtk_widget_queue_draw (widget);
        }
    }
  else
    {
      if (widget->priv->anchored)
        gtk_widget_queue_resize (widget);
    }
}

static gboolean
gtk_widget_real_show_help (GtkWidget        *widget,
                           GtkWidgetHelpType help_type)
{
  if (help_type == GTK_WIDGET_HELP_TOOLTIP)
    {
      _gtk_tooltip_toggle_keyboard_mode (widget);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_widget_real_focus (GtkWidget         *widget,
                       GtkDirectionType   direction)
{
  if (!gtk_widget_get_can_focus (widget))
    return FALSE;

  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_widget_real_move_focus (GtkWidget         *widget,
                            GtkDirectionType   direction)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (widget != toplevel && GTK_IS_WINDOW (toplevel))
    {
      g_signal_emit (toplevel, widget_signals[MOVE_FOCUS], 0,
                     direction);
    }
}

static gboolean
gtk_widget_real_keynav_failed (GtkWidget        *widget,
                               GtkDirectionType  direction)
{
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
      return FALSE;

    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      break;
    }

  gtk_widget_error_bell (widget);

  return TRUE;
}

/**
 * gtk_widget_set_can_focus:
 * @widget: a #GtkWidget
 * @can_focus: whether or not @widget can own the input focus.
 *
 * Specifies whether @widget can own the input focus. See
 * gtk_widget_grab_focus() for actually setting the input focus on a
 * widget.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_can_focus (GtkWidget *widget,
                          gboolean   can_focus)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->priv->can_focus != can_focus)
    {
      widget->priv->can_focus = can_focus;

      gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "can-focus");
    }
}

/**
 * gtk_widget_get_can_focus:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can own the input focus. See
 * gtk_widget_set_can_focus().
 *
 * Returns: %TRUE if @widget can own the input focus, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_can_focus (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->can_focus;
}

/**
 * gtk_widget_has_focus:
 * @widget: a #GtkWidget
 *
 * Determines if the widget has the global input focus. See
 * gtk_widget_is_focus() for the difference between having the global
 * input focus, and only having the focus within a toplevel.
 *
 * Returns: %TRUE if the widget has the global input focus.
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_has_focus (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_focus;
}

/**
 * gtk_widget_has_visible_focus:
 * @widget: a #GtkWidget
 *
 * Determines if the widget should show a visible indication that
 * it has the global input focus. This is a convenience function for
 * use in ::draw handlers that takes into account whether focus
 * indication should currently be shown in the toplevel window of
 * @widget. See gtk_window_get_focus_visible() for more information
 * about focus indication.
 *
 * To find out if the widget has the global input focus, use
 * gtk_widget_has_focus().
 *
 * Returns: %TRUE if the widget should display a “focus rectangle”
 *
 * Since: 3.2
 */
gboolean
gtk_widget_has_visible_focus (GtkWidget *widget)
{
  gboolean draw_focus;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (widget->priv->has_focus)
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_toplevel (widget);

      if (GTK_IS_WINDOW (toplevel))
        draw_focus = gtk_window_get_focus_visible (GTK_WINDOW (toplevel));
      else
        draw_focus = TRUE;
    }
  else
    draw_focus = FALSE;

  return draw_focus;
}

/**
 * gtk_widget_is_focus:
 * @widget: a #GtkWidget
 *
 * Determines if the widget is the focus widget within its
 * toplevel. (This does not mean that the #GtkWidget:has-focus property is
 * necessarily set; #GtkWidget:has-focus will only be set if the
 * toplevel widget additionally has the global input focus.)
 *
 * Returns: %TRUE if the widget is the focus widget.
 **/
gboolean
gtk_widget_is_focus (GtkWidget *widget)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    return widget == gtk_window_get_focus (GTK_WINDOW (toplevel));
  else
    return FALSE;
}

/**
 * gtk_widget_set_can_default:
 * @widget: a #GtkWidget
 * @can_default: whether or not @widget can be a default widget.
 *
 * Specifies whether @widget can be a default widget. See
 * gtk_widget_grab_default() for details about the meaning of
 * “default”.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_can_default (GtkWidget *widget,
                            gboolean   can_default)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->priv->can_default != can_default)
    {
      widget->priv->can_default = can_default;

      gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "can-default");
    }
}

/**
 * gtk_widget_get_can_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can be a default widget. See
 * gtk_widget_set_can_default().
 *
 * Returns: %TRUE if @widget can be a default widget, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_can_default (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->can_default;
}

/**
 * gtk_widget_has_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is the current default widget within its
 * toplevel. See gtk_widget_set_can_default().
 *
 * Returns: %TRUE if @widget is the current default widget within
 *     its toplevel, %FALSE otherwise
 *
 * Since: 2.18
 */
gboolean
gtk_widget_has_default (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_default;
}

void
_gtk_widget_set_has_default (GtkWidget *widget,
                             gboolean   has_default)
{
  GtkStyleContext *context;

  widget->priv->has_default = has_default;

  context = gtk_widget_get_style_context (widget);

  if (has_default)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_DEFAULT);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_DEFAULT);
}

/**
 * gtk_widget_grab_default:
 * @widget: a #GtkWidget
 *
 * Causes @widget to become the default widget. @widget must be able to be
 * a default widget; typically you would ensure this yourself
 * by calling gtk_widget_set_can_default() with a %TRUE value.
 * The default widget is activated when
 * the user presses Enter in a window. Default widgets must be
 * activatable, that is, gtk_widget_activate() should affect them. Note
 * that #GtkEntry widgets require the “activates-default” property
 * set to %TRUE before they activate the default widget when Enter
 * is pressed and the #GtkEntry is focused.
 **/
void
gtk_widget_grab_default (GtkWidget *widget)
{
  GtkWidget *window;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_can_default (widget));

  window = gtk_widget_get_toplevel (widget);

  if (window && gtk_widget_is_toplevel (window))
    gtk_window_set_default (GTK_WINDOW (window), widget);
  else
    g_warning (G_STRLOC ": widget not within a GtkWindow");
}

/**
 * gtk_widget_set_receives_default:
 * @widget: a #GtkWidget
 * @receives_default: whether or not @widget can be a default widget.
 *
 * Specifies whether @widget will be treated as the default widget
 * within its toplevel when it has the focus, even if another widget
 * is the default.
 *
 * See gtk_widget_grab_default() for details about the meaning of
 * “default”.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_receives_default (GtkWidget *widget,
                                 gboolean   receives_default)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->priv->receives_default != receives_default)
    {
      widget->priv->receives_default = receives_default;

      g_object_notify (G_OBJECT (widget), "receives-default");
    }
}

/**
 * gtk_widget_get_receives_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is always treated as the default widget
 * within its toplevel when it has the focus, even if another widget
 * is the default.
 *
 * See gtk_widget_set_receives_default().
 *
 * Returns: %TRUE if @widget acts as the default widget when focussed,
 *               %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_receives_default (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->receives_default;
}

/**
 * gtk_widget_has_grab:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget is currently grabbing events, so it
 * is the only widget receiving input events (keyboard and mouse).
 *
 * See also gtk_grab_add().
 *
 * Returns: %TRUE if the widget is in the grab_widgets stack
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_has_grab (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_grab;
}

void
_gtk_widget_set_has_grab (GtkWidget *widget,
                          gboolean   has_grab)
{
  widget->priv->has_grab = has_grab;
}

/**
 * gtk_widget_device_is_shadowed:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns %TRUE if @device has been shadowed by a GTK+
 * device grab on another widget, so it would stop sending
 * events to @widget. This may be used in the
 * #GtkWidget::grab-notify signal to check for specific
 * devices. See gtk_device_grab_add().
 *
 * Returns: %TRUE if there is an ongoing grab on @device
 *          by another #GtkWidget than @widget.
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_device_is_shadowed (GtkWidget *widget,
                               GdkDevice *device)
{
  GtkWindowGroup *group;
  GtkWidget *grab_widget, *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (!gtk_widget_get_realized (widget))
    return TRUE;

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    group = gtk_window_get_group (GTK_WINDOW (toplevel));
  else
    group = gtk_window_get_group (NULL);

  grab_widget = gtk_window_group_get_current_device_grab (group, device);

  /* Widget not inside the hierarchy of grab_widget */
  if (grab_widget &&
      widget != grab_widget &&
      !gtk_widget_is_ancestor (widget, grab_widget))
    return TRUE;

  grab_widget = gtk_window_group_get_current_grab (group);
  if (grab_widget && widget != grab_widget &&
      !gtk_widget_is_ancestor (widget, grab_widget))
    return TRUE;

  return FALSE;
}

/**
 * gtk_widget_set_name:
 * @widget: a #GtkWidget
 * @name: name for the widget
 *
 * Widgets can be named, which allows you to refer to them from a
 * CSS file. You can apply a style to widgets with a particular name
 * in the CSS file. See the documentation for the CSS syntax (on the
 * same page as the docs for #GtkStyleContext).
 *
 * Note that the CSS syntax has certain special characters to delimit
 * and represent elements in a selector (period, #, >, *...), so using
 * these will make your widget impossible to match by name. Any combination
 * of alphanumeric symbols, dashes and underscores will suffice.
 */
void
gtk_widget_set_name (GtkWidget	 *widget,
		     const gchar *name)
{
  GtkWidgetPrivate *priv;
  gchar *new_name;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  new_name = g_strdup (name);
  g_free (priv->name);
  priv->name = new_name;

  if (priv->context)
    gtk_style_context_set_id (priv->context, priv->name);

  g_object_notify (G_OBJECT (widget), "name");
}

/**
 * gtk_widget_get_name:
 * @widget: a #GtkWidget
 *
 * Retrieves the name of a widget. See gtk_widget_set_name() for the
 * significance of widget names.
 *
 * Returns: name of the widget. This string is owned by GTK+ and
 * should not be modified or freed
 **/
const gchar*
gtk_widget_get_name (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  if (priv->name)
    return priv->name;
  return G_OBJECT_TYPE_NAME (widget);
}

static void
gtk_widget_update_state_flags (GtkWidget     *widget,
                               GtkStateFlags  flags_to_set,
                               GtkStateFlags  flags_to_unset)
{
  GtkWidgetPrivate *priv;

  priv = widget->priv;

  /* Handle insensitive first, since it is propagated
   * differently throughout the widget hierarchy.
   */
  if ((priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && (flags_to_unset & GTK_STATE_FLAG_INSENSITIVE))
    gtk_widget_set_sensitive (widget, TRUE);
  else if (!(priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && (flags_to_set & GTK_STATE_FLAG_INSENSITIVE))
    gtk_widget_set_sensitive (widget, FALSE);

  flags_to_set &= ~(GTK_STATE_FLAG_INSENSITIVE);
  flags_to_unset &= ~(GTK_STATE_FLAG_INSENSITIVE);

  if (flags_to_set != 0 || flags_to_unset != 0)
    {
      GtkStateData data;

      data.flags_to_set = flags_to_set;
      data.flags_to_unset = flags_to_unset;

      gtk_widget_propagate_state (widget, &data);
    }
}

/**
 * gtk_widget_set_state_flags:
 * @widget: a #GtkWidget
 * @flags: State flags to turn on
 * @clear: Whether to clear state before turning on @flags
 *
 * This function is for use in widget implementations. Turns on flag
 * values in the current widget state (insensitive, prelighted, etc.).
 *
 * This function accepts the values %GTK_STATE_FLAG_DIR_LTR and
 * %GTK_STATE_FLAG_DIR_RTL but ignores them. If you want to set the widget's
 * direction, use gtk_widget_set_direction().
 *
 * It is worth mentioning that any other state than %GTK_STATE_FLAG_INSENSITIVE,
 * will be propagated down to all non-internal children if @widget is a
 * #GtkContainer, while %GTK_STATE_FLAG_INSENSITIVE itself will be propagated
 * down to all #GtkContainer children by different means than turning on the
 * state flag down the hierarchy, both gtk_widget_get_state_flags() and
 * gtk_widget_is_sensitive() will make use of these.
 *
 * Since: 3.0
 **/
void
gtk_widget_set_state_flags (GtkWidget     *widget,
                            GtkStateFlags  flags,
                            gboolean       clear)
{
#define ALLOWED_FLAGS (~(GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL))

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (flags < (1 << GTK_STATE_FLAGS_BITS));

  if ((!clear && (widget->priv->state_flags & flags) == flags) ||
      (clear && widget->priv->state_flags == flags))
    return;

  if (clear)
    gtk_widget_update_state_flags (widget, flags & ALLOWED_FLAGS, ~flags & ALLOWED_FLAGS);
  else
    gtk_widget_update_state_flags (widget, flags & ALLOWED_FLAGS, 0);

#undef ALLOWED_FLAGS
}

/**
 * gtk_widget_unset_state_flags:
 * @widget: a #GtkWidget
 * @flags: State flags to turn off
 *
 * This function is for use in widget implementations. Turns off flag
 * values for the current widget state (insensitive, prelighted, etc.).
 * See gtk_widget_set_state_flags().
 *
 * Since: 3.0
 **/
void
gtk_widget_unset_state_flags (GtkWidget     *widget,
                              GtkStateFlags  flags)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (flags < (1 << GTK_STATE_FLAGS_BITS));

  if ((widget->priv->state_flags & flags) == 0)
    return;

  gtk_widget_update_state_flags (widget, 0, flags);
}

/**
 * gtk_widget_get_state_flags:
 * @widget: a #GtkWidget
 *
 * Returns the widget state as a flag set. It is worth mentioning
 * that the effective %GTK_STATE_FLAG_INSENSITIVE state will be
 * returned, that is, also based on parent insensitivity, even if
 * @widget itself is sensitive.
 *
 * Returns: The state flags for widget
 *
 * Since: 3.0
 **/
GtkStateFlags
gtk_widget_get_state_flags (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return widget->priv->state_flags;
}

/**
 * gtk_widget_set_state:
 * @widget: a #GtkWidget
 * @state: new state for @widget
 *
 * This function is for use in widget implementations. Sets the state
 * of a widget (insensitive, prelighted, etc.) Usually you should set
 * the state using wrapper functions such as gtk_widget_set_sensitive().
 *
 * Deprecated: 3.0: Use gtk_widget_set_state_flags() instead.
 **/
void
gtk_widget_set_state (GtkWidget           *widget,
		      GtkStateType         state)
{
  GtkStateFlags flags;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  if (state == gtk_widget_get_state (widget))
    return;
  G_GNUC_END_IGNORE_DEPRECATIONS;

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_INCONSISTENT:
      flags = GTK_STATE_FLAG_INCONSISTENT;
      break;
    case GTK_STATE_FOCUSED:
      flags = GTK_STATE_FLAG_FOCUSED;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
      break;
    }

  gtk_widget_update_state_flags (widget,
                                 flags,
                                 (GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED
                                 | GTK_STATE_FLAG_INSENSITIVE | GTK_STATE_FLAG_INCONSISTENT | GTK_STATE_FLAG_FOCUSED) ^ flags);
}

/**
 * gtk_widget_get_state:
 * @widget: a #GtkWidget
 *
 * Returns the widget’s state. See gtk_widget_set_state().
 *
 * Returns: the state of @widget.
 *
 * Since: 2.18
 *
 * Deprecated: 3.0: Use gtk_widget_get_state_flags() instead.
 */
GtkStateType
gtk_widget_get_state (GtkWidget *widget)
{
  GtkStateFlags flags;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_STATE_NORMAL);

  flags = gtk_widget_get_state_flags (widget);

  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    return GTK_STATE_INSENSITIVE;
  else if (flags & GTK_STATE_FLAG_ACTIVE)
    return GTK_STATE_ACTIVE;
  else if (flags & GTK_STATE_FLAG_SELECTED)
    return GTK_STATE_SELECTED;
  else if (flags & GTK_STATE_FLAG_PRELIGHT)
    return GTK_STATE_PRELIGHT;
  else
    return GTK_STATE_NORMAL;
}

/**
 * gtk_widget_set_visible:
 * @widget: a #GtkWidget
 * @visible: whether the widget should be shown or not
 *
 * Sets the visibility state of @widget. Note that setting this to
 * %TRUE doesn’t mean the widget is actually viewable, see
 * gtk_widget_get_visible().
 *
 * This function simply calls gtk_widget_show() or gtk_widget_hide()
 * but is nicer to use when the visibility of the widget depends on
 * some condition.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_visible (GtkWidget *widget,
                        gboolean   visible)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (visible)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
}

void
_gtk_widget_set_visible_flag (GtkWidget *widget,
                              gboolean   visible)
{
  GtkWidgetPrivate *priv = widget->priv;

  priv->visible = visible;

  if (!visible)
    {
      priv->allocation.x = -1;
      priv->allocation.y = -1;
      priv->allocation.width = 1;
      priv->allocation.height = 1;
      memset (&priv->clip, 0, sizeof (priv->clip));
    }
}

/**
 * gtk_widget_get_visible:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget is visible. If you want to
 * take into account whether the widget’s parent is also marked as
 * visible, use gtk_widget_is_visible() instead.
 *
 * This function does not check if the widget is obscured in any way.
 *
 * See gtk_widget_set_visible().
 *
 * Returns: %TRUE if the widget is visible
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->visible;
}

/**
 * gtk_widget_is_visible:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget and all its parents are marked as
 * visible.
 *
 * This function does not check if the widget is obscured in any way.
 *
 * See also gtk_widget_get_visible() and gtk_widget_set_visible()
 *
 * Returns: %TRUE if the widget and all its parents are visible
 *
 * Since: 3.8
 **/
gboolean
gtk_widget_is_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  while (widget)
    {
      GtkWidgetPrivate *priv = widget->priv;

      if (!priv->visible)
        return FALSE;

      widget = priv->parent;
    }

  return TRUE;
}

/**
 * gtk_widget_set_has_window:
 * @widget: a #GtkWidget
 * @has_window: whether or not @widget has a window.
 *
 * Specifies whether @widget has a #GdkWindow of its own. Note that
 * all realized widgets have a non-%NULL “window” pointer
 * (gtk_widget_get_window() never returns a %NULL window when a widget
 * is realized), but for many of them it’s actually the #GdkWindow of
 * one of its parent widgets. Widgets that do not create a %window for
 * themselves in #GtkWidget::realize must announce this by
 * calling this function with @has_window = %FALSE.
 *
 * This function should only be called by widget implementations,
 * and they should call it in their init() function.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_has_window (GtkWidget *widget,
                           gboolean   has_window)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->no_window = !has_window;
}

/**
 * gtk_widget_get_has_window:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget has a #GdkWindow of its own. See
 * gtk_widget_set_has_window().
 *
 * Returns: %TRUE if @widget has a window, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_has_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return ! widget->priv->no_window;
}

/**
 * gtk_widget_is_toplevel:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is a toplevel widget.
 *
 * Currently only #GtkWindow and #GtkInvisible (and out-of-process
 * #GtkPlugs) are toplevel widgets. Toplevel widgets have no parent
 * widget.
 *
 * Returns: %TRUE if @widget is a toplevel, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_is_toplevel (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->toplevel;
}

void
_gtk_widget_set_is_toplevel (GtkWidget *widget,
                             gboolean   is_toplevel)
{
  widget->priv->toplevel = is_toplevel;
}

/**
 * gtk_widget_is_drawable:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can be drawn to. A widget can be drawn
 * to if it is mapped and visible.
 *
 * Returns: %TRUE if @widget is drawable, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_is_drawable (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (gtk_widget_get_visible (widget) &&
          gtk_widget_get_mapped (widget));
}

/**
 * gtk_widget_get_realized:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is realized.
 *
 * Returns: %TRUE if @widget is realized, %FALSE otherwise
 *
 * Since: 2.20
 **/
gboolean
gtk_widget_get_realized (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->realized;
}

/**
 * gtk_widget_set_realized:
 * @widget: a #GtkWidget
 * @realized: %TRUE to mark the widget as realized
 *
 * Marks the widget as being realized. This function must only be 
 * called after all #GdkWindows for the @widget have been created 
 * and registered.
 *
 * This function should only ever be called in a derived widget's
 * “realize” or “unrealize” implementation.
 *
 * Since: 2.20
 */
void
gtk_widget_set_realized (GtkWidget *widget,
                         gboolean   realized)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->realized = realized;
}

/**
 * gtk_widget_get_mapped:
 * @widget: a #GtkWidget
 *
 * Whether the widget is mapped.
 *
 * Returns: %TRUE if the widget is mapped, %FALSE otherwise.
 *
 * Since: 2.20
 */
gboolean
gtk_widget_get_mapped (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->mapped;
}

/**
 * gtk_widget_set_mapped:
 * @widget: a #GtkWidget
 * @mapped: %TRUE to mark the widget as mapped
 *
 * Marks the widget as being realized.
 *
 * This function should only ever be called in a derived widget's
 * “map” or “unmap” implementation.
 *
 * Since: 2.20
 */
void
gtk_widget_set_mapped (GtkWidget *widget,
                       gboolean   mapped)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->mapped = mapped;
}

/**
 * gtk_widget_set_app_paintable:
 * @widget: a #GtkWidget
 * @app_paintable: %TRUE if the application will paint on the widget
 *
 * Sets whether the application intends to draw on the widget in
 * an #GtkWidget::draw handler.
 *
 * This is a hint to the widget and does not affect the behavior of
 * the GTK+ core; many widgets ignore this flag entirely. For widgets
 * that do pay attention to the flag, such as #GtkEventBox and #GtkWindow,
 * the effect is to suppress default themed drawing of the widget's
 * background. (Children of the widget will still be drawn.) The application
 * is then entirely responsible for drawing the widget background.
 *
 * Note that the background is still drawn when the widget is mapped.
 **/
void
gtk_widget_set_app_paintable (GtkWidget *widget,
			      gboolean   app_paintable)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  app_paintable = (app_paintable != FALSE);

  if (widget->priv->app_paintable != app_paintable)
    {
      widget->priv->app_paintable = app_paintable;

      if (gtk_widget_is_drawable (widget))
	gtk_widget_queue_draw (widget);

      g_object_notify (G_OBJECT (widget), "app-paintable");
    }
}

/**
 * gtk_widget_get_app_paintable:
 * @widget: a #GtkWidget
 *
 * Determines whether the application intends to draw on the widget in
 * an #GtkWidget::draw handler.
 *
 * See gtk_widget_set_app_paintable()
 *
 * Returns: %TRUE if the widget is app paintable
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_app_paintable (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->app_paintable;
}

/**
 * gtk_widget_set_double_buffered:
 * @widget: a #GtkWidget
 * @double_buffered: %TRUE to double-buffer a widget
 *
 * Widgets are double buffered by default; you can use this function
 * to turn off the buffering. “Double buffered” simply means that
 * gdk_window_begin_paint_region() and gdk_window_end_paint() are called
 * automatically around expose events sent to the
 * widget. gdk_window_begin_paint_region() diverts all drawing to a widget's
 * window to an offscreen buffer, and gdk_window_end_paint() draws the
 * buffer to the screen. The result is that users see the window
 * update in one smooth step, and don’t see individual graphics
 * primitives being rendered.
 *
 * In very simple terms, double buffered widgets don’t flicker,
 * so you would only use this function to turn off double buffering
 * if you had special needs and really knew what you were doing.
 *
 * Note: if you turn off double-buffering, you have to handle
 * expose events, since even the clearing to the background color or
 * pixmap will not happen automatically (as it is done in
 * gdk_window_begin_paint_region()).
 *
 * In 3.10 GTK and GDK have been restructured for translucent drawing. Since
 * then expose events for double-buffered widgets are culled into a single
 * event to the toplevel GDK window. If you now unset double buffering, you
 * will cause a separate rendering pass for every widget. This will likely
 * cause rendering problems - in particular related to stacking - and usually
 * increases rrendering times significantly.
 *
 * Deprecated: 3.14: This function does not work under non-X11 backends or with
 * non-native windows.
 * It should not be used in newly written code.
 **/
void
gtk_widget_set_double_buffered (GtkWidget *widget,
				gboolean   double_buffered)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  double_buffered = (double_buffered != FALSE);

  if (widget->priv->double_buffered != double_buffered)
    {
      widget->priv->double_buffered = double_buffered;

      g_object_notify (G_OBJECT (widget), "double-buffered");
    }
}

/**
 * gtk_widget_get_double_buffered:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget is double buffered.
 *
 * See gtk_widget_set_double_buffered()
 *
 * Returns: %TRUE if the widget is double buffered
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_double_buffered (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->double_buffered;
}

/**
 * gtk_widget_set_redraw_on_allocate:
 * @widget: a #GtkWidget
 * @redraw_on_allocate: if %TRUE, the entire widget will be redrawn
 *   when it is allocated to a new size. Otherwise, only the
 *   new portion of the widget will be redrawn.
 *
 * Sets whether the entire widget is queued for drawing when its size
 * allocation changes. By default, this setting is %TRUE and
 * the entire widget is redrawn on every size change. If your widget
 * leaves the upper left unchanged when made bigger, turning this
 * setting off will improve performance.

 * Note that for widgets where gtk_widget_get_has_window() is %FALSE
 * setting this flag to %FALSE turns off all allocation on resizing:
 * the widget will not even redraw if its position changes; this is to
 * allow containers that don’t draw anything to avoid excess
 * invalidations. If you set this flag on a widget with no window that
 * does draw on @widget->window, you are
 * responsible for invalidating both the old and new allocation of the
 * widget when the widget is moved and responsible for invalidating
 * regions newly when the widget increases size.
 **/
void
gtk_widget_set_redraw_on_allocate (GtkWidget *widget,
				   gboolean   redraw_on_allocate)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->redraw_on_alloc = redraw_on_allocate;
}

/**
 * gtk_widget_set_sensitive:
 * @widget: a #GtkWidget
 * @sensitive: %TRUE to make the widget sensitive
 *
 * Sets the sensitivity of a widget. A widget is sensitive if the user
 * can interact with it. Insensitive widgets are “grayed out” and the
 * user can’t interact with them. Insensitive widgets are known as
 * “inactive”, “disabled”, or “ghosted” in some other toolkits.
 **/
void
gtk_widget_set_sensitive (GtkWidget *widget,
			  gboolean   sensitive)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  sensitive = (sensitive != FALSE);

  if (priv->sensitive == sensitive)
    return;

  priv->sensitive = sensitive;

  if (priv->parent == NULL
      || gtk_widget_is_sensitive (priv->parent))
    {
      GtkStateData data;

      if (sensitive)
        {
          data.flags_to_set = 0;
          data.flags_to_unset = GTK_STATE_FLAG_INSENSITIVE;
        }
      else
        {
          data.flags_to_set = GTK_STATE_FLAG_INSENSITIVE;
          data.flags_to_unset = 0;
        }

      gtk_widget_propagate_state (widget, &data);

      gtk_widget_queue_resize (widget);
    }

  g_object_notify (G_OBJECT (widget), "sensitive");
}

/**
 * gtk_widget_get_sensitive:
 * @widget: a #GtkWidget
 *
 * Returns the widget’s sensitivity (in the sense of returning
 * the value that has been set using gtk_widget_set_sensitive()).
 *
 * The effective sensitivity of a widget is however determined by both its
 * own and its parent widget’s sensitivity. See gtk_widget_is_sensitive().
 *
 * Returns: %TRUE if the widget is sensitive
 *
 * Since: 2.18
 */
gboolean
gtk_widget_get_sensitive (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->sensitive;
}

/**
 * gtk_widget_is_sensitive:
 * @widget: a #GtkWidget
 *
 * Returns the widget’s effective sensitivity, which means
 * it is sensitive itself and also its parent widget is sensitive
 *
 * Returns: %TRUE if the widget is effectively sensitive
 *
 * Since: 2.18
 */
gboolean
gtk_widget_is_sensitive (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return !(widget->priv->state_flags & GTK_STATE_FLAG_INSENSITIVE);
}

/**
 * gtk_widget_set_parent:
 * @widget: a #GtkWidget
 * @parent: parent container
 *
 * This function is useful only when implementing subclasses of
 * #GtkContainer.
 * Sets the container as the parent of @widget, and takes care of
 * some details such as updating the state and style of the child
 * to reflect its new location. The opposite function is
 * gtk_widget_unparent().
 **/
void
gtk_widget_set_parent (GtkWidget *widget,
		       GtkWidget *parent)
{
  GtkStateFlags parent_flags;
  GtkWidgetPrivate *priv;
  GtkStateData data;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (widget != parent);

  priv = widget->priv;

  if (priv->parent != NULL)
    {
      g_warning ("Can't set a parent on widget which has a parent\n");
      return;
    }
  if (gtk_widget_is_toplevel (widget))
    {
      g_warning ("Can't set a parent on a toplevel widget\n");
      return;
    }

  /* keep this function in sync with gtk_menu_attach_to_widget()
   */

  g_object_ref_sink (widget);

  gtk_widget_push_verify_invariants (widget);

  priv->parent = parent;

  parent_flags = gtk_widget_get_state_flags (parent);

  /* Merge both old state and current parent state,
   * making sure to only propagate the right states */
  data.flags_to_set = parent_flags & GTK_STATE_FLAGS_DO_PROPAGATE;
  data.flags_to_unset = 0;
  gtk_widget_propagate_state (widget, &data);

  if (priv->context)
    gtk_style_context_set_parent (priv->context,
                                  gtk_widget_get_style_context (parent));

  _gtk_widget_update_parent_muxer (widget);

  g_signal_emit (widget, widget_signals[PARENT_SET], 0, NULL);
  if (priv->parent->priv->anchored)
    _gtk_widget_propagate_hierarchy_changed (widget, NULL);
  g_object_notify (G_OBJECT (widget), "parent");

  /* Enforce realized/mapped invariants
   */
  if (gtk_widget_get_realized (priv->parent))
    gtk_widget_realize (widget);

  if (gtk_widget_get_visible (priv->parent) &&
      gtk_widget_get_visible (widget))
    {
      if (gtk_widget_get_child_visible (widget) &&
	  gtk_widget_get_mapped (priv->parent))
	gtk_widget_map (widget);

      gtk_widget_queue_resize (widget);
    }

  /* child may cause parent's expand to change, if the child is
   * expanded. If child is not expanded, then it can't modify the
   * parent's expand. If the child becomes expanded later then it will
   * queue compute_expand then. This optimization plus defaulting
   * newly-constructed widgets to need_compute_expand=FALSE should
   * mean that initially building a widget tree doesn't have to keep
   * walking up setting need_compute_expand on parents over and over.
   *
   * We can't change a parent to need to expand unless we're visible.
   */
  if (gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (parent);
    }

  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_get_parent:
 * @widget: a #GtkWidget
 *
 * Returns the parent container of @widget.
 *
 * Returns: (transfer none): the parent container of @widget, or %NULL
 **/
GtkWidget *
gtk_widget_get_parent (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return widget->priv->parent;
}

static GtkModifierStyle *
_gtk_widget_get_modifier_properties (GtkWidget *widget)
{
  GtkModifierStyle *style;

  style = g_object_get_qdata (G_OBJECT (widget), quark_modifier_style);

  if (G_UNLIKELY (!style))
    {
      GtkStyleContext *context;

      style = _gtk_modifier_style_new ();
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_modifier_style,
                               style,
                               (GDestroyNotify) g_object_unref);

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_add_provider (context,
                                      GTK_STYLE_PROVIDER (style),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  return style;
}

/**
 * gtk_widget_override_color:
 * @widget: a #GtkWidget
 * @state: the state for which to set the color
 * @color: (allow-none): the color to assign, or %NULL to undo the effect
 *     of previous calls to gtk_widget_override_color()
 *
 * Sets the color to use for a widget.
 *
 * All other style values are left untouched.
 *
 * This function does not act recursively. Setting the color of a
 * container does not affect its children. Note that some widgets that
 * you may not think of as containers, for instance #GtkButtons,
 * are actually containers.
 *
 * This API is mostly meant as a quick way for applications to
 * change a widget appearance. If you are developing a widgets
 * library and intend this change to be themeable, it is better
 * done by setting meaningful CSS classes and regions in your
 * widget/container implementation through gtk_style_context_add_class()
 * and gtk_style_context_add_region().
 *
 * This way, your widget library can install a #GtkCssProvider
 * with the %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK priority in order
 * to provide a default styling for those widgets that need so, and
 * this theming may fully overridden by the user’s theme.
 *
 * Note that for complex widgets this may bring in undesired
 * results (such as uniform background color everywhere), in
 * these cases it is better to fully style such widgets through a
 * #GtkCssProvider with the %GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
 * priority.
 *
 * Since: 3.0
 *
 * Deprecated:3.16: Use a custom style provider and style classes instead
 */
void
gtk_widget_override_color (GtkWidget     *widget,
                           GtkStateFlags  state,
                           const GdkRGBA *color)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_color (style, state, color);
}

/**
 * gtk_widget_override_background_color:
 * @widget: a #GtkWidget
 * @state: the state for which to set the background color
 * @color: (allow-none): the color to assign, or %NULL to undo the effect
 *     of previous calls to gtk_widget_override_background_color()
 *
 * Sets the background color to use for a widget.
 *
 * All other style values are left untouched.
 * See gtk_widget_override_color().
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: This function is not useful in the context of CSS-based
 *   rendering. If you wish to change the way a widget renders its background
 *   you should use a custom CSS style, through an application-specific
 *   #GtkStyleProvider and a CSS style class. You can also override the default
 *   drawing of a widget through the #GtkWidget::draw signal, and use Cairo to
 *   draw a specific color, regardless of the CSS style.
 */
void
gtk_widget_override_background_color (GtkWidget     *widget,
                                      GtkStateFlags  state,
                                      const GdkRGBA *color)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_background_color (style, state, color);
}

/**
 * gtk_widget_override_font:
 * @widget: a #GtkWidget
 * @font_desc: (allow-none): the font descriptiong to use, or %NULL to undo
 *     the effect of previous calls to gtk_widget_override_font()
 *
 * Sets the font to use for a widget. All other style values are
 * left untouched. See gtk_widget_override_color().
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: This function is not useful in the context of CSS-based
 *   rendering. If you wish to change the font a widget uses to render its text
 *   you should use a custom CSS style, through an application-specific
 *   #GtkStyleProvider and a CSS style class.
 */
void
gtk_widget_override_font (GtkWidget                  *widget,
                          const PangoFontDescription *font_desc)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_font (style, font_desc);
}

/**
 * gtk_widget_override_symbolic_color:
 * @widget: a #GtkWidget
 * @name: the name of the symbolic color to modify
 * @color: (allow-none): the color to assign (does not need
 *     to be allocated), or %NULL to undo the effect of previous
 *     calls to gtk_widget_override_symbolic_color()
 *
 * Sets a symbolic color for a widget.
 *
 * All other style values are left untouched.
 * See gtk_widget_override_color() for overriding the foreground
 * or background color.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: This function is not useful in the context of CSS-based
 *   rendering. If you wish to change the color used to render symbolic icons
 *   you should use a custom CSS style, through an application-specific
 *   #GtkStyleProvider and a CSS style class.
 */
void
gtk_widget_override_symbolic_color (GtkWidget     *widget,
                                    const gchar   *name,
                                    const GdkRGBA *color)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_map_color (style, name, color);
}

/**
 * gtk_widget_override_cursor:
 * @widget: a #GtkWidget
 * @cursor: (allow-none): the color to use for primary cursor (does not need to be
 *     allocated), or %NULL to undo the effect of previous calls to
 *     of gtk_widget_override_cursor().
 * @secondary_cursor: (allow-none): the color to use for secondary cursor (does not
 *     need to be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_override_cursor().
 *
 * Sets the cursor color to use in a widget, overriding the
 * cursor-color and secondary-cursor-color
 * style properties. All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Note that the underlying properties have the #GdkColor type,
 * so the alpha value in @primary and @secondary will be ignored.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: This function is not useful in the context of CSS-based
 *   rendering. If you wish to change the color used to render the primary
 *   and seconday cursors you should use a custom CSS style, through an
 *   application-specific #GtkStyleProvider and a CSS style class.
 */
void
gtk_widget_override_cursor (GtkWidget     *widget,
                            const GdkRGBA *cursor,
                            const GdkRGBA *secondary_cursor)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_color_property (style,
                                          GTK_TYPE_WIDGET,
                                          "cursor-color", cursor);
  _gtk_modifier_style_set_color_property (style,
                                          GTK_TYPE_WIDGET,
                                          "secondary-cursor-color",
                                          secondary_cursor);
}

static void
gtk_widget_real_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  gtk_widget_queue_resize (widget);
}

static void
gtk_widget_real_style_set (GtkWidget *widget,
                           GtkStyle  *previous_style)
{
}

typedef struct {
  GtkWidget *previous_toplevel;
  GdkScreen *previous_screen;
  GdkScreen *new_screen;
} HierarchyChangedInfo;

static void
do_screen_change (GtkWidget *widget,
		  GdkScreen *old_screen,
		  GdkScreen *new_screen)
{
  if (old_screen != new_screen)
    {
      GtkWidgetPrivate *priv = widget->priv;

      if (old_screen)
	{
	  PangoContext *context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
	  if (context)
	    g_object_set_qdata (G_OBJECT (widget), quark_pango_context, NULL);
	}

      _gtk_tooltip_hide (widget);

      if (new_screen && priv->context)
        gtk_style_context_set_screen (priv->context, new_screen);

      g_signal_emit (widget, widget_signals[SCREEN_CHANGED], 0, old_screen);
    }
}

static void
gtk_widget_propagate_hierarchy_changed_recurse (GtkWidget *widget,
						gpointer   client_data)
{
  GtkWidgetPrivate *priv = widget->priv;
  HierarchyChangedInfo *info = client_data;
  gboolean new_anchored = gtk_widget_is_toplevel (widget) ||
                 (priv->parent && priv->parent->priv->anchored);

  if (priv->anchored != new_anchored)
    {
      g_object_ref (widget);

      priv->anchored = new_anchored;

      /* This can only happen with gtk_widget_reparent() */
      if (priv->realized)
        {
          if (new_anchored)
            gtk_widget_connect_frame_clock (widget,
                                            gtk_widget_get_frame_clock (widget));
          else
            gtk_widget_disconnect_frame_clock (widget,
                                               gtk_widget_get_frame_clock (info->previous_toplevel));
        }

      g_signal_emit (widget, widget_signals[HIERARCHY_CHANGED], 0, info->previous_toplevel);
      do_screen_change (widget, info->previous_screen, info->new_screen);

      if (GTK_IS_CONTAINER (widget))
	gtk_container_forall (GTK_CONTAINER (widget),
			      gtk_widget_propagate_hierarchy_changed_recurse,
			      client_data);

      g_object_unref (widget);
    }
}

/**
 * _gtk_widget_propagate_hierarchy_changed:
 * @widget: a #GtkWidget
 * @previous_toplevel: Previous toplevel
 *
 * Propagates changes in the anchored state to a widget and all
 * children, unsetting or setting the %ANCHORED flag, and
 * emitting #GtkWidget::hierarchy-changed.
 **/
void
_gtk_widget_propagate_hierarchy_changed (GtkWidget *widget,
					 GtkWidget *previous_toplevel)
{
  GtkWidgetPrivate *priv = widget->priv;
  HierarchyChangedInfo info;

  info.previous_toplevel = previous_toplevel;
  info.previous_screen = previous_toplevel ? gtk_widget_get_screen (previous_toplevel) : NULL;

  if (gtk_widget_is_toplevel (widget) ||
      (priv->parent && priv->parent->priv->anchored))
    info.new_screen = gtk_widget_get_screen (widget);
  else
    info.new_screen = NULL;

  if (info.previous_screen)
    g_object_ref (info.previous_screen);
  if (previous_toplevel)
    g_object_ref (previous_toplevel);

  gtk_widget_propagate_hierarchy_changed_recurse (widget, &info);

  if (previous_toplevel)
    g_object_unref (previous_toplevel);
  if (info.previous_screen)
    g_object_unref (info.previous_screen);
}

static void
gtk_widget_propagate_screen_changed_recurse (GtkWidget *widget,
					     gpointer   client_data)
{
  HierarchyChangedInfo *info = client_data;

  g_object_ref (widget);

  do_screen_change (widget, info->previous_screen, info->new_screen);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_propagate_screen_changed_recurse,
			  client_data);

  g_object_unref (widget);
}

/**
 * gtk_widget_is_composited:
 * @widget: a #GtkWidget
 *
 * Whether @widget can rely on having its alpha channel
 * drawn correctly. On X11 this function returns whether a
 * compositing manager is running for @widget’s screen.
 *
 * Please note that the semantics of this call will change
 * in the future if used on a widget that has a composited
 * window in its hierarchy (as set by gdk_window_set_composited()).
 *
 * Returns: %TRUE if the widget can rely on its alpha
 * channel being drawn correctly.
 *
 * Since: 2.10
 */
gboolean
gtk_widget_is_composited (GtkWidget *widget)
{
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  screen = gtk_widget_get_screen (widget);

  return gdk_screen_is_composited (screen);
}

static void
propagate_composited_changed (GtkWidget *widget,
			      gpointer dummy)
{
  if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_forall (GTK_CONTAINER (widget),
			    propagate_composited_changed,
			    NULL);
    }

  g_signal_emit (widget, widget_signals[COMPOSITED_CHANGED], 0);
}

void
_gtk_widget_propagate_composited_changed (GtkWidget *widget)
{
  propagate_composited_changed (widget, NULL);
}

/**
 * _gtk_widget_propagate_screen_changed:
 * @widget: a #GtkWidget
 * @previous_screen: Previous screen
 *
 * Propagates changes in the screen for a widget to all
 * children, emitting #GtkWidget::screen-changed.
 **/
void
_gtk_widget_propagate_screen_changed (GtkWidget    *widget,
				      GdkScreen    *previous_screen)
{
  HierarchyChangedInfo info;

  info.previous_screen = previous_screen;
  info.new_screen = gtk_widget_get_screen (widget);

  if (previous_screen)
    g_object_ref (previous_screen);

  gtk_widget_propagate_screen_changed_recurse (widget, &info);

  if (previous_screen)
    g_object_unref (previous_screen);
}

static void
reset_style_recurse (GtkWidget *widget, gpointer data)
{
  _gtk_widget_invalidate_style_context (widget, GTK_CSS_CHANGE_ANY);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  reset_style_recurse,
			  NULL);
}

/**
 * gtk_widget_reset_style:
 * @widget: a #GtkWidget
 *
 * Updates the style context of @widget and all descendents
 * by updating its widget path. #GtkContainers may want
 * to use this on a child when reordering it in a way that a different
 * style might apply to it. See also gtk_container_get_path_for_child().
 *
 * Since: 3.0
 */
void
gtk_widget_reset_style (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  reset_style_recurse (widget, NULL);

  g_list_foreach (widget->priv->attached_windows,
                  (GFunc) reset_style_recurse, NULL);
}

#ifdef G_ENABLE_DEBUG

/* Verify invariants, see docs/widget_system.txt for notes on much of
 * this.  Invariants may be temporarily broken while we’re in the
 * process of updating state, of course, so you can only
 * verify_invariants() after a given operation is complete.
 * Use push/pop_verify_invariants to help with that.
 */
static void
gtk_widget_verify_invariants (GtkWidget *widget)
{
  GtkWidget *parent;

  if (widget->priv->verifying_invariants_count > 0)
    return;

  parent = widget->priv->parent;

  if (widget->priv->mapped)
    {
      /* Mapped implies ... */

      if (!widget->priv->realized)
        g_warning ("%s %p is mapped but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (!widget->priv->visible)
        g_warning ("%s %p is mapped but not visible",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (!widget->priv->toplevel)
        {
          if (!widget->priv->child_visible)
            g_warning ("%s %p is mapped but not child_visible",
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
    }
  else
    {
      /* Not mapped implies... */

#if 0
  /* This check makes sense for normal toplevels, but for
   * something like a toplevel that is embedded within a clutter
   * state, mapping may depend on external factors.
   */
      if (widget->priv->toplevel)
        {
          if (widget->priv->visible)
            g_warning ("%s %p toplevel is visible but not mapped",
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
#endif
    }

  /* Parent related checks aren't possible if parent has
   * verifying_invariants_count > 0 because parent needs to recurse
   * children first before the invariants will hold.
   */
  if (parent == NULL || parent->priv->verifying_invariants_count == 0)
    {
      if (parent &&
          parent->priv->realized)
        {
          /* Parent realized implies... */

#if 0
          /* This is in widget_system.txt but appears to fail
           * because there's no gtk_container_realize() that
           * realizes all children... instead we just lazily
           * wait for map to fix things up.
           */
          if (!widget->priv->realized)
            g_warning ("%s %p is realized but child %s %p is not realized",
                       G_OBJECT_TYPE_NAME (parent), parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
#endif
        }
      else if (!widget->priv->toplevel)
        {
          /* No parent or parent not realized on non-toplevel implies... */

          if (widget->priv->realized && !widget->priv->in_reparent)
            g_warning ("%s %p is not realized but child %s %p is realized",
                       parent ? G_OBJECT_TYPE_NAME (parent) : "no parent", parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
        }

      if (parent &&
          parent->priv->mapped &&
          widget->priv->visible &&
          widget->priv->child_visible)
        {
          /* Parent mapped and we are visible implies... */

          if (!widget->priv->mapped)
            g_warning ("%s %p is mapped but visible child %s %p is not mapped",
                       G_OBJECT_TYPE_NAME (parent), parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
      else if (!widget->priv->toplevel)
        {
          /* No parent or parent not mapped on non-toplevel implies... */

          if (widget->priv->mapped && !widget->priv->in_reparent)
            g_warning ("%s %p is mapped but visible=%d child_visible=%d parent %s %p mapped=%d",
                       G_OBJECT_TYPE_NAME (widget), widget,
                       widget->priv->visible,
                       widget->priv->child_visible,
                       parent ? G_OBJECT_TYPE_NAME (parent) : "no parent", parent,
                       parent ? parent->priv->mapped : FALSE);
        }
    }

  if (!widget->priv->realized)
    {
      /* Not realized implies... */

#if 0
      /* widget_system.txt says these hold, but they don't. */
      if (widget->priv->alloc_needed)
        g_warning ("%s %p alloc needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->width_request_needed)
        g_warning ("%s %p width request needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->height_request_needed)
        g_warning ("%s %p height request needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);
#endif
    }
}

/* The point of this push/pop is that invariants may not hold while
 * we’re busy making changes. So we only check at the outermost call
 * on the call stack, after we finish updating everything.
 */
static void
gtk_widget_push_verify_invariants (GtkWidget *widget)
{
  widget->priv->verifying_invariants_count += 1;
}

static void
gtk_widget_verify_child_invariants (GtkWidget *widget,
                                    gpointer   client_data)
{
  /* We don't recurse further; this is a one-level check. */
  gtk_widget_verify_invariants (widget);
}

static void
gtk_widget_pop_verify_invariants (GtkWidget *widget)
{
  g_assert (widget->priv->verifying_invariants_count > 0);

  widget->priv->verifying_invariants_count -= 1;

  if (widget->priv->verifying_invariants_count == 0)
    {
      gtk_widget_verify_invariants (widget);

      if (GTK_IS_CONTAINER (widget))
        {
          /* Check one level of children, because our
           * push_verify_invariants() will have prevented some of the
           * checks. This does not recurse because if recursion is
           * needed, it will happen naturally as each child has a
           * push/pop on that child. For example if we're recursively
           * mapping children, we'll push/pop on each child as we map
           * it.
           */
          gtk_container_forall (GTK_CONTAINER (widget),
                                gtk_widget_verify_child_invariants,
                                NULL);
        }
    }
}
#endif /* G_ENABLE_DEBUG */

static PangoContext *
gtk_widget_peek_pango_context (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
}

/**
 * gtk_widget_get_pango_context:
 * @widget: a #GtkWidget
 *
 * Gets a #PangoContext with the appropriate font map, font description,
 * and base direction for this widget. Unlike the context returned
 * by gtk_widget_create_pango_context(), this context is owned by
 * the widget (it can be used until the screen for the widget changes
 * or the widget is removed from its toplevel), and will be updated to
 * match any changes to the widget’s attributes. This can be tracked
 * by using the #GtkWidget::screen-changed signal on the widget.
 *
 * Returns: (transfer none): the #PangoContext for the widget.
 **/
PangoContext *
gtk_widget_get_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
  if (!context)
    {
      context = gtk_widget_create_pango_context (GTK_WIDGET (widget));
      g_object_set_qdata_full (G_OBJECT (widget),
			       quark_pango_context,
			       context,
			       g_object_unref);
    }

  return context;
}

static void
update_pango_context (GtkWidget    *widget,
		      PangoContext *context)
{
  PangoFontDescription *font_desc;
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_get (style_context,
                         gtk_widget_get_state_flags (widget),
                         "font", &font_desc,
                         NULL);

  pango_context_set_font_description (context, font_desc);
  pango_context_set_base_dir (context,
			      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
			      PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);

  pango_font_description_free (font_desc);

  pango_cairo_context_set_resolution (context,
                                      _gtk_css_number_value_get (
                                          _gtk_style_context_peek_property (style_context,
                                                                            GTK_CSS_PROPERTY_DPI),
                                          100));
}

static void
gtk_widget_update_pango_context (GtkWidget *widget)
{
  PangoContext *context = gtk_widget_peek_pango_context (widget);

  if (context)
    {
      GdkScreen *screen;

      update_pango_context (widget, context);

      screen = gtk_widget_get_screen_unchecked (widget);
      if (screen)
	{
	  pango_cairo_context_set_font_options (context,
						gdk_screen_get_font_options (screen));
	}
    }
}

/**
 * gtk_widget_create_pango_context:
 * @widget: a #GtkWidget
 *
 * Creates a new #PangoContext with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget. See also gtk_widget_get_pango_context().
 *
 * Returns: (transfer full): the new #PangoContext
 **/
PangoContext *
gtk_widget_create_pango_context (GtkWidget *widget)
{
  GdkScreen *screen;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  screen = gtk_widget_get_screen_unchecked (widget);
  if (!screen)
    {
      GTK_NOTE (MULTIHEAD,
		g_warning ("gtk_widget_create_pango_context ()) called without screen"));

      screen = gdk_screen_get_default ();
    }

  context = gdk_pango_context_get_for_screen (screen);

  update_pango_context (widget, context);
  pango_context_set_language (context, gtk_get_default_language ());

  return context;
}

/**
 * gtk_widget_create_pango_layout:
 * @widget: a #GtkWidget
 * @text: (nullable): text to set on the layout (can be %NULL)
 *
 * Creates a new #PangoLayout with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget.
 *
 * If you keep a #PangoLayout created in this way around, you need
 * to re-create it when the widget #PangoContext is replaced.
 * This can be tracked by using the #GtkWidget::screen-changed signal
 * on the widget.
 *
 * Returns: (transfer full): the new #PangoLayout
 **/
PangoLayout *
gtk_widget_create_pango_layout (GtkWidget   *widget,
				const gchar *text)
{
  PangoLayout *layout;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);

  if (text)
    pango_layout_set_text (layout, text, -1);

  return layout;
}

/**
 * gtk_widget_render_icon_pixbuf:
 * @widget: a #GtkWidget
 * @stock_id: a stock ID
 * @size: (type int): a stock size. A size of (GtkIconSize)-1 means
 *     render at the size of the source and don’t scale (if there are
 *     multiple source sizes, GTK+ picks one of the available sizes).
 *
 * A convenience function that uses the theme engine and style
 * settings for @widget to look up @stock_id and render it to
 * a pixbuf. @stock_id should be a stock icon ID such as
 * #GTK_STOCK_OPEN or #GTK_STOCK_OK. @size should be a size
 * such as #GTK_ICON_SIZE_MENU.
 *
 * The pixels in the returned #GdkPixbuf are shared with the rest of
 * the application and should not be modified. The pixbuf should be freed
 * after use with g_object_unref().
 *
 * Returns: (transfer full): a new pixbuf, or %NULL if the
 *     stock ID wasn’t known
 *
 * Since: 3.0
 *
 * Deprecated: 3.10: Use gtk_icon_theme_load_icon() instead.
 **/
GdkPixbuf*
gtk_widget_render_icon_pixbuf (GtkWidget   *widget,
                               const gchar *stock_id,
                               GtkIconSize  size)
{
  GtkStyleContext *context;
  GtkIconSet *icon_set;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == -1, NULL);

  context = gtk_widget_get_style_context (widget);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  icon_set = gtk_style_context_lookup_icon_set (context, stock_id);

  if (icon_set == NULL)
    return NULL;

  return gtk_icon_set_render_icon_pixbuf (icon_set, context, size);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

/**
 * gtk_widget_set_parent_window:
 * @widget: a #GtkWidget.
 * @parent_window: the new parent window.
 *
 * Sets a non default parent window for @widget.
 *
 * For #GtkWindow classes, setting a @parent_window effects whether
 * the window is a toplevel window or can be embedded into other
 * widgets.
 *
 * For #GtkWindow classes, this needs to be called before the
 * window is realized.
 */
void
gtk_widget_set_parent_window (GtkWidget *widget,
                              GdkWindow *parent_window)
{
  GdkWindow *old_parent_window;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_parent_window = g_object_get_qdata (G_OBJECT (widget),
					  quark_parent_window);

  if (parent_window != old_parent_window)
    {
      gboolean is_plug;

      g_object_set_qdata (G_OBJECT (widget), quark_parent_window,
			  parent_window);
      if (old_parent_window)
	g_object_unref (old_parent_window);
      if (parent_window)
	g_object_ref (parent_window);

      /* Unset toplevel flag when adding a parent window to a widget,
       * this is the primary entry point to allow toplevels to be
       * embeddable.
       */
#ifdef GDK_WINDOWING_X11
      is_plug = GTK_IS_PLUG (widget);
#else
      is_plug = FALSE;
#endif
      if (GTK_IS_WINDOW (widget) && !is_plug)
	_gtk_window_set_is_toplevel (GTK_WINDOW (widget), parent_window == NULL);
    }
}

/**
 * gtk_widget_get_parent_window:
 * @widget: a #GtkWidget.
 *
 * Gets @widget’s parent window.
 *
 * Returns: (transfer none): the parent window of @widget.
 **/
GdkWindow *
gtk_widget_get_parent_window (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GdkWindow *parent_window;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  parent_window = g_object_get_qdata (G_OBJECT (widget), quark_parent_window);

  return (parent_window != NULL) ? parent_window :
	 (priv->parent != NULL) ? priv->parent->priv->window : NULL;
}


/**
 * gtk_widget_set_child_visible:
 * @widget: a #GtkWidget
 * @is_visible: if %TRUE, @widget should be mapped along with its parent.
 *
 * Sets whether @widget should be mapped along with its when its parent
 * is mapped and @widget has been shown with gtk_widget_show().
 *
 * The child visibility can be set for widget before it is added to
 * a container with gtk_widget_set_parent(), to avoid mapping
 * children unnecessary before immediately unmapping them. However
 * it will be reset to its default state of %TRUE when the widget
 * is removed from a container.
 *
 * Note that changing the child visibility of a widget does not
 * queue a resize on the widget. Most of the time, the size of
 * a widget is computed from all visible children, whether or
 * not they are mapped. If this is not the case, the container
 * can queue a resize itself.
 *
 * This function is only useful for container implementations and
 * never should be called by an application.
 **/
void
gtk_widget_set_child_visible (GtkWidget *widget,
			      gboolean   is_visible)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!gtk_widget_is_toplevel (widget));

  priv = widget->priv;

  g_object_ref (widget);
  gtk_widget_verify_invariants (widget);

  if (is_visible)
    priv->child_visible = TRUE;
  else
    {
      GtkWidget *toplevel;

      priv->child_visible = FALSE;

      toplevel = gtk_widget_get_toplevel (widget);
      if (toplevel != widget && gtk_widget_is_toplevel (toplevel))
	_gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);
    }

  if (priv->parent && gtk_widget_get_realized (priv->parent))
    {
      if (gtk_widget_get_mapped (priv->parent) &&
	  priv->child_visible &&
	  gtk_widget_get_visible (widget))
	gtk_widget_map (widget);
      else
	gtk_widget_unmap (widget);
    }

  gtk_widget_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_get_child_visible:
 * @widget: a #GtkWidget
 *
 * Gets the value set with gtk_widget_set_child_visible().
 * If you feel a need to use this function, your code probably
 * needs reorganization.
 *
 * This function is only useful for container implementations and
 * never should be called by an application.
 *
 * Returns: %TRUE if the widget is mapped with the parent.
 **/
gboolean
gtk_widget_get_child_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->child_visible;
}

static GdkScreen *
gtk_widget_get_screen_unchecked (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);

  if (gtk_widget_is_toplevel (toplevel))
    {
      if (GTK_IS_WINDOW (toplevel))
	return gtk_window_get_screen (GTK_WINDOW (toplevel));
      else if (GTK_IS_INVISIBLE (toplevel))
	return gtk_invisible_get_screen (GTK_INVISIBLE (widget));
    }

  return NULL;
}

/**
 * gtk_widget_get_screen:
 * @widget: a #GtkWidget
 *
 * Get the #GdkScreen from the toplevel window associated with
 * this widget. This function can only be called after the widget
 * has been added to a widget hierarchy with a #GtkWindow
 * at the top.
 *
 * In general, you should only create screen specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 *
 * Returns: (transfer none): the #GdkScreen for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkScreen*
gtk_widget_get_screen (GtkWidget *widget)
{
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  screen = gtk_widget_get_screen_unchecked (widget);

  if (screen)
    return screen;
  else
    {
#if 0
      g_warning (G_STRLOC ": Can't get associated screen"
		 " for a widget unless it is inside a toplevel GtkWindow\n"
		 " widget type is %s associated top level type is %s",
		 g_type_name (G_OBJECT_TYPE(G_OBJECT (widget))),
		 g_type_name (G_OBJECT_TYPE(G_OBJECT (toplevel))));
#endif
      return gdk_screen_get_default ();
    }
}

/**
 * gtk_widget_has_screen:
 * @widget: a #GtkWidget
 *
 * Checks whether there is a #GdkScreen is associated with
 * this widget. All toplevel widgets have an associated
 * screen, and all widgets added into a hierarchy with a toplevel
 * window at the top.
 *
 * Returns: %TRUE if there is a #GdkScreen associcated
 *   with the widget.
 *
 * Since: 2.2
 **/
gboolean
gtk_widget_has_screen (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (gtk_widget_get_screen_unchecked (widget) != NULL);
}

void
_gtk_widget_scale_changed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (priv->context)
    gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));

  g_object_notify (G_OBJECT (widget), "scale-factor");

  gtk_widget_queue_draw (widget);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                          (GtkCallback) _gtk_widget_scale_changed,
                          NULL);
}

/**
 * gtk_widget_get_scale_factor:
 * @widget: a #GtkWidget
 *
 * Retrieves the internal scale factor that maps from window coordinates
 * to the actual device pixels. On traditional systems this is 1, on
 * high density outputs, it can be a higher value (typically 2).
 *
 * See gdk_window_get_scale_factor().
 *
 * Returns: the scale factor for @widget
 *
 * Since: 3.10
 */
gint
gtk_widget_get_scale_factor (GtkWidget *widget)
{
  GtkWidget *toplevel;
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 1);

  if (gtk_widget_get_realized (widget))
    return gdk_window_get_scale_factor (gtk_widget_get_window (widget));

  toplevel = gtk_widget_get_toplevel (widget);
  if (toplevel && toplevel != widget)
    return gtk_widget_get_scale_factor (toplevel);

  /* else fall back to something that is more likely to be right than
   * just returning 1:
   */
  screen = gtk_widget_get_screen (widget);
  if (screen)
    return gdk_screen_get_monitor_scale_factor (screen, 0);

  return 1;
}

/**
 * gtk_widget_get_display:
 * @widget: a #GtkWidget
 *
 * Get the #GdkDisplay for the toplevel window associated with
 * this widget. This function can only be called after the widget
 * has been added to a widget hierarchy with a #GtkWindow at the top.
 *
 * In general, you should only create display specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 *
 * Returns: (transfer none): the #GdkDisplay for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkDisplay*
gtk_widget_get_display (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_screen_get_display (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_root_window:
 * @widget: a #GtkWidget
 *
 * Get the root window where this widget is located. This function can
 * only be called after the widget has been added to a widget
 * hierarchy with #GtkWindow at the top.
 *
 * The root window is useful for such purposes as creating a popup
 * #GdkWindow associated with the window. In general, you should only
 * create display specific resources when a widget has been realized,
 * and you should free those resources when the widget is unrealized.
 *
 * Returns: (transfer none): the #GdkWindow root window for the toplevel for this widget.
 *
 * Since: 2.2
 *
 * Deprecated: 3.12: Use gdk_screen_get_root_window() instead
 */
GdkWindow*
gtk_widget_get_root_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_screen_get_root_window (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_child_focus:
 * @widget: a #GtkWidget
 * @direction: direction of focus movement
 *
 * This function is used by custom widget implementations; if you're
 * writing an app, you’d use gtk_widget_grab_focus() to move the focus
 * to a particular widget, and gtk_container_set_focus_chain() to
 * change the focus tab order. So you may want to investigate those
 * functions instead.
 *
 * gtk_widget_child_focus() is called by containers as the user moves
 * around the window using keyboard shortcuts. @direction indicates
 * what kind of motion is taking place (up, down, left, right, tab
 * forward, tab backward). gtk_widget_child_focus() emits the
 * #GtkWidget::focus signal; widgets override the default handler
 * for this signal in order to implement appropriate focus behavior.
 *
 * The default ::focus handler for a widget should return %TRUE if
 * moving in @direction left the focus on a focusable location inside
 * that widget, and %FALSE if moving in @direction moved the focus
 * outside the widget. If returning %TRUE, widgets normally
 * call gtk_widget_grab_focus() to place the focus accordingly;
 * if returning %FALSE, they don’t modify the current focus location.
 *
 * Returns: %TRUE if focus ended up inside @widget
 **/
gboolean
gtk_widget_child_focus (GtkWidget       *widget,
                        GtkDirectionType direction)
{
  gboolean return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!gtk_widget_get_visible (widget) ||
      !gtk_widget_is_sensitive (widget))
    return FALSE;

  /* child widgets must set CAN_FOCUS, containers
   * don't have to though.
   */
  if (!GTK_IS_CONTAINER (widget) &&
      !gtk_widget_get_can_focus (widget))
    return FALSE;

  g_signal_emit (widget,
		 widget_signals[FOCUS],
		 0,
		 direction, &return_val);

  return return_val;
}

/**
 * gtk_widget_keynav_failed:
 * @widget: a #GtkWidget
 * @direction: direction of focus movement
 *
 * This function should be called whenever keyboard navigation within
 * a single widget hits a boundary. The function emits the
 * #GtkWidget::keynav-failed signal on the widget and its return
 * value should be interpreted in a way similar to the return value of
 * gtk_widget_child_focus():
 *
 * When %TRUE is returned, stay in the widget, the failed keyboard
 * navigation is Ok and/or there is nowhere we can/should move the
 * focus to.
 *
 * When %FALSE is returned, the caller should continue with keyboard
 * navigation outside the widget, e.g. by calling
 * gtk_widget_child_focus() on the widget’s toplevel.
 *
 * The default ::keynav-failed handler returns %TRUE for
 * %GTK_DIR_TAB_FORWARD and %GTK_DIR_TAB_BACKWARD. For the other
 * values of #GtkDirectionType it returns %FALSE.
 *
 * Whenever the default handler returns %TRUE, it also calls
 * gtk_widget_error_bell() to notify the user of the failed keyboard
 * navigation.
 *
 * A use case for providing an own implementation of ::keynav-failed
 * (either by connecting to it or by overriding it) would be a row of
 * #GtkEntry widgets where the user should be able to navigate the
 * entire row with the cursor keys, as e.g. known from user interfaces
 * that require entering license keys.
 *
 * Returns: %TRUE if stopping keyboard navigation is fine, %FALSE
 *               if the emitting widget should try to handle the keyboard
 *               navigation attempt in its parent container(s).
 *
 * Since: 2.12
 **/
gboolean
gtk_widget_keynav_failed (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
  gboolean return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  g_signal_emit (widget, widget_signals[KEYNAV_FAILED], 0,
		 direction, &return_val);

  return return_val;
}

/**
 * gtk_widget_error_bell:
 * @widget: a #GtkWidget
 *
 * Notifies the user about an input-related error on this widget.
 * If the #GtkSettings:gtk-error-bell setting is %TRUE, it calls
 * gdk_window_beep(), otherwise it does nothing.
 *
 * Note that the effect of gdk_window_beep() can be configured in many
 * ways, depending on the windowing backend and the desktop environment
 * or window manager that is used.
 *
 * Since: 2.12
 **/
void
gtk_widget_error_bell (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GtkSettings* settings;
  gboolean beep;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  settings = gtk_widget_get_settings (widget);
  if (!settings)
    return;

  g_object_get (settings,
                "gtk-error-bell", &beep,
                NULL);

  if (beep && priv->window)
    gdk_window_beep (priv->window);
}

static void
gtk_widget_set_usize_internal (GtkWidget          *widget,
			       gint                width,
			       gint                height,
			       GtkQueueResizeFlags flags)
{
  GtkWidgetAuxInfo *aux_info;
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (widget));

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (width > -2 && aux_info->width != width)
    {
      if ((flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY) == 0)
	g_object_notify (G_OBJECT (widget), "width-request");
      aux_info->width = width;
      changed = TRUE;
    }
  if (height > -2 && aux_info->height != height)
    {
      if ((flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY) == 0)
	g_object_notify (G_OBJECT (widget), "height-request");
      aux_info->height = height;
      changed = TRUE;
    }

  if (gtk_widget_get_visible (widget) && changed)
    {
      if ((flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY) == 0)
	gtk_widget_queue_resize (widget);
      else
	_gtk_size_group_queue_resize (widget, GTK_QUEUE_RESIZE_INVALIDATE_ONLY);
    }

  g_object_thaw_notify (G_OBJECT (widget));
}

/**
 * gtk_widget_set_size_request:
 * @widget: a #GtkWidget
 * @width: width @widget should request, or -1 to unset
 * @height: height @widget should request, or -1 to unset
 *
 * Sets the minimum size of a widget; that is, the widget’s size
 * request will be at least @width by @height. You can use this 
 * function to force a widget to be larger than it normally would be.
 *
 * In most cases, gtk_window_set_default_size() is a better choice for
 * toplevel windows than this function; setting the default size will
 * still allow users to shrink the window. Setting the size request
 * will force them to leave the window at least as large as the size
 * request. When dealing with window sizes,
 * gtk_window_set_geometry_hints() can be a useful function as well.
 *
 * Note the inherent danger of setting any fixed size - themes,
 * translations into other languages, different fonts, and user action
 * can all change the appropriate size for a given widget. So, it's
 * basically impossible to hardcode a size that will always be
 * correct.
 *
 * The size request of a widget is the smallest size a widget can
 * accept while still functioning well and drawing itself correctly.
 * However in some strange cases a widget may be allocated less than
 * its requested size, and in many cases a widget may be allocated more
 * space than it requested.
 *
 * If the size request in a given direction is -1 (unset), then
 * the “natural” size request of the widget will be used instead.
 *
 * The size request set here does not include any margin from the
 * #GtkWidget properties margin-left, margin-right, margin-top, and
 * margin-bottom, but it does include pretty much all other padding
 * or border properties set by any subclass of #GtkWidget.
 **/
void
gtk_widget_set_size_request (GtkWidget *widget,
                             gint       width,
                             gint       height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  if (width == 0)
    width = 1;
  if (height == 0)
    height = 1;

  gtk_widget_set_usize_internal (widget, width, height, 0);
}


/**
 * gtk_widget_get_size_request:
 * @widget: a #GtkWidget
 * @width: (out) (allow-none): return location for width, or %NULL
 * @height: (out) (allow-none): return location for height, or %NULL
 *
 * Gets the size request that was explicitly set for the widget using
 * gtk_widget_set_size_request(). A value of -1 stored in @width or
 * @height indicates that that dimension has not been set explicitly
 * and the natural requisition of the widget will be used intead. See
 * gtk_widget_set_size_request(). To get the size a widget will
 * actually request, call gtk_widget_get_preferred_size() instead of
 * this function.
 **/
void
gtk_widget_get_size_request (GtkWidget *widget,
                             gint      *width,
                             gint      *height)
{
  const GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (width)
    *width = aux_info->width;

  if (height)
    *height = aux_info->height;
}

/**
 * _gtk_widget_override_size_request:
 * @widget: a #GtkWidget
 * @width: new forced minimum width
 * @height: new forced minimum height
 * @old_width: location to store previous forced minimum width
 * @old_height: location to store previous forced minumum height
 *
 * Temporarily establishes a forced minimum size for a widget; this
 * is used by GtkWindow when calculating the size to add to the
 * window’s geometry widget. Cached sizes for the widget and its
 * parents are invalidated, so that subsequent calls to the size
 * negotiation machinery produce the overriden result, but the
 * widget is not queued for relayout or redraw. The old size must
 * be restored with _gtk_widget_restore_size_request() or things
 * will go screwy.
 */
void
_gtk_widget_override_size_request (GtkWidget *widget,
				   int        width,
				   int        height,
				   int       *old_width,
				   int       *old_height)
{
  gtk_widget_get_size_request (widget, old_width, old_height);
  gtk_widget_set_usize_internal (widget, width, height,
				 GTK_QUEUE_RESIZE_INVALIDATE_ONLY);
}

/**
 * _gtk_widget_restore_size_request:
 * @widget: a #GtkWidget
 * @old_width: saved forced minimum size
 * @old_height: saved forced minimum size
 *
 * Undoes the operation of_gtk_widget_override_size_request().
 */
void
_gtk_widget_restore_size_request (GtkWidget *widget,
				  int        old_width,
				  int        old_height)
{
  gtk_widget_set_usize_internal (widget, old_width, old_height,
				 GTK_QUEUE_RESIZE_INVALIDATE_ONLY);
}

/**
 * gtk_widget_set_events:
 * @widget: a #GtkWidget
 * @events: event mask
 *
 * Sets the event mask (see #GdkEventMask) for a widget. The event
 * mask determines which events a widget will receive. Keep in mind
 * that different widgets have different default event masks, and by
 * changing the event mask you may disrupt a widget’s functionality,
 * so be careful. This function must be called while a widget is
 * unrealized. Consider gtk_widget_add_events() for widgets that are
 * already realized, or if you want to preserve the existing event
 * mask. This function can’t be used with widgets that have no window.
 * (See gtk_widget_get_has_window()).  To get events on those widgets,
 * place them inside a #GtkEventBox and receive events on the event
 * box.
 **/
void
gtk_widget_set_events (GtkWidget *widget,
		       gint	  events)
{
  gint e;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!gtk_widget_get_realized (widget));

  e = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), quark_event_mask));
  if (e != events)
    {
      g_object_set_qdata (G_OBJECT (widget), quark_event_mask,
                          GINT_TO_POINTER (events));
      g_object_notify (G_OBJECT (widget), "events");
    }
}

/**
 * gtk_widget_set_device_events:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @events: event mask
 *
 * Sets the device event mask (see #GdkEventMask) for a widget. The event
 * mask determines which events a widget will receive from @device. Keep
 * in mind that different widgets have different default event masks, and by
 * changing the event mask you may disrupt a widget’s functionality,
 * so be careful. This function must be called while a widget is
 * unrealized. Consider gtk_widget_add_device_events() for widgets that are
 * already realized, or if you want to preserve the existing event
 * mask. This function can’t be used with windowless widgets (which return
 * %FALSE from gtk_widget_get_has_window());
 * to get events on those widgets, place them inside a #GtkEventBox
 * and receive events on the event box.
 *
 * Since: 3.0
 **/
void
gtk_widget_set_device_events (GtkWidget    *widget,
                              GdkDevice    *device,
                              GdkEventMask  events)
{
  GHashTable *device_events;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (!gtk_widget_get_realized (widget));

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (G_UNLIKELY (!device_events))
    {
      device_events = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (widget), quark_device_event_mask, device_events,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_insert (device_events, device, GUINT_TO_POINTER (events));
}

/**
 * gtk_widget_set_device_enabled:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @enabled: whether to enable the device
 *
 * Enables or disables a #GdkDevice to interact with @widget
 * and all its children.
 *
 * It does so by descending through the #GdkWindow hierarchy
 * and enabling the same mask that is has for core events
 * (i.e. the one that gdk_window_get_events() returns).
 *
 * Since: 3.0
 */
void
gtk_widget_set_device_enabled (GtkWidget *widget,
                               GdkDevice *device,
                               gboolean   enabled)
{
  GList *enabled_devices;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));

  enabled_devices = g_object_get_qdata (G_OBJECT (widget), quark_enabled_devices);
  enabled_devices = g_list_append (enabled_devices, device);

  g_object_set_qdata_full (G_OBJECT (widget), quark_enabled_devices,
                           enabled_devices, (GDestroyNotify) g_list_free);;

  if (gtk_widget_get_realized (widget))
    gtk_widget_set_device_enabled_internal (widget, device, TRUE, enabled);
}

/**
 * gtk_widget_get_device_enabled:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns whether @device can interact with @widget and its
 * children. See gtk_widget_set_device_enabled().
 *
 * Returns: %TRUE is @device is enabled for @widget
 *
 * Since: 3.0
 */
gboolean
gtk_widget_get_device_enabled (GtkWidget *widget,
                               GdkDevice *device)
{
  GList *enabled_devices;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  enabled_devices = g_object_get_qdata (G_OBJECT (widget), quark_enabled_devices);

  return g_list_find (enabled_devices, device) != NULL;
}

static void
gtk_widget_add_events_internal_list (GtkWidget *widget,
                                     GdkDevice *device,
                                     gint       events,
                                     GList     *window_list)
{
  GdkEventMask controllers_mask;
  GList *l;

  controllers_mask = _gtk_widget_get_controllers_evmask (widget);

  for (l = window_list; l != NULL; l = l->next)
    {
      GdkWindow *window = l->data;
      gpointer user_data;

      gdk_window_get_user_data (window, &user_data);
      if (user_data == widget)
        {
          GList *children;

          if (device)
            {
              gdk_window_set_device_events (window, device,
                                            gdk_window_get_events (window) |
                                            events | controllers_mask);
            }
          else
            {
              gdk_window_set_events (window, gdk_window_get_events (window) |
                                     events | controllers_mask);
            }

          children = gdk_window_get_children (window);
          gtk_widget_add_events_internal_list (widget, device, events, children);
          g_list_free (children);
        }
    }
}

static void
gtk_widget_add_events_internal (GtkWidget *widget,
                                GdkDevice *device,
                                gint       events)
{
  GtkWidgetPrivate *priv = widget->priv;
  GList *window_list;

  if (!gtk_widget_get_has_window (widget))
    window_list = gdk_window_get_children (priv->window);
  else
    window_list = g_list_prepend (NULL, priv->window);

  gtk_widget_add_events_internal_list (widget, device, events, window_list);

  g_list_free (window_list);
}

/**
 * gtk_widget_add_events:
 * @widget: a #GtkWidget
 * @events: an event mask, see #GdkEventMask
 *
 * Adds the events in the bitfield @events to the event mask for
 * @widget. See gtk_widget_set_events() and the
 * [input handling overview][event-masks] for details.
 **/
void
gtk_widget_add_events (GtkWidget *widget,
		       gint	  events)
{
  gint old_events;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_events = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), quark_event_mask));
  g_object_set_qdata (G_OBJECT (widget), quark_event_mask,
                      GINT_TO_POINTER (old_events | events));

  if (gtk_widget_get_realized (widget))
    {
      gtk_widget_add_events_internal (widget, NULL, events);
      gtk_widget_update_devices_mask (widget, FALSE);
    }

  g_object_notify (G_OBJECT (widget), "events");
}

/**
 * gtk_widget_add_device_events:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @events: an event mask, see #GdkEventMask
 *
 * Adds the device events in the bitfield @events to the event mask for
 * @widget. See gtk_widget_set_device_events() for details.
 *
 * Since: 3.0
 **/
void
gtk_widget_add_device_events (GtkWidget    *widget,
                              GdkDevice    *device,
                              GdkEventMask  events)
{
  GdkEventMask old_events;
  GHashTable *device_events;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));

  old_events = gtk_widget_get_device_events (widget, device);

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (G_UNLIKELY (!device_events))
    {
      device_events = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (widget), quark_device_event_mask, device_events,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_insert (device_events, device,
                       GUINT_TO_POINTER (old_events | events));

  if (gtk_widget_get_realized (widget))
    gtk_widget_add_events_internal (widget, device, events);

  g_object_notify (G_OBJECT (widget), "events");
}

/**
 * gtk_widget_get_toplevel:
 * @widget: a #GtkWidget
 *
 * This function returns the topmost widget in the container hierarchy
 * @widget is a part of. If @widget has no parent widgets, it will be
 * returned as the topmost widget. No reference will be added to the
 * returned widget; it should not be unreferenced.
 *
 * Note the difference in behavior vs. gtk_widget_get_ancestor();
 * `gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW)`
 * would return
 * %NULL if @widget wasn’t inside a toplevel window, and if the
 * window was inside a #GtkWindow-derived widget which was in turn
 * inside the toplevel #GtkWindow. While the second case may
 * seem unlikely, it actually happens when a #GtkPlug is embedded
 * inside a #GtkSocket within the same application.
 *
 * To reliably find the toplevel #GtkWindow, use
 * gtk_widget_get_toplevel() and call gtk_widget_is_toplevel()
 * on the result.
 * |[<!-- language="C" -->
 *  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
 *  if (gtk_widget_is_toplevel (toplevel))
 *    {
 *      // Perform action on toplevel.
 *    }
 * ]|
 *
 * Returns: (transfer none): the topmost ancestor of @widget, or @widget itself
 *    if there’s no ancestor.
 **/
GtkWidget*
gtk_widget_get_toplevel (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  while (widget->priv->parent)
    widget = widget->priv->parent;

  return widget;
}

/**
 * gtk_widget_get_ancestor:
 * @widget: a #GtkWidget
 * @widget_type: ancestor type
 *
 * Gets the first ancestor of @widget with type @widget_type. For example,
 * `gtk_widget_get_ancestor (widget, GTK_TYPE_BOX)` gets
 * the first #GtkBox that’s an ancestor of @widget. No reference will be
 * added to the returned widget; it should not be unreferenced. See note
 * about checking for a toplevel #GtkWindow in the docs for
 * gtk_widget_get_toplevel().
 *
 * Note that unlike gtk_widget_is_ancestor(), gtk_widget_get_ancestor()
 * considers @widget to be an ancestor of itself.
 *
 * Returns: (transfer none): the ancestor widget, or %NULL if not found
 **/
GtkWidget*
gtk_widget_get_ancestor (GtkWidget *widget,
			 GType      widget_type)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  while (widget && !g_type_is_a (G_OBJECT_TYPE (widget), widget_type))
    widget = widget->priv->parent;

  if (!(widget && g_type_is_a (G_OBJECT_TYPE (widget), widget_type)))
    return NULL;

  return widget;
}

/**
 * gtk_widget_set_visual:
 * @widget: a #GtkWidget
 * @visual: (allow-none): visual to be used or %NULL to unset a previous one
 *
 * Sets the visual that should be used for by widget and its children for
 * creating #GdkWindows. The visual must be on the same #GdkScreen as
 * returned by gtk_widget_get_screen(), so handling the
 * #GtkWidget::screen-changed signal is necessary.
 *
 * Setting a new @visual will not cause @widget to recreate its windows,
 * so you should call this function before @widget is realized.
 **/
void
gtk_widget_set_visual (GtkWidget *widget,
                       GdkVisual *visual)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (visual == NULL || GDK_IS_VISUAL (visual));

  if (visual)
    g_return_if_fail (gtk_widget_get_screen (widget) == gdk_visual_get_screen (visual));

  g_object_set_qdata_full (G_OBJECT (widget),
                           quark_visual,
                           visual ? g_object_ref (visual) : NULL,
                           g_object_unref);
}

/**
 * gtk_widget_get_visual:
 * @widget: a #GtkWidget
 *
 * Gets the visual that will be used to render @widget.
 *
 * Returns: (transfer none): the visual for @widget
 **/
GdkVisual*
gtk_widget_get_visual (GtkWidget *widget)
{
  GtkWidget *w;
  GdkVisual *visual;
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (gtk_widget_get_has_window (widget) &&
      widget->priv->window)
    return gdk_window_get_visual (widget->priv->window);

  screen = gtk_widget_get_screen (widget);

  for (w = widget; w != NULL; w = w->priv->parent)
    {
      visual = g_object_get_qdata (G_OBJECT (w), quark_visual);
      if (visual)
        {
          if (gdk_visual_get_screen (visual) == screen)
            return visual;

          g_warning ("Ignoring visual set on widget `%s' that is not on the correct screen.",
                     gtk_widget_get_name (widget));
        }
    }

  return gdk_screen_get_system_visual (screen);
}

/**
 * gtk_widget_get_settings:
 * @widget: a #GtkWidget
 *
 * Gets the settings object holding the settings used for this widget.
 *
 * Note that this function can only be called when the #GtkWidget
 * is attached to a toplevel, since the settings object is specific
 * to a particular #GdkScreen.
 *
 * Returns: (transfer none): the relevant #GtkSettings object
 */
GtkSettings*
gtk_widget_get_settings (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_events:
 * @widget: a #GtkWidget
 *
 * Returns the event mask (see #GdkEventMask) for the widget. These are the
 * events that the widget will receive.
 *
 * Note: Internally, the widget event mask will be the logical OR of the event
 * mask set through gtk_widget_set_events() or gtk_widget_add_events(), and the
 * event mask necessary to cater for every #GtkEventController created for the
 * widget.
 *
 * Returns: event mask for @widget
 **/
gint
gtk_widget_get_events (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), quark_event_mask)) |
    _gtk_widget_get_controllers_evmask (widget);
}

/**
 * gtk_widget_get_device_events:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns the events mask for the widget corresponding to an specific device. These
 * are the events that the widget will receive when @device operates on it.
 *
 * Returns: device event mask for @widget
 *
 * Since: 3.0
 **/
GdkEventMask
gtk_widget_get_device_events (GtkWidget *widget,
                              GdkDevice *device)
{
  GHashTable *device_events;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (!device_events)
    return 0;

  return GPOINTER_TO_UINT (g_hash_table_lookup (device_events, device));
}

/**
 * gtk_widget_get_pointer:
 * @widget: a #GtkWidget
 * @x: (out) (allow-none): return location for the X coordinate, or %NULL
 * @y: (out) (allow-none): return location for the Y coordinate, or %NULL
 *
 * Obtains the location of the mouse pointer in widget coordinates.
 * Widget coordinates are a bit odd; for historical reasons, they are
 * defined as @widget->window coordinates for widgets that return %TRUE for
 * gtk_widget_get_has_window(); and are relative to @widget->allocation.x,
 * @widget->allocation.y otherwise.
 *
 * Deprecated: 3.4: Use gdk_window_get_device_position() instead.
 **/
void
gtk_widget_get_pointer (GtkWidget *widget,
			gint	  *x,
			gint	  *y)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (x)
    *x = -1;
  if (y)
    *y = -1;

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_get_device_position (priv->window,
                                      gdk_device_manager_get_client_pointer (
                                        gdk_display_get_device_manager (
                                          gtk_widget_get_display (widget))),
                                      x, y, NULL);

      if (!gtk_widget_get_has_window (widget))
	{
	  if (x)
	    *x -= priv->allocation.x;
	  if (y)
	    *y -= priv->allocation.y;
	}
    }
}

/**
 * gtk_widget_is_ancestor:
 * @widget: a #GtkWidget
 * @ancestor: another #GtkWidget
 *
 * Determines whether @widget is somewhere inside @ancestor, possibly with
 * intermediate containers.
 *
 * Returns: %TRUE if @ancestor contains @widget as a child,
 *    grandchild, great grandchild, etc.
 **/
gboolean
gtk_widget_is_ancestor (GtkWidget *widget,
			GtkWidget *ancestor)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  while (widget)
    {
      if (widget->priv->parent == ancestor)
	return TRUE;
      widget = widget->priv->parent;
    }

  return FALSE;
}

static GQuark quark_composite_name = 0;

/**
 * gtk_widget_set_composite_name:
 * @widget: a #GtkWidget.
 * @name: the name to set
 *
 * Sets a widgets composite name. The widget must be
 * a composite child of its parent; see gtk_widget_push_composite_child().
 *
 * Deprecated: 3.10: Use gtk_widget_class_set_template(), or don’t use this API at all.
 **/
void
gtk_widget_set_composite_name (GtkWidget   *widget,
			       const gchar *name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->priv->composite_child);
  g_return_if_fail (name != NULL);

  if (!quark_composite_name)
    quark_composite_name = g_quark_from_static_string ("gtk-composite-name");

  g_object_set_qdata_full (G_OBJECT (widget),
			   quark_composite_name,
			   g_strdup (name),
			   g_free);
}

/**
 * gtk_widget_get_composite_name:
 * @widget: a #GtkWidget
 *
 * Obtains the composite name of a widget.
 *
 * Returns: the composite name of @widget, or %NULL if @widget is not
 *   a composite child. The string should be freed when it is no
 *   longer needed.
 *
 * Deprecated: 3.10: Use gtk_widget_class_set_template(), or don’t use this API at all.
 **/
gchar*
gtk_widget_get_composite_name (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  if (widget->priv->composite_child && priv->parent)
    return _gtk_container_child_composite_name (GTK_CONTAINER (priv->parent),
					       widget);
  else
    return NULL;
}

/**
 * gtk_widget_push_composite_child:
 *
 * Makes all newly-created widgets as composite children until
 * the corresponding gtk_widget_pop_composite_child() call.
 *
 * A composite child is a child that’s an implementation detail of the
 * container it’s inside and should not be visible to people using the
 * container. Composite children aren’t treated differently by GTK (but
 * see gtk_container_foreach() vs. gtk_container_forall()), but e.g. GUI
 * builders might want to treat them in a different way.
 *
 * Deprecated: 3.10: This API never really worked well and was mostly unused, now
 * we have a more complete mechanism for composite children, see gtk_widget_class_set_template().
 **/
void
gtk_widget_push_composite_child (void)
{
  composite_child_stack++;
}

/**
 * gtk_widget_pop_composite_child:
 *
 * Cancels the effect of a previous call to gtk_widget_push_composite_child().
 *
 * Deprecated: 3.10: Use gtk_widget_class_set_template(), or don’t use this API at all.
 **/
void
gtk_widget_pop_composite_child (void)
{
  if (composite_child_stack)
    composite_child_stack--;
}

static void
gtk_widget_emit_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  old_dir)
{
  GtkTextDirection direction;
  GtkStateFlags state;

  gtk_widget_update_pango_context (widget);

  direction = gtk_widget_get_direction (widget);

  switch (direction)
    {
    case GTK_TEXT_DIR_LTR:
      state = GTK_STATE_FLAG_DIR_LTR;
      break;

    case GTK_TEXT_DIR_RTL:
      state = GTK_STATE_FLAG_DIR_RTL;
      break;

    case GTK_TEXT_DIR_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  gtk_widget_update_state_flags (widget,
                                 state,
                                 state ^ (GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL));

  g_signal_emit (widget, widget_signals[DIRECTION_CHANGED], 0, old_dir);
}

/**
 * gtk_widget_set_direction:
 * @widget: a #GtkWidget
 * @dir:    the new direction
 *
 * Sets the reading direction on a particular widget. This direction
 * controls the primary direction for widgets containing text,
 * and also the direction in which the children of a container are
 * packed. The ability to set the direction is present in order
 * so that correct localization into languages with right-to-left
 * reading directions can be done. Generally, applications will
 * let the default reading direction present, except for containers
 * where the containers are arranged in an order that is explicitly
 * visual rather than logical (such as buttons for text justification).
 *
 * If the direction is set to %GTK_TEXT_DIR_NONE, then the value
 * set by gtk_widget_set_default_direction() will be used.
 **/
void
gtk_widget_set_direction (GtkWidget        *widget,
                          GtkTextDirection  dir)
{
  GtkTextDirection old_dir;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (dir >= GTK_TEXT_DIR_NONE && dir <= GTK_TEXT_DIR_RTL);

  old_dir = gtk_widget_get_direction (widget);

  widget->priv->direction = dir;

  if (old_dir != gtk_widget_get_direction (widget))
    gtk_widget_emit_direction_changed (widget, old_dir);
}

/**
 * gtk_widget_get_direction:
 * @widget: a #GtkWidget
 *
 * Gets the reading direction for a particular widget. See
 * gtk_widget_set_direction().
 *
 * Returns: the reading direction for the widget.
 **/
GtkTextDirection
gtk_widget_get_direction (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_TEXT_DIR_LTR);

  if (widget->priv->direction == GTK_TEXT_DIR_NONE)
    return gtk_default_direction;
  else
    return widget->priv->direction;
}

static void
gtk_widget_set_default_direction_recurse (GtkWidget *widget, gpointer data)
{
  GtkTextDirection old_dir = GPOINTER_TO_UINT (data);

  g_object_ref (widget);

  if (widget->priv->direction == GTK_TEXT_DIR_NONE)
    gtk_widget_emit_direction_changed (widget, old_dir);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_set_default_direction_recurse,
			  data);

  g_object_unref (widget);
}

/**
 * gtk_widget_set_default_direction:
 * @dir: the new default direction. This cannot be
 *        %GTK_TEXT_DIR_NONE.
 *
 * Sets the default reading direction for widgets where the
 * direction has not been explicitly set by gtk_widget_set_direction().
 **/
void
gtk_widget_set_default_direction (GtkTextDirection dir)
{
  g_return_if_fail (dir == GTK_TEXT_DIR_RTL || dir == GTK_TEXT_DIR_LTR);

  if (dir != gtk_default_direction)
    {
      GList *toplevels, *tmp_list;
      GtkTextDirection old_dir = gtk_default_direction;

      gtk_default_direction = dir;

      tmp_list = toplevels = gtk_window_list_toplevels ();
      g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);

      while (tmp_list)
	{
	  gtk_widget_set_default_direction_recurse (tmp_list->data,
						    GUINT_TO_POINTER (old_dir));
	  g_object_unref (tmp_list->data);
	  tmp_list = tmp_list->next;
	}

      g_list_free (toplevels);
    }
}

/**
 * gtk_widget_get_default_direction:
 *
 * Obtains the current default reading direction. See
 * gtk_widget_set_default_direction().
 *
 * Returns: the current default direction.
 **/
GtkTextDirection
gtk_widget_get_default_direction (void)
{
  return gtk_default_direction;
}

static void
gtk_widget_constructed (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;

  /* As strange as it may seem, this may happen on object construction.
   * init() implementations of parent types may eventually call this function,
   * each with its corresponding GType, which could leave a child
   * implementation with a wrong widget type in the widget path
   */
  if (priv->path &&
      G_OBJECT_TYPE (widget) != gtk_widget_path_get_object_type (priv->path))
    {
      gtk_widget_path_free (priv->path);
      priv->path = NULL;
    }

  G_OBJECT_CLASS (gtk_widget_parent_class)->constructed (object);
}

static void
gtk_widget_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;

  if (priv->parent)
    gtk_container_remove (GTK_CONTAINER (priv->parent), widget);
  else if (gtk_widget_get_visible (widget))
    gtk_widget_hide (widget);

  priv->visible = FALSE;
  if (gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  if (!priv->in_destruction)
    {
      priv->in_destruction = TRUE;
      g_signal_emit (object, widget_signals[DESTROY], 0);
      priv->in_destruction = FALSE;
    }

  g_clear_object (&priv->muxer);

  while (priv->attached_windows)
    gtk_window_set_attached_to (priv->attached_windows->data, NULL);

  G_OBJECT_CLASS (gtk_widget_parent_class)->dispose (object);
}

#ifdef G_ENABLE_DEBUG
typedef struct {
  AutomaticChildClass *child_class;
  GType                widget_type;
  GObject             *object;
  gboolean             did_finalize;
} FinalizeAssertion;

static void
finalize_assertion_weak_ref (gpointer data,
			     GObject *where_the_object_was)
{
  FinalizeAssertion *assertion = (FinalizeAssertion *)data;
  assertion->did_finalize = TRUE;
}

static FinalizeAssertion *
finalize_assertion_new (GtkWidget           *widget,
			GType                widget_type,
			AutomaticChildClass *child_class)
{
  FinalizeAssertion *assertion = NULL;
  GObject           *object;

  object = gtk_widget_get_template_child (widget, widget_type, child_class->name);

  /* We control the hash table entry, the object should never be NULL
   */
  g_assert (object);
  if (!G_IS_OBJECT (object))
    g_critical ("Automated component `%s' of class `%s' seems to have been prematurely finalized",
		child_class->name, g_type_name (widget_type));
  else
    {
      assertion = g_slice_new0 (FinalizeAssertion);
      assertion->child_class = child_class;
      assertion->widget_type = widget_type;
      assertion->object = object;

      g_object_weak_ref (object, finalize_assertion_weak_ref, assertion);
    }

  return assertion;
}

static GSList *
build_finalize_assertion_list (GtkWidget *widget)
{
  GType class_type;
  GtkWidgetClass *class;
  GSList *l, *list = NULL;

  for (class = GTK_WIDGET_GET_CLASS (widget);
       GTK_IS_WIDGET_CLASS (class);
       class = g_type_class_peek_parent (class))
    {
      if (!class->priv->template)
	continue;

      class_type = G_OBJECT_CLASS_TYPE (class);

      for (l = class->priv->template->children; l; l = l->next)
	{
	  AutomaticChildClass *child_class = l->data;
	  FinalizeAssertion *assertion;

	  assertion = finalize_assertion_new (widget, class_type, child_class);
	  list = g_slist_prepend (list, assertion);
	}
    }

  return list;
}
#endif /* G_ENABLE_DEBUG */

static void
gtk_widget_real_destroy (GtkWidget *object)
{
  /* gtk_object_destroy() will already hold a refcount on object */
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;
  GList *l;

  if (priv->auto_children)
    {
      GtkWidgetClass *class;
      GSList *l;

#ifdef G_ENABLE_DEBUG
      GSList *assertions = NULL;

      /* Note, GTK_WIDGET_ASSERT_COMPONENTS is very useful
       * to catch ref counting bugs, but can only be used in
       * test cases which simply create and destroy a composite
       * widget.
       *
       * This is because some API can expose components explicitly,
       * and so we cannot assert that a component is expected to finalize
       * in a full application ecosystem.
       */
      if (g_getenv ("GTK_WIDGET_ASSERT_COMPONENTS") != NULL)
	assertions = build_finalize_assertion_list (widget);
#endif /* G_ENABLE_DEBUG */

      /* Release references to all automated children */
      g_hash_table_destroy (priv->auto_children);
      priv->auto_children = NULL;

#ifdef G_ENABLE_DEBUG
      for (l = assertions; l; l = l->next)
	{
	  FinalizeAssertion *assertion = l->data;

	  if (!assertion->did_finalize)
	    g_critical ("Automated component `%s' of class `%s' did not finalize in gtk_widget_destroy(). "
			"Current reference count is %d",
			assertion->child_class->name,
			g_type_name (assertion->widget_type),
			assertion->object->ref_count);

	  g_slice_free (FinalizeAssertion, assertion);
	}
      g_slist_free (assertions);
#endif /* G_ENABLE_DEBUG */

      /* Set any automatic private data pointers to NULL */
      for (class = GTK_WIDGET_GET_CLASS (widget);
	   GTK_IS_WIDGET_CLASS (class);
	   class = g_type_class_peek_parent (class))
	{
	  if (!class->priv->template)
	    continue;

	  for (l = class->priv->template->children; l; l = l->next)
	    {
	      AutomaticChildClass *child_class = l->data;

	      if (child_class->offset != 0)
		{
		  gpointer field_p;

		  /* Nullify instance private data for internal children */
		  field_p = G_STRUCT_MEMBER_P (widget, child_class->offset);
		  (* (gpointer *) field_p) = NULL;
		}
	    }
	}
    }

  if (GTK_WIDGET_GET_CLASS (widget)->priv->accessible_type != GTK_TYPE_ACCESSIBLE)
    {
      GtkAccessible *accessible = g_object_steal_qdata (G_OBJECT (widget), quark_accessible_object);
      
      if (accessible)
        {
          gtk_accessible_set_widget (accessible, NULL);
          g_object_unref (accessible);
        }
    }

  /* wipe accelerator closures (keep order) */
  g_object_set_qdata (G_OBJECT (widget), quark_accel_path, NULL);
  g_object_set_qdata (G_OBJECT (widget), quark_accel_closures, NULL);

  /* Callers of add_mnemonic_label() should disconnect on ::destroy */
  g_object_set_qdata (G_OBJECT (widget), quark_mnemonic_labels, NULL);

  gtk_grab_remove (widget);

  for (l = priv->tick_callbacks; l;)
    {
      GList *next = l->next;
      destroy_tick_callback_info (widget, l->data, l);
      l = next;
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  if (priv->style)
    g_object_unref (priv->style);
  priv->style = gtk_widget_get_default_style ();
  g_object_ref (priv->style);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
gtk_widget_finalize (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;
  GtkWidgetAuxInfo *aux_info;
  GtkAccessible *accessible;
  GList *l;

  gtk_grab_remove (widget);

  g_clear_object (&priv->style);

  g_free (priv->name);

  aux_info = gtk_widget_get_aux_info (widget, FALSE);
  if (aux_info)
    gtk_widget_aux_info_destroy (aux_info);

  accessible = g_object_get_qdata (G_OBJECT (widget), quark_accessible_object);
  if (accessible)
    g_object_unref (accessible);

  if (priv->path)
    gtk_widget_path_free (priv->path);

  if (priv->context)
    {
      _gtk_style_context_set_widget (priv->context, NULL);
      g_object_unref (priv->context);
    }

  _gtk_size_request_cache_free (&priv->requests);

  for (l = priv->event_controllers; l; l = l->next)
    {
      EventControllerData *data = l->data;
      if (data->controller)
        _gtk_widget_remove_controller (widget, data->controller);
    }
  g_list_free_full (priv->event_controllers, g_free);
  priv->event_controllers = NULL;

  if (g_object_is_floating (object))
    g_warning ("A floating object was finalized. This means that someone\n"
               "called g_object_unref() on an object that had only a floating\n"
               "reference; the initial floating reference is not owned by anyone\n"
               "and must be removed with g_object_ref_sink().");

  G_OBJECT_CLASS (gtk_widget_parent_class)->finalize (object);
}

/*****************************************
 * gtk_widget_real_map:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (gtk_widget_get_realized (widget));

  if (!gtk_widget_get_mapped (widget))
    {
      gtk_widget_set_mapped (widget, TRUE);

      if (gtk_widget_get_has_window (widget))
	gdk_window_show (priv->window);
    }
}

/*****************************************
 * gtk_widget_real_unmap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_unmap (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (gtk_widget_get_mapped (widget))
    {
      gtk_widget_set_mapped (widget, FALSE);

      if (gtk_widget_get_has_window (widget))
	gdk_window_hide (priv->window);
    }
}

/*****************************************
 * gtk_widget_real_realize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (!gtk_widget_get_has_window (widget));

  gtk_widget_set_realized (widget, TRUE);
  if (priv->parent)
    {
      priv->window = gtk_widget_get_parent_window (widget);
      g_object_ref (priv->window);
    }
}

/*****************************************
 * gtk_widget_real_unrealize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_unrealize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (!widget->priv->mapped);

  /* printf ("unrealizing %s\n", g_type_name (G_TYPE_FROM_INSTANCE (widget)));
   */

   /* We must do unrealize child widget BEFORE container widget.
    * gdk_window_destroy() destroys specified xwindow and its sub-xwindows.
    * So, unrealizing container widget bofore its children causes the problem
    * (for example, gdk_ic_destroy () with destroyed window causes crash. )
    */

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  (GtkCallback) gtk_widget_unrealize,
			  NULL);

  if (gtk_widget_get_has_window (widget))
    {
      gtk_widget_unregister_window (widget, priv->window);
      gdk_window_destroy (priv->window);
      priv->window = NULL;
    }
  else
    {
      g_object_unref (priv->window);
      priv->window = NULL;
    }

  gtk_selection_remove_all (widget);

  gtk_widget_set_realized (widget, FALSE);
}

static void
gtk_widget_real_adjust_size_request (GtkWidget         *widget,
                                     GtkOrientation     orientation,
                                     gint              *minimum_size,
                                     gint              *natural_size)
{
  const GtkWidgetAuxInfo *aux_info;

  aux_info =_gtk_widget_get_aux_info_or_defaults (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      aux_info->width > 0)
    {
      *minimum_size = MAX (*minimum_size, aux_info->width);
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL &&
           aux_info->height > 0)
    {
      *minimum_size = MAX (*minimum_size, aux_info->height);
    }

  /* Fix it if set_size_request made natural size smaller than min size.
   * This would also silently fix broken widgets, but we warn about them
   * in gtksizerequest.c when calling their size request vfuncs.
   */
  *natural_size = MAX (*natural_size, *minimum_size);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum_size += (aux_info->margin.left + aux_info->margin.right);
      *natural_size += (aux_info->margin.left + aux_info->margin.right);
    }
  else
    {
      *minimum_size += (aux_info->margin.top + aux_info->margin.bottom);
      *natural_size += (aux_info->margin.top + aux_info->margin.bottom);
    }
}

static void
gtk_widget_real_adjust_baseline_request (GtkWidget         *widget,
					 gint              *minimum_baseline,
					 gint              *natural_baseline)
{
  const GtkWidgetAuxInfo *aux_info;

  aux_info =_gtk_widget_get_aux_info_or_defaults (widget);

  if (aux_info->height >= 0)
    {
      /* No baseline support for explicitly set height */
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else
    {
      *minimum_baseline += aux_info->margin.top;
      *natural_baseline += aux_info->margin.top;
    }
}

/**
 * _gtk_widget_peek_request_cache:
 *
 * Returns the address of the widget’s request cache (strictly for
 * internal use in gtksizerequest.c)
 *
 * Returns: the address of @widget’s size request cache.
 **/
gpointer
_gtk_widget_peek_request_cache (GtkWidget *widget)
{
  /* Don't bother slowing things down with the return_if_fail guards here */
  return &widget->priv->requests;
}

static gboolean
is_my_window (GtkWidget *widget,
              GdkWindow *window)
{
  gpointer user_data;

  gdk_window_get_user_data (window, &user_data);
  return (user_data == widget);
}

/*
 * _gtk_widget_get_device_window:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns: the window of @widget that @device is in, or %NULL
 */
GdkWindow *
_gtk_widget_get_device_window (GtkWidget *widget,
                               GdkDevice *device)
{
  GdkWindow *window;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    return NULL;

  window = gdk_device_get_last_event_window (device);
  if (window && is_my_window (widget, window))
    return window;
  else
    return NULL;
}

static void
list_devices (GtkWidget        *widget,
              GdkDeviceManager *device_manager,
              GdkDeviceType     device_type,
              GList           **result)
{
  GList *devices = gdk_device_manager_list_devices (device_manager, device_type);
  GList *l;

  for (l = devices; l; l = l->next)
    {
      GdkDevice *device = l->data;
      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        {
          GdkWindow *window = gdk_device_get_last_event_window (device);
          if (window && is_my_window (widget, window))
            *result = g_list_prepend (*result, device);
        }
    }
  g_list_free (devices);
}

/*
 * _gtk_widget_list_devices:
 * @widget: a #GtkWidget
 *
 * Returns the list of #GdkDevices that is currently on top
 * of any window belonging to @widget.
 * Free the list with g_list_free(), the elements are owned
 * by GTK+ and must not be freed.
 */
GList *
_gtk_widget_list_devices (GtkWidget *widget)
{
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GList *result = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  display = gtk_widget_get_display (widget);
  device_manager = gdk_display_get_device_manager (display);
  if (!gtk_widget_get_mapped (widget))
    return NULL;

  list_devices (widget, device_manager, GDK_DEVICE_TYPE_MASTER, &result);
  /* Rare, but we can get events for grabbed slave devices */
  list_devices (widget, device_manager, GDK_DEVICE_TYPE_SLAVE, &result);

  return result;
}

static void
synth_crossing (GtkWidget       *widget,
                GdkEventType     type,
                GdkWindow       *window,
                GdkDevice       *device,
                GdkCrossingMode  mode,
                GdkNotifyType    detail)
{
  GdkEvent *event;

  event = gdk_event_new (type);

  event->crossing.window = g_object_ref (window);
  event->crossing.send_event = TRUE;
  event->crossing.subwindow = g_object_ref (window);
  event->crossing.time = GDK_CURRENT_TIME;
  gdk_device_get_position_double (device,
                                  NULL,
                                  &event->crossing.x_root,
                                  &event->crossing.y_root);
  gdk_window_get_device_position_double (window,
                                         device,
                                         &event->crossing.x,
                                         &event->crossing.y,
                                         NULL);
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.focus = FALSE;
  event->crossing.state = 0;
  gdk_event_set_device (event, device);

  if (!widget)
    widget = gtk_get_event_widget (event);

  if (widget)
    gtk_widget_event_internal (widget, event);

  gdk_event_free (event);
}

/*
 * _gtk_widget_synthesize_crossing:
 * @from: the #GtkWidget the virtual pointer is leaving.
 * @to: the #GtkWidget the virtual pointer is moving to.
 * @mode: the #GdkCrossingMode to place on the synthesized events.
 *
 * Generate crossing event(s) on widget state (sensitivity) or GTK+ grab change.
 *
 * The real pointer window is the window that most recently received an enter notify
 * event.  Windows that don’t select for crossing events can’t become the real
 * poiner window.  The real pointer widget that owns the real pointer window.  The
 * effective pointer window is the same as the real pointer window unless the real
 * pointer widget is either insensitive or there is a grab on a widget that is not
 * an ancestor of the real pointer widget (in which case the effective pointer
 * window should be the root window).
 *
 * When the effective pointer window is the same as the real poiner window, we
 * receive crossing events from the windowing system.  When the effective pointer
 * window changes to become different from the real pointer window we synthesize
 * crossing events, attempting to follow X protocol rules:
 *
 * When the root window becomes the effective pointer window:
 *   - leave notify on real pointer window, detail Ancestor
 *   - leave notify on all of its ancestors, detail Virtual
 *   - enter notify on root window, detail Inferior
 *
 * When the root window ceases to be the effective pointer window:
 *   - leave notify on root window, detail Inferior
 *   - enter notify on all ancestors of real pointer window, detail Virtual
 *   - enter notify on real pointer window, detail Ancestor
 */
void
_gtk_widget_synthesize_crossing (GtkWidget       *from,
				 GtkWidget       *to,
                                 GdkDevice       *device,
				 GdkCrossingMode  mode)
{
  GdkWindow *from_window = NULL, *to_window = NULL;

  g_return_if_fail (from != NULL || to != NULL);

  if (from != NULL)
    {
      from_window = _gtk_widget_get_device_window (from, device);

      if (!from_window)
        from_window = from->priv->window;
    }

  if (to != NULL)
    {
      to_window = _gtk_widget_get_device_window (to, device);

      if (!to_window)
        to_window = to->priv->window;
    }

  if (from_window == NULL && to_window == NULL)
    ;
  else if (from_window != NULL && to_window == NULL)
    {
      GList *from_ancestors = NULL, *list;
      GdkWindow *from_ancestor = from_window;

      while (from_ancestor != NULL)
	{
	  from_ancestor = gdk_window_get_effective_parent (from_ancestor);
          if (from_ancestor == NULL)
            break;
          from_ancestors = g_list_prepend (from_ancestors, from_ancestor);
	}

      synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
		      device, mode, GDK_NOTIFY_ANCESTOR);
      for (list = g_list_last (from_ancestors); list; list = list->prev)
	{
	  synth_crossing (NULL, GDK_LEAVE_NOTIFY, (GdkWindow *) list->data,
			  device, mode, GDK_NOTIFY_VIRTUAL);
	}

      /* XXX: enter/inferior on root window? */

      g_list_free (from_ancestors);
    }
  else if (from_window == NULL && to_window != NULL)
    {
      GList *to_ancestors = NULL, *list;
      GdkWindow *to_ancestor = to_window;

      while (to_ancestor != NULL)
	{
	  to_ancestor = gdk_window_get_effective_parent (to_ancestor);
	  if (to_ancestor == NULL)
            break;
          to_ancestors = g_list_prepend (to_ancestors, to_ancestor);
        }

      /* XXX: leave/inferior on root window? */

      for (list = to_ancestors; list; list = list->next)
	{
	  synth_crossing (NULL, GDK_ENTER_NOTIFY, (GdkWindow *) list->data,
			  device, mode, GDK_NOTIFY_VIRTUAL);
	}
      synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
		      device, mode, GDK_NOTIFY_ANCESTOR);

      g_list_free (to_ancestors);
    }
  else if (from_window == to_window)
    ;
  else
    {
      GList *from_ancestors = NULL, *to_ancestors = NULL, *list;
      GdkWindow *from_ancestor = from_window, *to_ancestor = to_window;

      while (from_ancestor != NULL || to_ancestor != NULL)
	{
	  if (from_ancestor != NULL)
	    {
	      from_ancestor = gdk_window_get_effective_parent (from_ancestor);
	      if (from_ancestor == to_window)
		break;
              if (from_ancestor)
	        from_ancestors = g_list_prepend (from_ancestors, from_ancestor);
	    }
	  if (to_ancestor != NULL)
	    {
	      to_ancestor = gdk_window_get_effective_parent (to_ancestor);
	      if (to_ancestor == from_window)
		break;
              if (to_ancestor)
	        to_ancestors = g_list_prepend (to_ancestors, to_ancestor);
	    }
	}
      if (to_ancestor == from_window)
	{
	  if (mode != GDK_CROSSING_GTK_UNGRAB)
	    synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
			    device, mode, GDK_NOTIFY_INFERIOR);
	  for (list = to_ancestors; list; list = list->next)
	    synth_crossing (NULL, GDK_ENTER_NOTIFY, (GdkWindow *) list->data,
			    device, mode, GDK_NOTIFY_VIRTUAL);
	  synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
			  device, mode, GDK_NOTIFY_ANCESTOR);
	}
      else if (from_ancestor == to_window)
	{
	  synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
			  device, mode, GDK_NOTIFY_ANCESTOR);
	  for (list = g_list_last (from_ancestors); list; list = list->prev)
	    {
	      synth_crossing (NULL, GDK_LEAVE_NOTIFY, (GdkWindow *) list->data,
			      device, mode, GDK_NOTIFY_VIRTUAL);
	    }
	  if (mode != GDK_CROSSING_GTK_GRAB)
	    synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
			    device, mode, GDK_NOTIFY_INFERIOR);
	}
      else
	{
	  while (from_ancestors != NULL && to_ancestors != NULL
		 && from_ancestors->data == to_ancestors->data)
	    {
	      from_ancestors = g_list_delete_link (from_ancestors,
						   from_ancestors);
	      to_ancestors = g_list_delete_link (to_ancestors, to_ancestors);
	    }

	  synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
			  device, mode, GDK_NOTIFY_NONLINEAR);

	  for (list = g_list_last (from_ancestors); list; list = list->prev)
	    {
	      synth_crossing (NULL, GDK_LEAVE_NOTIFY, (GdkWindow *) list->data,
			      device, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  for (list = to_ancestors; list; list = list->next)
	    {
	      synth_crossing (NULL, GDK_ENTER_NOTIFY, (GdkWindow *) list->data,
			      device, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
			  device, mode, GDK_NOTIFY_NONLINEAR);
	}
      g_list_free (from_ancestors);
      g_list_free (to_ancestors);
    }
}

static void
gtk_widget_propagate_state (GtkWidget    *widget,
                            GtkStateData *data)
{
  GtkWidgetPrivate *priv = widget->priv;
  GtkStateFlags new_flags, old_flags = priv->state_flags;
  GtkStateType old_state;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  old_state = gtk_widget_get_state (widget);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  priv->state_flags |= data->flags_to_set;
  priv->state_flags &= ~(data->flags_to_unset);

  /* make insensitivity unoverridable */
  if (!priv->sensitive)
    priv->state_flags |= GTK_STATE_FLAG_INSENSITIVE;

  if (gtk_widget_is_focus (widget) && !gtk_widget_is_sensitive (widget))
    {
      GtkWidget *window;

      window = gtk_widget_get_toplevel (widget);

      if (window && gtk_widget_is_toplevel (window))
        gtk_window_set_focus (GTK_WINDOW (window), NULL);
    }

  new_flags = priv->state_flags;

  if (old_flags != new_flags)
    {
      g_object_ref (widget);

      if (!gtk_widget_is_sensitive (widget) && gtk_widget_has_grab (widget))
        gtk_grab_remove (widget);

      gtk_style_context_set_state (gtk_widget_get_style_context (widget), new_flags);

      g_signal_emit (widget, widget_signals[STATE_CHANGED], 0, old_state);
      g_signal_emit (widget, widget_signals[STATE_FLAGS_CHANGED], 0, old_flags);

      if (!priv->shadowed &&
          (new_flags & GTK_STATE_FLAG_INSENSITIVE) != (old_flags & GTK_STATE_FLAG_INSENSITIVE))
        {
          GList *event_windows = NULL;
          GList *devices, *d;

          devices = _gtk_widget_list_devices (widget);

          for (d = devices; d; d = d->next)
            {
              GdkWindow *window;
              GdkDevice *device;

              device = d->data;
              window = _gtk_widget_get_device_window (widget, device);

              /* Do not propagate more than once to the
               * same window if non-multidevice aware.
               */
              if (!gdk_window_get_support_multidevice (window) &&
                  g_list_find (event_windows, window))
                continue;

              if (!gtk_widget_is_sensitive (widget))
                _gtk_widget_synthesize_crossing (widget, NULL, d->data,
                                                 GDK_CROSSING_STATE_CHANGED);
              else
                _gtk_widget_synthesize_crossing (NULL, widget, d->data,
                                                 GDK_CROSSING_STATE_CHANGED);

              event_windows = g_list_prepend (event_windows, window);
            }

          g_list_free (event_windows);
          g_list_free (devices);
        }

      if (!gtk_widget_is_sensitive (widget))
        {
          EventControllerData *data;
          GList *l;

          /* Reset all controllers */
          for (l = priv->event_controllers; l; l = l->next)
            {
              data = l->data;
              gtk_event_controller_reset (data->controller);
            }
        }

      if (GTK_IS_CONTAINER (widget))
        {
          GtkStateData child_data;

          /* Make sure to only propate the right states further */
          child_data.flags_to_set = data->flags_to_set & GTK_STATE_FLAGS_DO_PROPAGATE;
          child_data.flags_to_unset = data->flags_to_unset & GTK_STATE_FLAGS_DO_PROPAGATE;

          gtk_container_forall (GTK_CONTAINER (widget),
                                (GtkCallback) gtk_widget_propagate_state,
                                &child_data);
        }

      g_object_unref (widget);
    }
}

static const GtkWidgetAuxInfo default_aux_info = {
  -1, -1,
  GTK_ALIGN_FILL,
  GTK_ALIGN_FILL,
  { 0, 0, 0, 0 }
};

/*
 * gtk_widget_get_aux_info:
 * @widget: a #GtkWidget
 * @create: if %TRUE, create the #GtkWidgetAuxInfo-struct if it doesn’t exist
 *
 * Get the #GtkWidgetAuxInfo-struct for the widget.
 *
 * Returns: the #GtkWidgetAuxInfo-struct for the widget, or
 *    %NULL if @create is %FALSE and one doesn’t already exist.
 */
static GtkWidgetAuxInfo *
gtk_widget_get_aux_info (GtkWidget *widget,
			 gboolean   create)
{
  GtkWidgetAuxInfo *aux_info;

  aux_info = g_object_get_qdata (G_OBJECT (widget), quark_aux_info);
  if (!aux_info && create)
    {
      aux_info = g_slice_new0 (GtkWidgetAuxInfo);

      *aux_info = default_aux_info;

      g_object_set_qdata (G_OBJECT (widget), quark_aux_info, aux_info);
    }

  return aux_info;
}

static const GtkWidgetAuxInfo*
_gtk_widget_get_aux_info_or_defaults (GtkWidget *widget)
{
  GtkWidgetAuxInfo *aux_info;

  aux_info = gtk_widget_get_aux_info (widget, FALSE);
  if (aux_info == NULL)
    {
      return &default_aux_info;
    }
  else
    {
      return aux_info;
    }
}

/*****************************************
 * gtk_widget_aux_info_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_aux_info_destroy (GtkWidgetAuxInfo *aux_info)
{
  g_slice_free (GtkWidgetAuxInfo, aux_info);
}

/**
 * gtk_widget_shape_combine_region:
 * @widget: a #GtkWidget
 * @region: (allow-none): shape to be added, or %NULL to remove an existing shape
 *
 * Sets a shape for this widget’s GDK window. This allows for
 * transparent windows etc., see gdk_window_shape_combine_region()
 * for more information.
 *
 * Since: 3.0
 **/
void
gtk_widget_shape_combine_region (GtkWidget *widget,
                                 cairo_region_t *region)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (gtk_widget_get_has_window (widget));

  priv = widget->priv;

  if (region == NULL)
    {
      priv->has_shape_mask = FALSE;

      if (priv->window)
	gdk_window_shape_combine_region (priv->window, NULL, 0, 0);

      g_object_set_qdata (G_OBJECT (widget), quark_shape_info, NULL);
    }
  else
    {
      priv->has_shape_mask = TRUE;

      g_object_set_qdata_full (G_OBJECT (widget), quark_shape_info,
                               cairo_region_copy (region),
			       (GDestroyNotify) cairo_region_destroy);

      /* set shape if widget has a gdk window already.
       * otherwise the shape is scheduled to be set by gtk_widget_realize().
       */
      if (priv->window)
	gdk_window_shape_combine_region (priv->window, region, 0, 0);
    }
}

static void
gtk_widget_update_input_shape (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  /* set shape if widget has a gdk window already.
   * otherwise the shape is scheduled to be set by gtk_widget_realize().
   */
  if (priv->window)
    {
      cairo_region_t *region;
      cairo_region_t *csd_region;
      cairo_region_t *app_region;
      gboolean free_region;

      app_region = g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info);
      csd_region = g_object_get_data (G_OBJECT (widget), "csd-region");

      free_region = FALSE;

      if (app_region && csd_region)
        {
          free_region = TRUE;
          region = cairo_region_copy (app_region);
          cairo_region_intersect (region, csd_region);
        }
      else if (app_region)
        region = app_region;
      else if (csd_region)
        region = csd_region;
      else
        region = NULL;

      gdk_window_input_shape_combine_region (priv->window, region, 0, 0);

      if (free_region)
        cairo_region_destroy (region);
    }
}

void
gtk_widget_set_csd_input_shape (GtkWidget            *widget,
                                const cairo_region_t *region)
{
  if (region == NULL)
    g_object_set_data (G_OBJECT (widget), "csd-region", NULL);
  else
    g_object_set_data_full (G_OBJECT (widget), "csd-region",
                            cairo_region_copy (region),
                            (GDestroyNotify) cairo_region_destroy);
  gtk_widget_update_input_shape (widget);
}

/**
 * gtk_widget_input_shape_combine_region:
 * @widget: a #GtkWidget
 * @region: (allow-none): shape to be added, or %NULL to remove an existing shape
 *
 * Sets an input shape for this widget’s GDK window. This allows for
 * windows which react to mouse click in a nonrectangular region, see
 * gdk_window_input_shape_combine_region() for more information.
 *
 * Since: 3.0
 **/
void
gtk_widget_input_shape_combine_region (GtkWidget      *widget,
                                       cairo_region_t *region)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (gtk_widget_get_has_window (widget));

  if (region == NULL)
    g_object_set_qdata (G_OBJECT (widget), quark_input_shape_info, NULL);
  else
    g_object_set_qdata_full (G_OBJECT (widget), quark_input_shape_info,
                             cairo_region_copy (region),
                             (GDestroyNotify) cairo_region_destroy);
  gtk_widget_update_input_shape (widget);
}


/* style properties
 */

/**
 * gtk_widget_class_install_style_property_parser: (skip)
 * @klass: a #GtkWidgetClass
 * @pspec: the #GParamSpec for the style property
 * @parser: the parser for the style property
 *
 * Installs a style property on a widget class.
 **/
void
gtk_widget_class_install_style_property_parser (GtkWidgetClass     *klass,
						GParamSpec         *pspec,
						GtkRcPropertyParser parser)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (klass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->flags & G_PARAM_READABLE);
  g_return_if_fail (!(pspec->flags & (G_PARAM_CONSTRUCT_ONLY | G_PARAM_CONSTRUCT)));

  if (g_param_spec_pool_lookup (style_property_spec_pool, pspec->name, G_OBJECT_CLASS_TYPE (klass), FALSE))
    {
      g_warning (G_STRLOC ": class `%s' already contains a style property named `%s'",
		 G_OBJECT_CLASS_NAME (klass),
		 pspec->name);
      return;
    }

  g_param_spec_ref_sink (pspec);
  g_param_spec_set_qdata (pspec, quark_property_parser, (gpointer) parser);
  g_param_spec_pool_insert (style_property_spec_pool, pspec, G_OBJECT_CLASS_TYPE (klass));
}

/**
 * gtk_widget_class_install_style_property:
 * @klass: a #GtkWidgetClass
 * @pspec: the #GParamSpec for the property
 *
 * Installs a style property on a widget class. The parser for the
 * style property is determined by the value type of @pspec.
 **/
void
gtk_widget_class_install_style_property (GtkWidgetClass *klass,
					 GParamSpec     *pspec)
{
  GtkRcPropertyParser parser;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (klass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  parser = _gtk_rc_property_parser_from_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  gtk_widget_class_install_style_property_parser (klass, pspec, parser);
}

/**
 * gtk_widget_class_find_style_property:
 * @klass: a #GtkWidgetClass
 * @property_name: the name of the style property to find
 *
 * Finds a style property of a widget class by name.
 *
 * Returns: (transfer none): the #GParamSpec of the style property or
 *   %NULL if @class has no style property with that name.
 *
 * Since: 2.2
 */
GParamSpec*
gtk_widget_class_find_style_property (GtkWidgetClass *klass,
				      const gchar    *property_name)
{
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (style_property_spec_pool,
				   property_name,
				   G_OBJECT_CLASS_TYPE (klass),
				   TRUE);
}

/**
 * gtk_widget_class_list_style_properties:
 * @klass: a #GtkWidgetClass
 * @n_properties: (out): location to return the number of style properties found
 *
 * Returns all style properties of a widget class.
 *
 * Returns: (array length=n_properties) (transfer container): a
 *     newly allocated array of #GParamSpec*. The array must be
 *     freed with g_free().
 *
 * Since: 2.2
 */
GParamSpec**
gtk_widget_class_list_style_properties (GtkWidgetClass *klass,
					guint          *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  pspecs = g_param_spec_pool_list (style_property_spec_pool,
				   G_OBJECT_CLASS_TYPE (klass),
				   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

/**
 * gtk_widget_style_get_property:
 * @widget: a #GtkWidget
 * @property_name: the name of a style property
 * @value: location to return the property value
 *
 * Gets the value of a style property of @widget.
 */
void
gtk_widget_style_get_property (GtkWidget   *widget,
			       const gchar *property_name,
			       GValue      *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  g_object_ref (widget);
  pspec = g_param_spec_pool_lookup (style_property_spec_pool,
				    property_name,
				    G_OBJECT_TYPE (widget),
				    TRUE);
  if (!pspec)
    g_warning ("%s: widget class `%s' has no property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (widget),
	       property_name);
  else
    {
      GtkStyleContext *context;
      const GValue *peek_value;

      context = gtk_widget_get_style_context (widget);

      peek_value = _gtk_style_context_peek_style_property (context,
                                                           G_OBJECT_TYPE (widget),
                                                           pspec);

      /* auto-conversion of the caller's value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
	g_value_copy (peek_value, value);
      else if (g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
	g_value_transform (peek_value, value);
      else
	g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
		   pspec->name,
		   g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		   G_VALUE_TYPE_NAME (value));
    }
  g_object_unref (widget);
}

/**
 * gtk_widget_style_get_valist:
 * @widget: a #GtkWidget
 * @first_property_name: the name of the first property to get
 * @var_args: a va_list of pairs of property names and
 *     locations to return the property values, starting with the location
 *     for @first_property_name.
 *
 * Non-vararg variant of gtk_widget_style_get(). Used primarily by language
 * bindings.
 */
void
gtk_widget_style_get_valist (GtkWidget   *widget,
			     const gchar *first_property_name,
			     va_list      var_args)
{
  GtkStyleContext *context;
  const gchar *name;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (widget);
  context = gtk_widget_get_style_context (widget);

  name = first_property_name;
  while (name)
    {
      const GValue *peek_value;
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (style_property_spec_pool,
					name,
					G_OBJECT_TYPE (widget),
					TRUE);
      if (!pspec)
	{
	  g_warning ("%s: widget class `%s' has no property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (widget),
		     name);
	  break;
	}
      /* style pspecs are always readable so we can spare that check here */

      peek_value = _gtk_style_context_peek_style_property (context,
                                                           G_OBJECT_TYPE (widget),
                                                           pspec);

      G_VALUE_LCOPY (peek_value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  break;
	}

      name = va_arg (var_args, gchar*);
    }

  g_object_unref (widget);
}

/**
 * gtk_widget_style_get:
 * @widget: a #GtkWidget
 * @first_property_name: the name of the first property to get
 * @...: pairs of property names and locations to return the
 *     property values, starting with the location for
 *     @first_property_name, terminated by %NULL.
 *
 * Gets the values of a multiple style properties of @widget.
 */
void
gtk_widget_style_get (GtkWidget   *widget,
		      const gchar *first_property_name,
		      ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  va_start (var_args, first_property_name);
  gtk_widget_style_get_valist (widget, first_property_name, var_args);
  va_end (var_args);
}

/**
 * gtk_requisition_new:
 *
 * Allocates a new #GtkRequisition-struct and initializes its elements to zero.
 *
 * Returns: a new empty #GtkRequisition. The newly allocated #GtkRequisition should
 *   be freed with gtk_requisition_free().
 *
 * Since: 3.0
 */
GtkRequisition *
gtk_requisition_new (void)
{
  return g_slice_new0 (GtkRequisition);
}

/**
 * gtk_requisition_copy:
 * @requisition: a #GtkRequisition
 *
 * Copies a #GtkRequisition.
 *
 * Returns: a copy of @requisition
 **/
GtkRequisition *
gtk_requisition_copy (const GtkRequisition *requisition)
{
  return g_slice_dup (GtkRequisition, requisition);
}

/**
 * gtk_requisition_free:
 * @requisition: a #GtkRequisition
 *
 * Frees a #GtkRequisition.
 **/
void
gtk_requisition_free (GtkRequisition *requisition)
{
  g_slice_free (GtkRequisition, requisition);
}

G_DEFINE_BOXED_TYPE (GtkRequisition, gtk_requisition,
                     gtk_requisition_copy,
                     gtk_requisition_free)

/**
 * gtk_widget_class_set_accessible_type:
 * @widget_class: class to set the accessible type for
 * @type: The object type that implements the accessible for @widget_class
 *
 * Sets the type to be used for creating accessibles for widgets of
 * @widget_class. The given @type must be a subtype of the type used for
 * accessibles of the parent class.
 *
 * This function should only be called from class init functions of widgets.
 *
 * Since: 3.2
 **/
void
gtk_widget_class_set_accessible_type (GtkWidgetClass *widget_class,
                                      GType           type)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (g_type_is_a (type, widget_class->priv->accessible_type));

  priv = widget_class->priv;

  priv->accessible_type = type;
  /* reset this - honoring the type's role is better. */
  priv->accessible_role = ATK_ROLE_INVALID;
}

/**
 * gtk_widget_class_set_accessible_role:
 * @widget_class: class to set the accessible role for
 * @role: The role to use for accessibles created for @widget_class
 *
 * Sets the default #AtkRole to be set on accessibles created for
 * widgets of @widget_class. Accessibles may decide to not honor this
 * setting if their role reporting is more refined. Calls to 
 * gtk_widget_class_set_accessible_type() will reset this value.
 *
 * In cases where you want more fine-grained control over the role of
 * accessibles created for @widget_class, you should provide your own
 * accessible type and use gtk_widget_class_set_accessible_type()
 * instead.
 *
 * If @role is #ATK_ROLE_INVALID, the default role will not be changed
 * and the accessible’s default role will be used instead.
 *
 * This function should only be called from class init functions of widgets.
 *
 * Since: 3.2
 **/
void
gtk_widget_class_set_accessible_role (GtkWidgetClass *widget_class,
                                      AtkRole         role)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

  priv = widget_class->priv;

  priv->accessible_role = role;
}

/**
 * _gtk_widget_peek_accessible:
 * @widget: a #GtkWidget
 *
 * Gets the accessible for @widget, if it has been created yet.
 * Otherwise, this function returns %NULL. If the @widget’s implementation
 * does not use the default way to create accessibles, %NULL will always be
 * returned.
 *
 * Returns: the accessible for @widget or %NULL if none has been
 *     created yet.
 **/
AtkObject *
_gtk_widget_peek_accessible (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget),
                             quark_accessible_object);
}

/**
 * gtk_widget_get_accessible:
 * @widget: a #GtkWidget
 *
 * Returns the accessible object that describes the widget to an
 * assistive technology.
 *
 * If accessibility support is not available, this #AtkObject
 * instance may be a no-op. Likewise, if no class-specific #AtkObject
 * implementation is available for the widget instance in question,
 * it will inherit an #AtkObject implementation from the first ancestor
 * class for which such an implementation is defined.
 *
 * The documentation of the
 * [ATK](http://developer.gnome.org/atk/stable/)
 * library contains more information about accessible objects and their uses.
 *
 * Returns: (transfer none): the #AtkObject associated with @widget
 */
AtkObject*
gtk_widget_get_accessible (GtkWidget *widget)
{
  GtkWidgetClass *klass;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  klass = GTK_WIDGET_GET_CLASS (widget);

  g_return_val_if_fail (klass->get_accessible != NULL, NULL);

  return klass->get_accessible (widget);
}

static AtkObject*
gtk_widget_real_get_accessible (GtkWidget *widget)
{
  AtkObject* accessible;

  accessible = g_object_get_qdata (G_OBJECT (widget),
                                   quark_accessible_object);
  if (!accessible)
  {
    GtkWidgetClass *widget_class;
    GtkWidgetClassPrivate *priv;
    AtkObjectFactory *factory;
    AtkRegistry *default_registry;

    widget_class = GTK_WIDGET_GET_CLASS (widget);
    priv = widget_class->priv;

    if (priv->accessible_type == GTK_TYPE_ACCESSIBLE)
      {
        default_registry = atk_get_default_registry ();
        factory = atk_registry_get_factory (default_registry,
                                            G_TYPE_FROM_INSTANCE (widget));
        accessible =
          atk_object_factory_create_accessible (factory,
                                                G_OBJECT (widget));

        if (priv->accessible_role != ATK_ROLE_INVALID)
          atk_object_set_role (accessible, priv->accessible_role);

        g_object_set_qdata (G_OBJECT (widget),
                            quark_accessible_object,
                            accessible);
      }
    else
      {
        accessible = g_object_new (priv->accessible_type,
                                   "widget", widget,
                                   NULL);
        if (priv->accessible_role != ATK_ROLE_INVALID)
          atk_object_set_role (accessible, priv->accessible_role);

        g_object_set_qdata (G_OBJECT (widget),
                            quark_accessible_object,
                            accessible);

        atk_object_initialize (accessible, widget);

        /* Set the role again, since we don't want a role set
         * in some parent initialize() function to override
         * our own.
         */
        if (priv->accessible_role != ATK_ROLE_INVALID)
          atk_object_set_role (accessible, priv->accessible_role);
      }
  }
  return accessible;
}

/*
 * Initialize a AtkImplementorIface instance’s virtual pointers as
 * appropriate to this implementor’s class (GtkWidget).
 */
static void
gtk_widget_accessible_interface_init (AtkImplementorIface *iface)
{
  iface->ref_accessible = gtk_widget_ref_accessible;
}

static AtkObject*
gtk_widget_ref_accessible (AtkImplementor *implementor)
{
  AtkObject *accessible;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (implementor));
  if (accessible)
    g_object_ref (accessible);
  return accessible;
}

/*
 * Expand flag management
 */

static void
gtk_widget_update_computed_expand (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  priv = widget->priv;

  if (priv->need_compute_expand)
    {
      gboolean h, v;

      if (priv->hexpand_set)
        h = priv->hexpand;
      else
        h = FALSE;

      if (priv->vexpand_set)
        v = priv->vexpand;
      else
        v = FALSE;

      /* we don't need to use compute_expand if both expands are
       * forced by the app
       */
      if (!(priv->hexpand_set && priv->vexpand_set))
        {
          if (GTK_WIDGET_GET_CLASS (widget)->compute_expand != NULL)
            {
              gboolean ignored;

              GTK_WIDGET_GET_CLASS (widget)->compute_expand (widget,
                                                             priv->hexpand_set ? &ignored : &h,
                                                             priv->vexpand_set ? &ignored : &v);
            }
        }

      priv->need_compute_expand = FALSE;
      priv->computed_hexpand = h != FALSE;
      priv->computed_vexpand = v != FALSE;
    }
}

/**
 * gtk_widget_queue_compute_expand:
 * @widget: a #GtkWidget
 *
 * Mark @widget as needing to recompute its expand flags. Call
 * this function when setting legacy expand child properties
 * on the child of a container.
 *
 * See gtk_widget_compute_expand().
 */
void
gtk_widget_queue_compute_expand (GtkWidget *widget)
{
  GtkWidget *parent;
  gboolean changed_anything;

  if (widget->priv->need_compute_expand)
    return;

  changed_anything = FALSE;
  parent = widget;
  while (parent != NULL)
    {
      if (!parent->priv->need_compute_expand)
        {
          parent->priv->need_compute_expand = TRUE;
          changed_anything = TRUE;
        }

      /* Note: if we had an invariant that "if a child needs to
       * compute expand, its parents also do" then we could stop going
       * up when we got to a parent that already needed to
       * compute. However, in general we compute expand lazily (as
       * soon as we see something in a subtree that is expand, we know
       * we're expanding) and so this invariant does not hold and we
       * have to always walk all the way up in case some ancestor
       * is not currently need_compute_expand.
       */

      parent = parent->priv->parent;
    }

  /* recomputing expand always requires
   * a relayout as well
   */
  if (changed_anything)
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_widget_compute_expand:
 * @widget: the widget
 * @orientation: expand direction
 *
 * Computes whether a container should give this widget extra space
 * when possible. Containers should check this, rather than
 * looking at gtk_widget_get_hexpand() or gtk_widget_get_vexpand().
 *
 * This function already checks whether the widget is visible, so
 * visibility does not need to be checked separately. Non-visible
 * widgets are not expanded.
 *
 * The computed expand value uses either the expand setting explicitly
 * set on the widget itself, or, if none has been explicitly set,
 * the widget may expand if some of its children do.
 *
 * Returns: whether widget tree rooted here should be expanded
 */
gboolean
gtk_widget_compute_expand (GtkWidget     *widget,
                           GtkOrientation orientation)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  /* We never make a widget expand if not even showing. */
  if (!gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_update_computed_expand (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      return widget->priv->computed_hexpand;
    }
  else
    {
      return widget->priv->computed_vexpand;
    }
}

static void
gtk_widget_set_expand (GtkWidget     *widget,
                       GtkOrientation orientation,
                       gboolean       expand)
{
  const char *expand_prop;
  const char *expand_set_prop;
  gboolean was_both;
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  expand = expand != FALSE;

  was_both = priv->hexpand && priv->vexpand;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->hexpand_set &&
          priv->hexpand == expand)
        return;

      priv->hexpand_set = TRUE;
      priv->hexpand = expand;

      expand_prop = "hexpand";
      expand_set_prop = "hexpand-set";
    }
  else
    {
      if (priv->vexpand_set &&
          priv->vexpand == expand)
        return;

      priv->vexpand_set = TRUE;
      priv->vexpand = expand;

      expand_prop = "vexpand";
      expand_set_prop = "vexpand-set";
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_freeze_notify (G_OBJECT (widget));
  g_object_notify (G_OBJECT (widget), expand_prop);
  g_object_notify (G_OBJECT (widget), expand_set_prop);
  if (was_both != (priv->hexpand && priv->vexpand))
    g_object_notify (G_OBJECT (widget), "expand");
  g_object_thaw_notify (G_OBJECT (widget));
}

static void
gtk_widget_set_expand_set (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           gboolean        set)
{
  GtkWidgetPrivate *priv;
  const char *prop;

  priv = widget->priv;

  set = set != FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (set == priv->hexpand_set)
        return;

      priv->hexpand_set = set;
      prop = "hexpand-set";
    }
  else
    {
      if (set == priv->vexpand_set)
        return;

      priv->vexpand_set = set;
      prop = "vexpand-set";
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_notify (G_OBJECT (widget), prop);
}

/**
 * gtk_widget_get_hexpand:
 * @widget: the widget
 *
 * Gets whether the widget would like any available extra horizontal
 * space. When a user resizes a #GtkWindow, widgets with expand=TRUE
 * generally receive the extra space. For example, a list or
 * scrollable area or document in your window would often be set to
 * expand.
 *
 * Containers should use gtk_widget_compute_expand() rather than
 * this function, to see whether a widget, or any of its children,
 * has the expand flag set. If any child of a widget wants to
 * expand, the parent may ask to expand also.
 *
 * This function only looks at the widget’s own hexpand flag, rather
 * than computing whether the entire widget tree rooted at this widget
 * wants to expand.
 *
 * Returns: whether hexpand flag is set
 */
gboolean
gtk_widget_get_hexpand (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->hexpand;
}

/**
 * gtk_widget_set_hexpand:
 * @widget: the widget
 * @expand: whether to expand
 *
 * Sets whether the widget would like any available extra horizontal
 * space. When a user resizes a #GtkWindow, widgets with expand=TRUE
 * generally receive the extra space. For example, a list or
 * scrollable area or document in your window would often be set to
 * expand.
 *
 * Call this function to set the expand flag if you would like your
 * widget to become larger horizontally when the window has extra
 * room.
 *
 * By default, widgets automatically expand if any of their children
 * want to expand. (To see if a widget will automatically expand given
 * its current children and state, call gtk_widget_compute_expand(). A
 * container can decide how the expandability of children affects the
 * expansion of the container by overriding the compute_expand virtual
 * method on #GtkWidget.).
 *
 * Setting hexpand explicitly with this function will override the
 * automatic expand behavior.
 *
 * This function forces the widget to expand or not to expand,
 * regardless of children.  The override occurs because
 * gtk_widget_set_hexpand() sets the hexpand-set property (see
 * gtk_widget_set_hexpand_set()) which causes the widget’s hexpand
 * value to be used, rather than looking at children and widget state.
 */
void
gtk_widget_set_hexpand (GtkWidget      *widget,
                        gboolean        expand)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand (widget, GTK_ORIENTATION_HORIZONTAL, expand);
}

/**
 * gtk_widget_get_hexpand_set:
 * @widget: the widget
 *
 * Gets whether gtk_widget_set_hexpand() has been used to
 * explicitly set the expand flag on this widget.
 *
 * If hexpand is set, then it overrides any computed
 * expand value based on child widgets. If hexpand is not
 * set, then the expand value depends on whether any
 * children of the widget would like to expand.
 *
 * There are few reasons to use this function, but it’s here
 * for completeness and consistency.
 *
 * Returns: whether hexpand has been explicitly set
 */
gboolean
gtk_widget_get_hexpand_set (GtkWidget      *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->hexpand_set;
}

/**
 * gtk_widget_set_hexpand_set:
 * @widget: the widget
 * @set: value for hexpand-set property
 *
 * Sets whether the hexpand flag (see gtk_widget_get_hexpand()) will
 * be used.
 *
 * The hexpand-set property will be set automatically when you call
 * gtk_widget_set_hexpand() to set hexpand, so the most likely
 * reason to use this function would be to unset an explicit expand
 * flag.
 *
 * If hexpand is set, then it overrides any computed
 * expand value based on child widgets. If hexpand is not
 * set, then the expand value depends on whether any
 * children of the widget would like to expand.
 *
 * There are few reasons to use this function, but it’s here
 * for completeness and consistency.
 */
void
gtk_widget_set_hexpand_set (GtkWidget      *widget,
                            gboolean        set)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand_set (widget, GTK_ORIENTATION_HORIZONTAL, set);
}


/**
 * gtk_widget_get_vexpand:
 * @widget: the widget
 *
 * Gets whether the widget would like any available extra vertical
 * space.
 *
 * See gtk_widget_get_hexpand() for more detail.
 *
 * Returns: whether vexpand flag is set
 */
gboolean
gtk_widget_get_vexpand (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->vexpand;
}

/**
 * gtk_widget_set_vexpand:
 * @widget: the widget
 * @expand: whether to expand
 *
 * Sets whether the widget would like any available extra vertical
 * space.
 *
 * See gtk_widget_set_hexpand() for more detail.
 */
void
gtk_widget_set_vexpand (GtkWidget      *widget,
                        gboolean        expand)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand (widget, GTK_ORIENTATION_VERTICAL, expand);
}

/**
 * gtk_widget_get_vexpand_set:
 * @widget: the widget
 *
 * Gets whether gtk_widget_set_vexpand() has been used to
 * explicitly set the expand flag on this widget.
 *
 * See gtk_widget_get_hexpand_set() for more detail.
 *
 * Returns: whether vexpand has been explicitly set
 */
gboolean
gtk_widget_get_vexpand_set (GtkWidget      *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->vexpand_set;
}

/**
 * gtk_widget_set_vexpand_set:
 * @widget: the widget
 * @set: value for vexpand-set property
 *
 * Sets whether the vexpand flag (see gtk_widget_get_vexpand()) will
 * be used.
 *
 * See gtk_widget_set_hexpand_set() for more detail.
 */
void
gtk_widget_set_vexpand_set (GtkWidget      *widget,
                            gboolean        set)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand_set (widget, GTK_ORIENTATION_VERTICAL, set);
}

/*
 * GtkBuildable implementation
 */
static GQuark		 quark_builder_has_default = 0;
static GQuark		 quark_builder_has_focus = 0;
static GQuark		 quark_builder_atk_relations = 0;
static GQuark            quark_builder_set_name = 0;

static void
gtk_widget_buildable_interface_init (GtkBuildableIface *iface)
{
  quark_builder_has_default = g_quark_from_static_string ("gtk-builder-has-default");
  quark_builder_has_focus = g_quark_from_static_string ("gtk-builder-has-focus");
  quark_builder_atk_relations = g_quark_from_static_string ("gtk-builder-atk-relations");
  quark_builder_set_name = g_quark_from_static_string ("gtk-builder-set-name");

  iface->set_name = gtk_widget_buildable_set_name;
  iface->get_name = gtk_widget_buildable_get_name;
  iface->get_internal_child = gtk_widget_buildable_get_internal_child;
  iface->set_buildable_property = gtk_widget_buildable_set_buildable_property;
  iface->parser_finished = gtk_widget_buildable_parser_finished;
  iface->custom_tag_start = gtk_widget_buildable_custom_tag_start;
  iface->custom_finished = gtk_widget_buildable_custom_finished;
}

static void
gtk_widget_buildable_set_name (GtkBuildable *buildable,
			       const gchar  *name)
{
  g_object_set_qdata_full (G_OBJECT (buildable), quark_builder_set_name,
                           g_strdup (name), g_free);
}

static const gchar *
gtk_widget_buildable_get_name (GtkBuildable *buildable)
{
  return g_object_get_qdata (G_OBJECT (buildable), quark_builder_set_name);
}

static GObject *
gtk_widget_buildable_get_internal_child (GtkBuildable *buildable,
					 GtkBuilder   *builder,
					 const gchar  *childname)
{
  GtkWidgetClass *class;
  GSList *l;
  GType internal_child_type = 0;

  if (strcmp (childname, "accessible") == 0)
    return G_OBJECT (gtk_widget_get_accessible (GTK_WIDGET (buildable)));

  /* Find a widget type which has declared an automated child as internal by
   * the name 'childname', if any.
   */
  for (class = GTK_WIDGET_GET_CLASS (buildable);
       GTK_IS_WIDGET_CLASS (class);
       class = g_type_class_peek_parent (class))
    {
      GtkWidgetTemplate *template = class->priv->template;

      if (!template)
	continue;

      for (l = template->children; l && internal_child_type == 0; l = l->next)
	{
	  AutomaticChildClass *child_class = l->data;

	  if (child_class->internal_child && strcmp (childname, child_class->name) == 0)
	    internal_child_type = G_OBJECT_CLASS_TYPE (class);
	}
    }

  /* Now return the 'internal-child' from the class which declared it, note
   * that gtk_widget_get_template_child() an API used to access objects
   * which are in the private scope of a given class.
   */
  if (internal_child_type != 0)
    return gtk_widget_get_template_child (GTK_WIDGET (buildable), internal_child_type, childname);

  return NULL;
}

static void
gtk_widget_buildable_set_buildable_property (GtkBuildable *buildable,
					     GtkBuilder   *builder,
					     const gchar  *name,
					     const GValue *value)
{
  if (strcmp (name, "has-default") == 0 && g_value_get_boolean (value))
      g_object_set_qdata (G_OBJECT (buildable), quark_builder_has_default,
			  GINT_TO_POINTER (TRUE));
  else if (strcmp (name, "has-focus") == 0 && g_value_get_boolean (value))
      g_object_set_qdata (G_OBJECT (buildable), quark_builder_has_focus,
			  GINT_TO_POINTER (TRUE));
  else
    g_object_set_property (G_OBJECT (buildable), name, value);
}

typedef struct
{
  gchar *action_name;
  GString *description;
  gchar *context;
  gboolean translatable;
} AtkActionData;

typedef struct
{
  gchar *target;
  gchar *type;
} AtkRelationData;

static void
free_action (AtkActionData *data, gpointer user_data)
{
  g_free (data->action_name);
  g_string_free (data->description, TRUE);
  g_free (data->context);
  g_slice_free (AtkActionData, data);
}

static void
free_relation (AtkRelationData *data, gpointer user_data)
{
  g_free (data->target);
  g_free (data->type);
  g_slice_free (AtkRelationData, data);
}

static void
gtk_widget_buildable_parser_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder)
{
  GSList *atk_relations;

  if (g_object_get_qdata (G_OBJECT (buildable), quark_builder_has_default))
    gtk_widget_grab_default (GTK_WIDGET (buildable));
  if (g_object_get_qdata (G_OBJECT (buildable), quark_builder_has_focus))
    gtk_widget_grab_focus (GTK_WIDGET (buildable));

  atk_relations = g_object_get_qdata (G_OBJECT (buildable),
				      quark_builder_atk_relations);
  if (atk_relations)
    {
      AtkObject *accessible;
      AtkRelationSet *relation_set;
      GSList *l;
      GObject *target;
      AtkRelationType relation_type;
      AtkObject *target_accessible;

      accessible = gtk_widget_get_accessible (GTK_WIDGET (buildable));
      relation_set = atk_object_ref_relation_set (accessible);

      for (l = atk_relations; l; l = l->next)
	{
	  AtkRelationData *relation = (AtkRelationData*)l->data;

	  target = gtk_builder_get_object (builder, relation->target);
	  if (!target)
	    {
	      g_warning ("Target object %s in <relation> does not exist",
			 relation->target);
	      continue;
	    }
	  target_accessible = gtk_widget_get_accessible (GTK_WIDGET (target));
	  g_assert (target_accessible != NULL);

	  relation_type = atk_relation_type_for_name (relation->type);
	  if (relation_type == ATK_RELATION_NULL)
	    {
	      g_warning ("<relation> type %s not found",
			 relation->type);
	      continue;
	    }
	  atk_relation_set_add_relation_by_type (relation_set, relation_type,
						 target_accessible);
	}
      g_object_unref (relation_set);

      g_slist_free_full (atk_relations, (GDestroyNotify) free_relation);
      g_object_set_qdata (G_OBJECT (buildable), quark_builder_atk_relations,
			  NULL);
    }
}

typedef struct
{
  GSList *actions;
  GSList *relations;
} AccessibilitySubParserData;

static void
accessibility_start_element (GMarkupParseContext  *context,
			     const gchar          *element_name,
			     const gchar         **names,
			     const gchar         **values,
			     gpointer              user_data,
			     GError              **error)
{
  AccessibilitySubParserData *data = (AccessibilitySubParserData*)user_data;
  guint i;
  gint line_number, char_number;

  if (strcmp (element_name, "relation") == 0)
    {
      gchar *target = NULL;
      gchar *type = NULL;
      AtkRelationData *relation;

      for (i = 0; names[i]; i++)
	{
	  if (strcmp (names[i], "target") == 0)
	    target = g_strdup (values[i]);
	  else if (strcmp (names[i], "type") == 0)
	    type = g_strdup (values[i]);
	  else
	    {
	      g_markup_parse_context_get_position (context,
						   &line_number,
						   &char_number);
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
			   "%s:%d:%d '%s' is not a valid attribute of <%s>",
			   "<input>",
			   line_number, char_number, names[i], "relation");
	      g_free (target);
	      g_free (type);
	      return;
	    }
	}

      if (!target || !type)
	{
	  g_markup_parse_context_get_position (context,
					       &line_number,
					       &char_number);
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
		       "%s:%d:%d <%s> requires attribute \"%s\"",
		       "<input>",
		       line_number, char_number, "relation",
		       type ? "target" : "type");
	  g_free (target);
	  g_free (type);
	  return;
	}

      relation = g_slice_new (AtkRelationData);
      relation->target = target;
      relation->type = type;

      data->relations = g_slist_prepend (data->relations, relation);
    }
  else if (strcmp (element_name, "action") == 0)
    {
      const gchar *action_name = NULL;
      const gchar *description = NULL;
      const gchar *msg_context = NULL;
      gboolean translatable = FALSE;
      AtkActionData *action;

      for (i = 0; names[i]; i++)
	{
	  if (strcmp (names[i], "action_name") == 0)
	    action_name = values[i];
	  else if (strcmp (names[i], "description") == 0)
	    description = values[i];
          else if (strcmp (names[i], "translatable") == 0)
            {
              if (!_gtk_builder_boolean_from_string (values[i], &translatable, error))
                return;
            }
          else if (strcmp (names[i], "comments") == 0)
            {
              /* do nothing, comments are for translators */
            }
          else if (strcmp (names[i], "context") == 0)
            msg_context = values[i];
	  else
	    {
	      g_markup_parse_context_get_position (context,
						   &line_number,
						   &char_number);
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
			   "%s:%d:%d '%s' is not a valid attribute of <%s>",
			   "<input>",
			   line_number, char_number, names[i], "action");
	      return;
	    }
	}

      if (!action_name)
	{
	  g_markup_parse_context_get_position (context,
					       &line_number,
					       &char_number);
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
		       "%s:%d:%d <%s> requires attribute \"%s\"",
		       "<input>",
		       line_number, char_number, "action",
		       "action_name");
	  return;
	}

      action = g_slice_new (AtkActionData);
      action->action_name = g_strdup (action_name);
      action->description = g_string_new (description);
      action->context = g_strdup (msg_context);
      action->translatable = translatable;

      data->actions = g_slist_prepend (data->actions, action);
    }
  else if (strcmp (element_name, "accessibility") == 0)
    ;
  else
    g_warning ("Unsupported tag for GtkWidget: %s\n", element_name);
}

static void
accessibility_text (GMarkupParseContext  *context,
                    const gchar          *text,
                    gsize                 text_len,
                    gpointer              user_data,
                    GError              **error)
{
  AccessibilitySubParserData *data = (AccessibilitySubParserData*)user_data;

  if (strcmp (g_markup_parse_context_get_element (context), "action") == 0)
    {
      AtkActionData *action = data->actions->data;

      g_string_append_len (action->description, text, text_len);
    }
}

static const GMarkupParser accessibility_parser =
  {
    accessibility_start_element,
    NULL,
    accessibility_text,
  };

typedef struct
{
  GObject *object;
  guint    key;
  guint    modifiers;
  gchar   *signal;
} AccelGroupParserData;

static void
accel_group_start_element (GMarkupParseContext  *context,
			   const gchar          *element_name,
			   const gchar         **names,
			   const gchar         **values,
			   gpointer              user_data,
			   GError              **error)
{
  gint i;
  guint key = 0;
  guint modifiers = 0;
  gchar *signal = NULL;
  AccelGroupParserData *parser_data = (AccelGroupParserData*)user_data;

  for (i = 0; names[i]; i++)
    {
      if (strcmp (names[i], "key") == 0)
	key = gdk_keyval_from_name (values[i]);
      else if (strcmp (names[i], "modifiers") == 0)
	{
	  if (!_gtk_builder_flags_from_string (GDK_TYPE_MODIFIER_TYPE,
					       values[i],
					       &modifiers,
					       error))
	      return;
	}
      else if (strcmp (names[i], "signal") == 0)
	signal = g_strdup (values[i]);
    }

  if (key == 0 || signal == NULL)
    {
      g_warning ("<accelerator> requires key and signal attributes");
      return;
    }
  parser_data->key = key;
  parser_data->modifiers = modifiers;
  parser_data->signal = signal;
}

static const GMarkupParser accel_group_parser =
  {
    accel_group_start_element,
  };

typedef struct
{
  GSList *classes;
} StyleParserData;

static void
style_start_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **names,
                     const gchar         **values,
                     gpointer              user_data,
                     GError              **error)
{
  StyleParserData *style_data = (StyleParserData *)user_data;
  gchar *class_name;

  if (strcmp (element_name, "class") == 0)
    {
      if (g_markup_collect_attributes (element_name,
                                       names,
                                       values,
                                       error,
                                       G_MARKUP_COLLECT_STRDUP, "name", &class_name,
                                       G_MARKUP_COLLECT_INVALID))
        {
          style_data->classes = g_slist_append (style_data->classes, class_name);
        }
    }
  else if (strcmp (element_name, "style") == 0)
    ;
  else
    g_warning ("Unsupported tag for GtkWidget: %s\n", element_name);
}

static const GMarkupParser style_parser =
  {
    style_start_element,
  };

static gboolean
gtk_widget_buildable_custom_tag_start (GtkBuildable     *buildable,
				       GtkBuilder       *builder,
				       GObject          *child,
				       const gchar      *tagname,
				       GMarkupParser    *parser,
				       gpointer         *data)
{
  g_assert (buildable);

  if (strcmp (tagname, "accelerator") == 0)
    {
      AccelGroupParserData *parser_data;

      parser_data = g_slice_new0 (AccelGroupParserData);
      parser_data->object = g_object_ref (buildable);
      *parser = accel_group_parser;
      *data = parser_data;
      return TRUE;
    }
  if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilitySubParserData *parser_data;

      parser_data = g_slice_new0 (AccessibilitySubParserData);
      *parser = accessibility_parser;
      *data = parser_data;
      return TRUE;
    }
  if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *parser_data;

      parser_data = g_slice_new0 (StyleParserData);
      *parser = style_parser;
      *data = parser_data;
      return TRUE;
    }

  return FALSE;
}

void
_gtk_widget_buildable_finish_accelerator (GtkWidget *widget,
					  GtkWidget *toplevel,
					  gpointer   user_data)
{
  AccelGroupParserData *accel_data;
  GSList *accel_groups;
  GtkAccelGroup *accel_group;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (toplevel));
  g_return_if_fail (user_data != NULL);

  accel_data = (AccelGroupParserData*)user_data;
  accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
  if (g_slist_length (accel_groups) == 0)
    {
      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (toplevel), accel_group);
    }
  else
    {
      g_assert (g_slist_length (accel_groups) == 1);
      accel_group = g_slist_nth_data (accel_groups, 0);
    }

  gtk_widget_add_accelerator (GTK_WIDGET (accel_data->object),
			      accel_data->signal,
			      accel_group,
			      accel_data->key,
			      accel_data->modifiers,
			      GTK_ACCEL_VISIBLE);

  g_object_unref (accel_data->object);
  g_free (accel_data->signal);
  g_slice_free (AccelGroupParserData, accel_data);
}

static void
gtk_widget_buildable_custom_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder,
				      GObject      *child,
				      const gchar  *tagname,
				      gpointer      user_data)
{
  if (strcmp (tagname, "accelerator") == 0)
    {
      AccelGroupParserData *accel_data;
      GtkWidget *toplevel;

      accel_data = (AccelGroupParserData*)user_data;
      g_assert (accel_data->object);

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (accel_data->object));

      _gtk_widget_buildable_finish_accelerator (GTK_WIDGET (buildable), toplevel, user_data);
    }
  else if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilitySubParserData *a11y_data;

      a11y_data = (AccessibilitySubParserData*)user_data;

      if (a11y_data->actions)
	{
	  AtkObject *accessible;
	  AtkAction *action;
	  gint i, n_actions;
	  GSList *l;

	  accessible = gtk_widget_get_accessible (GTK_WIDGET (buildable));

          if (ATK_IS_ACTION (accessible))
            {
	      action = ATK_ACTION (accessible);
	      n_actions = atk_action_get_n_actions (action);

	      for (l = a11y_data->actions; l; l = l->next)
	        {
	          AtkActionData *action_data = (AtkActionData*)l->data;

	          for (i = 0; i < n_actions; i++)
		    if (strcmp (atk_action_get_name (action, i),
		  	        action_data->action_name) == 0)
		      break;

	          if (i < n_actions)
                    {
                      const gchar *description;

                      if (action_data->translatable && action_data->description->len)
                        description = _gtk_builder_parser_translate (gtk_builder_get_translation_domain (builder),
                                                                     action_data->context,
                                                                     action_data->description->str);
                      else
                        description = action_data->description->str;

		      atk_action_set_description (action, i, description);
                    }
                }
	    }
          else
            g_warning ("accessibility action on a widget that does not implement AtkAction");

	  g_slist_free_full (a11y_data->actions, (GDestroyNotify) free_action);
	}

      if (a11y_data->relations)
	g_object_set_qdata (G_OBJECT (buildable), quark_builder_atk_relations,
			    a11y_data->relations);

      g_slice_free (AccessibilitySubParserData, a11y_data);
    }
  else if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *style_data = (StyleParserData *)user_data;
      GtkStyleContext *context;
      GSList *l;

      context = gtk_widget_get_style_context (GTK_WIDGET (buildable));

      for (l = style_data->classes; l; l = l->next)
        gtk_style_context_add_class (context, (const gchar *)l->data);

      gtk_widget_reset_style (GTK_WIDGET (buildable));

      g_slist_free_full (style_data->classes, g_free);
      g_slice_free (StyleParserData, style_data);
    }
}

static GtkSizeRequestMode 
gtk_widget_real_get_request_mode (GtkWidget *widget)
{ 
  /* By default widgets dont trade size at all. */
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_widget_real_get_width (GtkWidget *widget,
			   gint      *minimum_size,
			   gint      *natural_size)
{
  *minimum_size = 0;
  *natural_size = 0;
}

static void
gtk_widget_real_get_height (GtkWidget *widget,
			    gint      *minimum_size,
			    gint      *natural_size)
{
  *minimum_size = 0;
  *natural_size = 0;
}

static void
gtk_widget_real_get_height_for_width (GtkWidget *widget,
                                      gint       width,
                                      gint      *minimum_height,
                                      gint      *natural_height)
{
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, minimum_height, natural_height);
}

static void
gtk_widget_real_get_width_for_height (GtkWidget *widget,
                                      gint       height,
                                      gint      *minimum_width,
                                      gint      *natural_width)
{
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_width, natural_width);
}

/**
 * gtk_widget_get_halign:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:halign property.
 *
 * For backwards compatibility reasons this method will never return
 * %GTK_ALIGN_BASELINE, but instead it will convert it to
 * %GTK_ALIGN_FILL. Baselines are not supported for horizontal
 * alignment.
 *
 * Returns: the horizontal alignment of @widget
 */
GtkAlign
gtk_widget_get_halign (GtkWidget *widget)
{
  GtkAlign align;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);

  align = _gtk_widget_get_aux_info_or_defaults (widget)->halign;
  if (align == GTK_ALIGN_BASELINE)
    return GTK_ALIGN_FILL;
  return align;
}

/**
 * gtk_widget_set_halign:
 * @widget: a #GtkWidget
 * @align: the horizontal alignment
 *
 * Sets the horizontal alignment of @widget.
 * See the #GtkWidget:halign property.
 */
void
gtk_widget_set_halign (GtkWidget *widget,
                       GtkAlign   align)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->halign == align)
    return;

  aux_info->halign = align;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "halign");
}

/**
 * gtk_widget_get_valign_with_baseline:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:valign property, including
 * %GTK_ALIGN_BASELINE.
 *
 * Returns: the vertical alignment of @widget
 *
 * Since: 3.10
 */
GtkAlign
gtk_widget_get_valign_with_baseline (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);
  return _gtk_widget_get_aux_info_or_defaults (widget)->valign;
}

/**
 * gtk_widget_get_valign:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:valign property.
 *
 * For backwards compatibility reasons this method will never return
 * %GTK_ALIGN_BASELINE, but instead it will convert it to
 * %GTK_ALIGN_FILL. If your widget want to support baseline aligned
 * children it must use gtk_widget_get_valign_with_baseline(), or
 * `g_object_get (widget, "valign", &value, NULL)`, which will
 * also report the true value.
 *
 * Returns: the vertical alignment of @widget, ignoring baseline alignment
 */
GtkAlign
gtk_widget_get_valign (GtkWidget *widget)
{
  GtkAlign align;

  align = gtk_widget_get_valign_with_baseline (widget);
  if (align == GTK_ALIGN_BASELINE)
    return GTK_ALIGN_FILL;
  return align;
}

/**
 * gtk_widget_set_valign:
 * @widget: a #GtkWidget
 * @align: the vertical alignment
 *
 * Sets the vertical alignment of @widget.
 * See the #GtkWidget:valign property.
 */
void
gtk_widget_set_valign (GtkWidget *widget,
                       GtkAlign   align)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->valign == align)
    return;

  aux_info->valign = align;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "valign");
}

/**
 * gtk_widget_get_margin_left:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-left property.
 *
 * Returns: The left margin of @widget
 *
 * Deprecated: 3.12: Use gtk_widget_get_margin_start() instead.
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_left (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.left;
}

/**
 * gtk_widget_set_margin_left:
 * @widget: a #GtkWidget
 * @margin: the left margin
 *
 * Sets the left margin of @widget.
 * See the #GtkWidget:margin-left property.
 *
 * Deprecated: 3.12: Use gtk_widget_set_margin_start() instead.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_left (GtkWidget *widget,
                            gint       margin)
{
  GtkWidgetAuxInfo *aux_info;
  gboolean rtl;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  if (aux_info->margin.left == margin)
    return;

  aux_info->margin.left = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-left");
  g_object_notify (G_OBJECT (widget), rtl ? "margin-end" : "margin-start");
}

/**
 * gtk_widget_get_margin_right:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-right property.
 *
 * Returns: The right margin of @widget
 *
 * Deprecated: 3.12: Use gtk_widget_get_margin_end() instead.
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_right (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.right;
}

/**
 * gtk_widget_set_margin_right:
 * @widget: a #GtkWidget
 * @margin: the right margin
 *
 * Sets the right margin of @widget.
 * See the #GtkWidget:margin-right property.
 *
 * Deprecated: 3.12: Use gtk_widget_set_margin_end() instead.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_right (GtkWidget *widget,
                             gint       margin)
{
  GtkWidgetAuxInfo *aux_info;
  gboolean rtl;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  if (aux_info->margin.right == margin)
    return;

  aux_info->margin.right = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-right");
  g_object_notify (G_OBJECT (widget), rtl ? "margin-start" : "margin-end");
}

/**
 * gtk_widget_get_margin_start:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-start property.
 *
 * Returns: The start margin of @widget
 *
 * Since: 3.12
 */
gint
gtk_widget_get_margin_start (GtkWidget *widget)
{
  const GtkWidgetAuxInfo *aux_info;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    return aux_info->margin.right;
  else
    return aux_info->margin.left;
}

/**
 * gtk_widget_set_margin_start:
 * @widget: a #GtkWidget
 * @margin: the start margin
 *
 * Sets the start margin of @widget.
 * See the #GtkWidget:margin-start property.
 *
 * Since: 3.12
 */
void
gtk_widget_set_margin_start (GtkWidget *widget,
                             gint       margin)
{
  GtkWidgetAuxInfo *aux_info;
  gint16 *start;
  gboolean rtl;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  if (rtl)
    start = &aux_info->margin.right;
  else
    start = &aux_info->margin.left;

  if (*start == margin)
    return;

  *start = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-start");
  g_object_notify (G_OBJECT (widget), rtl ? "margin-right" : "margin-left");
}

/**
 * gtk_widget_get_margin_end:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-end property.
 *
 * Returns: The end margin of @widget
 *
 * Since: 3.12
 */
gint
gtk_widget_get_margin_end (GtkWidget *widget)
{
  const GtkWidgetAuxInfo *aux_info;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    return aux_info->margin.left;
  else
    return aux_info->margin.right;
}

/**
 * gtk_widget_set_margin_end:
 * @widget: a #GtkWidget
 * @margin: the end margin
 *
 * Sets the end margin of @widget.
 * See the #GtkWidget:margin-end property.
 *
 * Since: 3.12
 */
void
gtk_widget_set_margin_end (GtkWidget *widget,
                           gint       margin)
{
  GtkWidgetAuxInfo *aux_info;
  gint16 *end;
  gboolean rtl;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  if (rtl)
    end = &aux_info->margin.left;
  else
    end = &aux_info->margin.right;

  if (*end == margin)
    return;

  *end = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-end");
  g_object_notify (G_OBJECT (widget), rtl ? "margin-left" : "margin-right");
}

/**
 * gtk_widget_get_margin_top:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-top property.
 *
 * Returns: The top margin of @widget
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_top (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.top;
}

/**
 * gtk_widget_set_margin_top:
 * @widget: a #GtkWidget
 * @margin: the top margin
 *
 * Sets the top margin of @widget.
 * See the #GtkWidget:margin-top property.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_top (GtkWidget *widget,
                           gint       margin)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->margin.top == margin)
    return;

  aux_info->margin.top = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-top");
}

/**
 * gtk_widget_get_margin_bottom:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-bottom property.
 *
 * Returns: The bottom margin of @widget
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_bottom (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.bottom;
}

/**
 * gtk_widget_set_margin_bottom:
 * @widget: a #GtkWidget
 * @margin: the bottom margin
 *
 * Sets the bottom margin of @widget.
 * See the #GtkWidget:margin-bottom property.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_bottom (GtkWidget *widget,
                              gint       margin)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->margin.bottom == margin)
    return;

  aux_info->margin.bottom = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-bottom");
}

/**
 * gtk_widget_get_clipboard:
 * @widget: a #GtkWidget
 * @selection: a #GdkAtom which identifies the clipboard
 *             to use. %GDK_SELECTION_CLIPBOARD gives the
 *             default clipboard. Another common value
 *             is %GDK_SELECTION_PRIMARY, which gives
 *             the primary X selection.
 *
 * Returns the clipboard object for the given selection to
 * be used with @widget. @widget must have a #GdkDisplay
 * associated with it, so must be attached to a toplevel
 * window.
 *
 * Returns: (transfer none): the appropriate clipboard object. If no
 *             clipboard already exists, a new one will
 *             be created. Once a clipboard object has
 *             been created, it is persistent for all time.
 *
 * Since: 2.2
 **/
GtkClipboard *
gtk_widget_get_clipboard (GtkWidget *widget, GdkAtom selection)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_has_screen (widget), NULL);

  return gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
					selection);
}

/**
 * gtk_widget_list_mnemonic_labels:
 * @widget: a #GtkWidget
 *
 * Returns a newly allocated list of the widgets, normally labels, for
 * which this widget is the target of a mnemonic (see for example,
 * gtk_label_set_mnemonic_widget()).

 * The widgets in the list are not individually referenced. If you
 * want to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets, you
 * must call `g_list_foreach (result,
 * (GFunc)g_object_ref, NULL)` first, and then unref all the
 * widgets afterwards.

 * Returns: (element-type GtkWidget) (transfer container): the list of
 *  mnemonic labels; free this list
 *  with g_list_free() when you are done with it.
 *
 * Since: 2.4
 **/
GList *
gtk_widget_list_mnemonic_labels (GtkWidget *widget)
{
  GList *list = NULL;
  GSList *l;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (l = g_object_get_qdata (G_OBJECT (widget), quark_mnemonic_labels); l; l = l->next)
    list = g_list_prepend (list, l->data);

  return list;
}

/**
 * gtk_widget_add_mnemonic_label:
 * @widget: a #GtkWidget
 * @label: a #GtkWidget that acts as a mnemonic label for @widget
 *
 * Adds a widget to the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). Note the
 * list of mnemonic labels for the widget is cleared when the
 * widget is destroyed, so the caller must make sure to update
 * its internal state at this point as well, by using a connection
 * to the #GtkWidget::destroy signal or a weak notifier.
 *
 * Since: 2.4
 **/
void
gtk_widget_add_mnemonic_label (GtkWidget *widget,
                               GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_prepend (old_list, label);

  g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
			   new_list, (GDestroyNotify) g_slist_free);
}

/**
 * gtk_widget_remove_mnemonic_label:
 * @widget: a #GtkWidget
 * @label: a #GtkWidget that was previously set as a mnemnic label for
 *         @widget with gtk_widget_add_mnemonic_label().
 *
 * Removes a widget from the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). The widget
 * must have previously been added to the list with
 * gtk_widget_add_mnemonic_label().
 *
 * Since: 2.4
 **/
void
gtk_widget_remove_mnemonic_label (GtkWidget *widget,
                                  GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_remove (old_list, label);

  if (new_list)
    g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
			     new_list, (GDestroyNotify) g_slist_free);
}

/**
 * gtk_widget_get_no_show_all:
 * @widget: a #GtkWidget
 *
 * Returns the current value of the #GtkWidget:no-show-all property,
 * which determines whether calls to gtk_widget_show_all()
 * will affect this widget.
 *
 * Returns: the current value of the “no-show-all” property.
 *
 * Since: 2.4
 **/
gboolean
gtk_widget_get_no_show_all (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->no_show_all;
}

/**
 * gtk_widget_set_no_show_all:
 * @widget: a #GtkWidget
 * @no_show_all: the new value for the “no-show-all” property
 *
 * Sets the #GtkWidget:no-show-all property, which determines whether
 * calls to gtk_widget_show_all() will affect this widget.
 *
 * This is mostly for use in constructing widget hierarchies with externally
 * controlled visibility, see #GtkUIManager.
 *
 * Since: 2.4
 **/
void
gtk_widget_set_no_show_all (GtkWidget *widget,
			    gboolean   no_show_all)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  no_show_all = (no_show_all != FALSE);

  if (widget->priv->no_show_all != no_show_all)
    {
      widget->priv->no_show_all = no_show_all;

      g_object_notify (G_OBJECT (widget), "no-show-all");
    }
}


static void
gtk_widget_real_set_has_tooltip (GtkWidget *widget,
			         gboolean   has_tooltip,
			         gboolean   force)
{
  GtkWidgetPrivate *priv = widget->priv;
  gboolean priv_has_tooltip;

  priv_has_tooltip = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (widget),
				       quark_has_tooltip));

  if (priv_has_tooltip != has_tooltip || force)
    {
      priv_has_tooltip = has_tooltip;

      if (priv_has_tooltip)
        {
	  if (gtk_widget_get_realized (widget) && !gtk_widget_get_has_window (widget))
	    gdk_window_set_events (priv->window,
				   gdk_window_get_events (priv->window) |
				   GDK_LEAVE_NOTIFY_MASK |
				   GDK_POINTER_MOTION_MASK);

	  if (gtk_widget_get_has_window (widget))
	      gtk_widget_add_events (widget,
				     GDK_LEAVE_NOTIFY_MASK |
				     GDK_POINTER_MOTION_MASK);
	}

      g_object_set_qdata (G_OBJECT (widget), quark_has_tooltip,
			  GUINT_TO_POINTER (priv_has_tooltip));

      g_object_notify (G_OBJECT (widget), "has-tooltip");
    }
}

/**
 * gtk_widget_set_tooltip_window:
 * @widget: a #GtkWidget
 * @custom_window: (allow-none): a #GtkWindow, or %NULL
 *
 * Replaces the default, usually yellow, window used for displaying
 * tooltips with @custom_window. GTK+ will take care of showing and
 * hiding @custom_window at the right moment, to behave likewise as
 * the default tooltip window. If @custom_window is %NULL, the default
 * tooltip window will be used.
 *
 * If the custom window should have the default theming it needs to
 * have the name “gtk-tooltip”, see gtk_widget_set_name().
 *
 * Since: 2.12
 */
void
gtk_widget_set_tooltip_window (GtkWidget *widget,
			       GtkWindow *custom_window)
{
  gboolean has_tooltip;
  gchar *tooltip_markup;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (custom_window == NULL || GTK_IS_WINDOW (custom_window));

  tooltip_markup = g_object_get_qdata (G_OBJECT (widget), quark_tooltip_markup);

  if (custom_window)
    g_object_ref (custom_window);

  g_object_set_qdata_full (G_OBJECT (widget), quark_tooltip_window,
			   custom_window, g_object_unref);

  has_tooltip = (custom_window != NULL || tooltip_markup != NULL);
  gtk_widget_real_set_has_tooltip (widget, has_tooltip, FALSE);

  if (has_tooltip && gtk_widget_get_visible (widget))
    gtk_widget_queue_tooltip_query (widget);
}

/**
 * gtk_widget_get_tooltip_window:
 * @widget: a #GtkWidget
 *
 * Returns the #GtkWindow of the current tooltip. This can be the
 * GtkWindow created by default, or the custom tooltip window set
 * using gtk_widget_set_tooltip_window().
 *
 * Returns: (transfer none): The #GtkWindow of the current tooltip.
 *
 * Since: 2.12
 */
GtkWindow *
gtk_widget_get_tooltip_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_get_qdata (G_OBJECT (widget), quark_tooltip_window);
}

/**
 * gtk_widget_trigger_tooltip_query:
 * @widget: a #GtkWidget
 *
 * Triggers a tooltip query on the display where the toplevel of @widget
 * is located. See gtk_tooltip_trigger_tooltip_query() for more
 * information.
 *
 * Since: 2.12
 */
void
gtk_widget_trigger_tooltip_query (GtkWidget *widget)
{
  gtk_tooltip_trigger_tooltip_query (gtk_widget_get_display (widget));
}

static guint tooltip_query_id;
static GSList *tooltip_query_displays;

static gboolean
tooltip_query_idle (gpointer data)
{
  g_slist_foreach (tooltip_query_displays, (GFunc)gtk_tooltip_trigger_tooltip_query, NULL);
  g_slist_foreach (tooltip_query_displays, (GFunc)g_object_unref, NULL);
  g_slist_free (tooltip_query_displays);

  tooltip_query_displays = NULL;
  tooltip_query_id = 0;

  return FALSE;
}

static void
gtk_widget_queue_tooltip_query (GtkWidget *widget)
{
  GdkDisplay *display;

  display = gtk_widget_get_display (widget);

  if (!g_slist_find (tooltip_query_displays, display))
    tooltip_query_displays = g_slist_prepend (tooltip_query_displays, g_object_ref (display));

  if (tooltip_query_id == 0)
    {
      tooltip_query_id = gdk_threads_add_idle (tooltip_query_idle, NULL);
      g_source_set_name_by_id (tooltip_query_id, "[gtk+] tooltip_query_idle");
    }
}

/**
 * gtk_widget_set_tooltip_text:
 * @widget: a #GtkWidget
 * @text: (allow-none): the contents of the tooltip for @widget
 *
 * Sets @text as the contents of the tooltip. This function will take
 * care of setting #GtkWidget:has-tooltip to %TRUE and of the default
 * handler for the #GtkWidget::query-tooltip signal.
 *
 * See also the #GtkWidget:tooltip-text property and gtk_tooltip_set_text().
 *
 * Since: 2.12
 */
void
gtk_widget_set_tooltip_text (GtkWidget   *widget,
                             const gchar *text)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set (G_OBJECT (widget), "tooltip-text", text, NULL);
}

/**
 * gtk_widget_get_tooltip_text:
 * @widget: a #GtkWidget
 *
 * Gets the contents of the tooltip for @widget.
 *
 * Returns: the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
 *
 * Since: 2.12
 */
gchar *
gtk_widget_get_tooltip_text (GtkWidget *widget)
{
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  g_object_get (G_OBJECT (widget), "tooltip-text", &text, NULL);

  return text;
}

/**
 * gtk_widget_set_tooltip_markup:
 * @widget: a #GtkWidget
 * @markup: (allow-none): the contents of the tooltip for @widget, or %NULL
 *
 * Sets @markup as the contents of the tooltip, which is marked up with
 *  the [Pango text markup language][PangoMarkupFormat].
 *
 * This function will take care of setting #GtkWidget:has-tooltip to %TRUE
 * and of the default handler for the #GtkWidget::query-tooltip signal.
 *
 * See also the #GtkWidget:tooltip-markup property and
 * gtk_tooltip_set_markup().
 *
 * Since: 2.12
 */
void
gtk_widget_set_tooltip_markup (GtkWidget   *widget,
                               const gchar *markup)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set (G_OBJECT (widget), "tooltip-markup", markup, NULL);
}

/**
 * gtk_widget_get_tooltip_markup:
 * @widget: a #GtkWidget
 *
 * Gets the contents of the tooltip for @widget.
 *
 * Returns: the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
 *
 * Since: 2.12
 */
gchar *
gtk_widget_get_tooltip_markup (GtkWidget *widget)
{
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  g_object_get (G_OBJECT (widget), "tooltip-markup", &text, NULL);

  return text;
}

/**
 * gtk_widget_set_has_tooltip:
 * @widget: a #GtkWidget
 * @has_tooltip: whether or not @widget has a tooltip.
 *
 * Sets the has-tooltip property on @widget to @has_tooltip.  See
 * #GtkWidget:has-tooltip for more information.
 *
 * Since: 2.12
 */
void
gtk_widget_set_has_tooltip (GtkWidget *widget,
			    gboolean   has_tooltip)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set (G_OBJECT (widget), "has-tooltip", has_tooltip, NULL);
}

/**
 * gtk_widget_get_has_tooltip:
 * @widget: a #GtkWidget
 *
 * Returns the current value of the has-tooltip property.  See
 * #GtkWidget:has-tooltip for more information.
 *
 * Returns: current value of has-tooltip on @widget.
 *
 * Since: 2.12
 */
gboolean
gtk_widget_get_has_tooltip (GtkWidget *widget)
{
  gboolean has_tooltip = FALSE;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  g_object_get (G_OBJECT (widget), "has-tooltip", &has_tooltip, NULL);

  return has_tooltip;
}

/**
 * gtk_widget_get_clip:
 * @widget: a #GtkWidget
 * @clip: (out): a pointer to a #GtkAllocation to copy to
 *
 * Retrieves the widget’s clip area.
 *
 * The clip area is the area in which all of @widget's drawing will
 * happen. Other toolkits call it the bounding box.
 *
 * Historically, in GTK+ the clip area has been equal to the allocation
 * retrieved via gtk_widget_get_allocation().
 *
 * Since: 3.14
 */
void
gtk_widget_get_clip (GtkWidget     *widget,
                     GtkAllocation *clip)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (clip != NULL);

  priv = widget->priv;

  *clip = priv->clip;
}

/**
 * gtk_widget_set_clip:
 * @widget: a #GtkWidget
 * @clip: a pointer to a #GtkAllocation to copy from
 *
 * Sets the widget’s clip.  This must not be used directly,
 * but from within a widget’s size_allocate method.
 * It must be called after gtk_widget_set_allocation() (or after chaning up
 * to the parent class), because that function resets the clip.
 *
 * The clip set should be the area that @widget draws on. If @widget is a
 * #GtkContainer, the area must contain all children's clips.
 *
 * If this function is not called by @widget during a ::size-allocate handler,
 * the clip will be set to @widget's allocation.
 *
 * Since: 3.14
 */
void
gtk_widget_set_clip (GtkWidget           *widget,
                     const GtkAllocation *clip)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_visible (widget) || gtk_widget_is_toplevel (widget));
  g_return_if_fail (clip != NULL);

  priv = widget->priv;

#ifdef G_ENABLE_DEBUG
  if (gtk_get_debug_flags () & GTK_DEBUG_GEOMETRY)
    {
      gint depth;
      GtkWidget *parent;
      const gchar *name;

      depth = 0;
      parent = widget;
      while (parent)
	{
	  depth++;
	  parent = gtk_widget_get_parent (parent);
	}

      name = g_type_name (G_OBJECT_TYPE (G_OBJECT (widget)));
      g_print ("gtk_widget_set_clip:      %*s%s %d %d %d %d\n",
	       2 * depth, " ", name,
	       clip->x, clip->y,
	       clip->width, clip->height);
    }
#endif /* G_ENABLE_DEBUG */

  priv->clip = *clip;
}

static void
union_with_clip (GtkWidget *widget,
                 gpointer   clip)
{
  GtkAllocation widget_clip;

  if (!gtk_widget_is_visible (widget) ||
      !gtk_widget_get_child_visible (widget))
    return;

  gtk_widget_get_clip (widget, &widget_clip);

  gdk_rectangle_union (&widget_clip, clip, clip);
}

/*
 * _gtk_widget_set_simple_clip:
 * @widget: a #GtkWidget
 * @content_clip: Clipping area of the contents or %NULL, if the contents
 *     do not extent the allocation.
 *
 * This is a convenience function for gtk_widget_set_clip(), if you
 * just want to set the clip for @widget based on its allocation,
 * CSS properties and - if the widget is a #GtkContainer - its
 * children. gtk_widget_set_allocation() must have been called
 * and all children must have been allocated with
 * gtk_widget_size_allocate() before calling this function.
 * It is therefore a good idea to call this function last in
 * your implementation of GtkWidget::size_allocate().
 *
 * If your widget overdraws its contents, you cannot use this
 * function and must call gtk_widget_set_clip() yourself.
 **/
void
_gtk_widget_set_simple_clip (GtkWidget     *widget,
                             GtkAllocation *content_clip)
{
  GtkStyleContext *context;
  GtkAllocation clip, allocation;
  GtkBorder extents;

  context = gtk_widget_get_style_context (widget);

  gtk_widget_get_allocation (widget, &allocation);

  _gtk_css_shadows_value_get_extents (_gtk_style_context_peek_property (context,
                                                                        GTK_CSS_PROPERTY_BOX_SHADOW),
                                      &extents);

  clip = allocation;
  clip.x -= extents.left;
  clip.y -= extents.top;
  clip.width += extents.left + extents.right;
  clip.height += extents.top + extents.bottom;

  if (content_clip)
    gdk_rectangle_union (content_clip, &clip, &clip);

  if (GTK_IS_CONTAINER (widget))
    {
      if (gtk_widget_get_has_window (widget))
        {
          clip.x -= allocation.x;
          clip.y -= allocation.y;
        }

      gtk_container_forall (GTK_CONTAINER (widget), union_with_clip, &clip);

      if (gtk_widget_get_has_window (widget))
        {
          clip.x += allocation.x;
          clip.y += allocation.y;
        }
    }

  gtk_widget_set_clip (widget, &clip);
}

/**
 * gtk_widget_get_allocation:
 * @widget: a #GtkWidget
 * @allocation: (out): a pointer to a #GtkAllocation to copy to
 *
 * Retrieves the widget’s allocation.
 *
 * Note, when implementing a #GtkContainer: a widget’s allocation will
 * be its “adjusted” allocation, that is, the widget’s parent
 * container typically calls gtk_widget_size_allocate() with an
 * allocation, and that allocation is then adjusted (to handle margin
 * and alignment for example) before assignment to the widget.
 * gtk_widget_get_allocation() returns the adjusted allocation that
 * was actually assigned to the widget. The adjusted allocation is
 * guaranteed to be completely contained within the
 * gtk_widget_size_allocate() allocation, however. So a #GtkContainer
 * is guaranteed that its children stay inside the assigned bounds,
 * but not that they have exactly the bounds the container assigned.
 * There is no way to get the original allocation assigned by
 * gtk_widget_size_allocate(), since it isn’t stored; if a container
 * implementation needs that information it will have to track it itself.
 *
 * Since: 2.18
 */
void
gtk_widget_get_allocation (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (allocation != NULL);

  priv = widget->priv;

  *allocation = priv->allocation;
}

/**
 * gtk_widget_set_allocation:
 * @widget: a #GtkWidget
 * @allocation: a pointer to a #GtkAllocation to copy from
 *
 * Sets the widget’s allocation.  This should not be used
 * directly, but from within a widget’s size_allocate method.
 *
 * The allocation set should be the “adjusted” or actual
 * allocation. If you’re implementing a #GtkContainer, you want to use
 * gtk_widget_size_allocate() instead of gtk_widget_set_allocation().
 * The GtkWidgetClass::adjust_size_allocation virtual method adjusts the
 * allocation inside gtk_widget_size_allocate() to create an adjusted
 * allocation.
 *
 * Since: 2.18
 */
void
gtk_widget_set_allocation (GtkWidget           *widget,
                           const GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_visible (widget) || gtk_widget_is_toplevel (widget));
  g_return_if_fail (allocation != NULL);

  priv = widget->priv;

  priv->allocation = *allocation;
  priv->clip = *allocation;
}

/**
 * gtk_widget_get_allocated_width:
 * @widget: the widget to query
 *
 * Returns the width that has currently been allocated to @widget.
 * This function is intended to be used when implementing handlers
 * for the #GtkWidget::draw function.
 *
 * Returns: the width of the @widget
 **/
int
gtk_widget_get_allocated_width (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return widget->priv->allocation.width;
}

/**
 * gtk_widget_get_allocated_height:
 * @widget: the widget to query
 *
 * Returns the height that has currently been allocated to @widget.
 * This function is intended to be used when implementing handlers
 * for the #GtkWidget::draw function.
 *
 * Returns: the height of the @widget
 **/
int
gtk_widget_get_allocated_height (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return widget->priv->allocation.height;
}

/**
 * gtk_widget_get_allocated_baseline:
 * @widget: the widget to query
 *
 * Returns the baseline that has currently been allocated to @widget.
 * This function is intended to be used when implementing handlers
 * for the #GtkWidget::draw function, and when allocating child
 * widgets in #GtkWidget::size_allocate.
 *
 * Returns: the baseline of the @widget, or -1 if none
 *
 * Since: 3.10
 **/
int
gtk_widget_get_allocated_baseline (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return widget->priv->allocated_baseline;
}

/**
 * gtk_widget_get_requisition:
 * @widget: a #GtkWidget
 * @requisition: (out): a pointer to a #GtkRequisition to copy to
 *
 * Retrieves the widget’s requisition.
 *
 * This function should only be used by widget implementations in
 * order to figure whether the widget’s requisition has actually
 * changed after some internal state change (so that they can call
 * gtk_widget_queue_resize() instead of gtk_widget_queue_draw()).
 *
 * Normally, gtk_widget_size_request() should be used.
 *
 * Since: 2.20
 *
 * Deprecated: 3.0: The #GtkRequisition cache on the widget was
 * removed, If you need to cache sizes across requests and allocations,
 * add an explicit cache to the widget in question instead.
 */
void
gtk_widget_get_requisition (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (requisition != NULL);

  gtk_widget_get_preferred_size (widget, requisition, NULL);
}

/**
 * gtk_widget_set_window:
 * @widget: a #GtkWidget
 * @window: (transfer full): a #GdkWindow
 *
 * Sets a widget’s window. This function should only be used in a
 * widget’s #GtkWidget::realize implementation. The %window passed is
 * usually either new window created with gdk_window_new(), or the
 * window of its parent widget as returned by
 * gtk_widget_get_parent_window().
 *
 * Widgets must indicate whether they will create their own #GdkWindow
 * by calling gtk_widget_set_has_window(). This is usually done in the
 * widget’s init() function.
 *
 * Note that this function does not add any reference to @window.
 *
 * Since: 2.18
 */
void
gtk_widget_set_window (GtkWidget *widget,
                       GdkWindow *window)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  priv = widget->priv;

  if (priv->window != window)
    {
      priv->window = window;

      g_object_notify (G_OBJECT (widget), "window");
    }
}

/**
 * gtk_widget_register_window:
 * @widget: a #GtkWidget
 * @window: a #GdkWindow
 *
 * Registers a #GdkWindow with the widget and sets it up so that
 * the widget receives events for it. Call gtk_widget_unregister_window()
 * when destroying the window.
 *
 * Before 3.8 you needed to call gdk_window_set_user_data() directly to set
 * this up. This is now deprecated and you should use gtk_widget_register_window()
 * instead. Old code will keep working as is, although some new features like
 * transparency might not work perfectly.
 *
 * Since: 3.8
 */
void
gtk_widget_register_window (GtkWidget    *widget,
			    GdkWindow    *window)
{
  GtkWidgetPrivate *priv;
  gpointer user_data;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_user_data (window, &user_data);
  g_assert (user_data == NULL);

  priv = widget->priv;

  gdk_window_set_user_data (window, widget);
  priv->registered_windows = g_list_prepend (priv->registered_windows, window);
}

/**
 * gtk_widget_unregister_window:
 * @widget: a #GtkWidget
 * @window: a #GdkWindow
 *
 * Unregisters a #GdkWindow from the widget that was previously set up with
 * gtk_widget_register_window(). You need to call this when the window is
 * no longer used by the widget, such as when you destroy it.
 *
 * Since: 3.8
 */
void
gtk_widget_unregister_window (GtkWidget    *widget,
			      GdkWindow    *window)
{
  GtkWidgetPrivate *priv;
  gpointer user_data;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_WINDOW (window));

  priv = widget->priv;

  gdk_window_get_user_data (window, &user_data);
  g_assert (user_data == widget);
  gdk_window_set_user_data (window, NULL);
  priv->registered_windows = g_list_remove (priv->registered_windows, window);
}

/**
 * gtk_widget_get_window:
 * @widget: a #GtkWidget
 *
 * Returns the widget’s window if it is realized, %NULL otherwise
 *
 * Returns: (transfer none): @widget’s window.
 *
 * Since: 2.14
 */
GdkWindow*
gtk_widget_get_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return widget->priv->window;
}

/**
 * gtk_widget_get_support_multidevice:
 * @widget: a #GtkWidget
 *
 * Returns %TRUE if @widget is multiple pointer aware. See
 * gtk_widget_set_support_multidevice() for more information.
 *
 * Returns: %TRUE if @widget is multidevice aware.
 **/
gboolean
gtk_widget_get_support_multidevice (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->multidevice;
}

/**
 * gtk_widget_set_support_multidevice:
 * @widget: a #GtkWidget
 * @support_multidevice: %TRUE to support input from multiple devices.
 *
 * Enables or disables multiple pointer awareness. If this setting is %TRUE,
 * @widget will start receiving multiple, per device enter/leave events. Note
 * that if custom #GdkWindows are created in #GtkWidget::realize,
 * gdk_window_set_support_multidevice() will have to be called manually on them.
 *
 * Since: 3.0
 **/
void
gtk_widget_set_support_multidevice (GtkWidget *widget,
                                    gboolean   support_multidevice)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;
  priv->multidevice = (support_multidevice == TRUE);

  if (gtk_widget_get_realized (widget))
    gdk_window_set_support_multidevice (priv->window, support_multidevice);
}

/* There are multiple alpha related sources. First of all the user can specify alpha
 * in gtk_widget_set_opacity, secondly we can get it from the css opacity. These two
 * are multiplied together to form the total alpha. Secondly, the user can specify
 * an opacity group for a widget, which means we must essentially handle it as having alpha.
 */

static void
gtk_widget_update_alpha (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GtkStyleContext *context;
  gdouble opacity;
  guint8 alpha;

  priv = widget->priv;

  alpha = priv->user_alpha;

  context = gtk_widget_get_style_context (widget);
  opacity =
    _gtk_css_number_value_get (_gtk_style_context_peek_property (context,
                                                                 GTK_CSS_PROPERTY_OPACITY),
                               100);
  opacity = CLAMP (opacity, 0.0, 1.0);
  alpha = round (priv->user_alpha * opacity);

  if (alpha == priv->alpha)
    return;

  priv->alpha = alpha;

  if (gtk_widget_get_realized (widget))
    {
      if (gtk_widget_is_toplevel (widget) &&
          gtk_widget_get_visual (widget) != gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget)))
	gdk_window_set_opacity (priv->window, priv->alpha / 255.0);

      gtk_widget_queue_draw (widget);
    }
}

/**
 * gtk_widget_set_opacity:
 * @widget: a #GtkWidget
 * @opacity: desired opacity, between 0 and 1
 *
 * Request the @widget to be rendered partially transparent,
 * with opacity 0 being fully transparent and 1 fully opaque. (Opacity values
 * are clamped to the [0,1] range.).
 * This works on both toplevel widget, and child widgets, although there
 * are some limitations:
 *
 * For toplevel widgets this depends on the capabilities of the windowing
 * system. On X11 this has any effect only on X screens with a compositing manager
 * running. See gtk_widget_is_composited(). On Windows it should work
 * always, although setting a window’s opacity after the window has been
 * shown causes it to flicker once on Windows.
 *
 * For child widgets it doesn’t work if any affected widget has a native window, or
 * disables double buffering.
 *
 * Since: 3.8
 **/
void
gtk_widget_set_opacity (GtkWidget *widget,
                        gdouble    opacity)
{
  GtkWidgetPrivate *priv;
  guint8 alpha;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  opacity = CLAMP (opacity, 0.0, 1.0);

  alpha = round (opacity * 255);

  if (alpha == priv->user_alpha)
    return;

  priv->user_alpha = alpha;

  gtk_widget_update_alpha (widget);

  g_object_notify (G_OBJECT (widget), "opacity");
}

/**
 * gtk_widget_get_opacity:
 * @widget: a #GtkWidget
 *
 * Fetches the requested opacity for this widget.
 * See gtk_widget_set_opacity().
 *
 * Returns: the requested opacity for this widget.
 *
 * Since: 3.8
 **/
gdouble
gtk_widget_get_opacity (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0.0);

  return widget->priv->user_alpha / 255.0;
}

static void
_gtk_widget_set_has_focus (GtkWidget *widget,
                           gboolean   has_focus)
{
  widget->priv->has_focus = has_focus;

  if (has_focus)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_FOCUSED, FALSE);
  else
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_FOCUSED);
}

/**
 * gtk_widget_send_focus_change:
 * @widget: a #GtkWidget
 * @event: a #GdkEvent of type GDK_FOCUS_CHANGE
 *
 * Sends the focus change @event to @widget
 *
 * This function is not meant to be used by applications. The only time it
 * should be used is when it is necessary for a #GtkWidget to assign focus
 * to a widget that is semantically owned by the first widget even though
 * it’s not a direct child - for instance, a search entry in a floating
 * window similar to the quick search in #GtkTreeView.
 *
 * An example of its usage is:
 *
 * |[<!-- language="C" -->
 *   GdkEvent *fevent = gdk_event_new (GDK_FOCUS_CHANGE);
 *
 *   fevent->focus_change.type = GDK_FOCUS_CHANGE;
 *   fevent->focus_change.in = TRUE;
 *   fevent->focus_change.window = gtk_widget_get_window (widget);
 *   if (fevent->focus_change.window != NULL)
 *     g_object_ref (fevent->focus_change.window);
 *
 *   gtk_widget_send_focus_change (widget, fevent);
 *
 *   gdk_event_free (event);
 * ]|
 *
 * Returns: the return value from the event signal emission: %TRUE
 *   if the event was handled, and %FALSE otherwise
 *
 * Since: 2.20
 */
gboolean
gtk_widget_send_focus_change (GtkWidget *widget,
                              GdkEvent  *event)
{
  gboolean res;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (event != NULL && event->type == GDK_FOCUS_CHANGE, FALSE);

  g_object_ref (widget);

  _gtk_widget_set_has_focus (widget, event->focus_change.in);

  res = gtk_widget_event (widget, event);

  g_object_notify (G_OBJECT (widget), "has-focus");

  g_object_unref (widget);

  return res;
}

/**
 * gtk_widget_in_destruction:
 * @widget: a #GtkWidget
 *
 * Returns whether the widget is currently being destroyed.
 * This information can sometimes be used to avoid doing
 * unnecessary work.
 *
 * Returns: %TRUE if @widget is being destroyed
 */
gboolean
gtk_widget_in_destruction (GtkWidget *widget)
{
  return widget->priv->in_destruction;
}

gboolean
_gtk_widget_get_in_reparent (GtkWidget *widget)
{
  return widget->priv->in_reparent;
}

void
_gtk_widget_set_in_reparent (GtkWidget *widget,
                             gboolean   in_reparent)
{
  widget->priv->in_reparent = in_reparent;
}

gboolean
_gtk_widget_get_anchored (GtkWidget *widget)
{
  return widget->priv->anchored;
}

void
_gtk_widget_set_anchored (GtkWidget *widget,
                          gboolean   anchored)
{
  widget->priv->anchored = anchored;
}

gboolean
_gtk_widget_get_shadowed (GtkWidget *widget)
{
  return widget->priv->shadowed;
}

void
_gtk_widget_set_shadowed (GtkWidget *widget,
                          gboolean   shadowed)
{
  widget->priv->shadowed = shadowed;
}

gboolean
_gtk_widget_get_alloc_needed (GtkWidget *widget)
{
  return widget->priv->alloc_needed;
}

void
_gtk_widget_set_alloc_needed (GtkWidget *widget,
                              gboolean   alloc_needed)
{
  widget->priv->alloc_needed = alloc_needed;
}

void
_gtk_widget_add_sizegroup (GtkWidget    *widget,
			   gpointer      group)
{
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_prepend (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  widget->priv->have_size_groups = TRUE;
}

void
_gtk_widget_remove_sizegroup (GtkWidget    *widget,
			      gpointer      group)
{
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_remove (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  widget->priv->have_size_groups = groups != NULL;
}

GSList *
_gtk_widget_get_sizegroups (GtkWidget    *widget)
{
  if (widget->priv->have_size_groups)
    return g_object_get_qdata (G_OBJECT (widget), quark_size_groups);

  return NULL;
}

void
_gtk_widget_add_attached_window (GtkWidget    *widget,
                                 GtkWindow    *window)
{
  widget->priv->attached_windows = g_list_prepend (widget->priv->attached_windows, window);
}

void
_gtk_widget_remove_attached_window (GtkWidget    *widget,
                                    GtkWindow    *window)
{
  widget->priv->attached_windows = g_list_remove (widget->priv->attached_windows, window);
}

/**
 * gtk_widget_path_append_for_widget:
 * @path: a widget path
 * @widget: the widget to append to the widget path
 *
 * Appends the data from @widget to the widget hierarchy represented
 * by @path. This function is a shortcut for adding information from
 * @widget to the given @path. This includes setting the name or
 * adding the style classes from @widget.
 *
 * Returns: the position where the data was inserted
 *
 * Since: 3.2
 */
gint
gtk_widget_path_append_for_widget (GtkWidgetPath *path,
                                   GtkWidget     *widget)
{
  gint pos;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  pos = gtk_widget_path_append_type (path, G_OBJECT_TYPE (widget));

  if (widget->priv->name)
    gtk_widget_path_iter_set_name (path, pos, widget->priv->name);

  gtk_widget_path_iter_set_state (path, pos, widget->priv->state_flags);

  if (widget->priv->context)
    {
      GList *classes, *l;

      /* Also add any persistent classes in
       * the style context the widget path
       */
      classes = gtk_style_context_list_classes (widget->priv->context);

      for (l = classes; l; l = l->next)
        gtk_widget_path_iter_add_class (path, pos, l->data);

      g_list_free (classes);
    }

  return pos;
}

GtkWidgetPath *
_gtk_widget_create_path (GtkWidget *widget)
{
  GtkWidget *parent;

  parent = widget->priv->parent;

  if (parent)
    return gtk_container_get_path_for_child (GTK_CONTAINER (parent), widget);
  else
    {
      /* Widget is either toplevel or unparented, treat both
       * as toplevels style wise, since there are situations
       * where style properties might be retrieved on that
       * situation.
       */
      GtkWidget *attach_widget = NULL;
      GtkWidgetPath *result;

      if (GTK_IS_WINDOW (widget))
        attach_widget = gtk_window_get_attached_to (GTK_WINDOW (widget));

      if (attach_widget != NULL)
        result = gtk_widget_path_copy (gtk_widget_get_path (attach_widget));
      else
        result = gtk_widget_path_new ();

      gtk_widget_path_append_for_widget (result, widget);

      return result;
    }
}

/**
 * gtk_widget_get_path:
 * @widget: a #GtkWidget
 *
 * Returns the #GtkWidgetPath representing @widget, if the widget
 * is not connected to a toplevel widget, a partial path will be
 * created.
 *
 * Returns: (transfer none): The #GtkWidgetPath representing @widget
 **/
GtkWidgetPath *
gtk_widget_get_path (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (!widget->priv->path)
    widget->priv->path = _gtk_widget_create_path (widget);

  return widget->priv->path;
}

void
_gtk_widget_style_context_invalidated (GtkWidget *widget)
{
  if (widget->priv->path)
    {
      gtk_widget_path_free (widget->priv->path);
      widget->priv->path = NULL;
    }

  if (gtk_widget_get_realized (widget))
    g_signal_emit (widget, widget_signals[STYLE_UPDATED], 0);
  else
    {
      /* Compress all style updates so it
       * is only emitted once pre-realize.
       */
      widget->priv->style_update_pending = TRUE;
    }
}

/**
 * gtk_widget_get_style_context:
 * @widget: a #GtkWidget
 *
 * Returns the style context associated to @widget.
 *
 * Returns: (transfer none): a #GtkStyleContext. This memory is owned by @widget and
 *          must not be freed.
 **/
GtkStyleContext *
gtk_widget_get_style_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;
  
  if (G_UNLIKELY (priv->context == NULL))
    {
      GdkScreen *screen;
      GdkFrameClock *frame_clock;

      priv->context = gtk_style_context_new ();

      gtk_style_context_set_id (priv->context, priv->name);
      gtk_style_context_set_state (priv->context, priv->state_flags);
      gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));

      screen = gtk_widget_get_screen (widget);
      if (screen)
        gtk_style_context_set_screen (priv->context, screen);

      frame_clock = gtk_widget_get_frame_clock (widget);
      if (frame_clock)
        gtk_style_context_set_frame_clock (priv->context, frame_clock);

      if (priv->parent)
        gtk_style_context_set_parent (priv->context,
                                      gtk_widget_get_style_context (priv->parent));

      _gtk_style_context_set_widget (priv->context, widget);
    }

  return widget->priv->context;
}

void
_gtk_widget_invalidate_style_context (GtkWidget    *widget,
                                      GtkCssChange  change)
{
  GtkWidgetPrivate *priv;

  priv = widget->priv;

  if (priv->context == NULL)
    return;

  _gtk_style_context_queue_invalidate (priv->context, change);
}

/**
 * gtk_widget_get_modifier_mask:
 * @widget: a #GtkWidget
 * @intent: the use case for the modifier mask
 *
 * Returns the modifier mask the @widget’s windowing system backend
 * uses for a particular purpose.
 *
 * See gdk_keymap_get_modifier_mask().
 *
 * Returns: the modifier mask used for @intent.
 *
 * Since: 3.4
 **/
GdkModifierType
gtk_widget_get_modifier_mask (GtkWidget         *widget,
                              GdkModifierIntent  intent)
{
  GdkDisplay *display;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  display = gtk_widget_get_display (widget);

  return gdk_keymap_get_modifier_mask (gdk_keymap_get_for_display (display),
                                       intent);
}

GtkStyle *
_gtk_widget_get_style (GtkWidget *widget)
{
  return widget->priv->style;
}

void
_gtk_widget_set_style (GtkWidget *widget,
                       GtkStyle  *style)
{
  widget->priv->style = style;
}

GtkActionMuxer *
_gtk_widget_get_parent_muxer (GtkWidget *widget,
                              gboolean   create)
{
  GtkWidget *parent;

  if (GTK_IS_WINDOW (widget))
    return gtk_application_get_parent_muxer_for_window (GTK_WINDOW (widget));

  if (GTK_IS_MENU (widget))
    parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
  else if (GTK_IS_POPOVER (widget))
    parent = gtk_popover_get_relative_to (GTK_POPOVER (widget));
  else
    parent = gtk_widget_get_parent (widget);

  if (parent)
    return _gtk_widget_get_action_muxer (parent, create);

  return NULL;
}

void
_gtk_widget_update_parent_muxer (GtkWidget *widget)
{
  if (widget->priv->muxer == NULL)
    return;

  gtk_action_muxer_set_parent (widget->priv->muxer,
                               _gtk_widget_get_parent_muxer (widget, TRUE));
}

GtkActionMuxer *
_gtk_widget_get_action_muxer (GtkWidget *widget,
                              gboolean   create)
{
  if (widget->priv->muxer)
    return widget->priv->muxer;

  if (create)
    {
      widget->priv->muxer = gtk_action_muxer_new ();
      _gtk_widget_update_parent_muxer (widget);

      return widget->priv->muxer;
    }
  else
    return _gtk_widget_get_parent_muxer (widget, FALSE);
}

/**
 * gtk_widget_insert_action_group:
 * @widget: a #GtkWidget
 * @name: the prefix for actions in @group
 * @group: (allow-none): a #GActionGroup, or %NULL
 *
 * Inserts @group into @widget. Children of @widget that implement
 * #GtkActionable can then be associated with actions in @group by
 * setting their “action-name” to
 * @prefix.`action-name`.
 *
 * If @group is %NULL, a previously inserted group for @name is removed
 * from @widget.
 *
 * Since: 3.6
 */
void
gtk_widget_insert_action_group (GtkWidget    *widget,
                                const gchar  *name,
                                GActionGroup *group)
{
  GtkActionMuxer *muxer;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (name != NULL);

  muxer = _gtk_widget_get_action_muxer (widget, TRUE);

  if (group)
    gtk_action_muxer_insert (muxer, name, group);
  else
    gtk_action_muxer_remove (muxer, name);
}

/****************************************************************
 *                 GtkBuilder automated templates               *
 ****************************************************************/
static AutomaticChildClass *
template_child_class_new (const gchar *name,
                          gboolean     internal_child,
                          gssize       offset)
{
  AutomaticChildClass *child_class = g_slice_new0 (AutomaticChildClass);

  child_class->name = g_strdup (name);
  child_class->internal_child = internal_child;
  child_class->offset = offset;

  return child_class;
}

static void
template_child_class_free (AutomaticChildClass *child_class)
{
  if (child_class)
    {
      g_free (child_class->name);
      g_slice_free (AutomaticChildClass, child_class);
    }
}

static CallbackSymbol *
callback_symbol_new (const gchar *name,
		     GCallback    callback)
{
  CallbackSymbol *cb = g_slice_new0 (CallbackSymbol);

  cb->callback_name = g_strdup (name);
  cb->callback_symbol = callback;

  return cb;
}

static void
callback_symbol_free (CallbackSymbol *callback)
{
  if (callback)
    {
      g_free (callback->callback_name);
      g_slice_free (CallbackSymbol, callback);
    }
}

static void
template_data_free (GtkWidgetTemplate *template_data)
{
  if (template_data)
    {
      g_bytes_unref (template_data->data);
      g_slist_free_full (template_data->children, (GDestroyNotify)template_child_class_free);
      g_slist_free_full (template_data->callbacks, (GDestroyNotify)callback_symbol_free);

      if (template_data->connect_data &&
	  template_data->destroy_notify)
	template_data->destroy_notify (template_data->connect_data);

      g_slice_free (GtkWidgetTemplate, template_data);
    }
}

static GHashTable *
get_auto_child_hash (GtkWidget *widget,
		     GType      type,
		     gboolean   create)
{
  GHashTable *auto_child_hash;

  if (widget->priv->auto_children == NULL)
    {
      if (!create)
	return NULL;

      widget->priv->auto_children =
	g_hash_table_new_full (g_direct_hash,
			       g_direct_equal,
			       NULL,
			       (GDestroyNotify)g_hash_table_destroy);
    }

  auto_child_hash =
    g_hash_table_lookup (widget->priv->auto_children, GSIZE_TO_POINTER (type));

  if (!auto_child_hash && create)
    {
      auto_child_hash = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       NULL,
					       (GDestroyNotify)g_object_unref);

      g_hash_table_insert (widget->priv->auto_children,
			   GSIZE_TO_POINTER (type),
			   auto_child_hash);
    }

  return auto_child_hash;
}

static gboolean
setup_template_child (GtkWidgetTemplate   *template_data,
                      GType                class_type,
                      AutomaticChildClass *child_class,
                      GtkWidget           *widget,
                      GtkBuilder          *builder)
{
  GHashTable *auto_child_hash;
  GObject    *object;

  object = gtk_builder_get_object (builder, child_class->name);
  if (!object)
    {
      g_critical ("Unable to retrieve object '%s' from class template for type '%s' while building a '%s'",
		  child_class->name, g_type_name (class_type), G_OBJECT_TYPE_NAME (widget));
      return FALSE;
    }

  /* Insert into the hash so that it can be fetched with
   * gtk_widget_get_template_child() and also in automated
   * implementations of GtkBuildable.get_internal_child()
   */
  auto_child_hash = get_auto_child_hash (widget, class_type, TRUE);
  g_hash_table_insert (auto_child_hash, child_class->name, g_object_ref (object));

  if (child_class->offset != 0)
    {
      gpointer field_p;

      /* Assign 'object' to the specified offset in the instance (or private) data */
      field_p = G_STRUCT_MEMBER_P (widget, child_class->offset);
      (* (gpointer *) field_p) = object;
    }

  return TRUE;
}

/**
 * gtk_widget_init_template:
 * @widget: a #GtkWidget
 *
 * Creates and initializes child widgets defined in templates. This
 * function must be called in the instance initializer for any
 * class which assigned itself a template using gtk_widget_class_set_template()
 *
 * It is important to call this function in the instance initializer
 * of a #GtkWidget subclass and not in #GObject.constructed() or
 * #GObject.constructor() for two reasons.
 *
 * One reason is that generally derived widgets will assume that parent
 * class composite widgets have been created in their instance
 * initializers.
 *
 * Another reason is that when calling g_object_new() on a widget with
 * composite templates, it’s important to build the composite widgets
 * before the construct properties are set. Properties passed to g_object_new()
 * should take precedence over properties set in the private template XML.
 *
 * Since: 3.10
 */
void
gtk_widget_init_template (GtkWidget *widget)
{
  GtkWidgetTemplate *template;
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *object;
  GSList *l;
  GType class_type;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  object = G_OBJECT (widget);
  class_type = G_OBJECT_TYPE (widget);

  template = GTK_WIDGET_GET_CLASS (widget)->priv->template;
  g_return_if_fail (template != NULL);

  builder = gtk_builder_new ();

  /* Add any callback symbols declared for this GType to the GtkBuilder namespace */
  for (l = template->callbacks; l; l = l->next)
    {
      CallbackSymbol *callback = l->data;

      gtk_builder_add_callback_symbol (builder, callback->callback_name, callback->callback_symbol);
    }

  /* This will build the template XML as children to the widget instance, also it
   * will validate that the template is created for the correct GType and assert that
   * there is no infinite recursion.
   */
  if (!_gtk_builder_extend_with_template (builder, widget, class_type,
					  (const gchar *)g_bytes_get_data (template->data, NULL),
					  g_bytes_get_size (template->data),
					  &error))
    {
      g_critical ("Error building template class '%s' for an instance of type '%s': %s",
		  g_type_name (class_type), G_OBJECT_TYPE_NAME (object), error->message);
      g_error_free (error);

      /* This should never happen, if the template XML cannot be built
       * then it is a critical programming error.
       */
      g_object_unref (builder);
      return;
    }

  /* Build the automatic child data
   */
  for (l = template->children; l; l = l->next)
    {
      AutomaticChildClass *child_class = l->data;

      /* This will setup the pointer of an automated child, and cause
       * it to be available in any GtkBuildable.get_internal_child()
       * invocations which may follow by reference in child classes.
       */
      if (!setup_template_child (template,
				  class_type,
				  child_class,
				  widget,
				  builder))
	{
	  g_object_unref (builder);
	  return;
	}
    }

  /* Connect signals. All signal data from a template receive the 
   * template instance as user data automatically.
   *
   * A GtkBuilderConnectFunc can be provided to gtk_widget_class_set_signal_connect_func()
   * in order for templates to be usable by bindings.
   */
  if (template->connect_func)
    gtk_builder_connect_signals_full (builder, template->connect_func, template->connect_data);
  else
    gtk_builder_connect_signals (builder, object);

  g_object_unref (builder);
}

/**
 * gtk_widget_class_set_template:
 * @widget_class: A #GtkWidgetClass
 * @template_bytes: A #GBytes holding the #GtkBuilder XML 
 *
 * This should be called at class initialization time to specify
 * the GtkBuilder XML to be used to extend a widget.
 *
 * For convenience, gtk_widget_class_set_template_from_resource() is also provided.
 *
 * Note that any class that installs templates must call gtk_widget_init_template()
 * in the widget’s instance initializer.
 *
 * Since: 3.10
 */
void
gtk_widget_class_set_template (GtkWidgetClass    *widget_class,
			       GBytes            *template_bytes)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template == NULL);
  g_return_if_fail (template_bytes != NULL);

  widget_class->priv->template = g_slice_new0 (GtkWidgetTemplate);
  widget_class->priv->template->data = g_bytes_ref (template_bytes);
}

/**
 * gtk_widget_class_set_template_from_resource:
 * @widget_class: A #GtkWidgetClass
 * @resource_name: The name of the resource to load the template from
 *
 * A convenience function to call gtk_widget_class_set_template().
 *
 * Note that any class that installs templates must call gtk_widget_init_template()
 * in the widget’s instance initializer.
 *
 * Since: 3.10
 */
void
gtk_widget_class_set_template_from_resource (GtkWidgetClass    *widget_class,
					     const gchar       *resource_name)
{
  GError *error = NULL;
  GBytes *bytes = NULL;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template == NULL);
  g_return_if_fail (resource_name && resource_name[0]);

  /* This is a hack, because class initializers now access resources
   * and GIR/gtk-doc initializes classes without initializing GTK+,
   * we ensure that our base resources are registered here and
   * avoid warnings which building GIRs/documentation.
   */
  _gtk_ensure_resources ();

  bytes = g_resources_lookup_data (resource_name, 0, &error);
  if (!bytes)
    {
      g_critical ("Unable to load resource for composite template for type '%s': %s",
		  G_OBJECT_CLASS_NAME (widget_class), error->message);
      g_error_free (error);
      return;
    }

  gtk_widget_class_set_template (widget_class, bytes);
  g_bytes_unref (bytes);
}

/**
 * gtk_widget_class_bind_template_callback_full:
 * @widget_class: A #GtkWidgetClass
 * @callback_name: The name of the callback as expected in the template XML
 * @callback_symbol: (scope async): The callback symbol
 *
 * Declares a @callback_symbol to handle @callback_name from the template XML
 * defined for @widget_type. See gtk_builder_add_callback_symbol().
 *
 * Note that this must be called from a composite widget classes class
 * initializer after calling gtk_widget_class_set_template().
 *
 * Since: 3.10
 */
void
gtk_widget_class_bind_template_callback_full (GtkWidgetClass *widget_class,
                                              const gchar    *callback_name,
                                              GCallback       callback_symbol)
{
  CallbackSymbol *cb;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template != NULL);
  g_return_if_fail (callback_name && callback_name[0]);
  g_return_if_fail (callback_symbol != NULL);

  cb = callback_symbol_new (callback_name, callback_symbol);
  widget_class->priv->template->callbacks = g_slist_prepend (widget_class->priv->template->callbacks, cb);
}

/**
 * gtk_widget_class_set_connect_func:
 * @widget_class: A #GtkWidgetClass
 * @connect_func: The #GtkBuilderConnectFunc to use when connecting signals in the class template
 * @connect_data: The data to pass to @connect_func
 * @connect_data_destroy: The #GDestroyNotify to free @connect_data, this will only be used at
 *                        class finalization time, when no classes of type @widget_type are in use anymore.
 *
 * For use in lanuage bindings, this will override the default #GtkBuilderConnectFunc to be
 * used when parsing GtkBuilder xml from this class’s template data.
 *
 * Note that this must be called from a composite widget classes class
 * initializer after calling gtk_widget_class_set_template().
 *
 * Since: 3.10
 */
void
gtk_widget_class_set_connect_func (GtkWidgetClass        *widget_class,
				   GtkBuilderConnectFunc  connect_func,
				   gpointer               connect_data,
				   GDestroyNotify         connect_data_destroy)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template != NULL);

  /* Defensive, destroy any previously set data */
  if (widget_class->priv->template->connect_data &&
      widget_class->priv->template->destroy_notify)
    widget_class->priv->template->destroy_notify (widget_class->priv->template->connect_data);

  widget_class->priv->template->connect_func   = connect_func;
  widget_class->priv->template->connect_data   = connect_data;
  widget_class->priv->template->destroy_notify = connect_data_destroy;
}

/**
 * gtk_widget_class_bind_template_child_full:
 * @widget_class: A #GtkWidgetClass
 * @name: The “id” of the child defined in the template XML
 * @internal_child: Whether the child should be accessible as an “internal-child”
 *                  when this class is used in GtkBuilder XML
 * @struct_offset: The structure offset into the composite widget’s instance public or private structure
 *                 where the automated child pointer should be set, or 0 to not assign the pointer.
 *
 * Automatically assign an object declared in the class template XML to be set to a location
 * on a freshly built instance’s private data, or alternatively accessible via gtk_widget_get_template_child().
 *
 * The struct can point either into the public instance, then you should use G_STRUCT_OFFSET(WidgetType, member)
 * for @struct_offset,  or in the private struct, then you should use G_PRIVATE_OFFSET(WidgetType, member).
 *
 * An explicit strong reference will be held automatically for the duration of your
 * instance’s life cycle, it will be released automatically when #GObjectClass.dispose() runs
 * on your instance and if a @struct_offset that is != 0 is specified, then the automatic location
 * in your instance public or private data will be set to %NULL. You can however access an automated child
 * pointer the first time your classes #GObjectClass.dispose() runs, or alternatively in
 * #GtkWidgetClass.destroy().
 *
 * If @internal_child is specified, #GtkBuildableIface.get_internal_child() will be automatically
 * implemented by the #GtkWidget class so there is no need to implement it manually.
 *
 * The wrapper macros gtk_widget_class_bind_template_child(), gtk_widget_class_bind_template_child_internal(),
 * gtk_widget_class_bind_template_child_private() and gtk_widget_class_bind_template_child_internal_private()
 * might be more convenient to use.
 *
 * Note that this must be called from a composite widget classes class
 * initializer after calling gtk_widget_class_set_template().
 *
 * Since: 3.10
 */
void
gtk_widget_class_bind_template_child_full (GtkWidgetClass *widget_class,
                                           const gchar    *name,
                                           gboolean        internal_child,
                                           gssize          struct_offset)
{
  AutomaticChildClass *child_class;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (widget_class->priv->template != NULL);
  g_return_if_fail (name && name[0]);

  child_class = template_child_class_new (name,
                                          internal_child,
                                          struct_offset);
  widget_class->priv->template->children =
    g_slist_prepend (widget_class->priv->template->children, child_class);
}

/**
 * gtk_widget_get_template_child:
 * @widget: A #GtkWidget
 * @widget_type: The #GType to get a template child for
 * @name: The “id” of the child defined in the template XML
 *
 * Fetch an object build from the template XML for @widget_type in this @widget instance.
 *
 * This will only report children which were previously declared with
 * gtk_widget_class_bind_template_child_full() or one of its
 * variants.
 *
 * This function is only meant to be called for code which is private to the @widget_type which
 * declared the child and is meant for language bindings which cannot easily make use
 * of the GObject structure offsets.
 *
 * Returns: (transfer none): The object built in the template XML with the id @name
 */
GObject *
gtk_widget_get_template_child (GtkWidget   *widget,
                               GType        widget_type,
                               const gchar *name)
{
  GHashTable *auto_child_hash;
  GObject *ret = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (g_type_name (widget_type) != NULL, NULL);
  g_return_val_if_fail (name && name[0], NULL);

  auto_child_hash = get_auto_child_hash (widget, widget_type, FALSE);

  if (auto_child_hash)
    ret = g_hash_table_lookup (auto_child_hash, name);

  return ret;
}

/**
 * gtk_widget_list_action_prefixes:
 * @widget: A #GtkWidget
 *
 * Retrieves a %NULL-terminated array of strings containing the prefixes of
 * #GActionGroup's available to @widget.
 *
 * Returns: (transfer container): a %NULL-terminated array of strings.
 *
 * Since: 3.16
 */
const gchar **
gtk_widget_list_action_prefixes (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (widget->priv->muxer)
      return gtk_action_muxer_list_prefixes (widget->priv->muxer);
  return g_new0 (const gchar *, 0 + 1);
}

/**
 * gtk_widget_get_action_group:
 * @widget: A #GtkWidget
 * @prefix: The “prefix” of the action group.
 *
 * Retrieves the #GActionGroup that was registered using @prefix. The resulting
 * #GActionGroup may have been registered to @widget or any #GtkWidget in its
 * ancestry.
 *
 * If no action group was found matching @prefix, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): A #GActionGroup or %NULL.
 *
 * Since: 3.16
 */
GActionGroup *
gtk_widget_get_action_group (GtkWidget   *widget,
                             const gchar *prefix)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (prefix, NULL);

  if (widget->priv->muxer)
    return gtk_action_muxer_lookup (widget->priv->muxer, prefix);
  return NULL;
}

static void
event_controller_grab_notify (GtkWidget           *widget,
                              gboolean             was_grabbed,
                              EventControllerData *data)
{
  GdkDevice *device = NULL;

  if (GTK_IS_GESTURE (data->controller))
    device = gtk_gesture_get_device (GTK_GESTURE (data->controller));

  if (!device || !gtk_widget_device_is_shadowed (widget, device))
    return;

  gtk_event_controller_reset (data->controller);
}

static void
_gtk_widget_update_evmask (GtkWidget *widget)
{
  if (gtk_widget_get_realized (widget))
    {
      gint events = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget),
                                                         quark_event_mask));
      gtk_widget_add_events_internal (widget, NULL, events);
    }
}

static void
event_controller_notify_event_mask (GtkEventController *controller,
                                    GParamSpec         *pspec,
                                    GtkWidget          *widget)
{
  _gtk_widget_update_evmask (widget);
}

static void
event_controller_sequence_state_changed (GtkGesture            *gesture,
                                         GdkEventSequence      *sequence,
                                         GtkEventSequenceState  state,
                                         GtkWidget             *widget)
{
  gboolean handled = FALSE;
  GtkWidget *event_widget;
  gboolean cancel = TRUE;
  const GdkEvent *event;

  handled = _gtk_widget_set_sequence_state_internal (widget, sequence,
                                                     state, gesture);

  if (!handled || state != GTK_EVENT_SEQUENCE_CLAIMED)
    return;

  event = _gtk_widget_get_last_event (widget, sequence);

  if (!event)
    return;

  event_widget = gtk_get_event_widget ((GdkEvent *) event);

  while (event_widget)
    {
      if (event_widget == widget)
        cancel = FALSE;
      else if (cancel)
        _gtk_widget_cancel_sequence (event_widget, sequence);
      else
        _gtk_widget_set_sequence_state_internal (event_widget, sequence,
                                                 GTK_EVENT_SEQUENCE_DENIED,
                                                 NULL);

      event_widget = gtk_widget_get_parent (event_widget);
    }
}

static EventControllerData *
_gtk_widget_has_controller (GtkWidget          *widget,
                            GtkEventController *controller)
{
  EventControllerData *data;
  GtkWidgetPrivate *priv;
  GList *l;

  priv = widget->priv;

  for (l = priv->event_controllers; l; l = l->next)
    {
      data = l->data;

      if (data->controller == controller)
        return data;
    }

  return NULL;
}

void
_gtk_widget_add_controller (GtkWidget          *widget,
                            GtkEventController *controller)
{
  EventControllerData *data;
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (widget == gtk_event_controller_get_widget (controller));

  priv = widget->priv;
  data = _gtk_widget_has_controller (widget, controller);

  if (data)
    return;

  data = g_new0 (EventControllerData, 1);
  data->controller = controller;
  data->evmask_notify_id =
    g_signal_connect (controller, "notify::event-mask",
                      G_CALLBACK (event_controller_notify_event_mask), widget);
  data->grab_notify_id =
    g_signal_connect (widget, "grab-notify",
                      G_CALLBACK (event_controller_grab_notify), data);

  g_object_add_weak_pointer (G_OBJECT (data->controller), (gpointer *) &data->controller);

  if (GTK_IS_GESTURE (controller))
    {
      data->sequence_state_changed_id =
        g_signal_connect (controller, "sequence-state-changed",
                          G_CALLBACK (event_controller_sequence_state_changed),
                          widget);
    }

  priv->event_controllers = g_list_prepend (priv->event_controllers, data);
  _gtk_widget_update_evmask (widget);
}

void
_gtk_widget_remove_controller (GtkWidget          *widget,
                               GtkEventController *controller)
{
  EventControllerData *data;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  data = _gtk_widget_has_controller (widget, controller);

  if (!data)
    return;

  g_object_remove_weak_pointer (G_OBJECT (data->controller), (gpointer *) &data->controller);

  if (g_signal_handler_is_connected (widget, data->grab_notify_id))
    g_signal_handler_disconnect (widget, data->grab_notify_id);

  g_signal_handler_disconnect (data->controller, data->evmask_notify_id);
  g_signal_handler_disconnect (data->controller, data->sequence_state_changed_id);
  data->controller = NULL;
}

GList *
_gtk_widget_list_controllers (GtkWidget           *widget,
                              GtkPropagationPhase  phase)
{
  EventControllerData *data;
  GtkWidgetPrivate *priv;
  GList *l, *retval = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  for (l = priv->event_controllers; l; l = l->next)
    {
      data = l->data;

      if (data->controller != NULL &&
          phase == gtk_event_controller_get_propagation_phase (data->controller))
        retval = g_list_prepend (retval, data->controller);
    }

  return retval;
}

gboolean
_gtk_widget_consumes_motion (GtkWidget        *widget,
                             GdkEventSequence *sequence)
{
  EventControllerData *data;
  GtkWidgetPrivate *priv;
  GList *l;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  priv = widget->priv;

  for (l = priv->event_controllers; l; l = l->next)
    {
      data = l->data;

      if (data->controller == NULL)
        continue;

      if ((!GTK_IS_GESTURE_SINGLE (data->controller) ||
           GTK_IS_GESTURE_DRAG (data->controller) ||
           GTK_IS_GESTURE_SWIPE (data->controller)) &&
          gtk_gesture_get_sequence_state (GTK_GESTURE (data->controller),
                                          sequence) != GTK_EVENT_SEQUENCE_DENIED)
        return TRUE;
    }

  return FALSE;
}
