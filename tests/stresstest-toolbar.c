/* stresstest-toolbar.c
 *
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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
#undef GTK_DISABLE_DEPRECATED
#include "config.h"
#include <gtk/gtk.h>

typedef struct _Info Info;
struct _Info
{
  GtkWindow  *window;
  GtkToolbar *toolbar;
  gint	      counter;
};

static void
add_random (GtkToolbar *toolbar, gint n)
{
  gint position;
  gchar *label = g_strdup_printf ("Button %d", n);

  GtkWidget *widget = gtk_button_new_with_label (label);

  g_free (label);
  gtk_widget_show_all (widget);

  if (g_list_length (toolbar->children) == 0)
    position = 0;
  else
    position = g_random_int_range (0, g_list_length (toolbar->children));

  gtk_toolbar_insert_widget (toolbar, widget, "Bar", "Baz", position);
}

static void
remove_random (GtkToolbar *toolbar)
{
  GtkToolbarChild *child;
  gint position;

  if (!toolbar->children)
    return;

  position = g_random_int_range (0, g_list_length (toolbar->children));

  child = g_list_nth_data (toolbar->children, position);
  
  gtk_container_remove (GTK_CONTAINER (toolbar), child->widget);
}

static gboolean
stress_test_old_api (gpointer data)
{
  typedef enum {
    ADD_RANDOM,
    REMOVE_RANDOM,
    LAST_ACTION
  } Action;
      
  Info *info = data;
  Action action;
  
  if (info->counter++ == 200)
    {
      gtk_main_quit ();
      return FALSE;
    }

  if (!info->toolbar)
    {
      info->toolbar = GTK_TOOLBAR (gtk_toolbar_new ());
      gtk_container_add (GTK_CONTAINER (info->window),
			 GTK_WIDGET (info->toolbar));
      gtk_widget_show (GTK_WIDGET (info->toolbar));
    }

  if (!info->toolbar->children)
    {
      add_random (info->toolbar, info->counter);
      return TRUE;
    }
  else if (g_list_length (info->toolbar->children) > 50)
    {
      int i;
      for (i = 0; i < 25; i++)
	remove_random (info->toolbar);
      return TRUE;
    }
  
  action = g_random_int_range (0, LAST_ACTION);

  switch (action)
    {
    case ADD_RANDOM:
      add_random (info->toolbar, info->counter);
      break;

    case REMOVE_RANDOM:
      remove_random (info->toolbar);
      break;
      
    default:
      g_assert_not_reached();
      break;
    }
  
  return TRUE;
}


gint
main (gint argc, gchar **argv)
{
  Info info;
  
  gtk_init (&argc, &argv);

  info.toolbar = NULL;
  info.counter = 0;
  info.window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));

  gtk_widget_show (GTK_WIDGET (info.window));
  
  gdk_threads_add_idle (stress_test_old_api, &info);

  gtk_widget_show_all (GTK_WIDGET (info.window));
  
  gtk_main ();

  gtk_widget_destroy (GTK_WIDGET (info.window));

  info.toolbar = NULL;
  info.window = NULL;
  
  return 0;
}
