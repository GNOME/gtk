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

#define GTK_MENU_INTERNALS

#include <string.h>
#include "gtkaccellabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtksignal.h"


#define BORDER_SPACING  3
#define SELECT_TIMEOUT  75

#define MENU_ITEM_CLASS(w)  GTK_MENU_ITEM_CLASS (GTK_OBJECT (w)->klass)


enum {
  ACTIVATE,
  ACTIVATE_ITEM,
  TOGGLE_SIZE_REQUEST,
  TOGGLE_SIZE_ALLOCATE,
  LAST_SIGNAL
};


static void gtk_menu_item_class_init     (GtkMenuItemClass *klass);
static void gtk_menu_item_init           (GtkMenuItem      *menu_item);
static void gtk_menu_item_destroy        (GtkObject        *object);
static void gtk_menu_item_finalize       (GObject          *object);
static void gtk_menu_item_size_request   (GtkWidget        *widget,
					  GtkRequisition   *requisition);
static void gtk_menu_item_size_allocate  (GtkWidget        *widget,
					  GtkAllocation    *allocation);
static void gtk_menu_item_realize        (GtkWidget        *widget);
static void gtk_menu_item_unrealize      (GtkWidget        *widget);
static void gtk_menu_item_map            (GtkWidget        *widget);
static void gtk_menu_item_unmap          (GtkWidget        *widget);
static void gtk_menu_item_paint          (GtkWidget        *widget,
					  GdkRectangle     *area);
static gint gtk_menu_item_expose         (GtkWidget        *widget,
					  GdkEventExpose   *event);
static void gtk_menu_item_parent_set     (GtkWidget        *widget,
					  GtkWidget        *previous_parent);


static void gtk_real_menu_item_select               (GtkItem     *item);
static void gtk_real_menu_item_deselect             (GtkItem     *item);
static void gtk_real_menu_item_activate_item        (GtkMenuItem *item);
static void gtk_real_menu_item_toggle_size_request  (GtkMenuItem *menu_item,
						     gint        *requisition);
static void gtk_real_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
						     gint         allocation);
static gboolean gtk_menu_item_mnemonic_activate     (GtkWidget   *widget,
						     gboolean     group_cycling);

static gint gtk_menu_item_select_timeout (gpointer          data);
static void gtk_menu_item_popup_submenu  (gpointer     data);
static void gtk_menu_item_position_menu  (GtkMenu          *menu,
					  gint             *x,
					  gint             *y,
					  gboolean         *push_in,
					  gpointer          user_data);
static void gtk_menu_item_show_all       (GtkWidget        *widget);
static void gtk_menu_item_hide_all       (GtkWidget        *widget);
static void gtk_menu_item_forall         (GtkContainer    *container,
					  gboolean         include_internals,
					  GtkCallback      callback,
					  gpointer         callback_data);


static GtkItemClass *parent_class;
static guint menu_item_signals[LAST_SIGNAL] = { 0 };
static guint32	last_submenu_deselect_time = 0;



GtkType
gtk_menu_item_get_type (void)
{
  static GtkType menu_item_type = 0;

  if (!menu_item_type)
    {
      static const GTypeInfo menu_item_info =
      {
	sizeof (GtkMenuItemClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_menu_item_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkMenuItem),
	16,             /* n_preallocs */
	(GInstanceInitFunc) gtk_menu_item_init,
      };

      menu_item_type = g_type_register_static (GTK_TYPE_ITEM, "GtkMenuItem", &menu_item_info, 0);
    }

  return menu_item_type;
}

static void
gtk_menu_item_class_init (GtkMenuItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkItemClass *item_class = GTK_ITEM_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gtk_menu_item_finalize;

  object_class->destroy = gtk_menu_item_destroy;

  widget_class->size_request = gtk_menu_item_size_request;
  widget_class->size_allocate = gtk_menu_item_size_allocate;
  widget_class->expose_event = gtk_menu_item_expose;
  widget_class->realize = gtk_menu_item_realize;
  widget_class->unrealize = gtk_menu_item_unrealize;
  widget_class->map = gtk_menu_item_map;
  widget_class->unmap = gtk_menu_item_unmap;
  widget_class->show_all = gtk_menu_item_show_all;
  widget_class->hide_all = gtk_menu_item_hide_all;
  widget_class->mnemonic_activate = gtk_menu_item_mnemonic_activate;
  widget_class->parent_set = gtk_menu_item_parent_set;
  
  container_class->forall = gtk_menu_item_forall;

  item_class->select = gtk_real_menu_item_select;
  item_class->deselect = gtk_real_menu_item_deselect;

  klass->activate = NULL;
  klass->activate_item = gtk_real_menu_item_activate_item;
  klass->toggle_size_request = gtk_real_menu_item_toggle_size_request;
  klass->toggle_size_allocate = gtk_real_menu_item_toggle_size_allocate;

  klass->hide_on_activate = TRUE;

  menu_item_signals[ACTIVATE] =
    gtk_signal_new ("activate",
                    GTK_RUN_FIRST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, activate),
                    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  widget_class->activate_signal = menu_item_signals[ACTIVATE];

  menu_item_signals[ACTIVATE_ITEM] =
    gtk_signal_new ("activate_item",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, activate_item),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  menu_item_signals[TOGGLE_SIZE_REQUEST] =
    gtk_signal_new ("toggle_size_request",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, toggle_size_request),
                    _gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);

  menu_item_signals[TOGGLE_SIZE_ALLOCATE] =
    gtk_signal_new ("toggle_size_allocate",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, toggle_size_allocate),
                    _gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);
}

static void
gtk_menu_item_init (GtkMenuItem *menu_item)
{
  GTK_WIDGET_SET_FLAGS (menu_item, GTK_NO_WINDOW);
  
  menu_item->submenu = NULL;
  menu_item->toggle_size = 0;
  menu_item->accelerator_width = 0;
  menu_item->show_submenu_indicator = FALSE;
  menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
  menu_item->submenu_placement = GTK_TOP_BOTTOM;
  menu_item->right_justify = FALSE;

  menu_item->timer = 0;
}

GtkWidget*
gtk_menu_item_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_menu_item_get_type ()));
}

GtkWidget*
gtk_menu_item_new_with_label (const gchar *label)
{
  GtkWidget *menu_item;
  GtkWidget *accel_label;

  menu_item = gtk_menu_item_new ();
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), menu_item);
  gtk_widget_show (accel_label);

  return menu_item;
}


/**
 * gtk_menu_item_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkMenuItem
 *
 * Creates a new #GtkMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 **/
GtkWidget*
gtk_menu_item_new_with_mnemonic (const gchar *label)
{
  GtkWidget *menu_item;
  GtkWidget *accel_label;

  menu_item = gtk_menu_item_new ();
  accel_label = gtk_type_new (GTK_TYPE_ACCEL_LABEL);
  gtk_label_set_text_with_mnemonic (GTK_LABEL (accel_label), label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), menu_item);
  gtk_widget_show (accel_label);

  return menu_item;
}

static void
gtk_menu_item_destroy (GtkObject *object)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (object));

  menu_item = GTK_MENU_ITEM (object);

  if (menu_item->submenu)
    gtk_widget_destroy (menu_item->submenu);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_menu_item_finalize (GObject *object)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);

  g_free (menu_item->accel_path);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_menu_item_detacher (GtkWidget     *widget,
			GtkMenu       *menu)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  menu_item = GTK_MENU_ITEM (widget);
  g_return_if_fail (menu_item->submenu == (GtkWidget*) menu);

  menu_item->submenu = NULL;
}

void
gtk_menu_item_set_submenu (GtkMenuItem *menu_item,
			   GtkWidget   *submenu)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  if (menu_item->submenu != submenu)
    {
      gtk_menu_item_remove_submenu (menu_item);
      
      menu_item->submenu = submenu;
      gtk_menu_attach_to_widget (GTK_MENU (submenu),
				 GTK_WIDGET (menu_item),
				 gtk_menu_item_detacher);
      
      if (GTK_WIDGET (menu_item)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (menu_item));
    }
}

/**
 * gtk_menu_item_get_submenu:
 * @menu_item: a #GtkMenuItem
 *
 * Gets the submenu underneath this menu item, if any. See
 * gtk_menu_item_set_submenu().
 *
 * Return value: submenu for this menu item, or %NULL if none.
 **/
GtkWidget *
gtk_menu_item_get_submenu (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return menu_item->submenu;
}

void
gtk_menu_item_remove_submenu (GtkMenuItem         *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
      
  if (menu_item->submenu)
    gtk_menu_detach (GTK_MENU (menu_item->submenu));
}

void _gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
				   GtkSubmenuPlacement  placement);

void
_gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
			     GtkSubmenuPlacement  placement)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->submenu_placement = placement;
}

void
gtk_menu_item_select (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  gtk_item_select (GTK_ITEM (menu_item));
}

void
gtk_menu_item_deselect (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  gtk_item_deselect (GTK_ITEM (menu_item));
}

void
gtk_menu_item_activate (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  gtk_signal_emit (GTK_OBJECT (menu_item), menu_item_signals[ACTIVATE]);
}

void
gtk_menu_item_toggle_size_request (GtkMenuItem *menu_item,
				   gint        *requisition)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_signal_emit (GTK_OBJECT (menu_item), menu_item_signals[TOGGLE_SIZE_REQUEST], requisition);
}

void
gtk_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
				    gint         allocation)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_signal_emit (GTK_OBJECT (menu_item), menu_item_signals[TOGGLE_SIZE_ALLOCATE], allocation);
}

static void
gtk_menu_item_accel_width_foreach (GtkWidget *widget,
				   gpointer data)
{
  guint *width = data;

  if (GTK_IS_ACCEL_LABEL (widget))
    {
      guint w;

      w = gtk_accel_label_get_accel_width (GTK_ACCEL_LABEL (widget));
      *width = MAX (*width, w);
    }
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_menu_item_accel_width_foreach,
			   data);
}

static void
gtk_menu_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkMenuItem *menu_item;
  GtkBin *bin;
  guint accel_width;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);
  menu_item = GTK_MENU_ITEM (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->xthickness +
			BORDER_SPACING) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->ythickness) * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }

  if (menu_item->submenu && menu_item->show_submenu_indicator)
    requisition->width += 21;

  accel_width = 0;
  gtk_container_foreach (GTK_CONTAINER (menu_item),
			 gtk_menu_item_accel_width_foreach,
			 &accel_width);
  menu_item->accelerator_width = accel_width;
}

static void
gtk_menu_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkMenuItem *menu_item;
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  menu_item = GTK_MENU_ITEM (widget);
  bin = GTK_BIN (widget);
  
  widget->allocation = *allocation;

  if (bin->child)
    {
      child_allocation.x = (GTK_CONTAINER (widget)->border_width +
                            widget->style->xthickness +
			    BORDER_SPACING);
      child_allocation.y = (GTK_CONTAINER (widget)->border_width +
			    widget->style->ythickness);
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);
      child_allocation.x += GTK_MENU_ITEM (widget)->toggle_size;
      child_allocation.width -= GTK_MENU_ITEM (widget)->toggle_size;
      child_allocation.x += widget->allocation.x;
      child_allocation.y += widget->allocation.y;
      
      if (menu_item->submenu && menu_item->show_submenu_indicator)
	child_allocation.width -= 21;
      
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (menu_item->event_window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  if (menu_item->submenu)
    gtk_menu_reposition (GTK_MENU (menu_item->submenu));
}

static void
gtk_menu_item_realize (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_POINTER_MOTION_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;
  menu_item->event_window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (menu_item->event_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_menu_item_unrealize (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);

  gdk_window_set_user_data (menu_item->event_window, NULL);
  gdk_window_destroy (menu_item->event_window);
  menu_item->event_window = NULL;
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_menu_item_map (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  
  GTK_WIDGET_CLASS (parent_class)->map (widget);

  gdk_window_show (menu_item->event_window);
}

static void
gtk_menu_item_unmap (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
    
  gdk_window_hide (menu_item->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
gtk_menu_item_paint (GtkWidget    *widget,
		     GdkRectangle *area)
{
  GtkMenuItem *menu_item;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint border_width = GTK_CONTAINER (widget)->border_width;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);

      state_type = widget->state;

      x = widget->allocation.x + border_width;
      y = widget->allocation.y + border_width;
      width = widget->allocation.width - border_width * 2;
      height = widget->allocation.height - border_width * 2;
      
      if ((state_type == GTK_STATE_PRELIGHT) &&
	  (GTK_BIN (menu_item)->child))
	gtk_paint_box (widget->style,
		       widget->window,
		       GTK_STATE_PRELIGHT,
		       GTK_SHADOW_OUT,
		       area, widget, "menuitem",
		       x, y, width, height);

      if (menu_item->submenu && menu_item->show_submenu_indicator)
	{
	  shadow_type = GTK_SHADOW_OUT;
	  if (state_type == GTK_STATE_PRELIGHT)
	    shadow_type = GTK_SHADOW_IN;

	  gtk_paint_arrow (widget->style, widget->window,
			   state_type, shadow_type, 
			   area, widget, "menuitem", 
			   GTK_ARROW_RIGHT, TRUE,
			   x + width - 15, y + height / 2 - 5, 10, 10);
	}
      else if (!GTK_BIN (menu_item)->child)
	{
	   gtk_paint_hline (widget->style, widget->window, GTK_STATE_NORMAL,
			    area, widget, "menuitem",
			    widget->allocation.x, widget->allocation.width,
			    widget->allocation.y);
	}
    }
}

static gint
gtk_menu_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_item_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static void
gtk_real_menu_item_select (GtkItem *item)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  menu_item = GTK_MENU_ITEM (item);

  /*  if (menu_item->submenu && !GTK_WIDGET_VISIBLE (menu_item->submenu))*/
  if (menu_item->submenu)
    {
      guint32 etime;
      GdkEvent *event = gtk_get_current_event ();

      etime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
      if (event &&
	  etime >= last_submenu_deselect_time &&
	  last_submenu_deselect_time + SELECT_TIMEOUT > etime)
	{
	  if (!menu_item->timer)
	    menu_item->timer = gtk_timeout_add (SELECT_TIMEOUT - (etime - last_submenu_deselect_time),
						gtk_menu_item_select_timeout,
						menu_item);
	  if (event &&
	      event->type != GDK_BUTTON_PRESS &&
	      event->type != GDK_ENTER_NOTIFY)
	    menu_item->timer_from_keypress = TRUE;
	  else
	    menu_item->timer_from_keypress = FALSE;
	}
      else
	gtk_menu_item_popup_submenu (menu_item);
      if (event)
	gdk_event_free(event);
    }
  
  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_PRELIGHT);
  gtk_widget_draw (GTK_WIDGET (menu_item), NULL);
}

static void
gtk_real_menu_item_deselect (GtkItem *item)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  menu_item = GTK_MENU_ITEM (item);

  if (menu_item->submenu)
    {
      guint32 etime;
      GdkEvent *event = gtk_get_current_event ();

      if (menu_item->timer)
	{
	  gtk_timeout_remove (menu_item->timer);
	  menu_item->timer = 0;
	}
      else
	gtk_menu_popdown (GTK_MENU (menu_item->submenu));

      etime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
      if (etime > last_submenu_deselect_time)
	last_submenu_deselect_time = etime;
      if (event)
	gdk_event_free(event);
    }

  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_NORMAL);
  gtk_widget_draw (GTK_WIDGET (menu_item), NULL);
}

static gboolean
gtk_menu_item_mnemonic_activate (GtkWidget *widget,
				 gboolean   group_cycling)
{
  if (group_cycling)
    {
      if (widget->parent &&
	  GTK_IS_MENU_SHELL (widget->parent))
	gtk_menu_shell_select_item (GTK_MENU_SHELL (widget->parent),
				    widget);

    }
  else
    gtk_signal_emit (GTK_OBJECT (widget), menu_item_signals[ACTIVATE_ITEM]);
  
  return TRUE;
}


static void
gtk_real_menu_item_activate_item (GtkMenuItem *menu_item)
{
  GtkWidget *widget;
  GtkMenuShell *submenu; 

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  widget = GTK_WIDGET (menu_item);
  
  if (widget->parent &&
      GTK_IS_MENU_SHELL (widget->parent))
    {
      if (menu_item->submenu == NULL)
	gtk_menu_shell_activate_item (GTK_MENU_SHELL (widget->parent),
				      widget, TRUE);
      else
	{
	  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget->parent);

	  if (!menu_shell->active)
	    {
	      gtk_grab_add (GTK_WIDGET (menu_shell));
	      menu_shell->have_grab = TRUE;
	      menu_shell->active = TRUE;
	    }

	  gtk_menu_shell_select_item (GTK_MENU_SHELL (widget->parent), widget); 
	  gtk_menu_item_popup_submenu (widget); 

	  _gtk_menu_shell_select_first (GTK_MENU_SHELL (menu_item->submenu));
	  submenu = GTK_MENU_SHELL (menu_item->submenu);
	}
    }
}
static void
gtk_real_menu_item_toggle_size_request (GtkMenuItem *menu_item,
					gint        *requisition)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  *requisition = 0;
}

static void
gtk_real_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
					 gint         allocation)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->toggle_size = allocation;
}

static gint
gtk_menu_item_select_timeout (gpointer data)
{
  GtkMenuItem *menu_item;
  
  GDK_THREADS_ENTER ();

  menu_item = GTK_MENU_ITEM (data);
  
  gtk_menu_item_popup_submenu (data);
  if (menu_item->timer_from_keypress && menu_item->submenu)
    GTK_MENU_SHELL (menu_item->submenu)->ignore_enter = TRUE;

  GDK_THREADS_LEAVE ();

  return FALSE;  
}

static void
gtk_menu_item_popup_submenu (gpointer data)
{
  GtkMenuItem *menu_item;

  menu_item = GTK_MENU_ITEM (data);
  menu_item->timer = 0;

  if (GTK_WIDGET_IS_SENSITIVE (menu_item->submenu))
    {
      gtk_menu_popup (GTK_MENU (menu_item->submenu),
		      GTK_WIDGET (menu_item)->parent,
		      GTK_WIDGET (menu_item),
		      gtk_menu_item_position_menu,
		      menu_item,
		      GTK_MENU_SHELL (GTK_WIDGET (menu_item)->parent)->button,
		      0);
    }
}

static void
gtk_menu_item_position_menu (GtkMenu  *menu,
			     gint     *x,
			     gint     *y,
			     gboolean *push_in,
			     gpointer  user_data)
{
  GtkMenuItem *menu_item;
  GtkWidget *widget;
  GtkWidget *parent_menu_item;
  gint screen_width;
  gint screen_height;
  gint twidth, theight;
  gint tx, ty;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  menu_item = GTK_MENU_ITEM (user_data);
  widget = GTK_WIDGET (user_data);

  twidth = GTK_WIDGET (menu)->requisition.width;
  theight = GTK_WIDGET (menu)->requisition.height;

  screen_width = gdk_screen_width ();
  screen_height = gdk_screen_height ();

  if (!gdk_window_get_origin (widget->window, &tx, &ty))
    {
      g_warning ("Menu not on screen");
      return;
    }

  tx += widget->allocation.x;
  ty += widget->allocation.y;

  switch (menu_item->submenu_placement)
    {
    case GTK_TOP_BOTTOM:
      if ((ty + widget->allocation.height + theight) <= screen_height)
	ty += widget->allocation.height;
      else if ((ty - theight) >= 0)
	ty -= theight;
      else if (screen_height - (ty + widget->allocation.height) > ty)
	ty += widget->allocation.height;
      else
	ty -= theight;
      break;

    case GTK_LEFT_RIGHT:
      menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
      parent_menu_item = GTK_MENU (widget->parent)->parent_menu_item;
      if (parent_menu_item)
	menu_item->submenu_direction = GTK_MENU_ITEM (parent_menu_item)->submenu_direction;

      switch (menu_item->submenu_direction)
	{
	case GTK_DIRECTION_LEFT:
	  if ((tx - twidth) >= 0)
	    tx -= twidth;
	  else
	    {
	      menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
	      tx += widget->allocation.width - 5;
	    }
	  break;

	case GTK_DIRECTION_RIGHT:
	  if ((tx + widget->allocation.width + twidth - 5) <= screen_width)
	    tx += widget->allocation.width - 5;
	  else
	    {
	      menu_item->submenu_direction = GTK_DIRECTION_LEFT;
	      tx -= twidth;
	    }
	  break;
	}

      ty += widget->allocation.height / 4;

      /* If the height of the menu doesn't fit we move it upward. */
      ty = CLAMP (ty, 0, MAX (0, screen_height - theight));
      break;
    }

  /* If we have negative, tx, here it is because we can't get
   * the menu all the way on screen. Favor the left portion.
   */
  *x = CLAMP (tx, 0, MAX (0, screen_width - twidth));
  *y = ty;
}

/**
 * gtk_menu_item_set_right_justified:
 * @menu_item: a #GtkMenuItem.
 * @right_justified: if %TRUE the menu item will appear at the 
 *   far right if added to a menu bar.
 * 
 * Sets whether the menu item appears justified at the right
 * side of a menu bar. This was traditionally done for "Help" menu
 * items, but is now considered a bad idea. (If the widget
 * layout is reversed for a right-to-left language like Hebrew
 * or Arabic, right-justified-menu-items appear at the left.)
 **/
void
gtk_menu_item_set_right_justified (GtkMenuItem *menu_item,
				   gboolean     right_justified)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  right_justified = right_justified != FALSE;

  if (right_justified != menu_item->right_justify)
    {
      menu_item->right_justify = right_justified;
      gtk_widget_queue_resize (GTK_WIDGET (menu_item));
    }
}

/**
 * gtk_menu_item_get_right_justified:
 * @menu_item: a #GtkMenuItem
 * 
 * Gets whether the menu item appears justified at the right
 * side of the menu bar.
 * 
 * Return value: %TRUE if the menu item will appear at the
 *   far right if added to a menu bar.
 **/
gboolean
gtk_menu_item_get_right_justified (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);
  
  return menu_item->right_justify;
}


static void
gtk_menu_item_show_all (GtkWidget *widget)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  menu_item = GTK_MENU_ITEM (widget);

  /* show children including submenu */
  if (menu_item->submenu)
    gtk_widget_show_all (menu_item->submenu);
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);

  gtk_widget_show (widget);
}

static void
gtk_menu_item_hide_all (GtkWidget *widget)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  gtk_widget_hide (widget);

  menu_item = GTK_MENU_ITEM (widget);

  /* hide children including submenu */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_hide_all, NULL);
  if (menu_item->submenu)
    gtk_widget_hide_all (menu_item->submenu);
}

static void
gtk_menu_item_accel_name_foreach (GtkWidget *widget,
				  gpointer data)
{
  const gchar **path_p = data;

  if (!*path_p)
    {
      if (GTK_IS_LABEL (widget))
	{
	  *path_p = gtk_label_get_text (GTK_LABEL (widget));
	  if (*path_p && (*path_p)[0] == 0)
	    *path_p = NULL;
	}
      else if (GTK_IS_CONTAINER (widget))
	gtk_container_foreach (GTK_CONTAINER (widget),
			       gtk_menu_item_accel_name_foreach,
			       data);
    }
}

static void
gtk_menu_item_parent_set (GtkWidget *widget,
			  GtkWidget *previous_parent)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenu *menu = GTK_IS_MENU (widget->parent) ? GTK_MENU (widget->parent) : NULL;

  if (menu)
    _gtk_menu_item_refresh_accel_path (menu_item,
				       menu->accel_path,
				       menu->accel_group,
				       TRUE);

  if (GTK_WIDGET_CLASS (parent_class)->parent_set)
    GTK_WIDGET_CLASS (parent_class)->parent_set (widget, previous_parent);
}

void
_gtk_menu_item_refresh_accel_path (GtkMenuItem   *menu_item,
				   const gchar   *prefix,
				   GtkAccelGroup *accel_group,
				   gboolean       group_changed)
{
  const gchar *path;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (!accel_group || GTK_IS_ACCEL_GROUP (accel_group));

  widget = GTK_WIDGET (menu_item);

  if (!accel_group)
    {
      gtk_widget_set_accel_path (widget, NULL, NULL);
      return;
    }

  path = _gtk_widget_get_accel_path (widget);
  if (!path)					/* no active accel_path yet */
    {
      path = menu_item->accel_path;
      if (!path && prefix)
	{
	  gchar *postfix = NULL;

	  /* try to construct one from label text */
	  gtk_container_foreach (GTK_CONTAINER (menu_item),
				 gtk_menu_item_accel_name_foreach,
				 &postfix);
	  menu_item->accel_path = postfix ? g_strconcat (prefix, "/", postfix, NULL) : NULL;
	  path = menu_item->accel_path;
	}
      if (path)
	gtk_widget_set_accel_path (widget, path, accel_group);
    }
  else if (group_changed)			/* reinstall accelerators */
    gtk_widget_set_accel_path (widget, path, accel_group);
}

/**
 * gtk_menu_item_set_accel_path
 * @menu_item:  a valid #GtkMenuItem
 * @accel_path: accelerator path, corresponding to this menu item's functionality
 *
 * Set the accelerator path on @menu_item, through which runtime changes of the
 * menu item's accelerator caused by the user can be identified and saved to
 * persistant storage (see gtk_accel_map_save() on this).
 * To setup a default accelerator for this menu item, call
 * gtk_accel_map_add_entry() with the same @accel_path.
 * See also gtk_accel_map_add_entry() on the specifics of accelerator paths,
 * and gtk_menu_set_accel_path() for a more convenient variant of this function.
 *
 * This function is basically a convenience wrapper that handles calling
 * gtk_widget_set_accel_path() with the appropriate accelerator group for
 * the menu item.
 */
void
gtk_menu_item_set_accel_path (GtkMenuItem *menu_item,
			      const gchar *accel_path)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (accel_path && accel_path[0] == '<' && strchr (accel_path, '/'));

  widget = GTK_WIDGET (menu_item);

  /* store new path */
  g_free (menu_item->accel_path);
  menu_item->accel_path = g_strdup (accel_path);

  /* forget accelerators associated with old path */
  gtk_widget_set_accel_path (widget, NULL, NULL);

  /* install accelerators associated with new path */
  if (widget->parent)
    {
      GtkMenu *menu = GTK_MENU (widget->parent);

      if (menu->accel_group)
	_gtk_menu_item_refresh_accel_path (GTK_MENU_ITEM (widget),
					   NULL,
					   menu->accel_group,
					   FALSE);
    }
}

static void
gtk_menu_item_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkBin *bin;
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (container));
  g_return_if_fail (callback != NULL);

  bin = GTK_BIN (container);
  menu_item = GTK_MENU_ITEM (container);

  if (bin->child)
    callback (bin->child, callback_data);
}

gboolean
_gtk_menu_item_is_selectable (GtkWidget *menu_item)
{
  if ((!GTK_BIN (menu_item)->child &&
       G_OBJECT_TYPE (menu_item) == GTK_TYPE_MENU_ITEM) ||
      GTK_IS_SEPARATOR_MENU_ITEM (menu_item) ||
      !GTK_WIDGET_IS_SENSITIVE (menu_item) ||
      !GTK_WIDGET_VISIBLE (menu_item))
    return FALSE;

  return TRUE;
}
