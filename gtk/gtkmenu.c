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
#include <ctype.h>
#include "gdk/gdkkeysyms.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtksignal.h"


#define MENU_ITEM_CLASS(w)  GTK_MENU_ITEM_CLASS (GTK_OBJECT (w)->klass)


typedef struct _GtkMenuAttachData	GtkMenuAttachData;

struct _GtkMenuAttachData
{
  GtkWidget *attach_widget;
  GtkMenuDetachFunc detacher;
};


static void gtk_menu_class_init     (GtkMenuClass      *klass);
static void gtk_menu_init           (GtkMenu           *menu);
static void gtk_menu_destroy        (GtkObject         *object);
static void gtk_menu_show           (GtkWidget         *widget);
static void gtk_menu_map            (GtkWidget         *widget);
static void gtk_menu_unmap          (GtkWidget         *widget);
static void gtk_menu_realize        (GtkWidget         *widget);
static void gtk_menu_size_request   (GtkWidget         *widget,
				     GtkRequisition    *requisition);
static void gtk_menu_size_allocate  (GtkWidget         *widget,
				     GtkAllocation     *allocation);
static void gtk_menu_paint          (GtkWidget         *widget);
static void gtk_menu_draw           (GtkWidget         *widget,
				     GdkRectangle      *area);
static gint gtk_menu_expose         (GtkWidget         *widget,
				     GdkEventExpose    *event);
static gint gtk_menu_configure      (GtkWidget         *widget,
				     GdkEventConfigure *event);
static gint gtk_menu_key_press      (GtkWidget         *widget,
				     GdkEventKey       *event);
static gint gtk_menu_need_resize    (GtkContainer      *container);
static void gtk_menu_deactivate     (GtkMenuShell      *menu_shell);
static void gtk_menu_show_all       (GtkWidget         *widget);
static void gtk_menu_hide_all       (GtkWidget         *widget);

static GtkMenuShellClass *parent_class = NULL;
static const gchar	*attach_data_key = "gtk-menu-attach-data";


guint
gtk_menu_get_type ()
{
  static guint menu_type = 0;

  if (!menu_type)
    {
      GtkTypeInfo menu_info =
      {
	"GtkMenu",
	sizeof (GtkMenu),
	sizeof (GtkMenuClass),
	(GtkClassInitFunc) gtk_menu_class_init,
	(GtkObjectInitFunc) gtk_menu_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      menu_type = gtk_type_unique (gtk_menu_shell_get_type (), &menu_info);
    }

  return menu_type;
}

static void
gtk_menu_class_init (GtkMenuClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkMenuShellClass *menu_shell_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  menu_shell_class = (GtkMenuShellClass*) class;
  parent_class = gtk_type_class (gtk_menu_shell_get_type ());

  object_class->destroy = gtk_menu_destroy;

  widget_class->show = gtk_menu_show;
  widget_class->map = gtk_menu_map;
  widget_class->unmap = gtk_menu_unmap;
  widget_class->realize = gtk_menu_realize;
  widget_class->draw = gtk_menu_draw;
  widget_class->size_request = gtk_menu_size_request;
  widget_class->size_allocate = gtk_menu_size_allocate;
  widget_class->expose_event = gtk_menu_expose;
  widget_class->configure_event = gtk_menu_configure;
  widget_class->key_press_event = gtk_menu_key_press;
  widget_class->show_all = gtk_menu_show_all;
  widget_class->hide_all = gtk_menu_hide_all;  
  
  container_class->need_resize = gtk_menu_need_resize;

  menu_shell_class->submenu_placement = GTK_LEFT_RIGHT;
  menu_shell_class->deactivate = gtk_menu_deactivate;
}

static void
gtk_menu_init (GtkMenu *menu)
{
  GTK_WIDGET_SET_FLAGS (menu, GTK_TOPLEVEL);

  menu->parent_menu_item = NULL;
  menu->old_active_menu_item = NULL;
  menu->accelerator_table = NULL;
  menu->position_func = NULL;
  menu->position_func_data = NULL;
  
  GTK_MENU_SHELL (menu)->menu_flag = TRUE;
  
  /* we don't need to register as toplevel anymore,
   * since there is the attach/detach functionality in place.
   * gtk_container_register_toplevel (GTK_CONTAINER (menu));
   */
}

static void
gtk_menu_destroy (GtkObject         *object)
{
  GtkMenuAttachData *data;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_MENU (object));

  gtk_widget_ref (GTK_WIDGET (object));

  data = gtk_object_get_data (object, attach_data_key);
  if (data)
    gtk_menu_detach (GTK_MENU (object));

  /* we don't need this:
   * gtk_container_unregister_toplevel (GTK_CONTAINER (object));
   */

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);

  gtk_widget_unref (GTK_WIDGET (object));
}


void
gtk_menu_attach_to_widget (GtkMenu             *menu,
			   GtkWidget           *attach_widget,
			   GtkMenuDetachFunc    detacher)
{
  GtkMenuAttachData *data;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (attach_widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (attach_widget));
  g_return_if_fail (detacher != NULL);

  /* keep this function in sync with gtk_widget_set_parent()
   */

  data = gtk_object_get_data (GTK_OBJECT (menu), attach_data_key);
  if (data)
    {
      g_warning ("gtk_menu_attach_to_widget(): menu already attached to %s",
		 gtk_type_name (GTK_OBJECT_TYPE (data->attach_widget)));
      return;
    }

  gtk_widget_ref (menu);
  gtk_object_sink (GTK_OBJECT (menu));

  data = g_new (GtkMenuAttachData, 1);
  data->attach_widget = attach_widget;
  data->detacher = detacher;
  gtk_object_set_data (GTK_OBJECT (menu), attach_data_key, data);

  if (GTK_WIDGET_STATE (menu) != GTK_STATE_NORMAL)
    gtk_widget_set_state (GTK_WIDGET (menu), GTK_STATE_NORMAL);

  /* we don't need to set the style here, since
   * we are a toplevel widget.
   */
}

GtkWidget*
gtk_menu_get_attach_widget (GtkMenu             *menu)
{
  GtkMenuAttachData *data;

  g_return_val_if_fail (menu != NULL, NULL);
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  data = gtk_object_get_data (GTK_OBJECT (menu), attach_data_key);
  if (data)
    return data->attach_widget;
  return NULL;
}

void
gtk_menu_detach (GtkMenu             *menu)
{
  GtkMenuAttachData *data;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  /* keep this function in sync with gtk_widget_unparent()
   */
  data = gtk_object_get_data (GTK_OBJECT (menu), attach_data_key);
  if (!data)
    {
      g_warning ("gtk_menu_detach(): menu is not attached");
      return;
    }
  gtk_object_remove_data (GTK_OBJECT (menu), attach_data_key);

  data->detacher (data->attach_widget, menu);

  if (GTK_WIDGET_REALIZED (menu))
    gtk_widget_unrealize (GTK_WIDGET (menu));

  g_free (data);

  gtk_widget_unref (GTK_WIDGET (menu));
}

GtkWidget*
gtk_menu_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_menu_get_type ()));
}

void
gtk_menu_append (GtkMenu   *menu,
		 GtkWidget *child)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), child);
}

void
gtk_menu_prepend (GtkMenu   *menu,
		  GtkWidget *child)
{
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), child);
}

void
gtk_menu_insert (GtkMenu   *menu,
		 GtkWidget *child,
		 gint       position)
{
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), child, position);
}

void
gtk_menu_popup (GtkMenu             *menu,
		GtkWidget           *parent_menu_shell,
		GtkWidget           *parent_menu_item,
		GtkMenuPositionFunc  func,
		gpointer             data,
		gint                 button,
		guint32              activate_time)
{
  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  GTK_MENU_SHELL (menu)->parent_menu_shell = parent_menu_shell;
  GTK_MENU_SHELL (menu)->active = TRUE;
  GTK_MENU_SHELL (menu)->button = button;

  menu->parent_menu_item = parent_menu_item;
  menu->position_func = func;
  menu->position_func_data = data;
  GTK_MENU_SHELL (menu)->activate_time = activate_time;

  gtk_widget_show (GTK_WIDGET (menu));
  gtk_grab_add (GTK_WIDGET (menu));
}

void
gtk_menu_popdown (GtkMenu *menu)
{
  GtkMenuShell *menu_shell;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  menu_shell = GTK_MENU_SHELL (menu);

  menu_shell->parent_menu_shell = NULL;
  menu_shell->active = FALSE;

  if (menu_shell->active_menu_item)
    {
      menu->old_active_menu_item = menu_shell->active_menu_item;
      gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }

  gtk_widget_hide (GTK_WIDGET (menu));
  gtk_grab_remove (GTK_WIDGET (menu));
}

GtkWidget*
gtk_menu_get_active (GtkMenu *menu)
{
  GtkWidget *child;
  GList *children;

  g_return_val_if_fail (menu != NULL, NULL);
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  if (!menu->old_active_menu_item)
    {
      child = NULL;
      children = GTK_MENU_SHELL (menu)->children;

      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_BIN (child)->child)
	    break;
	  child = NULL;
	}

      menu->old_active_menu_item = child;
    }

  return menu->old_active_menu_item;
}

void
gtk_menu_set_active (GtkMenu *menu,
		     gint     index)
{
  GtkWidget *child;
  GList *tmp_list;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  tmp_list = g_list_nth (GTK_MENU_SHELL (menu)->children, index);
  if (tmp_list)
    {
      child = tmp_list->data;
      if (GTK_BIN (child)->child)
	menu->old_active_menu_item = child;
    }
}

void
gtk_menu_set_accelerator_table (GtkMenu             *menu,
				GtkAcceleratorTable *table)
{
  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  if (menu->accelerator_table != table)
    {
      if (menu->accelerator_table)
	gtk_accelerator_table_unref (menu->accelerator_table);
      menu->accelerator_table = table;
      if (menu->accelerator_table)
	gtk_accelerator_table_ref (menu->accelerator_table);
    }
}


static void
gtk_menu_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_map (widget);
}

static void
gtk_menu_map (GtkWidget *widget)
{
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  GtkAllocation allocation;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  GTK_WIDGET_SET_FLAGS (menu_shell, GTK_MAPPED);

  gtk_widget_size_request (widget, &widget->requisition);

  if (menu_shell->menu_flag)
    {
      menu_shell->menu_flag = FALSE;

      allocation.x = widget->allocation.x;
      allocation.y = widget->allocation.y;
      allocation.width = widget->requisition.width;
      allocation.height = widget->requisition.height;

      gtk_widget_size_allocate (widget, &allocation);
    }

  gdk_window_get_pointer (NULL, &x, &y, NULL);

  if (menu->position_func)
    (* menu->position_func) (menu, &x, &y, menu->position_func_data);
  else
    {
      gint screen_width;
      gint screen_height;

      screen_width = gdk_screen_width ();
      screen_height = gdk_screen_height ();

      x -= 2;
      y -= 2;

      if ((x + widget->requisition.width) > screen_width)
	x -= ((x + widget->requisition.width) - screen_width);
      if (x < 0)
	x = 0;
      if ((y + widget->requisition.height) > screen_height)
	y -= ((y + widget->requisition.height) - screen_height);
      if (y < 0)
	y = 0;
    }

  gdk_window_move_resize (widget->window, x, y,
			  widget->requisition.width,
			  widget->requisition.height);

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child) && !GTK_WIDGET_MAPPED (child))
        gtk_widget_map (child);
    }

  gdk_window_show (widget->window);
}

static void
gtk_menu_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_hide (widget->window);
}

static void
gtk_menu_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_STRUCTURE_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (NULL, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_menu_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  gint max_accelerator_size;
  gint max_toggle_size;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (requisition != NULL);

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  requisition->width = 0;
  requisition->height = 0;

  max_accelerator_size = 0;
  max_toggle_size = 0;

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  GTK_MENU_ITEM (child)->show_submenu_indicator = TRUE;
	  gtk_widget_size_request (child, &child->requisition);

	  requisition->width = MAX (requisition->width, child->requisition.width);
	  requisition->height += child->requisition.height;

	  max_accelerator_size = MAX (max_accelerator_size, GTK_MENU_ITEM (child)->accelerator_size);
	  max_toggle_size = MAX (max_toggle_size, MENU_ITEM_CLASS (child)->toggle_size);
	}
    }

  requisition->width += max_toggle_size + max_accelerator_size;
  requisition->width += (GTK_CONTAINER (menu)->border_width +
			 widget->style->klass->xthickness) * 2;
  requisition->height += (GTK_CONTAINER (menu)->border_width +
			  widget->style->klass->ythickness) * 2;

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      GTK_MENU_ITEM (child)->accelerator_size = max_accelerator_size;
      GTK_MENU_ITEM (child)->toggle_size = max_toggle_size;
    }
}

static void
gtk_menu_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GtkAllocation child_allocation;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (allocation != NULL);

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  widget->allocation = *allocation;

  if (menu_shell->children)
    {
      child_allocation.x = (GTK_CONTAINER (menu)->border_width +
			    widget->style->klass->xthickness);
      child_allocation.y = (GTK_CONTAINER (menu)->border_width +
			    widget->style->klass->ythickness);
      child_allocation.width = allocation->width - child_allocation.x * 2;

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      child_allocation.height = child->requisition.height;

	      gtk_widget_size_allocate (child, &child_allocation);

	      child_allocation.y += child_allocation.height;
	    }
	}
    }
}

static void
gtk_menu_paint (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_draw_shadow (widget->style,
		       widget->window,
		       GTK_STATE_NORMAL,
		       GTK_SHADOW_OUT,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
    }
}

static void
gtk_menu_draw (GtkWidget    *widget,
	       GdkRectangle *area)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GdkRectangle child_area;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_paint (widget);

      menu_shell = GTK_MENU_SHELL (widget);

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_intersect (child, area, &child_area))
	    gtk_widget_draw (child, &child_area);
	}
    }
}

static gint
gtk_menu_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GdkEventExpose child_event;
  GList *children;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_paint (widget);

      menu_shell = GTK_MENU_SHELL (widget);
      child_event = *event;

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_NO_WINDOW (child) &&
	      gtk_widget_intersect (child, &event->area, &child_event.area))
	    gtk_widget_event (child, (GdkEvent*) &child_event);
	}
    }

  return FALSE;
}

static gint
gtk_menu_configure (GtkWidget         *widget,
		    GdkEventConfigure *event)
{
  GtkAllocation allocation;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_MENU_SHELL (widget)->menu_flag)
    {
      GTK_MENU_SHELL (widget)->menu_flag = FALSE;

      allocation.x = 0;
      allocation.y = 0;
      allocation.width = event->width;
      allocation.height = event->height;

      gtk_widget_size_allocate (widget, &allocation);
    }

  return FALSE;
}

static gint
gtk_menu_key_press (GtkWidget   *widget,
		    GdkEventKey *event)
{
  GtkAllocation allocation;
  GtkAcceleratorTable *table;
  GtkMenuShell *menu_shell;
  GtkMenuItem *menu_item;
  gchar *signame;
  int delete;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  delete = ((event->keyval == GDK_Delete) ||
	    (event->keyval == GDK_BackSpace));

  if (delete || ((event->keyval >= 0x20) && (event->keyval <= 0xFF)))
    {
      menu_shell = GTK_MENU_SHELL (widget);
      menu_item = GTK_MENU_ITEM (menu_shell->active_menu_item);

      if (menu_item && GTK_BIN (menu_item)->child)
	{
	  /* block resizes */
	  gtk_container_block_resize (GTK_CONTAINER (widget));

	  table = NULL;
	  /* if the menu item currently has an accelerator then we'll
	   *  remove it before we do anything else.
	   */
	  if (menu_item->accelerator_signal)
	    {
	      signame = gtk_signal_name (menu_item->accelerator_signal);
	      table = gtk_accelerator_table_find (GTK_OBJECT (widget),
						  signame,
						  menu_item->accelerator_key,
						  menu_item->accelerator_mods);
	      if (!table)
		table = GTK_MENU (widget)->accelerator_table;
	      gtk_widget_remove_accelerator (GTK_WIDGET (menu_item),
					     table, signame);
	    }

	  if (!table)
	    table = GTK_MENU (widget)->accelerator_table;

	  /* if we aren't simply deleting the accelerator, then we'll install
	   *  the new one now.
	   */
	  if (!delete)
	    gtk_widget_install_accelerator (GTK_WIDGET (menu_item),
					    table, "activate",
					    toupper (event->keyval),
					    event->state);

	  /* check and see if the menu has changed size. */
	  gtk_widget_size_request (widget, &widget->requisition);

	  allocation.x = widget->allocation.x;
	  allocation.y = widget->allocation.y;
	  allocation.width = widget->requisition.width;
	  allocation.height = widget->requisition.height;

	  if ((allocation.width == widget->allocation.width) &&
	      (allocation.height == widget->allocation.height))
	    {
	      gtk_widget_queue_draw (widget);
	    }
	  else
	    {
	      gtk_widget_size_allocate (GTK_WIDGET (widget), &allocation);
	      gtk_menu_map (widget);
	    }

	  /* unblock resizes */
	  gtk_container_unblock_resize (GTK_CONTAINER (widget));
	}
    }

  return FALSE;
}

static gint
gtk_menu_need_resize (GtkContainer *container)
{
  GtkAllocation allocation;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (container), FALSE);

  if (GTK_WIDGET_VISIBLE (container))
    {
      GTK_MENU_SHELL (container)->menu_flag = FALSE;

      gtk_widget_size_request (GTK_WIDGET (container),
			       &GTK_WIDGET (container)->requisition);

      allocation.x = GTK_WIDGET (container)->allocation.x;
      allocation.y = GTK_WIDGET (container)->allocation.y;
      allocation.width = GTK_WIDGET (container)->requisition.width;
      allocation.height = GTK_WIDGET (container)->requisition.height;

      gtk_widget_size_allocate (GTK_WIDGET (container), &allocation);
    }
  else
    {
      GTK_MENU_SHELL (container)->menu_flag = TRUE;
    }

  return FALSE;
}

static void
gtk_menu_deactivate (GtkMenuShell *menu_shell)
{
  GtkMenuShell *parent;

  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU (menu_shell));

  parent = GTK_MENU_SHELL (menu_shell->parent_menu_shell);

  menu_shell->activate_time = 0;
  gtk_menu_popdown (GTK_MENU (menu_shell));

  if (parent)
    gtk_menu_shell_deactivate (parent);
}


static void
gtk_menu_show_all (GtkWidget *widget)
{
  GtkContainer *container;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));
  container = GTK_CONTAINER (widget);

  /* Show children, but not self. */
  gtk_container_foreach (container, (GtkCallback) gtk_widget_show_all, NULL);
}


static void
gtk_menu_hide_all (GtkWidget *widget)
{
  GtkContainer *container;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU (widget));
  container = GTK_CONTAINER (widget);

  /* Hide children, but not self. */
  gtk_container_foreach (container, (GtkCallback) gtk_widget_hide_all, NULL);
}

