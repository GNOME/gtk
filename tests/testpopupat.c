#include <gtk/gtk.h>

static void
destroy_cb (GtkWidget  *window,
            GtkBuilder *builder)
{
  gtk_main_quit ();
}

static void
populate_popup_cb (GtkAppChooserWidget *app_chooser_widget,
                   GtkMenu             *menu,
                   GAppInfo            *app_info,
                   gpointer             user_data)
{
  GtkWidget *menu_item;

  menu_item = gtk_menu_item_new_with_label ("Menu Item A");
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_label ("Menu Item B");
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_label ("Menu Item C");
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_label ("Menu Item D");
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_label ("Menu Item E");
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
}

int
main (int   argc,
      char *argv[])
{
  GtkBuilder *builder;
  GtkWidget *window;
  GtkWidget *app_chooser_widget;

  gtk_init (&argc, &argv);

  builder = gtk_builder_new_from_file ("popupat.ui");

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  g_signal_connect (window, "destroy", G_CALLBACK (destroy_cb), builder);

  app_chooser_widget = GTK_WIDGET (gtk_builder_get_object (builder, "appchooserwidget"));
  g_signal_connect (app_chooser_widget, "populate-popup", G_CALLBACK (populate_popup_cb), builder);

  gtk_widget_show_all (window);

  gtk_main ();

  g_object_unref (builder);

  return 0;
}
