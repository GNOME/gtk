/* Color Spaces
 *
 * Demonstrates support for color spaces.
 *
 * The test images used here are taken from http://displaycal.net/icc-color-management-test/
 * and are licensed under the Creative Commons BY-SA 4.0 International License
 */

#include <gtk/gtk.h>

static GtkWidget *jpeg;
static GtkWidget *png;
static GtkWidget *tiff;
static GtkWidget *noprofile;
static GtkWidget *test1;
static GtkWidget *test2;

static void
on_changed (GtkCheckButton *button,
            gpointer        user_data)
{
  GdkTexture *texture;
  const char *extension = NULL;
  char *path;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (button)))
    return;

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (jpeg)))
    extension = ".jpg";
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (png)))
    extension = ".png";
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (tiff)))
    extension = ".tif";
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (noprofile)))
    extension = "-expected-result-no-cm.png";

  path = g_strconcat ("/colorspaces/sRGB_Gray", extension, NULL);
  texture = gdk_texture_new_from_resource (path);
  gtk_picture_set_paintable (GTK_PICTURE (test1), GDK_PAINTABLE (texture));
  g_object_unref (texture);

  path = g_strconcat ("/colorspaces/ICC-Rendering-Intent-Test", extension, NULL);
  texture = gdk_texture_new_from_resource (path);
  gtk_picture_set_paintable (GTK_PICTURE (test2), GDK_PAINTABLE (texture));
  g_object_unref (texture);
}

GtkWidget*
do_colorspaces (GtkWidget *do_widget)
{
  static GtkWidget *window;

  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (scope),
                                              "on_changed", G_CALLBACK (on_changed));

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/colorspaces/colorspaces.ui", NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      jpeg = GTK_WIDGET (gtk_builder_get_object (builder, "jpeg"));
      png = GTK_WIDGET (gtk_builder_get_object (builder, "png"));
      tiff = GTK_WIDGET (gtk_builder_get_object (builder, "tiff"));
      noprofile = GTK_WIDGET (gtk_builder_get_object (builder, "noprofile"));
      test1 = GTK_WIDGET (gtk_builder_get_object (builder, "test1"));
      test2 = GTK_WIDGET (gtk_builder_get_object (builder, "test2"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
