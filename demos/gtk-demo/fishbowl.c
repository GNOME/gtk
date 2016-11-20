/* Benchmark/Fishbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

char **icon_names = NULL;
gsize n_icon_names = 0;

GtkWidget *allow_changes;

static void
init_icon_names (GtkIconTheme *theme)
{
  GPtrArray *icons;
  GList *l, *icon_list;

  if (icon_names)
    return;

  icon_list = gtk_icon_theme_list_icons (theme, NULL);
  icons = g_ptr_array_new ();

  for (l = icon_list; l; l = l->next)
    {
      if (g_str_has_suffix (l->data, "symbolic"))
        continue;

      g_ptr_array_add (icons, g_strdup (l->data));
    }

  n_icon_names = icons->len;
  g_ptr_array_add (icons, NULL); /* NULL-terminate the array */
  icon_names = (char **) g_ptr_array_free (icons, FALSE);

  /* don't free strings, we assigned them to the array */
  g_list_free_full (icon_list, g_free);
}

static const char *
get_random_icon_name (GtkIconTheme *theme)
{
  init_icon_names (theme);

  return icon_names[g_random_int_range(0, n_icon_names)];
}

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

static gint64
do_stats (GtkWidget *widget,
          GtkWidget *info_label,
          gint      *suggested_change)
{
  Stats *stats;
  gint64 frame_time, elapsed;

  stats = get_stats (widget);
  frame_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
  elapsed = frame_time - stats->last_frame;

  if (stats->last_stats + STATS_UPDATE_TIME < frame_time)
    {
      char *new_label;
      guint i, n_frames;

      n_frames = 0;
      for (i = 0; i < N_STATS; i++)
        {
          n_frames += stats->frame_counter[i];
        }
      
      new_label = g_strdup_printf ("%u icons - %.1f fps",
                                   stats->item_counter[stats->stats_index],
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

  return elapsed;
}

static void
stats_update (GtkWidget *widget,
              gint       n_items)
{
  Stats *stats;

  stats = get_stats (widget);

  g_assert ((gint) stats->item_counter[stats->stats_index] + n_items > 0);
  stats->item_counter[stats->stats_index] += n_items;
}

typedef struct _FishData FishData;
struct _FishData {
  double x;
  double y;
  double x_speed;
  double y_speed;
};

static FishData *
get_fish_data (GtkWidget *fish)
{
  static GQuark fish_quark = 0;
  FishData *data;

  if (G_UNLIKELY (fish_quark == 0))
    fish_quark = g_quark_from_static_string ("fish");

  data = g_object_get_qdata (G_OBJECT (fish), fish_quark);
  if (data == NULL)
    {
      data = g_new0 (FishData, 1);
      g_object_set_qdata_full (G_OBJECT (fish), fish_quark, data, g_free);
      data->x = 10;
      data->y = 10;
      data->x_speed = g_random_double_range (1, 200);
      data->y_speed = g_random_double_range (1, 200);
    }

  return data;
}

static void
add_fish (GtkWidget *bowl,
          guint      n_fish)
{
  GtkWidget *new_fish;
  guint i;

  for (i = 0; i < n_fish; i++)
    {
      new_fish = gtk_image_new_from_icon_name (get_random_icon_name (gtk_icon_theme_get_default ()),
                                               GTK_ICON_SIZE_DIALOG);
      gtk_widget_show (new_fish);

      gtk_fixed_put (GTK_FIXED (bowl),
                     new_fish,
                     10, 10);
    }

  stats_update (bowl, n_fish);
}

static void
remove_fish (GtkWidget *bowl,
             guint      n_fish)
{
  GList *list, *children;
  guint i;

  children = gtk_container_get_children (GTK_CONTAINER (bowl));
  g_assert (n_fish < g_list_length (children));

  list = children;
  for (i = 0; i < n_fish; i++)
    {
      gtk_container_remove (GTK_CONTAINER (bowl), list->data);
      list = list->next;
    }

  g_list_free (children);

  stats_update (bowl, - (gint) n_fish);

  {
    Stats *stats = get_stats (bowl);

    children = gtk_container_get_children (GTK_CONTAINER (bowl));
    g_assert (stats->item_counter[stats->stats_index] == g_list_length (children));
    g_list_free (children);
  }
}

static void
move_one_fish (GtkWidget *fish,
               gpointer   elapsedp)
{
  GtkWidget *fixed = gtk_widget_get_parent (fish);
  FishData *data = get_fish_data (fish);
  gint64 elapsed = *(gint64 *) elapsedp;

  data->x += data->x_speed * ((double) elapsed / G_USEC_PER_SEC);
  data->y += data->y_speed * ((double) elapsed / G_USEC_PER_SEC);

  if (data->x <= 0)
    {
      data->x = 0;
      data->x_speed = - g_random_double_range (1, 200) * (data->x_speed > 0 ? 1 : -1);
    }
  else if (data->x > gtk_widget_get_allocated_width (fixed) - gtk_widget_get_allocated_width (fish))
    {
      data->x = gtk_widget_get_allocated_width (fixed) - gtk_widget_get_allocated_width (fish);
      data->x_speed = - g_random_double_range (1, 200) * (data->x_speed > 0 ? 1 : -1);
    }

  if (data->y <= 0)
    {
      data->y = 0;
      data->y_speed = - g_random_double_range (1, 200) * (data->y_speed > 0 ? 1 : -1);
    }
  else if (data->y > gtk_widget_get_allocated_height (fixed) - gtk_widget_get_allocated_height (fish))
    {
      data->y = gtk_widget_get_allocated_height (fixed) - gtk_widget_get_allocated_height (fish);
      data->y_speed = - g_random_double_range (1, 200) * (data->y_speed > 0 ? 1 : -1);
    }

  gtk_fixed_move (GTK_FIXED (fixed), fish, data->x, data->y);
}

static gboolean
move_fish (GtkWidget     *bowl,
           GdkFrameClock *frame_clock,
           gpointer       info_label)
{
  gint64 elapsed;
  gint suggested_change = 0;
  
  elapsed = do_stats (bowl,
                      info_label,
                      !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (allow_changes)) ? &suggested_change : NULL);

  gtk_container_foreach (GTK_CONTAINER (bowl), move_one_fish, &elapsed);

  if (suggested_change > 0)
    add_fish (bowl, suggested_change);
  else if (suggested_change < 0)
    remove_fish (bowl, - suggested_change);

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
