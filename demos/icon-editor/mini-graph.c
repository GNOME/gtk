#include <math.h>
#include "mini-graph.h"


static inline float
lerp (float t, float a, float b)
{
  return a + (b - a) * t;
}

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

  EasingFunction easing;
  CalcMode mode;

  int path_size;
  GskPath *path;

  KeyFrame *frames;
  unsigned int n_frames;
};

static struct {
  float params[4];
} easing_funcs[] = {
  { .params = { 0, 0, 1, 1 } },
  { .params = { 0.42, 0, 0.58, 1 } },
  { .params = { 0.42, 0, 1, 1 } },
  { .params = { 0, 0, 0.58, 1 } },
  { .params = { 0.25, 0.1, 0.25, 1 } },
};

static void
update_keyframes (MiniGraph *self)
{
  g_free (self->frames);

  self->frames = g_new (KeyFrame, 2);
  self->n_frames = 2;

  self->frames[0].value = 0;
  self->frames[0].time = 0;
  memcpy (self->frames[0].params, easing_funcs[self->easing].params, 4 * sizeof (float));

  self->frames[1].value = 1;
  self->frames[1].time = 1;
  memcpy (self->frames[1].params, easing_funcs[self->easing].params, 4 * sizeof (float));

  if (self->easing == EASING_FUNCTION_LINEAR)
    self->mode = CALC_MODE_LINEAR;
  else
    self->mode = CALC_MODE_SPLINE;
}

static float
compute_value (MiniGraph *self,
               float      t)
{
  size_t i;
  KeyFrame *kf0 = NULL;
  KeyFrame *kf1 = NULL;
  float t_rel;

  if (self->n_frames == 0)
    return 0;

  for (i = 1; i < self->n_frames; i++)
    {
      kf0 = &self->frames[i - 1];
      kf1 = &self->frames[i];

      if (t < kf1->time)
        break;
    }

  g_assert (kf0 && kf1);

  t_rel = (t - kf0->time) / (kf1->time - kf0->time);

  switch (self->mode)
    {
    case CALC_MODE_DISCRETE:
      return kf0->value;
    case CALC_MODE_LINEAR:
      return lerp (t_rel, kf0->value, kf1->value);
    case CALC_MODE_SPLINE:
      return lerp (apply_easing_params (kf0->params, t_rel), kf0->value, kf1->value);
    default:
      g_assert_not_reached ();
    }
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
  PROP_EASING_FUNCTION = 1,
  PROP_CALC_MODE,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (MiniGraph, mini_graph, GTK_TYPE_WIDGET)

static void
mini_graph_init (MiniGraph *self)
{
  self->easing = EASING_FUNCTION_LINEAR;
  self->mode = CALC_MODE_SPLINE;

  update_keyframes (self);
}

static void
mini_graph_finalize (GObject *object)
{
  MiniGraph *self = MINI_GRAPH (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  g_free (self->frames);

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
    case PROP_EASING_FUNCTION:
      mini_graph_set_easing_function (self, g_value_get_uint (value));
      break;

    case PROP_CALC_MODE:
      mini_graph_set_calc_mode (self, g_value_get_uint (value));
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
    case PROP_EASING_FUNCTION:
      g_value_set_uint (value, self->easing);
      break;

    case PROP_CALC_MODE:
      g_value_set_uint (value, self->mode);
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

  properties[PROP_EASING_FUNCTION] =
    g_param_spec_uint ("easing-function", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_CALC_MODE] =
    g_param_spec_uint ("calc-mode", NULL, NULL,
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
mini_graph_set_params (MiniGraph      *self,
                       EasingFunction  easing,
                       CalcMode        mode,
                       const KeyFrame *frames,
                       unsigned int    n_frames)
{
  if (self->easing == easing &&
      self->mode == mode &&
      self->n_frames == n_frames &&
      memcmp (self->frames, frames, sizeof (KeyFrame) * n_frames) == 0)
    return;

  self->easing = easing;

  if (easing == EASING_FUNCTION_CUSTOM)
    {
      self->mode = mode;
      if (self->frames != frames)
        {
          g_free (self->frames);
          self->frames = g_memdup2 (frames, sizeof (KeyFrame) * n_frames);
          self->n_frames = n_frames;
        }
    }
  else
    update_keyframes (self);

  g_clear_pointer (&self->path, gsk_path_unref);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EASING_FUNCTION]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CALC_MODE]);
}

void
mini_graph_set_easing_function (MiniGraph      *self,
                                EasingFunction  easing)
{
  mini_graph_set_params (self, easing, self->mode, self->frames, self->n_frames);
}

void
mini_graph_set_calc_mode (MiniGraph *self,
                          CalcMode   mode)
{
  mini_graph_set_params (self, self->easing, mode, self->frames, self->n_frames);
}
