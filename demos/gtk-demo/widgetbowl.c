/* Benchmark/Widgetbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"

GtkWidget *fishbowl;

static GtkWidget *
create_button (void)
{
  return gtk_button_new_with_label ("Button");;
}

static GtkWidget *
create_font_button (void)
{
  return gtk_font_button_new ();
}

static GtkWidget *
create_level_bar (void)
{
  GtkWidget *w = gtk_level_bar_new_for_interval (0, 100);

  gtk_level_bar_set_value (GTK_LEVEL_BAR (w), 50);

  /* Force them to be a bit larger */
  gtk_widget_set_size_request (w, 200, -1);

  return w;
}

static GtkWidget *
create_label (void)
{
  GtkWidget *w = gtk_label_new ("pLorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.");

  gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (w), 100);

  return w;
}

static const struct {
  const char *name;
  GtkWidget * (*create_func) (void);
} widget_types[] = {
  { "Button",     create_button      },
  { "Fontbutton", create_font_button },
  { "Levelbar"  , create_level_bar   },
  { "Label"     , create_label       },
};

static int selected_widget_type = -1;
static const int N_WIDGET_TYPES = G_N_ELEMENTS (widget_types);

#define N_STATS 5

#define STATS_UPDATE_TIME G_USEC_PER_SEC

static void
set_widget_type (GtkWidget *headerbar,
                 int        widget_type_index)
{
  GList *children, *l;

  if (widget_type_index == selected_widget_type)
    return;

  /* Remove everything */
  children = gtk_container_get_children (GTK_CONTAINER (fishbowl));
  for (l = children; l; l = l->next)
    {
      gtk_container_remove (GTK_CONTAINER (fishbowl), (GtkWidget*)l->data);
    }

  g_list_free (children);

  selected_widget_type = widget_type_index;

  gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar),
                            widget_types[selected_widget_type].name);
}


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

      new_label = g_strdup_printf ("widgets - %.1f fps",
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

  do_stats (bowl, info_label, &suggested_change);

  if (suggested_change > 0)
    {
      int i;

      for (i = 0; i < suggested_change; i ++)
        {
          GtkWidget *new_widget = widget_types[selected_widget_type].create_func ();

          gtk_container_add (GTK_CONTAINER (fishbowl), new_widget);

        }
    }
  else if (suggested_change < 0)
    {
      GList *children, *l;
      int n_removed = 0;

      children = gtk_container_get_children (GTK_CONTAINER (fishbowl));
      for (l = children; l; l = l->next)
        {
          gtk_container_remove (GTK_CONTAINER (fishbowl), (GtkWidget *)l->data);
          n_removed ++;

          if (n_removed >= (-suggested_change))
            break;
        }

      g_list_free (children);
    }

  stats_update (bowl);

  return G_SOURCE_CONTINUE;
}

static void
next_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkWidget *headerbar = user_data;
  int new_index;

  if (selected_widget_type + 1 >= N_WIDGET_TYPES)
    new_index = 0;
  else
    new_index = selected_widget_type + 1;

  set_widget_type (headerbar, new_index);
}

static void
prev_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkWidget *headerbar = user_data;
  int new_index;

  if (selected_widget_type - 1 < 0)
    new_index = N_WIDGET_TYPES - 1;
  else
    new_index = selected_widget_type - 1;

  set_widget_type (headerbar, new_index);
}

GtkWidget *
do_widgetbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *info_label;
      GtkWidget *count_label;
      GtkWidget *titlebar;
      GtkWidget *title_box;
      GtkWidget *left_box;
      GtkWidget *next_button;
      GtkWidget *prev_button;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      titlebar = gtk_header_bar_new ();
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (titlebar), TRUE);
      info_label = gtk_label_new ("widget - 00.0 fps");
      count_label = gtk_label_new ("0");
      fishbowl = gtk_fishbowl_new ();
      title_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      prev_button = gtk_button_new_from_icon_name ("pan-start-symbolic");
      next_button = gtk_button_new_from_icon_name ("pan-end-symbolic");
      left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      g_object_bind_property (fishbowl, "count", count_label, "label", 0);
      g_signal_connect (next_button, "clicked", G_CALLBACK (next_button_clicked_cb), titlebar);
      g_signal_connect (prev_button, "clicked", G_CALLBACK (prev_button_clicked_cb), titlebar);

      gtk_fishbowl_set_animating (GTK_FISHBOWL (fishbowl), TRUE);

      gtk_widget_set_hexpand (title_box, TRUE);
      gtk_widget_set_halign (title_box, GTK_ALIGN_END);

      gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);
      gtk_container_add (GTK_CONTAINER (title_box), count_label);
      gtk_container_add (GTK_CONTAINER (title_box), info_label);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (titlebar), title_box);
      gtk_container_add (GTK_CONTAINER (window), fishbowl);


      gtk_style_context_add_class (gtk_widget_get_style_context (left_box), "linked");
      gtk_container_add (GTK_CONTAINER (left_box), prev_button);
      gtk_container_add (GTK_CONTAINER (left_box), next_button);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), left_box);

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_realize (window);
      gtk_widget_add_tick_callback (fishbowl, move_fish, info_label, NULL);

      set_widget_type (titlebar, 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);


  return window;
}
