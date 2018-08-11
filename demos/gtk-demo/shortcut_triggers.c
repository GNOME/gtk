/* Shortcuts
 *
 * GtkShortcut is the abstraction used by GTK to handle shortcuts from
 * keyboard or other input devices.
 *
 * Shortcut triggers can be used to weave complex sequences of key
 * presses into sophisticated mechanisms to activate shortcuts.
 *
 * This demo code shows creative ways to do that.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static gboolean
shortcut_activated (GtkWidget *window,
                    GVariant  *unused,
                    gpointer   row)
{
  g_print ("activated %s\n", gtk_label_get_label (row));
  return TRUE;
}

static GtkShortcutTrigger *
create_ctrl_g (void)
{
  return gtk_keyval_trigger_new (GDK_KEY_g, GDK_CONTROL_MASK);
}

static GtkShortcutTrigger *
create_x (void)
{
  return gtk_keyval_trigger_new (GDK_KEY_x, 0);
}

struct {
  char *description;
  GtkShortcutTrigger * (* create_trigger_func) (void);
} shortcuts[] = {
  { "Press Ctrl-G", create_ctrl_g },
  { "Press X", create_x },
};

GtkWidget *
do_shortcut_triggers (GtkWidget *do_widget)
{
  guint i;

  if (!window)
    {
      GtkWidget *list;
      GtkEventController *controller;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Shortcuts");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      list = gtk_list_box_new ();
      g_object_set (list, "margin", 6, NULL);
      gtk_container_add (GTK_CONTAINER (window), list);

      for (i = 0; i < G_N_ELEMENTS (shortcuts); i++)
        {
          GtkShortcut *shortcut;
          GtkWidget *row;

          row = gtk_label_new (shortcuts[i].description);
          gtk_container_add (GTK_CONTAINER (list), row);

          controller = gtk_shortcut_controller_new ();
          gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller), GTK_SHORTCUT_SCOPE_GLOBAL);
          gtk_widget_add_controller (row, controller);

          shortcut = gtk_shortcut_new ();
          gtk_shortcut_set_trigger (shortcut, shortcuts[i].create_trigger_func());
          gtk_shortcut_set_callback (shortcut, shortcut_activated, row, NULL);
          gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
        }
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
