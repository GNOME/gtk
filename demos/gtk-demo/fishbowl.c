/* Benchmark/Fishbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"

GtkWidget *allow_changes;

#define N_STATS 5

#define STATS_UPDATE_TIME G_USEC_PER_SEC

typedef struct _Stats Stats;
struct _Stats {
  gint64 last_stats;
  gint64 last_frame;
  gint last_suggestion;
  guint frame_counter_max;

  guint stats_index;
  guint frame_counter[N_STATS];
  guint item_counter[N_STATS];
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
      stats->last_frame = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
      stats->last_stats = stats->last_frame;
    }

  return stats;
}

static void
do_stats (GtkWidget *widget,
          GtkWidget *info_label,
          gint      *suggested_change)
{
  Stats *stats;
  gint64 frame_time;

  stats = get_stats (widget);
  frame_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));

  if (stats->last_stats + STATS_UPDATE_TIME < frame_time)
    {
      char *new_label;
      guint i, n_frames;

      n_frames = 0;
      for (i = 0; i < N_STATS; i++)
        {
          n_frames += stats->frame_counter[i];
        }
      
      new_label = g_strdup_printf ("icons - %.1f fps",
                                   (double) G_USEC_PER_SEC * n_frames
                                       / (N_STATS * STATS_UPDATE_TIME));
      gtk_label_set_label (GTK_LABEL (info_label), new_label);
      g_free (new_label);

      if (stats->frame_counter[stats->stats_index] >= 19 * stats->frame_counter_max / 20)
        {
          if (stats->last_suggestion > 0)
            stats->last_suggestion *= 2;
          else
            stats->last_suggestion = 1;
        }
      else
        {
          if (stats->last_suggestion < 0)
            stats->last_suggestion--;
          else
            stats->last_suggestion = -1;
          stats->last_suggestion = MAX (stats->last_suggestion, 1 - (int) stats->item_counter[stats->stats_index]);
        }

      stats->stats_index = (stats->stats_index + 1) % N_STATS;
      stats->frame_counter[stats->stats_index] = 0;
      stats->item_counter[stats->stats_index] = stats->item_counter[(stats->stats_index + N_STATS - 1) % N_STATS];
      stats->last_stats = frame_time;
      
      if (suggested_change)
        *suggested_change = stats->last_suggestion;
      else
        stats->last_suggestion = 0;
    }
  else
    {
      if (suggested_change)
        *suggested_change = 0;
    }

  stats->last_frame = frame_time;
  stats->frame_counter[stats->stats_index]++;
  stats->frame_counter_max = MAX (stats->frame_counter_max, stats->frame_counter[stats->stats_index]);
}

static void
stats_update (GtkWidget *widget)
{
  Stats *stats;

  stats = get_stats (widget);

  stats->item_counter[stats->stats_index] = gtk_fishbowl_get_count (GTK_FISHBOWL (widget));
}

static gboolean
move_fish (GtkWidget     *bowl,
           GdkFrameClock *frame_clock,
           gpointer       info_label)
{
  gint suggested_change = 0;
  
  do_stats (bowl,
            info_label,
            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (allow_changes)) ? &suggested_change : NULL);

  gtk_fishbowl_set_count (GTK_FISHBOWL (bowl),
                          gtk_fishbowl_get_count (GTK_FISHBOWL (bowl)) + suggested_change);
  stats_update (bowl);

  return G_SOURCE_CONTINUE;
}

GtkWidget *
do_fishbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *bowl, *info_label;

      g_type_ensure (GTK_TYPE_FISHBOWL);

      builder = gtk_builder_new_from_resource ("/fishbowl/fishbowl.ui");
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      bowl = GTK_WIDGET (gtk_builder_get_object (builder, "bowl"));
      info_label = GTK_WIDGET (gtk_builder_get_object (builder, "info_label"));
      allow_changes = GTK_WIDGET (gtk_builder_get_object (builder, "changes_allow"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_realize (window);
      gtk_widget_add_tick_callback (bowl, move_fish, info_label, NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);


  return window;
}
