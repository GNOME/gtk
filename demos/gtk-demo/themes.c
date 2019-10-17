/* Benchmark/Themes
 *
 * This demo switches themes like a maniac, like some of you.
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
change_theme (GtkWidget *widget,
              GdkFrameClock *frame_clock,
              gpointer data)
{
  GtkBuilder *builder = data;
  GtkWidget *header;
  GtkWidget *label;
  Theme next = themes[theme++ % G_N_ELEMENTS (themes)];
  char *name;

  g_object_set (gtk_settings_get_default (),
                "gtk-theme-name", next.name,
                "gtk-application-prefer-dark-theme", next.dark,
                NULL);

  header = GTK_WIDGET (gtk_builder_get_object (builder, "header"));
  name = g_strconcat (next.name, next.dark ? " (dark)" : NULL, NULL);
  gtk_header_bar_set_title (GTK_HEADER_BAR (header), name);
  g_free (name);

  label = GTK_WIDGET (gtk_builder_get_object (builder, "fps"));
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
clicked (GtkGestureClick *gesture,
         int n_press,
         double x,
         double y,
         gpointer data)
{
  GtkWidget *window;
  const GdkEvent *event;
  GdkModifierType state;

  window = gtk_widget_get_ancestor (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)), GTK_TYPE_WINDOW);

  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), NULL);
  gdk_event_get_state (event, &state);

  if (state & GDK_CONTROL_MASK)
    {
      if (tick_cb)
        {
          gtk_widget_remove_tick_callback (window, tick_cb);
          tick_cb = 0;
        }

      change_theme (window, NULL, data);
    }
  else
    {
      if (tick_cb)
        {
          gtk_widget_remove_tick_callback (window, tick_cb);
          tick_cb = 0;
        }
      else
        {
          tick_cb = gtk_widget_add_tick_callback (window, change_theme, data, NULL);
        }
    }
}

GtkWidget *
do_themes (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *header;
      GtkGesture *gesture;

      builder = gtk_builder_new_from_resource ("/themes/themes.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      header = GTK_WIDGET (gtk_builder_get_object (builder, "header"));
      gesture = gtk_gesture_click_new ();
      g_signal_connect (gesture, "pressed", G_CALLBACK (clicked), builder);
      gtk_widget_add_controller (header, GTK_EVENT_CONTROLLER (gesture));

      gtk_widget_realize (window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
