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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkintl.h"
#include "gtkpaned.h"
#include "gtkbindings.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"
#include "gtkwindow.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"

enum {
  PROP_0,
  PROP_POSITION,
  PROP_POSITION_SET
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

static void     gtk_paned_class_init            (GtkPanedClass    *klass);
static void     gtk_paned_init                  (GtkPaned         *paned);
static void     gtk_paned_set_property          (GObject          *object,
						 guint             prop_id,
						 const GValue     *value,
						 GParamSpec       *pspec);
static void     gtk_paned_get_property          (GObject          *object,
						 guint             prop_id,
						 GValue           *value,
						 GParamSpec       *pspec);
static void     gtk_paned_realize               (GtkWidget        *widget);
static void     gtk_paned_unrealize             (GtkWidget        *widget);
static void     gtk_paned_map                   (GtkWidget        *widget);
static void     gtk_paned_unmap                 (GtkWidget        *widget);
static gboolean gtk_paned_expose                (GtkWidget        *widget,
						 GdkEventExpose   *event);
static gboolean gtk_paned_enter                 (GtkWidget        *widget,
						 GdkEventCrossing *event);
static gboolean gtk_paned_leave                 (GtkWidget        *widget,
						 GdkEventCrossing *event);
static gboolean gtk_paned_button_press		(GtkWidget      *widget,
						 GdkEventButton *event);
static gboolean gtk_paned_button_release	(GtkWidget      *widget,
						 GdkEventButton *event);
static gboolean gtk_paned_motion		(GtkWidget      *widget,
						 GdkEventMotion *event);
static gboolean gtk_paned_focus                 (GtkWidget        *widget,
						 GtkDirectionType  direction);
static void     gtk_paned_add                   (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_paned_remove                (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_paned_forall                (GtkContainer     *container,
						 gboolean          include_internals,
						 GtkCallback       callback,
						 gpointer          callback_data);
static void     gtk_paned_set_focus_child       (GtkContainer     *container,
						 GtkWidget        *child);
static void     gtk_paned_set_saved_focus       (GtkPaned         *paned,
						 GtkWidget        *widget);
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
static GtkType  gtk_paned_child_type            (GtkContainer     *container);

static GtkContainerClass *parent_class = NULL;


GtkType
gtk_paned_get_type (void)
{
  static GtkType paned_type = 0;
  
  if (!paned_type)
    {
      static const GtkTypeInfo paned_info =
      {
	"GtkPaned",
	sizeof (GtkPaned),
	sizeof (GtkPanedClass),
	(GtkClassInitFunc) gtk_paned_class_init,
	(GtkObjectInitFunc) gtk_paned_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      paned_type = gtk_type_unique (GTK_TYPE_CONTAINER, &paned_info);
    }
  
  return paned_type;
}

static guint signals[LAST_SIGNAL] = { 0 };

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  gboolean	    reverse)
{
  gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
                                "cycle_handle_focus", 1,
                                G_TYPE_BOOLEAN, reverse);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
                                "cycle_handle_focus", 1,
                                G_TYPE_BOOLEAN, reverse);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Left_Tab, modifiers,
                                "cycle_handle_focus", 1,
                                G_TYPE_BOOLEAN, reverse);
}

static void
add_move_binding (GtkBindingSet   *binding_set,
		  guint            keyval,
		  GdkModifierType  mask,
		  GtkScrollType    scroll)
{
  gtk_binding_entry_add_signal (binding_set, keyval, mask,
				"move_handle", 1,
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

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  object_class->set_property = gtk_paned_set_property;
  object_class->get_property = gtk_paned_get_property;

  widget_class->realize = gtk_paned_realize;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->map = gtk_paned_map;
  widget_class->unmap = gtk_paned_unmap;
  widget_class->expose_event = gtk_paned_expose;
  widget_class->focus = gtk_paned_focus;
  widget_class->enter_notify_event = gtk_paned_enter;
  widget_class->leave_notify_event = gtk_paned_leave;
  widget_class->button_press_event = gtk_paned_button_press;
  widget_class->button_release_event = gtk_paned_button_release;
  widget_class->motion_notify_event = gtk_paned_motion;
  
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
  
  g_object_class_install_property (object_class,
				   PROP_POSITION,
				   g_param_spec_int ("position",
						     _("Position"),
						     _("Position of paned separator in pixels (0 means all the way to the left/top)"),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (object_class,
				   PROP_POSITION_SET,
				   g_param_spec_boolean ("position_set",
							 _("Position Set"),
							 _("TRUE if the Position property should be used"),
							 FALSE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));
				   
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("handle_size",
							     _("Handle Size"),
							     _("Width of handle"),
							     0,
							     G_MAXINT,
							     5,
							     G_PARAM_READABLE));

  signals [CYCLE_HANDLE_FOCUS] =
    g_signal_new ("cycle_child_focus",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, cycle_child_focus),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  signals [TOGGLE_HANDLE_FOCUS] =
    g_signal_new ("toggle_handle_focus",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, toggle_handle_focus),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  signals[MOVE_HANDLE] =
    g_signal_new ("move_handle",
		  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPanedClass, move_handle),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_SCROLL_TYPE);

  signals [CYCLE_HANDLE_FOCUS] =
    g_signal_new ("cycle_handle_focus",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, cycle_handle_focus),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  signals [ACCEPT_POSITION] =
    g_signal_new ("accept_position",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, accept_position),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  signals [CANCEL_POSITION] =
    g_signal_new ("cancel_position",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkPanedClass, cancel_position),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  binding_set = gtk_binding_set_by_class (object_class);

  /* F6 and friends */
  gtk_binding_entry_add_signal (binding_set,				
                                GDK_F6, 0,
                                "cycle_child_focus", 1, 
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_F6, GDK_SHIFT_MASK,
				"cycle_child_focus", 1,
				G_TYPE_BOOLEAN, TRUE);

  /* F8 and friends */
  gtk_binding_entry_add_signal (binding_set,
				GDK_F8, 0,
				"toggle_handle_focus", 0);
 
  add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  /* accept and cancel positions */
  gtk_binding_entry_add_signal (binding_set,
				GDK_Escape, 0,
				"cancel_position", 0);

  gtk_binding_entry_add_signal (binding_set,
				GDK_Return, 0,
				"accept_position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Enter, 0,
				"accept_position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_space, 0,
				"accept_position", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Space, 0,
				"accept_position", 0);

  /* move handle */
  add_move_binding (binding_set, GDK_Left, 0, GTK_SCROLL_STEP_LEFT);
  add_move_binding (binding_set, GDK_KP_Left, 0, GTK_SCROLL_STEP_LEFT);
  add_move_binding (binding_set, GDK_Left, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_LEFT);
  add_move_binding (binding_set, GDK_KP_Left, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_LEFT);

  add_move_binding (binding_set, GDK_Right, 0, GTK_SCROLL_STEP_RIGHT);
  add_move_binding (binding_set, GDK_Right, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_RIGHT);
  add_move_binding (binding_set, GDK_KP_Right, 0, GTK_SCROLL_STEP_RIGHT);
  add_move_binding (binding_set, GDK_KP_Right, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_RIGHT);

  add_move_binding (binding_set, GDK_Up, 0, GTK_SCROLL_STEP_UP);
  add_move_binding (binding_set, GDK_Up, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_UP);
  add_move_binding (binding_set, GDK_KP_Up, 0, GTK_SCROLL_STEP_UP);
  add_move_binding (binding_set, GDK_KP_Up, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_UP);
  add_move_binding (binding_set, GDK_Page_Up, 0, GTK_SCROLL_PAGE_UP);
  add_move_binding (binding_set, GDK_KP_Page_Up, 0, GTK_SCROLL_PAGE_UP);

  add_move_binding (binding_set, GDK_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_move_binding (binding_set, GDK_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_DOWN);
  add_move_binding (binding_set, GDK_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_move_binding (binding_set, GDK_KP_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_DOWN);
  add_move_binding (binding_set, GDK_Page_Down, 0, GTK_SCROLL_PAGE_RIGHT);
  add_move_binding (binding_set, GDK_KP_Page_Down, 0, GTK_SCROLL_PAGE_RIGHT);

  add_move_binding (binding_set, GDK_Home, 0, GTK_SCROLL_START);
  add_move_binding (binding_set, GDK_KP_Home, 0, GTK_SCROLL_START);
  add_move_binding (binding_set, GDK_End, 0, GTK_SCROLL_END);
  add_move_binding (binding_set, GDK_KP_End, 0, GTK_SCROLL_END);
}

static GtkType
gtk_paned_child_type (GtkContainer *container)
{
  if (!GTK_PANED (container)->child1 || !GTK_PANED (container)->child2)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
gtk_paned_init (GtkPaned *paned)
{
  GTK_WIDGET_SET_FLAGS (paned, GTK_NO_WINDOW | GTK_CAN_FOCUS);
  
  paned->child1 = NULL;
  paned->child2 = NULL;
  paned->handle = NULL;
  paned->xor_gc = NULL;
  paned->cursor_type = GDK_CROSS;
  
  paned->handle_pos.width = 5;
  paned->handle_pos.height = 5;
  paned->position_set = FALSE;
  paned->last_allocation = -1;
  paned->in_drag = FALSE;

  paned->saved_focus = NULL;
  paned->last_child1_focus = NULL;
  paned->last_child2_focus = NULL;
  paned->in_recursion = FALSE;
  paned->handle_prelit = FALSE;
  paned->original_position = -1;
  
  paned->handle_pos.x = -1;
  paned->handle_pos.y = -1;

  paned->drag_pos = -1;
}

static void
gtk_paned_set_property (GObject        *object,
			guint           prop_id,
			const GValue   *value,
			GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);
  
  switch (prop_id)
    {
    case PROP_POSITION:
      gtk_paned_set_position (paned, g_value_get_int (value));
      break;
    case PROP_POSITION_SET:
      paned->position_set = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (paned));
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
  
  switch (prop_id)
    {
    case PROP_POSITION:
      g_value_set_int (value, paned->child1_size);
      break;
    case PROP_POSITION_SET:
      g_value_set_boolean (value, paned->position_set);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_realize (GtkWidget *widget)
{
  GtkPaned *paned;
  GdkWindowAttr attributes;
  gint attributes_mask;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  paned = GTK_PANED (widget);

  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = paned->handle_pos.x;
  attributes.y = paned->handle_pos.y;
  attributes.width = paned->handle_pos.width;
  attributes.height = paned->handle_pos.height;
  attributes.cursor = gdk_cursor_new (paned->cursor_type);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_CURSOR;

  paned->handle = gdk_window_new (widget->window,
				  &attributes, attributes_mask);
  gdk_window_set_user_data (paned->handle, paned);
  gdk_cursor_destroy (attributes.cursor);

  widget->style = gtk_style_attach (widget->style, widget->window);

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    gdk_window_show (paned->handle);
}

static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);

  if (paned->xor_gc)
    {
      gdk_gc_destroy (paned->xor_gc);
      paned->xor_gc = NULL;
    }

  if (paned->handle)
    {
      gdk_window_set_user_data (paned->handle, NULL);
      gdk_window_destroy (paned->handle);
      paned->handle = NULL;
    }

  gtk_paned_set_last_child1_focus (paned, NULL);
  gtk_paned_set_last_child2_focus (paned, NULL);
  gtk_paned_set_saved_focus (paned, NULL);
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_paned_map (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);

  gdk_window_show (paned->handle);

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
gtk_paned_unmap (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
    
  gdk_window_hide (paned->handle);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static gboolean
gtk_paned_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkPaned *paned = GTK_PANED (widget);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      GdkRegion *region;

      region = gdk_region_rectangle (&paned->handle_pos);
      gdk_region_intersect (region, event->region);

      if (!gdk_region_empty (region))
	{
	  GtkStateType state;
	  GdkRectangle clip;

	  gdk_region_get_clipbox (region, &clip);

	  if (gtk_widget_is_focus (widget))
	    state = GTK_STATE_SELECTED;
	  else if (paned->handle_prelit)
	    state = GTK_STATE_PRELIGHT;
	  else
	    state = GTK_WIDGET_STATE (widget);
	  
	  gtk_paint_handle (widget->style, widget->window,
			    state, GTK_SHADOW_NONE,
			    &clip, widget, "paned",
			    paned->handle_pos.x, paned->handle_pos.y,
			    paned->handle_pos.width, paned->handle_pos.height,
			    paned->orientation);
	}

      gdk_region_destroy (region);
    }

  /* Chain up to draw children */
  GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
  
  return FALSE;
}

static void
update_drag (GtkPaned *paned)
{
  gint pos;
  gint handle_size;
  gint size;
  
  if (paned->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_get_pointer (GTK_WIDGET (paned), NULL, &pos);
  else
    gtk_widget_get_pointer (GTK_WIDGET (paned), &pos, NULL);

  gtk_widget_style_get (GTK_WIDGET (paned), "handle_size", &handle_size, NULL);
  
  size = pos - GTK_CONTAINER (paned)->border_width - paned->drag_pos;
  size = CLAMP (size, paned->min_position, paned->max_position);

  if (size != paned->child1_size)
    gtk_paned_set_position (paned, size);
}

static gboolean
gtk_paned_enter (GtkWidget        *widget,
		 GdkEventCrossing *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  
  if (paned->in_drag)
    update_drag (paned);
  else
    {
      paned->handle_prelit = TRUE;
      gtk_widget_queue_draw_area (widget,
				  paned->handle_pos.x,
				  paned->handle_pos.y,
				  paned->handle_pos.width,
				  paned->handle_pos.height);
    }
  
  return TRUE;
}

static gboolean
gtk_paned_leave (GtkWidget        *widget,
		 GdkEventCrossing *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  
  if (paned->in_drag)
    update_drag (paned);
  else
    {
      paned->handle_prelit = FALSE;
      gtk_widget_queue_draw_area (widget,
				  paned->handle_pos.x,
				  paned->handle_pos.y,
				  paned->handle_pos.width,
				  paned->handle_pos.height);
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

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
  retval = (* GTK_WIDGET_CLASS (parent_class)->focus) (widget, direction);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);

  return retval;
}

static gboolean
gtk_paned_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);

  if (!paned->in_drag &&
      (event->window == paned->handle) && (event->button == 1))
    {
      paned->in_drag = TRUE;

      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      gdk_pointer_grab (paned->handle, FALSE,
			GDK_POINTER_MOTION_HINT_MASK
			| GDK_BUTTON1_MOTION_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_ENTER_NOTIFY_MASK
			| GDK_LEAVE_NOTIFY_MASK,
			NULL, NULL,
			event->time);

      if (paned->orientation == GTK_ORIENTATION_HORIZONTAL)
	paned->drag_pos = event->y;
      else
	paned->drag_pos = event->x;
      
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);

  if (paned->in_drag && (event->button == 1))
    {
      paned->in_drag = FALSE;
      paned->drag_pos = -1;
      paned->position_set = TRUE;
      gdk_pointer_ungrab (event->time);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_motion (GtkWidget      *widget,
		  GdkEventMotion *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  
  if (paned->in_drag)
    {
      update_drag (paned);
      return TRUE;
    }
  
  return FALSE;
}

void
gtk_paned_add1 (GtkPaned  *paned,
		GtkWidget *widget)
{
  gtk_paned_pack1 (paned, widget, FALSE, TRUE);
}

void
gtk_paned_add2 (GtkPaned  *paned,
		GtkWidget *widget)
{
  gtk_paned_pack2 (paned, widget, TRUE, TRUE);
}

void
gtk_paned_pack1 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!paned->child1)
    {
      paned->child1 = child;
      paned->child1_resize = resize;
      paned->child1_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));
    }
}

void
gtk_paned_pack2 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!paned->child2)
    {
      paned->child2 = child;
      paned->child2_resize = resize;
      paned->child2_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));
    }
}


static void
gtk_paned_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  GtkPaned *paned;

  g_return_if_fail (GTK_IS_PANED (container));

  paned = GTK_PANED (container);

  if (!paned->child1)
    gtk_paned_add1 (paned, widget);
  else if (!paned->child2)
    gtk_paned_add2 (paned, widget);
}

static void
gtk_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkPaned *paned;
  gboolean was_visible;

  paned = GTK_PANED (container);
  was_visible = GTK_WIDGET_VISIBLE (widget);

  if (paned->child1 == widget)
    {
      gtk_widget_unparent (widget);

      paned->child1 = NULL;

      if (was_visible && GTK_WIDGET_VISIBLE (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
  else if (paned->child2 == widget)
    {
      gtk_widget_unparent (widget);

      paned->child2 = NULL;

      if (was_visible && GTK_WIDGET_VISIBLE (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_paned_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkPaned *paned;

  g_return_if_fail (callback != NULL);

  paned = GTK_PANED (container);

  if (paned->child1)
    (*callback) (paned->child1, callback_data);
  if (paned->child2)
    (*callback) (paned->child2, callback_data);
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

  return paned->child1_size;
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
  GObject *object;
  
  g_return_if_fail (GTK_IS_PANED (paned));

  object = G_OBJECT (paned);
  
  if (position >= 0)
    {
      /* We don't clamp here - the assumption is that
       * if the total allocation changes at the same time
       * as the position, the position set is with reference
       * to the new total size. If only the position changes,
       * then clamping will occur in gtk_paned_compute_position()
       */

      paned->child1_size = position;
      paned->position_set = TRUE;
    }
  else
    {
      paned->position_set = FALSE;
    }

  g_object_freeze_notify (object);
  g_object_notify (object, "position");
  g_object_notify (object, "position_set");
  g_object_thaw_notify (object);

  gtk_widget_queue_resize (GTK_WIDGET (paned));
}

void
gtk_paned_compute_position (GtkPaned *paned,
			    gint      allocation,
			    gint      child1_req,
			    gint      child2_req)
{
  gint old_position;
  
  g_return_if_fail (GTK_IS_PANED (paned));

  old_position = paned->child1_size;

  paned->min_position = paned->child1_shrink ? 0 : child1_req;

  paned->max_position = allocation;
  if (!paned->child2_shrink)
    paned->max_position = MAX (1, paned->max_position - child2_req);

  if (!paned->position_set)
    {
      if (paned->child1_resize && !paned->child2_resize)
	paned->child1_size = MAX (1, allocation - child2_req);
      else if (!paned->child1_resize && paned->child2_resize)
	paned->child1_size = child1_req;
      else if (child1_req + child2_req != 0)
	paned->child1_size = allocation * ((gdouble)child1_req / (child1_req + child2_req));
      else
	paned->child1_size = allocation * 0.5;
    }
  else
    {
      /* If the position was set before the initial allocation.
       * (paned->last_allocation <= 0) just clamp it and leave it.
       */
      if (paned->last_allocation > 0)
	{
	  if (paned->child1_resize && !paned->child2_resize)
	    paned->child1_size += allocation - paned->last_allocation;
	  else if (!(!paned->child1_resize && paned->child2_resize))
	    paned->child1_size = allocation * ((gdouble) paned->child1_size / (paned->last_allocation));
	}
    }

  paned->child1_size = CLAMP (paned->child1_size,
			      paned->min_position,
			      paned->max_position);

  if (paned->child1_size != old_position)
    g_object_notify (G_OBJECT (paned), "position");

  paned->last_allocation = allocation;
}

static void
gtk_paned_set_saved_focus (GtkPaned *paned, GtkWidget *widget)
{
  if (paned->saved_focus)
    g_object_remove_weak_pointer (G_OBJECT (paned->saved_focus),
				  (gpointer *)&(paned->saved_focus));

  paned->saved_focus = widget;

  if (paned->saved_focus)
    g_object_add_weak_pointer (G_OBJECT (paned->saved_focus),
			       (gpointer *)&(paned->saved_focus));
}

static void
gtk_paned_set_last_child1_focus (GtkPaned *paned, GtkWidget *widget)
{
  if (paned->last_child1_focus)
    g_object_remove_weak_pointer (G_OBJECT (paned->last_child1_focus),
				  (gpointer *)&(paned->last_child1_focus));

  paned->last_child1_focus = widget;

  if (paned->last_child1_focus)
    g_object_add_weak_pointer (G_OBJECT (paned->last_child1_focus),
			       (gpointer *)&(paned->last_child1_focus));
}

static void
gtk_paned_set_last_child2_focus (GtkPaned *paned, GtkWidget *widget)
{
  if (paned->last_child2_focus)
    g_object_remove_weak_pointer (G_OBJECT (paned->last_child2_focus),
				  (gpointer *)&(paned->last_child2_focus));

  paned->last_child2_focus = widget;

  if (paned->last_child2_focus)
    g_object_add_weak_pointer (G_OBJECT (paned->last_child2_focus),
			       (gpointer *)&(paned->last_child2_focus));
}

static GtkWidget *
paned_get_focus_widget (GtkPaned *paned)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (paned));
  if (GTK_WIDGET_TOPLEVEL (toplevel))
    return GTK_WINDOW (toplevel)->focus_widget;

  return NULL;
}

static void
gtk_paned_set_focus_child (GtkContainer *container,
			   GtkWidget    *focus_child)
{
  GtkPaned *paned;
  
  g_return_if_fail (GTK_IS_PANED (container));

  paned = GTK_PANED (container);
 
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
	  for (w = last_focus; w != GTK_WIDGET (paned); w = w->parent)
	    if (GTK_IS_PANED (w))
	      last_focus = w;
	  
	  if (container->focus_child == paned->child1)
	    gtk_paned_set_last_child1_focus (paned, last_focus);
	  else if (container->focus_child == paned->child2)
	    gtk_paned_set_last_child2_focus (paned, last_focus);
	}
    }

  if (parent_class->set_focus_child)
    (* parent_class->set_focus_child) (container, focus_child);
}

static void
gtk_paned_get_cycle_chain (GtkPaned          *paned,
			   GtkDirectionType   direction,
			   GList            **widgets)
{
  GtkContainer *container = GTK_CONTAINER (paned);
  GtkWidget *ancestor = NULL;
  GList *temp_list = NULL;
  GList *list;

  if (paned->in_recursion)
    return;

  g_assert (widgets != NULL);

  if (paned->last_child1_focus &&
      !gtk_widget_is_ancestor (paned->last_child1_focus, GTK_WIDGET (paned)))
    {
      gtk_paned_set_last_child1_focus (paned, NULL);
    }

  if (paned->last_child2_focus &&
      !gtk_widget_is_ancestor (paned->last_child2_focus, GTK_WIDGET (paned)))
    {
      gtk_paned_set_last_child2_focus (paned, NULL);
    }

  if (GTK_WIDGET (paned)->parent)
    ancestor = gtk_widget_get_ancestor (GTK_WIDGET (paned)->parent, GTK_TYPE_PANED);

  /* The idea here is that temp_list is a list of widgets we want to cycle
   * to. The list is prioritized so that the first element is our first
   * choice, the next our second, and so on.
   *
   * We can't just use g_list_reverse(), because we want to try
   * paned->last_child?_focus before paned->child?, both when we
   * are going forward and backward.
   */
  if (direction == GTK_DIR_TAB_FORWARD)
    {
      if (container->focus_child == paned->child1)
	{
	  temp_list = g_list_append (temp_list, paned->last_child2_focus);
	  temp_list = g_list_append (temp_list, paned->child2);
	  temp_list = g_list_append (temp_list, ancestor);
	}
      else if (container->focus_child == paned->child2)
	{
	  temp_list = g_list_append (temp_list, ancestor);
	  temp_list = g_list_append (temp_list, paned->last_child1_focus);
	  temp_list = g_list_append (temp_list, paned->child1);
	}
      else
	{
	  temp_list = g_list_append (temp_list, paned->last_child1_focus);
	  temp_list = g_list_append (temp_list, paned->child1);
	  temp_list = g_list_append (temp_list, paned->last_child2_focus);
	  temp_list = g_list_append (temp_list, paned->child2);
	  temp_list = g_list_append (temp_list, ancestor);
	}
    }
  else
    {
      if (container->focus_child == paned->child1)
	{
	  temp_list = g_list_append (temp_list, ancestor);
	  temp_list = g_list_append (temp_list, paned->last_child2_focus);
	  temp_list = g_list_append (temp_list, paned->child2);
	}
      else if (container->focus_child == paned->child2)
	{
	  temp_list = g_list_append (temp_list, paned->last_child1_focus);
	  temp_list = g_list_append (temp_list, paned->child1);
	  temp_list = g_list_append (temp_list, ancestor);
	}
      else
	{
	  temp_list = g_list_append (temp_list, paned->last_child2_focus);
	  temp_list = g_list_append (temp_list, paned->child2);
	  temp_list = g_list_append (temp_list, paned->last_child1_focus);
	  temp_list = g_list_append (temp_list, paned->child1);
	  temp_list = g_list_append (temp_list, ancestor);
	}
    }

  /* Walk through the list and expand all the paned widgets. */
  for (list = temp_list; list != NULL; list = list->next)
    {
      GtkWidget *widget = list->data;

      if (widget)
	{
	  if (GTK_IS_PANED (widget))
	    {
	      paned->in_recursion = TRUE;
	      gtk_paned_get_cycle_chain (GTK_PANED (widget), direction, widgets);
	      paned->in_recursion = FALSE;
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
  if (GTK_IS_PANED (widget))
    {
      GtkPaned *paned = GTK_PANED (widget);
      
      get_child_panes (paned->child1, panes);
      *panes = g_list_prepend (*panes, widget);
      get_child_panes (paned->child2, panes);
    }
  else if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_foreach (GTK_CONTAINER (widget),
			     (GtkCallback)get_child_panes, panes);
    }
}

static GList *
get_all_panes (GtkPaned *paned)
{
  GtkPaned *topmost = NULL;
  GList *result = NULL;
  GtkWidget *w;
  
  for (w = GTK_WIDGET (paned); w != NULL; w = w->parent)
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
  GList *all_panes = get_all_panes (paned);
  GList *this_link;

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

  if (*next == paned)
    *next = NULL;

  if (*prev == paned)
    *prev = NULL;

  g_list_free (all_panes);
}

static gboolean
gtk_paned_move_handle (GtkPaned      *paned,
		       GtkScrollType  scroll)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      gint old_position;
      gint new_position;
      
      enum {
	SINGLE_STEP_SIZE = 1,
	PAGE_STEP_SIZE   = 75,
      };
      
      old_position = gtk_paned_get_position (paned);
      
      switch (scroll)
	{
	case GTK_SCROLL_STEP_LEFT:
	case GTK_SCROLL_STEP_UP:
	case GTK_SCROLL_STEP_BACKWARD:
	  new_position = old_position - SINGLE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_STEP_RIGHT:
	case GTK_SCROLL_STEP_DOWN:
	case GTK_SCROLL_STEP_FORWARD:
	  new_position = old_position + SINGLE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_PAGE_LEFT:
	case GTK_SCROLL_PAGE_UP:
	case GTK_SCROLL_PAGE_BACKWARD:
	  new_position = old_position - PAGE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_PAGE_RIGHT:
	case GTK_SCROLL_PAGE_DOWN:
	case GTK_SCROLL_PAGE_FORWARD:
	  new_position = old_position + PAGE_STEP_SIZE;
	  break;
	  
	case GTK_SCROLL_START:
	  new_position = paned->min_position;
	  break;
	  
	case GTK_SCROLL_END:
	  new_position = paned->max_position;
	  break;
	  
	default:
	  new_position = old_position;
	  break;
	}
      
      new_position = CLAMP (new_position, paned->min_position, paned->max_position);
      
      if (old_position != new_position)
	gtk_paned_set_position (paned, new_position);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_paned_restore_focus (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      if (paned->saved_focus && GTK_WIDGET_SENSITIVE (paned->saved_focus))
	{
	  gtk_widget_grab_focus (paned->saved_focus);
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
    }
}

static gboolean
gtk_paned_accept_position (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      paned->original_position = -1;
      gtk_paned_restore_focus (paned);

      return TRUE;
    }

  return FALSE;
}


static gboolean
gtk_paned_cancel_position (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      if (paned->original_position != -1)
	{
	  gtk_paned_set_position (paned, paned->original_position);
	  paned->original_position = -1;
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
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      GtkPaned *next, *prev;
      GtkPaned *focus = NULL;

      gtk_paned_find_neighbours (paned, &next, &prev);

      if (reversed && prev)
	focus = prev;
      else if (!reversed && next)
	focus = next;

      if (focus)
	{
	  gtk_paned_set_saved_focus (focus, paned->saved_focus);
	  gtk_paned_set_saved_focus (paned, NULL);
	  gtk_widget_grab_focus (GTK_WIDGET (focus));

	  if (!gtk_widget_is_focus (GTK_WIDGET (paned)))
	    {
	      paned->original_position = -1;
	      focus->original_position = gtk_paned_get_position (focus);
	    }
	}
      
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_paned_toggle_handle_focus (GtkPaned *paned)
{
  if (gtk_widget_is_focus (GTK_WIDGET (paned)))
    {
      gtk_paned_accept_position (paned);
    }
  else
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (paned));

      if (GTK_IS_WINDOW (toplevel))
	gtk_paned_set_saved_focus (paned, GTK_WINDOW (toplevel)->focus_widget);
  
      gtk_widget_grab_focus (GTK_WIDGET (paned));
      paned->original_position = gtk_paned_get_position (paned);
    }

  return TRUE;
}
