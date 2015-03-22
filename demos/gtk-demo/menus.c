/* Menus
 *
 * There are several widgets involved in displaying menus. The
 * GtkMenuBar widget is a menu bar, which normally appears horizontally
 * at the top of an application, but can also be layed out vertically.
 * The GtkMenu widget is the actual menu that pops up. Both GtkMenuBar
 * and GtkMenu are subclasses of GtkMenuShell; a GtkMenuShell contains
 * menu items (GtkMenuItem). Each menu item contains text and/or images
 * and can be selected by the user.
 *
 * There are several kinds of menu item, including plain GtkMenuItem,
 * GtkCheckMenuItem which can be checked/unchecked, GtkRadioMenuItem
 * which is a check menu item that's in a mutually exclusive group,
 * GtkSeparatorMenuItem which is a separator bar, GtkTearoffMenuItem
 * which allows a GtkMenu to be torn off, and GtkImageMenuItem which
 * can place a GtkImage or other widget next to the menu text.
 *
 * A GtkMenuItem can have a submenu, which is simply a GtkMenu to pop
 * up when the menu item is selected. Typically, all menu items in a menu bar
 * have submenus.
 *
 * GtkUIManager provides a higher-level interface for creating menu bars
 * and menus; while you can construct menus manually, most people don't
 * do that. There's a separate demo for GtkUIManager.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>

static GtkWidget *
create_menu (gint     depth)
{
  GtkWidget *menu;
  GtkRadioMenuItem *last_item;
  char buf[32];
  int i, j;

  if (depth < 1)
    return NULL;

  menu = gtk_menu_new ();
  last_item = NULL;

  for (i = 0, j = 1; i < 5; i++, j++)
    {
      GtkWidget *menu_item;

      sprintf (buf, "item %2d - %d", depth, j);

      menu_item = gtk_radio_menu_item_new_with_label_from_widget (NULL, buf);
      gtk_radio_menu_item_join_group (GTK_RADIO_MENU_ITEM (menu_item), last_item);
      last_item = GTK_RADIO_MENU_ITEM (menu_item);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      gtk_widget_show (menu_item);
      if (i == 3)
        gtk_widget_set_sensitive (menu_item, FALSE);

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), create_menu (depth - 1));
    }

  return menu;
}

static void
change_orientation (GtkWidget *button,
                    GtkWidget *menubar)
{
  GtkWidget *parent;
  GtkOrientation orientation;

  parent = gtk_widget_get_parent (menubar);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (parent));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (parent), 1 - orientation);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    g_object_set (menubar, "pack-direction", GTK_PACK_DIRECTION_TTB, NULL);
  else
    g_object_set (menubar, "pack-direction", GTK_PACK_DIRECTION_LTR, NULL);

}

static GtkWidget *window = NULL;

GtkWidget *
do_menus (GtkWidget *do_widget)
{
  GtkWidget *box;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;

  if (!window)
    {
      GtkWidget *menubar;
      GtkWidget *menu;
      GtkWidget *menuitem;
      GtkAccelGroup *accel_group;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Menus");
      g_signal_connect (window, "destroy",
                        G_CALLBACK(gtk_widget_destroyed), &window);

      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box);
      gtk_widget_show (box);

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (box), box1);
      gtk_widget_show (box1);

      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);

      menu = create_menu (2);

      menuitem = gtk_menu_item_new_with_label ("test\nline2");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_label ("foo");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (3));
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_label ("bar");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (4));
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("Flip");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (change_orientation), menubar);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
                                G_CALLBACK(gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
