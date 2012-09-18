/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>
#include <math.h>

#define WIDTH 600
#define HEIGHT 600
#define WINDOW_SIZE_JITTER 200
#define CYCLE_TIME 5.

static GtkWidget *window;
static int window_width = WIDTH, window_height = HEIGHT;

static double angle;
static int frames_since_last_print = 0;

static double load_factor = 1.0;
static double cb_no_resize = FALSE;

static cairo_surface_t *source_surface;

static void
ensure_resources(cairo_surface_t *target)
{
  cairo_t *cr;
  int i, j;

  if (source_surface != NULL)
    return;

  source_surface = cairo_surface_create_similar (target, CAIRO_CONTENT_COLOR_ALPHA, 2048, 2048);
  cr = cairo_create(source_surface);

  cairo_save(cr);
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_restore(cr);

  cairo_set_line_width(cr, 1.0);

  for (j = 0; j < 16; j++)
    for (i = 0; i < 16; i++)
      {
        cairo_set_source_rgba(cr,
                              ((i * 41) % 16) / 15.,
                              ((i * 31) % 16) / 15.,
                              ((i * 23) % 16) / 15.,
                              0.25);
        cairo_arc(cr,
                  i * 128 + 64, j * 128 + 64,
                  64 - 0.5, 0, 2 * M_PI);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr,
                              ((i * 41) % 16) / 15.,
                              ((i * 31) % 16) / 15.,
                              ((i * 23) % 16) / 15.,
                              1.0);
        cairo_stroke(cr);
      }
}

static void
on_window_draw (GtkWidget *widget,
                cairo_t   *cr)

{
  GRand *rand = g_rand_new_with_seed(0);
  int i;
  int width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  ensure_resources (cairo_get_target (cr));

  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle (cr, 0.5, 0.5, width - 1, height - 1);
  cairo_stroke (cr);

  for(i = 0; i < load_factor * 150; i++)
    {
      int source = g_rand_int_range(rand, 0, 255);
      double phi = g_rand_double_range(rand, 0, 2 * M_PI) + angle;
      double r = g_rand_double_range(rand, 0, width / 2 - 64);
      int x, y;

      int source_x = (source % 16) * 128;
      int source_y = (source / 16) * 128;

      x = round(width / 2 + r * cos(phi) - 64);
      y = round(height / 2 - r * sin(phi) - 64);

      cairo_set_source_surface(cr, source_surface,
                               x - source_x, y - source_y);
      cairo_rectangle(cr, x, y, 128, 128);
      cairo_fill(cr);
    }

  g_rand_free(rand);
}

static void
on_frame (GtkTimeline *timeline,
          double       progress)
{
  int jitter;
  double current_time;
  static double last_print_time = 0;

  current_time = g_get_monotonic_time () / 1000000.;
  if (current_time >= last_print_time + 5)
    {
      if (frames_since_last_print != 0)
        {
          g_print ("%g\n", frames_since_last_print / (current_time - last_print_time));
          frames_since_last_print = 0;
        }

      last_print_time = current_time;
    }
  frames_since_last_print++;

  angle = 2 * M_PI * progress;
  jitter = WINDOW_SIZE_JITTER * sin(angle);

  if (!cb_no_resize)
    {
      window_width = WIDTH + jitter;
      window_height = HEIGHT + jitter;
    }

  gtk_window_resize (GTK_WINDOW (window),
                     window_width, window_height);

  gtk_widget_queue_draw (window);
}

static GOptionEntry options[] = {
  { "factor", 'f', 0, G_OPTION_ARG_DOUBLE, &load_factor, "Load factor", "FACTOR" },
  { "no-resize", 'n', 0, G_OPTION_ARG_NONE, &cb_no_resize, "No Resize", NULL },
  { NULL }
};

int
main(int argc, char **argv)
{
  GError *error = NULL;
  GdkScreen *screen;
  GdkRectangle monitor_bounds;
  GtkTimeline *timeline;

  if (!gtk_init_with_args (&argc, &argv, "",
                           options, NULL, NULL))
    {
      g_printerr ("Option parsing failed: %s", error->message);
      return 1;
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  gtk_window_set_gravity (GTK_WINDOW (window), GDK_GRAVITY_CENTER);
  gtk_widget_set_app_paintable (window, TRUE);

  g_signal_connect (window, "draw",
                    G_CALLBACK (on_window_draw), NULL);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  timeline = gtk_timeline_new (window, CYCLE_TIME * 1000);
  gtk_timeline_set_loop (timeline, TRUE);
  gtk_timeline_set_progress_type (timeline, GTK_TIMELINE_PROGRESS_LINEAR);

  g_signal_connect (timeline, "frame",
                    G_CALLBACK (on_frame), NULL);
  on_frame (timeline, 0.);
  gtk_timeline_start (timeline);

  screen = gtk_widget_get_screen (window);
  gdk_screen_get_monitor_geometry (screen,
                                   gdk_screen_get_primary_monitor (screen),
                                   &monitor_bounds);

  gtk_window_move (GTK_WINDOW (window),
                   monitor_bounds.x + (monitor_bounds.width - window_width) / 2,
                   monitor_bounds.y + (monitor_bounds.height - window_height) / 2);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
