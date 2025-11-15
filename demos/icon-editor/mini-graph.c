#include <math.h>
#include "mini-graph.h"


static float
apply_easing_params (float *params,
                     float  progress)
{
  static const float epsilon = 0.00001;
  float tmin, t, tmax;
  float x1, y1, x2, y2;

  x1 = params[0];
  y1 = params[1];
  x2 = params[2];
  y2 = params[3];

  if (progress <= 0)
    return 0;
  if (progress >= 1)
    return 1;

  tmin = 0.0;
  tmax = 1.0;
  t = progress;

  while (tmin < tmax)
    {
      float sample;

      sample = (((1.0 + 3 * x1 - 3 * x2) * t
                +      -6 * x1 + 3 * x2) * t
                +       3 * x1         ) * t;
      if (fabsf (sample - progress) < epsilon)
        break;

      if (progress > sample)
        tmin = t;
      else
        tmax = t;
      t = (tmax + tmin) * .5;
    }
  return (((1.0 + 3 * y1 - 3 * y2) * t
          +      -6 * y1 + 3 * y2) * t
          +       3 * y1         ) * t;
}

struct _MiniGraph
{
  GtkWidget parent_instance;

  GpaEasing easing;

  int path_size;
  GskPath *path;
};

static float
compute_value (MiniGraph *self,
               float      t)
{
  static struct {
    float params[4];
  } easing[] = {
    { .params = { 0, 0, 1, 1 } },
    { .params = { 0.42, 0, 0.58, 1 } },
    { .params = { 0.42, 0, 1, 1 } },
    { .params = { 0, 0, 0.58, 1 } },
    { .params = { 0.25, 0.1, 0.25, 1 } },
  };

  return apply_easing_params (easing[self->easing].params, t);
}

static GskPath *
create_path (MiniGraph *self,
             int        size)
{
  GskPathBuilder *builder = gsk_path_builder_new ();

  for (int i = 0; i <= size; i++)
    {
      float x = i / (float) size;
      float y = compute_value (self, x);
      y = CLAMP (y, 0, 1);

      if (i == 0)
        gsk_path_builder_move_to (builder, x * size, (1 - y) * size);
      else
        gsk_path_builder_line_to (builder, x * size, (1 - y) * size);
    }

  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
ensure_path (MiniGraph *self,
             int        size)
{
  if (self->path_size != size)
    g_clear_pointer (&self->path, gsk_path_unref);

  if (!self->path)
    {
      self->path = create_path (self, size);
      self->path_size = size;
    }

  return self->path;
}

static void
mini_graph_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  MiniGraph *self = MINI_GRAPH (widget);
  int width, height, size;
  GskPath *path;
  GskStroke *stroke;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  size = MIN (width, height);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT ((width - size) / 2,
                                                (height - size) / 2));

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 1, 1, 1, 1 },
                             &GRAPHENE_RECT_INIT (0, 0, size, size));
  gtk_snapshot_append_border (snapshot,
                              &GSK_ROUNDED_RECT_INIT (0, 0, size, size),
                              (float [4]) { 1, 1, 1, 1 },
                              (GdkRGBA[4]) {
                                { 0, 0, 0, 0.75 },
                                { 0, 0, 0, 0.75 },
                                { 0, 0, 0, 0.75 },
                                { 0, 0, 0, 0.75 },
                              });

  path = ensure_path (self, size);

  stroke = gsk_stroke_new (2);

  gtk_snapshot_append_stroke (snapshot, path, stroke, &(GdkRGBA) { 0, 0, 1, 1 });

  gsk_stroke_free (stroke);

  gtk_snapshot_restore (snapshot);
}

struct _MiniGraphClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_EASING = 1,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (MiniGraph, mini_graph, GTK_TYPE_WIDGET)

static void
mini_graph_init (MiniGraph *self)
{
  self->easing = GPA_EASING_LINEAR;
}

static void
mini_graph_finalize (GObject *object)
{
  MiniGraph *self = MINI_GRAPH (object);

  g_clear_pointer (&self->path, gsk_path_unref);

  G_OBJECT_CLASS (mini_graph_parent_class)->finalize (object);
}

static void
mini_graph_set_property (GObject      *object,
                         unsigned int  prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  MiniGraph *self = MINI_GRAPH (object);

  switch (prop_id)
    {
    case PROP_EASING:
      mini_graph_set_easing (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mini_graph_get_property (GObject      *object,
                         unsigned int  prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  MiniGraph *self = MINI_GRAPH (object);

  switch (prop_id)
    {
    case PROP_EASING:
      g_value_set_uint (value, self->easing);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mini_graph_class_init (MiniGraphClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = mini_graph_finalize;
  object_class->set_property = mini_graph_set_property;
  object_class->get_property = mini_graph_get_property;

  properties[PROP_EASING] =
    g_param_spec_uint ("easing", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  widget_class->snapshot = mini_graph_snapshot;
}

GtkWidget *
mini_graph_new (void)
{
  return g_object_new (mini_graph_get_type (), NULL);
}

void
mini_graph_set_easing (MiniGraph *self,
                       GpaEasing  easing)
{
  if (self->easing == easing)
    return;

  self->easing = easing;

  g_clear_pointer (&self->path, gsk_path_unref);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EASING]);
}
