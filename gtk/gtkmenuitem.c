/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <string.h>
#include "gtkaccellabel.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtksignal.h"


#define BORDER_SPACING  3
#define SELECT_TIMEOUT  75

#define MENU_ITEM_CLASS(w)  GTK_MENU_ITEM_CLASS (GTK_OBJECT (w)->klass)


enum {
  ACTIVATE,
  ACTIVATE_ITEM,
  LAST_SIGNAL
};


static void gtk_menu_item_class_init     (GtkMenuItemClass *klass);
static void gtk_menu_item_init           (GtkMenuItem      *menu_item);
static void gtk_menu_item_destroy        (GtkObject        *object);
static void gtk_menu_item_size_request   (GtkWidget        *widget,
					  GtkRequisition   *requisition);
static void gtk_menu_item_size_allocate  (GtkWidget        *widget,
					  GtkAllocation    *allocation);
static void gtk_menu_item_paint          (GtkWidget        *widget,
					  GdkRectangle     *area);
static void gtk_menu_item_draw           (GtkWidget        *widget,
					  GdkRectangle     *area);
static gint gtk_menu_item_expose         (GtkWidget        *widget,
					  GdkEventExpose   *event);
static void gtk_real_menu_item_select    (GtkItem          *item);
static void gtk_real_menu_item_deselect  (GtkItem          *item);
static void gtk_real_menu_item_activate_item  (GtkMenuItem      *item);
static gint gtk_menu_item_select_timeout (gpointer          data);
static void gtk_menu_item_popup_submenu  (gpointer     data);
static void gtk_menu_item_position_menu  (GtkMenu          *menu,
					  gint             *x,
					  gint             *y,
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
      static const GtkTypeInfo menu_item_info =
      {
	"GtkMenuItem",
	sizeof (GtkMenuItem),
	sizeof (GtkMenuItemClass),
	(GtkClassInitFunc) gtk_menu_item_class_init,
	(GtkObjectInitFunc) gtk_menu_item_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      menu_item_type = gtk_type_unique (gtk_item_get_type (), &menu_item_info);
      gtk_type_set_chunk_alloc (menu_item_type, 16);
    }

  return menu_item_type;
}

static void
gtk_menu_item_class_init (GtkMenuItemClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkItemClass *item_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  item_class = (GtkItemClass*) klass;

  parent_class = gtk_type_class (gtk_item_get_type ());

  menu_item_signals[ACTIVATE] =
    gtk_signal_new ("activate",
                    GTK_RUN_FIRST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, activate),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  menu_item_signals[ACTIVATE_ITEM] =
    gtk_signal_new ("activate_item",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, activate_item),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, menu_item_signals, LAST_SIGNAL);

  object_class->destroy = gtk_menu_item_destroy;

  widget_class->activate_signal = menu_item_signals[ACTIVATE];
  widget_class->size_request = gtk_menu_item_size_request;
  widget_class->size_allocate = gtk_menu_item_size_allocate;
  widget_class->draw = gtk_menu_item_draw;
  widget_class->expose_event = gtk_menu_item_expose;
  widget_class->show_all = gtk_menu_item_show_all;
  widget_class->hide_all = gtk_menu_item_hide_all;

  container_class->forall = gtk_menu_item_forall;

  item_class->select = gtk_real_menu_item_select;
  item_class->deselect = gtk_real_menu_item_deselect;

  klass->activate = NULL;
  klass->activate_item = gtk_real_menu_item_activate_item;

  klass->toggle_size = 0;
  klass->hide_on_activate = TRUE;
}

static void
gtk_menu_item_init (GtkMenuItem *menu_item)
{
  menu_item->submenu = NULL;
  menu_item->accelerator_signal = menu_item_signals[ACTIVATE];
  menu_item->toggle_size = 0;
  menu_item->accelerator_width = 0;
  menu_item->show_toggle_indicator = FALSE;
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

static void
gtk_menu_item_destroy (GtkObject *object)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (object));

  menu_item = GTK_MENU_ITEM (object);

  if (menu_item->submenu)
    gtk_widget_destroy (menu_item->submenu);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_menu_item_detacher (GtkWidget     *widget,
			GtkMenu       *menu)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  menu_item = GTK_MENU_ITEM (widget);
  g_return_if_fail (menu_item->submenu == (GtkWidget*) menu);

  menu_item->submenu = NULL;
}

void
gtk_menu_item_set_submenu (GtkMenuItem *menu_item,
			   GtkWidget   *submenu)
{
  g_return_if_fail (menu_item != NULL);
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

void
gtk_menu_item_remove_submenu (GtkMenuItem         *menu_item)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
      
  if (menu_item->submenu)
    gtk_menu_detach (GTK_MENU (menu_item->submenu));
}

void
gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
			     GtkSubmenuPlacement  placement)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->submenu_placement = placement;
}

void
gtk_menu_item_configure (GtkMenuItem *menu_item,
			 gint         show_toggle_indicator,
			 gint         show_submenu_indicator)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->show_toggle_indicator = (show_toggle_indicator == TRUE);
  menu_item->show_submenu_indicator = (show_submenu_indicator == TRUE);
}

void
gtk_menu_item_select (GtkMenuItem *menu_item)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  gtk_item_select (GTK_ITEM (menu_item));
}

void
gtk_menu_item_deselect (GtkMenuItem *menu_item)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  gtk_item_deselect (GTK_ITEM (menu_item));
}

void
gtk_menu_item_activate (GtkMenuItem *menu_item)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  
  gtk_signal_emit (GTK_OBJECT (menu_item), menu_item_signals[ACTIVATE]);
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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);
  menu_item = GTK_MENU_ITEM (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->klass->xthickness +
			BORDER_SPACING) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->klass->ythickness) * 2;

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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  menu_item = GTK_MENU_ITEM (widget);
  bin = GTK_BIN (widget);
  
  widget->allocation = *allocation;

  if (bin->child)
    {
      child_allocation.x = (GTK_CONTAINER (widget)->border_width +
                            widget->style->klass->xthickness +
			    BORDER_SPACING);
      child_allocation.y = (GTK_CONTAINER (widget)->border_width +
			    widget->style->klass->ythickness);
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);
      child_allocation.x += GTK_MENU_ITEM (widget)->toggle_size;
      child_allocation.width -= GTK_MENU_ITEM (widget)->toggle_size;
      if (menu_item->submenu && menu_item->show_submenu_indicator)
	child_allocation.width -= 21;
      
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  if (menu_item->submenu)
    gtk_menu_reposition (GTK_MENU (menu_item->submenu));
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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);

      state_type = widget->state;

      x = GTK_CONTAINER (menu_item)->border_width;
      y = GTK_CONTAINER (menu_item)->border_width;
      width = widget->allocation.width - x * 2;
      height = widget->allocation.height - y * 2;
      
      if ((state_type == GTK_STATE_PRELIGHT) &&
	  (GTK_BIN (menu_item)->child))
	gtk_paint_box (widget->style,
		       widget->window,
		       GTK_STATE_PRELIGHT,
		       GTK_SHADOW_OUT,
		       area, widget, "menuitem",
		       x, y, width, height);
      else
	{
	  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
	  gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);
	}

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
			    0, widget->allocation.width, 0);
	}
    }
}

static void
gtk_menu_item_draw (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_item_paint (widget, area);

      bin = GTK_BIN (widget);

      if (bin->child)
        {
          if (gtk_widget_intersect (bin->child, area, &child_area))
            gtk_widget_draw (bin->child, &child_area);
        }
    }
}

static gint
gtk_menu_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_item_paint (widget, &event->area);

      bin = GTK_BIN (widget);

      if (bin->child)
        {
          child_event = *event;

          if (GTK_WIDGET_NO_WINDOW (bin->child) &&
              gtk_widget_intersect (bin->child, &event->area, &child_event.area))
            gtk_widget_event (bin->child, (GdkEvent*) &child_event);
        }
    }

  return FALSE;
}

static void
gtk_real_menu_item_select (GtkItem *item)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  menu_item = GTK_MENU_ITEM (item);

  /*  if (menu_item->submenu && !GTK_WIDGET_VISIBLE (menu_item->submenu))*/
  if (menu_item->submenu)
    {
      guint32 etime;
      GdkEvent *event = gtk_get_current_event ();

      etime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
      if (etime >= last_submenu_deselect_time &&
	  last_submenu_deselect_time + SELECT_TIMEOUT > etime)
	menu_item->timer = gtk_timeout_add (SELECT_TIMEOUT - (etime - last_submenu_deselect_time),
					    gtk_menu_item_select_timeout,
					    menu_item);
      else
	gtk_menu_item_popup_submenu (menu_item);
      if(event) gdk_event_free(event);
    }
  
  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_PRELIGHT);
  gtk_widget_draw (GTK_WIDGET (menu_item), NULL);
}

static void
gtk_real_menu_item_deselect (GtkItem *item)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (item != NULL);
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
      if(event) gdk_event_free(event);
    }

  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_NORMAL);
  gtk_widget_draw (GTK_WIDGET (menu_item), NULL);
}

static void
gtk_real_menu_item_activate_item (GtkMenuItem *menu_item)
{
  GtkWidget *widget;
  GtkMenuShell *submenu; 

  g_return_if_fail (menu_item != NULL);
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

	  submenu = GTK_MENU_SHELL (menu_item->submenu);
	  if (submenu->children)
	    gtk_menu_shell_select_item (submenu, submenu->children->data);
	}
    }
}

static gint
gtk_menu_item_select_timeout (gpointer data)
{
  GDK_THREADS_ENTER ();

  gtk_menu_item_popup_submenu (data);

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
			     gpointer  user_data)
{
  GtkMenuItem *menu_item;
  GtkWidget *parent_menu_item;
  gint screen_width;
  gint screen_height;
  gint twidth, theight;
  gint tx, ty;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  menu_item = GTK_MENU_ITEM (user_data);

  twidth = GTK_WIDGET (menu)->requisition.width;
  theight = GTK_WIDGET (menu)->requisition.height;

  screen_width = gdk_screen_width ();
  screen_height = gdk_screen_height ();

  if (!gdk_window_get_origin (GTK_WIDGET (menu_item)->window, &tx, &ty))
    {
      g_warning ("Menu not on screen");
      return;
    }

  switch (menu_item->submenu_placement)
    {
    case GTK_TOP_BOTTOM:
      if ((ty + GTK_WIDGET (menu_item)->allocation.height + theight) <= screen_height)
	ty += GTK_WIDGET (menu_item)->allocation.height;
      else if ((ty - theight) >= 0)
	ty -= theight;
      else
	ty += GTK_WIDGET (menu_item)->allocation.height;

      break;

    case GTK_LEFT_RIGHT:
      menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
      parent_menu_item = GTK_MENU (GTK_WIDGET (menu_item)->parent)->parent_menu_item;
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
	      tx += GTK_WIDGET (menu_item)->allocation.width - 5;
	    }
	  break;

	case GTK_DIRECTION_RIGHT:
	  if ((tx + GTK_WIDGET (menu_item)->allocation.width + twidth - 5) <= screen_width)
	    tx += GTK_WIDGET (menu_item)->allocation.width - 5;
	  else
	    {
	      menu_item->submenu_direction = GTK_DIRECTION_LEFT;
	      tx -= twidth;
	    }
	  break;
	}

      ty += GTK_WIDGET (menu_item)->allocation.height / 4;

      break;
    }

  /* If we have negative, tx, ty here it is because we can't get
   * the menu all the way on screen. Favor the upper-left portion.
   */
  *x = CLAMP (tx, 0, MAX (0, screen_width - twidth));
  *y = CLAMP (ty, 0, MAX (0, screen_height - theight));
}

void
gtk_menu_item_right_justify(GtkMenuItem *menuitem)
{
  g_return_if_fail (menuitem != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menuitem));

  menuitem->right_justify = 1;
}


static void
gtk_menu_item_show_all (GtkWidget *widget)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (widget != NULL);
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

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  gtk_widget_hide (widget);

  menu_item = GTK_MENU_ITEM (widget);

  /* hide children including submenu */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_hide_all, NULL);
  if (menu_item->submenu)
    gtk_widget_hide_all (menu_item->submenu);
}

static void
gtk_menu_item_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkBin *bin;
  GtkMenuItem *menu_item;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (container));
  g_return_if_fail (callback != NULL);

  bin = GTK_BIN (container);
  menu_item = GTK_MENU_ITEM (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);
  if (include_internals && menu_item->submenu)
    (* callback) (menu_item->submenu, callback_data);
}
