/* Builder
 *
 * Demonstrates an interface loaded from a XML description.
 */

#include <gtk/gtk.h>

static GtkBuilder *builder;

static void
quit_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *window;

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
  gtk_widget_destroy (window);
}

static void
about_activate (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkWidget *about_dlg;

  about_dlg = GTK_WIDGET (gtk_builder_get_object (builder, "aboutdialog1"));
  gtk_dialog_run (GTK_DIALOG (about_dlg));
  gtk_widget_hide (about_dlg);
}

static void
help_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  g_print ("Help not available\n");
}

static GActionEntry win_entries[] = {
  { "quit", quit_activate, NULL, NULL, NULL },
  { "about", about_activate, NULL, NULL, NULL },
  { "help", help_activate, NULL, NULL, NULL }
};

GtkWidget *
do_builder (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GError *err = NULL;
  GtkWidget *toolbar;
  GActionGroup *actions;
  GtkAccelGroup *accel_group;
  GtkWidget *item;

  if (!window)
    {
      builder = gtk_builder_new ();
      gtk_builder_add_from_resource (builder, "/builder/demo.ui", &err);
      if (err)
        {
          g_error ("ERROR: %s\n", err->message);
          return NULL;
        }
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      toolbar = GTK_WIDGET (gtk_builder_get_object (builder, "toolbar1"));
      gtk_style_context_add_class (gtk_widget_get_style_context (toolbar),
                                   "primary-toolbar");
      actions = (GActionGroup*)g_simple_action_group_new ();
      g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                       win_entries, G_N_ELEMENTS (win_entries),
                                       NULL);
      gtk_widget_insert_action_group (window, "win", actions);
      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

      item = (GtkWidget*)gtk_builder_get_object (builder, "new_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_n, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "open_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "save_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "quit_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "copy_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_c, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "cut_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_x, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "paste_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_v, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "help_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_F1, 0, GTK_ACCEL_VISIBLE);

      item = (GtkWidget*)gtk_builder_get_object (builder, "about_item");
      gtk_widget_add_accelerator (item, "activate", accel_group,
                                  GDK_KEY_F7, 0, GTK_ACCEL_VISIBLE);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }


  return window;
}
