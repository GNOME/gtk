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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include "gtkpaned.h"

#include "gtkbindings.h"
#include "gtkcontainerprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgesturepan.h"
#include "gtkgesturesingle.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcssboxesprivate.h"

#include "a11y/gtkpanedaccessible.h"

#include <math.h>

/**
 * SECTION:gtkpaned
 * @Short_description: A widget with two adjustable panes
 * @Title: GtkPaned
 *
 * #GtkPaned has two panes, arranged either
 * horizontally or vertically. The division between
 * the two panes is adjustable by the user by dragging
 * a handle.
 *
 * Child widgets are
 * added to the panes of the widget with gtk_paned_pack1() and
 * gtk_paned_pack2(). The division between the two children is set by default
 * from the size requests of the children, but it can be adjusted by the
 * user.
 *
 * A paned widget draws a separator between the two child widgets and a
 * small handle that the user can drag to adjust the division. It does not
 * draw any relief around the children or around the separator. (The space
 * in which the separator is called the gutter.) Often, it is useful to put
 * each child inside a #GtkFrame with the shadow type set to %GTK_SHADOW_IN
 * so that the gutter appears as a ridge. No separator is drawn if one of
 * the children is missing.
 *
 * Each child has two options that can be set, @resize and @shrink. If
 * @resize is true, then when the #GtkPaned is resized, that child will
 * expand or shrink along with the paned widget. If @shrink is true, then
 * that child can be made smaller than its requisition by the user.
 * Setting @shrink to %FALSE allows the application to set a minimum size.
 * If @resize is false for both children, then this is treated as if
 * @resize is true for both children.
 *
 * The application can set the position of the slider as if it were set
 * by the user, by calling gtk_paned_set_position().
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * paned
 * ├── <child>
 * ├── separator[.wide]
 * ╰── <child>
 * ]|
 *
 * GtkPaned has a main CSS node with name paned, and a subnode for
 * the separator with name separator. The subnode gets a .wide style
 * class when the paned is supposed to be wide.
 *
 * In horizontal orientation, the nodes are arranged based on the text
 * direction, so in left-to-right mode, :first-child will select the
 * leftmost child, while it will select the rightmost child in
 * RTL layouts.
 *
 * ## Creating a paned widget with minimum sizes.
 *
 * |[<!-- language="C" -->
 * GtkWidget *hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
 * GtkWidget *frame1 = gtk_frame_new (NULL);
 * GtkWidget *frame2 = gtk_frame_new (NULL);
 * gtk_frame_set_shadow_type (GTK_FRAME (frame1), GTK_SHADOW_IN);
 * gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_IN);
 *
 * gtk_widget_set_size_request (hpaned, 200, -1);
 *
 * gtk_paned_pack1 (GTK_PANED (hpaned), frame1, TRUE, FALSE);
 * gtk_widget_set_size_request (frame1, 50, -1);
 *
 * gtk_paned_pack2 (GTK_PANED (hpaned), frame2, FALSE, FALSE);
 * gtk_widget_set_size_request (frame2, 50, -1);
 * ]|
 */

#define HANDLE_EXTRA_SIZE 6

enum {
  CHILD1,
  CHILD2
};

typedef struct _GtkPanedClass   GtkPanedClass;

struct _GtkPaned
{
  GtkContainer parent_instance;
};

struct _GtkPanedClass
{
  GtkContainerClass parent_class;

  gboolean (* cycle_child_focus)   (GtkPaned      *paned,
                                    gboolean       reverse);
  gboolean (* toggle_handle_focus) (GtkPaned      *paned);
  gboolean (* move_handle)         (GtkPaned      *paned,
                                    GtkScrollType  scroll);
  gboolean (* cycle_handle_focus)  (GtkPaned      *paned,
                                    gboolean       reverse);
  gboolean (* accept_position)     (GtkPaned      *paned);
  gboolean (* cancel_position)     (GtkPaned      *paned);
};

typedef struct
{
  GtkPaned       *first_paned;
  GtkWidget      *child1;
  GtkWidget      *child2;
  GtkWidget      *last_child1_focus;
  GtkWidget      *last_child2_focus;
  GtkWidget      *saved_focus;
  GtkOrientation  orientation;

  GtkWidget     *handle_widget;

  GtkGesture    *pan_gesture;  /* Used for touch */
  GtkGesture    *drag_gesture; /* Used for mice */

  gint          child1_size;
  gint          drag_pos;
  gint          last_allocation;
  gint          max_position;
  gint          min_position;
  gint          original_position;

  guint         in_recursion  : 1;
  guint         child1_resize : 1;
  guint         child1_shrink : 1;
  guint         child2_resize : 1;
  guint         child2_shrink : 1;
  guint         position_set  : 1;
  guint         panning       : 1;
} GtkPanedPrivate;

enum {
  PROP_0,
  PROP_POSITION,
  PROP_POSITION_SET,
  PROP_MIN_POSITION,
  PROP_MAX_POSITION,
  PROP_WIDE_HANDLE,
  PROP_RESIZE_CHILD1,
  PROP_RESIZE_CHILD2,
  PROP_SHRINK_CHILD1,
  PROP_SHRINK_CHILD2,
  LAST_PROP,

  /* GtkOrientable */
  PROP_ORIENTATION,
};

enum {
  CYCLE_CHILD_FOCUS,
  TOGGLE_HANDLE_FOCUS,
  MOVE_HANDLE,
  CYCLE_HANDLE_FOCUS,
  ACCEPT_POSITION,
  CANCEL_POSITION,
  LAST_SIGNAL
};

static void     gtk_paned_set_property          (GObject          *object,
						 guint             prop_id,
						 const GValue     *value,
						 GParamSpec       *pspec);
static void     gtk_paned_get_property          (GObject          *object,
						 guint             prop_id,
						 GValue           *value,
						 GParamSpec       *pspec);
static void     gtk_paned_finalize              (GObject          *object);
static void     gtk_paned_measure (GtkWidget *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline);
static void     gtk_paned_size_allocate         (GtkWidget           *widget,
                                                 int                  width,
                                                 int                  height,
                                                 int                  baseline);
static void     gtk_paned_unrealize             (GtkWidget        *widget);
static gboolean gtk_paned_focus                 (GtkWidget        *widget,
						 GtkDirectionType  direction);
static void     gtk_paned_add                   (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_paned_remove                (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_paned_forall                (GtkContainer     *container,
						 GtkCallback       callback,
						 gpointer          callback_data);
static void     gtk_paned_calc_position         (GtkPaned         *paned,
                                                 gint              allocation,
                                                 gint              child1_req,
                                                 gint              child2_req);
static void     gtk_paned_set_focus_child       (GtkContainer     *container,
						 GtkWidget        *child);
static void     gtk_paned_set_saved_focus       (GtkPaned         *paned,
						 GtkWidget        *widget);
static void     gtk_paned_set_first_paned       (GtkPaned         *paned,
						 GtkPaned         *first_paned);
static void     gtk_paned_set_last_child1_focus (GtkPaned         *paned,
						 GtkWidget        *widget);
static void     gtk_paned_set_last_child2_focus (GtkPaned         *paned,
						 GtkWidget        *widget);
static gboolean gtk_paned_cycle_child_focus     (GtkPaned         *paned,
						 gboolean          reverse);
static gboolean gtk_paned_cycle_handle_focus    (GtkPaned         *paned,
						 gboolean          reverse);
static gboolean gtk_paned_move_handle           (GtkPaned         *paned,
						 GtkScrollType     scroll);
static gboolean gtk_paned_accept_position       (GtkPaned         *paned);
static gboolean gtk_paned_cancel_position       (GtkPaned         *paned);
static gboolean gtk_paned_toggle_handle_focus   (GtkPaned         *paned);

static GType    gtk_paned_child_type            (GtkContainer     *container);

static void     update_drag                     (GtkPaned         *paned,
                                                 int               xpos,
                                                 int               ypos);

G_DEFINE_TYPE_WITH_CODE (GtkPaned, gtk_paned, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkPaned)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *paned_props[LAST_PROP] = { NULL, };

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers)
{
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
                                "toggle-handle-focus", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
				"toggle-handle-focus", 0);
}

static void
add_move_binding (GtkBindingSet   *binding_set,
		  guint            keyval,
		  GdkModifierType  mask,
		  GtkScrollType    scroll)
{
  gtk_binding_entry_add_signal (binding_set, keyval, mask,
				"move-handle", 1,
				GTK_TYPE_SCROLL_TYPE, scroll);
}

static void
get_handle_area (GtkPaned        *paned,
                 graphene_rect_t *area)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  int extra = 0;

  if (!gtk_widget_compute_bounds (priv->handle_widget, GTK_WIDGET (paned), area))
    return;

  if (!gtk_paned_get_wide_handle (paned))
    extra = HANDLE_EXTRA_SIZE;

  graphene_rect_inset (area, - extra, - extra);
}

static gboolean
gtk_paned_handle_contains (GtkGizmo *handle,
                           double    x,
                           double    y)
{
  GtkWidget *paned;
  GtkCssBoxes boxes;
  graphene_rect_t area;

  gtk_css_boxes_init (&boxes, GTK_WIDGET (handle));

  graphene_rect_init_from_rect (&area, gtk_css_boxes_get_border_rect (&boxes));

  paned = gtk_widget_get_parent (GTK_WIDGET (handle));
  if (!gtk_paned_get_wide_handle (GTK_PANED (paned)))
    graphene_rect_inset (&area, - HANDLE_EXTRA_SIZE, - HANDLE_EXTRA_SIZE);

  return graphene_rect_contains_point (&area, &GRAPHENE_POINT_INIT (x, y));
}

static void
gtk_paned_class_init (GtkPanedClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkPanedClass *paned_class;
  GtkBindingSet *binding_set;

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;
  paned_class = (GtkPanedClass *) class;

  object_class->set_property = gtk_paned_set_property;
  object_class->get_property = gtk_paned_get_property;
  object_class->finalize = gtk_paned_finalize;

  widget_class->measure = gtk_paned_measure;
  widget_class->size_allocate = gtk_paned_size_allocate;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->focus = gtk_paned_focus;

  container_class->add = gtk_paned_add;
  container_class->remove = gtk_paned_remove;
  container_class->forall = gtk_paned_forall;
  container_class->child_type = gtk_paned_child_type;
  container_class->set_focus_child = gtk_paned_set_focus_child;

  paned_class->cycle_child_focus = gtk_paned_cycle_child_focus;
  paned_class->toggle_handle_focus = gtk_paned_toggle_handle_focus;
  paned_class->move_handle = gtk_paned_move_handle;
  paned_class->cycle_handle_focus = gtk_paned_cycle_handle_focus;
  paned_class->accept_position = gtk_paned_accept_position;
  paned_class->cancel_position = gtk_paned_cancel_position;


  paned_props[PROP_POSITION] =
    g_param_spec_int ("position",
                      P_("Position"),
                      P_("Position of paned separator in pixels (0 means all the way to the left/top)"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  paned_props[PROP_POSITION_SET] =
    g_param_spec_boolean ("position-set",
                          P_("Position Set"),
                          P_("TRUE if the Position property should be used"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:min-position:
   *
   * The smallest possible value for the position property.
   * This property is derived from the size and shrinkability
   * of the widget's children.
   */
  paned_props[PROP_MIN_POSITION] =
    g_param_spec_int ("min-position",
                      P_("Minimal Position"),
                      P_("Smallest possible value for the “position” property"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:max-position:
   *
   * The largest possible value for the position property.
   * This property is derived from the size and shrinkability
   * of the widget's children.
   */
  paned_props[PROP_MAX_POSITION] =
    g_param_spec_int ("max-position",
                      P_("Maximal Position"),
                      P_("Largest possible value for the “position” property"),
                      0, G_MAXINT, G_MAXINT,
                      GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:wide-handle:
   *
   * Setting this property to %TRUE indicates that the paned needs
   * to provide stronger visual separation (e.g. because it separates
   * between two notebooks, whose tab rows would otherwise merge visually).
   */
  paned_props[PROP_WIDE_HANDLE] =
    g_param_spec_boolean ("wide-handle",
                          P_("Wide Handle"),
                          P_("Whether the paned should have a prominent handle"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:resize-child1:
   *
   * The "resize-child1" property determines whether the first child expands and
   * shrinks along with the paned widget.
   */
  paned_props[PROP_RESIZE_CHILD1] =
    g_param_spec_boolean ("resize-child1", 
                          P_("Resize first child"),
                          P_("If TRUE, the first child expands and shrinks along with the paned widget"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:resize-child2:
   *
   * The "resize-child2" property determines whether the second child expands and
   * shrinks along with the paned widget.
   */
  paned_props[PROP_RESIZE_CHILD2] =
    g_param_spec_boolean ("resize-child2", 
                          P_("Resize second child"),
                          P_("If TRUE, the second child expands and shrinks along with the paned widget"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:shrink-child1:
   *
   * The "shrink-child1" property determines whether the first child can be made
   * smaller than its requisition.
   */
  paned_props[PROP_SHRINK_CHILD1] =
    g_param_spec_boolean ("shrink-child1", 
                          P_("Shrink first child"),
                          P_("If TRUE, the first child can be made smaller than its requisition"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPaned:shrink-child2:
   *
   * The "shrink-child2" property determines whether the second child can be made
   * smaller than its requisition.
   */
  paned_props[PROP_SHRINK_CHILD2] =
    g_param_spec_boolean ("shrink-child2", 
                          P_("Shrink second child"),
                          P_("If TRUE, the second child can be made smaller than its requisition"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, paned_props);
  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkPaned::cycle-child-focus:
   * @widget: the object that received the signal
   * @reversed: whether cycling backward or forward
   *
   * The ::cycle-child-focus signal is a 
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to cycle the focus between the children of the paned.
   *
   * The default binding is f6.
   */
  signals [CYCLE_CHILD_FOCUS] =
    g_signal_new (I_("cycle-child-focus"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, cycle_child_focus),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkPaned::toggle-handle-focus:
   * @widget: the object that received the signal
   *
   * The ::toggle-handle-focus is a 
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to accept the current position of the handle and then 
   * move focus to the next widget in the focus chain.
   *
   * The default binding is Tab.
   */
  signals [TOGGLE_HANDLE_FOCUS] =
    g_signal_new (I_("toggle-handle-focus"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, toggle_handle_focus),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  /**
   * GtkPaned::move-handle:
   * @widget: the object that received the signal
   * @scroll_type: a #GtkScrollType
   *
   * The ::move-handle signal is a 
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to move the handle when the user is using key bindings 
   * to move it.
   */
  signals[MOVE_HANDLE] =
    g_signal_new (I_("move-handle"),
		  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, move_handle),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkPaned::cycle-handle-focus:
   * @widget: the object that received the signal
   * @reversed: whether cycling backward or forward
   *
   * The ::cycle-handle-focus signal is a 
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to cycle whether the paned should grab focus to allow
   * the user to change position of the handle by using key bindings.
   *
   * The default binding for this signal is f8.
   */
  signals [CYCLE_HANDLE_FOCUS] =
    g_signal_new (I_("cycle-handle-focus"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, cycle_handle_focus),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkPaned::accept-position:
   * @widget: the object that received the signal
   *
   * The ::accept-position signal is a 
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to accept the current position of the handle when 
   * moving it using key bindings.
   *
   * The default binding for this signal is Return or Space.
   */
  signals [ACCEPT_POSITION] =
    g_signal_new (I_("accept-position"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, accept_position),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  /**
   * GtkPaned::cancel-position:
   * @widget: the object that received the signal
   *
   * The ::cancel-position signal is a 
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to cancel moving the position of the handle using key 
   * bindings. The position of the handle will be reset to the value prior to 
   * moving it.
   *
   * The default binding for this signal is Escape.
   */
  signals [CANCEL_POSITION] =
    g_signal_new (I_("cancel-position"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, cancel_position),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  binding_set = gtk_binding_set_by_class (class);

  /* F6 and friends */
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_F6, 0,
                                "cycle-child-focus", 1, 
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_F6, GDK_SHIFT_MASK,
				"cycle-child-focus", 1,
				G_TYPE_BOOLEAN, TRUE);

  /* F8 and friends */
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_F8, 0,
				"cycle-handle-focus", 1,
				G_TYPE_BOOLEAN, FALSE);
 
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_F8, GDK_SHIFT_MASK,
				"cycle-handle-focus", 1,
				G_TYPE_BOOLEAN, TRUE);
 
  add_tab_bindings (binding_set, 0);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK);

  /* accept and cancel positions */
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Escape, 0,
				"cancel-position", 0);

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Return, 0,
				"accept-position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_ISO_Enter, 0,
				"accept-position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Enter, 0,
				"accept-position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_space, 0,
				"accept-position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Space, 0,
				"accept-position", 0);

  /* move handle */
  add_move_binding (binding_set, GDK_KEY_Left, 0, GTK_SCROLL_STEP_LEFT);
  add_move_binding (binding_set, GDK_KEY_KP_Left, 0, GTK_SCROLL_STEP_LEFT);
  add_move_binding (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_LEFT);
  add_move_binding (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_LEFT);

  add_move_binding (binding_set, GDK_KEY_Right, 0, GTK_SCROLL_STEP_RIGHT);
  add_move_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_RIGHT);
  add_move_binding (binding_set, GDK_KEY_KP_Right, 0, GTK_SCROLL_STEP_RIGHT);
  add_move_binding (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_RIGHT);

  add_move_binding (binding_set, GDK_KEY_Up, 0, GTK_SCROLL_STEP_UP);
  add_move_binding (binding_set, GDK_KEY_Up, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_UP);
  add_move_binding (binding_set, GDK_KEY_KP_Up, 0, GTK_SCROLL_STEP_UP);
  add_move_binding (binding_set, GDK_KEY_KP_Up, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_UP);
  add_move_binding (binding_set, GDK_KEY_Page_Up, 0, GTK_SCROLL_PAGE_UP);
  add_move_binding (binding_set, GDK_KEY_KP_Page_Up, 0, GTK_SCROLL_PAGE_UP);

  add_move_binding (binding_set, GDK_KEY_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_move_binding (binding_set, GDK_KEY_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_DOWN);
  add_move_binding (binding_set, GDK_KEY_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_move_binding (binding_set, GDK_KEY_KP_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_DOWN);
  add_move_binding (binding_set, GDK_KEY_Page_Down, 0, GTK_SCROLL_PAGE_RIGHT);
  add_move_binding (binding_set, GDK_KEY_KP_Page_Down, 0, GTK_SCROLL_PAGE_RIGHT);

  add_move_binding (binding_set, GDK_KEY_Home, 0, GTK_SCROLL_START);
  add_move_binding (binding_set, GDK_KEY_KP_Home, 0, GTK_SCROLL_START);
  add_move_binding (binding_set, GDK_KEY_End, 0, GTK_SCROLL_END);
  add_move_binding (binding_set, GDK_KEY_KP_End, 0, GTK_SCROLL_END);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_PANED_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("paned"));
}

static GType
gtk_paned_child_type (GtkContainer *container)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (!priv->child1 || !priv->child2)
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static gboolean
initiates_touch_drag (GtkPaned *paned,
                      gdouble   start_x,
                      gdouble   start_y)
{
  int handle_size, handle_pos, drag_pos;
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  graphene_rect_t handle_area;

#define TOUCH_EXTRA_AREA_WIDTH 50
  get_handle_area (paned, &handle_area);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      handle_pos = handle_area.origin.x;
      drag_pos = start_x;
      handle_size = handle_area.size.width;
    }
  else
    {
      handle_pos = handle_area.origin.y;
      drag_pos = start_y;
      handle_size = handle_area.size.height;
    }

  if (drag_pos < handle_pos - TOUCH_EXTRA_AREA_WIDTH ||
      drag_pos > handle_pos + handle_size + TOUCH_EXTRA_AREA_WIDTH)
    return FALSE;

#undef TOUCH_EXTRA_AREA_WIDTH

  return TRUE;
}

static void
gesture_drag_begin_cb (GtkGestureDrag *gesture,
                       gdouble         start_x,
                       gdouble         start_y,
                       GtkPaned       *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  GdkEventSequence *sequence;
  graphene_rect_t handle_area;
  const GdkEvent *event;
  GdkDevice *device;
  gboolean is_touch;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  device = gdk_event_get_source_device (event);
  priv->panning = FALSE;

  is_touch = (gdk_event_get_event_type (event) == GDK_TOUCH_BEGIN ||
              gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);

  get_handle_area (paned, &handle_area);

  if ((is_touch && GTK_GESTURE (gesture) == priv->drag_gesture) ||
      (!is_touch && GTK_GESTURE (gesture) == priv->pan_gesture))
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (graphene_rect_contains_point (&handle_area, &(graphene_point_t){start_x, start_y}) ||
      (is_touch && initiates_touch_drag (paned, start_x, start_y)))
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        priv->drag_pos = start_x - handle_area.origin.x;
      else
        priv->drag_pos = start_y - handle_area.origin.y;

      priv->panning = TRUE;

      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_DENIED);
    }
}

static void
gesture_drag_update_cb (GtkGestureDrag   *gesture,
                        gdouble           offset_x,
                        gdouble           offset_y,
                        GtkPaned         *paned)
{
  gdouble start_x, start_y;

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture),
                               &start_x, &start_y);
  update_drag (paned, start_x + offset_x, start_y + offset_y);
}

static void
gesture_drag_end_cb (GtkGestureDrag *gesture,
                     gdouble         offset_x,
                     gdouble         offset_y,
                     GtkPaned       *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (!priv->panning)
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);

  priv->panning = FALSE;
}

static void
gtk_paned_set_property (GObject        *object,
			guint           prop_id,
			const GValue   *value,
			GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      if (priv->orientation != g_value_get_enum (value))
        {
          priv->orientation = g_value_get_enum (value);
          _gtk_orientable_set_style_classes (GTK_ORIENTABLE (paned));

          if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              gtk_gesture_pan_set_orientation (GTK_GESTURE_PAN (priv->pan_gesture),
                                               GTK_ORIENTATION_HORIZONTAL);
              gtk_widget_set_cursor_from_name (priv->handle_widget,
                                               "col-resize");
            }
          else
            {
              gtk_gesture_pan_set_orientation (GTK_GESTURE_PAN (priv->pan_gesture),
                                               GTK_ORIENTATION_VERTICAL);
              gtk_widget_set_cursor_from_name (priv->handle_widget,
                                               "row-resize");
            }

          gtk_widget_queue_resize (GTK_WIDGET (paned));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_POSITION:
      gtk_paned_set_position (paned, g_value_get_int (value));
      break;
    case PROP_POSITION_SET:
      if (priv->position_set != g_value_get_boolean (value))
        {
          priv->position_set = g_value_get_boolean (value);
          gtk_widget_queue_resize (GTK_WIDGET (paned));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_WIDE_HANDLE:
      gtk_paned_set_wide_handle (paned, g_value_get_boolean (value));
      break;
    case PROP_RESIZE_CHILD1:
      if (priv->child1_resize != g_value_get_boolean (value))
        {
          priv->child1_resize = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
          gtk_widget_queue_resize (GTK_WIDGET (object));
        }
      break;
    case PROP_RESIZE_CHILD2:
      if (priv->child2_resize != g_value_get_boolean (value))
        {
          priv->child2_resize = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
          gtk_widget_queue_resize (GTK_WIDGET (object));
        }
      break;
    case PROP_SHRINK_CHILD1:
      if (priv->child1_shrink != g_value_get_boolean (value))
        {
          priv->child1_shrink = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
          gtk_widget_queue_resize (GTK_WIDGET (object));
        }
      break;
    case PROP_SHRINK_CHILD2:
      if (priv->child2_shrink != g_value_get_boolean (value))
        {
          priv->child2_shrink = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
          gtk_widget_queue_resize (GTK_WIDGET (object));
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_get_property (GObject        *object,
			guint           prop_id,
			GValue         *value,
			GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_POSITION:
      g_value_set_int (value, priv->child1_size);
      break;
    case PROP_POSITION_SET:
      g_value_set_boolean (value, priv->position_set);
      break;
    case PROP_MIN_POSITION:
      g_value_set_int (value, priv->min_position);
      break;
    case PROP_MAX_POSITION:
      g_value_set_int (value, priv->max_position);
      break;
    case PROP_WIDE_HANDLE:
      g_value_set_boolean (value, gtk_paned_get_wide_handle (paned));
      break;
    case PROP_RESIZE_CHILD1:
      g_value_set_boolean (value, priv->child1_resize);
      break;
    case PROP_RESIZE_CHILD2:
      g_value_set_boolean (value, priv->child2_resize);
      break;
    case PROP_SHRINK_CHILD1:
      g_value_set_boolean (value, priv->child1_shrink);
      break;
    case PROP_SHRINK_CHILD2:
      g_value_set_boolean (value, priv->child2_shrink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_finalize (GObject *object)
{
  GtkPaned *paned = GTK_PANED (object);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  
  gtk_paned_set_saved_focus (paned, NULL);
  gtk_paned_set_first_paned (paned, NULL);

  gtk_widget_unparent (priv->handle_widget);

  G_OBJECT_CLASS (gtk_paned_parent_class)->finalize (object);
}

static void
gtk_paned_compute_position (GtkPaned *paned,
                            gint      allocation,
                            gint      child1_req,
                            gint      child2_req,
                            gint     *min_pos,
                            gint     *max_pos,
                            gint     *out_pos)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  gint min, max, pos;

  min = priv->child1_shrink ? 0 : child1_req;

  max = allocation;
  if (!priv->child2_shrink)
    max = MAX (1, max - child2_req);
  max = MAX (min, max);

  if (!priv->position_set)
    {
      if (priv->child1_resize && !priv->child2_resize)
	pos = MAX (0, allocation - child2_req);
      else if (!priv->child1_resize && priv->child2_resize)
	pos = child1_req;
      else if (child1_req + child2_req != 0)
	pos = allocation * ((gdouble)child1_req / (child1_req + child2_req)) + 0.5;
      else
	pos = allocation * 0.5 + 0.5;
    }
  else
    {
      /* If the position was set before the initial allocation.
       * (priv->last_allocation <= 0) just clamp it and leave it.
       */
      if (priv->last_allocation > 0)
	{
	  if (priv->child1_resize && !priv->child2_resize)
	    pos = priv->child1_size + allocation - priv->last_allocation;
	  else if (!(!priv->child1_resize && priv->child2_resize))
	    pos = allocation * ((gdouble) priv->child1_size / (priv->last_allocation)) + 0.5;
          else
            pos = priv->child1_size;
	}
      else
        pos = priv->child1_size;
    }

  pos = CLAMP (pos, min, max);
  
  if (min_pos)
    *min_pos = min;
  if (max_pos)
    *max_pos = max;
  if (out_pos)
    *out_pos = pos;
}

static void
gtk_paned_get_preferred_size_for_orientation (GtkWidget      *widget,
                                              gint            size,
                                              gint           *minimum,
                                              gint           *natural)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  gint child_min, child_nat;

  *minimum = *natural = 0;

  if (priv->child1 && gtk_widget_get_visible (priv->child1))
    {
      gtk_widget_measure (priv->child1, priv->orientation, size, &child_min, &child_nat, NULL, NULL);
      if (priv->child1_shrink)
        *minimum = 0;
      else
        *minimum = child_min;
      *natural = child_nat;
    }

  if (priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      gtk_widget_measure (priv->child2, priv->orientation, size, &child_min, &child_nat, NULL, NULL);

      if (!priv->child2_shrink)
        *minimum += child_min;
      *natural += child_nat;
    }

  if (priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      gint handle_size;

      gtk_widget_measure (priv->handle_widget,
                          priv->orientation,
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      *minimum += handle_size;
      *natural += handle_size;
    }
}

static void
gtk_paned_get_preferred_size_for_opposite_orientation (GtkWidget      *widget,
                                                       gint            size,
                                                       gint           *minimum,
                                                       gint           *natural)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  gint for_child1, for_child2;
  gint child_min, child_nat;

  if (size > -1 &&
      priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      gint child1_req, child2_req;
      gint handle_size;

      gtk_widget_measure (priv->handle_widget,
                          OPPOSITE_ORIENTATION (priv->orientation),
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      gtk_widget_measure (priv->child1, priv->orientation, -1, &child1_req, NULL, NULL, NULL);
      gtk_widget_measure (priv->child2, priv->orientation, -1, &child2_req, NULL, NULL, NULL);

      gtk_paned_compute_position (paned,
                                  size - handle_size, child1_req, child2_req,
                                  NULL, NULL, &for_child1);

      for_child2 = size - for_child1 - handle_size;
    }
  else
    {
      for_child1 = size;
      for_child2 = size;
    }

  *minimum = *natural = 0;

  if (priv->child1 && gtk_widget_get_visible (priv->child1))
    {
      gtk_widget_measure (priv->child1,
                          OPPOSITE_ORIENTATION (priv->orientation),
                          for_child1,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = child_min;
      *natural = child_nat;
    }

  if (priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      gtk_widget_measure (priv->child2,
                          OPPOSITE_ORIENTATION (priv->orientation),
                          for_child2,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_paned_measure (GtkWidget *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (orientation == priv->orientation)
    gtk_paned_get_preferred_size_for_orientation (widget, for_size, minimum, natural);
  else
    gtk_paned_get_preferred_size_for_opposite_orientation (widget, for_size, minimum, natural);
}

static void
flip_child (int            width,
            GtkAllocation *child_pos)
{
  child_pos->x = width - child_pos->x - child_pos->width;
}

static void
gtk_paned_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      GtkAllocation child1_allocation;
      GtkAllocation child2_allocation;
      GtkAllocation handle_allocation;
      gint handle_size;

      gtk_widget_measure (priv->handle_widget,
                          priv->orientation,
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gint child1_width, child2_width;

          gtk_widget_measure (priv->child1, GTK_ORIENTATION_HORIZONTAL,
                              height,
                              &child1_width, NULL, NULL, NULL);
          gtk_widget_measure (priv->child2, GTK_ORIENTATION_HORIZONTAL,
                              height,
                              &child2_width, NULL, NULL, NULL);

          gtk_paned_calc_position (paned,
                                   MAX (1, width - handle_size),
                                   child1_width,
                                   child2_width);

          handle_allocation = (GdkRectangle){
            priv->child1_size,
            0,
            handle_size,
            height
          };

          child1_allocation.height = child2_allocation.height = height;
          child1_allocation.width = MAX (1, priv->child1_size);
          child1_allocation.x = 0;
          child1_allocation.y = child2_allocation.y = 0;

          child2_allocation.x = child1_allocation.x + priv->child1_size + handle_size;
          child2_allocation.width = MAX (1, width - priv->child1_size - handle_size);

          if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL)
            {
              flip_child (width, &(child2_allocation));
              flip_child (width, &(child1_allocation));
              flip_child (width, &(handle_allocation));
            }

          if (child1_width > child1_allocation.width)
            {
              if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_LTR)
                child1_allocation.x -= child1_width - child1_allocation.width;
              child1_allocation.width = child1_width;
            }

          if (child2_width > child2_allocation.width)
            {
              if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL)
                child2_allocation.x -= child2_width - child2_allocation.width;
              child2_allocation.width = child2_width;
            }
        }
      else
        {
          gint child1_height, child2_height;

          gtk_widget_measure (priv->child1, GTK_ORIENTATION_VERTICAL,
                              width,
                              &child1_height, NULL, NULL, NULL);
          gtk_widget_measure (priv->child2, GTK_ORIENTATION_VERTICAL,
                              width,
                              &child2_height, NULL, NULL, NULL);

          gtk_paned_calc_position (paned,
                                   MAX (1, height - handle_size),
                                   child1_height,
                                   child2_height);

          handle_allocation = (GdkRectangle){
            0,
            priv->child1_size,
            width,
            handle_size,
          };

          child1_allocation.width = child2_allocation.width = width;
          child1_allocation.height = MAX (1, priv->child1_size);
          child1_allocation.x = child2_allocation.x = 0;
          child1_allocation.y = 0;

          child2_allocation.y = child1_allocation.y + priv->child1_size + handle_size;
          child2_allocation.height = MAX (1, height - child2_allocation.y);

          if (child1_height > child1_allocation.height)
            {
              child1_allocation.y -= child1_height - child1_allocation.height;
              child1_allocation.height = child1_height;
            }

          if (child2_height > child2_allocation.height)
            child2_allocation.height = child2_height;
        }

      gtk_widget_set_child_visible (priv->handle_widget, TRUE);

      gtk_widget_size_allocate (priv->handle_widget, &handle_allocation, -1);
      gtk_widget_size_allocate (priv->child1, &child1_allocation, -1);
      gtk_widget_size_allocate (priv->child2, &child2_allocation, -1);
    }
  else
    {
      if (priv->child1 && gtk_widget_get_visible (priv->child1))
        {
          gtk_widget_set_child_visible (priv->child1, TRUE);

          gtk_widget_size_allocate (priv->child1,
                                    &(GtkAllocation) {0, 0, width, height}, -1);
        }
      else if (priv->child2 && gtk_widget_get_visible (priv->child2))
        {
          gtk_widget_set_child_visible (priv->child2, TRUE);

          gtk_widget_size_allocate (priv->child2,
                                    &(GtkAllocation) {0, 0, width, height}, -1);

        }

      gtk_widget_set_child_visible (priv->handle_widget, FALSE);
    }
}


static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);

  gtk_paned_set_last_child1_focus (paned, NULL);
  gtk_paned_set_last_child2_focus (paned, NULL);
  gtk_paned_set_saved_focus (paned, NULL);
  gtk_paned_set_first_paned (paned, NULL);

  GTK_WIDGET_CLASS (gtk_paned_parent_class)->unrealize (widget);
}

static void
gtk_paned_render_handle (GtkGizmo    *gizmo,
                         GtkSnapshot *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (width > 0 && height > 0)
    gtk_css_style_snapshot_icon (style,
                                 snapshot,
                                 width, height,
                                 GTK_CSS_IMAGE_BUILTIN_PANE_SEPARATOR);
}

static void
connect_drag_gesture_signals (GtkPaned   *paned,
                              GtkGesture *gesture)
{
  g_signal_connect (gesture, "drag-begin",
                    G_CALLBACK (gesture_drag_begin_cb), paned);
  g_signal_connect (gesture, "drag-update",
                    G_CALLBACK (gesture_drag_update_cb), paned);
  g_signal_connect (gesture, "drag-end",
                    G_CALLBACK (gesture_drag_end_cb), paned);
}

static void
gtk_paned_init (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  GtkGesture *gesture;

  gtk_widget_set_can_focus (GTK_WIDGET (paned), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (paned), GTK_OVERFLOW_HIDDEN);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  priv->child1 = NULL;
  priv->child2 = NULL;

  priv->position_set = FALSE;
  priv->last_allocation = -1;

  priv->last_child1_focus = NULL;
  priv->last_child2_focus = NULL;
  priv->in_recursion = FALSE;
  priv->original_position = -1;
  priv->max_position = G_MAXINT;
  priv->child1_resize = TRUE;
  priv->child2_resize = TRUE;
  priv->child1_shrink = TRUE;
  priv->child2_shrink = TRUE;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (paned));

  /* Touch gesture */
  gesture = gtk_gesture_pan_new (GTK_ORIENTATION_HORIZONTAL);
  connect_drag_gesture_signals (paned, gesture);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (paned), GTK_EVENT_CONTROLLER (gesture));
  priv->pan_gesture = gesture;

  /* Pointer gesture */
  gesture = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  connect_drag_gesture_signals (paned, gesture);
  gtk_widget_add_controller (GTK_WIDGET (paned), GTK_EVENT_CONTROLLER (gesture));
  priv->drag_gesture = gesture;

  priv->handle_widget = gtk_gizmo_new ("separator", NULL, NULL, gtk_paned_render_handle, gtk_paned_handle_contains);
  gtk_widget_set_parent (priv->handle_widget, GTK_WIDGET (paned));
  gtk_widget_set_cursor_from_name (priv->handle_widget, "col-resize");
}

static gboolean
is_rtl (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (paned)) == GTK_TEXT_DIR_RTL)
    {
      return TRUE;
    }

  return FALSE;
}

static void
update_drag (GtkPaned *paned,
             int       xpos,
             int       ypos)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  gint pos;
  gint handle_size;
  gint size;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    pos = xpos;
  else
    pos = ypos;

  pos -= priv->drag_pos;

  if (is_rtl (paned))
    {
      gtk_widget_measure (priv->handle_widget,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &handle_size,
                          NULL, NULL);

      size = gtk_widget_get_width (GTK_WIDGET (paned)) - pos - handle_size;
    }
  else
    {
      size = pos;
    }

  size = CLAMP (size, priv->min_position, priv->max_position);

  if (size != priv->child1_size)
    gtk_paned_set_position (paned, size);
}

static gboolean
gtk_paned_focus (GtkWidget        *widget,
		 GtkDirectionType  direction)

{
  gboolean retval;
  
  /* This is a hack, but how can this be done without
   * excessive cut-and-paste from gtkcontainer.c?
   */

  gtk_widget_set_can_focus (widget, FALSE);
  retval = GTK_WIDGET_CLASS (gtk_paned_parent_class)->focus (widget, direction);
  gtk_widget_set_can_focus (widget, TRUE);

  return retval;
}

/**
 * gtk_paned_new:
 * @orientation: the paned’s orientation.
 *
 * Creates a new #GtkPaned widget.
 *
 * Returns: a new #GtkPaned.
 **/
GtkWidget *
gtk_paned_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_PANED,
                       "orientation", orientation,
                       NULL);
}

/**
 * gtk_paned_add1:
 * @paned: a paned widget
 * @child: the child to add
 *
 * Adds a child to the top or left pane with default parameters. This is
 * equivalent to
 * `gtk_paned_pack1 (paned, child, FALSE, TRUE)`.
 */
void
gtk_paned_add1 (GtkPaned  *paned,
		GtkWidget *widget)
{
  gtk_paned_pack1 (paned, widget, FALSE, TRUE);
}

/**
 * gtk_paned_add2:
 * @paned: a paned widget
 * @child: the child to add
 *
 * Adds a child to the bottom or right pane with default parameters. This
 * is equivalent to
 * `gtk_paned_pack2 (paned, child, TRUE, TRUE)`.
 */
void
gtk_paned_add2 (GtkPaned  *paned,
		GtkWidget *widget)
{
  gtk_paned_pack2 (paned, widget, TRUE, TRUE);
}

/**
 * gtk_paned_pack1:
 * @paned: a paned widget
 * @child: the child to add
 * @resize: should this child expand when the paned widget is resized.
 * @shrink: can this child be made smaller than its requisition.
 *
 * Adds a child to the top or left pane.
 */
void
gtk_paned_pack1 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!priv->child1)
    {
      priv->child1 = child;
      priv->child1_resize = resize;
      priv->child1_shrink = shrink;

      gtk_widget_insert_before (child, GTK_WIDGET (paned), priv->handle_widget);
    }
}

/**
 * gtk_paned_pack2:
 * @paned: a paned widget
 * @child: the child to add
 * @resize: should this child expand when the paned widget is resized.
 * @shrink: can this child be made smaller than its requisition.
 *
 * Adds a child to the bottom or right pane.
 */
void
gtk_paned_pack2 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!priv->child2)
    {
      priv->child2 = child;
      priv->child2_resize = resize;
      priv->child2_shrink = shrink;

      gtk_widget_insert_after (child, GTK_WIDGET (paned), priv->handle_widget);
    }
}


static void
gtk_paned_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  GtkPanedPrivate *priv;
  GtkPaned *paned;

  g_return_if_fail (GTK_IS_PANED (container));

  paned = GTK_PANED (container);
  priv = gtk_paned_get_instance_private (paned);

  if (!priv->child1)
    gtk_paned_add1 (paned, widget);
  else if (!priv->child2)
    gtk_paned_add2 (paned, widget);
  else
    g_warning ("GtkPaned cannot have more than 2 children");
}

static void
gtk_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->child1 == widget)
    {
      gtk_widget_unparent (widget);

      priv->child1 = NULL;
    }
  else if (priv->child2 == widget)
    {
      gtk_widget_unparent (widget);

      priv->child2 = NULL;
    }
}

static void
gtk_paned_forall (GtkContainer *container,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (GTK_PANED (container));

  g_return_if_fail (callback != NULL);

  if (priv->child1)
    (*callback) (priv->child1, callback_data);
  if (priv->child2)
    (*callback) (priv->child2, callback_data);
}

/**
 * gtk_paned_get_position:
 * @paned: a #GtkPaned widget
 * 
 * Obtains the position of the divider between the two panes.
 * 
 * Returns: position of the divider
 **/
gint
gtk_paned_get_position (GtkPaned  *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_val_if_fail (GTK_IS_PANED (paned), 0);

  return priv->child1_size;
}

/**
 * gtk_paned_set_position:
 * @paned: a #GtkPaned widget
 * @position: pixel position of divider, a negative value means that the position
 *            is unset.
 * 
 * Sets the position of the divider between the two panes.
 **/
void
gtk_paned_set_position (GtkPaned *paned,
			gint      position)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_if_fail (GTK_IS_PANED (paned));

  g_object_freeze_notify (G_OBJECT (paned));

  if (position >= 0)
    {
      /* We don't clamp here - the assumption is that
       * if the total allocation changes at the same time
       * as the position, the position set is with reference
       * to the new total size. If only the position changes,
       * then clamping will occur in gtk_paned_calc_position()
       */

      if (!priv->position_set)
        g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION_SET]);

      if (priv->child1_size != position)
        {
          g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION]);
          gtk_widget_queue_allocate (GTK_WIDGET (paned));
        }

      priv->child1_size = position;
      priv->position_set = TRUE;
    }
  else
    {
      if (priv->position_set)
        g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION_SET]);

      priv->position_set = FALSE;
    }

  g_object_thaw_notify (G_OBJECT (paned));

#ifdef G_OS_WIN32
  /* Hacky work-around for bug #144269 */
  if (priv->child2 != NULL)
    {
      gtk_widget_queue_draw (priv->child2);
    }
#endif
}

/**
 * gtk_paned_get_child1:
 * @paned: a #GtkPaned widget
 * 
 * Obtains the first child of the paned widget.
 * 
 * Returns: (nullable) (transfer none): first child, or %NULL if it is not set.
 **/
GtkWidget *
gtk_paned_get_child1 (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return priv->child1;
}

/**
 * gtk_paned_get_child2:
 * @paned: a #GtkPaned widget
 * 
 * Obtains the second child of the paned widget.
 * 
 * Returns: (nullable) (transfer none): second child, or %NULL if it is not set.
 **/
GtkWidget *
gtk_paned_get_child2 (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return priv->child2;
}

static void
gtk_paned_calc_position (GtkPaned *paned,
                         gint      allocation,
                         gint      child1_req,
                         gint      child2_req)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  gint old_position;
  gint old_min_position;
  gint old_max_position;

  old_position = priv->child1_size;
  old_min_position = priv->min_position;
  old_max_position = priv->max_position;

  gtk_paned_compute_position (paned,
                              allocation, child1_req, child2_req,
                              &priv->min_position, &priv->max_position,
                              &priv->child1_size);

  gtk_widget_set_child_visible (priv->child1, priv->child1_size != 0);
  gtk_widget_set_child_visible (priv->child2, priv->child1_size != allocation);

  g_object_freeze_notify (G_OBJECT (paned));
  if (priv->child1_size != old_position)
    g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_POSITION]);
  if (priv->min_position != old_min_position)
    g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_MIN_POSITION]);
  if (priv->max_position != old_max_position)
    g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_MAX_POSITION]);
  g_object_thaw_notify (G_OBJECT (paned));

  priv->last_allocation = allocation;
}

static void
gtk_paned_set_saved_focus (GtkPaned *paned, GtkWidget *widget)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->saved_focus)
    g_object_remove_weak_pointer (G_OBJECT (priv->saved_focus),
				  (gpointer *)&(priv->saved_focus));

  priv->saved_focus = widget;

  if (priv->saved_focus)
    g_object_add_weak_pointer (G_OBJECT (priv->saved_focus),
			       (gpointer *)&(priv->saved_focus));
}

static void
gtk_paned_set_first_paned (GtkPaned *paned, GtkPaned *first_paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->first_paned)
    g_object_remove_weak_pointer (G_OBJECT (priv->first_paned),
				  (gpointer *)&(priv->first_paned));

  priv->first_paned = first_paned;

  if (priv->first_paned)
    g_object_add_weak_pointer (G_OBJECT (priv->first_paned),
			       (gpointer *)&(priv->first_paned));
}

static void
gtk_paned_set_last_child1_focus (GtkPaned *paned, GtkWidget *widget)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->last_child1_focus)
    g_object_remove_weak_pointer (G_OBJECT (priv->last_child1_focus),
				  (gpointer *)&(priv->last_child1_focus));

  priv->last_child1_focus = widget;

  if (priv->last_child1_focus)
    g_object_add_weak_pointer (G_OBJECT (priv->last_child1_focus),
			       (gpointer *)&(priv->last_child1_focus));
}

static void
gtk_paned_set_last_child2_focus (GtkPaned *paned, GtkWidget *widget)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (priv->last_child2_focus)
    g_object_remove_weak_pointer (G_OBJECT (priv->last_child2_focus),
				  (gpointer *)&(priv->last_child2_focus));

  priv->last_child2_focus = widget;

  if (priv->last_child2_focus)
    g_object_add_weak_pointer (G_OBJECT (priv->last_child2_focus),
			       (gpointer *)&(priv->last_child2_focus));
}

static GtkWidget *
paned_get_focus_widget (GtkPaned *paned)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (paned)));
  if (GTK_IS_WINDOW (toplevel))
    return gtk_window_get_focus (GTK_WINDOW (toplevel));

  return NULL;
}

static void
gtk_paned_set_focus_child (GtkContainer *container,
			   GtkWidget    *focus_child)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  GtkWidget *container_focus_child;

  g_return_if_fail (GTK_IS_PANED (container));

  if (focus_child == NULL)
    {
      GtkWidget *last_focus;
      GtkWidget *w;

      last_focus = paned_get_focus_widget (paned);

      if (last_focus)
	{
	  /* If there is one or more paned widgets between us and the
	   * focus widget, we want the topmost of those as last_focus
	   */
	  for (w = last_focus; w != GTK_WIDGET (paned); w = gtk_widget_get_parent (w))
	    if (GTK_IS_PANED (w))
	      last_focus = w;

          container_focus_child = gtk_widget_get_focus_child (GTK_WIDGET (container));
          if (container_focus_child == priv->child1)
	    gtk_paned_set_last_child1_focus (paned, last_focus);
	  else if (container_focus_child == priv->child2)
	    gtk_paned_set_last_child2_focus (paned, last_focus);
	}
    }

  if (GTK_CONTAINER_CLASS (gtk_paned_parent_class)->set_focus_child)
    GTK_CONTAINER_CLASS (gtk_paned_parent_class)->set_focus_child (container, focus_child);
}

static void
gtk_paned_get_cycle_chain (GtkPaned          *paned,
			   GtkDirectionType   direction,
			   GList            **widgets)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  GtkWidget *ancestor = NULL;
  GtkWidget *focus_child;
  GtkWidget *parent;
  GtkWidget *widget = GTK_WIDGET (paned);
  GList *temp_list = NULL;
  GList *list;

  if (priv->in_recursion)
    return;

  g_assert (widgets != NULL);

  if (priv->last_child1_focus &&
      !gtk_widget_is_ancestor (priv->last_child1_focus, widget))
    {
      gtk_paned_set_last_child1_focus (paned, NULL);
    }

  if (priv->last_child2_focus &&
      !gtk_widget_is_ancestor (priv->last_child2_focus, widget))
    {
      gtk_paned_set_last_child2_focus (paned, NULL);
    }

  parent = gtk_widget_get_parent (widget);
  if (parent)
    ancestor = gtk_widget_get_ancestor (parent, GTK_TYPE_PANED);

  /* The idea here is that temp_list is a list of widgets we want to cycle
   * to. The list is prioritized so that the first element is our first
   * choice, the next our second, and so on.
   *
   * We can't just use g_list_reverse(), because we want to try
   * priv->last_child?_focus before priv->child?, both when we
   * are going forward and backward.
   */
  focus_child = gtk_widget_get_focus_child (GTK_WIDGET (paned));
  if (direction == GTK_DIR_TAB_FORWARD)
    {
      if (focus_child == priv->child1)
	{
	  temp_list = g_list_append (temp_list, priv->last_child2_focus);
	  temp_list = g_list_append (temp_list, priv->child2);
	  temp_list = g_list_append (temp_list, ancestor);
	}
      else if (focus_child == priv->child2)
	{
	  temp_list = g_list_append (temp_list, ancestor);
	  temp_list = g_list_append (temp_list, priv->last_child1_focus);
	  temp_list = g_list_append (temp_list, priv->child1);
	}
      else
	{
	  temp_list = g_list_append (temp_list, priv->last_child1_focus);
	  temp_list = g_list_append (temp_list, priv->child1);
	  temp_list = g_list_append (temp_list, priv->last_child2_focus);
	  temp_list = g_list_append (temp_list, priv->child2);
	  temp_list = g_list_append (temp_list, ancestor);
	}
    }
  else
    {
      if (focus_child == priv->child1)
	{
	  temp_list = g_list_append (temp_list, ancestor);
	  temp_list = g_list_append (temp_list, priv->last_child2_focus);
	  temp_list = g_list_append (temp_list, priv->child2);
	}
      else if (focus_child == priv->child2)
	{
	  temp_list = g_list_append (temp_list, priv->last_child1_focus);
	  temp_list = g_list_append (temp_list, priv->child1);
	  temp_list = g_list_append (temp_list, ancestor);
	}
      else
	{
	  temp_list = g_list_append (temp_list, priv->last_child2_focus);
	  temp_list = g_list_append (temp_list, priv->child2);
	  temp_list = g_list_append (temp_list, priv->last_child1_focus);
	  temp_list = g_list_append (temp_list, priv->child1);
	  temp_list = g_list_append (temp_list, ancestor);
	}
    }

  /* Walk the list and expand all the paned widgets. */
  for (list = temp_list; list != NULL; list = list->next)
    {
      widget = list->data;

      if (widget)
	{
	  if (GTK_IS_PANED (widget))
	    {
	      priv->in_recursion = TRUE;
	      gtk_paned_get_cycle_chain (GTK_PANED (widget), direction, widgets);
	      priv->in_recursion = FALSE;
	    }
	  else
	    {
	      *widgets = g_list_append (*widgets, widget);
	    }
	}
    }

  g_list_free (temp_list);
}

static gboolean
gtk_paned_cycle_child_focus (GtkPaned *paned,
			     gboolean  reversed)
{
  GList *cycle_chain = NULL;
  GList *list;
  
  GtkDirectionType direction = reversed? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD;

  /* ignore f6 if the handle is focused */
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    return TRUE;
  
  /* we can't just let the event propagate up the hierarchy,
   * because the paned will want to cycle focus _unless_ an
   * ancestor paned handles the event
   */
  gtk_paned_get_cycle_chain (paned, direction, &cycle_chain);

  for (list = cycle_chain; list != NULL; list = list->next)
    if (gtk_widget_child_focus (GTK_WIDGET (list->data), direction))
      break;

  g_list_free (cycle_chain);
  
  return TRUE;
}

static void
get_child_panes (GtkWidget  *widget,
		 GList     **panes)
{
  if (!widget || !gtk_widget_get_realized (widget))
    return;

  if (GTK_IS_PANED (widget))
    {
      GtkPaned *paned = GTK_PANED (widget);
      GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

      get_child_panes (priv->child1, panes);
      *panes = g_list_prepend (*panes, widget);
      get_child_panes (priv->child2, panes);
    }
  else if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_forall (GTK_CONTAINER (widget),
                            (GtkCallback)get_child_panes, panes);
    }
}

static GList *
get_all_panes (GtkPaned *paned)
{
  GtkPaned *topmost = NULL;
  GList *result = NULL;
  GtkWidget *w;

  for (w = GTK_WIDGET (paned); w != NULL; w = gtk_widget_get_parent (w))
    {
      if (GTK_IS_PANED (w))
	topmost = GTK_PANED (w);
    }

  g_assert (topmost);

  get_child_panes (GTK_WIDGET (topmost), &result);

  return g_list_reverse (result);
}

static void
gtk_paned_find_neighbours (GtkPaned  *paned,
			   GtkPaned **next,
			   GtkPaned **prev)
{
  GList *all_panes;
  GList *this_link;

  all_panes = get_all_panes (paned);
  g_assert (all_panes);

  this_link = g_list_find (all_panes, paned);

  g_assert (this_link);
  
  if (this_link->next)
    *next = this_link->next->data;
  else
    *next = all_panes->data;

  if (this_link->prev)
    *prev = this_link->prev->data;
  else
    *prev = g_list_last (all_panes)->data;

  g_list_free (all_panes);
}

static gboolean
gtk_paned_move_handle (GtkPaned      *paned,
		       GtkScrollType  scroll)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      gint old_position;
      gint new_position;
      gint increment;
      
      enum {
	SINGLE_STEP_SIZE = 1,
	PAGE_STEP_SIZE   = 75
      };
      
      new_position = old_position = gtk_paned_get_position (paned);
      increment = 0;
      
      switch (scroll)
	{
	case GTK_SCROLL_STEP_LEFT:
	case GTK_SCROLL_STEP_UP:
	case GTK_SCROLL_STEP_BACKWARD:
	  increment = - SINGLE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_STEP_RIGHT:
	case GTK_SCROLL_STEP_DOWN:
	case GTK_SCROLL_STEP_FORWARD:
	  increment = SINGLE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_PAGE_LEFT:
	case GTK_SCROLL_PAGE_UP:
	case GTK_SCROLL_PAGE_BACKWARD:
	  increment = - PAGE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_PAGE_RIGHT:
	case GTK_SCROLL_PAGE_DOWN:
	case GTK_SCROLL_PAGE_FORWARD:
	  increment = PAGE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_START:
	  new_position = priv->min_position;
	  break;
	  
	case GTK_SCROLL_END:
	  new_position = priv->max_position;
	  break;

	case GTK_SCROLL_NONE:
	case GTK_SCROLL_JUMP:
	default:
	  break;
	}

      if (increment)
	{
	  if (is_rtl (paned))
	    increment = -increment;
	  
	  new_position = old_position + increment;
	}
      
      new_position = CLAMP (new_position, priv->min_position, priv->max_position);
      
      if (old_position != new_position)
	gtk_paned_set_position (paned, new_position);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_paned_restore_focus (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      if (priv->saved_focus &&
	  gtk_widget_get_sensitive (priv->saved_focus))
	{
	  gtk_widget_grab_focus (priv->saved_focus);
	}
      else
	{
	  /* the saved focus is somehow not available for focusing,
	   * try
	   *   1) tabbing into the paned widget
	   * if that didn't work,
	   *   2) unset focus for the window if there is one
	   */
	  
	  if (!gtk_widget_child_focus (GTK_WIDGET (paned), GTK_DIR_TAB_FORWARD))
	    {
              GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (paned));
              gtk_root_set_focus (root, NULL);
	    }
	}
      
      gtk_paned_set_saved_focus (paned, NULL);
      gtk_paned_set_first_paned (paned, NULL);
    }
}

static gboolean
gtk_paned_accept_position (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      priv->original_position = -1;
      gtk_paned_restore_focus (paned);

      return TRUE;
    }

  return FALSE;
}


static gboolean
gtk_paned_cancel_position (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      if (priv->original_position != -1)
	{
	  gtk_paned_set_position (paned, priv->original_position);
	  priv->original_position = -1;
	}

      gtk_paned_restore_focus (paned);
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_cycle_handle_focus (GtkPaned *paned,
			      gboolean  reversed)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  GtkPaned *next, *prev;

  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      GtkPaned *focus = NULL;

      if (!priv->first_paned)
	{
	  /* The first_pane has disappeared. As an ad-hoc solution,
	   * we make the currently focused paned the first_paned. To the
	   * user this will seem like the paned cycling has been reset.
	   */
	  
	  gtk_paned_set_first_paned (paned, paned);
	}
      
      gtk_paned_find_neighbours (paned, &next, &prev);

      if (reversed && prev &&
	  prev != paned && paned != priv->first_paned)
	{
	  focus = prev;
	}
      else if (!reversed && next &&
	       next != paned && next != priv->first_paned)
	{
	  focus = next;
	}
      else
	{
	  gtk_paned_accept_position (paned);
	  return TRUE;
	}

      g_assert (focus);
      
      gtk_paned_set_saved_focus (focus, priv->saved_focus);
      gtk_paned_set_first_paned (focus, priv->first_paned);
      
      gtk_paned_set_saved_focus (paned, NULL);
      gtk_paned_set_first_paned (paned, NULL);
      
      gtk_widget_grab_focus (GTK_WIDGET (focus));
      
      if (!gtk_widget_is_focus (GTK_WIDGET (paned)))
	{
	  priv->original_position = -1;
	  priv->original_position = gtk_paned_get_position (focus);
	}
    }
  else
    {
      GtkPaned *focus;
      GtkPaned *first;
      GtkWidget *focus_child;

      gtk_paned_find_neighbours (paned, &next, &prev);
      focus_child = gtk_widget_get_focus_child (GTK_WIDGET (paned));

      if (focus_child == priv->child1)
	{
	  if (reversed)
	    {
	      focus = prev;
	      first = paned;
	    }
	  else
	    {
	      focus = paned;
	      first = paned;
	    }
	}
      else if (focus_child == priv->child2)
	{
	  if (reversed)
	    {
	      focus = paned;
	      first = next;
	    }
	  else
	    {
	      focus = next;
	      first = next;
	    }
	}
      else
	{
	  /* Focus is not inside this paned, and we don't have focus.
	   * Presumably this happened because the application wants us
	   * to start keyboard navigating.
	   */
	  focus = paned;

	  if (reversed)
	    first = paned;
	  else
	    first = next;
	}

      gtk_paned_set_saved_focus (focus, gtk_root_get_focus (gtk_widget_get_root (GTK_WIDGET (paned))));
      gtk_paned_set_first_paned (focus, first);
      priv->original_position = gtk_paned_get_position (focus);

      gtk_widget_grab_focus (GTK_WIDGET (focus));
   }

  return TRUE;
}

static gboolean
gtk_paned_toggle_handle_focus (GtkPaned *paned)
{
  /* This function/signal has the wrong name. It is called when you
   * press Tab or Shift-Tab and what we do is act as if
   * the user pressed Return and then Tab or Shift-Tab
   */
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    gtk_paned_accept_position (paned);

  return FALSE;
}

/**
 * gtk_paned_set_wide_handle:
 * @paned: a #GtkPaned
 * @wide: the new value for the #GtkPaned:wide-handle property
 *
 * Sets the #GtkPaned:wide-handle property.
 */
void
gtk_paned_set_wide_handle (GtkPaned *paned,
                           gboolean  wide)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);
  gboolean old_wide;

  g_return_if_fail (GTK_IS_PANED (paned));

  old_wide = gtk_paned_get_wide_handle (paned);
  if (old_wide != wide)
    {
      if (wide)
        gtk_style_context_add_class (gtk_widget_get_style_context (priv->handle_widget),
                                     GTK_STYLE_CLASS_WIDE);
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (priv->handle_widget),
                                        GTK_STYLE_CLASS_WIDE);

      g_object_notify_by_pspec (G_OBJECT (paned), paned_props[PROP_WIDE_HANDLE]);
    }
}

/**
 * gtk_paned_get_wide_handle:
 * @paned: a #GtkPaned
 *
 * Gets the #GtkPaned:wide-handle property.
 *
 * Returns: %TRUE if the paned should have a wide handle
 */
gboolean
gtk_paned_get_wide_handle (GtkPaned *paned)
{
  GtkPanedPrivate *priv = gtk_paned_get_instance_private (paned);

  g_return_val_if_fail (GTK_IS_PANED (paned), FALSE);

  return gtk_style_context_has_class (gtk_widget_get_style_context (priv->handle_widget),
                                      GTK_STYLE_CLASS_WIDE);
}
