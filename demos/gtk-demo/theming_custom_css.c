/* CSS Theming/Custom CSS :: fancy.css
 *
 * GTK+ uses CSS for theming. If required, applications can
 * install their own custom CSS style provider to achieve
 * special effects.
 *
 * Doing this has the downside that your application will no
 * longer react to the users theme preferences, so this should
 * be used sparingly.
 */

#include <gtk/gtk.h>
#include "demo-common.h"

static GtkWidget *window = NULL;

GtkWidget *
do_theming_custom_css (GtkWidget *do_widget)
{
  GtkWidget *box;
  GtkWidget *button;
  GtkCssProvider *provider;
  GBytes *bytes;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Custom CSS");
      gtk_container_set_border_width (GTK_CONTAINER (window), 18);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_container_add (GTK_CONTAINER (window), box);
      button = gtk_button_new_with_label ("Plain");
      gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
      button = gtk_button_new_with_label ("Fancy");
      gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
      gtk_widget_set_name (button, "fancy");

      provider = gtk_css_provider_new ();
      bytes = g_resources_lookup_data ("/theming_custom_css/gtk.css", 0, NULL);
      gtk_css_provider_load_from_data (provider, g_bytes_get_data (bytes, NULL),
                                       g_bytes_get_size (bytes), NULL);
      gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (do_widget),
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref (provider);
      g_bytes_unref (bytes);

      gtk_widget_show_all (box);
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
