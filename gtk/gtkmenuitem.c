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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <string.h>
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtksignal.h"


#define BORDER_SPACING  3
#define SELECT_TIMEOUT  20

#define MENU_ITEM_CLASS(w)  GTK_MENU_ITEM_CLASS (GTK_OBJECT (w)->klass)


enum {
  ACTIVATE,
  LAST_SIGNAL
};


static void gtk_menu_item_class_init     (GtkMenuItemClass *klass);
static void gtk_menu_item_init           (GtkMenuItem      *menu_item);
static void gtk_menu_item_destroy        (GtkObject        *object);
static void gtk_menu_item_size_request   (GtkWidget        *widget,
					  GtkRequisition   *requisition);
static void gtk_menu_item_size_allocate  (GtkWidget        *widget,
					  GtkAllocation    *allocation);
static gint gtk_menu_item_install_accel  (GtkWidget        *widget,
					  const gchar      *signal_name,
					  gchar             key,
					  guint8            modifiers);
static void gtk_menu_item_remove_accel   (GtkWidget        *widget,
					  const gchar      *signal_name);
static void gtk_menu_item_paint          (GtkWidget        *widget,
					  GdkRectangle     *area);
static void gtk_menu_item_draw           (GtkWidget        *widget,
					  GdkRectangle     *area);
static gint gtk_menu_item_expose         (GtkWidget        *widget,
					  GdkEventExpose   *event);
static gint gtk_menu_item_enter          (GtkWidget        *widget,
					  GdkEventCrossing *event);
static gint gtk_menu_item_leave          (GtkWidget        *widget,
					  GdkEventCrossing *event);
static void gtk_real_menu_item_select    (GtkItem          *item);
static void gtk_real_menu_item_deselect  (GtkItem          *item);
static gint gtk_menu_item_select_timeout (gpointer          data);
static void gtk_menu_item_position_menu  (GtkMenu          *menu,
					  gint             *x,
					  gint             *y,
					  gpointer          user_data);
static void gtk_menu_item_show_all       (GtkWidget        *widget);
static void gtk_menu_item_hide_all       (GtkWidget        *widget);

static GtkItemClass *parent_class;
static guint menu_item_signals[LAST_SIGNAL] = { 0 };


guint
gtk_menu_item_get_type ()
{
  static guint menu_item_type = 0;

  if (!menu_item_type)
    {
      GtkTypeInfo menu_item_info =
      {
	"GtkMenuItem",
	sizeof (GtkMenuItem),
	sizeof (GtkMenuItemClass),
	(GtkClassInitFunc) gtk_menu_item_class_init,
	(GtkObjectInitFunc) gtk_menu_item_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      menu_item_type = gtk_type_unique (gtk_item_get_type (), &menu_item_info);
    }

  return menu_item_type;
}

static void
gtk_menu_item_class_init (GtkMenuItemClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkItemClass *item_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  item_class = (GtkItemClass*) klass;

  parent_class = gtk_type_class (gtk_item_get_type ());

  menu_item_signals[ACTIVATE] =
    gtk_signal_new ("activate",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuItemClass, activate),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, menu_item_signals, LAST_SIGNAL);

  object_class->destroy = gtk_menu_item_destroy;

  widget_class->activate_signal = menu_item_signals[ACTIVATE];
  widget_class->size_request = gtk_menu_item_size_request;
  widget_class->size_allocate = gtk_menu_item_size_allocate;
  widget_class->install_accelerator = gtk_menu_item_install_accel;
  widget_class->remove_accelerator = gtk_menu_item_remove_accel;
  widget_class->draw = gtk_menu_item_draw;
  widget_class->expose_event = gtk_menu_item_expose;
  widget_class->enter_notify_event = gtk_menu_item_enter;
  widget_class->leave_notify_event = gtk_menu_item_leave;
  widget_class->show_all = gtk_menu_item_show_all;
  widget_class->hide_all = gtk_menu_item_hide_all;  

  item_class->select = gtk_real_menu_item_select;
  item_class->deselect = gtk_real_menu_item_deselect;

  klass->activate = NULL;

  klass->toggle_size = 0;
  klass->shift_text = "Shft";
  klass->control_text = "Ctl";
  klass->alt_text = "Alt";
  klass->separator_text = "+";
}

static void
gtk_menu_item_init (GtkMenuItem *menu_item)
{
  menu_item->submenu = NULL;
  menu_item->accelerator_key = 0;
  menu_item->accelerator_mods = 0;
  menu_item->accelerator_size = 0;
  menu_item->accelerator_signal = 0;
  menu_item->toggle_size = 0;
  menu_item->show_toggle_indicator = FALSE;
  menu_item->show_submenu_indicator = FALSE;
  menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
  menu_item->submenu_placement = GTK_TOP_BOTTOM;
  menu_item->right_justify = FALSE;

  menu_item->timer = 0;
}

GtkWidget*
gtk_menu_item_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_menu_item_get_type ()));
}

GtkWidget*
gtk_menu_item_new_with_label (const gchar *label)
{
  GtkWidget *menu_item;
  GtkWidget *label_widget;

  menu_item = gtk_menu_item_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (menu_item), label_widget);
  gtk_widget_show (label_widget);

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
gtk_menu_item_accelerator_size (GtkMenuItem *menu_item)
{
  char buf[32];

  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (menu_item->accelerator_key)
    {
      gtk_menu_item_accelerator_text (menu_item, buf);
      menu_item->accelerator_size = gdk_string_width (GTK_WIDGET (menu_item)->style->font, buf) + 13;
    }
  else if (menu_item->submenu && menu_item->show_submenu_indicator)
    {
      menu_item->accelerator_size = 21;
    }
  else
    {
      menu_item->accelerator_size = 0;
    }
}

void
gtk_menu_item_accelerator_text (GtkMenuItem *menu_item,
				gchar       *buffer)
{
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (menu_item->accelerator_key)
    {
      buffer[0] = '\0';
      if (menu_item->accelerator_mods & GDK_SHIFT_MASK)
	{
	  strcat (buffer, MENU_ITEM_CLASS (menu_item)->shift_text);
	  strcat (buffer, MENU_ITEM_CLASS (menu_item)->separator_text);
	}
      if (menu_item->accelerator_mods & GDK_CONTROL_MASK)
	{
	  strcat (buffer, MENU_ITEM_CLASS (menu_item)->control_text);
	  strcat (buffer, MENU_ITEM_CLASS (menu_item)->separator_text);
	}
      if (menu_item->accelerator_mods & GDK_MOD1_MASK)
	{
	  strcat (buffer, MENU_ITEM_CLASS (menu_item)->alt_text);
	  strcat (buffer, MENU_ITEM_CLASS (menu_item)->separator_text);
	}
      strncat (buffer, &menu_item->accelerator_key, 1);
    }
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
  gtk_item_select (GTK_ITEM (menu_item));
}

void
gtk_menu_item_deselect (GtkMenuItem *menu_item)
{
  gtk_item_deselect (GTK_ITEM (menu_item));
}

void
gtk_menu_item_activate (GtkMenuItem *menu_item)
{
  gtk_signal_emit (GTK_OBJECT (menu_item), menu_item_signals[ACTIVATE]);
}


static void
gtk_menu_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);

  gtk_menu_item_accelerator_size (GTK_MENU_ITEM (widget));

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->klass->xthickness +
			BORDER_SPACING) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->klass->ythickness) * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;
      requisition->height += bin->child->requisition.height;
    }
}

static void
gtk_menu_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  bin = GTK_BIN (widget);

  if (bin->child)
    {
      child_allocation.x = (GTK_CONTAINER (widget)->border_width +
                            widget->style->klass->xthickness +
			    BORDER_SPACING);
      child_allocation.y = GTK_CONTAINER (widget)->border_width;
      child_allocation.width = allocation->width - child_allocation.x * 2;
      child_allocation.height = allocation->height - child_allocation.y * 2;
      child_allocation.x += GTK_MENU_ITEM (widget)->toggle_size;
      child_allocation.width -= (GTK_MENU_ITEM (widget)->toggle_size +
				 GTK_MENU_ITEM (widget)->accelerator_size);

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static gint
gtk_menu_item_install_accel (GtkWidget   *widget,
			     const gchar *signal_name,
			     gchar        key,
			     guint8       modifiers)
{
  GtkMenuItem *menu_item;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (signal_name != NULL, FALSE);

  menu_item = GTK_MENU_ITEM (widget);

  menu_item->accelerator_signal = gtk_signal_lookup (signal_name, GTK_OBJECT_TYPE (widget));
  if (menu_item->accelerator_signal > 0)
    {
      menu_item->accelerator_key = key;
      menu_item->accelerator_mods = modifiers;

      if (widget->parent)
	gtk_widget_queue_resize (widget);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_menu_item_remove_accel (GtkWidget   *widget,
			    const gchar *signal_name)
{
  GtkMenuItem *menu_item;
  guint signal_num;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (signal_name != NULL);

  menu_item = GTK_MENU_ITEM (widget);

  signal_num = gtk_signal_lookup (signal_name, GTK_OBJECT_TYPE (widget));
  if (menu_item->accelerator_signal == signal_num)
    {
      menu_item->accelerator_key = 0;
      menu_item->accelerator_mods = 0;
      menu_item->accelerator_signal = 0;

      if (GTK_WIDGET_VISIBLE (widget))
	{
	  gtk_widget_queue_draw (widget);
	  GTK_MENU_SHELL (widget->parent)->menu_flag = TRUE;
	}
      else
	gtk_container_need_resize (GTK_CONTAINER (widget->parent));
    }
}

static void
gtk_menu_item_paint (GtkWidget    *widget,
		     GdkRectangle *area)
{
  GtkMenuItem *menu_item;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GdkFont *font;
  gint width, height;
  gint x, y;
  char buf[32];

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);

      state_type = widget->state;
      if (!GTK_WIDGET_IS_SENSITIVE (widget))
	state_type = GTK_STATE_INSENSITIVE;

      gtk_style_set_background (widget->style, widget->window, state_type);
      gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      x = GTK_CONTAINER (menu_item)->border_width;
      y = GTK_CONTAINER (menu_item)->border_width;
      width = widget->allocation.width - x * 2;
      height = widget->allocation.height - y * 2;

      if ((state_type == GTK_STATE_PRELIGHT) &&
	  (GTK_BIN (menu_item)->child))
	gtk_draw_shadow (widget->style,
			 widget->window,
			 GTK_STATE_PRELIGHT,
			 GTK_SHADOW_OUT,
			 x, y, width, height);

      if (menu_item->accelerator_key)
	{
	  gtk_menu_item_accelerator_text (menu_item, buf);

	  font = widget->style->font;
	  x = x + width - menu_item->accelerator_size + 13 - 4;
	  y = y + ((height - (font->ascent + font->descent)) / 2) + font->ascent;

	  if (state_type == GTK_STATE_INSENSITIVE)
	    gdk_draw_string (widget->window, widget->style->font,
			     widget->style->white_gc,
			     x + 1, y + 1, buf);

	  gdk_draw_string (widget->window, widget->style->font,
			   widget->style->fg_gc[state_type],
			   x, y, buf);
	}
      else if (menu_item->submenu && menu_item->show_submenu_indicator)
	{
	  shadow_type = GTK_SHADOW_OUT;
	  if (state_type == GTK_STATE_PRELIGHT)
	    shadow_type = GTK_SHADOW_IN;

	  gtk_draw_arrow (widget->style, widget->window,
			  state_type, shadow_type, GTK_ARROW_RIGHT, FALSE,
			  x + width - 15, y + height / 2 - 5, 10, 10);
	}
      else if (!GTK_BIN (menu_item)->child)
	{
	  gtk_draw_hline (widget->style, widget->window, GTK_STATE_NORMAL,
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

static gint
gtk_menu_item_enter (GtkWidget        *widget,
		     GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (widget->parent, (GdkEvent*) event);
}

static gint
gtk_menu_item_leave (GtkWidget        *widget,
		     GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (widget->parent, (GdkEvent*) event);
}

static void
gtk_real_menu_item_select (GtkItem *item)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  menu_item = GTK_MENU_ITEM (item);

  if (menu_item->submenu && !GTK_WIDGET_VISIBLE (menu_item->submenu))
    menu_item->timer = gtk_timeout_add (SELECT_TIMEOUT, gtk_menu_item_select_timeout, menu_item);

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
      if (menu_item->timer)
	gtk_timeout_remove (menu_item->timer);
      else
	gtk_menu_popdown (GTK_MENU (menu_item->submenu));
    }

  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_NORMAL);
  gtk_widget_draw (GTK_WIDGET (menu_item), NULL);
}

static gint
gtk_menu_item_select_timeout (gpointer data)
{
  GtkMenuItem *menu_item;

  menu_item = GTK_MENU_ITEM (data);
  menu_item->timer = 0;

  gtk_menu_popup (GTK_MENU (menu_item->submenu),
		  GTK_WIDGET (menu_item)->parent,
		  GTK_WIDGET (menu_item),
		  gtk_menu_item_position_menu,
		  menu_item,
		  GTK_MENU_SHELL (GTK_WIDGET (menu_item)->parent)->button,
		  0);

  return FALSE;
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

  g_return_if_fail (gdk_window_get_origin (GTK_WIDGET (menu_item)->window, &tx, &ty));

  switch (menu_item->submenu_placement)
    {
    case GTK_TOP_BOTTOM:
      if ((ty + GTK_WIDGET (menu_item)->allocation.height + theight) <= screen_height)
	ty += GTK_WIDGET (menu_item)->allocation.height;
      else if ((ty - theight) >= 0)
	ty -= theight;
      else
	ty += GTK_WIDGET (menu_item)->allocation.height;

      if ((tx + twidth) > screen_width)
	{
	  tx -= ((tx + twidth) - screen_width);
	  if (tx < 0)
	    tx = 0;
	}
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

      if ((ty + GTK_WIDGET (menu_item)->allocation.height / 4 + theight) <= screen_height)
	ty += GTK_WIDGET (menu_item)->allocation.height / 4;
      else
	{
	  ty -= ((ty + theight) - screen_height);
	  if (ty < 0)
	    ty = 0;
	}
      break;
    }

  *x = tx;
  *y = ty;
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
  GtkContainer *container;
  GtkMenuItem  *menu_item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  container = GTK_CONTAINER (widget);
  menu_item = GTK_MENU_ITEM (widget);

  /* Show children, traverse to submenu, show self. */
  gtk_container_foreach (container, (GtkCallback) gtk_widget_show_all, NULL);
  if (menu_item->submenu)
    gtk_widget_show_all (menu_item->submenu);
  gtk_widget_show (widget);
}


static void
gtk_menu_item_hide_all (GtkWidget *widget)
{
  GtkContainer *container;
  GtkMenuItem  *menu_item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  container = GTK_CONTAINER (widget);
  menu_item = GTK_MENU_ITEM (widget);

  /* Reverse order of gtk_menu_item_show_all */
  gtk_widget_hide (widget);
  if (menu_item->submenu)
    gtk_widget_hide_all (menu_item->submenu);
  gtk_container_foreach (container, (GtkCallback) gtk_widget_hide_all, NULL);
}
