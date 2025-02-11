/* Fixed Layout / Transformations
 * #Keywords: GtkLayoutManager
 *
 * GtkFixed is a container that allows placing and transforming
 * widgets manually.
 *
 * This demo shows how to rotate and scale a child widget using
 * a transform.
 */

#include <gtk/gtk.h>

static GtkWidget *demo_window = NULL;
static gint64 start_time = 0;

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  GtkFixed *fixed = GTK_FIXED (widget);
  GtkWidget *child = user_data;
  gint64 now = g_get_monotonic_time ();
  double duration;
  double angle;
  double width, height;
  double child_width, child_height;
  GskTransform *transform;
  double scale;

  duration = (now - start_time) / (double) G_TIME_SPAN_SECOND;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  child_width = gtk_widget_get_width (child);
  child_height = gtk_widget_get_height (child);

  angle = duration * 90;
  scale = 2 + sin (duration * M_PI);

  transform = gsk_transform_translate (
      gsk_transform_scale (
          gsk_transform_rotate (
              gsk_transform_translate (NULL,
                  &GRAPHENE_POINT_INIT (width / 2, height / 2)),
              angle),
          scale, scale),
      &GRAPHENE_POINT_INIT (- child_width / 2,  - child_height / 2));

  gtk_fixed_set_child_transform (fixed, child, transform);

  gsk_transform_unref (transform);

  return G_SOURCE_CONTINUE;
}

static GtkWidget *
create_demo_window (GtkWidget *do_widget)
{
  GtkWidget *window, *sw, *fixed, *child;

  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "Fixed Layout ‐ Transformations");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), sw);

  fixed = gtk_fixed_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), fixed);

  child = gtk_label_new ("All fixed?");
  gtk_fixed_put (GTK_FIXED (fixed), child, 0, 0);
  gtk_widget_set_overflow (fixed, GTK_OVERFLOW_VISIBLE);

  gtk_widget_add_tick_callback (fixed, tick_cb, child, NULL);

  return window;
}

GtkWidget*
do_fixed2 (GtkWidget *do_widget)
{
  if (demo_window == NULL)
    demo_window = create_demo_window (do_widget);

  start_time = g_get_monotonic_time ();

  if (!gtk_widget_get_visible (demo_window))
    gtk_widget_set_visible (demo_window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (demo_window));

  return demo_window;
}
