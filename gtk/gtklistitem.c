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

#include "config.h"

#include <gdk/gdkkeysyms.h>

#undef GTK_DISABLE_DEPRECATED
#define __GTK_LIST_ITEM_C__

#include "gtkbindings.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtklist.h"
#include "gtkmarshalers.h"
#include "gtksignal.h"
#include "gtkintl.h"

#include "gtkalias.h"


enum
{
  TOGGLE_FOCUS_ROW,
  SELECT_ALL,
  UNSELECT_ALL,
  UNDO_SELECTION,
  START_SELECTION,
  END_SELECTION,
  TOGGLE_ADD_MODE,
  EXTEND_SELECTION,
  SCROLL_VERTICAL,
  SCROLL_HORIZONTAL,
  LAST_SIGNAL
};

static void gtk_list_item_class_init        (GtkListItemClass *klass);
static void gtk_list_item_init              (GtkListItem      *list_item);
static void gtk_list_item_realize           (GtkWidget        *widget);
static void gtk_list_item_size_request      (GtkWidget        *widget,
					     GtkRequisition   *requisition);
static void gtk_list_item_size_allocate     (GtkWidget        *widget,
					     GtkAllocation    *allocation);
static void gtk_list_item_style_set         (GtkWidget        *widget,
					     GtkStyle         *previous_style);
static gint gtk_list_item_button_press      (GtkWidget        *widget,
					     GdkEventButton   *event);
static gint gtk_list_item_expose            (GtkWidget        *widget,
					     GdkEventExpose   *event);
static void gtk_real_list_item_select       (GtkItem          *item);
static void gtk_real_list_item_deselect     (GtkItem          *item);
static void gtk_real_list_item_toggle       (GtkItem          *item);


static GtkItemClass *parent_class = NULL;
static guint list_item_signals[LAST_SIGNAL] = {0};


GtkType
gtk_list_item_get_type (void)
{
  static GtkType list_item_type = 0;

  if (!list_item_type)
    {
      static const GtkTypeInfo list_item_info =
      {
	"GtkListItem",
	sizeof (GtkListItem),
	sizeof (GtkListItemClass),
	(GtkClassInitFunc) gtk_list_item_class_init,
	(GtkObjectInitFunc) gtk_list_item_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      I_("GtkListItem");
      list_item_type = gtk_type_unique (gtk_item_get_type (), &list_item_info);
    }

  return list_item_type;
}

static void
gtk_list_item_class_init (GtkListItemClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkItemClass *item_class;
  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  item_class = (GtkItemClass*) class;

  parent_class = gtk_type_class (gtk_item_get_type ());

  widget_class->realize = gtk_list_item_realize;
  widget_class->size_request = gtk_list_item_size_request;
  widget_class->size_allocate = gtk_list_item_size_allocate;
  widget_class->style_set = gtk_list_item_style_set;
  widget_class->button_press_event = gtk_list_item_button_press;
  widget_class->expose_event = gtk_list_item_expose;

  item_class->select = gtk_real_list_item_select;
  item_class->deselect = gtk_real_list_item_deselect;
  item_class->toggle = gtk_real_list_item_toggle;

  class->toggle_focus_row = NULL;
  class->select_all = NULL;
  class->unselect_all = NULL;
  class->undo_selection = NULL;
  class->start_selection = NULL;
  class->end_selection = NULL;
  class->extend_selection = NULL;
  class->scroll_horizontal = NULL;
  class->scroll_vertical = NULL;
  class->toggle_add_mode = NULL;

  list_item_signals[TOGGLE_FOCUS_ROW] =
    gtk_signal_new (I_("toggle-focus-row"),
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListItemClass, toggle_focus_row),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);
  list_item_signals[SELECT_ALL] =
    gtk_signal_new (I_("select-all"),
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListItemClass, select_all),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);
  list_item_signals[UNSELECT_ALL] =
    gtk_signal_new (I_("unselect-all"),
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListItemClass, unselect_all),
                    _gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);
  list_item_signals[UNDO_SELECTION] =
    gtk_signal_new (I_("undo-selection"),
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkListItemClass, undo_selection),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  list_item_signals[START_SELECTION] =
    gtk_signal_new (I_("start-selection"),
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkListItemClass, start_selection),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  list_item_signals[END_SELECTION] =
    gtk_signal_new (I_("end-selection"),
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkListItemClass, end_selection),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  list_item_signals[TOGGLE_ADD_MODE] =
    gtk_signal_new (I_("toggle-add-mode"),
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkListItemClass, toggle_add_mode),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  list_item_signals[EXTEND_SELECTION] =
    gtk_signal_new (I_("extend-selection"),
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListItemClass, extend_selection),
                    _gtk_marshal_VOID__ENUM_FLOAT_BOOLEAN,
                    GTK_TYPE_NONE, 3,
		    GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT, GTK_TYPE_BOOL);
  list_item_signals[SCROLL_VERTICAL] =
    gtk_signal_new (I_("scroll-vertical"),
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListItemClass, scroll_vertical),
                    _gtk_marshal_VOID__ENUM_FLOAT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);
  list_item_signals[SCROLL_HORIZONTAL] =
    gtk_signal_new (I_("scroll-horizontal"),
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListItemClass, scroll_horizontal),
                    _gtk_marshal_VOID__ENUM_FLOAT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Up, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Down, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Up, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Down, 0,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, GDK_CONTROL_MASK,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Home, GDK_CONTROL_MASK,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_End, GDK_CONTROL_MASK,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_End, GDK_CONTROL_MASK,
				"scroll-vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Up, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Up, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Down, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Down, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Up, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Down, GDK_SHIFT_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Home,
				GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Home,
				GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_End,
				GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_End,
				GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"extend-selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0, GTK_TYPE_BOOL, TRUE);

  
  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Home, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_End, 0,
				"scroll-horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				"undo-selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
				"toggle-focus-row", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, 0,
				"toggle-focus-row", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK,
				"toggle-add-mode", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, GDK_CONTROL_MASK,
				"toggle-add-mode", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_slash, GDK_CONTROL_MASK,
				"select-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Divide, GDK_CONTROL_MASK,
				"select-all", 0);
  gtk_binding_entry_add_signal (binding_set, '\\', GDK_CONTROL_MASK,
				"unselect-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_L,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK,
				"end-selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_R,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK,
				"end-selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_R,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK |
				GDK_CONTROL_MASK,
				"end-selection", 0);
}

static void
gtk_list_item_init (GtkListItem *list_item)
{
  gtk_widget_set_can_focus (GTK_WIDGET (list_item), TRUE);
}

GtkWidget*
gtk_list_item_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_list_item_get_type ()));
}

GtkWidget*
gtk_list_item_new_with_label (const gchar *label)
{
  GtkWidget *list_item;
  GtkWidget *label_widget;

  list_item = gtk_list_item_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label_widget), 0, 1);
  
  gtk_container_add (GTK_CONTAINER (list_item), label_widget);
  gtk_widget_show (label_widget);

  return list_item;
}

void
gtk_list_item_select (GtkListItem *list_item)
{
  gtk_item_select (GTK_ITEM (list_item));
}

void
gtk_list_item_deselect (GtkListItem *list_item)
{
  gtk_item_deselect (GTK_ITEM (list_item));
}


static void
gtk_list_item_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  /*GTK_WIDGET_CLASS (parent_class)->realize (widget);*/

  g_return_if_fail (GTK_IS_LIST_ITEM (widget));

  gtk_widget_set_realized (widget, TRUE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_BUTTON1_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK |
			   GDK_KEY_PRESS_MASK |
			   GDK_KEY_RELEASE_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gdk_window_set_background (widget->window, 
			     &widget->style->base[GTK_STATE_NORMAL]);
}

static void
gtk_list_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkRequisition child_requisition;
  gint focus_width;
  gint focus_pad;

  g_return_if_fail (GTK_IS_LIST_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);
  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			NULL);

  requisition->width = 2 * (GTK_CONTAINER (widget)->border_width +
			    widget->style->xthickness + focus_width + focus_pad - 1);
  requisition->height = 2 * (GTK_CONTAINER (widget)->border_width +
			     focus_width + focus_pad - 1);

  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_list_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (GTK_IS_LIST_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  bin = GTK_BIN (widget);

  if (bin->child)
    {
      child_allocation.x = (GTK_CONTAINER (widget)->border_width +
			    widget->style->xthickness);
      child_allocation.y = GTK_CONTAINER (widget)->border_width;
      child_allocation.width = allocation->width - child_allocation.x * 2;
      child_allocation.height = allocation->height - child_allocation.y * 2;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void 
gtk_list_item_style_set	(GtkWidget      *widget,
			 GtkStyle       *previous_style)
{
  GtkStyle *style;

  g_return_if_fail (widget != NULL);

  if (previous_style && gtk_widget_get_realized (widget))
    {
      style = gtk_widget_get_style (widget);
      gdk_window_set_background (gtk_widget_get_window (widget),
                                 &style->base[gtk_widget_get_state (widget)]);
    }
}

static gint
gtk_list_item_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  if (event->type == GDK_BUTTON_PRESS && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  return FALSE;
}

static gint
gtk_list_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);

  if (gtk_widget_is_drawable (widget))
    {
      if (widget->state == GTK_STATE_NORMAL)
        {
          gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
          gdk_window_clear_area (widget->window, event->area.x, event->area.y,
                                 event->area.width, event->area.height);
        }
      else
        {
          gtk_paint_flat_box (widget->style, widget->window, 
                              widget->state, GTK_SHADOW_ETCHED_OUT,
                              &event->area, widget, "listitem",
                              0, 0, -1, -1);           
        }

      GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

      if (gtk_widget_has_focus (widget))
        {
          if (GTK_IS_LIST (widget->parent) && GTK_LIST (widget->parent)->add_mode)
            gtk_paint_focus (widget->style, widget->window, gtk_widget_get_state (widget),
                             NULL, widget, "add-mode",
                             0, 0, widget->allocation.width, widget->allocation.height);
          else
            gtk_paint_focus (widget->style, widget->window, gtk_widget_get_state (widget),
                             NULL, widget, NULL,
                             0, 0, widget->allocation.width, widget->allocation.height);
        }
    }

  return FALSE;
}

static void
gtk_real_list_item_select (GtkItem *item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  switch (GTK_WIDGET (item)->state)
    {
    case GTK_STATE_SELECTED:
    case GTK_STATE_INSENSITIVE:
      break;
    default:
      gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);
      break;
    }
}

static void
gtk_real_list_item_deselect (GtkItem *item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  if (GTK_WIDGET (item)->state == GTK_STATE_SELECTED)
    gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
}

static void
gtk_real_list_item_toggle (GtkItem *item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (item));
  
  switch (GTK_WIDGET (item)->state)
    {
    case GTK_STATE_SELECTED:
      gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
      break;
    case GTK_STATE_INSENSITIVE:
      break;
    default:
      gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);
      break;
    }
}

#include "gtkaliasdef.c"
