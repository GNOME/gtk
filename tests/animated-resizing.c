/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#include "variable.h"

#define RADIUS 64
#define DIAMETER (2*RADIUS)
#define WIDTH 600
#define HEIGHT 600
#define WINDOW_SIZE_JITTER 200
#define CYCLE_TIME 5.

static GtkWidget *window;
static int window_width = WIDTH, window_height = HEIGHT;

gint64 start_frame_time;
static double angle;

static int max_stats = -1;
static double statistics_time = 5.;
static double load_factor = 1.0;
static double cb_no_resize = FALSE;
static gboolean machine_readable = FALSE;

static cairo_surface_t *source_surface;

static void
ensure_resources(cairo_surface_t *target)
{
  cairo_t *cr;
  int i, j;

  if (source_surface != NULL)
    return;

  source_surface = cairo_surface_create_similar (target, CAIRO_CONTENT_COLOR_ALPHA,
                                                 16 * DIAMETER, 16 * DIAMETER);
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
                  i * DIAMETER + RADIUS, j * DIAMETER + RADIUS,
                  RADIUS - 0.5, 0, 2 * M_PI);
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
      double r = g_rand_double_range(rand, 0, width / 2 - RADIUS);
      int x, y;

      int source_x = (source % 16) * DIAMETER;
      int source_y = (source / 16) * DIAMETER;

      x = round(width / 2 + r * cos(phi) - RADIUS);
      y = round(height / 2 - r * sin(phi) - RADIUS);

      cairo_set_source_surface(cr, source_surface,
                               x - source_x, y - source_y);
      cairo_rectangle(cr, x, y, DIAMETER, DIAMETER);
      cairo_fill(cr);
    }

  g_rand_free(rand);
}

static void
print_double (const char *description,
              double      value)
{
  if (machine_readable)
    g_print ("%g\t", value);
  else
    g_print ("%s: %g\n", description, value);
}

static void
print_variable (const char *description,
                Variable *variable)
{
  if (variable->weight != 0)
    {
      if (machine_readable)
        g_print ("%g\t%g\t",
                 variable_mean (variable),
                 variable_standard_deviation (variable));
      else
        g_print ("%s: %g +/- %g\n", description,
                 variable_mean (variable),
                 variable_standard_deviation (variable));
    }
  else
    {
      if (machine_readable)
        g_print ("-\t-\t");
      else
        g_print ("%s: <n/a>\n", description);
    }
}

static void
handle_frame_stats (GdkFrameClock *frame_clock)
{
  static int num_stats = 0;
  static double last_print_time = 0;
  static int frames_since_last_print = 0;
  static gint64 frame_counter;
  static gint64 last_handled_frame = -1;

  static Variable latency = VARIABLE_INIT;

  double current_time;

  current_time = g_get_monotonic_time ();
  if (current_time >= last_print_time + 1000000 * statistics_time)
    {
      if (frames_since_last_print)
        {
          if (num_stats == 0 && machine_readable)
            {
              g_print ("# load_factor frame_rate latency\n");
            }

          num_stats++;
          if (machine_readable)
            g_print ("%g	", load_factor);
          print_double ("Frame rate ",
                        frames_since_last_print / ((current_time - last_print_time) / 1000000.));

          print_variable ("Latency", &latency);

          g_print ("\n");
        }

      last_print_time = current_time;
      frames_since_last_print = 0;
      variable_reset (&latency);

      if (num_stats == max_stats)
        gtk_main_quit ();
    }

  frames_since_last_print++;

  for (frame_counter = last_handled_frame;
       frame_counter < gdk_frame_clock_get_frame_counter (frame_clock);
       frame_counter++)
    {
      GdkFrameTimings *timings = gdk_frame_clock_get_timings (frame_clock, frame_counter);
      GdkFrameTimings *previous_timings = gdk_frame_clock_get_timings (frame_clock, frame_counter - 1);

      if (!timings || gdk_frame_timings_get_complete (timings))
        last_handled_frame = frame_counter;

      if (timings && gdk_frame_timings_get_complete (timings) && previous_timings &&
          gdk_frame_timings_get_presentation_time (timings) != 0 &&
          gdk_frame_timings_get_presentation_time (previous_timings) != 0)
        {
          double display_time = (gdk_frame_timings_get_presentation_time (timings) - gdk_frame_timings_get_presentation_time (previous_timings)) / 1000.;
          double frame_latency = (gdk_frame_timings_get_presentation_time (previous_timings) - gdk_frame_timings_get_frame_time (previous_timings)) / 1000. + display_time / 2;

          variable_add_weighted (&latency, frame_latency, display_time);
        }
    }
}

static void
on_frame (double progress)
{
  GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (window);
  int jitter;

  if (frame_clock)
    handle_frame_stats (frame_clock);

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

static gboolean
tick_callback (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       user_data)
{
  gint64 frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  double scaled_time;

  if (start_frame_time == 0)
    start_frame_time = frame_time;

  scaled_time = (frame_time - start_frame_time) / (CYCLE_TIME * 1000000);
  on_frame (scaled_time - floor (scaled_time));

  return G_SOURCE_CONTINUE;
}

static gboolean
on_map_event (GtkWidget	  *widget,
              GdkEventAny *event)
{
  gtk_widget_add_tick_callback (window, tick_callback, NULL, NULL);

  return FALSE;
}

static GOptionEntry options[] = {
  { "factor", 'f', 0, G_OPTION_ARG_DOUBLE, &load_factor, "Load factor", "FACTOR" },
  { "max-statistics", 'm', 0, G_OPTION_ARG_INT, &max_stats, "Maximum statistics printed", NULL },
  { "machine-readable", 0, 0, G_OPTION_ARG_NONE, &machine_readable, "Print statistics in columns", NULL },
  { "no-resize", 'n', 0, G_OPTION_ARG_NONE, &cb_no_resize, "No Resize", NULL },
  { "statistics-time", 's', 0, G_OPTION_ARG_DOUBLE, &statistics_time, "Statistics accumulation time", "TIME" },
  { NULL }
};

int
main(int argc, char **argv)
{
  GError *error = NULL;
  GdkScreen *screen;
  GdkRectangle monitor_bounds;

  if (!gtk_init_with_args (&argc, &argv, "",
                           options, NULL, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  g_print ("%sLoad factor: %g\n",
           machine_readable ? "# " : "",
           load_factor);
  g_print ("%sResizing?: %s\n",
           machine_readable ? "# " : "",
           cb_no_resize ? "no" : "yes");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  gtk_window_set_gravity (GTK_WINDOW (window), GDK_GRAVITY_CENTER);
  gtk_widget_set_app_paintable (window, TRUE);

  g_signal_connect (window, "draw",
                    G_CALLBACK (on_window_draw), NULL);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  g_signal_connect (window, "map-event",
                    G_CALLBACK (on_map_event), NULL);
  on_frame (0.);

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
