/* Benchmark/Fishbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"

GtkWidget *info_label;
GtkWidget *allow_changes;

#define N_STATS 5

#define STATS_UPDATE_TIME G_USEC_PER_SEC

typedef struct _Stats Stats;
struct _Stats {
  gint last_suggestion;
};

static Stats *
get_stats (GtkWidget *widget)
{
  static GQuark stats_quark = 0;
  Stats *stats;

  if (G_UNLIKELY (stats_quark == 0))
    stats_quark = g_quark_from_static_string ("stats");

  stats = g_object_get_qdata (G_OBJECT (widget), stats_quark);
  if (stats == NULL)
    {
      stats = g_new0 (Stats, 1);
      g_object_set_qdata_full (G_OBJECT (widget), stats_quark, stats, g_free);
    }

  return stats;
}

static gint64
guess_refresh_interval (GdkFrameClock *frame_clock)
{
  gint64 interval;
  gint64 i;

  interval = G_MAXINT64;

  for (i = gdk_frame_clock_get_history_start (frame_clock);
       i < gdk_frame_clock_get_frame_counter (frame_clock);
       i++)
    {
      GdkFrameTimings *t, *before;
      gint64 ts, before_ts;

      t = gdk_frame_clock_get_timings (frame_clock, i);
      before = gdk_frame_clock_get_timings (frame_clock, i - 1);
      if (t == NULL || before == NULL)
        continue;

      ts = gdk_frame_timings_get_frame_time (t);
      before_ts = gdk_frame_timings_get_frame_time (before);
      if (ts == 0 || before_ts == 0)
        continue;

      interval = MIN (interval, ts - before_ts);
    }

  if (interval == G_MAXINT64)
    return 0;

  return interval;
}

static void
do_stats (GtkWidget *widget,
          gint      *suggested_change)
{
  GdkFrameClock *frame_clock;
  Stats *stats;
  GdkFrameTimings *start, *end;
  gint64 start_counter, end_counter;
  gint64 n_frames, expected_frames;
  gint64 start_timestamp, end_timestamp;
  gint64 interval;
  char *new_label;

  stats = get_stats (widget);
  frame_clock = gtk_widget_get_frame_clock (widget);
  if (frame_clock == NULL)
    return;

  start_counter = gdk_frame_clock_get_history_start (frame_clock);
  end_counter = gdk_frame_clock_get_frame_counter (frame_clock);
  start = gdk_frame_clock_get_timings (frame_clock, start_counter);
  for (end = gdk_frame_clock_get_timings (frame_clock, end_counter);
       end_counter > start_counter && end != NULL && !gdk_frame_timings_get_complete (end);
       end = gdk_frame_clock_get_timings (frame_clock, end_counter))
    end_counter--;
  if (end_counter - start_counter < 4)
    return;

  start_timestamp = gdk_frame_timings_get_presentation_time (start);
  end_timestamp = gdk_frame_timings_get_presentation_time (end);
  if (start_timestamp == 0 || end_timestamp == 0)
    {
      start_timestamp = gdk_frame_timings_get_frame_time (start);
      end_timestamp = gdk_frame_timings_get_frame_time (end);
    }

  interval = gdk_frame_timings_get_refresh_interval (end);
  if (interval == 0)
    {
      interval = guess_refresh_interval (frame_clock);
      if (interval == 0)
        return;
    }
  n_frames = end_counter - start_counter;
  expected_frames = round ((double) (end_timestamp - start_timestamp) / interval);

  new_label = g_strdup_printf ("icons - %.1f fps",
                               ((double) n_frames) * G_USEC_PER_SEC / (end_timestamp - start_timestamp));
  gtk_label_set_label (GTK_LABEL (info_label), new_label);
  g_free (new_label);

  if (n_frames >= expected_frames)
    {
      if (stats->last_suggestion > 0)
        stats->last_suggestion *= 2;
      else
        stats->last_suggestion = 1;
    }
  else if (n_frames + 1 < expected_frames)
    {
      if (stats->last_suggestion < 0)
        stats->last_suggestion--;
      else
        stats->last_suggestion = -1;
    }
  else
    {
      stats->last_suggestion = 0;
    }

  if (suggested_change)
    *suggested_change = stats->last_suggestion;
  else
    stats->last_suggestion = 0;
}

static gboolean
move_fish (gpointer bowl)
{
  gint suggested_change = 0, new_count;
  
  do_stats (bowl,
            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (allow_changes)) ? &suggested_change : NULL);

  new_count = gtk_fishbowl_get_count (GTK_FISHBOWL (bowl)) + suggested_change;
  new_count = MAX (1, new_count);
  gtk_fishbowl_set_count (GTK_FISHBOWL (bowl), new_count);

  return G_SOURCE_CONTINUE;
}

GtkWidget *
do_fishbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *bowl;

      g_type_ensure (GTK_TYPE_FISHBOWL);

      builder = gtk_builder_new_from_resource ("/fishbowl/fishbowl.ui");
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      bowl = GTK_WIDGET (gtk_builder_get_object (builder, "bowl"));
      gtk_fishbowl_set_use_icons (GTK_FISHBOWL (bowl), TRUE);
      info_label = GTK_WIDGET (gtk_builder_get_object (builder, "info_label"));
      allow_changes = GTK_WIDGET (gtk_builder_get_object (builder, "changes_allow"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_realize (window);
      g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE,
                                  1,
                                  move_fish,
                                  bowl,
                                  NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);


  return window;
}
