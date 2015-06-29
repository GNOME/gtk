/* Pango/Text Mask
 *
 * This demo shows how to use PangoCairo to draw text with more than
 * just a single color.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static gboolean
draw_text (GtkWidget *da,
           cairo_t   *cr,
           gpointer   data)
{
  cairo_pattern_t *pattern;
  PangoLayout *layout;
  PangoFontDescription *desc;

  cairo_save (cr);

  layout = gtk_widget_create_pango_layout (da, "Pango power!\nPango power!\nPango power!");
  desc = pango_font_description_from_string ("sans bold 34");
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);

  cairo_move_to (cr, 30, 20);
  pango_cairo_layout_path (cr, layout);
  g_object_unref (layout);

  pattern = cairo_pattern_create_linear (0.0, 0.0,
                                         gtk_widget_get_allocated_width (da),
                                         gtk_widget_get_allocated_height (da));
  cairo_pattern_add_color_stop_rgb (pattern, 0.0, 1.0, 0.0, 0.0);
  cairo_pattern_add_color_stop_rgb (pattern, 0.2, 1.0, 0.0, 0.0);
  cairo_pattern_add_color_stop_rgb (pattern, 0.3, 1.0, 1.0, 0.0);
  cairo_pattern_add_color_stop_rgb (pattern, 0.4, 0.0, 1.0, 0.0);
  cairo_pattern_add_color_stop_rgb (pattern, 0.6, 0.0, 1.0, 1.0);
  cairo_pattern_add_color_stop_rgb (pattern, 0.7, 0.0, 0.0, 1.0);
  cairo_pattern_add_color_stop_rgb (pattern, 0.8, 1.0, 0.0, 1.0);
  cairo_pattern_add_color_stop_rgb (pattern, 1.0, 1.0, 0.0, 1.0);

  cairo_set_source (cr, pattern);
  cairo_fill_preserve (cr);

  cairo_pattern_destroy (pattern);

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_set_line_width (cr, 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);

  return TRUE;
}

GtkWidget *
do_textmask (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkWidget *da;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_widget_set_size_request (window, 400, 200);
      gtk_window_set_title (GTK_WINDOW (window), "Text Mask");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      da = gtk_drawing_area_new ();

      gtk_container_add (GTK_CONTAINER (window), da);
      g_signal_connect (da, "draw",
                        G_CALLBACK (draw_text), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
