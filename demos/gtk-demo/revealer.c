/* Revealer
 *
 * GtkRevealer is a container that animates showing and hiding
 * of its sole child with nice transitions.
 */

#include <gtk/gtk.h>

static GtkBuilder *builder;
static gint count = 0;

static void
change_direction (GtkRevealer *revealer)
{
  gboolean revealed;

  revealed = gtk_revealer_get_child_revealed (revealer);
  gtk_revealer_set_reveal_child (revealer, !revealed);
}

static gboolean
reveal_one (gpointer data)
{
  gchar *name;
  GtkRevealer *revealer;

  name = g_strdup_printf ("revealer%d", count);
  revealer = (GtkRevealer *)gtk_builder_get_object (builder, name);

  gtk_revealer_set_reveal_child (revealer, TRUE);

  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (change_direction), NULL);
  count++;

  return count < 9;
}

static guint timeout = 0;

static void
response_cb (GtkWidget *dialog,
             gint       response_id,
             gpointer   data)
{
  if (timeout != 0)
    {
      g_source_remove (timeout);
      timeout = 0;
    }
  gtk_widget_destroy (dialog);
}

GtkWidget *
do_revealer (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GError *err = NULL;

  if (!window)
    {
      builder = gtk_builder_new ();
      gtk_builder_add_from_resource (builder, "/revealer/revealer.ui", &err);
      if (err)
        {
          g_error ("ERROR: %s\n", err->message);
          return NULL;
        }
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog1"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      g_signal_connect (window, "response", G_CALLBACK (response_cb), NULL);
    }

  if (!gtk_widget_get_visible (window))
    {
      count = 0;
      timeout = g_timeout_add (690, reveal_one, NULL);
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }


  return window;
}
