/* gtkpathbar.h
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
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

#include "gtkpathbar.h"
#include "gtktogglebutton.h"
#include "gtkarrow.h"
#include "gtklabel.h"

G_DEFINE_TYPE (GtkPathBar,
	       gtk_path_bar,
	       GTK_TYPE_CONTAINER);

static void gtk_path_bar_destroy (GtkObject *object);
static void gtk_path_bar_size_request  (GtkWidget      *widget,
					GtkRequisition *requisition);
static void gtk_path_bar_size_allocate (GtkWidget      *widget,
					GtkAllocation  *allocation);
static void gtk_path_bar_direction_changed (GtkWidget *widget,
					    GtkTextDirection direction);
static void gtk_path_bar_add (GtkContainer *container,
			      GtkWidget    *widget);
static void gtk_path_bar_remove (GtkContainer *container,
				 GtkWidget    *widget);
static void gtk_path_bar_forall (GtkContainer *container,
				 gboolean      include_internals,
				 GtkCallback   callback,
				 gpointer      callback_data);
static void gtk_path_bar_scroll_up (GtkWidget *button, GtkPathBar *path_bar);
static void gtk_path_bar_scroll_down (GtkWidget *button, GtkPathBar *path_bar);

static GtkWidget *
get_slider_button (GtkPathBar *path_bar)
{
  GtkWidget *button;
  
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT));
  gtk_container_add (GTK_CONTAINER (path_bar), button);
  gtk_widget_show_all (button);

  return button;
}

static void
gtk_path_bar_init (GtkPathBar *path_bar)
{
  GTK_WIDGET_SET_FLAGS (path_bar, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (path_bar), FALSE);

  path_bar->spacing = 3;
  path_bar->up_slider_button = get_slider_button (path_bar);
  path_bar->down_slider_button = get_slider_button (path_bar);

  g_signal_connect (path_bar->up_slider_button, "clicked", G_CALLBACK (gtk_path_bar_scroll_up), path_bar);
  g_signal_connect (path_bar->down_slider_button, "clicked", G_CALLBACK (gtk_path_bar_scroll_down), path_bar);
}

static void
gtk_path_bar_class_init (GtkPathBarClass *path_bar_class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass *) path_bar_class;
  object_class = (GtkObjectClass *) path_bar_class;
  widget_class = (GtkWidgetClass *) path_bar_class;
  container_class = (GtkContainerClass *) path_bar_class;

  object_class->destroy = gtk_path_bar_destroy;

  widget_class->size_request = gtk_path_bar_size_request;
  widget_class->size_allocate = gtk_path_bar_size_allocate;
  widget_class->direction_changed = gtk_path_bar_direction_changed;

  container_class->add = gtk_path_bar_add;
  container_class->forall = gtk_path_bar_forall;
  container_class->remove = gtk_path_bar_remove;
  /* FIXME: */
  /*  container_class->child_type = gtk_path_bar_child_type;*/
}


static void
gtk_path_bar_destroy (GtkObject *object)
{
  GtkPathBar *path_bar;

  path_bar = GTK_PATH_BAR (object);

  g_free ((void *) path_bar->path);
}

/* Size requisition:
 * 
 * Ideally, our size is determined by another widget, and we are just filling
 * available space.
 */
static void
gtk_path_bar_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkPathBar *path_bar;
  GtkRequisition child_requisition;
  GList *list;

  path_bar = GTK_PATH_BAR (widget);

  requisition->width = 0;
  requisition->height = 0;

  for (list = path_bar->button_list; list; list = list->next)
    {
      gtk_widget_size_request (GTK_WIDGET (list->data),
			       &child_requisition);
      requisition->width = MAX (child_requisition.width, requisition->width);
      requisition->height = MAX (child_requisition.height, requisition->height);
    }

  /* Add space for slider, if we have more than one path */
  /* Theoretically, the slider could be bigger than the other button.  But we're
   * not going to worry about that now.
   */
  path_bar->slider_width = requisition->height / 2 + 5;
  if (path_bar->button_list && path_bar->button_list->next != NULL)
    requisition->width += (path_bar->spacing + path_bar->slider_width) * 2;

  gtk_widget_size_request (path_bar->up_slider_button, &child_requisition);
  gtk_widget_size_request (path_bar->down_slider_button, &child_requisition);

  requisition->width += GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2;

  widget->requisition = *requisition;
}

static void
gtk_path_bar_update_slider_buttons (GtkPathBar *path_bar)
{
  GtkTextDirection direction;

  direction = gtk_widget_get_direction (GTK_WIDGET (path_bar));
  if (direction == GTK_TEXT_DIR_RTL)
    {
      GtkWidget *arrow;

      arrow = gtk_bin_get_child (GTK_BIN (path_bar->up_slider_button));
      g_object_set (arrow, "arrow_type", GTK_ARROW_RIGHT, NULL);

      arrow = gtk_bin_get_child (GTK_BIN (path_bar->down_slider_button));
      g_object_set (arrow, "arrow_type", GTK_ARROW_LEFT, NULL);
    }
  else
    {
      GtkWidget *arrow;

      arrow = gtk_bin_get_child (GTK_BIN (path_bar->up_slider_button));
      g_object_set (arrow, "arrow_type", GTK_ARROW_LEFT, NULL);

      arrow = gtk_bin_get_child (GTK_BIN (path_bar->down_slider_button));
      g_object_set (arrow, "arrow_type", GTK_ARROW_RIGHT, NULL);
    }

  if (path_bar->button_list)
    {
      GtkWidget *button;

      button = path_bar->button_list->data;
      if (gtk_widget_get_child_visible (button))
	gtk_widget_set_sensitive (path_bar->down_slider_button, FALSE);
      else
	gtk_widget_set_sensitive (path_bar->down_slider_button, TRUE);

      button = g_list_last (path_bar->button_list)->data;
      if (gtk_widget_get_child_visible (button))
	gtk_widget_set_sensitive (path_bar->up_slider_button, FALSE);
      else
	gtk_widget_set_sensitive (path_bar->up_slider_button, TRUE);
    }
}

/* This is a tad complicated
 */
static void
gtk_path_bar_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkWidget *child;
  GtkPathBar *path_bar = GTK_PATH_BAR (widget);
  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GList *list, *first_button;
  gint width;
  gint allocation_width;
  gint border_width;
  gboolean need_sliders = FALSE;
  gint up_slider_offset = 0;
  gint down_slider_offset = 0;

  widget->allocation = *allocation;

  /* No path is set; we don't have to allocate anything. */
  if (path_bar->button_list == NULL)
    return;

  direction = gtk_widget_get_direction (widget);
  border_width = (gint) GTK_CONTAINER (path_bar)->border_width;
  allocation_width = allocation->width - 2 * border_width;

  /* First, we check to see if we need the scrollbars. */
  width = GTK_WIDGET (path_bar->button_list->data)->requisition.width;
  for (list = path_bar->button_list->next; list; list = list->next)
    {
      child = GTK_WIDGET (list->data);

      width += child->requisition.width + path_bar->spacing;
    }

  if (width <= allocation_width)
    {
      first_button = g_list_last (path_bar->button_list);
    }
  else
    {
      gboolean reached_end = FALSE;
      gint slider_space = 2 * (path_bar->spacing + path_bar->slider_width);

      if (path_bar->first_scrolled_button)
	first_button = path_bar->first_scrolled_button;
      else
	first_button = path_bar->button_list;
      need_sliders = TRUE;
      
      /* To see how much space we have, and how many buttons we can display.
       * We start at the first button, count forward until hit the new
       * button, then count backwards.
       */
      /* Count down the path chain towards the end. */
      width = GTK_WIDGET (first_button->data)->requisition.width;
      list = first_button->prev;
      while (list && !reached_end)
	{
	  child = GTK_WIDGET (list->data);

	  if (width + child->requisition.width + path_bar->spacing + slider_space > allocation_width)
	    reached_end = TRUE;
	  else
	    width += child->requisition.width + path_bar->spacing;

	  list = list->prev;
	}

      /* Finally, we walk up, seeing how many of the previous buttons we can
       * add */
      while (first_button->next && ! reached_end)
	{
	  child = GTK_WIDGET (first_button->next->data);

	  if (width + child->requisition.width + path_bar->spacing + slider_space > allocation_width)
	    {
	      reached_end = TRUE;
	    }
	  else
	    {
	      width += child->requisition.width + path_bar->spacing;
	      first_button = first_button->next;
	    }
	}
    }

  /* Now, we allocate space to the buttons */
  child_allocation.y = allocation->y + border_width;
  child_allocation.height = MAX (1, (gint) allocation->height - border_width * 2);

  if (direction == GTK_TEXT_DIR_RTL)
    {
      child_allocation.x = allocation->x + allocation->width - border_width;
      if (need_sliders)
	{
	  child_allocation.x -= (path_bar->spacing + path_bar->slider_width);
	  up_slider_offset = allocation->width - border_width - path_bar->slider_width;
	}
    }
  else
    {
      child_allocation.x = allocation->x + border_width;
      if (need_sliders)
	{
	  up_slider_offset = border_width;
	  child_allocation.x += (path_bar->spacing + path_bar->slider_width);
	}
    }

  for (list = first_button; list; list = list->prev)
    {
      child = GTK_WIDGET (list->data);

      child_allocation.width = child->requisition.width;
      if (direction == GTK_TEXT_DIR_RTL)
	child_allocation.x -= child_allocation.width;

      /* Check to see if we've don't have any more space to allocate buttons */
      if (need_sliders && direction == GTK_TEXT_DIR_RTL)
	{
	  if (child_allocation.x - path_bar->spacing - path_bar->slider_width < widget->allocation.x + border_width)
	    break;
	}
      else if (need_sliders && direction == GTK_TEXT_DIR_LTR)
	{
	  if (child_allocation.x + child_allocation.width + path_bar->spacing + path_bar->slider_width >
	      widget->allocation.x + border_width + allocation_width)
	    break;
	}

      gtk_widget_set_child_visible (list->data, TRUE);
      gtk_widget_size_allocate (child, &child_allocation);

      if (direction == GTK_TEXT_DIR_RTL)
	{
	  child_allocation.x -= path_bar->spacing;
	  down_slider_offset = child_allocation.x - widget->allocation.x - path_bar->slider_width;
	}
      else
	{
	  child_allocation.x += child_allocation.width + path_bar->spacing;
	  down_slider_offset = child_allocation.x - widget->allocation.x;
	}
    }
  /* Now we go hide all the widgets that don't fit */
  while (list)
    {
      gtk_widget_set_child_visible (list->data, FALSE);
      list = list->prev;
    }
  for (list = first_button->next; list; list = list->next)
    {
      gtk_widget_set_child_visible (list->data, FALSE);
    }

  if (need_sliders)
    {
      child_allocation.width = path_bar->slider_width;
      
      child_allocation.x = up_slider_offset + allocation->x;
      gtk_widget_size_allocate (path_bar->up_slider_button, &child_allocation);

      child_allocation.x = down_slider_offset + allocation->x;
      gtk_widget_size_allocate (path_bar->down_slider_button, &child_allocation);

      gtk_widget_set_child_visible (path_bar->up_slider_button, TRUE);
      gtk_widget_set_child_visible (path_bar->down_slider_button, TRUE);
      gtk_widget_show_all (path_bar->up_slider_button);
      gtk_widget_show_all (path_bar->down_slider_button);
      gtk_path_bar_update_slider_buttons (path_bar);
    }
  else
    {
      gtk_widget_set_child_visible (path_bar->up_slider_button, FALSE);
      gtk_widget_set_child_visible (path_bar->down_slider_button, FALSE);
    }
}

static void
 gtk_path_bar_direction_changed (GtkWidget *widget,
				 GtkTextDirection direction)
{
  gtk_path_bar_update_slider_buttons (GTK_PATH_BAR (widget));

  (* GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->direction_changed) (widget, direction);
}

static void
gtk_path_bar_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
gtk_path_bar_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GtkPathBar *path_bar;
  GList *children;

  path_bar = GTK_PATH_BAR (container);

  children = path_bar->button_list;

  while (children)
    {
      if (widget == children->data)
	{
	  gboolean was_visible;

	  was_visible = GTK_WIDGET_VISIBLE (widget);
	  gtk_widget_unparent (widget);

	  path_bar->button_list = g_list_remove_link (path_bar->button_list, children);
	  g_list_free (children);

	  if (was_visible)
	    gtk_widget_queue_resize (GTK_WIDGET (container));
	  break;
	}
      
      children = children->next;
    }
}

static void
gtk_path_bar_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkPathBar *path_bar;
  GList *children;

  g_return_if_fail (callback != NULL);
  path_bar = GTK_PATH_BAR (container);

  children = path_bar->button_list;
  while (children)
    {
      GtkWidget *child;
      child = children->data;
      children = children->next;

      (* callback) (child, callback_data);
    }

  (* callback) (path_bar->up_slider_button, callback_data);
  (* callback) (path_bar->down_slider_button, callback_data);
}

static void
 gtk_path_bar_scroll_down (GtkWidget *button, GtkPathBar *path_bar)
{
  GList *list;
  GList *down_button, *up_button;
  gint space_available;
  gint space_needed;
  gint border_width;
  GtkTextDirection direction;
  
  border_width = GTK_CONTAINER (path_bar)->border_width;
  direction = gtk_widget_get_direction (GTK_WIDGET (path_bar));
  
  /* We find the button at the 'down' end that we have to make
   * visible */
  for (list = path_bar->button_list; list; list = list->next)
    {
      if (list->next && gtk_widget_get_child_visible (GTK_WIDGET (list->next->data)))
	{
	  down_button = list;
	  break;
	}
    }
  
  /* Find the last visible button on the 'up' end
   */
  for (list = g_list_last (path_bar->button_list); list; list = list->prev)
    {
      if (gtk_widget_get_child_visible (GTK_WIDGET (list->data)))
	{
	  up_button = list;
	  break;
	}
    }

  space_needed = GTK_WIDGET (down_button->data)->allocation.width + path_bar->spacing;
  if (direction == GTK_TEXT_DIR_RTL)
    space_available = GTK_WIDGET (path_bar)->allocation.x + GTK_WIDGET (path_bar)->allocation.width;
  else
    space_available = (GTK_WIDGET (path_bar)->allocation.x + GTK_WIDGET (path_bar)->allocation.width - border_width) -
      (path_bar->down_slider_button->allocation.x + path_bar->down_slider_button->allocation.width);

  /* We have space_available extra space that's not being used.  We
   * need space_needed space to make the button fit.  So we walk down
   * from the end, removing buttons until we get all the space we
   * need. */
  while (space_available < space_needed)
    {
      space_available += GTK_WIDGET (up_button->data)->allocation.width + path_bar->spacing;
      up_button = up_button->prev;
      path_bar->first_scrolled_button = up_button;
    }
}

static void
 gtk_path_bar_scroll_up (GtkWidget *button, GtkPathBar *path_bar)
{
  GList *list;

  for (list = g_list_last (path_bar->button_list); list; list = list->prev)
    {
      if (list->prev && gtk_widget_get_child_visible (GTK_WIDGET (list->prev->data)))
	{
	  path_bar->first_scrolled_button = list;
	  return;
	}
    }
}



/* Public functions. */
static void
gtk_path_bar_clear_buttons (GtkPathBar *path_bar)
{
  while (path_bar->button_list != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (path_bar), path_bar->button_list->data);
    }
}

/* FIXME: Do this right.  Quick hack to get something on the screen.  Get in
 * sync with nautilus-spatial-window.c:location_button_clicked_callback */

static GList *
gtk_path_bar_get_description_list (const gchar *path)
{
  GList *description_list = NULL;
  gchar** str_list;
  gint i = 0;

  str_list = g_strsplit (path, "/", -1);

  while (str_list[i])
    {
      if (str_list[i][0] == '\000')
	g_free (str_list[i]);
      else
	description_list = g_list_prepend (description_list, str_list[i]);

      i++;
    }
  g_free (str_list);

  description_list = g_list_append (description_list, " / ");

  return description_list;
}

/* FIXME: non UTF-8 strings */
void
gtk_path_bar_set_path (GtkPathBar  *path_bar,
		       const gchar *path)
{
  GList *button_list = NULL;
  GList *description_list;
  GList *list;

  g_return_if_fail (GTK_IS_PATH_BAR (path_bar));
  g_return_if_fail (path != NULL);

  if (path_bar->path)
    g_free ((void *) path_bar->path);
  gtk_path_bar_clear_buttons (path_bar);

  /* New path */
  path_bar->path = g_strdup (path);
  description_list = gtk_path_bar_get_description_list (path);
  for (list = description_list; list; list = list->next)
    {
      GtkWidget *button, *label;
      gchar *label_str;

      button = gtk_toggle_button_new ();

      if (list->prev == NULL)
	{
	  label_str = g_strdup_printf ("<span color=\"#000000\"><b>%s</b></span>", (char *)list->data);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
      else
	{
	  label_str = g_strdup_printf ("<span color=\"#000000\">%s</span>",(char *) list->data);
	}

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), label_str);
      gtk_container_add (GTK_CONTAINER (button), label);
      button_list = g_list_prepend (button_list, button);
      gtk_container_add (GTK_CONTAINER (path_bar), button);
      gtk_widget_show_all (button);
    }

  path_bar->button_list = g_list_reverse (button_list);
  gtk_widget_queue_resize (GTK_WIDGET (path_bar));
}


