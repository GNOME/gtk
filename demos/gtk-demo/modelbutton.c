/* Model Button
 *
 * GtkModelButton is a button widget that is designed to be used with
 * a GAction as model. The button will adjust its appearance according
 * to the kind of action it is connected to.
 *
 * It is also possible to use GtkModelButton without a GAction. In this
 * case, you should set the "role" attribute yourself, and connect to the
 * "clicked" signal as you would for any other button.
 *
 * A common use of GtkModelButton is to implement menu-like content
 * in popovers.
 */

#include <gtk/gtk.h>

static void
tool_clicked (GtkButton *button)
{
  gboolean active;

  g_object_get (button, "active", &active, NULL);
  g_object_set (button, "active", !active, NULL);
}

GtkWidget *
do_modelbutton (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GActionEntry win_entries[] = {
    { "color", NULL, "s", "'red'", NULL },
    { "chocolate", NULL, NULL, "true", NULL },
    { "vanilla", NULL, NULL, "false", NULL },
    { "sprinkles", NULL, NULL, NULL, NULL }
  };

  if (!window)
    {
      GtkBuilder *builder;
      GActionGroup *actions;

      builder = gtk_builder_new_from_resource ("/modelbutton/modelbutton.ui");
      gtk_builder_add_callback_symbol (builder, "tool_clicked", G_CALLBACK (tool_clicked));
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      actions = (GActionGroup*)g_simple_action_group_new ();
      g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                       win_entries, G_N_ELEMENTS (win_entries),
                                       window);
      gtk_widget_insert_action_group (window, "win", actions);


      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);


  return window;
}

