#include <gtk/gtk.h>

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())
G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;

  guint tick_cb;
  guint64 start_time;
  guint64 stop_time;

  char *text;

  float angle;
  float size;
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  guint64 now;

  now = gdk_frame_clock_get_frame_time (frame_clock);

  if (self->start_time == 0)
    self->start_time = now;

  self->angle = 360 * (now - self->start_time) / (double)(G_TIME_SPAN_SECOND * 10);
  self->size = 150 + 130 * sin (2 * G_PI * (now - self->start_time) / (double)(G_TIME_SPAN_SECOND * 5));

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static gboolean
pressed_cb (GtkEventController *controller,
            guint               keyval,
            guint               keycode,
            GdkModifierType     state,
            gpointer            data)
{
  DemoWidget *self = (DemoWidget *)gtk_event_controller_get_widget (controller);

  if (keyval == GDK_KEY_space)
    {
      GdkFrameClock *frame_clock;
      guint64 now;

      frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));
      now = gdk_frame_clock_get_frame_time (frame_clock);

      if (self->tick_cb)
        {
          gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_cb);
          self->tick_cb = 0;
          self->stop_time = now;
        }
      else
        {
          self->start_time += now - self->stop_time;
          self->tick_cb = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);
        }
    }

  return TRUE;
}

static void
demo_widget_init (DemoWidget *self)
{
  GtkEventController *controller;

  self->start_time = 0;
  self->tick_cb = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb, NULL, NULL);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (pressed_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  PangoLayout *layout;
  int width, height;
  int pwidth, pheight;
  PangoFontDescription *desc;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_save (snapshot);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), self->text);
  desc = pango_font_description_new ();
  pango_font_description_set_family (desc, "Cantarell");
  pango_font_description_set_weight (desc, 520);
  pango_font_description_set_size (desc, self->size * PANGO_SCALE);
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);
  pango_layout_get_pixel_size (layout, &pwidth, &pheight);

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0.5 * width, 0.5 * height));
  gtk_snapshot_rotate (snapshot, self->angle);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- 0.5 * pwidth, - 0.5 * pheight));

  gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA) { 0, 0, 0, 1});

  g_object_unref (layout);

  gtk_snapshot_restore (snapshot);
}

static void
demo_widget_dispose (GObject *object)
{
  DemoWidget *self = DEMO_WIDGET (object);

  g_free (self->text);

  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;

  widget_class->snapshot = demo_widget_snapshot;
}

static GtkWidget *
demo_widget_new (const char *text,
                 gsize       length)
{
  DemoWidget *demo;

  demo = g_object_new (DEMO_TYPE_WIDGET, NULL);

  demo->text = g_strndup (text, length);

  return GTK_WIDGET (demo);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *demo;
  char *text;
  gsize length;

  gtk_init ();

  window = gtk_window_new ();

  if (argc > 1)
    {
      GError *error = NULL;

      if (!g_file_get_contents (argv[1], &text, &length, &error))
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          text = NULL;
        }
    }

  if (!text)
    {
      text = g_strdup ("Best Aa");
      length = strlen (text);
    }

  demo = demo_widget_new (text, length);
  gtk_window_set_child (GTK_WINDOW (window), demo);

  g_free (text);

  gtk_widget_show (window);
  gtk_widget_grab_focus (demo);

  while (1)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
