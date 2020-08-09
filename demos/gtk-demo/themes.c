/* Benchmark/Themes
 *
 * This demo switches themes like a maniac, like some of you.
 *
 * Warning: This demo involves rapidly flashing changes and may
 * be hazardous to photosensitive viewers.
 */

#include <gtk/gtk.h>

static guint tick_cb;

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

static double
frame_clock_get_fps (GdkFrameClock *frame_clock)
{
  GdkFrameTimings *start, *end;
  gint64 start_counter, end_counter;
  gint64 start_timestamp, end_timestamp;
  gint64 interval;

  start_counter = gdk_frame_clock_get_history_start (frame_clock);
  end_counter = gdk_frame_clock_get_frame_counter (frame_clock);
  start = gdk_frame_clock_get_timings (frame_clock, start_counter);
  for (end = gdk_frame_clock_get_timings (frame_clock, end_counter);
       end_counter > start_counter && end != NULL && !gdk_frame_timings_get_complete (end);
       end = gdk_frame_clock_get_timings (frame_clock, end_counter))
    end_counter--;
  if (end_counter - start_counter < 4)
    return 0.0;

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
        return 0.0;
    }

  return ((double) end_counter - start_counter) * G_USEC_PER_SEC / (end_timestamp - start_timestamp);
}

typedef struct {
  const char *name;
  gboolean dark;
} Theme;

static Theme themes[] = {
  { "Adwaita", FALSE },
  { "Adwaita", TRUE },
  { "HighContrast", FALSE },
  { "HighContrastInverse", FALSE }
};

static int theme;

static gboolean
change_theme (GtkWidget     *widget,
              GdkFrameClock *frame_clock,
              gpointer       data)
{
  GtkWidget *label = data;
  Theme next = themes[theme++ % G_N_ELEMENTS (themes)];
  char *name;

  g_object_set (gtk_settings_get_default (),
                "gtk-theme-name", next.name,
                "gtk-application-prefer-dark-theme", next.dark,
                NULL);

  name = g_strconcat (next.name, next.dark ? " (dark)" : NULL, NULL);
  gtk_window_set_title (GTK_WINDOW (widget), name);
  g_free (name);

  if (frame_clock)
    {
      char *fps;

      fps = g_strdup_printf ("%.2f fps", frame_clock_get_fps (frame_clock));
      gtk_label_set_label (GTK_LABEL (label), fps);
      g_free (fps);
    }
  else
    gtk_label_set_label (GTK_LABEL (label), "");

  return G_SOURCE_CONTINUE;
}

static void
toggle_cycle (GObject    *button,
              GParamSpec *pspec,
              gpointer    data)
{
  GtkWidget *warning = data;
  gboolean active;
  GtkWidget *window;

  g_object_get (button, "active", &active, NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);

  if (active && !tick_cb)
    {
      gtk_window_present (GTK_WINDOW (warning));
    }
  else if (!active && tick_cb)
    {
      gtk_widget_remove_tick_callback (window, tick_cb);
      tick_cb = 0;
    }
}

static void
warning_closed (GtkDialog *warning,
                int        response_id,
                gpointer   data)
{
  GtkWidget *window;
  GtkWidget *button;

  gtk_widget_hide (GTK_WIDGET (warning));

  window = gtk_widget_get_ancestor (GTK_WIDGET (data), GTK_TYPE_WINDOW);
  button = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "button"));

  if (response_id == GTK_RESPONSE_OK)
    tick_cb = gtk_widget_add_tick_callback (window, change_theme, data, NULL);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

GtkWidget *
do_themes (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *button;
      GtkWidget *label;
      GtkWidget *warning;

      builder = gtk_builder_new_from_resource ("/themes/themes.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));

      label = GTK_WIDGET (gtk_builder_get_object (builder, "fps"));
      warning = GTK_WIDGET (gtk_builder_get_object (builder, "warning"));
      g_signal_connect (warning, "response", G_CALLBACK (warning_closed), label);

      button = GTK_WIDGET (gtk_builder_get_object (builder, "toggle"));
      g_object_set_data (G_OBJECT (window), "button", button);
      g_signal_connect (button, "notify::active", G_CALLBACK (toggle_cycle), warning);
      gtk_widget_realize (window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
