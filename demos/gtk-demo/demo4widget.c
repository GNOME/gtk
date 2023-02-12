#include <math.h>
#include "demo4widget.h"
#include "hsla.h"

struct _Demo4Widget
{
  GtkWidget parent_instance;
  PangoLayout *layout;
  GskColorStop stops[8];
  gsize n_stops;

  guint tick;
};

struct _Demo4WidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (Demo4Widget, demo4_widget, GTK_TYPE_WIDGET)

static void
rotate_color (GdkRGBA *rgba)
{
  GdkHSLA hsla;

  _gdk_hsla_init_from_rgba (&hsla, rgba);
  hsla.hue -= 1;
  _gdk_rgba_init_from_hsla (rgba, &hsla);
}

static gboolean
rotate_colors (GtkWidget     *widget,
               GdkFrameClock *clock,
               gpointer       user_data)
{
  Demo4Widget *self = DEMO4_WIDGET (widget);

  for (unsigned int i = 0; i < self->n_stops; i++)
    rotate_color (&self->stops[i].color);

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
demo4_widget_init (Demo4Widget *self)
{
  PangoFontDescription *desc;

  self->n_stops = 8;
  self->stops[0].offset = 0;
  self->stops[0].color = (GdkRGBA) { 1, 0, 0, 1 };

  for (unsigned int i = 1; i < self->n_stops; i++)
    {
      GdkHSLA hsla;

      self->stops[i].offset = i / (double)(self->n_stops - 1);
      _gdk_hsla_init_from_rgba (&hsla, &self->stops[i - 1].color);
      hsla.hue += 360.0 / (double)(self->n_stops - 1);
      _gdk_rgba_init_from_hsla (&self->stops[i].color, &hsla);
    }

  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "123");
  desc = pango_font_description_from_string ("Cantarell Bold 210");
  pango_layout_set_font_description (self->layout, desc);
  pango_font_description_free (desc);

  self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), rotate_colors, NULL, NULL);
}

static void
demo4_widget_dispose (GObject *object)
{
  Demo4Widget *self = DEMO4_WIDGET (object);

  g_clear_object (&self->layout);
  gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);

  G_OBJECT_CLASS (demo4_widget_parent_class)->dispose (object);
}

static void
demo4_widget_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  Demo4Widget *self = DEMO4_WIDGET (widget);
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_push_mask (snapshot);
  gtk_snapshot_append_layout (snapshot, self->layout, &(GdkRGBA) { 0, 0, 0, 1 });
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &GRAPHENE_POINT_INIT (width, height),
                                       self->stops,
                                       self->n_stops);
  gtk_snapshot_pop (snapshot);
}

static void
demo4_widget_class_init (Demo4WidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo4_widget_dispose;

  widget_class->snapshot = demo4_widget_snapshot;
}

GtkWidget *
demo4_widget_new (void)
{
  return g_object_new (DEMO4_TYPE_WIDGET, NULL);
}
