#include "path_explorer.h"

struct _PathExplorer
{
  GtkWidget parent_instance;

  GskPath *path;
  GskPath *segment;
  graphene_point_t pos;
  float start;
  float end;
  graphene_rect_t bounds;
  double zoom;

  gboolean do_fill;
  gboolean do_stroke;

  GdkRGBA fill_color;
  GdkRGBA stroke_color;

  GskStroke *stroke;
  char *dashes;
  GskFillRule fill_rule;

  gboolean show_points;
  gboolean show_bounds;

  graphene_point_t point;
  graphene_vec2_t tangent1;
  graphene_vec2_t tangent2;
  float curvature;
  graphene_point_t center;
  float distance;
};

struct _PathExplorerClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_PATH = 1,
  PROP_FILL,
  PROP_STROKE,
  PROP_LINE_WIDTH,
  PROP_LINE_CAP,
  PROP_LINE_JOIN,
  PROP_MITER_LIMIT,
  PROP_DASHES,
  PROP_DASH_OFFSET,
  PROP_FILL_RULE,
  PROP_SHOW_POINTS,
  PROP_SHOW_BOUNDS,
  PROP_START,
  PROP_END,
  PROP_FILL_COLOR,
  PROP_STROKE_COLOR,
  NUM_PROPERTIES,
};

static GParamSpec *pspecs[NUM_PROPERTIES] = { 0, };

static void
update_bounds (PathExplorer *self)
{
  if (self->do_stroke)
    {
      if (!gsk_path_get_stroke_bounds (self->path, self->stroke, &self->bounds))
        graphene_rect_init (&self->bounds, 0, 0, 0, 0);
    }
  else
    {
      if (!gsk_path_get_bounds (self->path, &self->bounds))
        graphene_rect_init (&self->bounds, 0, 0, 0, 0);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
update_closest_point (PathExplorer *self)
{
  GskPathPoint point;
  float c1, c2;

  gsk_path_get_closest_point (self->segment,
                              &self->pos,
                              INFINITY,
                              &point,
                              &self->distance);
  gsk_path_point_get_position (&point, self->segment, &self->point);
  gsk_path_point_get_tangent (&point, self->segment, GSK_PATH_TO_END, &self->tangent1);
  gsk_path_point_get_tangent (&point, self->segment, GSK_PATH_TO_START, &self->tangent2);
  c1 = gsk_path_point_get_curvature (&point, self->segment, GSK_PATH_FROM_START, &self->center);
  c2 =  gsk_path_point_get_curvature (&point, self->segment, GSK_PATH_FROM_END, &self->center);

  if (c1 == 0 || c2 == 0)
    self->curvature = 0;
  else
    self->curvature = (c1 + c2) / 2;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
update_path (PathExplorer *self)
{
  float start = self->start;
  float end = self->end;

  g_clear_pointer (&self->segment, gsk_path_unref);

  if (start > 0 || end < 1)
    {
      GskPathMeasure *measure;
      GskPathBuilder *builder;
      float length;
      GskPathPoint start_point, end_point;

      measure = gsk_path_measure_new (self->path);
      length = gsk_path_measure_get_length (measure);
      gsk_path_measure_get_point (measure, start * length, &start_point);
      gsk_path_measure_get_point (measure, end * length, &end_point);
      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, self->path, &start_point, &end_point);
      self->segment = gsk_path_builder_free_to_path (builder);
      gsk_path_measure_unref (measure);
    }
  else
    self->segment = gsk_path_ref (self->path);

  update_closest_point (self);
}

static void
path_explorer_set_path (PathExplorer *self,
                        GskPath      *path)
{
  g_clear_pointer (&self->path, gsk_path_unref);
  self->path = gsk_path_ref (path);

  update_path (self);
  update_bounds (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH]);
}

static void
path_explorer_set_line_width (PathExplorer *self,
                              float         line_width)
{
  if (gsk_stroke_get_line_width (self->stroke) == line_width)
    return;

  gsk_stroke_set_line_width (self->stroke, line_width);
  update_bounds (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_LINE_WIDTH]);
}

static void
path_explorer_set_line_cap (PathExplorer *self,
                            GskLineCap    line_cap)
{
  if (gsk_stroke_get_line_cap (self->stroke) == line_cap)
    return;

  gsk_stroke_set_line_cap (self->stroke, line_cap);
  update_bounds (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_LINE_CAP]);
}

static void
path_explorer_set_line_join (PathExplorer *self,
                             GskLineJoin   line_join)
{
  if (gsk_stroke_get_line_join (self->stroke) == line_join)
    return;

  gsk_stroke_set_line_join (self->stroke, line_join);
  update_bounds (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_LINE_JOIN]);
}

static void
path_explorer_set_miter_limit (PathExplorer *self,
                               float         miter_limit)
{
  if (gsk_stroke_get_miter_limit (self->stroke) == miter_limit)
    return;

  gsk_stroke_set_miter_limit (self->stroke, miter_limit);
  update_bounds (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_MITER_LIMIT]);
}

static void
path_explorer_set_fill_rule (PathExplorer *self,
                             GskFillRule   fill_rule)
{
  if (self->fill_rule == fill_rule)
    return;

  self->fill_rule = fill_rule;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FILL_RULE]);
}

static void
path_explorer_set_show_points (PathExplorer *self,
                               gboolean      show_points)
{
  if (self->show_points == show_points)
    return;

  self->show_points = show_points;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_POINTS]);
}

static void
path_explorer_set_show_bounds (PathExplorer *self,
                               gboolean      show_bounds)
{
  if (self->show_bounds == show_bounds)
    return;

  self->show_bounds = show_bounds;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_BOUNDS]);
}

static void
path_explorer_set_start (PathExplorer *self,
                         float         start)
{
  if (self->start == start)
    return;

  self->start = start;
  update_path (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_START]);
}

static void
path_explorer_set_end (PathExplorer *self,
                       float         end)
{
  if (self->end == end)
    return;

  self->end = end;
  update_path (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_END]);
}

static void
path_explorer_set_fill_color (PathExplorer *self,
                              GdkRGBA      *color)
{
  if (gdk_rgba_equal (&self->fill_color, color))
    return;

  self->fill_color = *color;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FILL_COLOR]);
}

static void
path_explorer_set_stroke_color (PathExplorer *self,
                                GdkRGBA      *color)
{
  if (gdk_rgba_equal (&self->stroke_color, color))
    return;

  self->stroke_color = *color;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_STROKE_COLOR]);
}

void
_gsk_stroke_set_dashes (GskStroke  *stroke,
                        const char *dashes)
{
  GArray *d;
  char **strings;

  if (!dashes)
    return;

  d = g_array_new (FALSE, FALSE, sizeof (float));
  strings = g_strsplit (dashes, ",", 0);

  for (unsigned int i = 0; strings[i]; i++)
    {
      char *end = NULL;
      float f;

      f = (float) g_ascii_strtod (g_strstrip (strings[i]), &end);

      if (*end != '\0')
        {
          char *msg = g_strdup_printf ("Failed to parse '%s' as number", strings[i]);
          g_warning ("%s\n", msg);
          g_array_set_size (d, 0);
          break;
        }

      g_array_append_val (d, f);
    }

  g_strfreev (strings);

  gsk_stroke_set_dash (stroke, (const float *)d->data, d->len);

  g_array_unref (d);
}

static void
path_explorer_set_dashes (PathExplorer *self,
                          const char   *dashes)
{
  if (!g_set_str (&self->dashes, dashes))
    return;

  _gsk_stroke_set_dashes (self->stroke, dashes);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_DASHES]);
}

static void
path_explorer_set_dash_offset (PathExplorer *self,
                               float         dash_offset)
{
  if (gsk_stroke_get_dash_offset (self->stroke) == dash_offset)
    return;

  gsk_stroke_set_dash_offset (self->stroke, dash_offset);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_DASH_OFFSET]);
}

G_DEFINE_TYPE (PathExplorer, path_explorer, GTK_TYPE_WIDGET)

static void
motion (GtkEventControllerMotion *controller,
        double                    x,
        double                    y,
        PathExplorer             *self)
{
  self->pos.x = x;
  self->pos.y = y;

  update_closest_point (self);
}

#define ROUNDED_RECT_INIT(_x,_y,_w,_h, cx, cy) \
  (GskRoundedRect) { .bounds = GRAPHENE_RECT_INIT(_x,_y,_w,_h), \
                     .corner = { GRAPHENE_SIZE_INIT(cx, cy),\
                                 GRAPHENE_SIZE_INIT(cx, cy),\
                                 GRAPHENE_SIZE_INIT(cx, cy),\
                                 GRAPHENE_SIZE_INIT(cx, cy), }}

static GskPath *
get_initial_path (void)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (150, 150), 100);
  gsk_path_builder_add_rounded_rect (builder, &ROUNDED_RECT_INIT (90, 90, 120, 120, 20, 20));
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (120, 120, 60, 60));
  gsk_path_builder_move_to (builder, 400, 50);
  gsk_path_builder_cubic_to (builder, 500, 50, 500, 150, 400, 150);
  gsk_path_builder_cubic_to (builder, 300, 150, 300, 250, 400, 250);
  gsk_path_builder_line_to (builder, 400, 350);
  gsk_path_builder_line_to (builder, 350, 270);

  return gsk_path_builder_free_to_path (builder);
}

static void
path_explorer_init (PathExplorer *self)
{
  GtkEventController *controller;

  self->path = get_initial_path ();
  self->segment = gsk_path_ref (self->path);
  self->do_fill = FALSE;
  self->do_stroke = TRUE;
  self->zoom = 1;
  self->stroke = gsk_stroke_new (1);
  self->fill_rule = GSK_FILL_RULE_WINDING;
  self->start = 0;
  self->end = 1;
  self->fill_color = (GdkRGBA) { 1, 1, 0, 1 };
  self->stroke_color = (GdkRGBA) { 0, 0, 0, 1 };

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
path_explorer_dispose (GObject *object)
{
  PathExplorer *self = PATH_EXPLORER (object);

  gsk_path_unref (self->path);
  gsk_path_unref (self->segment);
  gsk_stroke_free (self->stroke);
  g_free (self->dashes);

  G_OBJECT_CLASS (path_explorer_parent_class)->dispose (object);
}

static void
path_explorer_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PathExplorer *self = PATH_EXPLORER (object);

  switch (property_id)
    {
    case PROP_PATH:
      g_value_set_boxed (value, self->path);
      break;

    case PROP_FILL:
      g_value_set_boolean (value, self->do_fill);
      break;

    case PROP_STROKE:
      g_value_set_boolean (value, self->do_stroke);
      break;

    case PROP_LINE_WIDTH:
      g_value_set_float (value, gsk_stroke_get_line_width (self->stroke));
      break;

    case PROP_LINE_CAP:
      g_value_set_enum (value, gsk_stroke_get_line_cap (self->stroke));
      break;

    case PROP_LINE_JOIN:
      g_value_set_enum (value, gsk_stroke_get_line_join (self->stroke));
      break;

    case PROP_MITER_LIMIT:
      g_value_set_float (value, gsk_stroke_get_miter_limit (self->stroke));
      break;

    case PROP_FILL_RULE:
      g_value_set_enum (value, self->fill_rule);
      break;

    case PROP_SHOW_POINTS:
      g_value_set_boolean (value, self->show_points);
      break;

    case PROP_SHOW_BOUNDS:
      g_value_set_boolean (value, self->show_bounds);
      break;

    case PROP_START:
      g_value_set_float (value, self->start);
      break;

    case PROP_END:
      g_value_set_float (value, self->end);
      break;

    case PROP_FILL_COLOR:
      g_value_set_boxed (value, &self->fill_color);
      break;

    case PROP_STROKE_COLOR:
      g_value_set_boxed (value, &self->stroke_color);
      break;

    case PROP_DASHES:
      g_value_set_string (value, self->dashes ? self->dashes : "");
      break;

    case PROP_DASH_OFFSET:
      g_value_set_float (value, gsk_stroke_get_dash_offset (self->stroke));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
path_explorer_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PathExplorer *self = PATH_EXPLORER (object);

  switch (property_id)
    {
    case PROP_PATH:
      path_explorer_set_path (self, g_value_get_boxed (value));
      break;

    case PROP_FILL:
      self->do_fill = g_value_get_boolean (value);
      update_bounds (self);
      break;

    case PROP_STROKE:
      self->do_stroke = g_value_get_boolean (value);
      update_bounds (self);
      break;

    case PROP_LINE_WIDTH:
      path_explorer_set_line_width (self, g_value_get_float (value));
      break;

    case PROP_LINE_CAP:
      path_explorer_set_line_cap (self, g_value_get_enum (value));
      break;

    case PROP_LINE_JOIN:
      path_explorer_set_line_join (self, g_value_get_enum (value));
      break;

    case PROP_MITER_LIMIT:
      path_explorer_set_miter_limit (self, g_value_get_float (value));
      break;

    case PROP_FILL_RULE:
      path_explorer_set_fill_rule (self, g_value_get_enum (value));
      break;

    case PROP_SHOW_POINTS:
      path_explorer_set_show_points (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_BOUNDS:
      path_explorer_set_show_bounds (self, g_value_get_boolean (value));
      break;

    case PROP_START:
      path_explorer_set_start (self, g_value_get_float (value));
      break;

    case PROP_END:
      path_explorer_set_end (self, g_value_get_float (value));
      break;

    case PROP_FILL_COLOR:
      path_explorer_set_fill_color (self, g_value_get_boxed (value));
      break;

    case PROP_STROKE_COLOR:
      path_explorer_set_stroke_color (self, g_value_get_boxed (value));
      break;

    case PROP_DASHES:
      path_explorer_set_dashes (self, g_value_get_string (value));
      break;

    case PROP_DASH_OFFSET:
      path_explorer_set_dash_offset (self, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
path_explorer_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  PathExplorer *self = PATH_EXPLORER (widget);
  float size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = self->zoom * (self->bounds.origin.x + self->bounds.size.width);
  else
    size = self->zoom * (self->bounds.origin.y + self->bounds.size.height);

  *minimum = *natural = (int)size;
}

static void
draw_point (GtkSnapshot            *snapshot,
            const graphene_point_t *pt)
{
  graphene_rect_t bounds;
  GdkRGBA color = { 1, 0, 0, 1 };

  bounds.origin.x = pt->x - 2;
  bounds.origin.y = pt->y - 2;
  bounds.size.width = 4;
  bounds.size.height = 4;

  gtk_snapshot_append_color (snapshot, &color, &bounds);
}

static gboolean
point_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_ts,
          float                   weight,
          gpointer                user_data)
{
  GtkSnapshot *snapshot = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      draw_point (snapshot, &pts[0]);
      break;

    case GSK_PATH_CLOSE:
      draw_point (snapshot, &pts[0]);
      break;

    case GSK_PATH_LINE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      draw_point (snapshot, &pts[2]);
      break;

    case GSK_PATH_CUBIC:
      draw_point (snapshot, &pts[3]);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
path_explorer_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  PathExplorer *self = PATH_EXPLORER (widget);
  int width, height;

  if (!self->path)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_scale (snapshot, self->zoom, self->zoom);

  if (self->do_fill)
    {
      gtk_snapshot_push_fill (snapshot, self->segment, self->fill_rule);
      gtk_snapshot_append_color (snapshot,
                                 &self->fill_color,
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
    }

  if (self->do_stroke)
    {
      gtk_snapshot_push_stroke (snapshot, self->segment, self->stroke);
      gtk_snapshot_append_color (snapshot,
                                 &self->stroke_color,
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
    }

  if (self->show_bounds)
    {
      GskPathBuilder *builder;
      GskPath *path;
      GskStroke *stroke;

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_rect (builder, &self->bounds);
      path = gsk_path_builder_free_to_path (builder);

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, path, stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.5},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_stroke_free (stroke);
      gsk_path_unref (path);
    }

  if (self->show_points)
    {
      gsk_path_foreach (self->segment, -1, point_cb, snapshot);
    }

  if (self->distance < 40)
    {
      graphene_point_t p1, p2;
      GskPathBuilder *builder;
      GskPath *path;
      GskStroke *stroke;

      p1.x = self->point.x + graphene_vec2_get_x (&self->tangent1) * 40;
      p1.y = self->point.y + graphene_vec2_get_y (&self->tangent1) * 40;

      p2.x = self->point.x + graphene_vec2_get_x (&self->tangent2) * 40;
      p2.y = self->point.y + graphene_vec2_get_y (&self->tangent2) * 40;

      builder = gsk_path_builder_new ();

      gsk_path_builder_move_to (builder, p1.x, p1.y);
      gsk_path_builder_line_to (builder, self->point.x, self->point.y);
      gsk_path_builder_line_to (builder, p2.x, p2.y);

      path = gsk_path_builder_free_to_path (builder);

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_unref (path);

      builder = gsk_path_builder_new ();

      gsk_path_builder_add_circle (builder, &self->point, 5);
      gsk_path_builder_add_circle (builder, &p1, 2.5);
      gsk_path_builder_add_circle (builder, &p2, 2.5);

      path = gsk_path_builder_free_to_path (builder);

      gtk_snapshot_push_fill (snapshot, path, 0);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 1, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_unref (path);

      if (self->curvature != 0)
        {
          builder = gsk_path_builder_new ();
          gsk_path_builder_add_circle (builder, &self->center, fabs (1/self->curvature));
          gsk_path_builder_add_circle (builder, &self->center, 3);

          path = gsk_path_builder_free_to_path (builder);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 1, 0, 1, 1},
                                     &GRAPHENE_RECT_INIT (0, 0, width, height ));

          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }

  gtk_snapshot_restore (snapshot);
}

static void
path_explorer_class_init (PathExplorerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = path_explorer_dispose;
  object_class->get_property = path_explorer_get_property;
  object_class->set_property = path_explorer_set_property;

  widget_class->measure = path_explorer_measure;
  widget_class->snapshot = path_explorer_snapshot;

  pspecs[PROP_PATH] = g_param_spec_boxed ("path", NULL, NULL,
                                          GSK_TYPE_PATH,
                                          G_PARAM_READWRITE);

  pspecs[PROP_FILL] = g_param_spec_boolean ("fill", NULL, NULL,
                                            FALSE,
                                            G_PARAM_READWRITE);

  pspecs[PROP_STROKE] = g_param_spec_boolean ("stroke", NULL, NULL,
                                              TRUE,
                                              G_PARAM_READWRITE);

  pspecs[PROP_LINE_WIDTH] = g_param_spec_float ("line-width", NULL, NULL,
                                                0, G_MAXFLOAT, 1,
                                                G_PARAM_READWRITE);

  pspecs[PROP_LINE_CAP] = g_param_spec_enum ("line-cap", NULL, NULL,
                                             GSK_TYPE_LINE_CAP, GSK_LINE_CAP_BUTT,
                                             G_PARAM_READWRITE);

  pspecs[PROP_LINE_JOIN] = g_param_spec_enum ("line-join", NULL, NULL,
                                              GSK_TYPE_LINE_JOIN, GSK_LINE_JOIN_MITER,
                                              G_PARAM_READWRITE);

  pspecs[PROP_MITER_LIMIT] = g_param_spec_float ("miter-limit", NULL, NULL,
                                                 0, G_MAXFLOAT, 4,
                                                 G_PARAM_READWRITE);

  pspecs[PROP_DASHES] = g_param_spec_string ("dashes", NULL, NULL,
                                             NULL,
                                             G_PARAM_READWRITE);

  pspecs[PROP_DASH_OFFSET] = g_param_spec_float ("dash-offset", NULL, NULL,
                                                 0, G_MAXFLOAT, 0,
                                                 G_PARAM_READWRITE);

  pspecs[PROP_FILL_RULE] = g_param_spec_enum ("fill-rule", NULL, NULL,
                                              GSK_TYPE_FILL_RULE, GSK_FILL_RULE_WINDING,
                                              G_PARAM_READWRITE);

  pspecs[PROP_SHOW_POINTS] = g_param_spec_boolean ("show-points", NULL, NULL,
                                                   FALSE,
                                                   G_PARAM_READWRITE);

  pspecs[PROP_SHOW_BOUNDS] = g_param_spec_boolean ("show-bounds", NULL, NULL,
                                                   FALSE,
                                                   G_PARAM_READWRITE);

  pspecs[PROP_START] = g_param_spec_float ("start", NULL, NULL,
                                           0, 1, 0,
                                           G_PARAM_READWRITE);

  pspecs[PROP_END] = g_param_spec_float ("end", NULL, NULL,
                                         0, 1, 1,
                                         G_PARAM_READWRITE);

  pspecs[PROP_FILL_COLOR] = g_param_spec_boxed ("fill-color", NULL, NULL,
                                                GDK_TYPE_RGBA,
                                                G_PARAM_READWRITE);

  pspecs[PROP_STROKE_COLOR] = g_param_spec_boxed ("stroke-color", NULL, NULL,
                                                  GDK_TYPE_RGBA,
                                                  G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, pspecs);
}

GtkWidget *
path_explorer_new (void)
{
  return g_object_new (path_explorer_get_type (), NULL);
}
