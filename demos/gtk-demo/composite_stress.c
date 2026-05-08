/* Framebuffer Fetch Stress
 *
 * This demo exercises GtkSnapshot composite nodes in a tight animated grid.
 * Compare the runtime with and without GSK_GPU_DISABLE=framebuffer-fetch.
 */

#include "config.h"

#include <math.h>

#include <gtk/gtk.h>

typedef struct _CompositeStressWidget CompositeStressWidget;
typedef struct _CompositeStressWidgetClass CompositeStressWidgetClass;

#define COMPOSITE_TYPE_STRESS_WIDGET \
  (composite_stress_widget_get_type ())
#define COMPOSITE_STRESS_WIDGET(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), COMPOSITE_TYPE_STRESS_WIDGET, CompositeStressWidget))
#define COMPOSITE_IS_STRESS_WIDGET(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), COMPOSITE_TYPE_STRESS_WIDGET))

struct _CompositeStressWidget
{
  GtkWidget parent_instance;

  GtkLabel *fps_label;
  guint tick_id;
  gint64 start_time;
  double phase;
};

struct _CompositeStressWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CompositeStressWidget, composite_stress_widget, GTK_TYPE_WIDGET)

static const GskPorterDuff composite_ops[] = {
  GSK_PORTER_DUFF_SOURCE,
  GSK_PORTER_DUFF_DEST_OVER_SOURCE,
  GSK_PORTER_DUFF_SOURCE_IN_DEST,
  GSK_PORTER_DUFF_SOURCE_OUT_DEST,
  GSK_PORTER_DUFF_SOURCE_ATOP_DEST,
  GSK_PORTER_DUFF_DEST_ATOP_SOURCE,
  GSK_PORTER_DUFF_XOR,
};

static void
composite_stress_widget_set_fps_label (CompositeStressWidget *self,
                                       GtkLabel              *label)
{
  g_return_if_fail (COMPOSITE_IS_STRESS_WIDGET (self));
  g_return_if_fail (GTK_IS_LABEL (label));

  self->fps_label = label;
  g_object_add_weak_pointer (G_OBJECT (label), (gpointer *) &self->fps_label);
}

static gboolean
composite_stress_widget_tick (GtkWidget     *widget,
                              GdkFrameClock *frame_clock,
                              gpointer       unused G_GNUC_UNUSED)
{
  CompositeStressWidget *self;
  gint64 frame_time;
  gint64 frame;
  gint64 history_start;
  gint64 history_len;
  GdkFrameTimings *previous_timings;
  gint64 previous_frame_time;
  char *s;

  self = COMPOSITE_STRESS_WIDGET (widget);
  frame = gdk_frame_clock_get_frame_counter (frame_clock);
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (self->start_time == 0)
    {
      self->start_time = frame_time;
      if (self->fps_label)
        gtk_label_set_label (self->fps_label, "FPS: ---");
      return G_SOURCE_CONTINUE;
    }

  self->phase = (frame_time - self->start_time) / (double) G_USEC_PER_SEC;
  gtk_widget_queue_draw (widget);

  if (self->fps_label && frame % 30 == 0)
    {
      history_start = gdk_frame_clock_get_history_start (frame_clock);
      history_len = frame - history_start;
      if (history_len > 0)
        {
          previous_timings = gdk_frame_clock_get_timings (frame_clock, frame - history_len);
          if (previous_timings != NULL)
            {
              previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);
              s = g_strdup_printf ("FPS: %-4.1f",
                                   (G_USEC_PER_SEC * history_len) /
                                   (double) (frame_time - previous_frame_time));
              gtk_label_set_label (self->fps_label, s);
              g_free (s);
            }
        }
    }

  return G_SOURCE_CONTINUE;
}

static void
composite_stress_widget_dispose (GObject *object)
{
  CompositeStressWidget *self;

  self = COMPOSITE_STRESS_WIDGET (object);

  if (self->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick_id);
      self->tick_id = 0;
    }

  if (self->fps_label)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->fps_label),
                                    (gpointer *) &self->fps_label);
      self->fps_label = NULL;
    }

  G_OBJECT_CLASS (composite_stress_widget_parent_class)->dispose (object);
}

static void
composite_stress_widget_snapshot (GtkWidget   *widget,
                                  GtkSnapshot *snapshot)
{
  CompositeStressWidget *self;
  const int columns = 12;
  const int rows = 8;
  int width;
  int height;
  int col;
  int row;
  int index;
  double cell_width;
  double cell_height;

  self = COMPOSITE_STRESS_WIDGET (widget);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  cell_width = width / (double) columns;
  cell_height = height / (double) rows;

  for (row = 0; row < rows; row++)
    {
      for (col = 0; col < columns; col++)
        {
          GskPorterDuff op;
          graphene_rect_t cell;
          double t;
          GdkRGBA source;
          GdkRGBA mask;

          index = row * columns + col;
          op = composite_ops[index % G_N_ELEMENTS (composite_ops)];
          cell = GRAPHENE_RECT_INIT (col * cell_width + 4.0,
                                     row * cell_height + 4.0,
                                     cell_width - 8.0,
                                     cell_height - 8.0);
          t = self->phase * 0.85 + index * 0.19;

          source = (GdkRGBA) {
            0.25 + 0.75 * (0.5 + 0.5 * sin (t)),
            0.20 + 0.65 * (0.5 + 0.5 * sin (t + 2.1)),
            0.15 + 0.70 * (0.5 + 0.5 * sin (t + 4.2)),
            0.45 + 0.45 * (0.5 + 0.5 * sin (t + 1.3))
          };

          mask = (GdkRGBA) {
            0.90,
            0.95,
            1.00,
            0.15 + 0.80 * (0.5 + 0.5 * sin (t + 3.4))
          };

          gtk_snapshot_push_composite (snapshot, op);
          gtk_snapshot_append_color (snapshot, &mask, &cell);
          gtk_snapshot_pop (snapshot);
          gtk_snapshot_append_color (snapshot, &source, &cell);
          gtk_snapshot_pop (snapshot);
        }
    }
}

static void
composite_stress_widget_init (CompositeStressWidget *self)
{
  self->phase = 0.0;
  self->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                composite_stress_widget_tick,
                                                NULL,
                                                NULL);
}

static void
composite_stress_widget_class_init (CompositeStressWidgetClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = composite_stress_widget_dispose;
  widget_class->snapshot = composite_stress_widget_snapshot;
}

static GtkWidget *
composite_stress_widget_new (void)
{
  return g_object_new (COMPOSITE_TYPE_STRESS_WIDGET, NULL);
}

GtkWidget *
do_composite_stress (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *outer;
      GtkWidget *body;
      GtkWidget *picture;
      GtkWidget *header;
      GtkWidget *header_box;
      GtkWidget *info;
      GtkWidget *fps;
      GtkWidget *stress;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Framebuffer Fetch Stress");
      gtk_window_set_default_size (GTK_WINDOW (window), 1280, 900);
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      gtk_widget_set_hexpand (outer, TRUE);
      gtk_widget_set_vexpand (outer, TRUE);
      gtk_widget_set_halign (outer, GTK_ALIGN_FILL);
      gtk_widget_set_valign (outer, GTK_ALIGN_FILL);
      gtk_widget_set_margin_start (outer, 12);
      gtk_widget_set_margin_end (outer, 12);
      gtk_widget_set_margin_top (outer, 12);
      gtk_widget_set_margin_bottom (outer, 12);
      gtk_window_set_child (GTK_WINDOW (window), outer);

      header = gtk_frame_new (NULL);
      gtk_widget_set_hexpand (header, TRUE);

      header_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      gtk_widget_set_margin_start (header_box, 12);
      gtk_widget_set_margin_end (header_box, 12);
      gtk_widget_set_margin_top (header_box, 12);
      gtk_widget_set_margin_bottom (header_box, 12);

      info = gtk_label_new ("This scene uses GtkSnapshot composite nodes repeatedly.\n"
                            "Compare with and without GSK_GPU_DISABLE=framebuffer-fetch.");
      gtk_label_set_wrap (GTK_LABEL (info), TRUE);
      gtk_label_set_xalign (GTK_LABEL (info), 0.0);

      fps = gtk_label_new ("FPS: ---");
      gtk_label_set_xalign (GTK_LABEL (fps), 0.0);

      gtk_box_append (GTK_BOX (header_box), info);
      gtk_box_append (GTK_BOX (header_box), fps);
      gtk_frame_set_child (GTK_FRAME (header), header_box);
      gtk_box_append (GTK_BOX (outer), header);

      body = gtk_overlay_new ();
      gtk_widget_set_hexpand (body, TRUE);
      gtk_widget_set_vexpand (body, TRUE);
      gtk_box_append (GTK_BOX (outer), body);

      picture = gtk_picture_new_for_resource ("/transparent/portland-rose.jpg");
      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_COVER);
      gtk_overlay_set_child (GTK_OVERLAY (body), picture);

      stress = composite_stress_widget_new ();
      gtk_widget_set_hexpand (stress, TRUE);
      gtk_widget_set_vexpand (stress, TRUE);
      composite_stress_widget_set_fps_label (COMPOSITE_STRESS_WIDGET (stress),
                                             GTK_LABEL (fps));
      gtk_overlay_add_overlay (GTK_OVERLAY (body), stress);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
