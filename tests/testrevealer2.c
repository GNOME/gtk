/* Written by Florian Muellner
 * https://bugzilla.gnome.org/show_bug.cgi?id=761760
 */

#include <gtk/gtk.h>

static void
on_activate (GApplication *app,
             gpointer      data)
{
  static GtkWidget *window = NULL;

  if (window == NULL)
    {
      GtkWidget *header, *sidebar_toggle, *animation_switch;
      GtkWidget *hbox, *revealer, *sidebar, *img;

      window = gtk_application_window_new (GTK_APPLICATION (app));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

      /* titlebar */
      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      sidebar_toggle = gtk_toggle_button_new_with_label ("Show Sidebar");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), sidebar_toggle);

      animation_switch = gtk_switch_new ();
      gtk_widget_set_valign (animation_switch, GTK_ALIGN_CENTER);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), animation_switch);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header),
                               gtk_label_new ("Animations"));

      gtk_widget_show_all (header);

      /* content */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (window), hbox);

      revealer = gtk_revealer_new ();
      gtk_revealer_set_transition_type (GTK_REVEALER (revealer),
                                        GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
      gtk_container_add (GTK_CONTAINER (hbox), revealer);

      sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_size_request (sidebar, 150, -1);
      gtk_style_context_add_class (gtk_widget_get_style_context (sidebar),
                                   GTK_STYLE_CLASS_SIDEBAR);
      gtk_container_add (GTK_CONTAINER (revealer), sidebar);

      img = gtk_image_new ();
      g_object_set (img, "icon-name", "face-smile-symbolic",
                         "pixel-size", 128,
                         "hexpand", TRUE,
                         "halign", GTK_ALIGN_CENTER,
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
      gtk_container_add (GTK_CONTAINER (hbox), img);
      gtk_widget_show_all (hbox);

      g_object_bind_property (sidebar_toggle, "active",
                              revealer, "reveal-child",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (gtk_settings_get_default(), "gtk-enable-animations",
                              animation_switch, "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
                              
    }
  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  GtkApplication *app = gtk_application_new ("org.gtk.fmuellner.Revealer", 0);

  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}

