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
#include "gtkmain.h"
#include "gtkmarshalers.h"

enum {
  PATH_CLICKED,
  LAST_SIGNAL
};

static guint path_bar_signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GtkPathBar,
	       gtk_path_bar,
	       GTK_TYPE_CONTAINER);


static void gtk_path_bar_finalize (GObject *object);
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

  gobject_class->finalize = gtk_path_bar_finalize;

  widget_class->size_request = gtk_path_bar_size_request;
  widget_class->size_allocate = gtk_path_bar_size_allocate;
  widget_class->direction_changed = gtk_path_bar_direction_changed;

  container_class->add = gtk_path_bar_add;
  container_class->forall = gtk_path_bar_forall;
  container_class->remove = gtk_path_bar_remove;
  /* FIXME: */
  /*  container_class->child_type = gtk_path_bar_child_type;*/

  path_bar_signals [PATH_CLICKED] =
    g_signal_new ("path_clicked",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkPathBarClass, path_clicked),
		  NULL, NULL,
		  _gtk_marshal_VOID__STRING,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);
}


static void
gtk_path_bar_finalize (GObject *object)
{
  GtkPathBar *path_bar;

  path_bar = GTK_PATH_BAR (object);
  g_list_free (path_bar->button_list);

  G_OBJECT_CLASS (gtk_path_bar_parent_class)->finalize (object);
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

	  if (width + child->requisition.width +
	      path_bar->spacing + slider_space > allocation_width)
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
  GList *down_button = NULL;
  GList *up_button = NULL;
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
  path_bar->first_scrolled_button = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
		   gpointer   data)
{
  GtkWidget *path_bar;

  path_bar = button->parent;
  g_assert (path_bar);

  g_signal_emit (path_bar, path_bar_signals [PATH_CLICKED], 0, (const char *) data);
}

static GtkWidget *
make_directory_button (const char  *dir_name,
		       GtkFilePath *path,
		       gboolean     current_dir)
{
  GtkWidget *button, *label;
      
  button = gtk_toggle_button_new ();
  if (current_dir)
    {
      /* Yay gsignal! */
      g_signal_connect (G_OBJECT (button), "toggled",
			G_CALLBACK (gtk_toggle_button_set_active),
			GINT_TO_POINTER (TRUE));
    }
  else
    {
      gchar *str;

      str = g_strdup (gtk_file_path_get_string (path));
      g_signal_connect (button, "clicked", G_CALLBACK (button_clicked_cb), str);
      g_object_weak_ref (G_OBJECT (button), (GWeakNotify) g_free, str);
    }

  label = gtk_label_new (NULL);

  if (current_dir)
    {
      gchar *label_str;
      
      label_str = g_markup_printf_escaped ("<b>%s</b>", dir_name);
      gtk_label_set_markup (GTK_LABEL (label), label_str);
      g_free (label_str);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (label), dir_name);
    }

  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_widget_show_all (button);

  return button;
}


void
gtk_path_bar_set_path (GtkPathBar         *path_bar,
		       const GtkFilePath  *file_path,
		       GtkFileSystem      *file_system,
		       GError            **error)
{
  gboolean valid = TRUE;
  GtkFilePath *path;
  gboolean first_directory = TRUE;

  gtk_path_bar_clear_buttons (path_bar);
  path = gtk_file_path_copy (file_path);

  while (path != NULL)
    {
      GtkFilePath *parent_path = NULL;
      GtkWidget *button;
      const gchar *display_name;
      GError *err = NULL;

      valid = gtk_file_system_get_parent (file_system,
					  path,
					  &parent_path,
					  &err);
      if (!valid)
	{
	  g_propagate_error (error, err);
	  g_error_free (err);
	  gtk_file_path_free (path);
	  break;
	}

      if (parent_path)
	{
	  GtkFileFolder *file_folder;
	  GtkFileInfo *file_info;

	  file_folder = gtk_file_system_get_folder (file_system, parent_path,
						    GTK_FILE_INFO_DISPLAY_NAME, NULL);
	  file_info = gtk_file_folder_get_info (file_folder, path, NULL);
	  display_name = gtk_file_info_get_display_name (file_info);
	  button = make_directory_button (display_name, path, first_directory);
	  gtk_file_info_free (file_info);
	  /* FIXME: ask owen about mem management. gtk_file_folder_free (file_folder); */
	}
      else
	{
	  /* We've reached the root node */
	  /* FIXME: gtk_file_system_get_root_display_name() or something */
	  button = make_directory_button (gtk_file_path_get_string (path),
					  path, first_directory);
	}

      gtk_container_add (GTK_CONTAINER (path_bar), button);
      path_bar->button_list = g_list_prepend (path_bar->button_list, button);
      gtk_file_path_free (path);

      path = parent_path;
      first_directory = FALSE;
    }

  path_bar->button_list = g_list_reverse (path_bar->button_list);
}
