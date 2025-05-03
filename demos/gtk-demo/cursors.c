/* Cursors
 *
 * Demonstrates a useful set of available cursors. The cursors shown here are the
 * ones defined by CSS, which we assume to be available. The example shows creating
 * cursors by name or from an image, with or without a fallback.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
on_destroy (gpointer data)
{
  window = NULL;
}

static GdkTexture *
cursor_callback (GdkCursor *cursor,
                 int        cursor_size,
                 double     scale,
                 int       *width,
                 int       *height,
                 int       *hotspot_x,
                 int       *hotspot_y,
                 gpointer   data)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GError *error = NULL;
  int scaled_size;

  scaled_size = ceil (cursor_size * scale);

  pixbuf = gdk_pixbuf_new_from_resource_at_scale ("/cursors/images/gtk-logo.svg",
                                                  scaled_size, scaled_size,
                                                  TRUE,
                                                  &error);
  if (!pixbuf)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  texture = gdk_texture_new_for_pixbuf (pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS

  g_object_unref (pixbuf);

  *width = cursor_size;
  *height = cursor_size;
  *hotspot_x = 18 * cursor_size / 32.0;
  *hotspot_y = 2 * cursor_size / 32.0;

  return texture;
}

GtkWidget *
do_cursors (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *logo_callback;
      GdkCursor *cursor;
      GtkCssProvider *provider;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/cursors/cursors.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      builder = gtk_builder_new_from_resource ("/cursors/cursors.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (on_destroy), NULL);
      logo_callback = GTK_WIDGET (gtk_builder_get_object (builder, "logo_callback"));
      cursor = gdk_cursor_new_from_callback (cursor_callback, NULL, NULL, NULL);
      gtk_widget_set_cursor (logo_callback, cursor);
      g_object_unref (cursor);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
