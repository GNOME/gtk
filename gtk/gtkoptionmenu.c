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

#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkoptionmenu.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"


#define CHILD_LEFT_SPACING        5
#define CHILD_RIGHT_SPACING       1
#define CHILD_TOP_SPACING         1
#define CHILD_BOTTOM_SPACING      1

typedef struct _GtkOptionMenuProps GtkOptionMenuProps;

struct _GtkOptionMenuProps
{
  gint indicator_width;
  gint indicator_height;
  gint indicator_left_spacing;
  gint indicator_right_spacing;
  gint indicator_top_spacing;
  gint indicator_bottom_spacing;
};

static GtkOptionMenuProps default_props = {
  12,				/* width */
  8,				/* height */
  3,				/* left_spacing */
  7,				/* right_spacing */
  2,				/* top_spacing */
  2,				/* bottom_spacing */
};

static void gtk_option_menu_class_init      (GtkOptionMenuClass *klass);
static void gtk_option_menu_init            (GtkOptionMenu      *option_menu);
static void gtk_option_menu_destroy         (GtkObject          *object);
static void gtk_option_menu_size_request    (GtkWidget          *widget,
					     GtkRequisition     *requisition);
static void gtk_option_menu_size_allocate   (GtkWidget          *widget,
					     GtkAllocation      *allocation);
static void gtk_option_menu_paint           (GtkWidget          *widget,
					     GdkRectangle       *area);
static void gtk_option_menu_draw            (GtkWidget          *widget,
					     GdkRectangle       *area);
static gint gtk_option_menu_expose          (GtkWidget          *widget,
					     GdkEventExpose     *event);
static gint gtk_option_menu_button_press    (GtkWidget          *widget,
					     GdkEventButton     *event);
static gint gtk_option_menu_key_press	    (GtkWidget          *widget,
					     GdkEventKey        *event);
static void gtk_option_menu_deactivate      (GtkMenuShell       *menu_shell,
					     GtkOptionMenu      *option_menu);
static void gtk_option_menu_update_contents (GtkOptionMenu      *option_menu);
static void gtk_option_menu_remove_contents (GtkOptionMenu      *option_menu);
static void gtk_option_menu_calc_size       (GtkOptionMenu      *option_menu);
static void gtk_option_menu_position        (GtkMenu            *menu,
					     gint               *x,
					     gint               *y,
					     gpointer            user_data);
static void gtk_option_menu_show_all        (GtkWidget          *widget);
static void gtk_option_menu_hide_all        (GtkWidget          *widget);
static GtkType gtk_option_menu_child_type   (GtkContainer       *container);

				       

static GtkButtonClass *parent_class = NULL;


GtkType
gtk_option_menu_get_type (void)
{
  static GtkType option_menu_type = 0;

  if (!option_menu_type)
    {
      static const GtkTypeInfo option_menu_info =
      {
	"GtkOptionMenu",
	sizeof (GtkOptionMenu),
	sizeof (GtkOptionMenuClass),
	(GtkClassInitFunc) gtk_option_menu_class_init,
	(GtkObjectInitFunc) gtk_option_menu_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      option_menu_type = gtk_type_unique (gtk_button_get_type (), &option_menu_info);
    }

  return option_menu_type;
}

static void
gtk_option_menu_class_init (GtkOptionMenuClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_button_get_type ());

  object_class->destroy = gtk_option_menu_destroy;

  widget_class->draw = gtk_option_menu_draw;
  widget_class->size_request = gtk_option_menu_size_request;
  widget_class->size_allocate = gtk_option_menu_size_allocate;
  widget_class->expose_event = gtk_option_menu_expose;
  widget_class->button_press_event = gtk_option_menu_button_press;
  widget_class->key_press_event = gtk_option_menu_key_press;
  widget_class->show_all = gtk_option_menu_show_all;
  widget_class->hide_all = gtk_option_menu_hide_all;

  container_class->child_type = gtk_option_menu_child_type;
}

static GtkType
gtk_option_menu_child_type (GtkContainer       *container)
{
  return GTK_TYPE_NONE;
}

static void
gtk_option_menu_init (GtkOptionMenu *option_menu)
{
  GTK_WIDGET_SET_FLAGS (option_menu, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS (option_menu, GTK_CAN_DEFAULT | GTK_RECEIVES_DEFAULT);

  option_menu->menu = NULL;
  option_menu->menu_item = NULL;
  option_menu->width = 0;
  option_menu->height = 0;
}

GtkWidget*
gtk_option_menu_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_option_menu_get_type ()));
}

GtkWidget*
gtk_option_menu_get_menu (GtkOptionMenu *option_menu)
{
  g_return_val_if_fail (option_menu != NULL, NULL);
  g_return_val_if_fail (GTK_IS_OPTION_MENU (option_menu), NULL);

  return option_menu->menu;
}

static void
gtk_option_menu_detacher (GtkWidget     *widget,
			  GtkMenu	*menu)
{
  GtkOptionMenu *option_menu;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));

  option_menu = GTK_OPTION_MENU (widget);
  g_return_if_fail (option_menu->menu == (GtkWidget*) menu);

  gtk_option_menu_remove_contents (option_menu);
  gtk_signal_disconnect_by_data (GTK_OBJECT (option_menu->menu),
				 option_menu);
  
  option_menu->menu = NULL;
}

void
gtk_option_menu_set_menu (GtkOptionMenu *option_menu,
			  GtkWidget     *menu)
{
  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));
  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  if (option_menu->menu != menu)
    {
      gtk_option_menu_remove_menu (option_menu);

      option_menu->menu = menu;
      gtk_menu_attach_to_widget (GTK_MENU (menu),
				 GTK_WIDGET (option_menu),
				 gtk_option_menu_detacher);

      gtk_option_menu_calc_size (option_menu);

      gtk_signal_connect (GTK_OBJECT (option_menu->menu), "deactivate",
			  (GtkSignalFunc) gtk_option_menu_deactivate,
			  option_menu);

      if (GTK_WIDGET (option_menu)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (option_menu));

      gtk_option_menu_update_contents (option_menu);
    }
}

void
gtk_option_menu_remove_menu (GtkOptionMenu *option_menu)
{
  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu)
    gtk_menu_detach (GTK_MENU (option_menu->menu));
}

void
gtk_option_menu_set_history (GtkOptionMenu *option_menu,
			     guint          index)
{
  GtkWidget *menu_item;

  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu)
    {
      gtk_menu_set_active (GTK_MENU (option_menu->menu), index);
      menu_item = gtk_menu_get_active (GTK_MENU (option_menu->menu));

      if (menu_item != option_menu->menu_item)
	gtk_option_menu_update_contents (option_menu);
    }
}

static void
gtk_option_menu_destroy (GtkObject *object)
{
  GtkOptionMenu *option_menu;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (object));

  option_menu = GTK_OPTION_MENU (object);

  if (option_menu->menu)
    gtk_widget_destroy (option_menu->menu);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_option_menu_get_props (GtkOptionMenu       *option_menu,
			   GtkOptionMenuProps  *props)
{
  GtkWidget *widget =  GTK_WIDGET (option_menu);

  props->indicator_width = gtk_style_get_prop_experimental (widget->style,
							    "GtkOptionMenu::indicator_width",
							    default_props.indicator_width);

  props->indicator_height = gtk_style_get_prop_experimental (widget->style,
							     "GtkOptionMenu::indicator_height",
							     default_props.indicator_height);

  props->indicator_top_spacing = gtk_style_get_prop_experimental (widget->style,
								  "GtkOptionMenu::indicator_top_spacing",
								  default_props.indicator_top_spacing);
  props->indicator_bottom_spacing = gtk_style_get_prop_experimental (widget->style,
								     "GtkOptionMenu::indicator_bottom_spacing",
								     default_props.indicator_bottom_spacing);
  props->indicator_left_spacing = gtk_style_get_prop_experimental (widget->style,
							       "GtkOptionMenu::indicator_left_spacing",
							       default_props.indicator_left_spacing);
  props->indicator_right_spacing = gtk_style_get_prop_experimental (widget->style,
								    "GtkOptionMenu::indicator_right_spacing",
								    default_props.indicator_right_spacing);
}

static void
gtk_option_menu_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
  GtkOptionMenu *option_menu;
  GtkOptionMenuProps props;
  gint tmp;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  g_return_if_fail (requisition != NULL);

  option_menu = GTK_OPTION_MENU (widget);

  gtk_option_menu_get_props (option_menu, &props);

  requisition->width = ((GTK_CONTAINER (widget)->border_width +
			 GTK_WIDGET (widget)->style->klass->xthickness) * 2 +
			option_menu->width +
			props.indicator_width +
			props.indicator_left_spacing + props.indicator_right_spacing +
			CHILD_LEFT_SPACING + CHILD_RIGHT_SPACING + 2);
  requisition->height = ((GTK_CONTAINER (widget)->border_width +
			  GTK_WIDGET (widget)->style->klass->ythickness) * 2 +
			 option_menu->height +
			 CHILD_TOP_SPACING + CHILD_BOTTOM_SPACING + 2);

  tmp = (requisition->height - option_menu->height +
	 props.indicator_height + props.indicator_top_spacing + props.indicator_bottom_spacing);
  requisition->height = MAX (requisition->height, tmp);
}

static void
gtk_option_menu_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkWidget *child;
  GtkAllocation child_allocation;
  GtkOptionMenuProps props;
    
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  g_return_if_fail (allocation != NULL);

  gtk_option_menu_get_props (GTK_OPTION_MENU (widget), &props);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  child = GTK_BIN (widget)->child;
  if (child && GTK_WIDGET_VISIBLE (child))
    {
      child_allocation.x = (GTK_CONTAINER (widget)->border_width +
			    GTK_WIDGET (widget)->style->klass->xthickness) + 1;
      child_allocation.y = (GTK_CONTAINER (widget)->border_width +
			    GTK_WIDGET (widget)->style->klass->ythickness) + 1;
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2 -
				    props.indicator_width - props.indicator_left_spacing - props.indicator_right_spacing -
				    CHILD_LEFT_SPACING - CHILD_RIGHT_SPACING - 2);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2 -
				     CHILD_TOP_SPACING - CHILD_BOTTOM_SPACING - 2);
      child_allocation.x += CHILD_LEFT_SPACING;
      child_allocation.y += CHILD_TOP_SPACING;

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
gtk_option_menu_paint (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GdkRectangle button_area;
  GtkOptionMenuProps props;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_option_menu_get_props (GTK_OPTION_MENU (widget), &props);

      button_area.x = GTK_CONTAINER (widget)->border_width + 1;
      button_area.y = GTK_CONTAINER (widget)->border_width + 1;
      button_area.width = widget->allocation.width - button_area.x * 2;
      button_area.height = widget->allocation.height - button_area.y * 2;

      /* This is evil, and should be elimated here and in the button
       * code. The point is to clear the focus, and make it
       * sort of transparent if it isn't there.
       */
      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
      gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      gtk_paint_box(widget->style, widget->window,
		    GTK_WIDGET_STATE (widget), GTK_SHADOW_OUT,
		    area, widget, "optionmenu",
		    button_area.x, button_area.y,
		    button_area.width, button_area.height);
      
      gtk_paint_tab (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), GTK_SHADOW_OUT,
		     area, widget, "optionmenutab",
		     button_area.x + button_area.width - 
		     props.indicator_width - props.indicator_right_spacing -
		     widget->style->klass->xthickness,
		     button_area.y + (button_area.height - props.indicator_height) / 2,
		     props.indicator_width, props.indicator_height);
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	gtk_paint_focus (widget->style, widget->window,
			 area, widget, "button",
			 button_area.x - 1, 
			 button_area.y - 1, 
			 button_area.width + 1,
			 button_area.height + 1);
    }
}

static void
gtk_option_menu_draw (GtkWidget    *widget,
		      GdkRectangle *area)
{
  GtkWidget *child;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_option_menu_paint (widget, area);

      child = GTK_BIN (widget)->child;
      if (child && gtk_widget_intersect (child, area, &child_area))
	gtk_widget_draw (child, &child_area);
    }
}

static gint
gtk_option_menu_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
  GtkWidget *child;
  GdkEventExpose child_event;
  gint remove_child;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_option_menu_paint (widget, &event->area);


      /* The following code tries to draw the child in two places at
       * once. It fails miserably for several reasons
       *
       * - If the child is not no-window, removing generates
       *   more expose events. Bad, bad, bad.
       * 
       * - Even if the child is no-window, removing it now (properly)
       *   clears the space where it was, so it does no good
       */
      
#if 0
      remove_child = FALSE;
      child = GTK_BUTTON (widget)->child;

      if (!child)
	{
	  if (!GTK_OPTION_MENU (widget)->menu)
	    return FALSE;
	  gtk_option_menu_update_contents (GTK_OPTION_MENU (widget));
	  child = GTK_BUTTON (widget)->child;
	  if (!child)
	    return FALSE;
	  remove_child = TRUE;
	}

      child_event = *event;

      if (GTK_WIDGET_NO_WINDOW (child) &&
	  gtk_widget_intersect (child, &event->area, &child_event.area))
	gtk_widget_event (child, (GdkEvent*) &child_event);

      if (remove_child)
	gtk_option_menu_remove_contents (GTK_OPTION_MENU (widget));
#else
      remove_child = FALSE;
      child = GTK_BIN (widget)->child;
      child_event = *event;
      if (child && GTK_WIDGET_NO_WINDOW (child) &&
	  gtk_widget_intersect (child, &event->area, &child_event.area))
	gtk_widget_event (child, (GdkEvent*) &child_event);

#endif /* 0 */
    }

  return FALSE;
}

static gint
gtk_option_menu_button_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkOptionMenu *option_menu;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  option_menu = GTK_OPTION_MENU (widget);

  if ((event->type == GDK_BUTTON_PRESS) &&
      (event->button == 1))
    {
      gtk_option_menu_remove_contents (option_menu);
      gtk_menu_popup (GTK_MENU (option_menu->menu), NULL, NULL,
		      gtk_option_menu_position, option_menu,
		      event->button, event->time);
      return TRUE;
    }

  return FALSE;
}

static gint
gtk_option_menu_key_press (GtkWidget   *widget,
			   GdkEventKey *event)
{
  GtkOptionMenu *option_menu;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  option_menu = GTK_OPTION_MENU (widget);

  switch (event->keyval)
    {
    case GDK_space:
      gtk_option_menu_remove_contents (option_menu);
      gtk_menu_popup (GTK_MENU (option_menu->menu), NULL, NULL,
		      gtk_option_menu_position, option_menu,
		      0, event->time);
      return TRUE;
    }
  
  return FALSE;
}

static void
gtk_option_menu_deactivate (GtkMenuShell  *menu_shell,
			    GtkOptionMenu *option_menu)
{
  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  gtk_option_menu_update_contents (option_menu);
}

static void
gtk_option_menu_select_first_sensitive (GtkOptionMenu *option_menu)
{
  if (option_menu->menu)
    {
      GList *children = GTK_MENU_SHELL (option_menu->menu)->children;
      gint index = 0;

      while (children)
	{
	  if (GTK_WIDGET_SENSITIVE (children->data))
	    {
	      gtk_option_menu_set_history (option_menu, index);
	      return;
	    }
	  
	  children = children->next;
	  index++;
	}
    }
}

static void
gtk_option_menu_item_state_changed_cb (GtkWidget      *widget,
				       GtkStateType    previous_state,
				       GtkOptionMenu  *option_menu)
{
  GtkWidget *child = GTK_BIN (option_menu)->child;

  if (child && GTK_WIDGET_SENSITIVE (child) != GTK_WIDGET_IS_SENSITIVE (widget))
    gtk_widget_set_sensitive (child, GTK_WIDGET_IS_SENSITIVE (widget));
}

static void
gtk_option_menu_item_destroy_cb (GtkWidget     *widget,
				 GtkOptionMenu *option_menu)
{
  GtkWidget *child = GTK_BIN (option_menu)->child;

  if (child)
    {
      gtk_widget_ref (child);
      gtk_option_menu_remove_contents (option_menu);
      gtk_widget_destroy (child);
      gtk_widget_unref (child);

      gtk_option_menu_select_first_sensitive (option_menu);
    }
}

static void
gtk_option_menu_update_contents (GtkOptionMenu *option_menu)
{
  GtkWidget *child;
  GtkRequisition child_requisition;

  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu)
    {
      gtk_option_menu_remove_contents (option_menu);

      option_menu->menu_item = gtk_menu_get_active (GTK_MENU (option_menu->menu));
      if (option_menu->menu_item)
	{
	  gtk_widget_ref (option_menu->menu_item);
	  child = GTK_BIN (option_menu->menu_item)->child;
	  if (child)
	    {
	      if (!GTK_WIDGET_IS_SENSITIVE (option_menu->menu_item))
		gtk_widget_set_sensitive (child, FALSE);
	      gtk_widget_reparent (child, GTK_WIDGET (option_menu));
	    }

	  gtk_signal_connect (GTK_OBJECT (option_menu->menu_item), "state_changed",
			      GTK_SIGNAL_FUNC (gtk_option_menu_item_state_changed_cb), option_menu);
	  gtk_signal_connect (GTK_OBJECT (option_menu->menu_item), "destroy",
			      GTK_SIGNAL_FUNC (gtk_option_menu_item_destroy_cb), option_menu);

	  gtk_widget_size_request (child, &child_requisition);
	  gtk_widget_size_allocate (GTK_WIDGET (option_menu),
				    &(GTK_WIDGET (option_menu)->allocation));

	  if (GTK_WIDGET_DRAWABLE (option_menu))
	    gtk_widget_queue_draw (GTK_WIDGET (option_menu));
	}
    }
}

static void
gtk_option_menu_remove_contents (GtkOptionMenu *option_menu)
{
  GtkWidget *child;
  
  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu_item)
    {
      child = GTK_BIN (option_menu)->child;
  
      if (child)
	{
	  gtk_widget_set_sensitive (child, TRUE);
	  gtk_widget_reparent (child, option_menu->menu_item);
	}

      gtk_signal_disconnect_by_func (GTK_OBJECT (option_menu->menu_item),
				     GTK_SIGNAL_FUNC (gtk_option_menu_item_state_changed_cb),
				     option_menu);				     
      gtk_signal_disconnect_by_func (GTK_OBJECT (option_menu->menu_item),
				     GTK_SIGNAL_FUNC (gtk_option_menu_item_destroy_cb),
				     option_menu);   
      
      gtk_widget_unref (option_menu->menu_item);
      option_menu->menu_item = NULL;
    }
}

static void
gtk_option_menu_calc_size (GtkOptionMenu *option_menu)
{
  GtkWidget *child;
  GList *children;
  GtkRequisition child_requisition;

  g_return_if_fail (option_menu != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  option_menu->width = 0;
  option_menu->height = 0;

  if (option_menu->menu)
    {
      children = GTK_MENU_SHELL (option_menu->menu)->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      gtk_widget_size_request (child, &child_requisition);

	      option_menu->width = MAX (option_menu->width, child_requisition.width);
	      option_menu->height = MAX (option_menu->height, child_requisition.height);
	    }
	}
    }
}

static void
gtk_option_menu_position (GtkMenu  *menu,
			  gint     *x,
			  gint     *y,
			  gpointer  user_data)
{
  GtkOptionMenu *option_menu;
  GtkWidget *active;
  GtkWidget *child;
  GtkRequisition requisition;
  GList *children;
  gint shift_menu;
  gint screen_width;
  gint screen_height;
  gint menu_xpos;
  gint menu_ypos;
  gint width;
  gint height;

  g_return_if_fail (user_data != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (user_data));

  option_menu = GTK_OPTION_MENU (user_data);

  gtk_widget_get_child_requisition (GTK_WIDGET (menu), &requisition);
  width = requisition.width;
  height = requisition.height;

  active = gtk_menu_get_active (GTK_MENU (option_menu->menu));
  children = GTK_MENU_SHELL (option_menu->menu)->children;
  gdk_window_get_origin (GTK_WIDGET (option_menu)->window, &menu_xpos, &menu_ypos);

  menu_ypos += GTK_WIDGET (option_menu)->allocation.height / 2 - 2;

  if (active != NULL)
    {
      gtk_widget_get_child_requisition (active, &requisition);
      menu_ypos -= requisition.height / 2;
    }

  while (children)
    {
      child = children->data;

      if (active == child)
	break;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_get_child_requisition (child, &requisition);
	  menu_ypos -= requisition.height;
	}

      children = children->next;
    }

  screen_width = gdk_screen_width ();
  screen_height = gdk_screen_height ();

  shift_menu = FALSE;
  if (menu_ypos < 0)
    {
      menu_ypos = 0;
      shift_menu = TRUE;
    }
  else if ((menu_ypos + height) > screen_height)
    {
      menu_ypos -= ((menu_ypos + height) - screen_height);
      shift_menu = TRUE;
    }

  if (shift_menu)
    {
      if ((menu_xpos + GTK_WIDGET (option_menu)->allocation.width + width) <= screen_width)
	menu_xpos += GTK_WIDGET (option_menu)->allocation.width;
      else
	menu_xpos -= width;
    }

  if (menu_xpos < 0)
    menu_xpos = 0;
  else if ((menu_xpos + width) > screen_width)
    menu_xpos -= ((menu_xpos + width) - screen_width);

  *x = menu_xpos;
  *y = menu_ypos;
}


static void
gtk_option_menu_show_all (GtkWidget *widget)
{
  GtkContainer *container;
  GtkOptionMenu *option_menu;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  container = GTK_CONTAINER (widget);
  option_menu = GTK_OPTION_MENU (widget);

  gtk_widget_show (widget);
  gtk_container_foreach (container, (GtkCallback) gtk_widget_show_all, NULL);
  if (option_menu->menu)
    gtk_widget_show_all (option_menu->menu);
  if (option_menu->menu_item)
    gtk_widget_show_all (option_menu->menu_item);
}


static void
gtk_option_menu_hide_all (GtkWidget *widget)
{
  GtkContainer *container;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  container = GTK_CONTAINER (widget);

  gtk_widget_hide (widget);
  gtk_container_foreach (container, (GtkCallback) gtk_widget_hide_all, NULL);
}

