#include <gtk/gtk.h>
#include <glib-unix.h>
#include <libei.h>

#define EVENT_STEP_US 101 /* 16667 / 101 = 165.02 */
#define SIZE_X 165
#define SIZE_Y 33
#define STEP_X 5
#define STEP_Y 15
#define OFFSET_X 2
#define OFFSET_Y 2

static void
coords_from_timestamp (gint64  time_us,
                       gsize  *x,
                       gsize  *y)
{
  time_us /= EVENT_STEP_US;
  time_us %= (SIZE_X * SIZE_Y);

  *x = time_us % SIZE_X;
  *y = time_us / SIZE_X;
}

static gboolean
deadline_timer_ready (GSource *source)
{
  return g_source_get_time (source) >= g_source_get_ready_time (source);
}

static gboolean
deadline_timer_dispatch (GSource    *source, 
                         GSourceFunc callback,
                         gpointer    user_data)
{
  return callback (user_data);
}

static GSourceFuncs my_timeout_source_funcs = {
  NULL,
  deadline_timer_ready,
  deadline_timer_dispatch,
  NULL,
};

static gboolean
send_motion_events (gpointer data)
{
  struct ei_device *device = data;
  GSource *source = g_main_current_source ();
  gint64 t;

  for (t = g_source_get_ready_time (source);
       t <= g_source_get_time (source);
       t += EVENT_STEP_US)
    {
      gint64 coords = (t / EVENT_STEP_US) % (SIZE_X * SIZE_Y);
      
      ei_device_pointer_motion_absolute (device,
                                         STEP_X * (coords % SIZE_X) + OFFSET_X,
                                         STEP_Y * (coords / SIZE_X) + OFFSET_Y);
      ei_device_frame (device, t);
    }

  g_source_set_ready_time (source, t);

  return G_SOURCE_CONTINUE;
}

static void
setup_motion_events (struct ei_device *device)
{
  gint64 now, next;
  GSource *source;

  ei_device_start_emulating (device, 1);

  source = g_source_new (&my_timeout_source_funcs, sizeof (GSource));
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, send_motion_events, ei_device_ref (device), (GDestroyNotify) ei_device_unref);
  g_source_attach (source, g_main_context_get_thread_default ());

  now = g_get_monotonic_time ();
  next = (now + EVENT_STEP_US - 1) / EVENT_STEP_US * EVENT_STEP_US;
  g_source_set_ready_time (source, next);
  g_source_unref (source);
}

static gboolean
ei_source_dispatch (int          fd,
                    GIOCondition condition,
                    gpointer     data)
{
  struct ei *ei = data;
  struct ei_event *e;

  if (condition & (G_IO_ERR | G_IO_HUP))
    return G_SOURCE_REMOVE;

  ei_dispatch (ei);

  for (e = ei_get_event (ei);
       e != NULL;
       e = ei_get_event (ei))
    {
      switch (ei_event_get_type (e))
        {
        case EI_EVENT_DISCONNECT:
          ei_event_unref (e);
          return G_SOURCE_REMOVE;

        case EI_EVENT_SEAT_ADDED:
          ei_seat_bind_capabilities (ei_event_get_seat (e),
                                     EI_DEVICE_CAP_POINTER,
                                     EI_DEVICE_CAP_POINTER_ABSOLUTE,
                                     NULL);
          break;

        case EI_EVENT_DEVICE_RESUMED:
          {
            struct ei_device *device = ei_event_get_device (e);
            
            if (ei_device_has_capability (device, EI_DEVICE_CAP_POINTER_ABSOLUTE))
              {
                setup_motion_events (device);
              }
          }
          break;

        case EI_EVENT_CONNECT:
        case EI_EVENT_SEAT_REMOVED:
        case EI_EVENT_DEVICE_ADDED:
        case EI_EVENT_DEVICE_REMOVED:
        case EI_EVENT_DEVICE_PAUSED:
        case EI_EVENT_KEYBOARD_MODIFIERS:
        case EI_EVENT_PONG:
        case EI_EVENT_SYNC:
        case EI_EVENT_FRAME:
        case EI_EVENT_DEVICE_START_EMULATING:
        case EI_EVENT_DEVICE_STOP_EMULATING:
        case EI_EVENT_POINTER_MOTION:
        case EI_EVENT_POINTER_MOTION_ABSOLUTE:
        case EI_EVENT_BUTTON_BUTTON:
        case EI_EVENT_SCROLL_DELTA:
        case EI_EVENT_SCROLL_STOP:
        case EI_EVENT_SCROLL_CANCEL:
        case EI_EVENT_SCROLL_DISCRETE:
        case EI_EVENT_KEYBOARD_KEY:
        case EI_EVENT_TOUCH_DOWN:
        case EI_EVENT_TOUCH_UP:
        case EI_EVENT_TOUCH_MOTION:
          break;
        default:
          g_assert_not_reached ();
        }

      ei_event_unref (e);
    }

  return G_SOURCE_CONTINUE;
}

static gpointer
send_event_thread (gpointer data)
{
  GDBusConnection *bus;
  GVariant *v;
  GMainContext *context;
  GError *error = NULL;
  char *session;
  GUnixFDList *fdlist;
  int fd, res;
  struct ei *ei;
  GSource *source;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (bus == NULL)
    {
      g_printerr ("Error connecting to session bus: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  v = g_dbus_connection_call_sync (bus,
                                   "org.gnome.Mutter.RemoteDesktop",
                                   "/org/gnome/Mutter/RemoteDesktop",
                                   "org.gnome.Mutter.RemoteDesktop",
                                   "CreateSession",
                                   NULL,
                                   G_VARIANT_TYPE ("(o)"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  if (v == NULL)
    {
      g_printerr ("Failed to connect to org.gnome.Mutter.RemoteDesktop: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  g_variant_get (v, "(o)", &session);
  g_variant_unref (v);

  v = g_dbus_connection_call_with_unix_fd_list_sync (bus,
                                                     "org.gnome.Mutter.RemoteDesktop",
                                                     session,
                                                     "org.gnome.Mutter.RemoteDesktop.Session",
                                                     "ConnectToEIS",
                                                     g_variant_new ("(a{sv})", NULL),
                                                     G_VARIANT_TYPE ("(h)"),
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL,
                                                     &fdlist,
                                                     NULL,
                                                     &error);
  if (v == NULL)
    {
      g_printerr ("No EI support in Mutter: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }
  g_variant_unref (v);
  fd = g_unix_fd_list_get (fdlist, 0, &error);
  if (fd < 0)
    {
      g_printerr ("Could not get EI file descriptor: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  g_object_unref (fdlist);

  ei = ei_new_sender (NULL);
  res = ei_setup_backend_fd (ei, fd);
  if (res != 0)
    {
      g_printerr ("Failed to setup EI: %s\n", g_strerror (res));
      return NULL;
    }

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  source = g_unix_fd_source_new (fd, G_IO_IN | G_IO_ERR | G_IO_HUP);
  g_source_set_callback (source, (GSourceFunc) ei_source_dispatch, ei, (GDestroyNotify) ei_unref);
  g_source_attach (source, context);
  g_source_unref (source);

  v = g_dbus_connection_call_sync (bus,
                                   "org.gnome.Mutter.RemoteDesktop",
                                   session,
                                   "org.gnome.Mutter.RemoteDesktop.Session",
                                   "Start",
                                   NULL,
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  if (v == NULL)
    {
      g_printerr ("Failed to start remote desktop sesssion: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }
  g_variant_unref (v);

  while (TRUE)
    g_main_context_iteration (context, TRUE);
  
  return NULL;
}

typedef struct _LatencyView LatencyView;
typedef struct _LatencyViewPrivate LatencyViewPrivate;

struct _LatencyView
{
  GtkWidget parent_object;

  LatencyViewPrivate *priv;
};

struct _LatencyViewPrivate
{
  GdkTexture *texture;

  struct {
    gint64 event_time;
    GdkRGBA color;
  } cells[SIZE_X][SIZE_Y];
};

G_DECLARE_FINAL_TYPE (LatencyView, latency_view, LATENCY, VIEW, GtkWidget)
G_DEFINE_TYPE (LatencyView, latency_view, GTK_TYPE_WIDGET)

static void
latency_view_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum_size,
                      int            *natural_size,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
#if STEP_X * SIZE_X != STEP_Y * SIZE_Y
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum_size = STEP_X * SIZE_X;
      *natural_size = STEP_X * SIZE_X;
    }
  else
#endif
    {
      *minimum_size = STEP_Y * SIZE_Y;
      *natural_size = STEP_Y * SIZE_Y;
    }
}

static void
snapshot_timestamp (GtkSnapshot *snapshot,
                    gint64       t,
                    const GdkRGBA *color)
{
  gsize x, y;

  coords_from_timestamp (t, &x, &y);

  gtk_snapshot_append_color (snapshot,
                             color,
                             &GRAPHENE_RECT_INIT (x * STEP_X, y * STEP_Y, STEP_X, STEP_Y));
}

static void
latency_view_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  LatencyView *self = LATENCY_VIEW (widget);
  LatencyViewPrivate *priv = self->priv;
  GdkFrameClock *clock = gtk_widget_get_frame_clock (widget);
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 t, now;
  gsize x, y;

  /* checkerboard background */
  gtk_snapshot_push_repeat (snapshot,
                            &GRAPHENE_RECT_INIT (0, 0, STEP_X * SIZE_X, STEP_Y * SIZE_Y),
                            NULL);
  gtk_snapshot_append_scaled_texture (snapshot,
                                      priv->texture,
                                      GSK_SCALING_FILTER_NEAREST,
                                      &GRAPHENE_RECT_INIT (0, 0, 2 * STEP_X, 2 * STEP_Y));
  gtk_snapshot_pop (snapshot);

  /* trail for events */
  now = g_get_monotonic_time ();

  for (y = 0; y < SIZE_Y; y++)
    for (x = 0; x < SIZE_X; x++)
      {
        g_assert (now >= priv->cells[x][y].event_time);
        if (now - priv->cells[x][y].event_time < 500000)
          {
            gtk_snapshot_append_color (snapshot,
                                       &(GdkRGBA) { 1, 1, 1, 1.0 - (float) (now - priv->cells[x][y].event_time) / 500000 },
                                       &GRAPHENE_RECT_INIT (x * STEP_X, y * STEP_Y, STEP_X, STEP_Y));

          }
      }

  t = gdk_frame_timings_get_predicted_presentation_time (timings);
  if (t)
    snapshot_timestamp (snapshot, t, &(GdkRGBA) { 0, 0, 1, 1 });

  t = gdk_frame_clock_get_frame_time (clock);
  snapshot_timestamp (snapshot, t, &(GdkRGBA) { 0, 1, 0, 1 });

  snapshot_timestamp (snapshot, now, &(GdkRGBA) { 1, 0, 0, 1 });
}

static void
latency_view_class_init (LatencyViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = latency_view_measure;
  widget_class->snapshot = latency_view_snapshot;
}

static void
latency_view_motion (GtkEventControllerMotion *controller,
                     double                    x_in,
                     double                    y_in,
                     LatencyView              *self)
{
  LatencyViewPrivate *priv = self->priv;
  gsize x = ((gsize)(x_in / STEP_X) % SIZE_X);
  gsize y = ((gsize)(y_in / STEP_Y) % SIZE_Y);
  guint i, n_coords;
  gint64 t, event_time;
#if 0
  gint64 diff;
  static gint64 min = G_MAXINT64, max = 0, sum = 0, n = 0;
#endif
  GdkEvent *event;
  GdkTimeCoord *timecoords;
  GtkNative *native;
  GdkSurface *surface;
  double surf_x, surf_y;
  
  t = g_get_monotonic_time ();
  event_time = (t / EVENT_STEP_US / SIZE_X / SIZE_Y);
  event_time = ((event_time * SIZE_Y + y) * SIZE_X + x) * EVENT_STEP_US;
  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
  surface = gdk_event_get_surface (event);
  native = gtk_native_get_for_surface (surface);
  gtk_native_get_surface_transform (native, &surf_x, &surf_y);
  g_assert (gdk_event_get_event_type (event) == GDK_MOTION_NOTIFY);
  timecoords = gdk_event_get_history (event, &n_coords);

  for (i = 0; i < n_coords; i++)
    {
      graphene_point_t p;

      if (gtk_widget_compute_point (GTK_WIDGET (native), GTK_WIDGET (self),
                                    &GRAPHENE_POINT_INIT (timecoords[i].axes[GDK_AXIS_X] - surf_x,
                                                          timecoords[i].axes[GDK_AXIS_Y] - surf_y),
                                    &p))
        {
          gsize xpos = ((gsize)(p.x / STEP_X) % SIZE_X);
          gsize ypos = ((gsize)(p.y / STEP_Y) % SIZE_Y);
          priv->cells[xpos][ypos].event_time = 0;
        }
      else
        {
          g_assert_not_reached ();
        }
    }
  priv->cells[x][y].event_time = t;

#if 0
  diff = t - event_time;

  if (diff > 0 && diff < 10 * 1000)
    {
      min = MIN (min, diff);
      max = MAX (max, diff);
      sum += diff;
      n++;

      g_print ("%lld min-/avg/max %lld %lld %lld\n", (long long) diff, (long long) min, (long long) ((sum + n / 2) / n), (long long) max);
    }
#endif
}

static gboolean
latency_view_tick (GtkWidget     *widget,
                   GdkFrameClock *clock,
                   gpointer       user_data)
{
  static gint64 last_presentation_time = 0;
  GdkFrameTimings *timings = gdk_frame_clock_get_timings (clock, gdk_frame_clock_get_frame_counter (clock) - 2);
  gint64 pt = gdk_frame_timings_get_presentation_time (timings);

  gtk_widget_queue_draw (widget);

  g_print ("%u %u\n", (unsigned) (pt - last_presentation_time), (unsigned) gdk_frame_timings_get_refresh_interval (timings));
  last_presentation_time = pt;

  return G_SOURCE_CONTINUE;
}

static void
latency_view_init (LatencyView *self)
{
  LatencyViewPrivate *priv;
  GtkEventController *controller;
  GBytes *bytes;
  static const char data[4 * 4] = {
    0x00, 0x00, 0x00, 0xff,   0x20, 0x20, 0x20, 0xff,
    0x20, 0x20, 0x20, 0xff,   0x00, 0x00, 0x00, 0xff
  };

  self->priv = g_new0 (LatencyViewPrivate, 1);
  priv = self->priv;

  bytes = g_bytes_new_static (data, sizeof (data));
  priv->texture = gdk_memory_texture_new (2, 2,
                                          GDK_MEMORY_R8G8B8A8,
                                          bytes,
                                          2 * 4);
  g_bytes_unref (bytes);

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_static_name (controller, "gtk-text-motion-controller");
  g_signal_connect (controller, "motion", G_CALLBACK (latency_view_motion), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  gtk_widget_add_tick_callback (GTK_WIDGET (self), 
                                latency_view_tick,
                                NULL, NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *view;
  GThread *event_thread;

  gtk_init ();

  event_thread = g_thread_new ("eventpusher",
                               send_event_thread,
                               NULL);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

  view = g_object_new (latency_view_get_type (), NULL);
  gtk_widget_set_halign (view, GTK_ALIGN_START);
  gtk_widget_set_valign (view, GTK_ALIGN_START);

  gtk_window_set_child (GTK_WINDOW (window), view);

  gtk_window_present (GTK_WINDOW (window));
  gtk_window_fullscreen (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  g_thread_join (event_thread);

  return 0;
}
