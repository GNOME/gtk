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

#include "gtkwidgetprivate.h"

#include "gtkaccelmapprivate.h"
#include "gtkaccelgroupprivate.h"
#include "gtkaccessible.h"
#include "gtkapplicationprivate.h"
#include "gtkbindings.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcssboxesprivate.h"
#include "gtkcssfiltervalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkcssfontvariationsvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtkdebug.h"
#include "gtkgesturedrag.h"
#include "gtkgestureprivate.h"
#include "gtkgesturesingle.h"
#include "gtkgestureswipe.h"
#include "gtkintl.h"
#include "gtklayoutmanagerprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrootprivate.h"
#include "gtknativeprivate.h"
#include "gtkscrollable.h"
#include "gtkselection.h"
#include "gtksettingsprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcutcontroller.h"
#include "gtkshortcuttrigger.h"
#include "gtksizegroup-private.h"
#include "gtksnapshotprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtktooltipprivate.h"
#include "gsktransformprivate.h"
#include "gtktypebuiltins.h"
#include "gtkversion.h"
#include "gtkwidgetpaintableprivate.h"
#include "gtkwidgetpathprivate.h"
#include "gtkwindowgroup.h"
#include "gtkwindowprivate.h"
#include "gtknativeprivate.h"

#include "a11y/gtkwidgetaccessible.h"
#include "inspector/window.h"

#include "gdk/gdkeventsprivate.h"
#include "gsk/gskdebugprivate.h"
#include "gsk/gskrendererprivate.h"

#include <cairo-gobject.h>
#include <gobject/gobjectnotifyqueue.c>
#include <gobject/gvaluecollector.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

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
 * of two virtual methods:
 *
 * - #GtkWidgetClass.get_request_mode()
 * - #GtkWidgetClass.measure()
 *
 * There are some important things to keep in mind when implementing
 * height-for-width and when using it in widget implementations.
 *
 * If you implement a direct #GtkWidget subclass that supports
 * height-for-width or width-for-height geometry management for
 * itself or its child widgets, the #GtkWidgetClass.get_request_mode()
 * virtual function must be implemented as well and return the widget's
 * preferred request mode. The default implementation of this virtual function
 * returns %GTK_SIZE_REQUEST_CONSTANT_SIZE, which means that the widget will only ever
 * get -1 passed as the for_size value to its #GtkWidgetClass.measure() implementation.
 *
 * The geometry management system will query a widget hierarchy in
 * only one orientation at a time. When widgets are initially queried
 * for their minimum sizes it is generally done in two initial passes
 * in the #GtkSizeRequestMode chosen by the toplevel.
 *
 * For example, when queried in the normal
 * %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH mode:
 * First, the default minimum and natural width for each widget
 * in the interface will be computed using gtk_widget_measure() with an orientation
 * of %GTK_ORIENTATION_HORIZONTAL and a for_size of -1.
 * Because the preferred widths for each widget depend on the preferred
 * widths of their children, this information propagates up the hierarchy,
 * and finally a minimum and natural width is determined for the entire
 * toplevel. Next, the toplevel will use the minimum width to query for the
 * minimum height contextual to that width using gtk_widget_measure() with an
 * orientation of %GTK_ORIENTATION_VERTICAL and a for_size of the just computed
 * width. This will also be a highly recursive operation.
 * The minimum height for the minimum width is normally
 * used to set the minimum size constraint on the toplevel
 * (unless gtk_window_set_geometry_hints() is explicitly used instead).
 *
 * After the toplevel window has initially requested its size in both
 * dimensions it can go on to allocate itself a reasonable size (or a size
 * previously specified with gtk_window_set_default_size()). During the
 * recursive allocation process it’s important to note that request cycles
 * will be recursively executed while widgets allocate their children.
 * Each widget, once allocated a size, will go on to first share the
 * space in one orientation among its children and then request each child's
 * height for its target allocated width or its width for allocated height,
 * depending. In this way a #GtkWidget will typically be requested its size
 * a number of times before actually being allocated a size. The size a
 * widget is finally allocated can of course differ from the size it has
 * requested. For this reason, #GtkWidget caches a  small number of results
 * to avoid re-querying for the same sizes in one allocation cycle.
 *
 * If a widget does move content around to intelligently use up the
 * allocated size then it must support the request in both
 * #GtkSizeRequestModes even if the widget in question only
 * trades sizes in a single orientation.
 *
 * For instance, a #GtkLabel that does height-for-width word wrapping
 * will not expect to have #GtkWidgetClass.measure() with an orientation of
 * %GTK_ORIENTATION_VERTICAL called because that call is specific to a
 * width-for-height request. In this
 * case the label must return the height required for its own minimum
 * possible width. By following this rule any widget that handles
 * height-for-width or width-for-height requests will always be allocated
 * at least enough space to fit its own content.
 *
 * Here are some examples of how a %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH widget
 * generally deals with width-for-height requests:
 *
 * |[<!-- language="C" -->
 * static void
 * foo_widget_measure (GtkWidget      *widget,
 *                     GtkOrientation  orientation,
 *                     int             for_size,
 *                     int            *minimum_size,
 *                     int            *natural_size,
 *                     int            *minimum_baseline,
 *                     int            *natural_baseline)
 * {
 *   if (orientation == GTK_ORIENTATION_HORIZONTAL)
 *     {
 *       // Calculate minimum and natural width
 *     }
 *   else // VERTICAL
 *     {
 *        if (i_am_in_height_for_width_mode)
 *          {
 *            int min_width, dummy;
 *
 *            // First, get the minimum width of our widget
 *            GTK_WIDGET_GET_CLASS (widget)->measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
 *                                                    &min_width, &dummy, &dummy, &dummy);
 *
 *            // Now use the minimum width to retrieve the minimum and natural height to display
 *            // that width.
 *            GTK_WIDGET_GET_CLASS (widget)->measure (widget, GTK_ORIENTATION_VERTICAL, min_width,
 *                                                    minimum_size, natural_size, &dummy, &dummy);
 *          }
 *        else
 *          {
 *            // ... some widgets do both.
 *          }
 *    }
 * }
 * ]|
 *
 * Often a widget needs to get its own request during size request or
 * allocation. For example, when computing height it may need to also
 * compute width. Or when deciding how to use an allocation, the widget
 * may need to know its natural size. In these cases, the widget should
 * be careful to call its virtual methods directly, like in the code
 * example above.
 *
 * It will not work to use the wrapper function gtk_widget_measure()
 * inside your own #GtkWidgetClass.size-allocate() implementation.
 * These return a request adjusted by #GtkSizeGroup, the widget's align and expand flags
 * as well as its CSS style.
 * If a widget used the wrappers inside its virtual method implementations,
 * then the adjustments (such as widget margins) would be applied
 * twice. GTK+ therefore does not allow this and will warn if you try
 * to do it.
 *
 * Of course if you are getting the size request for
 * another widget, such as a child widget, you must use gtk_widget_measure().
 * Otherwise, you would not properly consider widget margins,
 * #GtkSizeGroup, and so forth.
 *
 * GTK+ also supports baseline vertical alignment of widgets. This
 * means that widgets are positioned such that the typographical baseline of
 * widgets in the same row are aligned. This happens if a widget supports baselines,
 * has a vertical alignment of %GTK_ALIGN_BASELINE, and is inside a widget
 * that supports baselines and has a natural “row” that it aligns to the baseline,
 * or a baseline assigned to it by the grandparent.
 *
 * Baseline alignment support for a widget is also done by the #GtkWidgetClass.measure()
 * virtual function. It allows you to report a both a minimum and natural 
 *
 * If a widget ends up baseline aligned it will be allocated all the space in the parent
 * as if it was %GTK_ALIGN_FILL, but the selected baseline can be found via gtk_widget_get_allocated_baseline().
 * If this has a value other than -1 you need to align the widget such that the baseline
 * appears at the position.
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
 * <object class="GtkLabel" id="label1"/>
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
 * If the parent widget uses a #GtkLayoutManager, #GtkWidget supports a
 * custom <layout> element, used to define layout properties:
 *
 * |[
 * <object class="MyGrid" id="grid1">
 *   <child>
 *     <object class="GtkLabel" id="label1">
 *       <property name="label">Description</property>
 *       <layout>
 *         <property name="left-attach">0</property>
 *         <property name="top-attach">0</property>
 *         <property name="row-span">1</property>
 *         <property name="col-span">1</property>
 *       </layout>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="description_entry">
 *       <layout>
 *         <property name="left-attach">1</property>
 *         <property name="top-attach">0</property>
 *         <property name="row-span">1</property>
 *         <property name="col-span">1</property>
 *       </layout>
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
 * GtkWidget exposes some facilities to automate the procedure
 * of creating composite widgets using #GtkBuilder interface description
 * language.
 *
 * To create composite widgets with #GtkBuilder XML, one must associate
 * the interface description with the widget class at class initialization
 * time using gtk_widget_class_set_template().
 *
 * The interface description semantics expected in composite template descriptions
 * is slightly different from regular #GtkBuilder XML.
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
 * <template> tag in the normal way, allowing one to define auxiliary objects
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
 *
 * static void
 * foo_widget_init (FooWidget *widget)
 * {
 *
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

#define GTK_STATE_FLAGS_DO_SET_PROPAGATE   (GTK_STATE_FLAG_INSENSITIVE | \
                                            GTK_STATE_FLAG_BACKDROP)
#define GTK_STATE_FLAGS_DO_UNSET_PROPAGATE (GTK_STATE_FLAG_INSENSITIVE | \
                                            GTK_STATE_FLAG_BACKDROP | \
                                            GTK_STATE_FLAG_PRELIGHT | \
                                            GTK_STATE_FLAG_ACTIVE)

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

struct _GtkWidgetClassPrivate
{
  GtkWidgetTemplate *template;
  GSList *shortcuts;
  GType accessible_type;
  AtkRole accessible_role;
  const char *css_name;
  GType layout_manager_type;
  GPtrArray *actions;
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
  HIERARCHY_CHANGED,
  DIRECTION_CHANGED,
  GRAB_NOTIFY,
  CHILD_NOTIFY,
  MNEMONIC_ACTIVATE,
  MOVE_FOCUS,
  KEYNAV_FAILED,
  DRAG_BEGIN,
  DRAG_END,
  DRAG_DATA_DELETE,
  DRAG_LEAVE,
  DRAG_MOTION,
  DRAG_DROP,
  DRAG_DATA_GET,
  DRAG_DATA_RECEIVED,
  POPUP_MENU,
  ACCEL_CLOSURES_CHANGED,
  DISPLAY_CHANGED,
  CAN_ACTIVATE_ACCEL,
  QUERY_TOOLTIP,
  DRAG_FAILED,
  STYLE_UPDATED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_PARENT,
  PROP_ROOT,
  PROP_WIDTH_REQUEST,
  PROP_HEIGHT_REQUEST,
  PROP_VISIBLE,
  PROP_SENSITIVE,
  PROP_CAN_FOCUS,
  PROP_HAS_FOCUS,
  PROP_IS_FOCUS,
  PROP_CAN_TARGET,
  PROP_FOCUS_ON_CLICK,
  PROP_HAS_DEFAULT,
  PROP_RECEIVES_DEFAULT,
  PROP_CURSOR,
  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_MARKUP,
  PROP_TOOLTIP_TEXT,
  PROP_OPACITY,
  PROP_OVERFLOW,
  PROP_HALIGN,
  PROP_VALIGN,
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
  PROP_SCALE_FACTOR,
  PROP_CSS_NAME,
  PROP_LAYOUT_MANAGER,
  NUM_PROPERTIES
};

static GParamSpec *widget_props[NUM_PROPERTIES] = { NULL, };

typedef	struct	_GtkStateData	 GtkStateData;

struct _GtkStateData
{
  guint         flags_to_set;
  guint         flags_to_unset;

  gint          old_scale_factor;
};

/* --- prototypes --- */
static void	gtk_widget_base_class_init	(gpointer            g_class);
static void	gtk_widget_class_init		(GtkWidgetClass     *klass);
static void	gtk_widget_base_class_finalize	(GtkWidgetClass     *klass);
static void     gtk_widget_init                  (GTypeInstance     *instance,
                                                  gpointer           g_class);
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
static void	gtk_widget_real_size_allocate    (GtkWidget         *widget,
                                                  int                width,
                                                  int                height,
                                                  int                baseline);
static void	gtk_widget_real_direction_changed(GtkWidget         *widget,
                                                  GtkTextDirection   previous_direction);

static void	gtk_widget_real_grab_focus	 (GtkWidget         *focus_widget);
static gboolean gtk_widget_real_query_tooltip    (GtkWidget         *widget,
						  gint               x,
						  gint               y,
						  gboolean           keyboard_tip,
						  GtkTooltip        *tooltip);
static void     gtk_widget_real_style_updated    (GtkWidget         *widget);

static gboolean		gtk_widget_real_focus			(GtkWidget        *widget,
								 GtkDirectionType  direction);
static void             gtk_widget_real_move_focus              (GtkWidget        *widget,
                                                                 GtkDirectionType  direction);
static gboolean		gtk_widget_real_keynav_failed		(GtkWidget        *widget,
								 GtkDirectionType  direction);
#ifdef G_ENABLE_CONSISTENCY_CHECKS
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
static void             gtk_widget_propagate_state              (GtkWidget          *widget,
                                                                 const GtkStateData *data);
static void             gtk_widget_update_alpha                 (GtkWidget        *widget);

static gint		gtk_widget_event_internal		(GtkWidget	  *widget,
                                                                 const GdkEvent   *event);
static gboolean		gtk_widget_real_mnemonic_activate	(GtkWidget	  *widget,
								 gboolean	   group_cycling);
static void             gtk_widget_real_measure                 (GtkWidget        *widget,
                                                                 GtkOrientation    orientation,
                                                                 int               for_size,
                                                                 int              *minimum,
                                                                 int              *natural,
                                                                 int              *minimum_baseline,
                                                                 int              *natural_baseline);
static void             gtk_widget_real_state_flags_changed     (GtkWidget        *widget,
                                                                 GtkStateFlags     old_state);
static AtkObject*	gtk_widget_real_get_accessible		(GtkWidget	  *widget);
static void		gtk_widget_accessible_interface_init	(AtkImplementorIface *iface);
static AtkObject*	gtk_widget_ref_accessible		(AtkImplementor *implementor);
static gboolean         gtk_widget_real_can_activate_accel      (GtkWidget *widget,
                                                                 guint      signal_id);

static void             gtk_widget_buildable_interface_init     (GtkBuildableIface *iface);
static void             gtk_widget_buildable_set_name           (GtkBuildable     *buildable,
                                                                 const gchar      *name);
static const gchar *    gtk_widget_buildable_get_name           (GtkBuildable     *buildable);
static GObject *        gtk_widget_buildable_get_internal_child (GtkBuildable *buildable,
								 GtkBuilder   *builder,
								 const gchar  *childname);
static gboolean         gtk_widget_buildable_custom_tag_start   (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 GMarkupParser    *parser,
                                                                 gpointer         *data);
static void             gtk_widget_buildable_custom_tag_end     (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 gpointer          data);
static void             gtk_widget_buildable_custom_finished    (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 gpointer          data);
static void             gtk_widget_buildable_parser_finished    (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder);

static GtkSizeRequestMode gtk_widget_real_get_request_mode      (GtkWidget        *widget);

static void                  template_data_free                 (GtkWidgetTemplate*template_data);

static void gtk_widget_set_usize_internal (GtkWidget          *widget,
					   gint                width,
					   gint                height);

static gboolean event_surface_is_still_viewable (const GdkEvent *event);

static void gtk_widget_update_input_shape (GtkWidget *widget);

static gboolean gtk_widget_class_get_visible_by_default (GtkWidgetClass *widget_class);

static void remove_parent_surface_transform_changed_listener (GtkWidget *widget);
static void add_parent_surface_transform_changed_listener (GtkWidget *widget);


/* --- variables --- */
static gint             GtkWidget_private_offset = 0;
static gpointer         gtk_widget_parent_class = NULL;
static guint            widget_signals[LAST_SIGNAL] = { 0 };
GtkTextDirection gtk_default_direction = GTK_TEXT_DIR_LTR;

static GQuark		quark_accel_path = 0;
static GQuark		quark_accel_closures = 0;
static GQuark		quark_input_shape_info = 0;
static GQuark		quark_pango_context = 0;
static GQuark		quark_mnemonic_labels = 0;
static GQuark		quark_tooltip_markup = 0;
static GQuark		quark_tooltip_window = 0;
static GQuark           quark_size_groups = 0;
static GQuark           quark_auto_children = 0;
static GQuark           quark_widget_path = 0;
static GQuark           quark_action_muxer = 0;
static GQuark           quark_font_options = 0;
static GQuark           quark_font_map = 0;

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
	gtk_widget_init,
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

      widget_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, g_intern_static_string ("GtkWidget"),
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
gtk_widget_real_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    gtk_widget_snapshot_child (widget, child, snapshot);
}

static gboolean
gtk_widget_real_contains (GtkWidget *widget,
                          gdouble    x,
                          gdouble    y)
{
  GtkCssBoxes boxes;

  gtk_css_boxes_init (&boxes, widget);

  return gsk_rounded_rect_contains_point (gtk_css_boxes_get_border_box (&boxes),
                                          &GRAPHENE_POINT_INIT (x, y));
}

static void
gtk_widget_real_grab_notify (GtkWidget *widget,
                             gboolean   was_grabbed)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  for (l = g_list_last (priv->event_controllers); l; l = l->prev)
    {
      GtkEventController *controller = l->data;
      GdkDevice *device = NULL;

      if (GTK_IS_GESTURE (controller))
        device = gtk_gesture_get_device (GTK_GESTURE (controller));

      if (!device || !gtk_widget_device_is_shadowed (widget, device))
        continue;

      gtk_event_controller_reset (controller);
    }
}

static void
gtk_widget_real_root (GtkWidget *widget)
{
  gtk_widget_forall (widget, (GtkCallback) gtk_widget_root, NULL);
}

static void
gtk_widget_real_unroot (GtkWidget *widget)
{
  gtk_widget_forall (widget, (GtkCallback) gtk_widget_unroot, NULL);
}

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  g_type_class_adjust_private_offset (klass, &GtkWidget_private_offset);
  gtk_widget_parent_class = g_type_class_peek_parent (klass);

  quark_accel_path = g_quark_from_static_string ("gtk-accel-path");
  quark_accel_closures = g_quark_from_static_string ("gtk-accel-closures");
  quark_input_shape_info = g_quark_from_static_string ("gtk-input-shape-info");
  quark_pango_context = g_quark_from_static_string ("gtk-pango-context");
  quark_mnemonic_labels = g_quark_from_static_string ("gtk-mnemonic-labels");
  quark_tooltip_markup = g_quark_from_static_string ("gtk-tooltip-markup");
  quark_tooltip_window = g_quark_from_static_string ("gtk-tooltip-window");
  quark_size_groups = g_quark_from_static_string ("gtk-widget-size-groups");
  quark_auto_children = g_quark_from_static_string ("gtk-widget-auto-children");
  quark_widget_path = g_quark_from_static_string ("gtk-widget-path");
  quark_action_muxer = g_quark_from_static_string ("gtk-widget-action-muxer");
  quark_font_options = g_quark_from_static_string ("gtk-widget-font-options");
  quark_font_map = g_quark_from_static_string ("gtk-widget-font-map");

  gobject_class->constructed = gtk_widget_constructed;
  gobject_class->dispose = gtk_widget_dispose;
  gobject_class->finalize = gtk_widget_finalize;
  gobject_class->set_property = gtk_widget_set_property;
  gobject_class->get_property = gtk_widget_get_property;

  klass->destroy = gtk_widget_real_destroy;

  klass->activate_signal = 0;
  klass->show = gtk_widget_real_show;
  klass->hide = gtk_widget_real_hide;
  klass->map = gtk_widget_real_map;
  klass->unmap = gtk_widget_real_unmap;
  klass->realize = gtk_widget_real_realize;
  klass->unrealize = gtk_widget_real_unrealize;
  klass->root = gtk_widget_real_root;
  klass->unroot = gtk_widget_real_unroot;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->get_request_mode = gtk_widget_real_get_request_mode;
  klass->measure = gtk_widget_real_measure;
  klass->state_flags_changed = gtk_widget_real_state_flags_changed;
  klass->direction_changed = gtk_widget_real_direction_changed;
  klass->grab_notify = gtk_widget_real_grab_notify;
  klass->snapshot = gtk_widget_real_snapshot;
  klass->mnemonic_activate = gtk_widget_real_mnemonic_activate;
  klass->grab_focus = gtk_widget_real_grab_focus;
  klass->focus = gtk_widget_real_focus;
  klass->move_focus = gtk_widget_real_move_focus;
  klass->keynav_failed = gtk_widget_real_keynav_failed;
  klass->drag_begin = NULL;
  klass->drag_end = NULL;
  klass->drag_data_delete = NULL;
  klass->drag_leave = NULL;
  klass->drag_motion = NULL;
  klass->drag_drop = NULL;
  klass->drag_data_received = NULL;
  klass->can_activate_accel = gtk_widget_real_can_activate_accel;
  klass->query_tooltip = gtk_widget_real_query_tooltip;
  klass->style_updated = gtk_widget_real_style_updated;

  /* Accessibility support */
  klass->priv->accessible_type = GTK_TYPE_ACCESSIBLE;
  klass->priv->accessible_role = ATK_ROLE_INVALID;
  klass->get_accessible = gtk_widget_real_get_accessible;

  klass->contains = gtk_widget_real_contains;

  widget_props[PROP_NAME] =
      g_param_spec_string ("name",
                           P_("Widget name"),
                           P_("The name of the widget"),
                           NULL,
                           GTK_PARAM_READWRITE);

  widget_props[PROP_PARENT] =
      g_param_spec_object ("parent",
                           P_("Parent widget"),
                           P_("The parent widget of this widget."),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:root:
   *
   * The #GtkRoot widget of the widget tree containing this widget or %NULL if
   * the widget is not contained in a root widget.
   */
  widget_props[PROP_ROOT] =
      g_param_spec_object ("root",
                           P_("Root widget"),
                           P_("The root widget in the widget tree."),
                           GTK_TYPE_ROOT,
                           GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_WIDTH_REQUEST] =
      g_param_spec_int ("width-request",
                        P_("Width request"),
                        P_("Override for width request of the widget, or -1 if natural request should be used"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_HEIGHT_REQUEST] =
      g_param_spec_int ("height-request",
                        P_("Height request"),
                        P_("Override for height request of the widget, or -1 if natural request should be used"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_VISIBLE] =
      g_param_spec_boolean ("visible",
                            P_("Visible"),
                            P_("Whether the widget is visible"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_SENSITIVE] =
      g_param_spec_boolean ("sensitive",
                            P_("Sensitive"),
                            P_("Whether the widget responds to input"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_CAN_FOCUS] =
      g_param_spec_boolean ("can-focus",
                            P_("Can focus"),
                            P_("Whether the widget can accept the input focus"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_HAS_FOCUS] =
      g_param_spec_boolean ("has-focus",
                            P_("Has focus"),
                            P_("Whether the widget has the input focus"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_IS_FOCUS] =
      g_param_spec_boolean ("is-focus",
                            P_("Is focus"),
                            P_("Whether the widget is the focus widget within the toplevel"),
                            FALSE,
                            GTK_PARAM_READWRITE);

  widget_props[PROP_CAN_TARGET] =
      g_param_spec_boolean ("can-target",
                            P_("Can target"),
                            P_("Whether the widget can receive pointer events"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:focus-on-click:
   *
   * Whether the widget should grab focus when it is clicked with the mouse.
   *
   * This property is only relevant for widgets that can take focus.
   *
   * Before 3.20, several widgets (GtkButton, GtkFileChooserButton,
   * GtkComboBox) implemented this property individually.
   */
  widget_props[PROP_FOCUS_ON_CLICK] =
      g_param_spec_boolean ("focus-on-click",
                            P_("Focus on click"),
                            P_("Whether the widget should grab focus when it is clicked with the mouse"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_HAS_DEFAULT] =
      g_param_spec_boolean ("has-default",
                            P_("Has default"),
                            P_("Whether the widget is the default widget"),
                            FALSE,
                            GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  widget_props[PROP_RECEIVES_DEFAULT] =
      g_param_spec_boolean ("receives-default",
                            P_("Receives default"),
                            P_("If TRUE, the widget will receive the default action when it is focused"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

/**
 * GtkWidget:cursor:
 *
 * The cursor used by @widget. See gtk_widget_set_cursor() for details.
 */
  widget_props[PROP_CURSOR] =
      g_param_spec_object("cursor",
                          P_("Cursor"),
                          P_("The cursor to show when hoving above widget"),
                          GDK_TYPE_CURSOR,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

/**
 * GtkWidget:has-tooltip:
 *
 * Enables or disables the emission of #GtkWidget::query-tooltip on @widget.
 * A value of %TRUE indicates that @widget can have a tooltip, in this case
 * the widget will be queried using #GtkWidget::query-tooltip to determine
 * whether it will provide a tooltip or not.
 */
  widget_props[PROP_HAS_TOOLTIP] =
      g_param_spec_boolean ("has-tooltip",
                            P_("Has tooltip"),
                            P_("Whether this widget has a tooltip"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

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
   */
  widget_props[PROP_TOOLTIP_TEXT] =
      g_param_spec_string ("tooltip-text",
                           P_("Tooltip Text"),
                           P_("The contents of the tooltip for this widget"),
                           NULL,
                           GTK_PARAM_READWRITE);

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
   */
  widget_props[PROP_TOOLTIP_MARKUP] =
      g_param_spec_string ("tooltip-markup",
                           P_("Tooltip markup"),
                           P_("The contents of the tooltip for this widget"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkWidget:halign:
   *
   * How to distribute horizontal space if widget gets extra space, see #GtkAlign
   */
  widget_props[PROP_HALIGN] =
      g_param_spec_enum ("halign",
                         P_("Horizontal Alignment"),
                         P_("How to position in extra horizontal space"),
                         GTK_TYPE_ALIGN,
                         GTK_ALIGN_FILL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:valign:
   *
   * How to distribute vertical space if widget gets extra space, see #GtkAlign
   */
  widget_props[PROP_VALIGN] =
      g_param_spec_enum ("valign",
                         P_("Vertical Alignment"),
                         P_("How to position in extra vertical space"),
                         GTK_TYPE_ALIGN,
                         GTK_ALIGN_FILL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-start:
   *
   * Margin on start of widget, horizontally. This property supports
   * left-to-right and right-to-left text directions.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   */
  widget_props[PROP_MARGIN_START] =
      g_param_spec_int ("margin-start",
                        P_("Margin on Start"),
                        P_("Pixels of extra space on the start"),
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-end:
   *
   * Margin on end of widget, horizontally. This property supports
   * left-to-right and right-to-left text directions.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   */
  widget_props[PROP_MARGIN_END] =
      g_param_spec_int ("margin-end",
                        P_("Margin on End"),
                        P_("Pixels of extra space on the end"),
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-top:
   *
   * Margin on top side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   */
  widget_props[PROP_MARGIN_TOP] =
      g_param_spec_int ("margin-top",
                        P_("Margin on Top"),
                        P_("Pixels of extra space on the top side"),
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin-bottom:
   *
   * Margin on bottom side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   */
  widget_props[PROP_MARGIN_BOTTOM] =
      g_param_spec_int ("margin-bottom",
                        P_("Margin on Bottom"),
                        P_("Pixels of extra space on the bottom side"),
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:margin:
   *
   * Sets all four sides' margin at once. If read, returns max
   * margin on any side.
   */
  widget_props[PROP_MARGIN] =
      g_param_spec_int ("margin",
                        P_("All Margins"),
                        P_("Pixels of extra space on all four sides"),
                        0, G_MAXINT16,
                        0,
                        GTK_PARAM_READWRITE);

  /**
   * GtkWidget:hexpand:
   *
   * Whether to expand horizontally. See gtk_widget_set_hexpand().
   */
  widget_props[PROP_HEXPAND] =
      g_param_spec_boolean ("hexpand",
                            P_("Horizontal Expand"),
                            P_("Whether widget wants more horizontal space"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:hexpand-set:
   *
   * Whether to use the #GtkWidget:hexpand property. See gtk_widget_get_hexpand_set().
   */
  widget_props[PROP_HEXPAND_SET] =
      g_param_spec_boolean ("hexpand-set",
                            P_("Horizontal Expand Set"),
                            P_("Whether to use the hexpand property"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:vexpand:
   *
   * Whether to expand vertically. See gtk_widget_set_vexpand().
   */
  widget_props[PROP_VEXPAND] =
      g_param_spec_boolean ("vexpand",
                            P_("Vertical Expand"),
                            P_("Whether widget wants more vertical space"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:vexpand-set:
   *
   * Whether to use the #GtkWidget:vexpand property. See gtk_widget_get_vexpand_set().
   */
  widget_props[PROP_VEXPAND_SET] =
      g_param_spec_boolean ("vexpand-set",
                            P_("Vertical Expand Set"),
                            P_("Whether to use the vexpand property"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:expand:
   *
   * Whether to expand in both directions. Setting this sets both #GtkWidget:hexpand and #GtkWidget:vexpand
   */
  widget_props[PROP_EXPAND] =
      g_param_spec_boolean ("expand",
                            P_("Expand Both"),
                            P_("Whether widget wants to expand in both directions"),
                            FALSE,
                            GTK_PARAM_READWRITE);

  /**
   * GtkWidget:opacity:
   *
   * The requested opacity of the widget. See gtk_widget_set_opacity() for
   * more details about window opacity.
   *
   * Before 3.8 this was only available in GtkWindow
   */
  widget_props[PROP_OPACITY] =
      g_param_spec_double ("opacity",
                           P_("Opacity for Widget"),
                           P_("The opacity of the widget, from 0 to 1"),
                           0.0, 1.0,
                           1.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:overflow:
   *
   * How content outside the widget's content area is treated.
   */
  widget_props[PROP_OVERFLOW] =
      g_param_spec_enum ("overflow",
                         P_("Overflow"),
                         P_("How content outside the widget’s content area is treated"),
                         GTK_TYPE_OVERFLOW,
                         GTK_OVERFLOW_VISIBLE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkWidget:scale-factor:
   *
   * The scale factor of the widget. See gtk_widget_get_scale_factor() for
   * more details about widget scaling.
   */
  widget_props[PROP_SCALE_FACTOR] =
      g_param_spec_int ("scale-factor",
                        P_("Scale factor"),
                        P_("The scaling factor of the window"),
                        1, G_MAXINT,
                        1,
                        GTK_PARAM_READABLE);

  /**
   * GtkWidget:css-name:
   *
   * The name of this widget in the CSS tree.
   */
  widget_props[PROP_CSS_NAME] =
      g_param_spec_string ("css-name",
                           P_("CSS Name"),
                           P_("The name of this widget in the CSS tree"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * GtkWidget:layout-manager:
   *
   * The #GtkLayoutManager instance to use to compute the preferred size
   * of the widget, and allocate its children.
   */
  widget_props[PROP_LAYOUT_MANAGER] =
    g_param_spec_object ("layout-manager",
                         P_("Layout Manager"),
                         P_("The layout manager used to layout children of the widget"),
                         GTK_TYPE_LAYOUT_MANAGER,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, widget_props);

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
                  NULL,
                  G_TYPE_NONE, 0);

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
                  NULL,
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
                  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::map:
   * @widget: the object which received the signal.
   *
   * The ::map signal is emitted when @widget is going to be mapped, that is
   * when the widget is visible (which is controlled with
   * gtk_widget_set_visible()) and all its parents up to the toplevel widget
   * are also visible.
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
                  NULL,
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
                  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::realize:
   * @widget: the object which received the signal.
   *
   * The ::realize signal is emitted when @widget is associated with a
   * #GdkSurface, which means that gtk_widget_realize() has been called or the
   * widget has been mapped (that is, it is going to be drawn).
   */
  widget_signals[REALIZE] =
    g_signal_new (I_("realize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, realize),
		  NULL, NULL,
                  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unrealize:
   * @widget: the object which received the signal.
   *
   * The ::unrealize signal is emitted when the #GdkSurface associated with
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
                  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::size-allocate:
   * @widget: the object which received the signal.
   * @width: the content width of the widget
   * @height: the content height of the widget
   * @baseline: the baseline
   */
  widget_signals[SIZE_ALLOCATE] =
    g_signal_new (I_("size-allocate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, size_allocate),
		  NULL, NULL,
                  _gtk_marshal_VOID__INT_INT_INT,
		  G_TYPE_NONE, 3,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (widget_signals[SIZE_ALLOCATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_VOID__INT_INT_INTv);

  /**
   * GtkWidget::state-flags-changed:
   * @widget: the object which received the signal.
   * @flags: The previous state flags.
   *
   * The ::state-flags-changed signal is emitted when the widget state
   * changes, see gtk_widget_get_state_flags().
   */
  widget_signals[STATE_FLAGS_CHANGED] =
    g_signal_new (I_("state-flags-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, state_flags_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_STATE_FLAGS);

  /**
   * GtkWidget::style-updated:
   * @widget: the object on which the signal is emitted
   *
   * The ::style-updated signal is a convenience signal that is emitted when the
   * #GtkStyleContext::changed signal is emitted on the @widget's associated
   * #GtkStyleContext as returned by gtk_widget_get_style_context().
   */
  widget_signals[STYLE_UPDATED] =
    g_signal_new (I_("style-updated"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, style_updated),
                  NULL, NULL,
                  NULL,
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
		  NULL,
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
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkWidget::mnemonic-activate:
   * @widget: the object which received the signal.
   * @group_cycling: %TRUE if there are other widgets with the same mnemonic
   *
   * The default handler for this signal activates @widget if @group_cycling
   * is %FALSE, or just makes @widget grab focus if @group_cycling is %TRUE.
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
  g_signal_set_va_marshaller (widget_signals[MNEMONIC_ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

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
                  NULL,
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
   *          navigation attempt in its parent widget(s).
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
  g_signal_set_va_marshaller (widget_signals[KEYNAV_FAILED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__ENUMv);

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
		  NULL,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DROP);

  /**
   * GtkWidget::drag-begin:
   * @widget: the object which received the signal
   * @context: the drag context 
   *
   * The ::drag-begin signal is emitted on the drag source when a drag is
   * started. A typical reason to connect to this signal is to set up a
   * custom drag icon with e.g. gtk_drag_source_set_icon_paintable().
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
		  NULL,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG);

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
		  NULL,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG);

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
		  NULL,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG);

  /**
   * GtkWidget::drag-failed:
   * @widget: the object which received the signal
   * @context: the drag context
   * @result: the result of the drag operation
   *
   * The ::drag-failed signal is emitted on the drag source when a drag has
   * failed. The signal handler may hook custom code to handle a failed DnD
   * operation based on the type of error, it returns %TRUE is the failure has
   * been already handled (not showing the default "drag operation failed"
   * animation), otherwise it returns %FALSE.
   *
   * Returns: %TRUE if the failed drag operation has been already handled.
   */
  widget_signals[DRAG_FAILED] =
    g_signal_new (I_("drag-failed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_failed),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_ENUM,
		  G_TYPE_BOOLEAN, 2,
		  GDK_TYPE_DRAG,
		  GTK_TYPE_DRAG_RESULT);
  g_signal_set_va_marshaller (widget_signals[DRAG_FAILED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__OBJECT_ENUMv);

  /**
   * GtkWidget::drag-motion:
   * @widget: the object which received the signal
   * @drop: the #GdkDrop
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
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
   * handler. Note that you must pass #GTK_DEST_DEFAULT_DROP,
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
   * drag_motion (GtkWidget *widget,
   *              GdkDrop   *drop,
   *              gint       x,
   *              gint       y,
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
   *   target = gtk_drag_dest_find_target (widget, drop, NULL);
   *   if (target == NULL)
   *     gdk_drop_status (drop, 0);
   *   else
   *    {
   *      private_data->pending_status
   *         = gdk_drop_get_actions (drop);
   *      gtk_drag_get_data (widget, drop, target);
   *    }
   *
   *   return TRUE;
   * }
   *
   * static void
   * drag_data_received (GtkWidget        *widget,
   *                     GdkDrop          *drop,
   *                     GtkSelectionData *selection_data)
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
   *        gdk_drop_status (drop, 0);
   *      else
   *        gdk_drag_status (drop, GDK_ACTION_ALL);
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
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT,
		  G_TYPE_BOOLEAN, 3,
		  GDK_TYPE_DROP,
		  G_TYPE_INT,
		  G_TYPE_INT);
  g_signal_set_va_marshaller (widget_signals[DRAG_MOTION],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__OBJECT_INT_INTv);

  /**
   * GtkWidget::drag-drop:
   * @widget: the object which received the signal
   * @drop: the #GdkDrop
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   *
   * The ::drag-drop signal is emitted on the drop site when the user drops
   * the data onto the widget. The signal handler must determine whether
   * the cursor position is in a drop zone or not. If it is not in a drop
   * zone, it returns %FALSE and no further processing is necessary.
   * Otherwise, the handler returns %TRUE. In this case, the handler must
   * ensure that gdk_drag_finish() is called to let the source know that
   * the drop is done. The call to gdk_drag_finish() can be done either
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
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT,
		  G_TYPE_BOOLEAN, 3,
		  GDK_TYPE_DROP,
		  G_TYPE_INT,
		  G_TYPE_INT);
  g_signal_set_va_marshaller (widget_signals[DRAG_DROP],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__OBJECT_INT_INTv);

  /**
   * GtkWidget::drag-data-get:
   * @widget: the object which received the signal
   * @context: the drag context
   * @data: the #GtkSelectionData to be filled with the dragged data
   * @info: the info that has been registered with the target in the
   *        #GtkTargetList
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
		  _gtk_marshal_VOID__OBJECT_BOXED,
		  G_TYPE_NONE, 2,
		  GDK_TYPE_DRAG,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[DRAG_DATA_GET],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__OBJECT_BOXEDv);

  /**
   * GtkWidget::drag-data-received:
   * @widget: the object which received the signal
   * @drop: the #GdkDrop
   * @x: where the drop happened
   * @y: where the drop happened
   * @data: the received data
   *
   * The ::drag-data-received signal is emitted on the drop site when the
   * dragged data has been received. If the data was received in order to
   * determine whether the drop will be accepted, the handler is expected
   * to call gdk_drag_status() and not finish the drag.
   * If the data was received in response to a #GtkWidget::drag-drop signal
   * (and this is the last target to be received), the handler for this
   * signal is expected to process the received data and then call
   * gdk_drag_finish(), setting the @success parameter depending on
   * whether the data was processed successfully.
   *
   * Applications must create some means to determine why the signal was emitted 
   * and therefore whether to call gdk_drag_status() or gdk_drag_finish(). 
   *
   * The handler may inspect the selected action with
   * gdk_drag_context_get_selected_action() before calling
   * gdk_drag_finish(), e.g. to implement %GDK_ACTION_ASK as
   * shown in the following example:
   * |[<!-- language="C" -->
   * void
   * drag_data_received (GtkWidget          *widget,
   *                     GdkDrop            *drop,
   *                     GtkSelectionData   *data)
   * {
   *   if ((data->length >= 0) && (data->format == 8))
   *     {
   *       GdkDragAction action;
   *
   *       // handle data here
   *
   *       action = gdk_drop_get_actions (drop);
   *       if (!gdk_drag_action_is_unique (action))
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
   *       gdk_drop_finish (context, action);
   *     }
   *   else
   *     gdk_drop_finish (context, 0);
   *  }
   * ]|
   */
  widget_signals[DRAG_DATA_RECEIVED] =
    g_signal_new (I_("drag-data-received"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_BOXED,
		  G_TYPE_NONE, 2,
		  GDK_TYPE_DROP,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (widget_signals[DRAG_DATA_RECEIVED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__OBJECT_BOXEDv);

  /**
   * GtkWidget::query-tooltip:
   * @widget: the object which received the signal
   * @x: the x coordinate of the cursor position where the request has
   *     been emitted, relative to @widget's left side
   * @y: the y coordinate of the cursor position where the request has
   *     been emitted, relative to @widget's top
   * @keyboard_mode: %TRUE if the tooltip was triggered using the keyboard
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
  g_signal_set_va_marshaller (widget_signals[QUERY_TOOLTIP],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__INT_INT_BOOLEAN_OBJECTv);

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
  g_signal_set_va_marshaller (widget_signals[POPUP_MENU],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__VOIDv);

  /**
   * GtkWidget::accel-closures-changed:
   * @widget: the object which received the signal.
   *
   * The ::accel-closures-changed signal gets emitted when accelerators for this
   * widget get added, removed or changed.
   */
  widget_signals[ACCEL_CLOSURES_CHANGED] =
    g_signal_new (I_("accel-closures-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  0,
		  0,
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

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
  g_signal_set_va_marshaller (widget_signals[CAN_ACTIVATE_ACCEL],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__UINTv);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F10, GDK_SHIFT_MASK,
                                "popup-menu", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Menu, 0,
                                "popup-menu", 0);

  gtk_widget_class_set_accessible_type (klass, GTK_TYPE_WIDGET_ACCESSIBLE);
  gtk_widget_class_set_css_name (klass, I_("widget"));
}

static void
gtk_widget_base_class_finalize (GtkWidgetClass *klass)
{
  template_data_free (klass->priv->template);
  g_slist_free_full (klass->priv->shortcuts, g_object_unref);
}

static void
gtk_widget_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  switch (prop_id)
    {
      gboolean tmp;
      gchar *tooltip_markup;
      const gchar *tooltip_text;
      GtkWindow *tooltip_window;

    case PROP_NAME:
      gtk_widget_set_name (widget, g_value_get_string (value));
      break;
    case PROP_WIDTH_REQUEST:
      gtk_widget_set_usize_internal (widget, g_value_get_int (value), -2);
      break;
    case PROP_HEIGHT_REQUEST:
      gtk_widget_set_usize_internal (widget, -2, g_value_get_int (value));
      break;
    case PROP_VISIBLE:
      gtk_widget_set_visible (widget, g_value_get_boolean (value));
      break;
    case PROP_SENSITIVE:
      gtk_widget_set_sensitive (widget, g_value_get_boolean (value));
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
    case PROP_CAN_TARGET:
      gtk_widget_set_can_target (widget, g_value_get_boolean (value));
      break;
    case PROP_FOCUS_ON_CLICK:
      gtk_widget_set_focus_on_click (widget, g_value_get_boolean (value));
      break;
    case PROP_RECEIVES_DEFAULT:
      gtk_widget_set_receives_default (widget, g_value_get_boolean (value));
      break;
    case PROP_CURSOR:
      gtk_widget_set_cursor (widget, g_value_get_object (value));
      break;
    case PROP_HAS_TOOLTIP:
      gtk_widget_set_has_tooltip (widget, g_value_get_boolean (value));
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
      gtk_widget_set_has_tooltip (widget, tmp);
      if (_gtk_widget_get_visible (widget))
        gtk_widget_trigger_tooltip_query (widget);
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
      gtk_widget_set_has_tooltip (widget, tmp);
      if (_gtk_widget_get_visible (widget))
        gtk_widget_trigger_tooltip_query (widget);
      break;
    case PROP_HALIGN:
      gtk_widget_set_halign (widget, g_value_get_enum (value));
      break;
    case PROP_VALIGN:
      gtk_widget_set_valign (widget, g_value_get_enum (value));
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
    case PROP_OVERFLOW:
      gtk_widget_set_overflow (widget, g_value_get_enum (value));
      break;
    case PROP_CSS_NAME:
      if (g_value_get_string (value) != NULL)
        gtk_css_node_set_name (priv->cssnode, g_intern_string (g_value_get_string (value)));
      else
        gtk_css_node_set_name (priv->cssnode, GTK_WIDGET_GET_CLASS (widget)->priv->css_name);
      break;
    case PROP_LAYOUT_MANAGER:
      gtk_widget_set_layout_manager (widget, g_value_get_object (value));
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  switch (prop_id)
    {
    case PROP_NAME:
      if (priv->name)
	g_value_set_string (value, priv->name);
      else
	g_value_set_static_string (value, "");
      break;
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    case PROP_ROOT:
      g_value_set_object (value, priv->root);
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
      g_value_set_boolean (value, _gtk_widget_get_visible (widget));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, gtk_widget_get_sensitive (widget));
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
    case PROP_CAN_TARGET:
      g_value_set_boolean (value, gtk_widget_get_can_target (widget));
      break;
    case PROP_FOCUS_ON_CLICK:
      g_value_set_boolean (value, gtk_widget_get_focus_on_click (widget));
      break;
    case PROP_HAS_DEFAULT:
      g_value_set_boolean (value, gtk_widget_has_default (widget));
      break;
    case PROP_RECEIVES_DEFAULT:
      g_value_set_boolean (value, gtk_widget_get_receives_default (widget));
      break;
    case PROP_CURSOR:
      g_value_set_object (value, gtk_widget_get_cursor (widget));
      break;
    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, gtk_widget_get_has_tooltip (widget));
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
    case PROP_HALIGN:
      g_value_set_enum (value, gtk_widget_get_halign (widget));
      break;
    case PROP_VALIGN:
      g_value_set_enum (value, gtk_widget_get_valign (widget));
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
      g_value_set_int (value, MAX (MAX (priv->margin.left,
                                        priv->margin.right),
                                   MAX (priv->margin.top,
                                        priv->margin.bottom)));
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
    case PROP_OVERFLOW:
      g_value_set_enum (value, gtk_widget_get_overflow (widget));
      break;
    case PROP_SCALE_FACTOR:
      g_value_set_int (value, gtk_widget_get_scale_factor (widget));
      break;
    case PROP_CSS_NAME:
      g_value_set_string (value, gtk_css_node_get_name (priv->cssnode));
      break;
    case PROP_LAYOUT_MANAGER:
      g_value_set_object (value, gtk_widget_get_layout_manager (widget));
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
  gdouble x, y;
  graphene_point_t p;

  event_widget = gtk_get_event_target ((GdkEvent *) event);

  if (event_widget == widget)
    return;

  gdk_event_get_coords (event, &x, &y);
  if (!gtk_widget_compute_point (event_widget,
                                 GTK_WIDGET (gtk_widget_get_root (event_widget)),
                                 &GRAPHENE_POINT_INIT (x, y),
                                 &p))
      return;

  if (event->any.type == GDK_TOUCH_BEGIN ||
      event->any.type == GDK_TOUCH_UPDATE ||
      event->any.type == GDK_TOUCH_END)
    {
      press = gdk_event_copy (event);
      press->any.type = GDK_TOUCH_BEGIN;
    }
  else if (event->any.type == GDK_BUTTON_PRESS ||
           event->any.type == GDK_BUTTON_RELEASE)
    {
      press = gdk_event_copy (event);
      press->any.type = GDK_BUTTON_PRESS;
    }
  else if (event->any.type == GDK_MOTION_NOTIFY)
    {
      press = gdk_event_new (GDK_BUTTON_PRESS);
      press->any.surface = g_object_ref (event->any.surface);
      press->button.time = event->motion.time;
      press->button.state = event->motion.state;

      press->button.axes = g_memdup (event->motion.axes,
                                     sizeof (gdouble) *
                                     gdk_device_get_n_axes (event->any.device));

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

  gdk_event_set_coords (press, p.x, p.y);

  press->any.send_event = TRUE;
  next_child = event_widget;
  parent = _gtk_widget_get_parent (next_child);

  while (parent && parent != widget)
    {
      next_child = parent;
      parent = _gtk_widget_get_parent (parent);
    }

  /* Perform propagation state starting from the next child in the chain */
  gtk_propagate_event_internal (event_widget, press, next_child);
  g_object_unref (press);
}

static const GdkEvent *
_gtk_widget_get_last_event (GtkWidget        *widget,
                            GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkEventController *controller;
  const GdkEvent *event;
  GList *l;

  for (l = priv->event_controllers; l; l = l->next)
    {
      controller = l->data;

      if (!GTK_IS_GESTURE (controller))
        continue;

      event = gtk_gesture_get_last_event (GTK_GESTURE (controller),
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  *sequence_out = sequence;

  if (sequence)
    {
      const GdkEvent *last_event;

      last_event = _gtk_widget_get_last_event (widget, sequence);

      if (last_event &&
          (last_event->any.type == GDK_TOUCH_BEGIN ||
           last_event->any.type == GDK_TOUCH_UPDATE ||
           last_event->any.type == GDK_TOUCH_END) &&
          last_event->touch.emulating_pointer)
        return TRUE;
    }
  else
    {
      /* For a NULL(pointer) sequence, find the pointer emulating one */
      for (l = priv->event_controllers; l; l = l->next)
        {
          GtkEventController *controller = l->data;

          if (!GTK_IS_GESTURE (controller))
            continue;

          if (_gtk_gesture_get_pointer_emulating_sequence (GTK_GESTURE (controller),
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean sequence_press_handled = FALSE;
  GList *l;

  /* Check whether there is any remaining gesture in
   * the capture phase that handled the press event
   */
  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;
      GtkPropagationPhase phase;
      GtkGesture *gesture;

      phase = gtk_event_controller_get_propagation_phase (controller);

      if (phase != GTK_PHASE_CAPTURE)
        continue;
      if (!GTK_IS_GESTURE (controller))
        continue;

      gesture = GTK_GESTURE (controller);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean emulates_pointer, sequence_handled = FALSE;
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
      GtkEventController *controller;
      GtkGesture *gesture;
      gboolean retval;

      seq = sequence;
      controller = l->data;
      gesture_state = state;

      if (!GTK_IS_GESTURE (controller))
        continue;

      gesture = GTK_GESTURE (controller);

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

      if (group && !g_list_find (group, controller))
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

      retval = gtk_gesture_set_sequence_state (gesture, seq, gesture_state);

      if (retval || gesture == emitter)
        {
          sequence_handled |=
            _gtk_gesture_handled_sequence_press (gesture, seq);
          n_handled++;
        }
    }

  /* If the sequence goes denied, check whether this is a controller attached
   * to the capture phase, that additionally handled the button/touch press (i.e.
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean emulates_pointer;
  gboolean handled = FALSE;
  GdkEventSequence *seq;
  GList *l;

  emulates_pointer = _gtk_widget_get_emulating_sequence (widget, sequence, &seq);

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller;
      GtkGesture *gesture;

      seq = sequence;
      controller = l->data;

      if (!GTK_IS_GESTURE (controller))
        continue;

      gesture = GTK_GESTURE (controller);

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
gtk_widget_init (GTypeInstance *instance, gpointer g_class)
{
  GtkWidget *widget = GTK_WIDGET (instance);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GType layout_manager_type;

  widget->priv = priv;

  priv->visible = gtk_widget_class_get_visible_by_default (g_class);
  priv->child_visible = TRUE;
  priv->name = NULL;
  priv->user_alpha = 255;
  priv->alpha = 255;
  priv->surface = NULL;
  priv->parent = NULL;
  priv->first_child = NULL;
  priv->last_child = NULL;
  priv->prev_sibling = NULL;
  priv->next_sibling = NULL;
  priv->baseline = -1;
  priv->allocated_size_baseline = -1;

  priv->sensitive = TRUE;
  priv->alloc_needed = TRUE;
  priv->alloc_needed_on_child = TRUE;
  priv->draw_needed = TRUE;
  priv->focus_on_click = TRUE;
#ifdef G_ENABLE_DEBUG
  priv->highlight_resize = FALSE;
#endif
  priv->can_target = TRUE;

  switch (_gtk_widget_get_direction (widget))
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

  priv->halign = GTK_ALIGN_FILL;
  priv->valign = GTK_ALIGN_FILL;

  priv->width_request = -1;
  priv->height_request = -1;

  _gtk_size_request_cache_init (&priv->requests);

  priv->cssnode = gtk_css_widget_node_new (widget);
  gtk_css_node_set_state (priv->cssnode, priv->state_flags);
  gtk_css_node_set_visible (priv->cssnode, priv->visible);
  /* need to set correct type here, and only class has the correct type here */
  gtk_css_node_set_widget_type (priv->cssnode, G_TYPE_FROM_CLASS (g_class));

  if (g_type_is_a (G_TYPE_FROM_CLASS (g_class), GTK_TYPE_ROOT))
    priv->root = (GtkRoot *) widget;

  layout_manager_type = gtk_widget_class_get_layout_manager_type (g_class);
  if (layout_manager_type != G_TYPE_INVALID)
    gtk_widget_set_layout_manager (widget, g_object_new (layout_manager_type, NULL));

  gtk_widget_add_controller (widget, gtk_shortcut_controller_new ());
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

void
gtk_widget_root (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (!priv->realized);

  if (GTK_IS_ROOT (widget))
    {
      g_assert (priv->root == GTK_ROOT (widget));
    }
  else
    {
      g_assert (priv->root == NULL);
      priv->root = priv->parent->priv->root;
    }

  if (priv->context)
    gtk_style_context_set_display (priv->context, gtk_root_get_display (priv->root));

  if (priv->surface_transform_data)
    add_parent_surface_transform_changed_listener (widget);

  GTK_WIDGET_GET_CLASS (widget)->root (widget);

  if (!GTK_IS_ROOT (widget))
    g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_ROOT]);
}

void
gtk_widget_unroot (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data;

  g_assert (priv->root);
  g_assert (!priv->realized);

  surface_transform_data = priv->surface_transform_data;
  if (surface_transform_data &&
      surface_transform_data->tracked_parent)
    remove_parent_surface_transform_changed_listener (widget);

  GTK_WIDGET_GET_CLASS (widget)->unroot (widget);

  if (priv->context)
    gtk_style_context_set_display (priv->context, gdk_display_get_default ());

  if (g_object_get_qdata (G_OBJECT (widget), quark_pango_context))
    g_object_set_qdata (G_OBJECT (widget), quark_pango_context, NULL);

  _gtk_tooltip_hide (widget);

  if (!GTK_IS_ROOT (widget))
    {
      priv->root = NULL;
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_ROOT]);
    }
}

/**
 * gtk_widget_unparent:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 * Should be called by parent widgets to dissociate @widget
 * from the parent.
 **/
void
gtk_widget_unparent (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *old_parent;
  GtkWidget *old_prev_sibling;
  GtkRoot *root;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->parent == NULL)
    return;

  /* keep this function in sync with gtk_menu_detach() */

  gtk_widget_push_verify_invariants (widget);

  g_object_freeze_notify (G_OBJECT (widget));

  root = _gtk_widget_get_root (widget);
  if (GTK_IS_WINDOW (root))
    _gtk_window_unset_focus_and_default (GTK_WINDOW (root), widget);

  if (gtk_widget_get_focus_child (priv->parent) == widget)
    gtk_widget_set_focus_child (priv->parent, NULL);

  if (_gtk_widget_is_drawable (priv->parent))
    gtk_widget_queue_draw (priv->parent);

  if (priv->visible && _gtk_widget_get_visible (priv->parent))
    gtk_widget_queue_resize (priv->parent);

  /* Reset the width and height here, to force reallocation if we
   * get added back to a new parent.
   */
  priv->width = 0;
  priv->height = 0;

  if (_gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  if (priv->root)
    gtk_widget_unroot (widget);

  root = NULL;

  /* Removing a widget from a container restores the child visible
   * flag to the default state, so it doesn't affect the child
   * in the next parent.
   */
  priv->child_visible = TRUE;

  old_parent = priv->parent;
  if (old_parent)
    {
      if (old_parent->priv->first_child == widget)
        old_parent->priv->first_child = priv->next_sibling;

      if (old_parent->priv->last_child == widget)
        old_parent->priv->last_child = priv->prev_sibling;

      if (priv->prev_sibling)
        priv->prev_sibling->priv->next_sibling = priv->next_sibling;
      if (priv->next_sibling)
        priv->next_sibling->priv->prev_sibling = priv->prev_sibling;
    }
  old_prev_sibling = priv->prev_sibling;
  priv->parent = NULL;
  priv->prev_sibling = NULL;
  priv->next_sibling = NULL;

  /* parent may no longer expand if the removed
   * child was expand=TRUE and could therefore
   * be forcing it to.
   */
  if (_gtk_widget_get_visible (widget) &&
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
  gtk_css_node_set_parent (priv->cssnode, NULL);

  _gtk_widget_update_parent_muxer (widget);

  if (old_parent->priv->children_observer)
    gtk_list_list_model_item_removed (old_parent->priv->children_observer, old_prev_sibling);

  if (old_parent->priv->layout_manager)
    gtk_layout_manager_remove_layout_child (old_parent->priv->layout_manager, widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_PARENT]);
  g_object_thaw_notify (G_OBJECT (widget));

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_destroy:
 * @widget: a #GtkWidget
 *
 * Destroys a widget.
 *
 * When a widget is destroyed all references it holds on other objects
 * will be released:
 *
 *  - if the widget is inside a container, it will be removed from its
 *  parent
 *  - if the widget is a container, all its children will be destroyed,
 *  recursively
 *  - if the widget is a top level, it will be removed from the list
 *  of top level widgets that GTK+ maintains internally
 *
 * It's expected that all references held on the widget will also
 * be released; you should connect to the #GtkWidget::destroy signal
 * if you hold a reference to @widget and you wish to remove it when
 * this function is called. It is not necessary to do so if you are
 * implementing a #GtkContainer, as you'll be able to use the
 * #GtkContainerClass.remove() virtual function for that.
 *
 * It's important to notice that gtk_widget_destroy() will only cause
 * the @widget to be finalized if no additional references, acquired
 * using g_object_ref(), are held on it. In case additional references
 * are in place, the @widget will be in an "inert" state after calling
 * this function; @widget will still point to valid memory, allowing you
 * to release the references you hold, but you may not query the widget's
 * own state.
 *
 * You should typically call this function on top level widgets, and
 * rarely on child widgets.
 *
 * See also: gtk_container_remove()
 */
void
gtk_widget_destroy (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!priv->in_destruction)
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

static void
gtk_widget_update_paintables (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *l;

  for (l = priv->paintables; l; l = l->next)
    gtk_widget_paintable_update_image (l->data);
}

static void
gtk_widget_push_paintables (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *l;

  for (l = priv->paintables; l; l = l->next)
    gtk_widget_paintable_push_snapshot_count (l->data);
}

static void
gtk_widget_pop_paintables (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *l;

  for (l = priv->paintables; l; l = l->next)
    gtk_widget_paintable_pop_snapshot_count (l->data);
}

/**
 * gtk_widget_show:
 * @widget: a #GtkWidget
 *
 * Flags a widget to be displayed. Any widget that isn’t shown will
 * not appear on the screen.
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!_gtk_widget_get_visible (widget))
    {
      GtkWidget *parent;

      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      parent = _gtk_widget_get_parent (widget);
      if (parent)
        {
          gtk_widget_queue_resize (parent);

          /* see comment in set_parent() for why this should and can be
           * conditional
           */
          if (priv->need_compute_expand ||
              priv->computed_hexpand ||
              priv->computed_vexpand)
            gtk_widget_queue_compute_expand (parent);
        }

      gtk_css_node_set_visible (priv->cssnode, TRUE);

      g_signal_emit (widget, widget_signals[SHOW], 0);
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_VISIBLE]);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_show (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (!_gtk_widget_get_visible (widget));

  priv->visible = TRUE;

  if (priv->parent &&
      _gtk_widget_get_mapped (priv->parent) &&
      _gtk_widget_get_child_visible (widget) &&
      !_gtk_widget_get_mapped (widget))
    gtk_widget_map (widget);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_visible (widget))
    {
      GtkWidget *parent;
      GtkRoot *root;

      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      root = _gtk_widget_get_root (widget);
      if (GTK_WIDGET (root) != widget && GTK_IS_WINDOW (root))
        _gtk_window_unset_focus_and_default (GTK_WINDOW (root), widget);

      /* a parent may now be expand=FALSE since we're hidden. */
      if (priv->need_compute_expand ||
          priv->computed_hexpand ||
          priv->computed_vexpand)
        {
          gtk_widget_queue_compute_expand (widget);
        }

      gtk_css_node_set_visible (priv->cssnode, FALSE);

      g_signal_emit (widget, widget_signals[HIDE], 0);
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_VISIBLE]);

      parent = gtk_widget_get_parent (widget);
      if (parent)
	gtk_widget_queue_resize (parent);

      gtk_widget_queue_allocate (widget);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_hide (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (_gtk_widget_get_visible (widget));

  priv->visible = FALSE;

  if (_gtk_widget_get_mapped (widget))
    gtk_widget_unmap (widget);

  g_clear_pointer (&priv->allocated_transform, gsk_transform_unref);
  priv->allocated_width = 0;
  priv->allocated_height = 0;
  priv->allocated_size_baseline = 0;
  g_clear_pointer (&priv->transform, gsk_transform_unref);
  priv->width = 0;
  priv->height = 0;
  gtk_widget_update_paintables (widget);
}

static void
update_cursor_on_state_change (GtkWidget *widget)
{
  GtkRoot *root;

  root = _gtk_widget_get_root (widget);
  if (root)
    gtk_window_update_pointer_focus_on_state_change (GTK_WINDOW (root),
                                                     widget);
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
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (_gtk_widget_get_visible (widget));
  g_return_if_fail (_gtk_widget_get_child_visible (widget));

  if (!_gtk_widget_get_mapped (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      if (!_gtk_widget_get_realized (widget))
        gtk_widget_realize (widget);

      g_signal_emit (widget, widget_signals[MAP], 0);

      update_cursor_on_state_change (widget);

      gtk_widget_queue_draw (widget);

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
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_mapped (widget))
    {
      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      gtk_widget_queue_draw (widget);
      _gtk_tooltip_hide (widget);

      g_signal_emit (widget, widget_signals[UNMAP], 0);

      update_cursor_on_state_change (widget);

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

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
destroy_tick_callbacks (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  for (l = priv->tick_callbacks; l;)
    {
      GList *next = l->next;
      destroy_tick_callback_info (widget, l->data, l);
      l = next;
    }
}

static void
gtk_widget_on_frame_clock_update (GdkFrameClock *frame_clock,
                                  GtkWidget     *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
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
 * gtk_widget_queue_draw() yourself.
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
 *     by passing the id returned from this function to
 *     gtk_widget_remove_tick_callback()
 */
guint
gtk_widget_add_tick_callback (GtkWidget       *widget,
                              GtkTickCallback  callback,
                              gpointer         user_data,
                              GDestroyNotify   notify)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkTickCallbackInfo *info;
  GdkFrameClock *frame_clock;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  if (priv->realized && !priv->clock_tick_id)
    {
      frame_clock = gtk_widget_get_frame_clock (widget);

      if (frame_clock)
        {
          priv->clock_tick_id = g_signal_connect (frame_clock, "update",
                                                  G_CALLBACK (gtk_widget_on_frame_clock_update),
                                                  widget);
          gdk_frame_clock_begin_updating (frame_clock);
        }
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
 */
void
gtk_widget_remove_tick_callback (GtkWidget *widget,
                                 guint      id)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));

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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->tick_callbacks != NULL;
}

static void
gtk_widget_connect_frame_clock (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GdkFrameClock *frame_clock;

  if (GTK_IS_CONTAINER (widget) && GTK_IS_ROOT (widget))
    gtk_container_start_idle_sizer (GTK_CONTAINER (widget));

  frame_clock = gtk_widget_get_frame_clock (widget);

  if (priv->tick_callbacks != NULL && !priv->clock_tick_id)
    {
      priv->clock_tick_id = g_signal_connect (frame_clock, "update",
                                              G_CALLBACK (gtk_widget_on_frame_clock_update),
                                              widget);
      gdk_frame_clock_begin_updating (frame_clock);
    }

  gtk_css_node_invalidate_frame_clock (priv->cssnode, FALSE);
}

static void
gtk_widget_disconnect_frame_clock (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (GTK_IS_CONTAINER (widget) && GTK_IS_ROOT (widget))
    gtk_container_stop_idle_sizer (GTK_CONTAINER (widget));

  gtk_css_node_invalidate_frame_clock (priv->cssnode, FALSE);

  if (priv->clock_tick_id)
    {
      GdkFrameClock *frame_clock;

      frame_clock = gtk_widget_get_frame_clock (widget);

      g_signal_handler_disconnect (frame_clock, priv->clock_tick_id);
      priv->clock_tick_id = 0;
      gdk_frame_clock_end_updating (frame_clock);
    }
}

typedef struct _GtkSurfaceTransformChangedCallbackInfo GtkSurfaceTransformChangedCallbackInfo;

struct _GtkSurfaceTransformChangedCallbackInfo
{
  guint id;
  GtkSurfaceTransformChangedCallback callback;
  gpointer user_data;
  GDestroyNotify notify;
};

static void
surface_transform_changed_callback_info_destroy (GtkSurfaceTransformChangedCallbackInfo *info)
{
  if (info->notify)
    info->notify (info->user_data);

  g_slice_free (GtkSurfaceTransformChangedCallbackInfo, info);
}

static void
notify_surface_transform_changed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;
  graphene_matrix_t *surface_transform;
  GList *l;

  if (surface_transform_data->cached_surface_transform_valid)
    surface_transform = &surface_transform_data->cached_surface_transform;
  else
    surface_transform = NULL;

  for (l = surface_transform_data->callbacks; l;)
    {
      GtkSurfaceTransformChangedCallbackInfo *info = l->data;
      GList *l_next = l->next;

      if (info->callback (widget,
                          surface_transform,
                          info->user_data) == G_SOURCE_REMOVE)
        {
          surface_transform_data->callbacks =
            g_list_delete_link (surface_transform_data->callbacks, l);
          surface_transform_changed_callback_info_destroy (info);
        }

      l = l_next;
    }
}

static void
destroy_surface_transform_data (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data;

  surface_transform_data = priv->surface_transform_data;
  if (!surface_transform_data)
    return;

  g_list_free_full (surface_transform_data->callbacks,
                    (GDestroyNotify) surface_transform_changed_callback_info_destroy);
  g_slice_free (GtkWidgetSurfaceTransformData, surface_transform_data);
  priv->surface_transform_data = NULL;
}

static void
sync_widget_surface_transform (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;
  gboolean was_valid;
  graphene_matrix_t prev_transform;

  was_valid = surface_transform_data->cached_surface_transform_valid;
  prev_transform = surface_transform_data->cached_surface_transform;

  if (GTK_IS_NATIVE (widget))
    {
      gsk_transform_to_matrix (priv->transform,
                               &surface_transform_data->cached_surface_transform);
      surface_transform_data->cached_surface_transform_valid = TRUE;
    }
  else if (!priv->root)
    {
      surface_transform_data->cached_surface_transform_valid = FALSE;
    }
  else
    {
      GtkWidget *native = GTK_WIDGET (gtk_widget_get_native (widget));

      if (gtk_widget_compute_transform (widget, native,
                                        &surface_transform_data->cached_surface_transform))
        {
          surface_transform_data->cached_surface_transform_valid = TRUE;
        }
      else
        {
          g_warning ("Could not compute surface transform");
          surface_transform_data->cached_surface_transform_valid = FALSE;
        }
    }

  if (was_valid != surface_transform_data->cached_surface_transform_valid ||
      (was_valid && surface_transform_data->cached_surface_transform_valid &&
       !graphene_matrix_equal (&surface_transform_data->cached_surface_transform,
                               &prev_transform)))
    notify_surface_transform_changed (widget);
}

static guint surface_transform_changed_callback_id;

static gboolean
parent_surface_transform_changed_cb (GtkWidget               *parent,
                                     const graphene_matrix_t *transform,
                                     gpointer                 user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);

  sync_widget_surface_transform (widget);

  return G_SOURCE_CONTINUE;
}

static void
remove_parent_surface_transform_changed_listener (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;

  g_assert (surface_transform_data->tracked_parent);

  gtk_widget_remove_surface_transform_changed_callback (
    surface_transform_data->tracked_parent,
    surface_transform_data->parent_surface_transform_changed_id);
  surface_transform_data->parent_surface_transform_changed_id = 0;
  g_clear_object (&surface_transform_data->tracked_parent);
}

static void
add_parent_surface_transform_changed_listener (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidgetSurfaceTransformData *surface_transform_data =
    priv->surface_transform_data;
  GtkWidget *parent;


  g_assert (!surface_transform_data->tracked_parent);

  parent = priv->parent;
  surface_transform_data->parent_surface_transform_changed_id =
    gtk_widget_add_surface_transform_changed_callback (
      parent,
      parent_surface_transform_changed_cb,
      widget,
      NULL);
  surface_transform_data->tracked_parent = g_object_ref (parent);
}

static GtkWidgetSurfaceTransformData *
ensure_surface_transform_data (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->surface_transform_data)
    priv->surface_transform_data = g_slice_new0 (GtkWidgetSurfaceTransformData);

  return priv->surface_transform_data;
}

/**
 * gtk_widget_add_surface_transform_changed_callback:
 * @widget: a #GtkWidget
 * @callback: a function to call when the surface transform changes
 * @user_data: data to pass to @callback
 * @notify: function to call to free @user_data when the callback is removed
 *
 * Invokes the callback whenever the surface relative transform of the widget
 * changes.
 *
 * Returns: an id for the connection of this callback. Remove the callback by
 *     passing the id returned from this funcction to
 *     gtk_widget_remove_surface_transform_changed_callback()
 */
guint
gtk_widget_add_surface_transform_changed_callback (GtkWidget                          *widget,
                                                   GtkSurfaceTransformChangedCallback  callback,
                                                   gpointer                            user_data,
                                                   GDestroyNotify                      notify)
{
  GtkWidgetPrivate *priv;
  GtkWidgetSurfaceTransformData *surface_transform_data;
  GtkSurfaceTransformChangedCallbackInfo *info;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (callback, 0);

  priv = gtk_widget_get_instance_private (widget);
  surface_transform_data = ensure_surface_transform_data (widget);

  if (priv->parent &&
      !surface_transform_data->parent_surface_transform_changed_id)
    add_parent_surface_transform_changed_listener (widget);

  if (!surface_transform_data->callbacks)
    sync_widget_surface_transform (widget);

  info = g_slice_new0 (GtkSurfaceTransformChangedCallbackInfo);

  info->id = ++surface_transform_changed_callback_id;
  info->callback = callback;
  info->user_data = user_data;
  info->notify = notify;

  surface_transform_data->callbacks =
    g_list_prepend (surface_transform_data->callbacks, info);

  return info->id;
}

/**
 * gtk_widget_remove_surface_transform_changed_callback:
 * @widget: a #GtkWidget
 * @id: an id returned by gtk_widget_add_surface_transform_changed_callback()
 *
 * Removes a surface transform changed callback previously registered with
 * gtk_widget_add_surface_transform_changed_callback().
 */
void
gtk_widget_remove_surface_transform_changed_callback (GtkWidget *widget,
                                                      guint      id)
{
  GtkWidgetPrivate *priv;
  GtkWidgetSurfaceTransformData *surface_transform_data;
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (id);

  priv = gtk_widget_get_instance_private (widget);
  surface_transform_data = priv->surface_transform_data;

  g_return_if_fail (surface_transform_data);

  for (l = surface_transform_data->callbacks; l; l = l->next)
    {
      GtkSurfaceTransformChangedCallbackInfo *info = l->data;

      if (info->id == id)
        {
          surface_transform_data->callbacks =
            g_list_delete_link (surface_transform_data->callbacks, l);

          surface_transform_changed_callback_info_destroy (info);
          break;
        }
    }

  if (!surface_transform_data->callbacks)
    {
      if (surface_transform_data->tracked_parent)
        remove_parent_surface_transform_changed_listener (widget);
      g_slice_free (GtkWidgetSurfaceTransformData, surface_transform_data);
      priv->surface_transform_data = NULL;
    }
}

/**
 * gtk_widget_realize:
 * @widget: a #GtkWidget
 *
 * Creates the GDK (windowing system) resources associated with a
 * widget.  For example, @widget->surface will be created when a widget
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
 * #GtkWidget::realize.
 **/
void
gtk_widget_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!_gtk_widget_get_realized (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      /*
	if (GTK_IS_CONTAINER (widget) && _gtk_widget_get_has_surface (widget))
	  g_message ("gtk_widget_realize(%s)", G_OBJECT_TYPE_NAME (widget));
      */

      if (priv->parent == NULL && !GTK_IS_ROOT (widget))
        g_warning ("Calling gtk_widget_realize() on a widget that isn't "
                   "inside a toplevel window is not going to work very well. "
                   "Widgets must be inside a toplevel container before realizing them.");

      if (priv->parent && !_gtk_widget_get_realized (priv->parent))
	gtk_widget_realize (priv->parent);

      g_signal_emit (widget, widget_signals[REALIZE], 0);

      gtk_widget_update_input_shape (widget);

      if (priv->multidevice)
        gdk_surface_set_support_multidevice (priv->surface, TRUE);

      gtk_widget_update_alpha (widget);

      if (priv->context)
	gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));

      gtk_widget_pop_verify_invariants (widget);
    }
}

/**
 * gtk_widget_unrealize:
 * @widget: a #GtkWidget
 *
 * This function is only useful in widget implementations.
 * Causes a widget to be unrealized (frees all GDK resources
 * associated with the widget, such as @widget->surface).
 **/
void
gtk_widget_unrealize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (widget);
  gtk_widget_push_verify_invariants (widget);

  if (g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info))
    gtk_widget_input_shape_combine_region (widget, NULL);

  if (_gtk_widget_get_realized (widget))
    {
      if (priv->mapped)
        gtk_widget_unmap (widget);

      g_signal_emit (widget, widget_signals[UNREALIZE], 0);
      g_assert (!priv->mapped);
      g_assert (!priv->realized);
    }

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

void
gtk_widget_get_surface_allocation (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
  GtkWidget *parent;
  graphene_rect_t bounds;

  /* Don't consider the parent == widget case here. */
  parent = _gtk_widget_get_parent (widget);
  while (parent && !GTK_IS_NATIVE (parent))
    parent = _gtk_widget_get_parent (parent);

  g_assert (GTK_IS_WINDOW (parent) || GTK_IS_POPOVER (parent));

  if (gtk_widget_compute_bounds (widget, parent, &bounds))
    {
      *allocation = (GtkAllocation){
        floorf (bounds.origin.x),
        floorf (bounds.origin.y),
        ceilf (bounds.size.width),
        ceilf (bounds.size.height)
      };
    }
  else
    {
      *allocation = (GtkAllocation) { 0, 0, 0, 0 };
    }
}

/**
 * gtk_widget_queue_draw:
 * @widget: a #GtkWidget
 *
 * Schedules this widget to be redrawn in paint phase of the
 * current or the next frame. This means @widget's GtkWidgetClass.snapshot()
 * implementation will be called.
 **/
void
gtk_widget_queue_draw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* Just return if the widget isn't mapped */
  if (!_gtk_widget_get_mapped (widget))
    return;

  for (; widget; widget = _gtk_widget_get_parent (widget))
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      if (priv->draw_needed)
        break;

      priv->draw_needed = TRUE;
      g_clear_pointer (&priv->render_node, gsk_render_node_unref);
      if (GTK_IS_NATIVE (widget) && _gtk_widget_get_realized (widget))
        gdk_surface_queue_expose (gtk_native_get_surface (GTK_NATIVE (widget)));
    }
}

static void
gtk_widget_set_alloc_needed (GtkWidget *widget);
/**
 * gtk_widget_queue_allocate:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 *
 * Flags the widget for a rerun of the GtkWidgetClass::size_allocate
 * function. Use this function instead of gtk_widget_queue_resize()
 * when the @widget's size request didn't change but it wants to
 * reposition its contents.
 *
 * An example user of this function is gtk_widget_set_halign().
 */
void
gtk_widget_queue_allocate (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (_gtk_widget_get_realized (widget))
    gtk_widget_queue_draw (widget);

  gtk_widget_set_alloc_needed (widget);
}

static inline gboolean
gtk_widget_get_resize_needed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->resize_needed;
}

/*
 * gtk_widget_queue_resize_internal:
 * @widget: a #GtkWidget
 * 
 * Queue a resize on a widget, and on all other widgets grouped with this widget.
 */
static void
gtk_widget_queue_resize_internal (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *groups, *l, *widgets;

  if (gtk_widget_get_resize_needed (widget))
    return;

  priv->resize_needed = TRUE;
  gtk_widget_set_alloc_needed (widget);

  groups = _gtk_widget_get_sizegroups (widget);

  for (l = groups; l; l = l->next)
  {
    for (widgets = gtk_size_group_get_widgets (l->data); widgets; widgets = widgets->next)
      {
        gtk_widget_queue_resize_internal (widgets->data);
      }
  }

  if (_gtk_widget_get_visible (widget))
    {
      GtkWidget *parent = _gtk_widget_get_parent (widget);
      if (parent)
        {
          if (GTK_IS_NATIVE (widget))
            gtk_widget_queue_allocate (parent);
          else
            gtk_widget_queue_resize_internal (parent);
        }
    }
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

  if (_gtk_widget_get_realized (widget))
    gtk_widget_queue_draw (widget);

  gtk_widget_queue_resize_internal (widget);
}

/**
 * gtk_widget_queue_resize_no_redraw:
 * @widget: a #GtkWidget
 *
 * This function works like gtk_widget_queue_resize(),
 * except that the widget is not invalidated.
 **/
void
gtk_widget_queue_resize_no_redraw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_queue_resize_internal (widget);
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
 * Returns: (nullable) (transfer none): a #GdkFrameClock,
 * or %NULL if widget is unrealized
 */
GdkFrameClock*
gtk_widget_get_frame_clock (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->realized)
    {
      return gdk_surface_get_frame_clock (priv->surface);
    }
  else
    {
      return NULL;
    }
}

static gint
get_number (GtkCssStyle *style,
            guint        property)
{
  double d = _gtk_css_number_value_get (gtk_css_style_get_value (style, property), 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static void
get_box_margin (GtkCssStyle *style,
                GtkBorder   *margin)
{
  margin->top = get_number (style, GTK_CSS_PROPERTY_MARGIN_TOP);
  margin->left = get_number (style, GTK_CSS_PROPERTY_MARGIN_LEFT);
  margin->bottom = get_number (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM);
  margin->right = get_number (style, GTK_CSS_PROPERTY_MARGIN_RIGHT);
}

static void
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH);
  border->left = get_number (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH);
  border->right = get_number (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH);
}

static void
get_box_padding (GtkCssStyle *style,
                 GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_PADDING_TOP);
  border->left = get_number (style, GTK_CSS_PROPERTY_PADDING_LEFT);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_PADDING_BOTTOM);
  border->right = get_number (style, GTK_CSS_PROPERTY_PADDING_RIGHT);
}

/**
 * gtk_widget_size_allocate:
 * @widget: a #GtkWidget
 * @allocation: position and size to be allocated to @widget
 * @baseline: The baseline of the child, or -1
 *
 * This is a simple form of gtk_widget_allocate() that takes the new position
 * of @widget as part of @allocation.
 */
void
gtk_widget_size_allocate (GtkWidget           *widget,
                          const GtkAllocation *allocation,
                          int                  baseline)
{
  GskTransform *transform;

  if (allocation->x || allocation->y)
    transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (allocation->x, allocation->y));
  else
    transform = NULL;

  gtk_widget_allocate (widget,
                       allocation->width,
                       allocation->height,
                       baseline,
                       transform);
}

/**
 * gtk_widget_allocate:
 * @widget: A #GtkWidget
 * @width: New width of @widget
 * @height: New height of @widget
 * @baseline: New baseline of @widget, or -1
 * @transform: (transfer full) (allow-none): Transformation to be applied to @widget
 *
 * This function is only used by #GtkWidget subclasses, to assign a size,
 * position and (optionally) baseline to their child widgets.
 *
 * In this function, the allocation and baseline may be adjusted. The given
 * allocation will be forced to be bigger than the widget's minimum size,
 * as well as at least 0×0 in size.
 *
 * For a version that does not take a transform, see gtk_widget_size_allocate()
 */
void
gtk_widget_allocate (GtkWidget    *widget,
                     int           width,
                     int           height,
                     int           baseline,
                     GskTransform *transform)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GdkRectangle adjusted;
  gboolean alloc_needed;
  gboolean size_changed;
  gboolean baseline_changed;
  gboolean transform_changed;
  gint natural_width, natural_height, dummy = 0;
  gint min_width, min_height;
  GtkCssStyle *style;
  GtkBorder margin, border, padding;
  GskTransform *css_transform;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (baseline >= -1);

  gtk_widget_push_verify_invariants (widget);

  if (!priv->visible && !GTK_IS_ROOT (widget))
    goto out;

#ifdef G_ENABLE_DEBUG
  if (GTK_DISPLAY_DEBUG_CHECK (_gtk_widget_get_display (widget), RESIZE))
    {
      priv->highlight_resize = TRUE;
      gtk_widget_queue_draw (widget);
    }

  if (gtk_widget_get_resize_needed (widget))
    {
      g_warning ("Allocating size to %s %p without calling gtk_widget_measure(). "
                 "How does the code know the size to allocate?",
                 gtk_widget_get_name (widget), widget);
    }
#endif /* G_ENABLE_DEBUG */

  alloc_needed = priv->alloc_needed;
  /* Preserve request/allocate ordering */
  priv->alloc_needed = FALSE;

  baseline_changed = priv->allocated_size_baseline != baseline;
  size_changed = (priv->allocated_width != width ||
                  priv->allocated_height != height);
  transform_changed = !gsk_transform_equal (priv->allocated_transform, transform);

  gsk_transform_unref (priv->allocated_transform);
  priv->allocated_transform = gsk_transform_ref (transform);
  priv->allocated_width = width;
  priv->allocated_height = height;
  priv->allocated_size_baseline = baseline;

  adjusted = (GdkRectangle) { 0, 0, width, height };
  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      /* Go ahead and request the height for allocated width, note that the internals
       * of get_height_for_width will internally limit the for_size to natural size
       * when aligning implicitly.
       */
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          &min_width, &natural_width, NULL, NULL);
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, width,
                          &min_height, &natural_height, NULL, NULL);
    }
  else
    {
      /* Go ahead and request the width for allocated height, note that the internals
       * of get_width_for_height will internally limit the for_size to natural size
       * when aligning implicitly.
       */
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1,
                          &min_height, &natural_height, NULL, NULL);
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, height,
                          &min_width, &natural_width, NULL, NULL);
    }

#ifdef G_ENABLE_CONSISTENCY_CHECKS
  if ((min_width > width || min_height > height) &&
      !GTK_IS_SCROLLABLE (widget))
    g_warning ("gtk_widget_size_allocate(): attempt to underallocate %s%s %s %p. "
               "Allocation is %dx%d, but minimum required size is %dx%d.",
               priv->parent ? G_OBJECT_TYPE_NAME (priv->parent) : "", priv->parent ? "'s child" : "toplevel",
               G_OBJECT_TYPE_NAME (widget), widget,
               width, height,
               min_width, min_height);
#endif
  /* Now that we have the right natural height and width, go ahead and remove any margins from the
   * allocated sizes and possibly limit them to the natural sizes */
  gtk_widget_adjust_size_allocation (widget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     &dummy,
                                     &natural_width,
                                     &adjusted.x,
                                     &adjusted.width);
  gtk_widget_adjust_size_allocation (widget,
                                     GTK_ORIENTATION_VERTICAL,
                                     &dummy,
                                     &natural_height,
                                     &adjusted.y,
                                     &adjusted.height);
  if (baseline >= 0)
    baseline -= priv->margin.top;

  if (adjusted.width < 0 || adjusted.height < 0)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate %s %s %p with width %d and height %d",
                 G_OBJECT_TYPE_NAME (widget), gtk_css_node_get_name (priv->cssnode), widget,
                 adjusted.width,
                 adjusted.height);

      adjusted.width = 0;
      adjusted.height = 0;
    }

  if (G_UNLIKELY (GTK_IS_NATIVE (widget)))
    {
      adjusted.width = MAX (1, adjusted.width);
      adjusted.height = MAX (1, adjusted.height);
    }

  style = gtk_css_node_get_style (priv->cssnode);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  /* Apply CSS transformation */
  adjusted.x += margin.left;
  adjusted.y += margin.top;
  adjusted.width -= margin.left + margin.right;
  adjusted.height -= margin.top + margin.bottom;
  css_transform = gtk_css_transform_value_get_transform (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TRANSFORM));

  if (css_transform)
    {
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (adjusted.x, adjusted.y));
      adjusted.x = adjusted.y = 0;

      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (adjusted.width / 2, adjusted.height / 2));
      transform = gsk_transform_transform (transform, css_transform);
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- adjusted.width / 2, - adjusted.height / 2));
    }

  adjusted.x += border.left + padding.left;
  adjusted.y += border.top + padding.top;

  /* Since gtk_widget_measure does it for us, we can be sure here that
   * the given alloaction is large enough for the css margin/bordder/padding */
  adjusted.width -= border.left + padding.left +
                    border.right + padding.right;
  adjusted.height -= border.top + padding.top +
                     border.bottom + padding.bottom;
  if (baseline >= 0)
    baseline -= margin.top + border.top + padding.top;
  if (adjusted.x || adjusted.y)
    transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (adjusted.x, adjusted.y));

  priv->transform = transform;

  if (priv->surface_transform_data)
    sync_widget_surface_transform (widget);

  if (!alloc_needed && !size_changed && !baseline_changed)
    goto skip_allocate;

  priv->width = adjusted.width;
  priv->height = adjusted.height;
  priv->baseline = baseline;

  if (priv->layout_manager != NULL)
    {
      gtk_layout_manager_allocate (priv->layout_manager, widget,
                                   priv->width,
                                   priv->height,
                                   baseline);
    }
  else
    {
      if (g_signal_has_handler_pending (widget, widget_signals[SIZE_ALLOCATE], 0, FALSE))
        g_signal_emit (widget, widget_signals[SIZE_ALLOCATE], 0,
                       priv->width,
                       priv->height,
                       baseline);
      else
        GTK_WIDGET_GET_CLASS (widget)->size_allocate (widget,
                                                      priv->width,
                                                      priv->height,
                                                      baseline);
    }

  /* Size allocation is god... after consulting god, no further requests or allocations are needed */
#ifdef G_ENABLE_DEBUG
  if (GTK_DISPLAY_DEBUG_CHECK (_gtk_widget_get_display (widget), GEOMETRY) &&
      gtk_widget_get_resize_needed (widget))
    {
      g_warning ("%s %p or a child called gtk_widget_queue_resize() during size_allocate().",
                 gtk_widget_get_name (widget), widget);
    }
#endif

  gtk_widget_ensure_resize (widget);
  priv->alloc_needed = FALSE;
  priv->alloc_needed_on_child = FALSE;

  gtk_widget_update_paintables (widget);

skip_allocate:
  if (size_changed || baseline_changed)
    gtk_widget_queue_draw (widget);
  else if (transform_changed && priv->parent)
    gtk_widget_queue_draw (priv->parent);

out:
  if (priv->alloc_needed_on_child)
    gtk_widget_ensure_allocate (widget);

  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_common_ancestor:
 * @widget_a: a #GtkWidget
 * @widget_b: a #GtkWidget
 *
 * Find the common ancestor of @widget_a and @widget_b that
 * is closest to the two widgets.
 *
 * Returns: (nullable): the closest common ancestor of @widget_a and
 *   @widget_b or %NULL if @widget_a and @widget_b do not
 *   share a common ancestor.
 **/
GtkWidget *
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
 * @dest_x: (out) (optional): location to store X position relative to @dest_widget
 * @dest_y: (out) (optional): location to store Y position relative to @dest_widget
 *
 * Translate coordinates relative to @src_widget’s allocation to coordinates
 * relative to @dest_widget’s allocations. In order to perform this
 * operation, both widget must share a common toplevel.
 *
 * Returns: %FALSE if @src_widget and @dest_widget have no common
 *   ancestor. In this case, 0 is stored in
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
  graphene_point_t p;

  if (!gtk_widget_compute_point (src_widget, dest_widget,
                                 &GRAPHENE_POINT_INIT (src_x, src_y),
                                 &p))
    return FALSE;

  if (dest_x)
    *dest_x = p.x;

  if (dest_y)
    *dest_y = p.y;

  return TRUE;
}

/**
 * gtk_widget_compute_point:
 * @widget: the #GtkWidget to query
 * @target: the #GtkWidget to transform into
 * @point: a point in @widget's coordinate system
 * @out_point: (out caller-allocates): Set to the corresponding coordinates in
 *     @target's coordinate system
 *
 * Translates the given @point in @widget's coordinates to coordinates
 * relative to @target’s coodinate system. In order to perform this
 * operation, both widgets must share a common root.
 *
 * Returns: %TRUE if the point could be determined, %FALSE on failure.
 *   In this case, 0 is stored in @out_point.
 **/
gboolean
gtk_widget_compute_point (GtkWidget              *widget,
                          GtkWidget              *target,
                          const graphene_point_t *point,
                          graphene_point_t       *out_point)
{
  graphene_matrix_t transform;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (target), FALSE);

  if (!gtk_widget_compute_transform (widget, target, &transform))
    {
      graphene_point_init (out_point, 0, 0);
      return FALSE;
    }

  graphene_matrix_transform_point (&transform, point, out_point);

  return TRUE;
}

static void
gtk_widget_real_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
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
    case GTK_ALIGN_FILL:
    case GTK_ALIGN_CENTER:
    case GTK_ALIGN_BASELINE:
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
    default:
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

void
gtk_widget_adjust_size_allocation (GtkWidget         *widget,
                                   GtkOrientation     orientation,
                                   gint              *minimum_size,
                                   gint              *natural_size,
                                   gint              *allocated_pos,
                                   gint              *allocated_size)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      adjust_for_margin (priv->margin.left,
                         priv->margin.right,
                         minimum_size, natural_size,
                         allocated_pos, allocated_size);
      adjust_for_align (effective_align (priv->halign, _gtk_widget_get_direction (widget)),
                        natural_size, allocated_pos, allocated_size);
    }
  else
    {
      adjust_for_margin (priv->margin.top,
                         priv->margin.bottom,
                         minimum_size, natural_size,
                         allocated_pos, allocated_size);
      adjust_for_align (effective_align (priv->valign, GTK_TEXT_DIR_NONE),
                        natural_size, allocated_pos, allocated_size);
    }
}

/**
 * gtk_widget_class_add_binding: (skip)
 * @widget_class: the class to add the binding to
 * @keyval: key value of binding to install
 * @mods: key modifier of binding to install
 * @callback: the callback to call upon activation
 * @format_string: GVariant format string for arguments or %NULL for
 *     no arguments
 * @...: arguments, as given by format string.
 *
 * Creates a new shortcut for @widget_class that calls the given @callback
 * with arguments read according to @format_string.
 * The arguments and format string must be provided in the same way as
 * with g_variant_new().
 *
 * This function is a convenience wrapper around
 * gtk_widget_class_add_shortcut() and must be called during class
 * initialization. It does not provide for user_data, if you need that,
 * you will have to use gtk_widget_class_add_shortcut() with a custom
 * shortcut.
 **/
void
gtk_widget_class_add_binding (GtkWidgetClass  *widget_class,
                              guint            keyval,
                              GdkModifierType  mods,
                              GtkShortcutFunc  func,
                              const gchar     *format_string,
                              ...)
{
  GtkShortcut *shortcut;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

  shortcut = gtk_shortcut_new ();
  gtk_shortcut_set_trigger (shortcut, gtk_keyval_trigger_new (keyval, mods));
  gtk_shortcut_set_callback (shortcut, func, NULL, NULL);
  if (format_string)
    {
      va_list args;
      va_start (args, format_string);
      gtk_shortcut_set_arguments (shortcut,
                                  g_variant_new_va (format_string, NULL, &args));
      va_end (args);
    }

  gtk_widget_class_add_shortcut (widget_class, shortcut);

  g_object_unref (shortcut);
}

/**
 * gtk_widget_class_add_binding_signal: (skip)
 * @widget_class: the class to add the binding to
 * @keyval: key value of binding to install
 * @mods: key modifier of binding to install
 * @signal: the signal to execute
 * @format_string: GVariant format string for arguments or %NULL for
 *     no arguments
 * @...: arguments, as given by format string.
 *
 * Creates a new shortcut for @widget_class that emits the given action
 * @signal with arguments read according to @format_string.
 * The arguments and format string must be provided in the same way as
 * with g_variant_new().
 *
 * This function is a convenience wrapper around
 * gtk_widget_class_add_shortcut() and must be called during class
 * initialization.
 **/
void
gtk_widget_class_add_binding_signal (GtkWidgetClass  *widget_class,
                                     guint            keyval,
                                     GdkModifierType  mods,
                                     const gchar     *signal,
                                     const gchar     *format_string,
                                     ...)
{
  GtkShortcut *shortcut;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (g_signal_lookup (signal, G_TYPE_FROM_CLASS (widget_class)));
  /* XXX: validate variant format for signal */

  shortcut = gtk_shortcut_new ();
  gtk_shortcut_set_trigger (shortcut, gtk_keyval_trigger_new (keyval, mods));
  gtk_shortcut_set_signal (shortcut, signal);
  if (format_string)
    {
      va_list args;
      va_start (args, format_string);
      gtk_shortcut_set_arguments (shortcut,
                                  g_variant_new_va (format_string, NULL, &args));
      va_end (args);
    }

  gtk_widget_class_add_shortcut (widget_class, shortcut);

  g_object_unref (shortcut);
}

/**
 * gtk_widget_class_add_shortcut:
 * @widget_class: the class to add the shortcut to
 * @shortcut: (transfer none): the #GtkShortcut to add
 *
 * Installs a shortcut in @widget_class. Every instance created for
 * @widget_class or its subclasses will inherit this shortcut and
 * trigger it.
 *
 * Shortcuts added this way will be triggered in the @GTK_PHASE_BUBBLE
 * phase, which means they may also trigger if child widgets have focus.
 *
 * This function must only be used in class initialization functions
 * otherwise it is not guaranteed that the shortcut will be installed.
 **/
void
gtk_widget_class_add_shortcut (GtkWidgetClass *widget_class,
                               GtkShortcut    *shortcut)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (GTK_IS_SHORTCUT (shortcut));

  priv = widget_class->priv;

  priv->shortcuts = g_slist_prepend (priv->shortcuts, g_object_ref (shortcut));
}

const GSList *
gtk_widget_class_get_shortcuts (GtkWidgetClass *widget_class)
{
  return widget_class->priv->shortcuts;
}

static gboolean
gtk_widget_real_can_activate_accel (GtkWidget *widget,
                                    guint      signal_id)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  /* widgets must be onscreen for accels to take effect */
  return gtk_widget_is_sensitive (widget) &&
         _gtk_widget_is_drawable (widget) &&
         gdk_surface_is_viewable (priv->surface);
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
      g_warning (G_STRLOC ": widget '%s' has no activatable signal \"%s\" without arguments",
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
 * be used by a menu creation system.
 *
 * If you only want to
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
 * @group_cycling: %TRUE if there are other widgets with the same mnemonic
 *
 * Emits the #GtkWidget::mnemonic-activate signal.
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
      g_warning ("widget '%s' isn't suitable for mnemonic activation",
		 G_OBJECT_TYPE_NAME (widget));
      gtk_widget_error_bell (widget);
    }
  return TRUE;
}

#define WIDGET_REALIZED_FOR_EVENT(widget, event) \
     (event->any.type == GDK_FOCUS_CHANGE || _gtk_widget_get_realized(widget))

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
 * it were in the event queue.
 *
 * Returns: return from the event signal emission (%TRUE if
 *               the event was handled)
 **/
gboolean
gtk_widget_event (GtkWidget       *widget,
		  const GdkEvent  *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  return gtk_widget_event_internal (widget, event);
}

void
_gtk_widget_set_captured_event_handler (GtkWidget               *widget,
                                        GtkCapturedEventHandler  callback)
{
  g_object_set_data (G_OBJECT (widget), I_("captured-event-handler"), callback);
}

gboolean
gtk_widget_run_controllers (GtkWidget           *widget,
			    const GdkEvent      *event,
			    GtkPropagationPhase  phase)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkEventController *controller;
  gboolean handled = FALSE;
  GList *l;

  g_object_ref (widget);

  l = priv->event_controllers;
  while (l != NULL)
    {
      GList *next = l->next;

      if (!WIDGET_REALIZED_FOR_EVENT (widget, event))
        break;

      controller = l->data;

      if (controller == NULL)
        {
          priv->event_controllers = g_list_delete_link (priv->event_controllers, l);
        }
      else
        {
          GtkPropagationPhase controller_phase;

          controller_phase = gtk_event_controller_get_propagation_phase (controller);

          if (controller_phase == phase)
            handled |= gtk_event_controller_handle_event (controller, event);

          /* Non-gesture controllers are basically unique entities not meant
           * to collaborate with anything else. Break early if any such event
           * controller handled the event.
           */
          if (handled && !GTK_IS_GESTURE (controller))
            break;
        }

      l = next;
    }

  g_object_unref (widget);

  return handled;
}

static gboolean
translate_event_coordinates (GdkEvent  *event,
                             GtkWidget *widget);
gboolean
_gtk_widget_captured_event (GtkWidget      *widget,
                            const GdkEvent *event)
{
  gboolean return_val = FALSE;
  GtkCapturedEventHandler handler;
  GdkEvent *event_copy;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  if (!event_surface_is_still_viewable (event))
    return TRUE;

  event_copy = gdk_event_copy (event);
  translate_event_coordinates (event_copy, widget);

  return_val = gtk_widget_run_controllers (widget, event_copy, GTK_PHASE_CAPTURE);

  handler = g_object_get_data (G_OBJECT (widget), I_("captured-event-handler"));
  if (!handler)
    goto out;

  g_object_ref (widget);

  return_val |= handler (widget, event_copy);
  return_val |= !WIDGET_REALIZED_FOR_EVENT (widget, event_copy);

  g_object_unref (widget);

out:
  g_object_unref (event_copy);

  return return_val;
}

static gboolean
event_surface_is_still_viewable (const GdkEvent *event)
{
  /* Check that we think the event's window is viewable before
   * delivering the event, to prevent surprises. We do this here
   * at the last moment, since the event may have been queued
   * up behind other events, held over a recursive main loop, etc.
   */
  switch ((guint) event->any.type)
    {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_KEY_PRESS:
    case GDK_ENTER_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_SCROLL:
      return event->any.surface && gdk_surface_is_viewable (event->any.surface);

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

static gboolean
translate_event_coordinates (GdkEvent  *event,
                             GtkWidget *widget)
{
  GtkWidget *event_widget;
  double x, y;
  graphene_point_t p;

  if (!gdk_event_get_coords (event, &x, &y))
    return TRUE;

  event_widget = gtk_get_event_widget (event);

  if (!gtk_widget_compute_point (event_widget,
                                 widget,
                                 &GRAPHENE_POINT_INIT (x, y),
                                 &p))
    return FALSE;

  gdk_event_set_coords (event, p.x, p.y);

  return TRUE;
}

static gboolean
gtk_widget_event_internal (GtkWidget      *widget,
                           const GdkEvent *event)
{
  gboolean return_val = FALSE;
  GdkEvent *event_copy;

  /* We check only once for is-still-visible; if someone
   * hides the window in on of the signals on the widget,
   * they are responsible for returning TRUE to terminate
   * handling.
   */
  if (!event_surface_is_still_viewable (event))
    return TRUE;

  if (!_gtk_widget_get_mapped (widget))
    return FALSE;

  event_copy = gdk_event_copy (event);

  translate_event_coordinates (event_copy, widget);

  if (widget == gtk_get_event_target (event_copy))
    return_val |= gtk_widget_run_controllers (widget, event_copy, GTK_PHASE_TARGET);

  if (return_val == FALSE)
    return_val |= gtk_widget_run_controllers (widget, event_copy, GTK_PHASE_BUBBLE);
  g_object_unref (event_copy);

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

  if (GTK_WIDGET_GET_CLASS (widget)->activate_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      g_signal_emit (widget, GTK_WIDGET_GET_CLASS (widget)->activate_signal, 0);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * _gtk_widget_grab_notify:
 * @widget: a #GtkWidget
 * @was_grabbed: whether a grab is now in effect
 *
 * Emits the #GtkWidget::grab-notify signal on @widget.
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
 * Causes @widget (or one of its descendents) to have the keyboard focus
 * for the #GtkWindow it's inside.
 *
 * @widget must be focusable, or have a ::grab_focus implementation that
 * transfers the focus to a descendant of @widget that is focusable.
 **/
void
gtk_widget_grab_focus (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GTK_WIDGET_GET_CLASS (widget)->grab_focus (widget);
}

static void
gtk_widget_real_grab_focus (GtkWidget *focus_widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (focus_widget);
  if (priv->root)
    gtk_root_set_focus (priv->root, focus_widget);
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
  has_tooltip = gtk_widget_get_has_tooltip (widget);

  if (has_tooltip && tooltip_markup)
    {
      gtk_tooltip_set_markup (tooltip, tooltip_markup);
      return TRUE;
    }

  return FALSE;
}

gboolean
gtk_widget_query_tooltip (GtkWidget  *widget,
                          gint        x,
                          gint        y,
                          gboolean    keyboard_mode,
                          GtkTooltip *tooltip)
{
  gboolean retval = FALSE;

  g_signal_emit (widget,
                 widget_signals[QUERY_TOOLTIP],
                 0,
                 x, y,
                 keyboard_mode,
                 tooltip,
                 &retval);

  return retval;
}

static void
gtk_widget_real_state_flags_changed (GtkWidget     *widget,
                                     GtkStateFlags  old_state)
{
}

static void
gtk_widget_real_style_updated (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssStyleChange *change = NULL;

  gtk_widget_update_alpha (widget);

  if (priv->context)
    change = gtk_style_context_get_change (priv->context);

  if (change)
    {
      const gboolean has_text = gtk_widget_peek_pango_context (widget) != NULL;

      if (has_text && gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT))
        gtk_widget_update_pango_context (widget);

      if (priv->root)
        {
          if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE) ||
              (has_text && gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_SIZE)))
            {
              gtk_widget_queue_resize (widget);
            }
          else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TRANSFORM))
            {
                gtk_widget_queue_allocate (widget);
            }
          else if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_REDRAW) ||
                   (has_text && gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_CONTENT)))
            {
              gtk_widget_queue_draw (widget);
            }
        }
    }
  else
    {
      gtk_widget_update_pango_context (widget);

      if (priv->root)
        gtk_widget_queue_resize (widget);
    }
}

static gboolean
direction_is_forward (GtkDirectionType direction)
{
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_RIGHT:
    case GTK_DIR_DOWN:
      return TRUE;
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_LEFT:
    case GTK_DIR_UP:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
gtk_widget_real_focus (GtkWidget         *widget,
                       GtkDirectionType   direction)
{
  GtkWidget *focus;

  /* The easy case: not focusable. Just try the children */
  if (!gtk_widget_get_can_focus (widget))
    {
      if (gtk_widget_focus_move (widget, direction))
        return TRUE;

      return FALSE;
    }

  /* For focusable widgets, we want to focus the widget
   * before its children. We differentiate 3 cases:
   * 1) focus is currently on widget
   * 2) focus is on some child
   * 3) focus is outside
   */

  if (gtk_widget_is_focus (widget))
    {
      if (direction_is_forward (direction) &&
          gtk_widget_focus_move (widget, direction))
        return TRUE;

      return FALSE;
    }

  focus = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_root (widget)));

  if (focus && gtk_widget_is_ancestor (focus, widget))
    {
      if (gtk_widget_focus_move (widget, direction))
        return TRUE;

      if (direction_is_forward (direction))
        return FALSE;
      else
        {
          gtk_widget_grab_focus (widget);
          return TRUE;
        }
    }

  if (!direction_is_forward (direction))
    {
      if (gtk_widget_focus_move (widget, direction))
        return TRUE;
    }

  gtk_widget_grab_focus (widget);
  return TRUE;
}

static void
gtk_widget_real_move_focus (GtkWidget         *widget,
                            GtkDirectionType   direction)
{
  GtkRoot *root;

  root = _gtk_widget_get_root (widget);
  if (widget != GTK_WIDGET (root))
    g_signal_emit (root, widget_signals[MOVE_FOCUS], 0, direction);
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
    default:
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
 * Specifies whether @widget can own the input focus.
 * 
 * Note that having @can_focus be %TRUE is only one of the
 * necessary conditions for being focusable. A widget must
 * also be sensitive and not have a ancestor that is marked
 * as not child-focusable in order to receive input focus.
 *
 * See gtk_widget_grab_focus() for actually setting the input
 * focus on a widget.
 **/
void
gtk_widget_set_can_focus (GtkWidget *widget,
                          gboolean   can_focus)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->can_focus != can_focus)
    {
      priv->can_focus = can_focus;

      gtk_widget_queue_resize (widget);
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CAN_FOCUS]);
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
 **/
gboolean
gtk_widget_get_can_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->can_focus;
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
 **/
gboolean
gtk_widget_has_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_focus;
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
 */
gboolean
gtk_widget_has_visible_focus (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean draw_focus;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (priv->has_focus)
    {
      GtkRoot *root = _gtk_widget_get_root (widget);

      if (GTK_IS_WINDOW (root))
        draw_focus = gtk_window_get_focus_visible (GTK_WINDOW (root));
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (priv->root)
    return widget == gtk_root_get_focus (priv->root);

  return FALSE;
}

/**
 * gtk_widget_set_focus_on_click:
 * @widget: a #GtkWidget
 * @focus_on_click: whether the widget should grab focus when clicked with the mouse
 *
 * Sets whether the widget should grab focus when it is clicked with the mouse.
 * Making mouse clicks not grab focus is useful in places like toolbars where
 * you don’t want the keyboard focus removed from the main area of the
 * application.
 **/
void
gtk_widget_set_focus_on_click (GtkWidget *widget,
			       gboolean   focus_on_click)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  focus_on_click = focus_on_click != FALSE;

  if (priv->focus_on_click != focus_on_click)
    {
      priv->focus_on_click = focus_on_click;

      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_FOCUS_ON_CLICK]);
    }
}

/**
 * gtk_widget_get_focus_on_click:
 * @widget: a #GtkWidget
 *
 * Returns whether the widget should grab focus when it is clicked with the mouse.
 * See gtk_widget_set_focus_on_click().
 *
 * Returns: %TRUE if the widget should grab focus when it is clicked with
 *               the mouse.
 **/
gboolean
gtk_widget_get_focus_on_click (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->focus_on_click;
}

/**
 * gtk_widget_has_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is the current default widget within its
 * toplevel.
 *
 * Returns: %TRUE if @widget is the current default widget within
 *     its toplevel, %FALSE otherwise
 */
gboolean
gtk_widget_has_default (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_default;
}

void
_gtk_widget_set_has_default (GtkWidget *widget,
                             gboolean   has_default)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkStyleContext *context;

  priv->has_default = has_default;

  context = _gtk_widget_get_style_context (widget);

  if (has_default)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_DEFAULT);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_DEFAULT);
}

/**
 * gtk_widget_set_receives_default:
 * @widget: a #GtkWidget
 * @receives_default: whether or not @widget can be a default widget.
 *
 * Specifies whether @widget will be treated as the default
 * widget within its toplevel when it has the focus, even if
 * another widget is the default.
 **/
void
gtk_widget_set_receives_default (GtkWidget *widget,
                                 gboolean   receives_default)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->receives_default != receives_default)
    {
      priv->receives_default = receives_default;

      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_RECEIVES_DEFAULT]);
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
 * Returns: %TRUE if @widget acts as the default widget when focused,
 *               %FALSE otherwise
 **/
gboolean
gtk_widget_get_receives_default (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->receives_default;
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
 **/
gboolean
gtk_widget_has_grab (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_grab;
}

void
_gtk_widget_set_has_grab (GtkWidget *widget,
                          gboolean   has_grab)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->has_grab = has_grab;
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
 **/
gboolean
gtk_widget_device_is_shadowed (GtkWidget *widget,
                               GdkDevice *device)
{
  GtkWindowGroup *group;
  GtkWidget *grab_widget;
  GtkRoot *root;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (!_gtk_widget_get_realized (widget))
    return TRUE;

  root = _gtk_widget_get_root (widget);

  if (GTK_IS_WINDOW (root))
    group = gtk_window_get_group (GTK_WINDOW (root));
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_free (priv->name);
  priv->name = g_strdup (name);

  if (priv->context)
    gtk_style_context_set_id (priv->context, priv->name);

  gtk_css_node_set_id (priv->cssnode, priv->name);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_NAME]);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->name)
    return priv->name;
  return G_OBJECT_TYPE_NAME (widget);
}

static void
gtk_widget_update_state_flags (GtkWidget     *widget,
                               GtkStateFlags  flags_to_set,
                               GtkStateFlags  flags_to_unset)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

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

      data.old_scale_factor = gtk_widget_get_scale_factor (widget);
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
 **/
void
gtk_widget_set_state_flags (GtkWidget     *widget,
                            GtkStateFlags  flags,
                            gboolean       clear)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

#define ALLOWED_FLAGS (~(GTK_STATE_FLAG_DIR_LTR | GTK_STATE_FLAG_DIR_RTL))

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((!clear && (priv->state_flags & flags) == flags) ||
      (clear && priv->state_flags == flags))
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
 **/
void
gtk_widget_unset_state_flags (GtkWidget     *widget,
                              GtkStateFlags  flags)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((priv->state_flags & flags) == 0)
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
 * Also note that if you are looking for a way to obtain the
 * #GtkStateFlags to pass to a #GtkStyleContext method, you
 * should look at gtk_style_context_get_state().
 *
 * Returns: The state flags for widget
 **/
GtkStateFlags
gtk_widget_get_state_flags (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->state_flags;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->visible = visible;

  if (!visible)
    {
      g_clear_pointer (&priv->allocated_transform, gsk_transform_unref);
      priv->allocated_width = 0;
      priv->allocated_height = 0;
      priv->allocated_size_baseline = 0;
      g_clear_pointer (&priv->transform, gsk_transform_unref);
      priv->width = 0;
      priv->height = 0;
      gtk_widget_update_paintables (widget);
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
 **/
gboolean
gtk_widget_get_visible (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->visible;
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
 **/
gboolean
gtk_widget_is_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  while (widget)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      if (!priv->visible)
        return FALSE;

      widget = priv->parent;
    }

  return TRUE;
}

/**
 * gtk_widget_is_drawable:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can be drawn to. A widget can be drawn
 * if it is mapped and visible.
 *
 * Returns: %TRUE if @widget is drawable, %FALSE otherwise
 **/
gboolean
gtk_widget_is_drawable (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (_gtk_widget_get_visible (widget) &&
          _gtk_widget_get_mapped (widget));
}

/**
 * gtk_widget_get_realized:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is realized.
 *
 * Returns: %TRUE if @widget is realized, %FALSE otherwise
 **/
gboolean
gtk_widget_get_realized (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->realized;
}

/**
 * gtk_widget_get_mapped:
 * @widget: a #GtkWidget
 *
 * Whether the widget is mapped.
 *
 * Returns: %TRUE if the widget is mapped, %FALSE otherwise.
 */
gboolean
gtk_widget_get_mapped (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->mapped;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  sensitive = (sensitive != FALSE);

  if (priv->sensitive == sensitive)
    return;

  priv->sensitive = sensitive;

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;

      gtk_event_controller_reset (controller);
    }

  if (priv->parent == NULL
      || gtk_widget_is_sensitive (priv->parent))
    {
      GtkStateData data;

      data.old_scale_factor = gtk_widget_get_scale_factor (widget);

      if (sensitive)
        {
          data.flags_to_set = 0;
          data.flags_to_unset = GTK_STATE_FLAG_INSENSITIVE;
        }
      else
        {
          data.flags_to_set = GTK_STATE_FLAG_INSENSITIVE;
          data.flags_to_unset = GTK_STATE_FLAG_PRELIGHT |
                                GTK_STATE_FLAG_ACTIVE;
        }

      gtk_widget_propagate_state (widget, &data);
      update_cursor_on_state_change (widget);
    }

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_SENSITIVE]);
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
 */
gboolean
gtk_widget_get_sensitive (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->sensitive;
}

/**
 * gtk_widget_is_sensitive:
 * @widget: a #GtkWidget
 *
 * Returns the widget’s effective sensitivity, which means
 * it is sensitive itself and also its parent widget is sensitive
 *
 * Returns: %TRUE if the widget is effectively sensitive
 */
gboolean
gtk_widget_is_sensitive (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return _gtk_widget_is_sensitive (widget);
}


/* Insert @widget into the children list of @parent,
 * after @previous_sibling */
static void
gtk_widget_reposition_after (GtkWidget *widget,
                             GtkWidget *parent,
                             GtkWidget *previous_sibling)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkStateFlags parent_flags;
  GtkWidget *prev_parent, *prev_previous;
  GtkStateData data;

  prev_parent = priv->parent;
  prev_previous = priv->prev_sibling;

  if (priv->parent != NULL && priv->parent != parent)
    {
      g_warning ("Can't set new parent %s %p on widget %s %p, "
                 "which already has parent %s %p",
                 gtk_widget_get_name (parent), (void *)parent,
                 gtk_widget_get_name (widget), (void *)widget,
                 gtk_widget_get_name (priv->parent), (void *)priv->parent);
      return;
    }

  data.old_scale_factor = gtk_widget_get_scale_factor (widget);

  /* keep this function in sync with gtk_menu_attach_to_widget()
   */

  g_object_ref_sink (widget);

  gtk_widget_push_verify_invariants (widget);

  priv->parent = parent;

  if (previous_sibling)
    {
      if (previous_sibling->priv->next_sibling)
        previous_sibling->priv->next_sibling->priv->prev_sibling = widget;

      if (priv->prev_sibling)
        priv->prev_sibling->priv->next_sibling = priv->next_sibling;

      if (priv->next_sibling)
        priv->next_sibling->priv->prev_sibling = priv->prev_sibling;


      if (parent->priv->first_child == widget)
        parent->priv->first_child = priv->next_sibling;

      if (parent->priv->last_child == widget)
        parent->priv->last_child = priv->prev_sibling;

      priv->prev_sibling = previous_sibling;
      priv->next_sibling = previous_sibling->priv->next_sibling;
      previous_sibling->priv->next_sibling = widget;

      if (parent->priv->last_child == previous_sibling)
        parent->priv->last_child = widget;
      else if (parent->priv->last_child == widget)
        parent->priv->last_child = priv->next_sibling;
    }
  else
    {
      /* Beginning */
      if (parent->priv->last_child == widget)
        {
          parent->priv->last_child = priv->prev_sibling;
          if (priv->prev_sibling)
            priv->prev_sibling->priv->next_sibling = NULL;
        }
      if (priv->prev_sibling)
        priv->prev_sibling->priv->next_sibling = priv->next_sibling;

      if (priv->next_sibling)
        priv->next_sibling->priv->prev_sibling = priv->prev_sibling;

      priv->prev_sibling = NULL;
      priv->next_sibling = parent->priv->first_child;
      if (parent->priv->first_child)
        parent->priv->first_child->priv->prev_sibling = widget;

      parent->priv->first_child = widget;

      if (parent->priv->last_child == NULL)
        parent->priv->last_child = widget;
    }

  parent_flags = _gtk_widget_get_state_flags (parent);

  /* Merge both old state and current parent state,
   * making sure to only propagate the right states */
  data.flags_to_set = parent_flags & GTK_STATE_FLAGS_DO_SET_PROPAGATE;
  data.flags_to_unset = 0;
  gtk_widget_propagate_state (widget, &data);

  if (gtk_css_node_get_parent (priv->cssnode) == NULL)
    {
      gtk_css_node_insert_after (parent->priv->cssnode,
                                 priv->cssnode,
                                 previous_sibling ? previous_sibling->priv->cssnode : NULL);
    }

  if (priv->context)
    gtk_style_context_set_parent (priv->context,
                                  _gtk_widget_get_style_context (parent));

  _gtk_widget_update_parent_muxer (widget);

  if (parent->priv->children_observer)
    {
      if (prev_previous)
        g_warning ("oops");
      else
        gtk_list_list_model_item_added (parent->priv->children_observer, widget);
    }

  if (parent->priv->root && priv->root == NULL)
    gtk_widget_root (widget);

  if (prev_parent == NULL)
    g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_PARENT]);

  /* Enforce realized/mapped invariants
   */
  if (_gtk_widget_get_realized (priv->parent))
    gtk_widget_realize (widget);

  if (_gtk_widget_get_visible (priv->parent) &&
      _gtk_widget_get_visible (widget))
    {
      if (_gtk_widget_get_child_visible (widget) &&
          _gtk_widget_get_mapped (priv->parent))
        gtk_widget_map (widget);

      gtk_widget_queue_resize (priv->parent);
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
  if (_gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (parent);
    }

  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_set_parent:
 * @widget: a #GtkWidget
 * @parent: parent widget
 *
 * This function is useful only when implementing subclasses of
 * #GtkWidget.
 * Sets @parent as the parent widget of @widget, and takes care of
 * some details such as updating the state and style of the child
 * to reflect its new location and resizing the parent. The opposite
 * function is gtk_widget_unparent().
 **/
void
gtk_widget_set_parent (GtkWidget *widget,
                       GtkWidget *parent)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (_gtk_widget_get_parent (widget) == NULL);

  gtk_widget_reposition_after (widget,
                               parent,
                               _gtk_widget_get_last_child (parent));
}

/**
 * gtk_widget_get_parent:
 * @widget: a #GtkWidget
 *
 * Returns the parent widget of @widget.
 *
 * Returns: (transfer none) (nullable): the parent widget of @widget, or %NULL
 **/
GtkWidget *
gtk_widget_get_parent (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->parent;
}

/**
 * gtk_widget_get_root:
 * @widget: a #GtkWidget
 *
 * Returns the #GtkRoot widget of @widget or %NULL if the widget is not contained
 * inside a widget tree with a root widget.
 *
 * #GtkRoot widgets will return themselves here.
 *
 * Returns: (transfer none) (nullable): the root widget of @widget, or %NULL
 **/
GtkRoot *
gtk_widget_get_root (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return _gtk_widget_get_root (widget);
}

/**
 * gtk_widget_get_native:
 * @widget: a #GtkWidget
 *
 * Returns the GtkNative widget that contains @widget,
 * or %NULL if the widget is not contained inside a
 * widget tree with a native ancestor.
 *
 * #GtkNative widgets will return themselves here.
 *
 * Returns: (transfer none) (nullable): the #GtkNative
 *   widget of @widget, or %NULL
 */
GtkNative *
gtk_widget_get_native (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return GTK_NATIVE (gtk_widget_get_ancestor (widget, GTK_TYPE_NATIVE));
}

static void
gtk_widget_real_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  gtk_widget_queue_resize (widget);
}

static void
reset_style_recurse (GtkWidget *widget, gpointer user_data)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  gtk_css_node_invalidate (priv->cssnode, GTK_CSS_CHANGE_ANY);

  gtk_widget_forall (widget, reset_style_recurse, user_data);
}

/**
 * gtk_widget_reset_style:
 * @widget: a #GtkWidget
 *
 * Updates the style context of @widget and all descendants
 * by updating its widget path. #GtkContainers may want
 * to use this on a child when reordering it in a way that a different
 * style might apply to it. See also gtk_container_get_path_for_child().
 */
void
gtk_widget_reset_style (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  reset_style_recurse (widget, NULL);

  g_list_foreach (priv->attached_windows,
                  (GFunc) reset_style_recurse, NULL);
}

#ifdef G_ENABLE_CONSISTENCY_CHECKS

/* Verify invariants, see docs/widget_system.txt for notes on much of
 * this.  Invariants may be temporarily broken while we’re in the
 * process of updating state, of course, so you can only
 * verify_invariants() after a given operation is complete.
 * Use push/pop_verify_invariants to help with that.
 */
static void
gtk_widget_verify_invariants (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *parent;

  if (priv->verifying_invariants_count > 0)
    return;

  parent = priv->parent;

  if (priv->mapped)
    {
      /* Mapped implies ... */

      if (!priv->realized)
        g_warning ("%s %p is mapped but not realized",
                   gtk_widget_get_name (widget), widget);

      if (!priv->visible)
        g_warning ("%s %p is mapped but not visible",
                   gtk_widget_get_name (widget), widget);

      if (!GTK_IS_ROOT (widget))
        {
          if (!priv->child_visible)
            g_warning ("%s %p is mapped but not child_visible",
                       gtk_widget_get_name (widget), widget);
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
      else if (!GTK_IS_ROOT (widget))
        {
          /* No parent or parent not realized on non-toplevel implies... */

          if (priv->realized)
            g_warning ("%s %p is not realized but child %s %p is realized",
                       parent ? gtk_widget_get_name (parent) : "no parent", parent,
                       gtk_widget_get_name (widget), widget);
        }

      if (parent &&
          parent->priv->mapped &&
          priv->visible &&
          priv->child_visible)
        {
          /* Parent mapped and we are visible implies... */

          if (!priv->mapped)
            g_warning ("%s %p is mapped but visible child %s %p is not mapped",
                       gtk_widget_get_name (parent), parent,
                       gtk_widget_get_name (widget), widget);
        }
      else if (!GTK_IS_ROOT (widget))
        {
          /* No parent or parent not mapped on non-toplevel implies... */

          if (priv->mapped)
            g_warning ("%s %p is mapped but visible=%d child_visible=%d parent %s %p mapped=%d",
                       gtk_widget_get_name (widget), widget,
                       priv->visible,
                       priv->child_visible,
                       parent ? gtk_widget_get_name (parent) : "no parent", parent,
                       parent ? parent->priv->mapped : FALSE);
        }
    }

  if (!priv->realized)
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->verifying_invariants_count += 1;
}

static void
gtk_widget_verify_child_invariants (GtkWidget *widget)
{
  /* We don't recurse further; this is a one-level check. */
  gtk_widget_verify_invariants (widget);
}

static void
gtk_widget_pop_verify_invariants (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (priv->verifying_invariants_count > 0);

  priv->verifying_invariants_count -= 1;

  if (priv->verifying_invariants_count == 0)
    {
      GtkWidget *child;
      gtk_widget_verify_invariants (widget);

      /* Check one level of children, because our
       * push_verify_invariants() will have prevented some of the
       * checks. This does not recurse because if recursion is
       * needed, it will happen naturally as each child has a
       * push/pop on that child. For example if we're recursively
       * mapping children, we'll push/pop on each child as we map
       * it.
       */
      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          gtk_widget_verify_child_invariants (child);
        }
    }
}
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

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
 * by using the #GtkWidget::display-changed signal on the widget.
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

static PangoFontMap *
gtk_widget_get_effective_font_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  PangoFontMap *font_map;

  font_map = PANGO_FONT_MAP (g_object_get_qdata (G_OBJECT (widget), quark_font_map));
  if (font_map)
    return font_map;
  else if (priv->parent)
    return gtk_widget_get_effective_font_map (priv->parent);
  else
    return pango_cairo_font_map_get_default ();
}

static void
update_pango_context (GtkWidget    *widget,
                      PangoContext *context)
{
  PangoFontDescription *font_desc;
  GtkStyleContext *style_context;
  GtkSettings *settings;
  cairo_font_options_t *font_options;
  GtkCssValue *value;
  char *variations;

  style_context = _gtk_widget_get_style_context (widget);
  gtk_style_context_get (style_context,
                         "font", &font_desc,
                         NULL);

  value = _gtk_style_context_peek_property (_gtk_widget_get_style_context (widget), GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS);
  variations = gtk_css_font_variations_value_get_variations (value);

  pango_font_description_set_variations (font_desc, variations);

  pango_context_set_font_description (context, font_desc);

  pango_font_description_free (font_desc);
  g_free (variations);

  pango_context_set_base_dir (context,
			      _gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
			      PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);

  pango_cairo_context_set_resolution (context,
                                      _gtk_css_number_value_get (
                                          _gtk_style_context_peek_property (style_context,
                                                                            GTK_CSS_PROPERTY_DPI),
                                          100));

  settings = gtk_widget_get_settings (widget);
  font_options = (cairo_font_options_t*)g_object_get_qdata (G_OBJECT (widget), quark_font_options);
  if (settings && font_options)
    {
      cairo_font_options_t *options;

      options = cairo_font_options_copy (gtk_settings_get_font_options (settings));
      cairo_font_options_merge (options, font_options);
      pango_cairo_context_set_font_options (context, options);
      cairo_font_options_destroy (options);
    }
  else if (settings)
    {
      pango_cairo_context_set_font_options (context,
                                            gtk_settings_get_font_options (settings));
    }

  pango_context_set_font_map (context, gtk_widget_get_effective_font_map (widget));
}

static void
gtk_widget_update_pango_context (GtkWidget *widget)
{
  PangoContext *context = gtk_widget_peek_pango_context (widget);

  if (context)
    update_pango_context (widget, context);
}

/**
 * gtk_widget_set_font_options:
 * @widget: a #GtkWidget
 * @options: (allow-none): a #cairo_font_options_t, or %NULL to unset any
 *   previously set default font options.
 *
 * Sets the #cairo_font_options_t used for Pango rendering in this widget.
 * When not set, the default font options for the #GdkDisplay will be used.
 **/
void
gtk_widget_set_font_options (GtkWidget                  *widget,
                             const cairo_font_options_t *options)
{
  cairo_font_options_t *font_options;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  font_options = (cairo_font_options_t *)g_object_get_qdata (G_OBJECT (widget), quark_font_options);
  if (font_options != options)
    {
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_font_options,
                               options ? cairo_font_options_copy (options) : NULL,
                               (GDestroyNotify)cairo_font_options_destroy);

      gtk_widget_update_pango_context (widget);
    }
}

/**
 * gtk_widget_get_font_options:
 * @widget: a #GtkWidget
 *
 * Returns the #cairo_font_options_t used for Pango rendering. When not set,
 * the defaults font options for the #GdkDisplay will be used.
 *
 * Returns: (transfer none) (nullable): the #cairo_font_options_t or %NULL if not set
 **/
const cairo_font_options_t *
gtk_widget_get_font_options (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return (cairo_font_options_t *)g_object_get_qdata (G_OBJECT (widget), quark_font_options);
}

static void
gtk_widget_set_font_map_recurse (GtkWidget *widget, gpointer user_data)
{
  if (g_object_get_qdata (G_OBJECT (widget), quark_font_map))
    return;

  gtk_widget_update_pango_context (widget);

  gtk_widget_forall (widget, gtk_widget_set_font_map_recurse, user_data);
}

/**
 * gtk_widget_set_font_map:
 * @widget: a #GtkWidget
 * @font_map: (allow-none): a #PangoFontMap, or %NULL to unset any previously
 *     set font map
 *
 * Sets the font map to use for Pango rendering. When not set, the widget
 * will inherit the font map from its parent.
 */
void
gtk_widget_set_font_map (GtkWidget    *widget,
                         PangoFontMap *font_map)
{
  PangoFontMap *map;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  map = PANGO_FONT_MAP (g_object_get_qdata (G_OBJECT (widget), quark_font_map));
  if (map == font_map)
    return;

  g_object_set_qdata_full (G_OBJECT (widget),
                           quark_font_map,
                           g_object_ref (font_map),
                           g_object_unref);

  gtk_widget_update_pango_context (widget);

  gtk_widget_forall (widget, gtk_widget_set_font_map_recurse, NULL);
}

/**
 * gtk_widget_get_font_map:
 * @widget: a #GtkWidget
 *
 * Gets the font map that has been set with gtk_widget_set_font_map().
 *
 * Returns: (transfer none) (nullable): A #PangoFontMap, or %NULL
 */
PangoFontMap *
gtk_widget_get_font_map (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return PANGO_FONT_MAP (g_object_get_qdata (G_OBJECT (widget), quark_font_map));
}

/**
 * gtk_widget_create_pango_context:
 * @widget: a #GtkWidget
 *
 * Creates a new #PangoContext with the appropriate font map,
 * font options, font description, and base direction for drawing
 * text for this widget. See also gtk_widget_get_pango_context().
 *
 * Returns: (transfer full): the new #PangoContext
 **/
PangoContext *
gtk_widget_create_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
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
 * This can be tracked by using the #GtkWidget::display-changed signal
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
 * gtk_widget_set_child_visible:
 * @widget: a #GtkWidget
 * @child_visible: if %TRUE, @widget should be mapped along with its parent.
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
                              gboolean   child_visible)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!GTK_IS_ROOT (widget));

  child_visible = !!child_visible;

  if (priv->child_visible == child_visible)
    return;

  g_object_ref (widget);
  gtk_widget_verify_invariants (widget);

  if (child_visible)
    priv->child_visible = TRUE;
  else
    {
      GtkRoot *root;

      priv->child_visible = FALSE;

      root = _gtk_widget_get_root (widget);
      if (GTK_WIDGET (root) != widget && GTK_IS_WINDOW (root))
	_gtk_window_unset_focus_and_default (GTK_WINDOW (root), widget);
    }

  if (priv->parent && _gtk_widget_get_realized (priv->parent))
    {
      if (_gtk_widget_get_mapped (priv->parent) &&
	  priv->child_visible &&
	  _gtk_widget_get_visible (widget))
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->child_visible;
}

void
_gtk_widget_scale_changed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->context)
    gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_SCALE_FACTOR]);

  gtk_widget_queue_draw (widget);

  gtk_widget_forall (widget, (GtkCallback)_gtk_widget_scale_changed, NULL);
}

/**
 * gtk_widget_get_scale_factor:
 * @widget: a #GtkWidget
 *
 * Retrieves the internal scale factor that maps from window coordinates
 * to the actual device pixels. On traditional systems this is 1, on
 * high density outputs, it can be a higher value (typically 2).
 *
 * See gdk_surface_get_scale_factor().
 *
 * Returns: the scale factor for @widget
 */
gint
gtk_widget_get_scale_factor (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkRoot *root;
  GdkDisplay *display;
  GdkMonitor *monitor;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 1);

  if (_gtk_widget_get_realized (widget))
    return gdk_surface_get_scale_factor (priv->surface);

  root = _gtk_widget_get_root (widget);
  if (root && GTK_WIDGET (root) != widget)
    return gtk_widget_get_scale_factor (GTK_WIDGET (root));

  /* else fall back to something that is more likely to be right than
   * just returning 1:
   */
  display = _gtk_widget_get_display (widget);
  if (display)
    {
      monitor = gdk_display_get_monitor (display, 0);
      if (monitor)
        return gdk_monitor_get_scale_factor (monitor);
    }

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
 **/
GdkDisplay*
gtk_widget_get_display (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return _gtk_widget_get_display (widget);
}

/**
 * gtk_widget_child_focus:
 * @widget: a #GtkWidget
 * @direction: direction of focus movement
 *
 * This function is used by custom widget implementations; if you're
 * writing an app, you’d use gtk_widget_grab_focus() to move the focus
 * to a particular widget.
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
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!_gtk_widget_get_visible (widget) ||
      !gtk_widget_is_sensitive (widget))
    return FALSE;

  /* Emit ::focus in any case, even if can-focus is FALSE,
   * since any widget might have child widgets that will take
   * focus
   */

  return GTK_WIDGET_GET_CLASS (widget)->focus (widget, direction);
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
 * navigation is OK and/or there is nowhere we can/should move the
 * focus to.
 *
 * When %FALSE is returned, the caller should continue with keyboard
 * navigation outside the widget, e.g. by calling
 * gtk_widget_child_focus() on the widget’s toplevel.
 *
 * The default ::keynav-failed handler returns %FALSE for
 * %GTK_DIR_TAB_FORWARD and %GTK_DIR_TAB_BACKWARD. For the other
 * values of #GtkDirectionType it returns %TRUE.
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
 * gdk_surface_beep(), otherwise it does nothing.
 *
 * Note that the effect of gdk_surface_beep() can be configured in many
 * ways, depending on the windowing backend and the desktop environment
 * or window manager that is used.
 **/
void
gtk_widget_error_bell (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkSettings* settings;
  gboolean beep;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  settings = gtk_widget_get_settings (widget);
  if (!settings)
    return;

  g_object_get (settings,
                "gtk-error-bell", &beep,
                NULL);

  if (beep && priv->surface)
    gdk_surface_beep (priv->surface);
}

static void
gtk_widget_set_usize_internal (GtkWidget          *widget,
			       gint                width,
			       gint                height)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (widget));

  if (width > -2 && priv->width_request != width)
    {
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_WIDTH_REQUEST]);
      priv->width_request = width;
      changed = TRUE;
    }
  if (height > -2 && priv->height_request != height)
    {
      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HEIGHT_REQUEST]);
      priv->height_request = height;
      changed = TRUE;
    }

  if (_gtk_widget_get_visible (widget) && changed)
    {
      gtk_widget_queue_resize (widget);
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

  gtk_widget_set_usize_internal (widget, width, height);
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
 * and the natural requisition of the widget will be used instead. See
 * gtk_widget_set_size_request(). To get the size a widget will
 * actually request, call gtk_widget_measure() instead of
 * this function.
 **/
void
gtk_widget_get_size_request (GtkWidget *widget,
                             gint      *width,
                             gint      *height)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (width)
    *width = priv->width_request;

  if (height)
    *height = priv->height_request;
}

/*< private >
 * gtk_widget_has_size_request:
 * @widget: a #GtkWidget
 *
 * Returns if the widget has a size request set (anything besides -1 for height
 * or width)
 */
gboolean
gtk_widget_has_size_request (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return !(priv->width_request == -1 && priv->height_request == -1);
}

/**
 * gtk_widget_get_ancestor:
 * @widget: a #GtkWidget
 * @widget_type: ancestor type
 *
 * Gets the first ancestor of @widget with type @widget_type. For example,
 * `gtk_widget_get_ancestor (widget, GTK_TYPE_BOX)` gets
 * the first #GtkBox that’s an ancestor of @widget. No reference will be
 * added to the returned widget; it should not be unreferenced.
 *
 * Note that unlike gtk_widget_is_ancestor(), gtk_widget_get_ancestor()
 * considers @widget to be an ancestor of itself.
 *
 * Returns: (transfer none) (nullable): the ancestor widget, or %NULL if not found
 **/
GtkWidget*
gtk_widget_get_ancestor (GtkWidget *widget,
			 GType      widget_type)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  while (widget && !g_type_is_a (G_OBJECT_TYPE (widget), widget_type))
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      widget = priv->parent;
    }

  return widget;
}

/**
 * gtk_widget_get_settings:
 * @widget: a #GtkWidget
 *
 * Gets the settings object holding the settings used for this widget.
 *
 * Note that this function can only be called when the #GtkWidget
 * is attached to a toplevel, since the settings object is specific
 * to a particular #GdkDisplay.
 *
 * Returns: (transfer none): the relevant #GtkSettings object
 */
GtkSettings*
gtk_widget_get_settings (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_settings_get_for_display (_gtk_widget_get_display (widget));
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
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

      if (priv->parent == ancestor)
        return TRUE;

      widget = priv->parent;
    }

  return FALSE;
}

static void
gtk_widget_emit_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  old_dir)
{
  GtkTextDirection direction;
  GtkStateFlags state;

  gtk_widget_update_pango_context (widget);

  direction = _gtk_widget_get_direction (widget);

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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkTextDirection old_dir;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (dir >= GTK_TEXT_DIR_NONE && dir <= GTK_TEXT_DIR_RTL);

  old_dir = _gtk_widget_get_direction (widget);

  priv->direction = dir;

  if (old_dir != _gtk_widget_get_direction (widget))
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_TEXT_DIR_LTR);

  if (priv->direction == GTK_TEXT_DIR_NONE)
    return gtk_default_direction;
  else
    return priv->direction;
}

static void
gtk_widget_set_default_direction_recurse (GtkWidget        *widget,
                                          GtkTextDirection  old_dir)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *child;

  g_object_ref (widget);

  if (priv->direction == GTK_TEXT_DIR_NONE)
    gtk_widget_emit_direction_changed (widget, old_dir);

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      gtk_widget_set_default_direction_recurse (child, old_dir);
    }

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
	  gtk_widget_set_default_direction_recurse (tmp_list->data, old_dir);
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
  GtkWidgetPath *path;

  /* As strange as it may seem, this may happen on object construction.
   * init() implementations of parent types may eventually call this function,
   * each with its corresponding GType, which could leave a child
   * implementation with a wrong widget type in the widget path
   */
  path = (GtkWidgetPath*)g_object_get_qdata (object, quark_widget_path);
  if (path && G_OBJECT_TYPE (widget) != gtk_widget_path_get_object_type (path))
    g_object_set_qdata (object, quark_widget_path, NULL);

  G_OBJECT_CLASS (gtk_widget_parent_class)->constructed (object);
}

static void
gtk_widget_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *sizegroups;

  if (priv->children_observer)
    gtk_list_list_model_clear (priv->children_observer);
  if (priv->controller_observer)
    gtk_list_list_model_clear (priv->controller_observer);

  if (priv->parent && GTK_IS_CONTAINER (priv->parent))
    gtk_container_remove (GTK_CONTAINER (priv->parent), widget);
  else if (priv->parent)
    gtk_widget_unparent (widget);
  else if (_gtk_widget_get_visible (widget))
    gtk_widget_hide (widget);

  while (priv->paintables)
    gtk_widget_paintable_set_widget (priv->paintables->data, NULL);

  if (priv->layout_manager != NULL)
    gtk_layout_manager_set_widget (priv->layout_manager, NULL);
  g_clear_object (&priv->layout_manager);

  priv->visible = FALSE;
  if (_gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  g_clear_object (&priv->cursor);

  if (!priv->in_destruction)
    {
      priv->in_destruction = TRUE;
      g_signal_emit (object, widget_signals[DESTROY], 0);
      priv->in_destruction = FALSE;
    }

  sizegroups = _gtk_widget_get_sizegroups (widget);
  while (sizegroups)
    {
      GtkSizeGroup *size_group;

      size_group = sizegroups->data;
      sizegroups = sizegroups->next;
      gtk_size_group_remove_widget (size_group, widget);
    }

  g_object_set_qdata (object, quark_action_muxer, NULL);

  while (priv->attached_windows)
    gtk_window_set_attached_to (priv->attached_windows->data, NULL);

  G_OBJECT_CLASS (gtk_widget_parent_class)->dispose (object);
}

#ifdef G_ENABLE_CONSISTENCY_CHECKS
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
    g_critical ("Automated component '%s' of class '%s' seems to have been prematurely finalized",
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
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

static void
gtk_widget_real_destroy (GtkWidget *object)
{
  /* gtk_object_destroy() will already hold a refcount on object */
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (g_object_get_qdata (G_OBJECT (widget), quark_auto_children))
    {
      GtkWidgetClass *class;
      GSList *l;

#ifdef G_ENABLE_CONSISTENCY_CHECKS
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
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

      /* Release references to all automated children */
      g_object_set_qdata (G_OBJECT (widget), quark_auto_children, NULL);

#ifdef G_ENABLE_CONSISTENCY_CHECKS
      for (l = assertions; l; l = l->next)
	{
	  FinalizeAssertion *assertion = l->data;

	  if (!assertion->did_finalize)
	    g_critical ("Automated component '%s' of class '%s' did not finalize in gtk_widget_destroy(). "
			"Current reference count is %d",
			assertion->child_class->name,
			g_type_name (assertion->widget_type),
			assertion->object->ref_count);

	  g_slice_free (FinalizeAssertion, assertion);
	}
      g_slist_free (assertions);
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

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

  if (priv->accessible)
    {
      gtk_accessible_set_widget (GTK_ACCESSIBLE (priv->accessible), NULL);
      g_object_unref (priv->accessible);
      priv->accessible = NULL;
    }

  /* wipe accelerator closures (keep order) */
  g_object_set_qdata (G_OBJECT (widget), quark_accel_path, NULL);
  g_object_set_qdata (G_OBJECT (widget), quark_accel_closures, NULL);

  /* Callers of add_mnemonic_label() should disconnect on ::destroy */
  g_object_set_qdata (G_OBJECT (widget), quark_mnemonic_labels, NULL);

  gtk_grab_remove (widget);

  destroy_tick_callbacks (widget);
  destroy_surface_transform_data (widget);
}

static void
gtk_widget_finalize (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  gtk_grab_remove (widget);

  g_free (priv->name);

  g_clear_object (&priv->accessible);
  g_clear_pointer (&priv->transform, gsk_transform_unref);
  g_clear_pointer (&priv->allocated_transform, gsk_transform_unref);

  gtk_widget_clear_path (widget);

  gtk_css_widget_node_widget_destroyed (GTK_CSS_WIDGET_NODE (priv->cssnode));
  g_object_unref (priv->cssnode);

  g_clear_object (&priv->context);

  _gtk_size_request_cache_free (&priv->requests);

  l = priv->event_controllers;
  while (l)
    {
      GList *next = l->next;
      GtkEventController *controller = l->data;

      if (controller)
        gtk_widget_remove_controller (widget, controller);

      l = next;
    }
  g_assert (priv->event_controllers == NULL);

  if (_gtk_widget_get_first_child (widget) != NULL)
    {
      GtkWidget *child;
      g_warning ("Finalizing %s %p, but it still has children left:",
                 gtk_widget_get_name (widget), widget);
      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          g_warning ("   - %s %p", gtk_widget_get_name (child), child);
        }
    }

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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (_gtk_widget_get_realized (widget));

  if (!_gtk_widget_get_mapped (widget))
    {
      GtkWidget *p;
      priv->mapped = TRUE;

      for (p = gtk_widget_get_first_child (widget);
           p != NULL;
           p = gtk_widget_get_next_sibling (p))
        {
          if (_gtk_widget_get_visible (p) &&
              _gtk_widget_get_child_visible (p) &&
              !_gtk_widget_get_mapped (p))
            gtk_widget_map (p);
        }
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (_gtk_widget_get_mapped (widget))
    {
      GtkWidget *child;
      priv->mapped = FALSE;

      for (child = gtk_widget_get_first_child (widget);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          gtk_widget_unmap (child);
        }

      gtk_widget_update_paintables (widget);

      gtk_widget_unset_state_flags (widget,
                                    GTK_STATE_FLAG_PRELIGHT |
                                    GTK_STATE_FLAG_ACTIVE);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (GTK_IS_NATIVE (widget))
    {
      priv->surface = gtk_native_get_surface (GTK_NATIVE (widget));
      g_object_ref (priv->surface);
    }
  else
    {
      g_assert (priv->parent);
      priv->surface = priv->parent->priv->surface;
      g_object_ref (priv->surface);
    }

  priv->realized = TRUE;

  gtk_widget_connect_frame_clock (widget);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_assert (!priv->mapped);

   /* We must do unrealize child widget BEFORE container widget.
    * gdk_surface_destroy() destroys specified xwindow and its sub-xwindows.
    * So, unrealizing container widget before its children causes the problem
    * (for example, gdk_ic_destroy () with destroyed window causes crash.)
    */

  gtk_widget_forall (widget, (GtkCallback)gtk_widget_unrealize, NULL);

  gtk_widget_disconnect_frame_clock (widget);

  priv->realized = FALSE;

  g_clear_object (&priv->surface);
}

void
gtk_widget_adjust_size_request (GtkWidget      *widget,
                                GtkOrientation  orientation,
                                gint           *minimum_size,
                                gint           *natural_size)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL && priv->width_request > 0)
    *minimum_size = MAX (*minimum_size, priv->width_request);
  else if (orientation == GTK_ORIENTATION_VERTICAL && priv->height_request > 0)
    *minimum_size = MAX (*minimum_size, priv->height_request);

  /* Fix it if set_size_request made natural size smaller than min size.
   * This would also silently fix broken widgets, but we warn about them
   * in gtksizerequest.c when calling their size request vfuncs.
   */
  *natural_size = MAX (*natural_size, *minimum_size);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum_size += priv->margin.left + priv->margin.right;
      *natural_size += priv->margin.left + priv->margin.right;
    }
  else
    {
      *minimum_size += priv->margin.top + priv->margin.bottom;
      *natural_size += priv->margin.top + priv->margin.bottom;
    }
}

void
gtk_widget_adjust_baseline_request (GtkWidget *widget,
                                    gint      *minimum_baseline,
                                    gint      *natural_baseline)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->height_request >= 0)
    {
      /* No baseline support for explicitly set height */
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else
    {
      *minimum_baseline += priv->margin.top;
      *natural_baseline += priv->margin.top;
    }
}

static gboolean
is_my_surface (GtkWidget *widget,
	       GdkSurface *surface)
{
  if (!surface)
    return FALSE;

  return gdk_surface_get_widget (surface) == widget;
}

/*
 * _gtk_widget_get_device_surface:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns: (nullable): the surface of @widget that @device is in, or %NULL
 */
GdkSurface *
_gtk_widget_get_device_surface (GtkWidget *widget,
				GdkDevice *device)
{
  GdkSurface *surface;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    return NULL;

  surface = gdk_device_get_last_event_surface (device);
  if (surface && is_my_surface (widget, surface))
    return surface;
  else
    return NULL;
}

/*
 * _gtk_widget_list_devices:
 * @widget: a #GtkWidget
 *
 * Returns the list of pointer #GdkDevices that are currently
 * on top of any surface belonging to @widget. Free the list
 * with g_list_free(), the elements are owned by GTK+ and must
 * not be freed.
 */
GList *
_gtk_widget_list_devices (GtkWidget *widget)
{
  GdkSeat *seat;
  GList *result = NULL;
  GList *devices;
  GList *l;
  GdkDevice *device;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (!_gtk_widget_get_mapped (widget))
    return NULL;

  seat = gdk_display_get_default_seat (_gtk_widget_get_display (widget));
  device = gdk_seat_get_pointer (seat);
  if (is_my_surface (widget, gdk_device_get_last_event_surface (device)))
    result = g_list_prepend (result, device);

  devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_ALL_POINTING);
  for (l = devices; l; l = l->next)
    {
      device = l->data;
      if (is_my_surface (widget, gdk_device_get_last_event_surface (device)))
        result = g_list_prepend (result, device);
    }
  g_list_free (devices);

  return result;
}

static void
synth_crossing (GtkWidget       *widget,
                GdkEventType     type,
                GdkSurface       *surface,
                GdkDevice       *device,
                GdkCrossingMode  mode,
                GdkNotifyType    detail)
{
  GdkEvent *event;

  event = gdk_event_new (type);

  event->any.surface = g_object_ref (surface);
  event->any.send_event = TRUE;
  event->crossing.child_surface = g_object_ref (surface);
  event->crossing.time = GDK_CURRENT_TIME;
  gdk_surface_get_device_position (surface,
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

  g_object_unref (event);
}

/*
 * _gtk_widget_synthesize_crossing:
 * @from: the #GtkWidget the virtual pointer is leaving.
 * @to: the #GtkWidget the virtual pointer is moving to.
 * @mode: the #GdkCrossingMode to place on the synthesized events.
 *
 * Generate crossing event(s) on widget state (sensitivity) or GTK+ grab change.
 */
void
_gtk_widget_synthesize_crossing (GtkWidget       *from,
				 GtkWidget       *to,
                                 GdkDevice       *device,
				 GdkCrossingMode  mode)
{
  GdkSurface *from_surface = NULL, *to_surface = NULL;

  g_return_if_fail (from != NULL || to != NULL);

  if (from != NULL)
    {
      from_surface = _gtk_widget_get_device_surface (from, device);

      if (!from_surface)
        from_surface = from->priv->surface;
    }

  if (to != NULL)
    {
      to_surface = _gtk_widget_get_device_surface (to, device);

      if (!to_surface)
        to_surface = to->priv->surface;
    }

  if (from_surface == NULL && to_surface == NULL)
    ;
  else if (from_surface != NULL && to_surface == NULL)
    {
      synth_crossing (from, GDK_LEAVE_NOTIFY, from_surface,
		      device, mode, GDK_NOTIFY_ANCESTOR);
    }
  else if (from_surface == NULL && to_surface != NULL)
    {
      synth_crossing (to, GDK_ENTER_NOTIFY, to_surface,
		      device, mode, GDK_NOTIFY_ANCESTOR);
    }
  else if (from_surface == to_surface)
    ;
  else
    {
      synth_crossing (from, GDK_LEAVE_NOTIFY, from_surface,
                      device, mode, GDK_NOTIFY_NONLINEAR);

      synth_crossing (to, GDK_ENTER_NOTIFY, to_surface,
                      device, mode, GDK_NOTIFY_NONLINEAR);
    }
}

static void
gtk_widget_propagate_state (GtkWidget          *widget,
                            const GtkStateData *data)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkStateFlags new_flags, old_flags = priv->state_flags;
  GtkStateData child_data;
  GtkWidget *child;
  gint new_scale_factor = gtk_widget_get_scale_factor (widget);

  priv->state_flags |= data->flags_to_set;
  priv->state_flags &= ~(data->flags_to_unset);

  /* make insensitivity unoverridable */
  if (!priv->sensitive)
    priv->state_flags |= GTK_STATE_FLAG_INSENSITIVE;

  if (gtk_widget_is_focus (widget) && !gtk_widget_is_sensitive (widget))
    gtk_root_set_focus (priv->root, NULL);

  new_flags = priv->state_flags;

  if (data->old_scale_factor != new_scale_factor)
    _gtk_widget_scale_changed (widget);

  if (old_flags != new_flags)
    {
      g_object_ref (widget);

      if (!gtk_widget_is_sensitive (widget) && gtk_widget_has_grab (widget))
        gtk_grab_remove (widget);

      gtk_style_context_set_state (_gtk_widget_get_style_context (widget), new_flags);

      g_signal_emit (widget, widget_signals[STATE_FLAGS_CHANGED], 0, old_flags);

      if (!priv->shadowed &&
          (new_flags & GTK_STATE_FLAG_INSENSITIVE) != (old_flags & GTK_STATE_FLAG_INSENSITIVE))
        {
          GList *event_surfaces = NULL;
          GList *devices, *d;

          devices = _gtk_widget_list_devices (widget);

          for (d = devices; d; d = d->next)
            {
              GdkSurface *surface;
              GdkDevice *device;

              device = d->data;
              surface = _gtk_widget_get_device_surface (widget, device);

              /* Do not propagate more than once to the
               * same surface if non-multidevice aware.
               */
              if (!gdk_surface_get_support_multidevice (surface) &&
                  g_list_find (event_surfaces, surface))
                continue;

              if (!gtk_widget_is_sensitive (widget))
                _gtk_widget_synthesize_crossing (widget, NULL, d->data,
                                                 GDK_CROSSING_STATE_CHANGED);
              else
                _gtk_widget_synthesize_crossing (NULL, widget, d->data,
                                                 GDK_CROSSING_STATE_CHANGED);

              event_surfaces = g_list_prepend (event_surfaces, surface);
            }

          g_list_free (event_surfaces);
          g_list_free (devices);
        }

      if (!gtk_widget_is_sensitive (widget))
        gtk_widget_reset_controllers (widget);


      /* Make sure to only propagate the right states further */
      child_data.old_scale_factor = new_scale_factor;
      child_data.flags_to_set = data->flags_to_set & GTK_STATE_FLAGS_DO_SET_PROPAGATE;
      child_data.flags_to_unset = data->flags_to_unset & GTK_STATE_FLAGS_DO_UNSET_PROPAGATE;

      if (child_data.flags_to_set != 0 ||
          child_data.flags_to_unset != 0)
        {
          for (child = _gtk_widget_get_first_child (widget);
               child != NULL;
               child = _gtk_widget_get_next_sibling (child))
            {
              gtk_widget_propagate_state (child, &child_data);
            }
        }

      g_object_unref (widget);
    }
}

static void
gtk_widget_update_input_shape (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  /* set shape if widget has a GDK surface already.
   * otherwise the shape is scheduled to be set by gtk_widget_realize().
   */
  if (priv->surface)
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

      gdk_surface_input_shape_combine_region (priv->surface, region, 0, 0);

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
 * Sets an input shape for this widget’s GDK surface. This allows for
 * windows which react to mouse click in a nonrectangular region, see
 * gdk_surface_input_shape_combine_region() for more information.
 **/
void
gtk_widget_input_shape_combine_region (GtkWidget      *widget,
                                       cairo_region_t *region)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without GDK surface */
  g_return_if_fail (GTK_IS_NATIVE (widget));

  if (region == NULL)
    g_object_set_qdata (G_OBJECT (widget), quark_input_shape_info, NULL);
  else
    g_object_set_qdata_full (G_OBJECT (widget), quark_input_shape_info,
                             cairo_region_copy (region),
                             (GDestroyNotify) cairo_region_destroy);
  gtk_widget_update_input_shape (widget);
}

/**
 * gtk_requisition_new:
 *
 * Allocates a new #GtkRequisition-struct and initializes its elements to zero.
 *
 * Returns: a new empty #GtkRequisition. The newly allocated #GtkRequisition should
 *   be freed with gtk_requisition_free().
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
 * Returns: (nullable): the accessible for @widget or %NULL if none has been
 *     created yet.
 **/
AtkObject *
_gtk_widget_peek_accessible (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->accessible;
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
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return GTK_WIDGET_GET_CLASS (widget)->get_accessible (widget);
}

static AtkObject*
gtk_widget_real_get_accessible (GtkWidget *widget)
{
  AtkObject* accessible;

  accessible = widget->priv->accessible;

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
          accessible = atk_object_factory_create_accessible (factory, G_OBJECT (widget));

          if (priv->accessible_role != ATK_ROLE_INVALID)
            atk_object_set_role (accessible, priv->accessible_role);

          widget->priv->accessible = accessible;
        }
      else
        {
          accessible = g_object_new (priv->accessible_type,
                                     "widget", widget,
                                     NULL);
          if (priv->accessible_role != ATK_ROLE_INVALID)
            atk_object_set_role (accessible, priv->accessible_role);

          widget->priv->accessible = accessible;

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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

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
gtk_widget_compute_expand (GtkWidget      *widget,
                           GtkOrientation  orientation)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  /* We never make a widget expand if not even showing. */
  if (!_gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_update_computed_expand (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return priv->computed_hexpand;
  else
    return priv->computed_vexpand;
}

static void
gtk_widget_set_expand (GtkWidget     *widget,
                       GtkOrientation orientation,
                       gboolean       expand)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gint expand_prop;
  gint expand_set_prop;
  gboolean was_both;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  expand = expand != FALSE;

  was_both = priv->hexpand && priv->vexpand;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->hexpand_set &&
          priv->hexpand == expand)
        return;

      priv->hexpand_set = TRUE;
      priv->hexpand = expand;

      expand_prop = PROP_HEXPAND;
      expand_set_prop = PROP_HEXPAND_SET;
    }
  else
    {
      if (priv->vexpand_set &&
          priv->vexpand == expand)
        return;

      priv->vexpand_set = TRUE;
      priv->vexpand = expand;

      expand_prop = PROP_VEXPAND;
      expand_set_prop = PROP_VEXPAND_SET;
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_freeze_notify (G_OBJECT (widget));
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[expand_prop]);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[expand_set_prop]);
  if (was_both != (priv->hexpand && priv->vexpand))
    g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_EXPAND]);
  g_object_thaw_notify (G_OBJECT (widget));
}

static void
gtk_widget_set_expand_set (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           gboolean        set)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  gint prop;

  set = set != FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (set == priv->hexpand_set)
        return;

      priv->hexpand_set = set;
      prop = PROP_HEXPAND_SET;
    }
  else
    {
      if (set == priv->vexpand_set)
        return;

      priv->vexpand_set = set;
      prop = PROP_VEXPAND_SET;
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[prop]);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->hexpand;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->hexpand_set;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->vexpand;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->vexpand_set;
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
static GQuark		 quark_builder_atk_relations = 0;
static GQuark            quark_builder_set_name = 0;

static void
gtk_widget_buildable_add_child (GtkBuildable  *buildable,
                                GtkBuilder    *builder,
                                GObject       *child,
                                const gchar   *type)
{
  if (type != NULL)
    {
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
    }
  if (GTK_IS_WIDGET (child))
    {
      gtk_widget_set_parent (GTK_WIDGET (child), GTK_WIDGET (buildable));
    }
  else if (GTK_IS_EVENT_CONTROLLER (child))
    {
      gtk_widget_add_controller (GTK_WIDGET (buildable), g_object_ref (GTK_EVENT_CONTROLLER (child)));
    }
  else
    {
      g_warning ("Cannot add an object of type %s to a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (child)), g_type_name (G_OBJECT_TYPE (buildable)));
    }
}

static void
gtk_widget_buildable_interface_init (GtkBuildableIface *iface)
{
  quark_builder_atk_relations = g_quark_from_static_string ("gtk-builder-atk-relations");
  quark_builder_set_name = g_quark_from_static_string ("gtk-builder-set-name");

  iface->set_name = gtk_widget_buildable_set_name;
  iface->get_name = gtk_widget_buildable_get_name;
  iface->get_internal_child = gtk_widget_buildable_get_internal_child;
  iface->parser_finished = gtk_widget_buildable_parser_finished;
  iface->custom_tag_start = gtk_widget_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_widget_buildable_custom_tag_end;
  iface->custom_finished = gtk_widget_buildable_custom_finished;
  iface->add_child = gtk_widget_buildable_add_child;
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
  AtkRelationType type;
  gint line;
  gint col;
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
  g_slice_free (AtkRelationData, data);
}

static void
gtk_widget_buildable_parser_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder)
{
  GSList *atk_relations;

  atk_relations = g_object_get_qdata (G_OBJECT (buildable),
				      quark_builder_atk_relations);
  if (atk_relations)
    {
      AtkObject *accessible;
      AtkRelationSet *relation_set;
      GSList *l;
      GObject *target;
      AtkObject *target_accessible;

      accessible = gtk_widget_get_accessible (GTK_WIDGET (buildable));
      relation_set = atk_object_ref_relation_set (accessible);

      for (l = atk_relations; l; l = l->next)
	{
	  AtkRelationData *relation = (AtkRelationData*)l->data;

	  target = _gtk_builder_lookup_object (builder, relation->target, relation->line, relation->col);
	  if (!target)
	    continue;
	  target_accessible = gtk_widget_get_accessible (GTK_WIDGET (target));
	  g_assert (target_accessible != NULL);

	  atk_relation_set_add_relation_by_type (relation_set, relation->type, target_accessible);
	}
      g_object_unref (relation_set);

      g_slist_free_full (atk_relations, (GDestroyNotify) free_relation);
      g_object_steal_qdata (G_OBJECT (buildable), quark_builder_atk_relations);
    }
}

typedef struct
{
  GtkBuilder *builder;
  GSList *actions;
  GSList *relations;
  AtkRole role;
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

  if (strcmp (element_name, "relation") == 0)
    {
      gchar *target = NULL;
      gchar *type = NULL;
      AtkRelationData *relation;
      AtkRelationType relation_type;

      if (!_gtk_builder_check_parent (data->builder, context, "accessibility", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "target", &target,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      relation_type = atk_relation_type_for_name (type);
      if (relation_type == ATK_RELATION_NULL)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "No such relation type: '%s'", type);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      relation = g_slice_new (AtkRelationData);
      relation->target = g_strdup (target);
      relation->type = relation_type;

      data->relations = g_slist_prepend (data->relations, relation);
    }
  else if (strcmp (element_name, "action") == 0)
    {
      const gchar *action_name;
      const gchar *description = NULL;
      const gchar *msg_context = NULL;
      gboolean translatable = FALSE;
      AtkActionData *action;

      if (!_gtk_builder_check_parent (data->builder, context, "accessibility", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "action_name", &action_name,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "description", &description,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      action = g_slice_new (AtkActionData);
      action->action_name = g_strdup (action_name);
      action->description = g_string_new (description);
      action->context = g_strdup (msg_context);
      action->translatable = translatable;

      data->actions = g_slist_prepend (data->actions, action);
    }
  else if (strcmp (element_name, "role") == 0)
    {
      const gchar *type;
      AtkRole role;

      if (!_gtk_builder_check_parent (data->builder, context, "accessibility", error))
        return;

      if (data->role != ATK_ROLE_INVALID)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Duplicate accessibility role definition");
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      role = atk_role_for_name (type);
      if (role == ATK_ROLE_INVALID)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "No such role type: '%s'", type);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->role = role;
    }
  else if (strcmp (element_name, "accessibility") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
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
  GtkBuilder *builder;
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
  AccelGroupParserData *data = (AccelGroupParserData*)user_data;

  if (strcmp (element_name, "accelerator") == 0)
    {
      const gchar *key_str = NULL;
      const gchar *signal = NULL;
      const gchar *modifiers_str = NULL;
      guint key = 0;
      guint modifiers = 0;

      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "key", &key_str,
                                        G_MARKUP_COLLECT_STRING, "signal", &signal,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "modifiers", &modifiers_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      key = gdk_keyval_from_name (key_str);
      if (key == 0)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Could not parse key '%s'", key_str);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (modifiers_str != NULL)
        {
          GFlagsValue aliases[2] = {
            { 0, "primary", "primary" },
            { 0, NULL, NULL }
          };

          aliases[0].value = _gtk_get_primary_accel_mod ();

          if (!_gtk_builder_flags_from_string (GDK_TYPE_MODIFIER_TYPE, aliases,
                                               modifiers_str, &modifiers, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
	      return;
            }
        }

      data->key = key;
      data->modifiers = modifiers;
      data->signal = g_strdup (signal);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

static const GMarkupParser accel_group_parser =
  {
    accel_group_start_element,
  };

typedef struct
{
  GtkBuilder *builder;
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
  StyleParserData *data = (StyleParserData *)user_data;

  if (strcmp (element_name, "class") == 0)
    {
      const gchar *name;

      if (!_gtk_builder_check_parent (data->builder, context, "style", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->classes = g_slist_prepend (data->classes, g_strdup (name));
    }
  else if (strcmp (element_name, "style") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

static const GMarkupParser style_parser =
  {
    style_start_element,
  };

typedef struct
{
  char *name;
  GString *value;
  char *context;
  gboolean translatable;
} LayoutPropertyInfo;

typedef struct
{
  GObject *object;
  GtkBuilder *builder;

  LayoutPropertyInfo *cur_property;

  /* SList<LayoutPropertyInfo> */
  GSList *properties;
} LayoutParserData;

static void
layout_property_info_free (gpointer data)
{
  LayoutPropertyInfo *pinfo = data;

  if (pinfo == NULL)
    return;

  g_free (pinfo->name);
  g_free (pinfo->context);
  g_string_free (pinfo->value, TRUE);
}

static void
layout_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  LayoutParserData *layout_data = user_data;

  if (strcmp (element_name, "property") == 0)
    {
      const char *name = NULL;
      const char *ctx = NULL;
      gboolean translatable = FALSE;
      LayoutPropertyInfo *pinfo;

      if (!_gtk_builder_check_parent (layout_data->builder, context, "layout", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "context", &ctx,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (layout_data->builder, context, error);
          return;
        }

      pinfo = g_new0 (LayoutPropertyInfo, 1);
      pinfo->name = g_strdup (name);
      pinfo->translatable = translatable;
      pinfo->context = g_strdup (ctx);
      pinfo->value = g_string_new (NULL);

      layout_data->cur_property = pinfo;
    }
  else if (strcmp (element_name, "layout") == 0)
    {
      if (!_gtk_builder_check_parent (layout_data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (layout_data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (layout_data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}

static void
layout_text (GMarkupParseContext  *context,
             const gchar          *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
  LayoutParserData *layout_data = user_data;

  if (layout_data->cur_property != NULL)
    g_string_append_len (layout_data->cur_property->value, text, text_len);
}

static void
layout_end_element (GMarkupParseContext  *context,
                    const char           *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  LayoutParserData *layout_data = user_data;

  if (layout_data->cur_property != NULL)
    {
      LayoutPropertyInfo *pinfo = g_steal_pointer (&layout_data->cur_property);

      /* Translate the string, if needed */
      if (pinfo->value->len != 0 && pinfo->translatable)
        {
          const char *translated;
          const char *domain;

          domain = gtk_builder_get_translation_domain (layout_data->builder);

          translated = _gtk_builder_parser_translate (domain, pinfo->context, pinfo->value->str);

          g_string_assign (pinfo->value, translated);
        }

      /* We assign all properties at the end of the `layout` section */
      layout_data->properties = g_slist_prepend (layout_data->properties, pinfo);
    }
}

static const GMarkupParser layout_parser =
  {
    layout_start_element,
    layout_end_element,
    layout_text,
  };

static gboolean
gtk_widget_buildable_custom_tag_start (GtkBuildable     *buildable,
                                       GtkBuilder       *builder,
                                       GObject          *child,
                                       const gchar      *tagname,
                                       GMarkupParser    *parser,
                                       gpointer         *parser_data)
{
  if (strcmp (tagname, "accelerator") == 0)
    {
      AccelGroupParserData *data;

      data = g_slice_new0 (AccelGroupParserData);
      data->object = (GObject *)g_object_ref (buildable);
      data->builder = builder;

      *parser = accel_group_parser;
      *parser_data = data;

      return TRUE;
    }

  if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilitySubParserData *data;

      data = g_slice_new0 (AccessibilitySubParserData);
      data->builder = builder;

      *parser = accessibility_parser;
      *parser_data = data;

      return TRUE;
    }

  if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *data;

      data = g_slice_new0 (StyleParserData);
      data->builder = builder;

      *parser = style_parser;
      *parser_data = data;

      return TRUE;
    }

  if (strcmp (tagname, "layout") == 0)
    {
      LayoutParserData *data;

      data = g_slice_new0 (LayoutParserData);
      data->builder = builder;
      data->object = (GObject *) g_object_ref (buildable);

      *parser = layout_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_widget_buildable_custom_tag_end (GtkBuildable  *buildable,
                                     GtkBuilder    *builder,
                                     GObject       *child,
                                     const gchar   *tagname,
                                     gpointer       data)
{
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
      if (GTK_IS_WINDOW (toplevel))
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
gtk_widget_buildable_finish_layout_properties (GtkWidget *widget,
                                               GtkWidget *parent,
                                               gpointer   data)
{
  LayoutParserData *layout_data = data;
  GtkLayoutManager *layout_manager;
  GtkLayoutChild *layout_child;
  GObject *gobject;
  GObjectClass *gobject_class;
  GSList *layout_properties, *l;

  layout_manager = gtk_widget_get_layout_manager (parent);
  if (layout_manager == NULL)
    return;

  layout_child = gtk_layout_manager_get_layout_child (layout_manager, widget);
  if (layout_child == NULL)
    return;

  gobject = G_OBJECT (layout_child);
  gobject_class = G_OBJECT_GET_CLASS (layout_child);

  layout_properties = g_slist_reverse (layout_data->properties);
  layout_data->properties = NULL;

  for (l = layout_properties; l != NULL; l = l->next)
    {
      LayoutPropertyInfo *pinfo = l->data;
      GParamSpec *pspec;
      GValue value = G_VALUE_INIT;
      GError *error = NULL;

      pspec = g_object_class_find_property (gobject_class, pinfo->name);
      if (pspec == NULL)
        {
          g_warning ("Unable to find layout property “%s” for children "
                     "of layout managers of type “%s”",
                     pinfo->name,
                     G_OBJECT_TYPE_NAME (layout_manager));
          continue;
        }

      gtk_builder_value_from_string (layout_data->builder,
                                     pspec,
                                     pinfo->value->str,
                                     &value,
                                     &error);
      if (error != NULL)
        {
          g_warning ("Failed to set property “%s.%s” to “%s”: %s",
                     G_OBJECT_TYPE_NAME (layout_child),
                     pinfo->name,
                     pinfo->value->str,
                     error->message);
          g_error_free (error);
          continue;
        }

      g_object_set_property (gobject, pinfo->name, &value);
      g_value_unset (&value);
    }

  g_slist_free_full (layout_properties, layout_property_info_free);
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
      GtkRoot *root;

      accel_data = (AccelGroupParserData*)user_data;
      g_assert (accel_data->object);

      root = _gtk_widget_get_root (GTK_WIDGET (accel_data->object));

      _gtk_widget_buildable_finish_accelerator (GTK_WIDGET (buildable), GTK_WIDGET (root), user_data);
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

      if (a11y_data->role != ATK_ROLE_INVALID)
        {
          AtkObject *accessible = gtk_widget_get_accessible (GTK_WIDGET (buildable));
          atk_object_set_role (accessible, a11y_data->role);
        }

      g_slice_free (AccessibilitySubParserData, a11y_data);
    }
  else if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *style_data = (StyleParserData *)user_data;
      GtkStyleContext *context;
      GSList *l;

      context = _gtk_widget_get_style_context (GTK_WIDGET (buildable));

      for (l = style_data->classes; l; l = l->next)
        gtk_style_context_add_class (context, (const gchar *)l->data);

      gtk_widget_reset_style (GTK_WIDGET (buildable));

      g_slist_free_full (style_data->classes, g_free);
      g_slice_free (StyleParserData, style_data);
    }
  else if (strcmp (tagname, "layout") == 0)
    {
      LayoutParserData *layout_data = (LayoutParserData *) user_data;
      GtkWidget *parent = _gtk_widget_get_parent (GTK_WIDGET (buildable));

      if (parent != NULL)
        gtk_widget_buildable_finish_layout_properties (GTK_WIDGET (buildable),
                                                       parent,
                                                       layout_data);

      /* Free the unapplied properties, if any */
      g_slist_free_full (layout_data->properties, layout_property_info_free);
      g_object_unref (layout_data->object);
      g_slice_free (LayoutParserData, layout_data);
    }
}

static GtkSizeRequestMode
gtk_widget_real_get_request_mode (GtkWidget *widget)
{
  /* By default widgets don't trade size at all. */
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_widget_real_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  *minimum = 0;
  *natural = 0;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkAlign align;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);

  align = priv->halign;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->halign == align)
    return;

  priv->halign = align;
  gtk_widget_queue_allocate (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HALIGN]);
}

/**
 * gtk_widget_get_valign:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:valign property.
 *
 * Returns: the vertical alignment of @widget
 */
GtkAlign
gtk_widget_get_valign (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);

  return priv->valign;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->valign == align)
    return;

  priv->valign = align;
  gtk_widget_queue_allocate (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_VALIGN]);
}

/**
 * gtk_widget_get_margin_start:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-start property.
 *
 * Returns: The start margin of @widget
 */
gint
gtk_widget_get_margin_start (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.left;
}

/**
 * gtk_widget_set_margin_start:
 * @widget: a #GtkWidget
 * @margin: the start margin
 *
 * Sets the start margin of @widget.
 * See the #GtkWidget:margin-start property.
 */
void
gtk_widget_set_margin_start (GtkWidget *widget,
                             gint       margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  /* We always save margin-start as .left */

  if (priv->margin.left == margin)
    return;

  priv->margin.left = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_START]);
}

/**
 * gtk_widget_get_margin_end:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-end property.
 *
 * Returns: The end margin of @widget
 */
gint
gtk_widget_get_margin_end (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.right;
}

/**
 * gtk_widget_set_margin_end:
 * @widget: a #GtkWidget
 * @margin: the end margin
 *
 * Sets the end margin of @widget.
 * See the #GtkWidget:margin-end property.
 */
void
gtk_widget_set_margin_end (GtkWidget *widget,
                           gint       margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  /* We always set margin-end as .right */

  if (priv->margin.right == margin)
    return;

  priv->margin.right = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_END]);
}

/**
 * gtk_widget_get_margin_top:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-top property.
 *
 * Returns: The top margin of @widget
 */
gint
gtk_widget_get_margin_top (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.top;
}

/**
 * gtk_widget_set_margin_top:
 * @widget: a #GtkWidget
 * @margin: the top margin
 *
 * Sets the top margin of @widget.
 * See the #GtkWidget:margin-top property.
 */
void
gtk_widget_set_margin_top (GtkWidget *widget,
                           gint       margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  if (priv->margin.top == margin)
    return;

  priv->margin.top = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_TOP]);
}

/**
 * gtk_widget_get_margin_bottom:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-bottom property.
 *
 * Returns: The bottom margin of @widget
 */
gint
gtk_widget_get_margin_bottom (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->margin.bottom;
}

/**
 * gtk_widget_set_margin_bottom:
 * @widget: a #GtkWidget
 * @margin: the bottom margin
 *
 * Sets the bottom margin of @widget.
 * See the #GtkWidget:margin-bottom property.
 */
void
gtk_widget_set_margin_bottom (GtkWidget *widget,
                              gint       margin)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  if (priv->margin.bottom == margin)
    return;

  priv->margin.bottom = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_MARGIN_BOTTOM]);
}

/**
 * gtk_widget_get_clipboard:
 * @widget: a #GtkWidget
 *
 * This is a utility function to get the clipboard object for the
 * #GdkDisplay that @widget is using.
 *
 * Note that this function always works, even when @widget is not
 * realized yet.
 *
 * Returns: (transfer none): the appropriate clipboard object.
 **/
GdkClipboard *
gtk_widget_get_clipboard (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_display_get_clipboard (_gtk_widget_get_display (widget));
}

/**
 * gtk_widget_get_primary_clipboard:
 * @widget: a #GtkWidget
 *
 * This is a utility function to get the primary clipboard object 
 * for the #GdkDisplay that @widget is using.
 *
 * Note that this function always works, even when @widget is not
 * realized yet.
 *
 * Returns: (transfer none): the appropriate clipboard object.
 **/
GdkClipboard *
gtk_widget_get_primary_clipboard (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_display_get_primary_clipboard (_gtk_widget_get_display (widget));
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
 * @label: a #GtkWidget that was previously set as a mnemonic label for
 *         @widget with gtk_widget_add_mnemonic_label().
 *
 * Removes a widget from the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). The widget
 * must have previously been added to the list with
 * gtk_widget_add_mnemonic_label().
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
 * gtk_widget_set_tooltip_window:
 * @widget: a #GtkWidget
 * @custom_window: (allow-none): a #GtkWindow, or %NULL
 *
 * Replaces the default window used for displaying
 * tooltips with @custom_window. GTK+ will take care of showing and
 * hiding @custom_window at the right moment, to behave likewise as
 * the default tooltip window. If @custom_window is %NULL, the default
 * tooltip window will be used.
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
  gtk_widget_set_has_tooltip (widget, has_tooltip);

  if (has_tooltip && _gtk_widget_get_visible (widget))
    gtk_widget_trigger_tooltip_query (widget);
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
 */
void
gtk_widget_trigger_tooltip_query (GtkWidget *widget)
{
  gtk_tooltip_trigger_tooltip_query (widget);
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
 * Returns: (nullable): the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
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
 * Returns: (nullable): the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
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
 */
void
gtk_widget_set_has_tooltip (GtkWidget *widget,
                            gboolean   has_tooltip)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  has_tooltip = !!has_tooltip;

  if (priv->has_tooltip != has_tooltip)
    {
      priv->has_tooltip = has_tooltip;

      g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HAS_TOOLTIP]);
    }
}

/**
 * gtk_widget_get_has_tooltip:
 * @widget: a #GtkWidget
 *
 * Returns the current value of the has-tooltip property.  See
 * #GtkWidget:has-tooltip for more information.
 *
 * Returns: current value of has-tooltip on @widget.
 */
gboolean
gtk_widget_get_has_tooltip (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->has_tooltip;
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
 */
void
gtk_widget_get_allocation (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  const graphene_rect_t *margin_rect;
  float dx, dy;
  GtkCssBoxes boxes;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (allocation != NULL);

  gtk_css_boxes_init (&boxes, widget);
  margin_rect = gtk_css_boxes_get_margin_rect (&boxes);

  if (gsk_transform_get_category (priv->transform) >= GSK_TRANSFORM_CATEGORY_2D_TRANSLATE)
    gsk_transform_to_translate (priv->transform, &dx, &dy);
  else
    dx = dy = 0;

  allocation->x = dx + ceil (margin_rect->origin.x);
  allocation->y = dy + ceil (margin_rect->origin.y);
  allocation->width = ceil (margin_rect->size.width);
  allocation->height = ceil (margin_rect->size.height);
}

/**
 * gtk_widget_contains:
 * @widget: the widget to query
 * @x: X coordinate to test, relative to @widget's origin
 * @y: Y coordinate to test, relative to @widget's origin
 *
 * Tests if the point at (@x, @y) is contained in @widget.
 *
 * The coordinates for (@x, @y) must be in widget coordinates, so
 * (0, 0) is assumed to be the top left of @widget's content area.
 *
 * Returns: %TRUE if @widget contains (@x, @y).
 **/
gboolean
gtk_widget_contains (GtkWidget  *widget,
                     gdouble     x,
                     gdouble     y)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!_gtk_widget_is_drawable (widget))
    return FALSE;

  return GTK_WIDGET_GET_CLASS (widget)->contains (widget, x, y);
}

/**
 * gtk_widget_pick:
 * @widget: the widget to query
 * @x: X coordinate to test, relative to @widget's origin
 * @y: Y coordinate to test, relative to @widget's origin
 * @flags: Flags to influence what is picked
 *
 * Finds the descendant of @widget (including @widget itself) closest
 * to the screen at the point (@x, @y). The point must be given in
 * widget coordinates, so (0, 0) is assumed to be the top left of
 * @widget's content area.
 *
 * Usually widgets will return %NULL if the given coordinate is not
 * contained in @widget checked via gtk_widget_contains(). Otherwise
 * they will recursively try to find a child that does not return %NULL.
 * Widgets are however free to customize their picking algorithm.
 *
 * This function is used on the toplevel to determine the widget below
 * the mouse cursor for purposes of hover hilighting and delivering events.
 *
 * Returns: (nullable) (transfer none): The widget descendant at the given
 *     coordinate or %NULL if none.
 **/
GtkWidget *
gtk_widget_pick (GtkWidget    *widget,
                 gdouble       x,
                 gdouble       y,
                 GtkPickFlags  flags)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkWidget *child;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (!_gtk_widget_is_drawable (widget))
    return NULL;

  if (!(flags & GTK_PICK_NON_TARGETABLE) &&
      !gtk_widget_get_can_target (widget))
    return NULL;

  if (!(flags & GTK_PICK_INSENSITIVE) &&
      !_gtk_widget_is_sensitive (widget))
    return NULL;

  switch (priv->overflow)
    {
    default:
    case GTK_OVERFLOW_VISIBLE:
      break;

    case GTK_OVERFLOW_HIDDEN:
      {
        GtkCssBoxes boxes;

        gtk_css_boxes_init (&boxes, widget);

        if (!gsk_rounded_rect_contains_point (gtk_css_boxes_get_padding_box (&boxes),
                                              &GRAPHENE_POINT_INIT (x, y)))
          return NULL;
      }
      break;
    }

  if (GTK_IS_WINDOW (widget))
    {
      GtkWidget *picked;

      picked = gtk_window_pick_popover (GTK_WINDOW (widget), x, y, flags);
      if (picked)
        return picked;
    }

  for (child = _gtk_widget_get_last_child (widget);
       child;
       child = _gtk_widget_get_prev_sibling (child))
    {
      GtkWidgetPrivate *child_priv = gtk_widget_get_instance_private (child);
      GskTransform *transform;
      graphene_matrix_t inv;
      GtkWidget *picked;
      graphene_point3d_t p0, p1, res;

      if (GTK_IS_NATIVE (child))
        continue;

      if (child_priv->transform)
        {
          transform = gsk_transform_invert (gsk_transform_ref (child_priv->transform));
          if (transform == NULL)
            continue;
        }
      else
        {
          transform = NULL;
        }
      gsk_transform_to_matrix (transform, &inv);
      gsk_transform_unref (transform);
      graphene_point3d_init (&p0, x, y, 0);
      graphene_point3d_init (&p1, x, y, 1);
      graphene_matrix_transform_point3d (&inv, &p0, &p0);
      graphene_matrix_transform_point3d (&inv, &p1, &p1);
      if (fabs (p0.z - p1.z) < 1.f / 4096)
        continue;

      graphene_point3d_interpolate (&p0, &p1, p0.z / (p0.z - p1.z), &res);

      picked = gtk_widget_pick (child, res.x, res.y, flags);
      if (picked)
        return picked;
    }

  if (!gtk_widget_contains (widget, x, y))
    return NULL;

  return widget;
}

/**
 * gtk_widget_compute_transform:
 * @widget: a #GtkWidget
 * @target: the target widget that the matrix will transform to
 * @out_transform: (out caller-allocates): location to
 *   store the final transformation
 *
 * Computes a matrix suitable to describe a transformation from
 * @widget's coordinate system into @target's coordinate system.
 *
 * Returns: %TRUE if the transform could be computed, %FALSE otherwise.
 *   The transform can not be computed in certain cases, for example when
 *   @widget and @target do not share a common ancestor. In that
 *   case @out_transform gets set to the identity matrix.
 */
gboolean
gtk_widget_compute_transform (GtkWidget         *widget,
                              GtkWidget         *target,
                              graphene_matrix_t *out_transform)
{
  GtkWidget *ancestor, *iter;
  graphene_matrix_t transform, inverse, tmp;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (target), FALSE);
  g_return_val_if_fail (out_transform != NULL, FALSE);

  if (widget->priv->root != target->priv->root)
    return FALSE;

  /* optimization for common case: parent wants coordinates of a direct child */
  if (target == widget->priv->parent)
    {
      gsk_transform_to_matrix (widget->priv->transform, out_transform);
      return TRUE;
    }

  ancestor = gtk_widget_common_ancestor (widget, target);
  if (ancestor == NULL)
    {
      graphene_matrix_init_identity (out_transform);
      return FALSE;
    }

  graphene_matrix_init_identity (&transform);
  for (iter = widget; iter != ancestor; iter = iter->priv->parent)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (iter);
      gsk_transform_to_matrix (priv->transform, &tmp);

      graphene_matrix_multiply (&transform, &tmp, &transform);
    }

  /* optimization for common case: parent wants coordinates of a non-direct child */
  if (ancestor == target)
    {
      graphene_matrix_init_from_matrix (out_transform, &transform);
      return TRUE;
    }

  graphene_matrix_init_identity (&inverse);
  for (iter = target; iter != ancestor; iter = iter->priv->parent)
    {
      GtkWidgetPrivate *priv = gtk_widget_get_instance_private (iter);
      gsk_transform_to_matrix (priv->transform, &tmp);

      graphene_matrix_multiply (&inverse, &tmp, &inverse);
    }
  if (!graphene_matrix_inverse (&inverse, &inverse))
    {
      graphene_matrix_init_identity (out_transform);
      return FALSE;
    }

  graphene_matrix_multiply (&transform, &inverse, out_transform);

  return TRUE;
}

/**
 * gtk_widget_compute_bounds:
 * @widget: the #GtkWidget to query
 * @target: the #GtkWidget
 * @out_bounds: (out caller-allocates): the rectangle taking the bounds
 *
 * Computes the bounds for @widget in the coordinate space of @target.
 * FIXME: Explain what "bounds" are.
 *
 * If the operation is successful, %TRUE is returned. If @widget has no
 * bounds or the bounds cannot be expressed in @target's coordinate space
 * (for example if both widgets are in different windows), %FALSE is
 * returned and @bounds is set to the zero rectangle.
 *
 * It is valid for @widget and @target to be the same widget.
 *
 * Returns: %TRUE if the bounds could be computed
 **/
gboolean
gtk_widget_compute_bounds (GtkWidget       *widget,
                           GtkWidget       *target,
                           graphene_rect_t *out_bounds)
{
  graphene_matrix_t transform;
  GtkCssBoxes boxes;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (target), FALSE);
  g_return_val_if_fail (out_bounds != NULL, FALSE);

  if (!gtk_widget_compute_transform (widget, target, &transform))
    {
      graphene_rect_init_from_rect (out_bounds, graphene_rect_zero ());
      return FALSE;
    }

  gtk_css_boxes_init (&boxes, widget);
  graphene_matrix_transform_bounds (&transform,
                                    gtk_css_boxes_get_border_rect (&boxes),
                                    out_bounds);

  return TRUE;
}

/**
 * gtk_widget_get_allocated_width:
 * @widget: the widget to query
 *
 * Returns the width that has currently been allocated to @widget.
 *
 * Returns: the width of the @widget
 **/
int
gtk_widget_get_allocated_width (GtkWidget *widget)
{
  GtkCssBoxes boxes;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  gtk_css_boxes_init (&boxes, widget);

  return gtk_css_boxes_get_margin_rect (&boxes)->size.width;
}

/**
 * gtk_widget_get_allocated_height:
 * @widget: the widget to query
 *
 * Returns the height that has currently been allocated to @widget.
 *
 * Returns: the height of the @widget
 **/
int
gtk_widget_get_allocated_height (GtkWidget *widget)
{
  GtkCssBoxes boxes;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  gtk_css_boxes_init (&boxes, widget);

  return gtk_css_boxes_get_margin_rect (&boxes)->size.height;
}

/**
 * gtk_widget_get_allocated_baseline:
 * @widget: the widget to query
 *
 * Returns the baseline that has currently been allocated to @widget.
 * This function is intended to be used when implementing handlers
 * for the #GtkWidget::snapshot function, and when allocating child
 * widgets in #GtkWidget::size_allocate.
 *
 * Returns: the baseline of the @widget, or -1 if none
 **/
int
gtk_widget_get_allocated_baseline (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssStyle *style;
  GtkBorder margin, border, padding;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  if (priv->baseline == -1)
    return -1;

  style = gtk_css_node_get_style (priv->cssnode);
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  return priv->baseline - margin.top - border.top - padding.top;
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return priv->multidevice;
}

/**
 * gtk_widget_set_support_multidevice:
 * @widget: a #GtkWidget
 * @support_multidevice: %TRUE to support input from multiple devices.
 *
 * Enables or disables multiple pointer awareness. If this setting is %TRUE,
 * @widget will start receiving multiple, per device enter/leave events. Note
 * that if custom #GdkSurfaces are created in #GtkWidget::realize,
 * gdk_surface_set_support_multidevice() will have to be called manually on them.
 **/
void
gtk_widget_set_support_multidevice (GtkWidget *widget,
                                    gboolean   support_multidevice)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv->multidevice = (support_multidevice == TRUE);

  if (_gtk_widget_get_realized (widget))
    gdk_surface_set_support_multidevice (priv->surface, support_multidevice);
}

/* There are multiple alpha related sources. First of all the user can specify alpha
 * in gtk_widget_set_opacity, secondly we can get it from the CSS opacity. These two
 * are multiplied together to form the total alpha. Secondly, the user can specify
 * an opacity group for a widget, which means we must essentially handle it as having alpha.
 */

static void
gtk_widget_update_alpha (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkStyleContext *context;
  gdouble opacity;
  guint8 alpha;

  context = _gtk_widget_get_style_context (widget);
  opacity =
    _gtk_css_number_value_get (_gtk_style_context_peek_property (context,
                                                                 GTK_CSS_PROPERTY_OPACITY),
                               100);
  opacity = CLAMP (opacity, 0.0, 1.0);
  alpha = round (priv->user_alpha * opacity);

  if (alpha == priv->alpha)
    return;

  priv->alpha = alpha;

  if (_gtk_widget_get_realized (widget))
    {
      if (GTK_IS_NATIVE (widget))
	gdk_surface_set_opacity (priv->surface, priv->alpha / 255.0);

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
 * system. On X11 this has any effect only on X displays with a compositing manager
 * running. See gdk_display_is_composited(). On Windows it should work
 * always, although setting a window’s opacity after the window has been
 * shown causes it to flicker once on Windows.
 *
 * For child widgets it doesn’t work if any affected widget has a native window.
 **/
void
gtk_widget_set_opacity (GtkWidget *widget,
                        gdouble    opacity)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  guint8 alpha;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  opacity = CLAMP (opacity, 0.0, 1.0);

  alpha = round (opacity * 255);

  if (alpha == priv->user_alpha)
    return;

  priv->user_alpha = alpha;

  gtk_widget_update_alpha (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_OPACITY]);
}

/**
 * gtk_widget_get_opacity:
 * @widget: a #GtkWidget
 *
 * Fetches the requested opacity for this widget.
 * See gtk_widget_set_opacity().
 *
 * Returns: the requested opacity for this widget.
 **/
gdouble
gtk_widget_get_opacity (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0.0);

  return priv->user_alpha / 255.0;
}

/**
 * gtk_widget_set_overflow:
 * @widget: a #GtkWidget
 * @overflow: desired overflow
 *
 * Sets how @widget treats content that is drawn outside the widget's content area.
 * See the definition of #GtkOverflow for details.
 *
 * This setting is provided for widget implementations and should not be used by
 * application code.
 *
 * The default value is %GTK_OVERFLOW_VISIBLE.
 **/
void
gtk_widget_set_overflow (GtkWidget   *widget,
                         GtkOverflow  overflow)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (priv->overflow == overflow)
    return;

  priv->overflow = overflow;

  gtk_widget_queue_draw (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_OVERFLOW]);
}

/**
 * gtk_widget_get_overflow:
 * @widget: a #GtkWidget
 *
 * Returns the value set via gtk_widget_set_overflow().
 *
 * Returns: The widget's overflow.
 **/
GtkOverflow
gtk_widget_get_overflow (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_OVERFLOW_VISIBLE);

  return priv->overflow;
}

void
gtk_widget_set_has_focus (GtkWidget *widget,
                          gboolean   has_focus)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->has_focus == has_focus)
    return;

  priv->has_focus = has_focus;
  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_HAS_FOCUS]);
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
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->in_destruction;
}

gboolean
_gtk_widget_get_shadowed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->shadowed;
}

void
_gtk_widget_set_shadowed (GtkWidget *widget,
                          gboolean   shadowed)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->shadowed = shadowed;
}

gboolean
_gtk_widget_get_alloc_needed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->alloc_needed;
}

static void
gtk_widget_set_alloc_needed (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->alloc_needed = TRUE;

  do
    {
      if (priv->alloc_needed_on_child)
        break;

      priv->alloc_needed_on_child = TRUE;

      if (!priv->visible)
        break;

      if (GTK_IS_ROOT (widget))
        {
          gtk_container_start_idle_sizer (GTK_CONTAINER (widget));
          break;
        }

      widget = priv->parent;
      if (widget == NULL)
        break;

      priv = widget->priv;
    }
  while (TRUE);
}

gboolean
gtk_widget_needs_allocate (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->visible || !priv->child_visible)
    return FALSE;

  if (priv->resize_needed || priv->alloc_needed || priv->alloc_needed_on_child)
    return TRUE;

  return FALSE;
}

void
gtk_widget_ensure_allocate (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!gtk_widget_needs_allocate (widget))
    return;

  gtk_widget_ensure_resize (widget);

  /*  This code assumes that we only reach here if the previous
   *  allocation is still valid (ie no resize was queued).
   *  If that wasn't true, the parent would have taken care of
   *  things.
   */
  if (priv->alloc_needed)
    {
      gtk_widget_allocate (widget,
                           priv->allocated_width,
                           priv->allocated_height,
                           priv->allocated_size_baseline,
                           gsk_transform_ref (priv->allocated_transform));
    }
  else if (priv->alloc_needed_on_child)
    {
      GtkWidget *child;

      priv->alloc_needed_on_child = FALSE;

      for (child = _gtk_widget_get_first_child (widget);
           child != NULL;
           child = _gtk_widget_get_next_sibling (child))
        {
          gtk_widget_ensure_allocate (child);
        }
    }
}

void
gtk_widget_ensure_resize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!priv->resize_needed)
    return;

  priv->resize_needed = FALSE;
  _gtk_size_request_cache_clear (&priv->requests);
}

void
_gtk_widget_add_sizegroup (GtkWidget    *widget,
			   gpointer      group)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_prepend (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  priv->have_size_groups = TRUE;
}

void
_gtk_widget_remove_sizegroup (GtkWidget    *widget,
			      gpointer      group)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_remove (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  priv->have_size_groups = groups != NULL;
}

GSList *
_gtk_widget_get_sizegroups (GtkWidget    *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (priv->have_size_groups)
    return g_object_get_qdata (G_OBJECT (widget), quark_size_groups);

  return NULL;
}

void
_gtk_widget_add_attached_window (GtkWidget    *widget,
                                 GtkWindow    *window)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->attached_windows = g_list_prepend (priv->attached_windows, window);
}

void
_gtk_widget_remove_attached_window (GtkWidget    *widget,
                                    GtkWindow    *window)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->attached_windows = g_list_remove (priv->attached_windows, window);
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
 */
gint
gtk_widget_path_append_for_widget (GtkWidgetPath *path,
                                   GtkWidget     *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  const GQuark *classes;
  guint n_classes, i;
  gint pos;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  pos = gtk_widget_path_append_type (path, gtk_css_node_get_widget_type (priv->cssnode));
  gtk_widget_path_iter_set_object_name (path, pos, gtk_css_node_get_name (priv->cssnode));

  if (priv->name)
    gtk_widget_path_iter_set_name (path, pos, priv->name);

  gtk_widget_path_iter_set_state (path, pos, priv->state_flags);

  classes = gtk_css_node_list_classes (priv->cssnode, &n_classes);

  for (i = n_classes; i-- > 0;)
    gtk_widget_path_iter_add_qclass (path, pos, classes[i]);

  return pos;
}

GtkWidgetPath *
_gtk_widget_create_path (GtkWidget *widget)
{
  GtkWidget *parent = _gtk_widget_get_parent (widget);

  if (parent && GTK_IS_CONTAINER (parent))
    return gtk_container_get_path_for_child (GTK_CONTAINER (parent), widget);
  else if (parent)
    {
      GtkWidgetPath *path = _gtk_widget_create_path (parent);
      gtk_widget_path_append_for_widget (path, widget);
      return path;
    }
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
  GtkWidgetPath *path;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  path = (GtkWidgetPath*)g_object_get_qdata (G_OBJECT (widget), quark_widget_path);
  if (!path)
    {
      path = _gtk_widget_create_path (widget);
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_widget_path,
                               path,
                               (GDestroyNotify)gtk_widget_path_free);
    }

  return path;
}

void
gtk_widget_clear_path (GtkWidget *widget)
{
  g_object_set_qdata (G_OBJECT (widget), quark_widget_path, NULL);
}

/**
 * gtk_widget_class_set_css_name:
 * @widget_class: class to set the name on
 * @name: name to use
 *
 * Sets the name to be used for CSS matching of widgets.
 *
 * If this function is not called for a given class, the name
 * of the parent class is used.
 */
void
gtk_widget_class_set_css_name (GtkWidgetClass *widget_class,
                               const char     *name)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (name != NULL);

  priv = widget_class->priv;

  priv->css_name = g_intern_string (name);
}

static gboolean
gtk_widget_class_get_visible_by_default (GtkWidgetClass *widget_class)
{
  return !g_type_is_a (G_TYPE_FROM_CLASS (widget_class), GTK_TYPE_NATIVE);
}

/**
 * gtk_widget_class_get_css_name:
 * @widget_class: class to set the name on
 *
 * Gets the name used by this class for matching in CSS code. See
 * gtk_widget_class_set_css_name() for details.
 *
 * Returns: the CSS name of the given class
 */
const char *
gtk_widget_class_get_css_name (GtkWidgetClass *widget_class)
{
  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), NULL);

  return widget_class->priv->css_name;
}

void
_gtk_widget_style_context_invalidated (GtkWidget *widget)
{
  g_signal_emit (widget, widget_signals[STYLE_UPDATED], 0);
}

GtkCssNode *
gtk_widget_get_css_node (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->cssnode;
}

GtkStyleContext *
_gtk_widget_peek_style_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->context;
}


/**
 * gtk_widget_get_style_context:
 * @widget: a #GtkWidget
 *
 * Returns the style context associated to @widget. The returned object is
 * guaranteed to be the same for the lifetime of @widget.
 *
 * Returns: (transfer none): a #GtkStyleContext. This memory is owned by @widget and
 *          must not be freed.
 **/
GtkStyleContext *
gtk_widget_get_style_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (G_UNLIKELY (priv->context == NULL))
    {
      GdkDisplay *display;

      priv->context = gtk_style_context_new_for_node (priv->cssnode);

      gtk_style_context_set_id (priv->context, priv->name);
      gtk_style_context_set_state (priv->context, priv->state_flags);
      gtk_style_context_set_scale (priv->context, gtk_widget_get_scale_factor (widget));

      display = _gtk_widget_get_display (widget);
      if (display)
        gtk_style_context_set_display (priv->context, display);

      if (priv->parent)
        gtk_style_context_set_parent (priv->context,
                                      _gtk_widget_get_style_context (priv->parent));
    }

  return priv->context;
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
 **/
GdkModifierType
gtk_widget_get_modifier_mask (GtkWidget         *widget,
                              GdkModifierIntent  intent)
{
  GdkDisplay *display;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  display = _gtk_widget_get_display (widget);

  return gdk_keymap_get_modifier_mask (gdk_display_get_keymap (display),
                                       intent);
}

static GtkActionMuxer *
gtk_widget_get_parent_muxer (GtkWidget *widget,
                              gboolean   create)
{
  GtkWidget *parent;

  if (GTK_IS_WINDOW (widget))
    return gtk_application_get_parent_muxer_for_window (GTK_WINDOW (widget));

  if (GTK_IS_MENU (widget))
    parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
  else
    parent = _gtk_widget_get_parent (widget);

  if (parent)
    return _gtk_widget_get_action_muxer (parent, create);

  return NULL;
}

void
_gtk_widget_update_parent_muxer (GtkWidget *widget)
{
  GtkActionMuxer *muxer;

  muxer = (GtkActionMuxer*)g_object_get_qdata (G_OBJECT (widget), quark_action_muxer);
  if (muxer == NULL)
    return;

  gtk_action_muxer_set_parent (muxer,
                               gtk_widget_get_parent_muxer (widget, TRUE));
}

GtkActionMuxer *
_gtk_widget_get_action_muxer (GtkWidget *widget,
                              gboolean   create)
{
  GtkActionMuxer *muxer;
  GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS (widget);
  GtkWidgetClassPrivate *priv = widget_class->priv;

  muxer = (GtkActionMuxer*)g_object_get_qdata (G_OBJECT (widget), quark_action_muxer);
  if (muxer)
    return muxer;

  if (create || priv->actions)
    {
      muxer = gtk_action_muxer_new (widget, priv->actions);
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_action_muxer,
                               muxer,
                               g_object_unref);
      _gtk_widget_update_parent_muxer (widget);

      return muxer;
    }
  else
    return gtk_widget_get_parent_muxer (widget, FALSE);
}

/**
 * gtk_widget_insert_action_group:
 * @widget: a #GtkWidget
 * @name: the prefix for actions in @group
 * @group: (allow-none): a #GActionGroup, or %NULL
 *
 * Inserts @group into @widget. Children of @widget that implement
 * #GtkActionable can then be associated with actions in @group by
 * setting their “action-name” to @prefix.`action-name`.
 *
 * Note that inheritance is defined for individual actions. I.e.
 * even if you insert a group with prefix @prefix, actions with
 * the same prefix will still be inherited from the parent, unless
 * the group contains an action with the same name.
 *
 * If @group is %NULL, a previously inserted group for @name is
 * removed from @widget.
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
  GHashTable *auto_children;
  GHashTable *auto_child_hash;

  auto_children = (GHashTable *)g_object_get_qdata (G_OBJECT (widget), quark_auto_children);
  if (auto_children == NULL)
    {
      if (!create)
        return NULL;

      auto_children = g_hash_table_new_full (g_direct_hash,
                                             NULL,
			                     NULL, (GDestroyNotify)g_hash_table_destroy);
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_auto_children,
                               auto_children,
                               (GDestroyNotify)g_hash_table_destroy);
    }

  auto_child_hash =
    g_hash_table_lookup (auto_children, GSIZE_TO_POINTER (type));

  if (!auto_child_hash && create)
    {
      auto_child_hash = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       NULL,
					       (GDestroyNotify)g_object_unref);

      g_hash_table_insert (auto_children,
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
  if (!gtk_builder_extend_with_template  (builder, widget, class_type,
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
 * For use in language bindings, this will override the default #GtkBuilderConnectFunc to be
 * used when parsing GtkBuilder XML from this class’s template data.
 *
 * Note that this must be called from a composite widget classes class
 * initializer after calling gtk_widget_class_set_template().
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
 * gtk_widget_activate_action:
 * @widget: a #GtkWidget
 * @name: a prefixed action name
 * @parameter: parameters that required by the action
 *
 * Looks up the action in the action groups associated
 * with @widget and its ancestors, and activates it.
 *
 * The action name is expected to be prefixed with the
 * prefix that was used when adding the action group
 * with gtk_widget_insert_action_group().
 *
 * The @parameter must match the actions expected parameter
 * type, as returned by g_action_get_parameter_type().
 */
void
gtk_widget_activate_action (GtkWidget  *widget,
                            const char *name,
                            GVariant   *parameter)
{
  GtkActionMuxer *muxer;

  muxer = _gtk_widget_get_action_muxer (widget, FALSE);
  if (muxer)
    g_action_group_activate_action (G_ACTION_GROUP (muxer),
                                    name,
                                    parameter);
}

/**
 * gtk_widget_activate_default:
 * @widget: a #GtkWidget
 *
 * Activate the default.activate action from @widget.
 */
void
gtk_widget_activate_default (GtkWidget *widget)
{
  gtk_widget_activate_action (widget, "default.activate", NULL);
}

void
gtk_widget_cancel_event_sequence (GtkWidget             *widget,
                                  GtkGesture            *gesture,
                                  GdkEventSequence      *sequence,
                                  GtkEventSequenceState  state)
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

  event_widget = gtk_get_event_target ((GdkEvent *) event);

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

      event_widget = _gtk_widget_get_parent (event_widget);
    }

}

/**
 * gtk_widget_add_controller:
 * @widget: a #GtkWidget
 * @controller: (transfer full): a #GtkEventController that hasn't been
 *     added to a widget yet
 *
 * Adds @controller to @widget so that it will receive events. You will
 * usually want to call this function right after creating any kind of
 * #GtkEventController.
 **/
void
gtk_widget_add_controller (GtkWidget          *widget,
                           GtkEventController *controller)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (gtk_event_controller_get_widget (controller) == NULL);

  GTK_EVENT_CONTROLLER_GET_CLASS (controller)->set_widget (controller, widget);

  priv->event_controllers = g_list_prepend (priv->event_controllers, controller);

  if (priv->controller_observer)
    gtk_list_list_model_item_added_at (priv->controller_observer, 0);
}

/**
 * gtk_widget_remove_controller:
 * @widget: a #GtkWidget
 * @controller: (transfer none): a #GtkEventController
 *
 * Removes @controller from @widget, so that it doesn't process
 * events anymore. It should not be used again.
 *
 * Widgets will remove all event controllers automatically when they
 * are destroyed, there is normally no need to call this function.
 **/
void
gtk_widget_remove_controller (GtkWidget          *widget,
                              GtkEventController *controller)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *before, *list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (gtk_event_controller_get_widget (controller) == widget);

  GTK_EVENT_CONTROLLER_GET_CLASS (controller)->unset_widget (controller);

  list = g_list_find (priv->event_controllers, controller);
  before = list->prev;
  priv->event_controllers = g_list_delete_link (priv->event_controllers, list);
  g_object_unref (controller);

  if (priv->controller_observer)
    gtk_list_list_model_item_removed (priv->controller_observer, before);
}

GList *
_gtk_widget_list_controllers (GtkWidget           *widget,
                              GtkPropagationPhase  phase)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l, *retval = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;

      if (controller != NULL &&
          phase == gtk_event_controller_get_propagation_phase (controller))
        retval = g_list_prepend (retval, controller);
    }

  return retval;
}

gboolean
_gtk_widget_consumes_motion (GtkWidget        *widget,
                             GdkEventSequence *sequence)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  while (widget != NULL && !GTK_IS_WINDOW (widget))
    {
      GList *l;

      for (l = priv->event_controllers; l; l = l->next)
        {
          GtkEventController *controller = l->data;

          if (controller == NULL ||
              !GTK_IS_GESTURE (controller))
            continue;

          if ((!GTK_IS_GESTURE_SINGLE (controller) ||
               GTK_IS_GESTURE_DRAG (controller) ||
               GTK_IS_GESTURE_SWIPE (controller)) &&
              gtk_gesture_handles_sequence (GTK_GESTURE (controller), sequence))
            return TRUE;
        }

      widget = priv->parent;
      priv = gtk_widget_get_instance_private (widget);
    }

  return FALSE;
}

void
gtk_widget_reset_controllers (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GList *l;

  /* Reset all controllers */
  for (l = priv->event_controllers; l; l = l->next)
    {
      GtkEventController *controller = l->data;

      if (controller == NULL)
        continue;

      gtk_event_controller_reset (controller);
    }
}

static inline void
gtk_widget_maybe_add_debug_render_nodes (GtkWidget   *widget,
                                         GtkSnapshot *snapshot)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GdkDisplay *display = _gtk_widget_get_display (widget);

  if (GTK_DISPLAY_DEBUG_CHECK (display, BASELINES))
    {
      if (priv->baseline != -1)
        {
          GdkRGBA red = {1, 0, 0, 1};
          graphene_rect_t bounds;

          /* Baselines are relative to the widget's origin,
           * and we are offset to the widget's allocation here */
          graphene_rect_init (&bounds,
                              0, priv->baseline,
                              priv->width, 1);
          gtk_snapshot_append_color (snapshot,
                                     &red,
                                     &bounds);
        }
    }

#ifdef G_ENABLE_DEBUG
  if (GTK_DISPLAY_DEBUG_CHECK (display, RESIZE) && priv->highlight_resize)
    {
      GdkRGBA blue = {0, 0, 1, 0.2};
      graphene_rect_t bounds;

      graphene_rect_init (&bounds,
                          0, 0,
                          priv->width, priv->height);

      gtk_snapshot_append_color (snapshot,
                                 &blue,
                                 &bounds);
      priv->highlight_resize = FALSE;
      gtk_widget_queue_draw (widget);
    }
#endif
}

static GskRenderNode *
gtk_widget_create_render_node (GtkWidget   *widget,
                               GtkSnapshot *parent_snapshot)
{
  GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (widget);
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkCssBoxes boxes;
  GtkCssValue *filter_value;
  double opacity;
  GtkSnapshot *snapshot;

  opacity = priv->alpha / 255.0;
  if (opacity <= 0.0)
    return NULL;

  gtk_css_boxes_init (&boxes, widget);
  snapshot = gtk_snapshot_new_with_parent (parent_snapshot);

  gtk_snapshot_push_debug (snapshot,
                           "RenderNode for %s %p",
                           G_OBJECT_TYPE_NAME (widget), widget);

  filter_value = _gtk_style_context_peek_property (_gtk_widget_get_style_context (widget), GTK_CSS_PROPERTY_FILTER);
  gtk_css_filter_value_push_snapshot (filter_value, snapshot);

  if (opacity < 1.0)
    gtk_snapshot_push_opacity (snapshot, opacity);

  if (!GTK_IS_WINDOW (widget))
    {
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }

  if (priv->overflow == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_push_rounded_clip (snapshot, gtk_css_boxes_get_padding_box (&boxes));

  klass->snapshot (widget, snapshot);

  if (priv->overflow == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_pop (snapshot);

  gtk_css_style_snapshot_outline (&boxes, snapshot);

  if (opacity < 1.0)
    gtk_snapshot_pop (snapshot);

  gtk_css_filter_value_pop_snapshot (filter_value, snapshot);

#ifdef G_ENABLE_DEBUG
  gtk_widget_maybe_add_debug_render_nodes (widget, snapshot);
#endif

  gtk_snapshot_pop (snapshot);

  return gtk_snapshot_free_to_node (snapshot);
}

void
gtk_widget_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  if (!_gtk_widget_is_drawable (widget))
    return;

  if (_gtk_widget_get_alloc_needed (widget))
    {
      g_warning ("Trying to snapshot %s %p without a current allocation", gtk_widget_get_name (widget), widget);
      return;
    }

  if (priv->draw_needed)
    {
      GskRenderNode *render_node;

      gtk_widget_push_paintables (widget);

      render_node = gtk_widget_create_render_node (widget, snapshot);
      /* This can happen when nested drawing happens and a widget contains itself
       * or when we replace a clipped area */
      g_clear_pointer (&priv->render_node, gsk_render_node_unref);
      priv->render_node = render_node;

      priv->draw_needed = FALSE;

      gtk_widget_pop_paintables (widget);
      gtk_widget_update_paintables (widget);
    }

  if (priv->render_node)
    gtk_snapshot_append_node (snapshot, priv->render_node);
}

void
gtk_widget_render (GtkWidget            *widget,
                   GdkSurface           *surface,
                   const cairo_region_t *region)
{
  GtkSnapshot *snapshot;
  GskRenderer *renderer;
  GskRenderNode *root;
  int x, y;

  if (!GTK_IS_NATIVE (widget))
    return;

  renderer = gtk_native_get_renderer (GTK_NATIVE (widget));
  if (renderer == NULL)
    return;

  snapshot = gtk_snapshot_new ();
  gtk_native_get_surface_transform (GTK_NATIVE (widget), &x, &y);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gtk_widget_snapshot (widget, snapshot);
  root = gtk_snapshot_free_to_node (snapshot);

  if (root != NULL)
    {
      root = gtk_inspector_prepare_render (widget,
                                           renderer,
                                           surface,
                                           region,
                                           root);

      gsk_renderer_render (renderer, root, region);

      gsk_render_node_unref (root);
    }
}

static void
gtk_widget_child_observer_destroyed (gpointer widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->children_observer = NULL;
}

/**
 * gtk_widget_observe_children:
 * @widget: a #GtkWidget
 *
 * Returns a #GListModel to track the children of @widget. 
 *
 * Calling this function will enable extra internal bookkeeping to track
 * children and emit signals on the returned listmodel. It may slow down
 * operations a lot.
 * 
 * Applications should try hard to avoid calling this function because of
 * the slowdowns.
 *
 * Returns: (transfer full): a #GListModel tracking @widget's children
 **/
GListModel *
gtk_widget_observe_children (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->children_observer)
    return g_object_ref (G_LIST_MODEL (priv->children_observer));

  priv->children_observer = gtk_list_list_model_new (GTK_TYPE_WIDGET,
                                                     (gpointer) gtk_widget_get_first_child,
                                                     (gpointer) gtk_widget_get_next_sibling,
                                                     (gpointer) gtk_widget_get_prev_sibling,
                                                     (gpointer) gtk_widget_get_last_child,
                                                     (gpointer) g_object_ref,
                                                     widget,
                                                     gtk_widget_child_observer_destroyed);

  return G_LIST_MODEL (priv->children_observer);
}

static void
gtk_widget_controller_observer_destroyed (gpointer widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  priv->controller_observer = NULL;
}

static gpointer
gtk_widget_controller_list_get_first (gpointer widget)
{
  return GTK_WIDGET (widget)->priv->event_controllers;
}

static gpointer
gtk_widget_controller_list_get_next (gpointer item,
                                     gpointer widget)
{
  return g_list_next (item);
}

static gpointer
gtk_widget_controller_list_get_prev (gpointer item,
                                     gpointer widget)
{
  return g_list_previous (item);
}

static gpointer
gtk_widget_controller_list_get_item (gpointer item,
                                     gpointer widget)
{
  return g_object_ref (((GList *) item)->data);
}

/**
 * gtk_widget_observe_controllers:
 * @widget: a #GtkWidget
 *
 * Returns a #GListModel to track the #GtkEventControllers of @widget. 
 *
 * Calling this function will enable extra internal bookkeeping to track
 * controllers and emit signals on the returned listmodel. It may slow down
 * operations a lot.
 * 
 * Applications should try hard to avoid calling this function because of
 * the slowdowns.
 *
 * Returns: (transfer full): a #GListModel tracking @widget's controllers
 **/
GListModel *
gtk_widget_observe_controllers (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (priv->controller_observer)
    return g_object_ref (G_LIST_MODEL (priv->controller_observer));

  priv->controller_observer = gtk_list_list_model_new (GTK_TYPE_EVENT_CONTROLLER,
                                                      gtk_widget_controller_list_get_first,
                                                      gtk_widget_controller_list_get_next,
                                                      gtk_widget_controller_list_get_prev,
                                                      NULL,
                                                      gtk_widget_controller_list_get_item,
                                                      widget,
                                                      gtk_widget_controller_observer_destroyed);

  return G_LIST_MODEL (priv->controller_observer);
}

/**
 * gtk_widget_get_first_child:
 * @widget: a #GtkWidget
 *
 * Returns: (transfer none) (nullable): The widget's first child
 */
GtkWidget *
gtk_widget_get_first_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->first_child;
}

/**
 * gtk_widget_get_last_child:
 * @widget: a #GtkWidget
 *
 * Returns: (transfer none) (nullable): The widget's last child
 */
GtkWidget *
gtk_widget_get_last_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->last_child;
}

/**
 * gtk_widget_get_next_sibling:
 * @widget: a #GtkWidget
 *
 * Returns: (transfer none) (nullable): The widget's next sibling
 */
GtkWidget *
gtk_widget_get_next_sibling (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->next_sibling;
}

/**
 * gtk_widget_get_prev_sibling:
 * @widget: a #GtkWidget
 *
 * Returns: (transfer none) (nullable): The widget's previous sibling
 */
GtkWidget *
gtk_widget_get_prev_sibling (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->prev_sibling;
}

/**
 * gtk_widget_insert_after:
 * @widget: a #GtkWidget
 * @parent: the parent #GtkWidget to insert @widget into
 * @previous_sibling: (nullable): the new previous sibling of @widget or %NULL
 *
 * Inserts @widget into the child widget list of @parent.
 * It will be placed after @previous_sibling, or at the beginning if @previous_sibling is %NULL.
 *
 * After calling this function, gtk_widget_get_prev_sibling(widget) will return @previous_sibling.
 *
 * If @parent is already set as the parent widget of @widget, this function can also be used
 * to reorder @widget in the child widget list of @parent.
 */
void
gtk_widget_insert_after (GtkWidget *widget,
                         GtkWidget *parent,
                         GtkWidget *previous_sibling)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (previous_sibling == NULL || GTK_IS_WIDGET (previous_sibling));
  g_return_if_fail (previous_sibling == NULL || _gtk_widget_get_parent (previous_sibling) == parent);

  if (widget == previous_sibling ||
      (previous_sibling && _gtk_widget_get_prev_sibling (widget) == previous_sibling))
    return;

  if (!previous_sibling && _gtk_widget_get_first_child (parent) == widget)
    return;

  gtk_widget_reposition_after (widget,
                               parent,
                               previous_sibling);
}

/**
 * gtk_widget_insert_before:
 * @widget: a #GtkWidget
 * @parent: the parent #GtkWidget to insert @widget into
 * @next_sibling: (nullable): the new next sibling of @widget or %NULL
 *
 * Inserts @widget into the child widget list of @parent.
 * It will be placed before @next_sibling, or at the end if @next_sibling is %NULL.
 *
 * After calling this function, gtk_widget_get_next_sibling(widget) will return @next_sibling.
 *
 * If @parent is already set as the parent widget of @widget, this function can also be used
 * to reorder @widget in the child widget list of @parent.
 */
void
gtk_widget_insert_before (GtkWidget *widget,
                          GtkWidget *parent,
                          GtkWidget *next_sibling)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (next_sibling == NULL || GTK_IS_WIDGET (next_sibling));
  g_return_if_fail (next_sibling == NULL || _gtk_widget_get_parent (next_sibling) == parent);

  if (widget == next_sibling ||
      (next_sibling && _gtk_widget_get_next_sibling (widget) == next_sibling))
    return;

  if (!next_sibling && _gtk_widget_get_last_child (parent) == widget)
    return;

  gtk_widget_reposition_after (widget, parent,
                               next_sibling ? _gtk_widget_get_prev_sibling (next_sibling) :
                                              _gtk_widget_get_last_child (parent));
}

void
gtk_widget_forall (GtkWidget   *widget,
                   GtkCallback  callback,
                   gpointer     user_data)
{
  GtkWidget *child;
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child = gtk_widget_get_first_child (widget);
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      callback(child, user_data);

      child = next;
    }
}

/**
 * gtk_widget_snapshot_child:
 * @widget: a #GtkWidget
 * @child: a child of @widget
 * @snapshot: #GtkSnapshot as passed to the widget. In particular, no
 *   calls to gtk_snapshot_translate() or other transform calls should
 *   have been made.
 *
 * When a widget receives a call to the snapshot function, it must send
 * synthetic #GtkWidget::snapshot calls to all children. This function
 * provides a convenient way of doing this. A widget, when it receives
 * a call to its #GtkWidget::snapshot function, calls
 * gtk_widget_snapshot_child() once for each child, passing in
 * the @snapshot the widget received.
 *
 * gtk_widget_snapshot_child() takes care of translating the origin of
 * @snapshot, and deciding whether the child needs to be snapshot.
 *
 * This function does nothing for children that implement #GtkNative.
 **/
void
gtk_widget_snapshot_child (GtkWidget   *widget,
                           GtkWidget   *child,
                           GtkSnapshot *snapshot)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (child);

  g_return_if_fail (_gtk_widget_get_parent (child) == widget);
  g_return_if_fail (snapshot != NULL);

  if (GTK_IS_NATIVE (child))
    return;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_transform (snapshot, priv->transform);

  gtk_widget_snapshot (child, snapshot);

  gtk_snapshot_restore (snapshot);
}

/**
 * gtk_widget_set_focus_child:
 * @widget: a #GtkWidget
 * @child: (nullable): a direct child widget of @widget or %NULL
 *   to unset the focus child of @widget
 *
 * Set @child as the current focus child of @widget. The previous
 * focus child will be unset.
 *
 * This function is only suitable for widget implementations.
 * If you want a certain widget to get the input focus, call
 * gtk_widget_grab_focus() on it.
*/
void
gtk_widget_set_focus_child (GtkWidget *widget,
                            GtkWidget *child)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (child != NULL)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));
      g_return_if_fail (gtk_widget_get_parent (child) == widget);
    }

  g_set_object (&priv->focus_child, child);
}

/**
 * gtk_widget_get_focus_child:
 * @widget: a #GtkWidget
 *
 * Returns the current focus child of @widget.
 *
 * Returns: (nullable) (transfer none): The current focus child of @widget,
 *   or %NULL in case the focus child is unset.
 */
GtkWidget *
gtk_widget_get_focus_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->focus_child;
}

/**
 * gtk_widget_set_cursor:
 * @widget: a #GtkWidget
 * @cursor: (allow-none): the new cursor or %NULL to use the default
 *     cursor
 *
 * Sets the cursor to be shown when pointer devices point towards @widget.
 *
 * If the @cursor is NULL, @widget will use the cursor inherited from the
 * parent widget.
 **/
void
gtk_widget_set_cursor (GtkWidget *widget,
                       GdkCursor *cursor)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);
  GtkRoot *root;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cursor == NULL || GDK_IS_CURSOR (cursor));

  if (!g_set_object (&priv->cursor, cursor))
    return;

  root = _gtk_widget_get_root (widget);
  if (root)
    gtk_window_maybe_update_cursor (GTK_WINDOW (root), widget, NULL);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CURSOR]);
}

/**
 * gtk_widget_set_cursor_from_name:
 * @widget: a #GtkWidget
 * @name: (nullable): The name of the cursor or %NULL to use the default
 *     cursor
 *
 * Sets a named cursor to be shown when pointer devices point towards @widget.
 *
 * This is a utility function that creates a cursor via
 * gdk_cursor_new_from_name() and then sets it on @widget with
 * gtk_widget_set_cursor(). See those 2 functions for details.
 *
 * On top of that, this function allows @name to be %NULL, which will
 * do the same as calling gtk_widget_set_cursor() with a %NULL cursor.
 **/
void
gtk_widget_set_cursor_from_name (GtkWidget  *widget,
                                 const char *name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (name)
    {
      GdkCursor *cursor;

      cursor = gdk_cursor_new_from_name (name, NULL);
      gtk_widget_set_cursor (widget, cursor);
      g_object_unref (cursor);
    }
  else
    {
      gtk_widget_set_cursor (widget, NULL);
    }
}

/**
 * gtk_widget_get_cursor:
 * @widget: a #GtkWidget
 *
 * Queries the cursor set via gtk_widget_set_cursor(). See that function for
 * details.
 *
 * Returns: (nullable) (transfer none): the cursor currently in use or %NULL
 *     to use the default.
 **/
GdkCursor *
gtk_widget_get_cursor (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->cursor;
}

/**
 * gtk_widget_set_can_target:
 * @widget: a #GtkWidget
 * @can_target: whether this widget should be able to receive pointer events
 *
 * Sets whether @widget can be the target of pointer events.
 */
void
gtk_widget_set_can_target (GtkWidget *widget,
                           gboolean   can_target)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  can_target = !!can_target;

  if (priv->can_target == can_target)
    return;

  priv->can_target = can_target;

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_CAN_TARGET]);
}

/**
 * gtk_widget_get_can_target:
 * @widget: a #GtkWidget
 * 
 * Queries whether @widget can be the target of pointer events.
 * 
 * Returns: %TRUE if @widget can receive pointer events
 */
gboolean
gtk_widget_get_can_target (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  return priv->can_target;
}

/**
 * gtk_widget_get_width:
 * @widget: a #GtkWidget
 *
 * Returns the content width of the widget, as passed to its size-allocate implementation.
 * This is the size you should be using in GtkWidgetClass.snapshot(). For pointer
 * events, see gtk_widget_contains().
 *
 * Returns: The width of @widget
 */
int
gtk_widget_get_width (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->width;
}

/**
 * gtk_widget_get_height:
 * @widget: a #GtkWidget
 *
 * Returns the content height of the widget, as passed to its size-allocate implementation.
 * This is the size you should be using in GtkWidgetClass.snapshot(). For pointer
 * events, see gtk_widget_contains().
 *
 * Returns: The height of @widget
 */
int
gtk_widget_get_height (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return priv->height;
}

/**
 * gtk_widget_class_set_layout_manager_type:
 * @widget_class: class to set the layout manager type for
 * @type: The object type that implements the #GtkLayoutManager for @widget_class
 *
 * Sets the type to be used for creating layout managers for widgets of
 * @widget_class. The given @type must be a subtype of #GtkLayoutManager.
 *
 * This function should only be called from class init functions of widgets.
 **/
void
gtk_widget_class_set_layout_manager_type (GtkWidgetClass *widget_class,
                                          GType           type)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (g_type_is_a (type, GTK_TYPE_LAYOUT_MANAGER));

  priv = widget_class->priv;

  priv->layout_manager_type = type;
}

/**
 * gtk_widget_class_get_layout_manager_type:
 * @widget_class: a #GtkWidgetClass
 *
 * Retrieves the type of the #GtkLayoutManager used by the #GtkWidget class.
 *
 * See also: gtk_widget_class_set_layout_manager_type()
 *
 * Returns: a #GtkLayoutManager subclass, or %G_TYPE_INVALID
 */
GType
gtk_widget_class_get_layout_manager_type (GtkWidgetClass *widget_class)
{
  GtkWidgetClassPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET_CLASS (widget_class), G_TYPE_INVALID);

  priv = widget_class->priv;

  return priv->layout_manager_type;
}

/**
 * gtk_widget_set_layout_manager:
 * @widget: a #GtkWidget
 * @layout_manager: (nullable) (transfer full): a #GtkLayoutManager
 *
 * Sets the layout manager delegate instance that provides an implementation
 * for measuring and allocating the children of @widget.
 */
void
gtk_widget_set_layout_manager (GtkWidget        *widget,
                               GtkLayoutManager *layout_manager)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (layout_manager == NULL || GTK_IS_LAYOUT_MANAGER (layout_manager));
  g_return_if_fail (layout_manager == NULL || gtk_layout_manager_get_widget (layout_manager) == NULL);

  if (priv->layout_manager == layout_manager)
    return;

  priv->layout_manager = layout_manager;
  if (priv->layout_manager != NULL)
    gtk_layout_manager_set_widget (priv->layout_manager, widget);

  gtk_widget_queue_resize (widget);

  g_object_notify_by_pspec (G_OBJECT (widget), widget_props[PROP_LAYOUT_MANAGER]);
}

/**
 * gtk_widget_get_layout_manager:
 * @widget: a #GtkWidget
 *
 * Retrieves the layout manager set using gtk_widget_set_layout_manager().
 *
 * Returns: (transfer none) (nullable): a #GtkLayoutManager
 */
GtkLayoutManager *
gtk_widget_get_layout_manager (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = gtk_widget_get_instance_private (widget);

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return priv->layout_manager;
}

/**
 * gtk_widget_should_layout:
 * @widget: a widget
 *
 * Returns whether @widget should contribute to
 * the measuring and allocation of its parent.
 * This is %FALSE for invisible children, but also
 * for children that have their own surface.
 *
 * Returns: %TRUE if child should be included in
 *   measuring and allocating
 */
gboolean
gtk_widget_should_layout (GtkWidget *widget)
{
  if (!_gtk_widget_get_visible (widget))
    return FALSE;

  if (GTK_IS_NATIVE (widget))
    return FALSE;

  return TRUE;
}

static void
gtk_widget_class_add_action (GtkWidgetClass  *widget_class,
                             GtkWidgetAction *action)
{
  GtkWidgetClassPrivate *priv = widget_class->priv;

  if (priv->actions == NULL)
    priv->actions = g_ptr_array_new ();
  else if (GTK_IS_WIDGET_CLASS (&widget_class->parent_class))
    {
      GtkWidgetClass *parent_class = GTK_WIDGET_CLASS (&widget_class->parent_class);
      GtkWidgetClassPrivate *parent_priv = parent_class->priv;
      GPtrArray *parent_actions = parent_priv->actions;

      if (priv->actions == parent_actions)
        {
          int i;

          priv->actions = g_ptr_array_new ();
          for (i = 0; i < parent_actions->len; i++)
            g_ptr_array_add (priv->actions, g_ptr_array_index (parent_actions, i));
        }
    }

  GTK_NOTE(ACTIONS, g_message ("%sClass: Adding %s action\n",
                               g_type_name (G_TYPE_FROM_CLASS (widget_class)),
                               action->name));

  g_ptr_array_add (priv->actions, action);
}

/*
 * gtk_widget_class_install_action:
 * @widget_class: a #GtkWidgetClass
 * @action_name: a prefixed action name, such as "clipboard.paste"
 * @parameter_type: (allow-none): the parameter type, or %NULL
 * @activate: callback to use when the action is activated
 *
 * This should be called at class initialization time to specify
 * actions to be added for all instances of this class.
 *
 * Actions installed by this function are stateless. The only state
 * they have is whether they are enabled or not. For more complicated
 * actions, see gtk_widget_class_install_stateful_action().
 */
void
gtk_widget_class_install_action (GtkWidgetClass              *widget_class,
                                 const char                  *action_name,
                                 const char                  *parameter_type,
                                 GtkWidgetActionActivateFunc  activate)
{
  GtkWidgetAction *action;

  action = g_new0 (GtkWidgetAction, 1);
  action->owner = G_TYPE_FROM_CLASS (widget_class);
  action->name = g_strdup (action_name);
  if (parameter_type)
    action->parameter_type = g_variant_type_new (parameter_type);
  else
    action->parameter_type = NULL;
  action->activate = activate;

  gtk_widget_class_add_action (widget_class, action);
}

static const GVariantType *
determine_type (GParamSpec *pspec)
{
  if (G_TYPE_IS_ENUM (pspec->value_type))
    return G_VARIANT_TYPE_STRING;

  switch (pspec->value_type)
    {
    case G_TYPE_BOOLEAN:
      return G_VARIANT_TYPE_BOOLEAN;

    case G_TYPE_INT:
      return G_VARIANT_TYPE_INT32;

    case G_TYPE_UINT:
      return G_VARIANT_TYPE_UINT32;

    case G_TYPE_DOUBLE:
    case G_TYPE_FLOAT:
      return G_VARIANT_TYPE_DOUBLE;

    case G_TYPE_STRING:
      return G_VARIANT_TYPE_STRING;

    default:
      g_critical ("Unable to use gtk_widget_class_install_property_action with property '%s::%s' of type '%s'",
                  g_type_name (pspec->owner_type), pspec->name, g_type_name (pspec->value_type));
      return NULL;
    }
}

void
gtk_widget_class_install_property_action (GtkWidgetClass *widget_class,
                                          const char     *action_name,
                                          const char     *property_name)
{
  GParamSpec *pspec;
  GtkWidgetAction *action;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

  pspec = g_object_class_find_property (G_OBJECT_CLASS (widget_class), property_name);

  if (pspec == NULL)
    {
      g_critical ("Attempted to use non-existent property '%s::%s' for dgtk_widget_class_install_property_action",
                  g_type_name (G_TYPE_FROM_CLASS (widget_class)), property_name);
      return;
    }

  if (~pspec->flags & G_PARAM_READABLE || ~pspec->flags & G_PARAM_WRITABLE || pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_critical ("Property '%s::%s' used with gtk_widget_class_install_property_action must be readable, writable, and not construct-only",
                  g_type_name (G_TYPE_FROM_CLASS (widget_class)), property_name);
      return;
    }

  action = g_new0 (GtkWidgetAction, 1);
  action->owner = G_TYPE_FROM_CLASS (widget_class);
  action->name = g_strdup (action_name);
  action->pspec = pspec;
  action->state_type = determine_type (action->pspec);
  if (action->pspec->value_type == G_TYPE_BOOLEAN)
    action->parameter_type = NULL;
  else
    action->parameter_type = action->state_type;
  action->activate = NULL;

  gtk_widget_class_add_action (widget_class, action);
}

/**
 * gtk_widget_action_set_enabled:
 * @widget: a #GtkWidget
 * @action_name: action name, such as "clipboard.paste"
 * @enabled: whether the action is now enabled
 *
 * Enable or disable an action installed with
 * gtk_widget_class_install_action().
 */
void
gtk_widget_action_set_enabled (GtkWidget  *widget,
                               const char *action_name,
                               gboolean    enabled)
{
  GtkActionMuxer *muxer;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  muxer = _gtk_widget_get_action_muxer (widget, TRUE);
  gtk_action_muxer_action_enabled_changed (muxer, action_name, enabled);
}

/**
 * gtk_widget_class_query_action:
 * @widget_class: a #GtkWidgetClass
 * @index_: position of the action to query
 * @owner: the type where the action was defined
 * @action_name: return location for the action name
 * @parameter_type: return location for the parameter type
 * @property_name: return location for the property name
 *
 * Queries the actions that have been installed for
 * a widget class using gtk_widget_class_install_action()
 * during class initialization.
 *
 * Note that this function will also return actions defined
 * by parent classes. You can identify those by looking
 * at @owner.
 *
 * Returns: %TRUE if the action was found,
 *     %FALSE if @index_ is out of range
 */
gboolean
gtk_widget_class_query_action (GtkWidgetClass      *widget_class,
                               guint                index_,
                               GType               *owner,
                               const char         **action_name,
                               const GVariantType **parameter_type,
                               const char         **property_name)
{
  GtkWidgetClassPrivate *priv = widget_class->priv;

  if (priv->actions && index_ < priv->actions->len)
    {
      GtkWidgetAction *action = g_ptr_array_index (priv->actions, index_);

      *owner = action->owner;
      *action_name = action->name;
      *parameter_type = action->parameter_type;
      if (action->pspec)
        *property_name = action->pspec->name;
      else
        *property_name = NULL;

      return TRUE;
    }

  return FALSE;
}
