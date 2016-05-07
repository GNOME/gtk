/* Revealer
 *
 * GtkRevealer is a container that animates showing and hiding
 * of its sole child with nice transitions.
 */

#include <gtk/gtk.h>

static gint count = 0;
static guint timeout = 0;

static void
change_direction (GtkRevealer *revealer)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (revealer)))
    {
      gboolean revealed;

      revealed = gtk_revealer_get_child_revealed (revealer);
      gtk_revealer_set_reveal_child (revealer, !revealed);
    }
}

static gboolean
reveal_one (gpointer data)
{
  GtkWidget *window = data;
  GtkBuilder *builder;
  gchar *name;
  GtkRevealer *revealer;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (window), "builder"));
  name = g_strdup_printf ("revealer%d", count);
  revealer = (GtkRevealer *)gtk_builder_get_object (builder, name);

  gtk_revealer_set_reveal_child (revealer, TRUE);

  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (change_direction), NULL);
  count++;

  if (count >= 9)
    {
      timeout = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static GtkWidget *window = NULL;

static void
on_destroy (gpointer data)
{
  window = NULL;
  if (timeout != 0)
    {
      g_source_remove (timeout);
      timeout = 0;
    }
}

GtkWidget *
do_revealer (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/revealer/revealer.ui");
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (on_destroy), NULL);
      g_object_set_data_full (G_OBJECT (window), "builder", builder, g_object_unref);
    }

  if (!gtk_widget_get_visible (window))
    {
      count = 0;
      timeout = g_timeout_add (690, reveal_one, window);
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
    }


  return window;
}
