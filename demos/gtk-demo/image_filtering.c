/* Image Filtering
 *
 * Show some image filter effects.
 */

#include <gtk/gtk.h>

#include "filter_paintable.h"

static GtkWidget *window = NULL;

GtkWidget *
do_image_filtering (GtkWidget *do_widget)
{
  static GtkCssProvider *css_provider = NULL;

  if (!css_provider)
    {
      css_provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (css_provider, "/image_filtering/image_filtering.css");

      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (css_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (!window)
    {
      GtkBuilder *builder;

      g_type_ensure (GTK_TYPE_FILTER_PAINTABLE);

      builder = gtk_builder_new ();

      GError *error = NULL;
      if (!gtk_builder_add_from_resource (builder, "/image_filtering/image_filtering.ui", &error))
        g_print ("%s", error->message);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
