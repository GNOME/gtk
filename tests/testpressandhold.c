/* testpressandhold.c: Test application for GTK+ >= 3.2 press-n-hold code
 *
 * Copyright (C) 2007,2008  Imendio AB
 * Contact: Kristian Rietveld <kris@imendio.com>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gtk/gtk.h>

struct CoordData
{
  gint x;
  gint y;
  GtkWidget *widget;
};

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer   user_data)
{
  GtkRequisition req;
  GdkScreen *screen;
  struct CoordData *data = user_data;

  screen = gtk_widget_get_screen (data->widget);
  gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);

  *x = data->x;
  *y = data->y;

  *x = CLAMP (*x, 0, MAX (0, gdk_screen_get_width (screen) - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_get_height (screen) - req.height));
}

static void
press_and_hold_show_menu (GtkWidget *widget,
			  gint       x,
			  gint       y)
{
  GtkWidget *menu;
  GtkWidget *item;
  struct CoordData data;

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_label ("Test 1");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Test 2");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label ("Test 3");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  data.widget = widget;
  gdk_window_get_origin (gtk_widget_get_window (widget), &data.x, &data.y);
  data.x += x;
  data.y += y;

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
		  popup_position_func,
		  &data,
		  1,
		  GDK_CURRENT_TIME);
}

static gboolean
press_and_hold (GtkWidget             *widget,
	        GtkPressAndHoldAction  action,
	        gint                   x,
	        gint                   y,
	        gboolean               keyboard)
{
  switch (action)
    {
      case GTK_PRESS_AND_HOLD_QUERY:
	g_print ("press-and-hold-query on %s\n", gtk_widget_get_name (widget));
        return TRUE;

      case GTK_PRESS_AND_HOLD_TRIGGER:
	g_print ("press-and-hold-trigger on %s\n", gtk_widget_get_name (widget));
        press_and_hold_show_menu (widget, x, y);
        break;

      case GTK_PRESS_AND_HOLD_CANCEL:
	g_print ("press-and-hold-cancel on %s\n", gtk_widget_get_name (widget));
        break;
    }

  return FALSE;
}

static GtkTreeModel *
create_model (void)
{
  GtkTreeStore *store;
  GtkTreeIter iter;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  /* A tree store with some random words ... */
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "File Manager", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "Gossip", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "System Settings", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "The GIMP", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "Terminal", -1);
  gtk_tree_store_insert_with_values (store, &iter, NULL, 0,
				     0, "Word Processor", -1);

  return GTK_TREE_MODEL (store);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *label, *checkbutton, *tree_view, *entry;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Press and Hold test");
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  g_signal_connect (window, "delete_event",
		    G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (window), box);

  label = gtk_button_new_with_label ("Press-n-hold me!");
  g_signal_connect (label, "press-and-hold",
		    G_CALLBACK (press_and_hold), NULL);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  label = gtk_button_new_with_label ("No press and hold");
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  checkbutton = gtk_check_button_new_with_label ("Checkable check button");
  g_signal_connect (checkbutton, "press-and-hold",
		    G_CALLBACK (press_and_hold), NULL);
  gtk_box_pack_start (GTK_BOX (box), checkbutton, FALSE, FALSE, 0);


  tree_view = gtk_tree_view_new_with_model (create_model ());
  gtk_widget_set_size_request (tree_view, 200, 240);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       0, "Test",
					       gtk_cell_renderer_text_new (),
					       "text", 0,
					       NULL);

  g_signal_connect (tree_view, "press-and-hold",
		    G_CALLBACK (press_and_hold), NULL);

  gtk_box_pack_start (GTK_BOX (box), tree_view, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "Press and hold me");
  g_signal_connect (entry, "press-and-hold",
                    G_CALLBACK (press_and_hold), NULL);
  gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
