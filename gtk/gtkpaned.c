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
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkwindow.h"
#include "gtktypebuiltins.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwidgetprivate.h"
#include "a11y/gtkpanedaccessible.h"

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
 * <example>
 * <title>Creating a paned widget with minimum sizes.</title>
 * <programlisting>
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
 * </programlisting>
 * </example>
 */

enum {
  CHILD1,
  CHILD2
};

struct _GtkPanedPrivate
{
  GtkPaned       *first_paned;
  GtkWidget      *child1;
  GtkWidget      *child2;
  GdkWindow      *child1_window;
  GdkWindow      *child2_window;
  GtkWidget      *last_child1_focus;
  GtkWidget      *last_child2_focus;
  GtkWidget      *saved_focus;
  GtkOrientation  orientation;

  GdkCursorType  cursor_type;
  GdkDevice     *grab_device;
  GdkRectangle   handle_pos;
  GdkWindow     *handle;

  gint          child1_size;
  gint          drag_pos;
  gint          last_allocation;
  gint          max_position;
  gint          min_position;
  gint          original_position;

  guint32       grab_time;

  guint         handle_prelit : 1;
  guint         in_drag       : 1;
  guint         in_recursion  : 1;
  guint         child1_resize : 1;
  guint         child1_shrink : 1;
  guint         child2_resize : 1;
  guint         child2_shrink : 1;
  guint         position_set  : 1;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_POSITION,
  PROP_POSITION_SET,
  PROP_MIN_POSITION,
  PROP_MAX_POSITION
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_RESIZE,
  CHILD_PROP_SHRINK
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
static void     gtk_paned_set_child_property    (GtkContainer     *container,
                                                 GtkWidget        *child,
                                                 guint             property_id,
                                                 const GValue     *value,
                                                 GParamSpec       *pspec);
static void     gtk_paned_get_child_property    (GtkContainer     *container,
                                                 GtkWidget        *child,
                                                 guint             property_id,
                                                 GValue           *value,
                                                 GParamSpec       *pspec);
static void     gtk_paned_finalize              (GObject          *object);

static void     gtk_paned_get_preferred_width   (GtkWidget        *widget,
                                                 gint             *minimum,
                                                 gint             *natural);
static void     gtk_paned_get_preferred_height  (GtkWidget        *widget,
                                                 gint             *minimum,
                                                 gint             *natural);
static void     gtk_paned_get_preferred_width_for_height
                                                (GtkWidget        *widget,
                                                 gint              height,
                                                 gint             *minimum,
                                                 gint             *natural);
static void     gtk_paned_get_preferred_height_for_width
                                                (GtkWidget        *widget,
                                                 gint              width,
                                                 gint             *minimum,
                                                 gint              *natural);

static void     gtk_paned_size_allocate         (GtkWidget        *widget,
                                                 GtkAllocation    *allocation);
static void     gtk_paned_realize               (GtkWidget        *widget);
static void     gtk_paned_unrealize             (GtkWidget        *widget);
static void     gtk_paned_map                   (GtkWidget        *widget);
static void     gtk_paned_unmap                 (GtkWidget        *widget);
static void     gtk_paned_state_flags_changed   (GtkWidget        *widget,
                                                 GtkStateFlags     previous_state);
static gboolean gtk_paned_draw                  (GtkWidget        *widget,
						 cairo_t          *cr);
static gboolean gtk_paned_enter                 (GtkWidget        *widget,
						 GdkEventCrossing *event);
static gboolean gtk_paned_leave                 (GtkWidget        *widget,
						 GdkEventCrossing *event);
static gboolean gtk_paned_button_press          (GtkWidget        *widget,
						 GdkEventButton   *event);
static gboolean gtk_paned_button_release        (GtkWidget        *widget,
						 GdkEventButton   *event);
static gboolean gtk_paned_motion                (GtkWidget        *widget,
						 GdkEventMotion   *event);
static gboolean gtk_paned_focus                 (GtkWidget        *widget,
						 GtkDirectionType  direction);
static gboolean gtk_paned_grab_broken           (GtkWidget          *widget,
						 GdkEventGrabBroken *event);
static void     gtk_paned_add                   (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_paned_remove                (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_paned_forall                (GtkContainer     *container,
						 gboolean          include_internals,
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
static void     gtk_paned_grab_notify           (GtkWidget        *widget,
		                                 gboolean          was_grabbed);


G_DEFINE_TYPE_WITH_CODE (GtkPaned, gtk_paned, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkPaned)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))

static guint signals[LAST_SIGNAL] = { 0 };


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

  widget_class->get_preferred_width = gtk_paned_get_preferred_width;
  widget_class->get_preferred_height = gtk_paned_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_paned_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_paned_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_paned_size_allocate;
  widget_class->realize = gtk_paned_realize;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->map = gtk_paned_map;
  widget_class->unmap = gtk_paned_unmap;
  widget_class->draw = gtk_paned_draw;
  widget_class->focus = gtk_paned_focus;
  widget_class->enter_notify_event = gtk_paned_enter;
  widget_class->leave_notify_event = gtk_paned_leave;
  widget_class->button_press_event = gtk_paned_button_press;
  widget_class->button_release_event = gtk_paned_button_release;
  widget_class->motion_notify_event = gtk_paned_motion;
  widget_class->grab_broken_event = gtk_paned_grab_broken;
  widget_class->grab_notify = gtk_paned_grab_notify;
  widget_class->state_flags_changed = gtk_paned_state_flags_changed;

  container_class->add = gtk_paned_add;
  container_class->remove = gtk_paned_remove;
  container_class->forall = gtk_paned_forall;
  container_class->child_type = gtk_paned_child_type;
  container_class->set_focus_child = gtk_paned_set_focus_child;
  container_class->set_child_property = gtk_paned_set_child_property;
  container_class->get_child_property = gtk_paned_get_child_property;
  gtk_container_class_handle_border_width (container_class);

  paned_class->cycle_child_focus = gtk_paned_cycle_child_focus;
  paned_class->toggle_handle_focus = gtk_paned_toggle_handle_focus;
  paned_class->move_handle = gtk_paned_move_handle;
  paned_class->cycle_handle_focus = gtk_paned_cycle_handle_focus;
  paned_class->accept_position = gtk_paned_accept_position;
  paned_class->cancel_position = gtk_paned_cancel_position;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_property (object_class,
				   PROP_POSITION,
				   g_param_spec_int ("position",
						     P_("Position"),
						     P_("Position of paned separator in pixels (0 means all the way to the left/top)"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_POSITION_SET,
				   g_param_spec_boolean ("position-set",
							 P_("Position Set"),
							 P_("TRUE if the Position property should be used"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("handle-size",
							     P_("Handle Size"),
							     P_("Width of handle"),
							     0,
							     G_MAXINT,
							     5,
							     GTK_PARAM_READABLE));
  /**
   * GtkPaned:min-position:
   *
   * The smallest possible value for the position property. This property is derived from the
   * size and shrinkability of the widget's children.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
				   PROP_MIN_POSITION,
				   g_param_spec_int ("min-position",
						     P_("Minimal Position"),
						     P_("Smallest possible value for the \"position\" property"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READABLE));

  /**
   * GtkPaned:max-position:
   *
   * The largest possible value for the position property. This property is derived from the
   * size and shrinkability of the widget's children.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
				   PROP_MAX_POSITION,
				   g_param_spec_int ("max-position",
						     P_("Maximal Position"),
						     P_("Largest possible value for the \"position\" property"),
						     0,
						     G_MAXINT,
						     G_MAXINT,
						     GTK_PARAM_READABLE));

  /**
   * GtkPaned:resize:
   *
   * The "resize" child property determines whether the child expands and
   * shrinks along with the paned widget.
   *
   * Since: 2.4
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_RESIZE,
					      g_param_spec_boolean ("resize", 
								    P_("Resize"),
								    P_("If TRUE, the child expands and shrinks along with the paned widget"),
								    TRUE,
								    GTK_PARAM_READWRITE));

  /**
   * GtkPaned:shrink:
   *
   * The "shrink" child property determines whether the child can be made
   * smaller than its requisition.
   *
   * Since: 2.4
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_SHRINK,
					      g_param_spec_boolean ("shrink", 
								    P_("Shrink"),
								    P_("If TRUE, the child can be made smaller than its requisition"),
								    TRUE,
								    GTK_PARAM_READWRITE));

  /**
   * GtkPaned::cycle-child-focus:
   * @widget: the object that received the signal
   * @reversed: whether cycling backward or forward
   *
   * The ::cycle-child-focus signal is a 
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to cycle the focus between the children of the paned.
   *
   * The default binding is f6.
   *
   * Since: 2.0
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
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to accept the current position of the handle and then 
   * move focus to the next widget in the focus chain.
   *
   * The default binding is Tab.
   *
   * Since: 2.0
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
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to move the handle when the user is using key bindings 
   * to move it.
   *
   * Since: 2.0
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
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to cycle whether the paned should grab focus to allow
   * the user to change position of the handle by using key bindings.
   *
   * The default binding for this signal is f8.
   *
   * Since: 2.0
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
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to accept the current position of the handle when 
   * moving it using key bindings.
   *
   * The default binding for this signal is Return or Space.
   *
   * Since: 2.0
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
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to cancel moving the position of the handle using key 
   * bindings. The position of the handle will be reset to the value prior to 
   * moving it.
   *
   * The default binding for this signal is Escape.
   *
   * Since: 2.0
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
}

static GType
gtk_paned_child_type (GtkContainer *container)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = paned->priv;

  if (!priv->child1 || !priv->child2)
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static void
gtk_paned_init (GtkPaned *paned)
{
  GtkPanedPrivate *priv;

  gtk_widget_set_has_window (GTK_WIDGET (paned), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (paned), TRUE);

  /* We only need to redraw when the handle position moves, which is
   * independent of the overall allocation of the GtkPaned
   */
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (paned), FALSE);

  paned->priv = gtk_paned_get_instance_private (paned);
  priv = paned->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->cursor_type = GDK_SB_H_DOUBLE_ARROW;

  priv->child1 = NULL;
  priv->child2 = NULL;
  priv->handle = NULL;

  priv->handle_pos.width = 5;
  priv->handle_pos.height = 5;
  priv->position_set = FALSE;
  priv->last_allocation = -1;
  priv->in_drag = FALSE;

  priv->last_child1_focus = NULL;
  priv->last_child2_focus = NULL;
  priv->in_recursion = FALSE;
  priv->handle_prelit = FALSE;
  priv->original_position = -1;

  priv->handle_pos.x = -1;
  priv->handle_pos.y = -1;

  priv->drag_pos = -1;
}

static void
gtk_paned_set_property (GObject        *object,
			guint           prop_id,
			const GValue   *value,
			GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);
  GtkPanedPrivate *priv = paned->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      priv->orientation = g_value_get_enum (value);
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (paned));

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        priv->cursor_type = GDK_SB_H_DOUBLE_ARROW;
      else
        priv->cursor_type = GDK_SB_V_DOUBLE_ARROW;

      /* state_flags_changed updates the cursor */
      gtk_paned_state_flags_changed (GTK_WIDGET (paned), 0);
      gtk_widget_queue_resize (GTK_WIDGET (paned));
      break;
    case PROP_POSITION:
      gtk_paned_set_position (paned, g_value_get_int (value));
      break;
    case PROP_POSITION_SET:
      priv->position_set = g_value_get_boolean (value);
      gtk_widget_queue_resize_no_redraw (GTK_WIDGET (paned));
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
  GtkPanedPrivate *priv = paned->priv;

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_set_child_property (GtkContainer    *container,
			      GtkWidget       *child,
			      guint            property_id,
			      const GValue    *value,
			      GParamSpec      *pspec)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = paned->priv;
  gboolean old_value, new_value;

  g_assert (child == priv->child1 || child == priv->child2);

  new_value = g_value_get_boolean (value);
  switch (property_id)
    {
    case CHILD_PROP_RESIZE:
      if (child == priv->child1)
	{
	  old_value = priv->child1_resize;
	  priv->child1_resize = new_value;
	}
      else
	{
	  old_value = priv->child2_resize;
	  priv->child2_resize = new_value;
	}
      break;
    case CHILD_PROP_SHRINK:
      if (child == priv->child1)
	{
	  old_value = priv->child1_shrink;
	  priv->child1_shrink = new_value;
	}
      else
	{
	  old_value = priv->child2_shrink;
	  priv->child2_shrink = new_value;
	}
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      old_value = -1; /* quiet gcc */
      break;
    }
  if (old_value != new_value)
    gtk_widget_queue_resize_no_redraw (GTK_WIDGET (container));
}

static void
gtk_paned_get_child_property (GtkContainer *container,
			      GtkWidget    *child,
			      guint         property_id,
			      GValue       *value,
			      GParamSpec   *pspec)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = paned->priv;

  g_assert (child == priv->child1 || child == priv->child2);
  
  switch (property_id)
    {
    case CHILD_PROP_RESIZE:
      if (child == priv->child1)
	g_value_set_boolean (value, priv->child1_resize);
      else
	g_value_set_boolean (value, priv->child2_resize);
      break;
    case CHILD_PROP_SHRINK:
      if (child == priv->child1)
	g_value_set_boolean (value, priv->child1_shrink);
      else
	g_value_set_boolean (value, priv->child2_shrink);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_paned_finalize (GObject *object)
{
  GtkPaned *paned = GTK_PANED (object);
  
  gtk_paned_set_saved_focus (paned, NULL);
  gtk_paned_set_first_paned (paned, NULL);

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
  GtkPanedPrivate *priv = paned->priv;
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
  GtkPanedPrivate *priv = paned->priv;
  gint child_min, child_nat;

  *minimum = *natural = 0;

  if (priv->child1 && gtk_widget_get_visible (priv->child1))
    {
      _gtk_widget_get_preferred_size_for_size (priv->child1, priv->orientation, size, &child_min, &child_nat, NULL, NULL);
      if (priv->child1_shrink)
        *minimum = 0;
      else
        *minimum = child_min;
      *natural = child_nat;
    }

  if (priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      _gtk_widget_get_preferred_size_for_size (priv->child2, priv->orientation, size, &child_min, &child_nat, NULL, NULL);

      if (!priv->child2_shrink)
        *minimum += child_min;
      *natural += child_nat;
    }

  if (priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      gint handle_size;

      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

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
  GtkPanedPrivate *priv = paned->priv;
  gint for_child1, for_child2;
  gint child_min, child_nat;

  if (size > -1 &&
      priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      gint child1_req, child2_req;
      gint handle_size;

      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

      _gtk_widget_get_preferred_size_for_size (priv->child1, priv->orientation, -1, &child1_req, NULL, NULL, NULL);
      _gtk_widget_get_preferred_size_for_size (priv->child2, priv->orientation, -1, &child2_req, NULL, NULL, NULL);

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
      _gtk_widget_get_preferred_size_for_size (priv->child1,
                                               OPPOSITE_ORIENTATION (priv->orientation),
                                               for_child1,
                                               &child_min, &child_nat,
                                               NULL, NULL);
      
      *minimum = child_min;
      *natural = child_nat;
    }

  if (priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      _gtk_widget_get_preferred_size_for_size (priv->child2,
                                               OPPOSITE_ORIENTATION (priv->orientation),
                                               for_child2,
                                               &child_min, &child_nat,
                                               NULL, NULL);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_paned_get_preferred_size (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              gint            size,
                              gint           *minimum,
                              gint           *natural)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (orientation == priv->orientation)
    return gtk_paned_get_preferred_size_for_orientation (widget, size, minimum, natural);
  else
    return gtk_paned_get_preferred_size_for_opposite_orientation (widget, size, minimum, natural);
}

static void
gtk_paned_get_preferred_width (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  gtk_paned_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, -1, minimum, natural);
}

static void
gtk_paned_get_preferred_height (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  gtk_paned_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, -1, minimum, natural);
}

static void
gtk_paned_get_preferred_width_for_height (GtkWidget *widget,
                                          gint       height,
                                          gint      *minimum,
                                          gint      *natural)
{
  gtk_paned_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, height, minimum, natural);
}

static void
gtk_paned_get_preferred_height_for_width (GtkWidget *widget,
                                          gint       width,
                                          gint      *minimum,
                                          gint      *natural)
{
  gtk_paned_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, width, minimum, natural);
}

static void
flip_child (GtkWidget     *widget,
            GtkAllocation *child_pos)
{
  GtkAllocation allocation;
  gint x, width;

  gtk_widget_get_allocation (widget, &allocation);
  x = allocation.x;
  width = allocation.width;

  child_pos->x = 2 * x + width - child_pos->x - child_pos->width;
}

static void
gtk_paned_set_child_visible (GtkPaned  *paned,
                             guint      id,
                             gboolean   visible)
{
  GtkPanedPrivate *priv = paned->priv;
  GtkWidget *child;

  child = id == CHILD1 ? priv->child1 : priv->child2;

  if (child == NULL)
    return;

  gtk_widget_set_child_visible (child, visible);

  if (gtk_widget_get_mapped (GTK_WIDGET (paned)))
    {
      GdkWindow *window = id == CHILD1 ? priv->child1_window : priv->child2_window;

      if (visible != gdk_window_is_visible (window))
        {
          if (visible)
            gdk_window_show (window);
          else
            gdk_window_hide (window);
        }
    }
}

static void
gtk_paned_child_allocate (GtkWidget           *child,
                          GdkWindow           *child_window, /* can be NULL */
                          const GtkAllocation *window_allocation,
                          GtkAllocation       *child_allocation)
{
  if (child_window)
    gdk_window_move_resize (child_window,
                            window_allocation->x, window_allocation->y,
                            window_allocation->width, window_allocation->height);

  gtk_widget_size_allocate (child, child_allocation);
}

static void
gtk_paned_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  gtk_widget_set_allocation (widget, allocation);

  if (priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      GtkAllocation child1_allocation, window1_allocation;
      GtkAllocation child2_allocation, window2_allocation;
      GtkAllocation priv_child1_allocation;
      GdkRectangle old_handle_pos;
      gint handle_size;

      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

      old_handle_pos = priv->handle_pos;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gint child1_width, child2_width;

          gtk_widget_get_preferred_width_for_height (priv->child1,
                                                     allocation->height,
                                                     &child1_width, NULL);
          gtk_widget_get_preferred_width_for_height (priv->child2,
                                                     allocation->height,
                                                     &child2_width, NULL);

          gtk_paned_calc_position (paned,
                                   MAX (1, allocation->width - handle_size),
                                   child1_width,
                                   child2_width);

          priv->handle_pos.x = allocation->x + priv->child1_size;
          priv->handle_pos.y = allocation->y;
          priv->handle_pos.width = handle_size;
          priv->handle_pos.height = allocation->height;

          window1_allocation.height = window2_allocation.height = allocation->height;
          window1_allocation.width = MAX (1, priv->child1_size);
          window1_allocation.x = allocation->x;
          window1_allocation.y = window2_allocation.y = allocation->y;

          window2_allocation.x = window1_allocation.x + priv->child1_size + priv->handle_pos.width;
          window2_allocation.width = MAX (1, allocation->x + allocation->width - window2_allocation.x);

          if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL)
            {
              flip_child (widget, &(window2_allocation));
              flip_child (widget, &(window1_allocation));
              flip_child (widget, &(priv->handle_pos));
            }

          child1_allocation.x = child1_allocation.y = 0;
          child1_allocation.width = window1_allocation.width;
          child1_allocation.height = window1_allocation.height;
          if (child1_width > child1_allocation.width)
            {
              if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_LTR)
                child1_allocation.x -= child1_width - child1_allocation.width;
              child1_allocation.width = child1_width;
            }

          child2_allocation.x = child2_allocation.y = 0;
          child2_allocation.width = window2_allocation.width;
          child2_allocation.height = window2_allocation.height;
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

          gtk_widget_get_preferred_height_for_width (priv->child1,
                                                     allocation->width,
                                                     &child1_height, NULL);
          gtk_widget_get_preferred_height_for_width (priv->child2,
                                                     allocation->width,
                                                     &child2_height, NULL);

          gtk_paned_calc_position (paned,
                                   MAX (1, allocation->height - handle_size),
                                   child1_height,
                                   child2_height);

          priv->handle_pos.x = allocation->x;
          priv->handle_pos.y = allocation->y + priv->child1_size;
          priv->handle_pos.width = allocation->width;
          priv->handle_pos.height = handle_size;

          window1_allocation.width = window2_allocation.width = allocation->width;
          window1_allocation.height = MAX (1, priv->child1_size);
          window1_allocation.x = window2_allocation.x = allocation->x;
          window1_allocation.y = allocation->y;

          window2_allocation.y = window1_allocation.y + priv->child1_size + priv->handle_pos.height;
          window2_allocation.height = MAX (1, allocation->y + allocation->height - window2_allocation.y);

          child1_allocation.x = child1_allocation.y = 0;
          child1_allocation.width = window1_allocation.width;
          child1_allocation.height = window1_allocation.height;
          if (child1_height > child1_allocation.height)
            {
              child1_allocation.y -= child1_height - child1_allocation.height;
              child1_allocation.height = child1_height;
            }

          child2_allocation.x = child2_allocation.y = 0;
          child2_allocation.width = window2_allocation.width;
          child2_allocation.height = window2_allocation.height;
          if (child2_height > child2_allocation.height)
            child2_allocation.height = child2_height;
        }

      if (gtk_widget_get_mapped (widget) &&
          (old_handle_pos.x != priv->handle_pos.x ||
           old_handle_pos.y != priv->handle_pos.y ||
           old_handle_pos.width != priv->handle_pos.width ||
           old_handle_pos.height != priv->handle_pos.height))
        {
          GdkWindow *window;

          window = gtk_widget_get_window (widget);
          gdk_window_invalidate_rect (window, &old_handle_pos, FALSE);
          gdk_window_invalidate_rect (window, &priv->handle_pos, FALSE);
        }

      if (gtk_widget_get_realized (widget))
	{
	  if (gtk_widget_get_mapped (widget))
	    gdk_window_show (priv->handle);

          if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              gdk_window_move_resize (priv->handle,
                                      priv->handle_pos.x,
                                      priv->handle_pos.y,
                                      handle_size,
                                      priv->handle_pos.height);
            }
          else
            {
              gdk_window_move_resize (priv->handle,
                                      priv->handle_pos.x,
                                      priv->handle_pos.y,
                                      priv->handle_pos.width,
                                      handle_size);
            }
	}

      /* Now allocate the childen, making sure, when resizing not to
       * overlap the windows
       */
      gtk_widget_get_allocation (priv->child1, &priv_child1_allocation);
      if (gtk_widget_get_mapped (widget) &&
          ((priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
            priv_child1_allocation.width < child1_allocation.width) ||

           (priv->orientation == GTK_ORIENTATION_VERTICAL &&
            priv_child1_allocation.height < child1_allocation.height)))
	{
          gtk_paned_child_allocate (priv->child2,
                                    priv->child2_window,
                                    &window2_allocation,
                                    &child2_allocation);
          gtk_paned_child_allocate (priv->child1,
                                    priv->child1_window,
                                    &window1_allocation,
                                    &child1_allocation);
	}
      else
	{
          gtk_paned_child_allocate (priv->child1,
                                    priv->child1_window,
                                    &window1_allocation,
                                    &child1_allocation);
          gtk_paned_child_allocate (priv->child2,
                                    priv->child2_window,
                                    &window2_allocation,
                                    &child2_allocation);
	}
    }
  else
    {
      GtkAllocation window_allocation, child_allocation;

      if (gtk_widget_get_realized (widget))
	gdk_window_hide (priv->handle);

      window_allocation.x = allocation->x;
      window_allocation.y = allocation->y;
      window_allocation.width = allocation->width;
      window_allocation.height = allocation->height;
      child_allocation.x = child_allocation.y = 0;
      child_allocation.width = allocation->width;
      child_allocation.height = allocation->height;

      if (priv->child1 && gtk_widget_get_visible (priv->child1))
        {
          gtk_paned_set_child_visible (paned, 0, TRUE);
          if (priv->child2)
            gtk_paned_set_child_visible (paned, 1, FALSE);

          gtk_paned_child_allocate (priv->child1,
                                    priv->child1_window,
                                    &window_allocation,
                                    &child_allocation);
        }
      else if (priv->child2 && gtk_widget_get_visible (priv->child2))
        {
          gtk_paned_set_child_visible (paned, 1, TRUE);
          if (priv->child1)
            gtk_paned_set_child_visible (paned, 0, FALSE);

          gtk_paned_child_allocate (priv->child2,
                                    priv->child2_window,
                                    &window_allocation,
                                    &child_allocation);
        }
      else
        {
          if (priv->child1)
            gtk_paned_set_child_visible (paned, 0, FALSE);
          if (priv->child2)
            gtk_paned_set_child_visible (paned, 1, FALSE);
        }
    }
}

static GdkWindow *
gtk_paned_create_child_window (GtkPaned  *paned,
                               GtkWidget *child) /* may be NULL */
{
  GtkWidget *widget = GTK_WIDGET (paned);
  GtkPanedPrivate *priv = paned->priv;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  if (child)
    {
      GtkAllocation allocation;
      int handle_size;

      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

      gtk_widget_get_allocation (widget, &allocation);
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
          child == priv->child2 && priv->child1 &&
          gtk_widget_get_visible (priv->child1))
        attributes.x = priv->handle_pos.x + handle_size;
      else
        attributes.x = allocation.x;
      if (priv->orientation == GTK_ORIENTATION_VERTICAL &&
          child == priv->child2 && priv->child1 &&
          gtk_widget_get_visible (priv->child1))
        attributes.y = priv->handle_pos.y + handle_size;
      else
        attributes.y = allocation.y;

      gtk_widget_get_allocation (child, &allocation);
      attributes.width = allocation.width;
      attributes.height = allocation.height;
      attributes_mask = GDK_WA_X | GDK_WA_Y| GDK_WA_VISUAL;
    }
  else
    {
      attributes.width = 1;
      attributes.height = 1;
      attributes_mask = GDK_WA_VISUAL;
    }

  window = gdk_window_new (gtk_widget_get_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_register_window (widget, window);
  gtk_style_context_set_background (gtk_widget_get_style_context (widget), window);

  if (child)
    gtk_widget_set_parent_window (child, window);

  return window;
}

static void
gtk_paned_realize (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = priv->handle_pos.x;
  attributes.y = priv->handle_pos.y;
  attributes.width = priv->handle_pos.width;
  attributes.height = priv->handle_pos.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_POINTER_MOTION_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;
  if (gtk_widget_is_sensitive (widget))
    {
      attributes.cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						      priv->cursor_type);
      attributes_mask |= GDK_WA_CURSOR;
    }

  priv->handle = gdk_window_new (window,
                                 &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->handle);
  if (attributes_mask & GDK_WA_CURSOR)
    g_object_unref (attributes.cursor);

  priv->child1_window = gtk_paned_create_child_window (paned, priv->child1);
  priv->child2_window = gtk_paned_create_child_window (paned, priv->child2);
}

static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (priv->child2)
    gtk_widget_set_parent_window (priv->child2, NULL);
  gtk_widget_unregister_window (widget, priv->child2_window);
  gdk_window_destroy (priv->child2_window);
  priv->child2_window = NULL;

  if (priv->child1)
    gtk_widget_set_parent_window (priv->child1, NULL);
  gtk_widget_unregister_window (widget, priv->child1_window);
  gdk_window_destroy (priv->child1_window);
  priv->child1_window = NULL;

  if (priv->handle)
    {
      gtk_widget_unregister_window (widget, priv->handle);
      gdk_window_destroy (priv->handle);
      priv->handle = NULL;
    }

  gtk_paned_set_last_child1_focus (paned, NULL);
  gtk_paned_set_last_child2_focus (paned, NULL);
  gtk_paned_set_saved_focus (paned, NULL);
  gtk_paned_set_first_paned (paned, NULL);

  GTK_WIDGET_CLASS (gtk_paned_parent_class)->unrealize (widget);
}

static void
gtk_paned_map (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    gdk_window_show (priv->handle);

  if (priv->child1 && gtk_widget_get_visible (priv->child1) && gtk_widget_get_child_visible (priv->child1))
    gdk_window_show (priv->child1_window);
  if (priv->child2 && gtk_widget_get_visible (priv->child2) && gtk_widget_get_child_visible (priv->child2))
    gdk_window_show (priv->child2_window);

  GTK_WIDGET_CLASS (gtk_paned_parent_class)->map (widget);
}

static void
gtk_paned_unmap (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  gdk_window_hide (priv->handle);
  
  if (gdk_window_is_visible (priv->child1_window))
    gdk_window_hide (priv->child1_window);
  if (gdk_window_is_visible (priv->child2_window))
    gdk_window_hide (priv->child2_window);

  GTK_WIDGET_CLASS (gtk_paned_parent_class)->unmap (widget);
}

static gboolean
gtk_paned_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (gtk_cairo_should_draw_window (cr, priv->child1_window))
    {
      cairo_save (cr);
      gtk_cairo_transform_to_window (cr, widget, priv->child1_window);
      gtk_render_background (gtk_widget_get_style_context (widget),
                             cr,
                             0, 0,
                             gdk_window_get_width (priv->child1_window),
                             gdk_window_get_height (priv->child1_window));
      cairo_restore (cr);
    }

  if (gtk_cairo_should_draw_window (cr, priv->child2_window))
    {
      cairo_save (cr);
      gtk_cairo_transform_to_window (cr, widget, priv->child2_window);
      gtk_render_background (gtk_widget_get_style_context (widget),
                             cr,
                             0, 0,
                             gdk_window_get_width (priv->child2_window),
                             gdk_window_get_height (priv->child2_window));
      cairo_restore (cr);
    }

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)) &&
      priv->child1 && gtk_widget_get_visible (priv->child1) &&
      priv->child2 && gtk_widget_get_visible (priv->child2))
    {
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);

      if (gtk_widget_is_focus (widget))
	state |= GTK_STATE_FLAG_SELECTED;
      if (priv->handle_prelit)
	state |= GTK_STATE_FLAG_PRELIGHT;

      gtk_style_context_save (context);
      gtk_style_context_set_state (context, state);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_PANE_SEPARATOR);
      gtk_render_handle (context, cr,
                         priv->handle_pos.x - allocation.x,
                         priv->handle_pos.y - allocation.y,
                         priv->handle_pos.width,
                         priv->handle_pos.height);

      gtk_style_context_restore (context);
    }

  /* Chain up to draw children */
  GTK_WIDGET_CLASS (gtk_paned_parent_class)->draw (widget, cr);
  
  return FALSE;
}

static gboolean
is_rtl (GtkPaned *paned)
{
  GtkPanedPrivate *priv = paned->priv;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (paned)) == GTK_TEXT_DIR_RTL)
    {
      return TRUE;
    }

  return FALSE;
}

static void
update_drag (GtkPaned         *paned,
             /* relative to priv->handle */
             int               xpos,
             int               ypos)
{
  GtkPanedPrivate *priv = paned->priv;
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (paned);
  gint pos;
  gint handle_size;
  gint size;
  gint x, y;

  gdk_window_get_position (priv->handle, &x, &y);
  gtk_widget_get_allocation (widget, &allocation);
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      pos = xpos + x - allocation.x;
    }
  else
    {
      pos = ypos + y - allocation.y;
    }

  pos -= priv->drag_pos;

  if (is_rtl (paned))
    {
      gtk_widget_style_get (widget,
			    "handle-size", &handle_size,
			    NULL);

      size = allocation.width - pos - handle_size;
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
gtk_paned_enter (GtkWidget        *widget,
		 GdkEventCrossing *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (priv->in_drag)
    update_drag (paned, event->x, event->y);
  else
    {
      priv->handle_prelit = TRUE;
      gtk_widget_queue_draw_area (widget,
				  priv->handle_pos.x,
				  priv->handle_pos.y,
				  priv->handle_pos.width,
				  priv->handle_pos.height);
    }
  
  return TRUE;
}

static gboolean
gtk_paned_leave (GtkWidget        *widget,
		 GdkEventCrossing *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (priv->in_drag)
    update_drag (paned, event->x, event->y);
  else
    {
      priv->handle_prelit = FALSE;
      gtk_widget_queue_draw_area (widget,
				  priv->handle_pos.x,
				  priv->handle_pos.y,
				  priv->handle_pos.width,
				  priv->handle_pos.height);
    }

  return TRUE;
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

static gboolean
gtk_paned_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (!priv->in_drag &&
      (event->window == priv->handle) && (event->button == GDK_BUTTON_PRIMARY))
    {
      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      if (gdk_device_grab (event->device,
                           priv->handle,
                           GDK_OWNERSHIP_WINDOW, FALSE,
                           GDK_BUTTON1_MOTION_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_ENTER_NOTIFY_MASK
                           | GDK_LEAVE_NOTIFY_MASK,
                           NULL, event->time) != GDK_GRAB_SUCCESS)
	return FALSE;

      priv->in_drag = TRUE;
      priv->grab_time = event->time;
      priv->grab_device = event->device;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	priv->drag_pos = event->x;
      else
	priv->drag_pos = event->y;

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_grab_broken (GtkWidget          *widget,
		       GdkEventGrabBroken *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  priv->in_drag = FALSE;
  priv->drag_pos = -1;
  priv->position_set = TRUE;

  return TRUE;
}

static void
stop_drag (GtkPaned *paned)
{
  GtkPanedPrivate *priv = paned->priv;

  priv->in_drag = FALSE;
  priv->drag_pos = -1;
  priv->position_set = TRUE;

  gdk_device_ungrab (priv->grab_device,
                     priv->grab_time);
  priv->grab_device = NULL;
}

static void
gtk_paned_grab_notify (GtkWidget *widget,
		       gboolean   was_grabbed)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;
  GdkDevice *grab_device;

  grab_device = priv->grab_device;

  if (priv->in_drag && grab_device &&
      gtk_widget_device_is_shadowed (widget, grab_device))
    stop_drag (paned);
}

static void
gtk_paned_state_flags_changed (GtkWidget     *widget,
                               GtkStateFlags  previous_state)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;
  GdkCursor *cursor;

  if (gtk_widget_get_realized (widget))
    {
      if (gtk_widget_is_sensitive (widget))
        cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
                                             priv->cursor_type);
      else
        cursor = NULL;

      gdk_window_set_cursor (priv->handle, cursor);

      if (cursor)
        g_object_unref (cursor);
    }
}

static gboolean
gtk_paned_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (priv->in_drag && (event->button == GDK_BUTTON_PRIMARY))
    {
      stop_drag (paned);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_motion (GtkWidget      *widget,
		  GdkEventMotion *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkPanedPrivate *priv = paned->priv;

  if (priv->in_drag)
    {
      update_drag (paned, event->x, event->y);
      return TRUE;
    }
  
  return FALSE;
}

/**
 * gtk_paned_new:
 * @orientation: the paned's orientation.
 *
 * Creates a new #GtkPaned widget.
 *
 * Return value: a new #GtkPaned.
 *
 * Since: 3.0
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
 * <literal>gtk_paned_pack1 (paned, child, FALSE, TRUE)</literal>.
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
 * <literal>gtk_paned_pack2 (paned, child, TRUE, TRUE)</literal>.
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
  GtkPanedPrivate *priv;

  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = paned->priv;

  if (!priv->child1)
    {
      priv->child1 = child;
      priv->child1_resize = resize;
      priv->child1_shrink = shrink;

      gtk_widget_set_parent_window (child, priv->child1_window);
      gtk_widget_set_parent (child, GTK_WIDGET (paned));
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
  GtkPanedPrivate *priv;

  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = paned->priv;

  if (!priv->child2)
    {
      priv->child2 = child;
      priv->child2_resize = resize;
      priv->child2_shrink = shrink;

      gtk_widget_set_parent_window (child, priv->child2_window);
      gtk_widget_set_parent (child, GTK_WIDGET (paned));
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
  priv = paned->priv;

  if (!priv->child1)
    gtk_paned_add1 (paned, widget);
  else if (!priv->child2)
    gtk_paned_add2 (paned, widget);
  else
    g_warning ("GtkPaned cannot have more than 2 children\n");
}

static void
gtk_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkPaned *paned = GTK_PANED (container);
  GtkPanedPrivate *priv = paned->priv;
  gboolean was_visible;

  was_visible = gtk_widget_get_visible (widget);

  if (priv->child1 == widget)
    {
      if (priv->child1_window && gdk_window_is_visible (priv->child1_window))
        gdk_window_hide (priv->child1_window);

      gtk_widget_unparent (widget);

      priv->child1 = NULL;

      if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
	gtk_widget_queue_resize_no_redraw (GTK_WIDGET (container));
    }
  else if (priv->child2 == widget)
    {
      if (priv->child2_window && gdk_window_is_visible (priv->child2_window))
        gdk_window_hide (priv->child2_window);

      gtk_widget_unparent (widget);

      priv->child2 = NULL;

      if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
	gtk_widget_queue_resize_no_redraw (GTK_WIDGET (container));
    }
}

static void
gtk_paned_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkPanedPrivate *priv;
  GtkPaned *paned;

  g_return_if_fail (callback != NULL);

  paned = GTK_PANED (container);
  priv = paned->priv;

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
 * Return value: position of the divider
 **/
gint
gtk_paned_get_position (GtkPaned  *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), 0);

  return paned->priv->child1_size;
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
  GtkPanedPrivate *priv;
  GObject *object;

  g_return_if_fail (GTK_IS_PANED (paned));

  priv = paned->priv;

  if (priv->child1_size == position)
    return;

  object = G_OBJECT (paned);
  
  if (position >= 0)
    {
      /* We don't clamp here - the assumption is that
       * if the total allocation changes at the same time
       * as the position, the position set is with reference
       * to the new total size. If only the position changes,
       * then clamping will occur in gtk_paned_calc_position()
       */

      priv->child1_size = position;
      priv->position_set = TRUE;
    }
  else
    {
      priv->position_set = FALSE;
    }

  g_object_freeze_notify (object);
  g_object_notify (object, "position");
  g_object_notify (object, "position-set");
  g_object_thaw_notify (object);

  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (paned));

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
 * Return value: (transfer none): first child, or %NULL if it is not set.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_paned_get_child1 (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return paned->priv->child1;
}

/**
 * gtk_paned_get_child2:
 * @paned: a #GtkPaned widget
 * 
 * Obtains the second child of the paned widget.
 * 
 * Return value: (transfer none): second child, or %NULL if it is not set.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_paned_get_child2 (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return paned->priv->child2;
}

static void
gtk_paned_calc_position (GtkPaned *paned,
                         gint      allocation,
                         gint      child1_req,
                         gint      child2_req)
{
  GtkPanedPrivate *priv = paned->priv;
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

  if (priv->child1)
    gtk_paned_set_child_visible (paned, 0, priv->child1_size != 0);
  
  if (priv->child2)
    gtk_paned_set_child_visible (paned, 1, priv->child1_size != allocation); 

  g_object_freeze_notify (G_OBJECT (paned));
  if (priv->child1_size != old_position)
    g_object_notify (G_OBJECT (paned), "position");
  if (priv->min_position != old_min_position)
    g_object_notify (G_OBJECT (paned), "min-position");
  if (priv->max_position != old_max_position)
    g_object_notify (G_OBJECT (paned), "max-position");
  g_object_thaw_notify (G_OBJECT (paned));

  priv->last_allocation = allocation;
}

static void
gtk_paned_set_saved_focus (GtkPaned *paned, GtkWidget *widget)
{
  GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;

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

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (paned));
  if (gtk_widget_is_toplevel (toplevel))
    return gtk_window_get_focus (GTK_WINDOW (toplevel));

  return NULL;
}

static void
gtk_paned_set_focus_child (GtkContainer *container,
			   GtkWidget    *focus_child)
{
  GtkPaned *paned;
  GtkPanedPrivate *priv;
  GtkWidget *container_focus_child;

  g_return_if_fail (GTK_IS_PANED (container));

  paned = GTK_PANED (container);
  priv = paned->priv;

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

          container_focus_child = gtk_container_get_focus_child (container);
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
  GtkPanedPrivate *priv = paned->priv;
  GtkContainer *container = GTK_CONTAINER (paned);
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
  focus_child = gtk_container_get_focus_child (container);
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
      GtkWidget *widget = list->data;

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
      GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;

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
	      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (paned));
	      
	      if (GTK_IS_WINDOW (toplevel))
		gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
	    }
	}
      
      gtk_paned_set_saved_focus (paned, NULL);
      gtk_paned_set_first_paned (paned, NULL);
    }
}

static gboolean
gtk_paned_accept_position (GtkPaned *paned)
{
  GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;

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
  GtkPanedPrivate *priv = paned->priv;
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
	  focus->priv->original_position = gtk_paned_get_position (focus);
	}
    }
  else
    {
      GtkContainer *container = GTK_CONTAINER (paned);
      GtkPaned *focus;
      GtkPaned *first;
      GtkPaned *prev, *next;
      GtkWidget *toplevel;
      GtkWidget *focus_child;

      gtk_paned_find_neighbours (paned, &next, &prev);
      focus_child = gtk_container_get_focus_child (container);

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

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (paned));

      if (GTK_IS_WINDOW (toplevel))
        gtk_paned_set_saved_focus (focus, gtk_window_get_focus (GTK_WINDOW (toplevel)));
      gtk_paned_set_first_paned (focus, first);
      focus->priv->original_position = gtk_paned_get_position (focus);

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
 * gtk_paned_get_handle_window:
 * @paned: a #GtkPaned
 *
 * Returns the #GdkWindow of the handle. This function is
 * useful when handling button or motion events because it
 * enables the callback to distinguish between the window
 * of the paned, a child and the handle.
 *
 * Return value: (transfer none): the paned's handle window.
 *
 * Since: 2.20
 **/
GdkWindow *
gtk_paned_get_handle_window (GtkPaned *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), NULL);

  return paned->priv->handle;
}
