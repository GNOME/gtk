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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
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
  gint n_items;
  gint position;
  gchar *label = g_strdup_printf ("Button %d", n);

  GtkToolItem *toolitem = gtk_tool_button_new (NULL, label);
  gtk_tool_item_set_tooltip_text (toolitem, "Bar");

  g_free (label);
  gtk_widget_show_all (GTK_WIDGET (toolitem));

  n_items = gtk_toolbar_get_n_items (toolbar);
  if (n_items == 0)
    position = 0;
  else
    position = g_random_int_range (0, n_items);

  gtk_toolbar_insert (toolbar, toolitem, position);
}

static void
remove_random (GtkToolbar *toolbar)
{
  GtkToolItem *tool_item;
  gint n_items;
  gint position;

  n_items = gtk_toolbar_get_n_items (toolbar);

  if (n_items == 0)
    return;

  position = g_random_int_range (0, n_items);

  tool_item = gtk_toolbar_get_nth_item (toolbar, position);

  gtk_container_remove (GTK_CONTAINER (toolbar),
                        GTK_WIDGET (tool_item));
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
  gint n_items;

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

  n_items = gtk_toolbar_get_n_items (info->toolbar);
  if (n_items == 0)
    {
      add_random (info->toolbar, info->counter);
      return TRUE;
    }
  else if (n_items > 50)
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
