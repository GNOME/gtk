/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* gtkpathbar.c
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkpathbar.h"

#include <string.h>

#include "gtkbox.h"
#include "gtkdragsource.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkdragsource.h"

struct _GtkPathBar
{
  GtkWidget parent_instance;

  GFile *root_file;
  GFile *home_file;
  GFile *desktop_file;

  /* List of running GCancellable.  When we cancel one, we remove it from this list.
   * The pathbar cancels all outstanding cancellables when it is disposed.
   *
   * In code that queues async I/O operations:
   *
   *   - Obtain a cancellable from the async I/O APIs, and call add_cancellable().
   *
   * To cancel a cancellable:
   *
   *   - Call cancel_cancellable().
   *
   * In async I/O callbacks:
   *
   *   - Check right away if g_cancellable_is_cancelled():  if true, just
   *     g_object_unref() the cancellable and return early (also free your
   *     closure data if you have one).
   *
   *   - If it was not cancelled, call cancellable_async_done().  This will
   *     unref the cancellable and unqueue it from the pathbar's outstanding
   *     cancellables.  Do your normal work to process the async result and free
   *     your closure data if you have one.
   */
  GList *cancellables;

  GCancellable *get_info_cancellable;

  GIcon *root_icon;
  GIcon *home_icon;
  GIcon *desktop_icon;

  GList *button_list;
  GList *first_scrolled_button;
  GList *fake_root;
  GtkWidget *up_slider_button;
  GtkWidget *down_slider_button;
  gint16 slider_width;
};

typedef struct _GtkPathBarClass GtkPathBarClass;

struct _GtkPathBarClass
{
  GtkWidgetClass parent_class;

  void (* path_clicked) (GtkPathBar  *path_bar,
                         GFile       *file,
                         GFile       *child_file,
                         gboolean     child_is_hidden);
};

enum {
  PATH_CLICKED,
  LAST_SIGNAL
};

typedef enum {
  NORMAL_BUTTON,
  ROOT_BUTTON,
  HOME_BUTTON,
  DESKTOP_BUTTON
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *)(x))

static guint path_bar_signals [LAST_SIGNAL] = { 0 };

typedef struct _ButtonData ButtonData;

struct _ButtonData
{
  GtkWidget *button;
  ButtonType type;
  char *dir_name;
  GFile *file;
  GtkWidget *image;
  GtkWidget *label;
  GCancellable *cancellable;
  guint ignore_changes : 1;
  guint file_is_hidden : 1;
};
/* This macro is used to check if a button can be used as a fake root.
 * All buttons in front of a fake root are automatically hidden when in a
 * directory below a fake root and replaced with the "<" arrow button.
 */
#define BUTTON_IS_FAKE_ROOT(button) ((button)->type == HOME_BUTTON)

G_DEFINE_TYPE (GtkPathBar, gtk_path_bar, GTK_TYPE_WIDGET)

static void gtk_path_bar_finalize                 (GObject          *object);
static void gtk_path_bar_dispose                  (GObject          *object);
static void gtk_path_bar_measure (GtkWidget *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);
static void gtk_path_bar_size_allocate            (GtkWidget           *widget,
                                                   int                  width,
                                                   int                  height,
                                                   int                  baseline);
static void gtk_path_bar_scroll_up                (GtkPathBar       *path_bar);
static void gtk_path_bar_scroll_down              (GtkPathBar       *path_bar);
static void gtk_path_bar_update_button_appearance (GtkPathBar       *path_bar,
						   ButtonData       *button_data,
						   gboolean          current_dir);

static gboolean gtk_path_bar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                       double                    dx,
                                                       double                    dy,
                                                       GtkPathBar               *path_bar);

static void
add_cancellable (GtkPathBar   *path_bar,
                 GCancellable *cancellable)
{
  g_assert (g_list_find (path_bar->cancellables, cancellable) == NULL);
  path_bar->cancellables = g_list_prepend (path_bar->cancellables, cancellable);
}

static void
drop_node_for_cancellable (GtkPathBar *path_bar,
                           GCancellable *cancellable)
{
  GList *node;

  node = g_list_find (path_bar->cancellables, cancellable);
  g_assert (node != NULL);
  node->data = NULL;
  path_bar->cancellables = g_list_delete_link (path_bar->cancellables, node);
}

static void
cancel_cancellable (GtkPathBar   *path_bar,
                    GCancellable *cancellable)
{
  drop_node_for_cancellable (path_bar, cancellable);
  g_cancellable_cancel (cancellable);
}

static void
cancellable_async_done (GtkPathBar   *path_bar,
                        GCancellable *cancellable)
{
  drop_node_for_cancellable (path_bar, cancellable);
  g_object_unref (cancellable);
}

static void
cancel_all_cancellables (GtkPathBar *path_bar)
{
  while (path_bar->cancellables)
    {
      GCancellable *cancellable = path_bar->cancellables->data;
      cancel_cancellable (path_bar, cancellable);
    }
}

static void
gtk_path_bar_init (GtkPathBar *path_bar)
{
  GtkEventController *controller;
  const char *home;

  path_bar->up_slider_button = gtk_button_new_from_icon_name ("pan-start-symbolic");
  gtk_widget_add_css_class (path_bar->up_slider_button, "slider-button");
  gtk_widget_set_parent (path_bar->up_slider_button, GTK_WIDGET (path_bar));

  path_bar->down_slider_button = gtk_button_new_from_icon_name ("pan-end-symbolic");
  gtk_widget_add_css_class (path_bar->down_slider_button, "slider-button");
  gtk_widget_set_parent (path_bar->down_slider_button, GTK_WIDGET (path_bar));

  /* GtkBuilder won't let us connect 'swapped' without specifying the signal's
   * user data in the .ui file
   */
  g_signal_connect_swapped (path_bar->up_slider_button, "clicked",
			    G_CALLBACK (gtk_path_bar_scroll_up), path_bar);
  g_signal_connect_swapped (path_bar->down_slider_button, "clicked",
			    G_CALLBACK (gtk_path_bar_scroll_down), path_bar);

  gtk_widget_add_css_class (GTK_WIDGET (path_bar), "linked");

  path_bar->get_info_cancellable = NULL;
  path_bar->cancellables = NULL;

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                                GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_path_bar_scroll_controller_scroll),
                    path_bar);
  gtk_widget_add_controller (GTK_WIDGET (path_bar), controller);

  home = g_get_home_dir ();
  if (home != NULL)
    {
      const char *desktop;

      path_bar->home_file = g_file_new_for_path (home);
      /* FIXME: Need file system backend specific way of getting the
       * Desktop path.
       */
      desktop = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
      if (desktop != NULL)
        path_bar->desktop_file = g_file_new_for_path (desktop);
      else 
        path_bar->desktop_file = NULL;
    }
  else
    {
      path_bar->home_file = NULL;
      path_bar->desktop_file = NULL;
    }
  path_bar->root_file = g_file_new_for_path ("/");
}

static void
gtk_path_bar_class_init (GtkPathBarClass *path_bar_class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass *) path_bar_class;
  widget_class = (GtkWidgetClass *) path_bar_class;

  gobject_class->finalize = gtk_path_bar_finalize;
  gobject_class->dispose = gtk_path_bar_dispose;

  widget_class->measure = gtk_path_bar_measure;
  widget_class->size_allocate = gtk_path_bar_size_allocate;

  path_bar_signals [PATH_CLICKED] =
    g_signal_new (I_("path-clicked"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkPathBarClass, path_clicked),
		  NULL, NULL,
		  _gtk_marshal_VOID__POINTER_POINTER_BOOLEAN,
		  G_TYPE_NONE, 3,
		  G_TYPE_POINTER,
		  G_TYPE_POINTER,
		  G_TYPE_BOOLEAN);

  gtk_widget_class_set_css_name (widget_class, "pathbar");
}

static void
gtk_path_bar_finalize (GObject *object)
{
  GtkPathBar *path_bar = GTK_PATH_BAR (object);

  cancel_all_cancellables (path_bar);

  g_list_free (path_bar->button_list);
  g_clear_object (&path_bar->root_file);
  g_clear_object (&path_bar->home_file);
  g_clear_object (&path_bar->desktop_file);

  g_clear_object (&path_bar->root_icon);
  g_clear_object (&path_bar->home_icon);
  g_clear_object (&path_bar->desktop_icon);

  G_OBJECT_CLASS (gtk_path_bar_parent_class)->finalize (object);
}

static void
gtk_path_bar_dispose (GObject *object)
{
  GtkPathBar *path_bar = GTK_PATH_BAR (object);
  GtkWidget *w;

  while ((w = gtk_widget_get_first_child (GTK_WIDGET (path_bar))) != NULL)
    gtk_widget_unparent (w);

  path_bar->get_info_cancellable = NULL;
  cancel_all_cancellables (path_bar);

  G_OBJECT_CLASS (gtk_path_bar_parent_class)->dispose (object);
}

static void
gtk_path_bar_measure (GtkWidget *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkPathBar *path_bar = GTK_PATH_BAR (widget);
  ButtonData *button_data;
  GList *list;
  int child_size;
  int size = 0;
  int child_min, child_nat;

  *minimum = 0;
  *natural = 0;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      for (list = path_bar->button_list; list; list = list->next)
        {
          button_data = BUTTON_DATA (list->data);
          gtk_widget_measure (button_data->button, GTK_ORIENTATION_HORIZONTAL, -1,
                              &child_min, &child_nat, NULL, NULL);
          gtk_widget_measure (button_data->button, GTK_ORIENTATION_VERTICAL, -1,
                              &child_size, NULL, NULL, NULL);
          size = MAX (size, child_size);

          if (button_data->type == NORMAL_BUTTON)
            {
              /* Use 2*Height as button width because of ellipsized label.  */
              child_min = MAX (child_min, child_size * 2);
              child_nat = MAX (child_min, child_size * 2);
            }

          *minimum = MAX (*minimum, child_min);
          *natural = *natural + child_nat;
        }

      /* Add space for slider, if we have more than one path */
      /* Theoretically, the slider could be bigger than the other button.  But we're
       * not going to worry about that now.
       */
      path_bar->slider_width = 0;

      gtk_widget_measure (path_bar->up_slider_button, GTK_ORIENTATION_HORIZONTAL, -1,
                          &child_min, &child_nat, NULL, NULL);
      if (path_bar->button_list && path_bar->button_list->next != NULL)
        {
          *minimum += child_min;
          *natural += child_nat;
        }
      path_bar->slider_width = MAX (path_bar->slider_width, child_min);

      gtk_widget_measure (path_bar->down_slider_button, GTK_ORIENTATION_HORIZONTAL, -1,
                          &child_min, &child_nat, NULL, NULL);
      if (path_bar->button_list && path_bar->button_list->next != NULL)
        {
          *minimum += child_min;
          *natural += child_nat;
        }
      path_bar->slider_width = MAX (path_bar->slider_width, child_min);

    }
  else /* VERTICAL */
    {
      for (list = path_bar->button_list; list; list = list->next)
        {
          button_data = BUTTON_DATA (list->data);
          gtk_widget_measure (button_data->button, GTK_ORIENTATION_VERTICAL, -1,
                              &child_min, &child_nat, NULL, NULL);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
        }

      gtk_widget_measure (path_bar->up_slider_button, GTK_ORIENTATION_VERTICAL, -1,
                          &child_min, &child_nat, NULL, NULL);
      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);

      gtk_widget_measure (path_bar->up_slider_button, GTK_ORIENTATION_VERTICAL, -1,
                          &child_min, &child_nat, NULL, NULL);
      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_path_bar_update_slider_buttons (GtkPathBar *path_bar)
{
  if (path_bar->button_list)
    {
      GtkWidget *button;

      button = BUTTON_DATA (path_bar->button_list->data)->button;
      if (gtk_widget_get_child_visible (button))
	gtk_widget_set_sensitive (path_bar->down_slider_button, FALSE);
      else
	gtk_widget_set_sensitive (path_bar->down_slider_button, TRUE);

      button = BUTTON_DATA (g_list_last (path_bar->button_list)->data)->button;
      if (gtk_widget_get_child_visible (button))
	gtk_widget_set_sensitive (path_bar->up_slider_button, FALSE);
      else
	gtk_widget_set_sensitive (path_bar->up_slider_button, TRUE);
    }
}

/* This is a tad complicated
 */
static void
gtk_path_bar_size_allocate (GtkWidget *widget,
                            int        widget_width,
                            int        widget_height,
                            int        baseline)
{
  GtkPathBar *path_bar = GTK_PATH_BAR (widget);
  GtkWidget *child;
  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GList *list, *first_button;
  int width;
  int allocation_width;
  gboolean need_sliders = TRUE;
  int up_slider_offset = 0;
  int down_slider_offset = 0;
  GtkRequisition child_requisition;

  /* No path is set; we don't have to allocate anything. */
  if (path_bar->button_list == NULL)
    return;

  direction = gtk_widget_get_direction (widget);
  allocation_width = widget_width;

  /* First, we check to see if we need the scrollbars. */
  if (path_bar->fake_root)
    width = path_bar->slider_width;
  else
    width = 0;

  for (list = path_bar->button_list; list; list = list->next)
    {
      child = BUTTON_DATA (list->data)->button;

      gtk_widget_get_preferred_size (child, &child_requisition, NULL);

      width += child_requisition.width;
      if (list == path_bar->fake_root)
	break;
    }

  if (width <= allocation_width)
    {
      if (path_bar->fake_root)
	first_button = path_bar->fake_root;
      else
	first_button = g_list_last (path_bar->button_list);
    }
  else
    {
      gboolean reached_end = FALSE;
      int slider_space = 2 * path_bar->slider_width;

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
      gtk_widget_get_preferred_size (BUTTON_DATA (first_button->data)->button,
                                     &child_requisition, NULL);

      width = child_requisition.width;
      list = first_button->prev;
      while (list && !reached_end)
	{
	  child = BUTTON_DATA (list->data)->button;

          gtk_widget_get_preferred_size (child, &child_requisition, NULL);

	  if (width + child_requisition.width + slider_space > allocation_width)
	    reached_end = TRUE;
	  else if (list == path_bar->fake_root)
	    break;
	  else
	    width += child_requisition.width;

	  list = list->prev;
	}

      /* Finally, we walk up, seeing how many of the previous buttons we can
       * add */
      while (first_button->next && !reached_end)
	{
	  child = BUTTON_DATA (first_button->next->data)->button;

          gtk_widget_get_preferred_size (child, &child_requisition, NULL);

	  if (width + child_requisition.width + slider_space > allocation_width)
	    {
	      reached_end = TRUE;
	    }
	  else
	    {
	      width += child_requisition.width;
	      if (first_button == path_bar->fake_root)
		break;
	      first_button = first_button->next;
	    }
	}
    }

  /* Now, we allocate space to the buttons */
  child_allocation.y = 0;
  child_allocation.height = widget_height;

  if (direction == GTK_TEXT_DIR_RTL)
    {
      child_allocation.x = widget_width;
      if (need_sliders || path_bar->fake_root)
	{
	  child_allocation.x -= path_bar->slider_width;
	  up_slider_offset = widget_width - path_bar->slider_width;
	}
    }
  else
    {
      child_allocation.x = 0;
      if (need_sliders || path_bar->fake_root)
	{
	  up_slider_offset = 0;
	  child_allocation.x += path_bar->slider_width;
	}
    }

  for (list = first_button; list; list = list->prev)
    {
      GtkAllocation widget_allocation;
      ButtonData *button_data;

      button_data = BUTTON_DATA (list->data);
      child = button_data->button;

      gtk_widget_get_preferred_size (child, &child_requisition, NULL);

      child_allocation.width = MIN (child_requisition.width,
				    allocation_width - 2 * path_bar->slider_width);

      if (direction == GTK_TEXT_DIR_RTL)
	child_allocation.x -= child_allocation.width;

      /* Check to see if we've don't have any more space to allocate buttons */
      if (need_sliders && direction == GTK_TEXT_DIR_RTL)
	{
          gtk_widget_get_allocation (widget, &widget_allocation);
	  if (child_allocation.x - path_bar->slider_width < widget_allocation.x)
	    break;
	}
      else if (need_sliders && direction == GTK_TEXT_DIR_LTR)
	{
          gtk_widget_get_allocation (widget, &widget_allocation);
	  if (child_allocation.x + child_allocation.width + path_bar->slider_width >
	      widget_allocation.x + allocation_width)
	    break;
	}

      if (child_allocation.width < child_requisition.width)
	{
	  if (!gtk_widget_get_has_tooltip (child))
	    gtk_widget_set_tooltip_text (child, button_data->dir_name);
	}
      else if (gtk_widget_get_has_tooltip (child))
	gtk_widget_set_tooltip_text (child, NULL);
      
      gtk_widget_set_child_visible (child, TRUE);
      gtk_widget_size_allocate (child, &child_allocation, baseline);

      if (direction == GTK_TEXT_DIR_RTL)
        {
          down_slider_offset = child_allocation.x - path_bar->slider_width;
        }
      else
        {
          down_slider_offset += child_allocation.width;
          child_allocation.x += child_allocation.width;
        }
    }
  /* Now we go hide all the widgets that don't fit */
  while (list)
    {
      child = BUTTON_DATA (list->data)->button;
      gtk_widget_set_child_visible (child, FALSE);
      list = list->prev;
    }
  for (list = first_button->next; list; list = list->next)
    {
      child = BUTTON_DATA (list->data)->button;
      gtk_widget_set_child_visible (child, FALSE);
    }

  if (need_sliders || path_bar->fake_root)
    {
      child_allocation.width = path_bar->slider_width;
      child_allocation.x = up_slider_offset;
      gtk_widget_size_allocate (path_bar->up_slider_button,
                                &child_allocation,
                                -1);

      gtk_widget_set_child_visible (path_bar->up_slider_button, TRUE);
      gtk_widget_show (path_bar->up_slider_button);

      if (direction == GTK_TEXT_DIR_LTR)
        down_slider_offset += path_bar->slider_width;
    }
  else
    {
      gtk_widget_set_child_visible (path_bar->up_slider_button, FALSE);
    }

  if (need_sliders)
    {
      child_allocation.width = path_bar->slider_width;
      child_allocation.x = down_slider_offset;
      
      gtk_widget_size_allocate (path_bar->down_slider_button,
                                &child_allocation,
                                -1);

      gtk_widget_set_child_visible (path_bar->down_slider_button, TRUE);
      gtk_widget_show (path_bar->down_slider_button);
      gtk_path_bar_update_slider_buttons (path_bar);
    }
  else
    {
      gtk_widget_set_child_visible (path_bar->down_slider_button, FALSE);
    }
}

static gboolean
gtk_path_bar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                       double                    dx,
                                       double                    dy,
                                       GtkPathBar               *path_bar)
{
  if (dy > 0)
    gtk_path_bar_scroll_down (path_bar);
  else if (dy < 0)
    gtk_path_bar_scroll_up (path_bar);

  return GDK_EVENT_STOP;
}

static void
gtk_path_bar_scroll_down (GtkPathBar *path_bar)
{
  GtkAllocation allocation, button_allocation;
  GList *list;
  GList *down_button = NULL;
  int space_available;

  if (gtk_widget_get_child_visible (BUTTON_DATA (path_bar->button_list->data)->button))
    {
      /* Return if the last button is already visible */
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (path_bar));

  /* We find the button at the 'down' end that we have to make
   * visible */
  for (list = path_bar->button_list; list; list = list->next)
    {
      if (list->next && gtk_widget_get_child_visible (BUTTON_DATA (list->next->data)->button))
	{
	  down_button = list;
	  break;
	}
    }
  g_assert (down_button);

  gtk_widget_get_allocation (GTK_WIDGET (path_bar), &allocation);
  gtk_widget_get_allocation (BUTTON_DATA (down_button->data)->button, &button_allocation);

  space_available = (allocation.width
		     - 2 * path_bar->slider_width
                     - button_allocation.width);
  path_bar->first_scrolled_button = down_button;
  
  /* We have space_available free space that's not being used.  
   * So we walk down from the end, adding buttons until we use all free space.
   */
  while (space_available > 0)
    {
      path_bar->first_scrolled_button = down_button;
      down_button = down_button->next;
      if (!down_button)
	break;
      space_available -= button_allocation.width;
    }
}

static void
gtk_path_bar_scroll_up (GtkPathBar *path_bar)
{
  GList *list;

  list = g_list_last (path_bar->button_list);

  if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button))
    {
      /* Return if the first button is already visible */
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (path_bar));

  for ( ; list; list = list->prev)
    {
      if (list->prev && gtk_widget_get_child_visible (BUTTON_DATA (list->prev->data)->button))
	{
	  if (list->prev == path_bar->fake_root)
	    path_bar->fake_root = NULL;
	  path_bar->first_scrolled_button = list;
	  return;
	}
    }
}

static void
gtk_path_bar_clear_buttons (GtkPathBar *path_bar)
{
  GtkWidget *w;

  w = gtk_widget_get_first_child (GTK_WIDGET (path_bar));
  while (w)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (w);

      if (w != path_bar->up_slider_button && w != path_bar->down_slider_button)
        {
          gtk_widget_unparent (w);
        }

      w = next;
    }

  path_bar->first_scrolled_button = NULL;
  path_bar->fake_root = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
		   gpointer   data)
{
  GtkPathBar *path_bar;
  ButtonData *button_data;
  GList *button_list;
  gboolean child_is_hidden;
  GFile *child_file;

  button_data = BUTTON_DATA (data);
  if (button_data->ignore_changes)
    return;

  path_bar = GTK_PATH_BAR (gtk_widget_get_parent (button));

  button_list = g_list_find (path_bar->button_list, button_data);
  g_assert (button_list != NULL);

  g_signal_handlers_block_by_func (button,
				   G_CALLBACK (button_clicked_cb), data);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_handlers_unblock_by_func (button,
				     G_CALLBACK (button_clicked_cb), data);

  if (button_list->prev)
    {
      ButtonData *child_data;

      child_data = BUTTON_DATA (button_list->prev->data);
      child_file = child_data->file;
      child_is_hidden = child_data->file_is_hidden;
    }
  else
    {
      child_file = NULL;
      child_is_hidden = FALSE;
    }

  g_signal_emit (path_bar, path_bar_signals [PATH_CLICKED], 0,
		 button_data->file, child_file, child_is_hidden);
}

struct SetButtonImageData
{
  GtkPathBar *path_bar;
  ButtonData *button_data;
};

static void
set_button_image_get_info_cb (GObject      *source,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GFile *file = G_FILE (source);
  struct SetButtonImageData *data = user_data;
  GFileInfo *info;
  GIcon *icon;

  info = g_file_query_info_finish (file, result, NULL);
  if (!info)
    goto out;

  g_assert (GTK_IS_PATH_BAR (data->path_bar));
  g_assert (G_OBJECT (data->path_bar)->ref_count > 0);

  cancellable_async_done (data->path_bar, data->button_data->cancellable);
  data->button_data->cancellable = NULL;

  icon = g_file_info_get_symbolic_icon (info);
  gtk_image_set_from_gicon (GTK_IMAGE (data->button_data->image), icon);

  switch (data->button_data->type)
    {
      case HOME_BUTTON:
        g_set_object (&data->path_bar->home_icon, icon);
        break;

      case DESKTOP_BUTTON:
        g_set_object (&data->path_bar->desktop_icon, icon);
        break;

      case NORMAL_BUTTON:
      case ROOT_BUTTON:
      default:
        break;
    };

out:
  g_free (data);
}

static void
set_button_image (GtkPathBar *path_bar,
                  ButtonData *button_data)
{
  struct SetButtonImageData *data;
  GMount *mount;

  switch (button_data->type)
    {
    case ROOT_BUTTON:

      if (path_bar->root_icon != NULL)
        {
          gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), path_bar->root_icon);
	  break;
	}

      mount = g_file_find_enclosing_mount (button_data->file, NULL, NULL);

      if (!mount && g_file_is_native (button_data->file))
        path_bar->root_icon = g_themed_icon_new ("drive-harddisk-symbolic");
      else if (mount)
        path_bar->root_icon = g_mount_get_symbolic_icon (mount);
      else
        path_bar->root_icon = NULL;

      g_clear_object (&mount);

      gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), path_bar->root_icon);

      break;

    case HOME_BUTTON:
      if (path_bar->home_icon != NULL)
        {
          gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), path_bar->home_icon);
	  break;
	}

      data = g_new0 (struct SetButtonImageData, 1);
      data->path_bar = path_bar;
      data->button_data = button_data;

      if (button_data->cancellable)
        {
          cancel_cancellable (path_bar, button_data->cancellable);
          g_clear_object (&button_data->cancellable);
        }

      button_data->cancellable = g_cancellable_new ();
      g_file_query_info_async (path_bar->home_file,
                               "standard::symbolic-icon",
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               button_data->cancellable,
                               set_button_image_get_info_cb,
                               data);
      add_cancellable (path_bar, button_data->cancellable);
      break;

    case DESKTOP_BUTTON:
      if (path_bar->desktop_icon != NULL)
        {
          gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), path_bar->desktop_icon);
	  break;
	}

      data = g_new0 (struct SetButtonImageData, 1);
      data->path_bar = path_bar;
      data->button_data = button_data;

      if (button_data->cancellable)
        {
          cancel_cancellable (path_bar, button_data->cancellable);
          g_clear_object (&button_data->cancellable);
        }

      button_data->cancellable = g_cancellable_new ();
      g_file_query_info_async (path_bar->desktop_file,
                               "standard::symbolic-icon",
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               button_data->cancellable,
                               set_button_image_get_info_cb,
                               data);
      add_cancellable (path_bar, button_data->cancellable);
      break;

    case NORMAL_BUTTON:
    default:
      break;
    }
}

static void
button_data_free (ButtonData *button_data)
{
  g_clear_object (&button_data->file);
  g_free (button_data->dir_name);
  g_free (button_data);
}

static const char *
get_dir_name (ButtonData *button_data)
{
  return button_data->dir_name;
}

static void
gtk_path_bar_update_button_appearance (GtkPathBar *path_bar,
				       ButtonData *button_data,
				       gboolean    current_dir)
{
  const char *dir_name = get_dir_name (button_data);

  gtk_widget_remove_css_class (button_data->button, "text-button");
  gtk_widget_remove_css_class (button_data->button, "image-button");

  if (button_data->label != NULL)
    {
      gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);
      if (button_data->image == NULL)
        gtk_widget_add_css_class (button_data->button, "text-button");
    }

  if (button_data->image != NULL)
    {
      set_button_image (path_bar, button_data);
      if (button_data->label == NULL)
        gtk_widget_add_css_class (button_data->button, "image-button");
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)) != current_dir)
    {
      button_data->ignore_changes = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_data->button), current_dir);
      button_data->ignore_changes = FALSE;
    }
}

static ButtonType
find_button_type (GtkPathBar  *path_bar,
		  GFile       *file)
{
  if (path_bar->root_file != NULL &&
      g_file_equal (file, path_bar->root_file))
    return ROOT_BUTTON;
  if (path_bar->home_file != NULL &&
      g_file_equal (file, path_bar->home_file))
    return HOME_BUTTON;
  if (path_bar->desktop_file != NULL &&
      g_file_equal (file, path_bar->desktop_file))
    return DESKTOP_BUTTON;

 return NORMAL_BUTTON;
}

static ButtonData *
make_directory_button (GtkPathBar  *path_bar,
		       const char  *dir_name,
		       GFile       *file,
		       gboolean     current_dir,
		       gboolean     file_is_hidden)
{
  GtkWidget *child = NULL;
  ButtonData *button_data;
  GdkContentProvider *content;
  GtkDragSource *source;

  file_is_hidden = !! file_is_hidden;
  /* Is it a special button? */
  button_data = g_new0 (ButtonData, 1);
  button_data->type = find_button_type (path_bar, file);
  button_data->button = gtk_toggle_button_new ();
  gtk_widget_set_focus_on_click (button_data->button, FALSE);

  switch (button_data->type)
    {
    case ROOT_BUTTON:
      button_data->image = gtk_image_new ();
      child = button_data->image;
      button_data->label = NULL;
      break;
    case HOME_BUTTON:
    case DESKTOP_BUTTON:
      button_data->image = gtk_image_new ();
      button_data->label = gtk_label_new (NULL);
      child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (child), button_data->image);
      gtk_box_append (GTK_BOX (child), button_data->label);
      break;
    case NORMAL_BUTTON:
    default:
      button_data->label = gtk_label_new (NULL);
      child = button_data->label;
      button_data->image = NULL;
    }

  button_data->dir_name = g_strdup (dir_name);
  button_data->file = g_object_ref (file);
  button_data->file_is_hidden = file_is_hidden;

  gtk_button_set_child (GTK_BUTTON (button_data->button), child);

  gtk_path_bar_update_button_appearance (path_bar, button_data, current_dir);

  g_signal_connect (button_data->button, "clicked",
		    G_CALLBACK (button_clicked_cb),
		    button_data);
  g_object_weak_ref (G_OBJECT (button_data->button),
		     (GWeakNotify) button_data_free, button_data);

  source = gtk_drag_source_new ();
  content = gdk_content_provider_new_typed (G_TYPE_FILE, button_data->file);
  gtk_drag_source_set_content (source, content);
  g_object_unref (content);
  gtk_widget_add_controller (button_data->button, GTK_EVENT_CONTROLLER (source));

  return button_data;
}

static gboolean
gtk_path_bar_check_parent_path (GtkPathBar         *path_bar,
				GFile              *file)
{
  GList *list;
  GList *current_path = NULL;
  gboolean need_new_fake_root = FALSE;

  for (list = path_bar->button_list; list; list = list->next)
    {
      ButtonData *button_data;

      button_data = list->data;
      if (g_file_equal (file, button_data->file))
	{
	  current_path = list;
	  break;
	}
      if (list == path_bar->fake_root)
	need_new_fake_root = TRUE;
    }

  if (current_path)
    {
      if (need_new_fake_root)
	{
	  path_bar->fake_root = NULL;
	  for (list = current_path; list; list = list->next)
	    {
	      ButtonData *button_data;

	      button_data = list->data;
	      if (BUTTON_IS_FAKE_ROOT (button_data))
		{
		  path_bar->fake_root = list;
		  break;
		}
	    }
	}

      for (list = path_bar->button_list; list; list = list->next)
	{
	  gtk_path_bar_update_button_appearance (path_bar,
						 BUTTON_DATA (list->data),
						 (list == current_path) ? TRUE : FALSE);
	}

      if (!gtk_widget_get_child_visible (BUTTON_DATA (current_path->data)->button))
	{
	  path_bar->first_scrolled_button = current_path;
	  gtk_widget_queue_resize (GTK_WIDGET (path_bar));
	}

      return TRUE;
    }
  return FALSE;
}


struct SetFileInfo
{
  GFile *file;
  GFile *parent_file;
  GtkPathBar *path_bar;
  GList *new_buttons;
  GList *fake_root;
  GCancellable *cancellable;
  gboolean first_directory;
};

static void
gtk_path_bar_set_file_finish (struct SetFileInfo *info,
                              gboolean            result)
{
  if (result)
    {
      GList *l;

      gtk_path_bar_clear_buttons (info->path_bar);
      info->path_bar->button_list = g_list_reverse (info->new_buttons);
      info->path_bar->fake_root = info->fake_root;

      for (l = info->path_bar->button_list; l; l = l->next)
	{
	  GtkWidget *button = BUTTON_DATA (l->data)->button;

          gtk_widget_insert_after (button, GTK_WIDGET (info->path_bar), info->path_bar->up_slider_button);
	}
    }
  else
    {
      GList *l;

      for (l = info->new_buttons; l; l = l->next)
        {
          ButtonData *button_data = BUTTON_DATA (l->data);

          gtk_widget_unparent (button_data->button);
        }

      g_list_free (info->new_buttons);
    }

  if (info->file)
    g_object_unref (info->file);
  if (info->parent_file)
    g_object_unref (info->parent_file);

  g_free (info);
}

static void
gtk_path_bar_get_info_callback (GObject      *source,
                                GAsyncResult *result,
                                gpointer      data)
{
  GFile *file = G_FILE (source);
  struct SetFileInfo *file_info = data;
  GFileInfo *info;
  ButtonData *button_data;
  const char *display_name;
  gboolean is_hidden;

  info = g_file_query_info_finish (file, result, NULL);
  if (!info)
    {
      gtk_path_bar_set_file_finish (file_info, FALSE);
      return;
    }

  g_assert (GTK_IS_PATH_BAR (file_info->path_bar));
  g_assert (G_OBJECT (file_info->path_bar)->ref_count > 0);

  cancellable_async_done (file_info->path_bar, file_info->cancellable);
  if (file_info->path_bar->get_info_cancellable == file_info->cancellable)
    file_info->path_bar->get_info_cancellable = NULL;
  file_info->cancellable = NULL;

  display_name = g_file_info_get_display_name (info);
  is_hidden = g_file_info_get_is_hidden (info) || g_file_info_get_is_backup (info);

  button_data = make_directory_button (file_info->path_bar, display_name,
                                       file_info->file,
                                       file_info->first_directory, is_hidden);
  g_clear_object (&file_info->file);

  file_info->new_buttons = g_list_prepend (file_info->new_buttons, button_data);

  if (BUTTON_IS_FAKE_ROOT (button_data))
    file_info->fake_root = file_info->new_buttons;

  /* We have assigned the info for the innermost button, i.e. the deepest directory.
   * Now, go on to fetch the info for this directory's parent.
   */

  file_info->file = file_info->parent_file;
  file_info->first_directory = FALSE;

  if (!file_info->file)
    {
      /* No parent?  Okay, we are done. */
      gtk_path_bar_set_file_finish (file_info, TRUE);
      return;
    }

  file_info->parent_file = g_file_get_parent (file_info->file);

  /* Recurse asynchronously */
  file_info->cancellable = g_cancellable_new ();
  file_info->path_bar->get_info_cancellable = file_info->cancellable;
  g_file_query_info_async (file_info->file,
                           "standard::display-name,"
                           "standard::is-hidden,"
                           "standard::is-backup",
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           file_info->cancellable,
                           gtk_path_bar_get_info_callback,
                           file_info);
  add_cancellable (file_info->path_bar, file_info->cancellable);
}

void
_gtk_path_bar_set_file (GtkPathBar *path_bar,
                        GFile      *file,
                        gboolean    keep_trail)
{
  struct SetFileInfo *info;

  g_return_if_fail (GTK_IS_PATH_BAR (path_bar));
  g_return_if_fail (G_IS_FILE (file));

  /* Check whether the new path is already present in the pathbar as buttons.
   * This could be a parent directory or a previous selected subdirectory.
   */
  if (keep_trail && gtk_path_bar_check_parent_path (path_bar, file))
    return;

  info = g_new0 (struct SetFileInfo, 1);
  info->file = g_object_ref (file);
  info->path_bar = path_bar;
  info->first_directory = TRUE;
  info->parent_file = g_file_get_parent (info->file);

  if (path_bar->get_info_cancellable)
    cancel_cancellable (path_bar, path_bar->get_info_cancellable);

  info->cancellable = g_cancellable_new ();
  path_bar->get_info_cancellable = info->cancellable;
  g_file_query_info_async (info->file,
                           "standard::display-name,"
                           "standard::is-hidden,"
                           "standard::is-backup",
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           info->cancellable,
                           gtk_path_bar_get_info_callback,
                           info);
  add_cancellable (path_bar, info->cancellable);
}

/**
 * _gtk_path_bar_up:
 * @path_bar: a #GtkPathBar
 * 
 * If the selected button in the pathbar is not the furthest button “up” (in the
 * root direction), act as if the user clicked on the next button up.
 **/
void
_gtk_path_bar_up (GtkPathBar *path_bar)
{
  GList *l;

  for (l = path_bar->button_list; l; l = l->next)
    {
      GtkWidget *button = BUTTON_DATA (l->data)->button;
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  if (l->next)
	    {
	      GtkWidget *next_button = BUTTON_DATA (l->next->data)->button;
	      button_clicked_cb (next_button, l->next->data);
	    }
	  break;
	}
    }
}

/**
 * _gtk_path_bar_down:
 * @path_bar: a #GtkPathBar
 * 
 * If the selected button in the pathbar is not the furthest button “down” (in the
 * leaf direction), act as if the user clicked on the next button down.
 **/
void
_gtk_path_bar_down (GtkPathBar *path_bar)
{
  GList *l;

  for (l = path_bar->button_list; l; l = l->next)
    {
      GtkWidget *button = BUTTON_DATA (l->data)->button;
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  if (l->prev)
	    {
	      GtkWidget *prev_button = BUTTON_DATA (l->prev->data)->button;
	      button_clicked_cb (prev_button, l->prev->data);
	    }
	  break;
	}
    }
}
