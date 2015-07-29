/* Overlay/Transparency
 *
 * Use transparent background on GdkWindows to create a shadow effect on a GtkOverlay widget.
 */

#include <gtk/gtk.h>

#define SHADOW_OFFSET_X 7
#define SHADOW_OFFSET_Y 7
#define SHADOW_RADIUS 5

static void
draw_shadow_box (cairo_t   *cr,
                 GdkRectangle rect,
                 double radius,
                 double transparency)
{
  cairo_pattern_t *pattern;
  double x0, x1, x2, x3;
  double y0, y1, y2, y3;

  x0 = rect.x;
  x1 = rect.x + radius;
  x2 = rect.x + rect.width - radius;
  x3 = rect.x + rect.width;

  y0 = rect.y;
  y1 = rect.y + radius;
  y2 = rect.y + rect.height - radius;
  y3 = rect.y + rect.height;

   /* Fill non-border part */
  cairo_set_source_rgba (cr, 0, 0, 0, transparency);
  cairo_rectangle (cr,
                   x1, y1, x2 - x1, y2 - y1);
  cairo_fill (cr);

  /* Upper border */

  pattern = cairo_pattern_create_linear (0, y0, 0, y1);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, transparency);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x1, y0,
                   x2 - x1, y1 - y0);
  cairo_fill (cr);

  /* Bottom border */

  pattern = cairo_pattern_create_linear (0, y2, 0, y3);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, transparency);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x1, y2,
                   x2 - x1, y3 - y2);
  cairo_fill (cr);

  /* Left border */

  pattern = cairo_pattern_create_linear (x0, 0, x1, 0);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, 0.0);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, transparency);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x0, y1,
                   x1 - x0, y2 - y1);
  cairo_fill (cr);

  /* Right border */

  pattern = cairo_pattern_create_linear (x2, 0, x3, 0);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, transparency);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x2, y1,
                   x3 - x2, y2 - y1);
  cairo_fill (cr);

  /* NW corner */

  pattern = cairo_pattern_create_radial (x1, y1, 0,
                                         x1, y1, radius);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, transparency);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x0, y0,
                   x1 - x0, y1 - y0);
  cairo_fill (cr);

  /* NE corner */

  pattern = cairo_pattern_create_radial (x2, y1, 0,
                                         x2, y1, radius);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, transparency);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x2, y0,
                   x3 - x2, y1 - y0);
  cairo_fill (cr);

  /* SW corner */

  pattern = cairo_pattern_create_radial (x1, y2, 0,
                                         x1, y2, radius);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, transparency);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x0, y2,
                   x1 - x0, y3 - y2);
  cairo_fill (cr);

  /* SE corner */

  pattern = cairo_pattern_create_radial (x2, y2, 0,
                                         x2, y2, radius);

  cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0, 0, transparency);
  cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0, 0, 0.0);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   x2, y2,
                   x3 - x2, y3 - y2);
  cairo_fill (cr);
}

static gboolean
draw_callback (GtkWidget *widget,
               cairo_t   *cr,
               gpointer   data)
{
  GdkRectangle rect;

  gtk_widget_get_allocation (widget, &rect);
  rect.x += SHADOW_OFFSET_X;
  rect.y += SHADOW_OFFSET_Y;
  rect.width -= SHADOW_OFFSET_X;
  rect.height -= SHADOW_OFFSET_Y;

  draw_shadow_box (cr,
                   rect, SHADOW_RADIUS, 0.4);

  return FALSE;
}

GtkWidget *
do_transparent (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkWidget *overlay;
      GtkWidget *entry;
      GtkCssProvider *provider;
      gchar *css;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 450, 450);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "Transparency");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      view = gtk_text_view_new ();

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (sw), view);

      overlay = gtk_overlay_new ();
      gtk_container_add (GTK_CONTAINER (overlay), sw);
      gtk_container_add (GTK_CONTAINER (window), overlay);

      entry = gtk_entry_new ();
      provider = gtk_css_provider_new ();
      css = g_strdup_printf ("* { border-width: 0px %dpx %dpx 0px; }",
                             SHADOW_OFFSET_X, SHADOW_OFFSET_Y);
      gtk_css_provider_load_from_data (provider, css, -1, NULL);
      g_free (css);
      gtk_style_context_add_provider (gtk_widget_get_style_context (entry),
                                      GTK_STYLE_PROVIDER (provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_signal_connect (entry, "draw", G_CALLBACK (draw_callback), NULL);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), entry);
      gtk_widget_set_halign (entry, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (entry, GTK_ALIGN_START);

      gtk_widget_show_all (overlay);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
