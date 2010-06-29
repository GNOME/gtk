#include <gtk/gtk.h>

/* The expose event handler for the event box.
 *
 * This function simply draws a transparency onto a widget on the area
 * for which it receives expose events.  This is intended to give the
 * event box a "transparent" background.
 *
 * In order for this to work properly, the widget must have an RGBA
 * colourmap.  The widget should also be set as app-paintable since it
 * doesn't make sense for GTK+ to draw a background if we are drawing it
 * (and because GTK+ might actually replace our transparency with its
 * default background colour).
 */
static gboolean
transparent_expose (GtkWidget      *widget,
                    GdkEventExpose *event)
{
  cairo_t *cr;

   cr = gdk_cairo_create (widget->window);
   cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
   gdk_cairo_region (cr, event->region);
   cairo_fill (cr);
   cairo_destroy (cr);

   return FALSE;
 }

/* The expose event handler for the window.
 *
 * This function performs the actual compositing of the event box onto
 * the already-existing background of the window at 50% normal opacity.
 *
 * In this case we do not want app-paintable to be set on the widget
 * since we want it to draw its own (red) background. Because of this,
 * however, we must ensure that we use g_signal_connect_after so that
 * this handler is called after the red has been drawn. If it was
 * called before then GTK would just blindly paint over our work.
 *
 * Note: if the child window has children, then you need a cairo 1.6
 * feature to make this work correctly.
 */
static gboolean
window_expose_event (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  cairo_region_t *region;
  GtkWidget *child;
  cairo_t *cr;

  /* get our child (in this case, the event box) */
  child = gtk_bin_get_child (GTK_BIN (widget));

  /* create a cairo context to draw to the window */
  cr = gdk_cairo_create (widget->window);

  /* the source data is the (composited) event box */
  gdk_cairo_set_source_pixmap (cr, child->window,
			       child->allocation.x,
			       child->allocation.y);

  /* draw no more than our expose event intersects our child */
  region = cairo_region_create_rectangle (&child->allocation);
  cairo_region_intersect (region, region, event->region);
  gdk_cairo_region (cr, region);
  cairo_clip (cr);
  cairo_region_destroy (region);

  /* composite, with a 50% opacity */
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint_with_alpha (cr, 0.5);

  /* we're done */
  cairo_destroy (cr);

  return FALSE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *event, *button;
  GdkScreen *screen;
  GdkColormap *rgba;
  GdkColor red;

  gtk_init (&argc, &argv);

  /* Make the widgets */
  button = gtk_button_new_with_label ("A Button");
  event = gtk_event_box_new ();
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* Put a red background on the window */
  gdk_color_parse ("red", &red);
  gtk_widget_modify_bg (window, GTK_STATE_NORMAL, &red);

  /* Set the colourmap for the event box.
   * Must be done before the event box is realised.
   */
  screen = gtk_widget_get_screen (event);
  rgba = gdk_screen_get_rgba_colormap (screen);
  gtk_widget_set_colormap (event, rgba);

  /* Set our event box to have a fully-transparent background
   * drawn on it. Currently there is no way to simply tell GTK+
   * that "transparency" is the background colour for a widget.
   */
  gtk_widget_set_app_paintable (GTK_WIDGET (event), TRUE);
  g_signal_connect (event, "expose-event",
		    G_CALLBACK (transparent_expose), NULL);

  /* Put them inside one another */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  gtk_container_add (GTK_CONTAINER (window), event);
  gtk_container_add (GTK_CONTAINER (event), button);

  /* Realise and show everything */
  gtk_widget_show_all (window);

  /* Set the event box GdkWindow to be composited.
   * Obviously must be performed after event box is realised.
   */
  gdk_window_set_composited (event->window, TRUE);

  /* Set up the compositing handler.
   * Note that we do _after_ so that the normal (red) background is drawn
   * by gtk before our compositing occurs.
   */
  g_signal_connect_after (window, "expose-event",
			  G_CALLBACK (window_expose_event), NULL);

  gtk_main ();

  return 0;
}
