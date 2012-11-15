#include <math.h>
#include <gtk/gtk.h>

#include "variable.h"

typedef struct {
  gdouble angle;
  gint64 stream_time;
  gint64 clock_time;
  gint64 frame_counter;
} FrameData;

static FrameData *displayed_frame;
static GtkWidget *window;
static GList *past_frames;
static Variable latency_error = VARIABLE_INIT;
static int dropped_frames = 0;
static int n_frames = 0;

static int fps = 24;

/* Thread-safe frame queue */

#define MAX_QUEUE_LENGTH 5

static GQueue *frame_queue;
static GMutex frame_mutex;
static GCond frame_cond;

static void
queue_frame (FrameData *frame_data)
{
  g_mutex_lock (&frame_mutex);

  while (frame_queue->length == MAX_QUEUE_LENGTH)
    g_cond_wait (&frame_cond, &frame_mutex);

  g_queue_push_tail (frame_queue, frame_data);

  g_mutex_unlock (&frame_mutex);
}

static FrameData *
unqueue_frame (void)
{
  FrameData *frame_data;

  g_mutex_lock (&frame_mutex);

  if (frame_queue->length > 0)
    {
      frame_data = g_queue_pop_head (frame_queue);
      g_cond_signal (&frame_cond);
    }
  else
    {
      frame_data = NULL;
    }

  g_mutex_unlock (&frame_mutex);

  return frame_data;
}

static FrameData *
peek_pending_frame (void)
{
  FrameData *frame_data;

  g_mutex_lock (&frame_mutex);

  if (frame_queue->head)
    frame_data = frame_queue->head->data;
  else
    frame_data = NULL;

  g_mutex_unlock (&frame_mutex);

  return frame_data;
}

static FrameData *
peek_next_frame (void)
{
  FrameData *frame_data;

  g_mutex_lock (&frame_mutex);

  if (frame_queue->head && frame_queue->head->next)
    frame_data = frame_queue->head->next->data;
  else
    frame_data = NULL;

  g_mutex_unlock (&frame_mutex);

  return frame_data;
}

/* Frame producer thread */

static gpointer
create_frames_thread (gpointer data)
{
  int frame_count = 0;

  while (TRUE)
    {
      FrameData *frame_data = g_slice_new0 (FrameData);
      frame_data->angle = 2 * M_PI * (frame_count % fps) / (double)fps;
      frame_data->stream_time = (G_GINT64_CONSTANT (1000000) * frame_count) / fps;

      queue_frame (frame_data);
      frame_count++;
    }

  return NULL;
}

/* Clock management */

#define PRE_BUFFER_TIME 500000

static gint64 start_time;

static gint64
stream_time_to_clock_time (gint64 stream_time)
{
  return start_time + stream_time + PRE_BUFFER_TIME;
}

/* Drawing */

static void
on_window_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GdkRectangle allocation;
  double cx, cy, r;

  cairo_set_source_rgb (cr, 1., 1., 1.);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0., 0., 0.);
  gtk_widget_get_allocation (widget, &allocation);

  cx = allocation.width / 2.;
  cy = allocation.height / 2.;
  r = MIN (allocation.width, allocation.height) / 2.;

  cairo_arc (cr, cx, cy, r,
             0, 2 * M_PI);
  cairo_stroke (cr);
  if (displayed_frame)
    {
      cairo_move_to (cr, cx, cy);
      cairo_line_to (cr,
                     cx + r * cos(displayed_frame->angle - M_PI / 2),
                     cy + r * sin(displayed_frame->angle - M_PI / 2));
      cairo_stroke (cr);

      if (displayed_frame->frame_counter == 0)
        {
          GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (window);
          GdkFrameHistory *history = gdk_frame_clock_get_history (frame_clock);

          displayed_frame->frame_counter = gdk_frame_history_get_frame_counter (history);
        }
    }
}

static void
collect_old_frames (void)
{
  GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (window);
  GdkFrameHistory *history = gdk_frame_clock_get_history (frame_clock);
  GList *l, *l_next;

  for (l = past_frames; l; l = l_next)
    {
      FrameData *frame_data = l->data;
      gboolean remove = FALSE;
      l_next = l->next;

      GdkFrameTimings *timings = gdk_frame_history_get_timings (history,
                                                                frame_data->frame_counter);
      if (timings == NULL)
        {
          remove = TRUE;
        }
      else if (gdk_frame_timings_get_complete (timings))
        {
          gint64 presentation_time = gdk_frame_timings_get_presentation_time (timings);
          if (presentation_time)
            variable_add (&latency_error,
                          presentation_time - frame_data->clock_time);

          remove = TRUE;
        }

      if (remove)
        {
          past_frames = g_list_delete_link (past_frames, l);
          g_slice_free (FrameData, frame_data);
        }
    }
}

static void
print_statistics (void)
{
  gint64 now = g_get_monotonic_time ();
  static gint64 last_print_time = 0;

  if (last_print_time == 0)
    last_print_time = now;
  else if (now -last_print_time > 5000000)
    {
      g_print ("dropped_frames: %d/%d\n",
               dropped_frames, n_frames);
      g_print ("collected_frames: %g/%d\n",
               latency_error.weight, n_frames);
      g_print ("latency_error: %g +/- %g\n",
               variable_mean (&latency_error),
               variable_standard_deviation (&latency_error));
      variable_reset (&latency_error);
      dropped_frames = 0;
      n_frames = 0;
      last_print_time = now;
    }
}

static void
on_update (GdkFrameClock *frame_clock,
           gpointer       data)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_frame_timings (frame_clock);
  gint64 frame_time = gdk_frame_timings_get_frame_time (timings);
  gint64 predicted_presentation_time = gdk_frame_timings_get_predicted_presentation_time (timings);
  gint64 refresh_interval;
  FrameData *pending_frame;

  if (start_time == 0)
    start_time = frame_time;

  gdk_frame_clock_get_refresh_info (frame_clock, frame_time,
                                    &refresh_interval, NULL);

  pending_frame = peek_pending_frame ();
  if (stream_time_to_clock_time (pending_frame->stream_time)
      < predicted_presentation_time + refresh_interval / 2)
    {
      while (TRUE)
        {
          FrameData *next_frame = peek_next_frame ();
          if (next_frame &&
              stream_time_to_clock_time (next_frame->stream_time)
              < predicted_presentation_time + refresh_interval / 2)
            {
              g_slice_free (FrameData, unqueue_frame ());
              n_frames++;
              dropped_frames++;
              pending_frame = next_frame;
            }
          else
            break;
        }

      if (displayed_frame)
        past_frames = g_list_prepend (past_frames, displayed_frame);

      n_frames++;
      displayed_frame = unqueue_frame ();
      displayed_frame->clock_time = stream_time_to_clock_time (displayed_frame->stream_time);
      displayed_frame->frame_counter = gdk_frame_timings_get_frame_counter (timings);

      collect_old_frames ();
      print_statistics ();

      gtk_widget_queue_draw (window);
    }

  gdk_frame_clock_request_phase (frame_clock, GDK_FRAME_CLOCK_PHASE_UPDATE);
}

static GOptionEntry options[] = {
  { "fps", 'f', 0, G_OPTION_ARG_INT, &fps, "Frame rate", "FPS" },
  { NULL }
};

int
main(int argc, char **argv)
{
  GError *error = NULL;
  GdkFrameClock *frame_clock;

  if (!gtk_init_with_args (&argc, &argv, "",
                           options, NULL, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable (window, TRUE);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  g_signal_connect (window, "draw",
                    G_CALLBACK (on_window_draw), NULL);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (window);

  frame_queue = g_queue_new ();
  g_mutex_init (&frame_mutex);
  g_cond_init (&frame_cond);

  g_thread_new ("Create Frames", create_frames_thread, NULL);

  frame_clock = gtk_widget_get_frame_clock (window);
  g_signal_connect (frame_clock, "update",
                    G_CALLBACK (on_update), NULL);
  gdk_frame_clock_request_phase (frame_clock, GDK_FRAME_CLOCK_PHASE_UPDATE);

  gtk_main ();

  return 0;
}
