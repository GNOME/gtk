#include "config.h"

#include "glyph-demo.h"

#include <gtk/gtk.h>

#include <hb-ot.h>

#define MIN_ZOOM 0.3
#define MAX_ZOOM 5
#define DEFAULT_ZOOM 1
#define ZOOM_STEP 0.1

typedef enum
{
  GLYPH_DEMO_ORIGINAL,
  GLYPH_DEMO_REVERSE,
  GLYPH_DEMO_SIMPLIFIED,
  GLYPH_DEMO_UNION,
  GLYPH_DEMO_INTERSECTION,
  GLYPH_DEMO_DIFFERENCE,
  GLYPH_DEMO_SYMMETRIC_DIFFERENCE
} GlyphDemoOperation;

#define GLYPH_TYPE_DEMO_OPERATION (glyph_demo_operation_get_type ())
G_DEFINE_ENUM_TYPE (GlyphDemoOperation, glyph_demo_operation,
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_ORIGINAL, "original"),
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_REVERSE, "reverse"),
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_SIMPLIFIED, "simplified"),
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_UNION, "union"),
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_INTERSECTION, "intersection"),
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_DIFFERENCE, "difference"),
  G_DEFINE_ENUM_VALUE (GLYPH_DEMO_SYMMETRIC_DIFFERENCE, "symmetric-difference"))

struct _GlyphDemo
{
  GtkWidget parent_instance;
  GskPath *orig_path1;
  GskPath *orig_path2;
  GskPath *path;
  GskPath *control_path;
  graphene_point_t point;
  graphene_point_t point2;
  graphene_vec2_t tangent;

  GlyphDemoOperation operation;
  gboolean show_outline;
  gboolean show_fill;
  gboolean show_points;
  gboolean show_controls;
  gboolean show_bounds;
  GskFillRule fill_rule;

  graphene_rect_t bounds;

  GskPath *short_path;
  guint outline_step;

  double zoom;
};

struct _GlyphDemoClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_ZOOM = 1,
  PROP_FILL_RULE,
  PROP_SHOW_FILL,
  PROP_SHOW_OUTLINE,
  PROP_SHOW_POINTS,
  PROP_SHOW_CONTROLS,
  PROP_SHOW_BOUNDS,
  PROP_OPERATION,
  PROP_PATH1,
  PROP_PATH2,
  PROP_PATH,
  PROP_OUTLINE_STEP,
  NUM_PROPERTIES,
};

static GParamSpec *pspecs[NUM_PROPERTIES] = { 0, };

static void
glyph_demo_set_zoom (GlyphDemo *self,
                     double     zoom)
{
  zoom = CLAMP (zoom, MIN_ZOOM, MAX_ZOOM);

  if (self->zoom == zoom)
    return;

  self->zoom = zoom;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_ZOOM]);
}

static void
glyph_demo_set_fill_rule (GlyphDemo   *self,
                          GskFillRule  fill_rule)
{
  if (self->fill_rule == fill_rule)
    return;

  self->fill_rule = fill_rule;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FILL_RULE]);
}

static void
glyph_demo_set_show_fill (GlyphDemo *self,
                          gboolean   show_fill)
{
  if (self->show_fill == show_fill)
    return;

  self->show_fill = show_fill;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_FILL]);
}

static void
glyph_demo_set_show_outline (GlyphDemo *self,
                             gboolean   show_outline)
{
  if (self->show_outline == show_outline)
    return;

  self->show_outline = show_outline;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_OUTLINE]);
}

static void
glyph_demo_set_show_points (GlyphDemo *self,
                            gboolean   show_points)
{
  if (self->show_points == show_points)
    return;

  self->show_points = show_points;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_POINTS]);
}

static void
glyph_demo_set_show_controls (GlyphDemo *self,
                             gboolean    show_controls)
{
  if (self->show_controls == show_controls)
    return;

  self->show_controls = show_controls;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_CONTROLS]);
}

static void
glyph_demo_set_show_bounds (GlyphDemo *self,
                            gboolean    show_bounds)
{
  if (self->show_bounds == show_bounds)
    return;

  self->show_bounds = show_bounds;
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_SHOW_BOUNDS]);
}

static void init_demo_from_paths (GlyphDemo *self);

static void
glyph_demo_set_operation (GlyphDemo          *self,
                          GlyphDemoOperation  operation)
{
  if (self->operation == operation)
    return;

  self->operation = operation;

  init_demo_from_paths (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_OPERATION]);
}

static void
glyph_demo_set_path1 (GlyphDemo *self,
                      GskPath   *path)
{
  g_clear_pointer (&self->orig_path1, gsk_path_unref);
  self->orig_path1 = gsk_path_ref (path);

  init_demo_from_paths (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH1]);
}

static void
glyph_demo_set_path2 (GlyphDemo *self,
                      GskPath   *path)
{
  g_clear_pointer (&self->orig_path2, gsk_path_unref);
  self->orig_path2 = gsk_path_ref (path);

  init_demo_from_paths (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH2]);
}

static void update_short_path (GlyphDemo *self);

static void
glyph_demo_set_outline_step (GlyphDemo *self,
                             guint      outline_step)
{
  if (self->outline_step == outline_step)
    return;

  self->outline_step = outline_step;

  update_short_path (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_OUTLINE_STEP]);
}


G_DEFINE_TYPE (GlyphDemo, glyph_demo, GTK_TYPE_WIDGET)

static int short_count;

static gboolean
short_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_pts,
          float                   weight,
          gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  if (short_count <= 0)
    return FALSE;

  short_count--;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
      break;
    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
update_short_path (GlyphDemo *self)
{
  GskPathBuilder *builder;

  g_clear_pointer (&self->short_path, gsk_path_unref);

  builder = gsk_path_builder_new ();

  short_count = self->outline_step;
  gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC, short_cb, builder);
  self->short_path = gsk_path_builder_free_to_path (builder);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pressed_cb (GtkEventController *controller,
            guint               keyval,
            guint               keycode,
            GdkModifierType     state,
            gpointer            data)
{
  GlyphDemo *self = (GlyphDemo *)gtk_event_controller_get_widget (controller);
  guint step = self->outline_step;

  if (keyval == GDK_KEY_BackSpace)
    step = step - 1;
  else if (keyval == GDK_KEY_Return)
    step = step + 1;

  glyph_demo_set_outline_step (self, step);
}

static gboolean
zoom_in (GtkWidget *widget,
         GVariant  *args,
         gpointer   user_data)
{
  GlyphDemo *self = (GlyphDemo *)widget;

  glyph_demo_set_zoom (self, self->zoom + ZOOM_STEP);
  return TRUE;
}

static gboolean
zoom_out (GtkWidget *widget,
          GVariant  *args,
          gpointer   user_data)
{
  GlyphDemo *self = (GlyphDemo *)widget;

  glyph_demo_set_zoom (self, self->zoom - ZOOM_STEP);
  return TRUE;
}

static void
glyph_demo_init (GlyphDemo *self)
{
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;

  self->zoom = DEFAULT_ZOOM;
  self->fill_rule = GSK_FILL_RULE_WINDING;
  self->show_outline = TRUE;
  self->show_fill = FALSE;
  self->show_points = FALSE;
  self->show_controls = FALSE;
  self->show_bounds = FALSE;
  self->operation = GLYPH_DEMO_ORIGINAL;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (pressed_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller), GTK_SHORTCUT_SCOPE_GLOBAL);

  trigger = gtk_keyval_trigger_new (GDK_KEY_plus, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (zoom_in, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_minus, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (zoom_out, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
draw_point2 (GtkSnapshot            *snapshot,
             const graphene_point_t *pt,
             GdkRGBA                *color,
             float                   size)
{
  graphene_rect_t bounds;

  bounds.origin.x = pt->x - size / 2;
  bounds.origin.y = pt->y - size / 2;
  bounds.size.width = size;
  bounds.size.height = size;

  gtk_snapshot_append_color (snapshot, color, &bounds);
}

static void
draw_point (GtkSnapshot            *snapshot,
            const graphene_point_t *pt)
{
  draw_point2 (snapshot, pt, &(GdkRGBA) { 1, 0, 0, 1 }, 4);
}

static gboolean
curve_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_ts,
          float                   weight,
          gpointer                user_data)
{
  GtkSnapshot *snapshot = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      break;

    case GSK_PATH_CLOSE:
      draw_point (snapshot, &pts[0]);
      break;

    case GSK_PATH_LINE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_QUAD:
      draw_point (snapshot, &pts[1]);
      draw_point (snapshot, &pts[2]);
      break;

    case GSK_PATH_CUBIC:
      draw_point (snapshot, &pts[1]);
      draw_point (snapshot, &pts[2]);
      draw_point (snapshot, &pts[3]);
      break;

    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
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
      draw_point2 (snapshot, &pts[0], &(GdkRGBA) { 0, 0, 1, 1 }, 8);
      break;

    case GSK_PATH_CLOSE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_LINE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_QUAD:
      draw_point (snapshot, &pts[2]);
      break;

    case GSK_PATH_CUBIC:
      draw_point (snapshot, &pts[3]);
      break;

    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
glyph_demo_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GlyphDemo *self = GLYPH_DEMO (widget);
  int width, height;

  if (!self->path)
    return;

  gtk_snapshot_save (snapshot);

  gtk_snapshot_scale (snapshot, self->zoom, self->zoom);

  width = self->bounds.origin.x + self->bounds.size.width + 10;
  height =self->bounds.origin.y + self->bounds.size.height + 10;

  if (self->show_fill)
    {
      gtk_snapshot_push_fill (snapshot, self->path, self->fill_rule);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 1, 0, 1, 0.2},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);
    }

  if (self->show_outline)
    {
      GskStroke *stroke;

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, self->orig_path1, stroke);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.3 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_push_stroke (snapshot, self->orig_path2, stroke);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.3 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
      gsk_stroke_free (stroke);
   }

  if (1)
    {
      GskStroke *stroke;

      stroke = gsk_stroke_new (2.0);
      gtk_snapshot_push_stroke (snapshot, self->short_path, stroke);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1.0 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
      gsk_stroke_free (stroke);
    }

  if (self->show_controls)
    {
      GskStroke *stroke;

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, self->control_path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 1, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC, curve_cb, snapshot);
    }
  else if (self->show_points)
    {
      gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC, point_cb, snapshot);
    }

  if (self->show_bounds)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_bounds (self->path, &bounds))
        {
          GskPathBuilder *builder;
          GskPath *path;
          GskStroke *stroke;

          builder = gsk_path_builder_new ();

          gsk_path_builder_add_rect (builder, &bounds);

          path = gsk_path_builder_free_to_path (builder);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 0, 0, 0, 0.5},
                                     &GRAPHENE_RECT_INIT (0, 0, width, height ));

          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }

  gtk_snapshot_restore (snapshot);
}

static void
glyph_demo_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GlyphDemo *self = GLYPH_DEMO (widget);
  float size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = self->zoom * (self->bounds.origin.x + self->bounds.size.width + 10);
  else
    size = self->zoom * (self->bounds.origin.y + self->bounds.size.height + 10);

  *minimum = *natural = (int)size;
}

static void
glyph_demo_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GlyphDemo *self = GLYPH_DEMO (object);

  switch (prop_id)
    {
    case PROP_ZOOM:
      g_value_set_double (value, self->zoom);
      break;

    case PROP_FILL_RULE:
      g_value_set_enum (value, self->fill_rule);
      break;

    case PROP_SHOW_FILL:
      g_value_set_boolean (value, self->show_fill);
      break;

    case PROP_SHOW_OUTLINE:
      g_value_set_boolean (value, self->show_outline);
      break;

    case PROP_SHOW_POINTS:
      g_value_set_boolean (value, self->show_points);
      break;

    case PROP_SHOW_CONTROLS:
      g_value_set_boolean (value, self->show_controls);
      break;

    case PROP_SHOW_BOUNDS:
      g_value_set_boolean (value, self->show_bounds);
      break;

    case PROP_OPERATION:
      g_value_set_enum (value, self->operation);
      break;

    case PROP_PATH1:
      g_value_set_boxed (value, self->orig_path1);
      break;

    case PROP_PATH2:
      g_value_set_boxed (value, self->orig_path2);
      break;

    case PROP_PATH:
      g_value_set_boxed (value, self->path);
      break;

    case PROP_OUTLINE_STEP:
      g_value_set_uint (value, self->outline_step);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glyph_demo_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GlyphDemo *self = GLYPH_DEMO (object);

  switch (prop_id)
    {
    case PROP_ZOOM:
      glyph_demo_set_zoom (self, g_value_get_double (value));
      break;

    case PROP_FILL_RULE:
      glyph_demo_set_fill_rule (self, g_value_get_enum (value));
      break;

    case PROP_SHOW_FILL:
      glyph_demo_set_show_fill (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_OUTLINE:
      glyph_demo_set_show_outline (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_POINTS:
      glyph_demo_set_show_points (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_CONTROLS:
      glyph_demo_set_show_controls (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_BOUNDS:
      glyph_demo_set_show_bounds (self, g_value_get_boolean (value));
      break;

    case PROP_OPERATION:
      glyph_demo_set_operation (self, g_value_get_enum (value));
      break;

    case PROP_PATH1:
      glyph_demo_set_path1 (self, g_value_get_boxed (value));
      break;

    case PROP_PATH2:
      glyph_demo_set_path2 (self, g_value_get_boxed (value));
      break;

    case PROP_OUTLINE_STEP:
      glyph_demo_set_outline_step (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glyph_demo_dispose (GObject *object)
{
  GlyphDemo *self = GLYPH_DEMO (object);

  g_clear_pointer (&self->orig_path1, gsk_path_unref);
  g_clear_pointer (&self->orig_path2, gsk_path_unref);
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->control_path, gsk_path_unref);
  g_clear_pointer (&self->short_path, gsk_path_unref);

  G_OBJECT_CLASS (glyph_demo_parent_class)->dispose (object);
  g_clear_pointer (&self->path, gsk_path_unref);
}

static void
glyph_demo_class_init (GlyphDemoClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = glyph_demo_dispose;
  object_class->set_property = glyph_demo_set_property;
  object_class->get_property = glyph_demo_get_property;

  widget_class->snapshot = glyph_demo_snapshot;
  widget_class->measure = glyph_demo_measure;

  pspecs[PROP_ZOOM] = g_param_spec_double ("zoom", NULL, NULL,
                                           MIN_ZOOM, MAX_ZOOM, DEFAULT_ZOOM,
                                           G_PARAM_READWRITE);
  pspecs[PROP_FILL_RULE] = g_param_spec_enum ("fill-rule", NULL, NULL,
                                              GSK_TYPE_FILL_RULE, GSK_FILL_RULE_WINDING,
                                              G_PARAM_READWRITE);
  pspecs[PROP_SHOW_FILL] = g_param_spec_boolean ("show-fill", NULL, NULL,
                                                 FALSE, G_PARAM_READWRITE);
  pspecs[PROP_SHOW_OUTLINE] = g_param_spec_boolean ("show-outline", NULL, NULL,
                                                    TRUE, G_PARAM_READWRITE);
  pspecs[PROP_SHOW_POINTS] = g_param_spec_boolean ("show-points", NULL, NULL,
                                                   FALSE, G_PARAM_READWRITE);
  pspecs[PROP_SHOW_CONTROLS] = g_param_spec_boolean ("show-controls", NULL, NULL,
                                                     FALSE, G_PARAM_READWRITE);
  pspecs[PROP_SHOW_BOUNDS] = g_param_spec_boolean ("show-bounds", NULL, NULL,
                                                   FALSE, G_PARAM_READWRITE);
  pspecs[PROP_OPERATION] = g_param_spec_enum ("operation", NULL, NULL,
                                              GLYPH_TYPE_DEMO_OPERATION, GLYPH_DEMO_ORIGINAL,
                                              G_PARAM_READWRITE);
  pspecs[PROP_PATH1] = g_param_spec_boxed ("path1", NULL, NULL,
                                           GSK_TYPE_PATH, G_PARAM_READWRITE);
  pspecs[PROP_PATH2] = g_param_spec_boxed ("path2", NULL, NULL,
                                           GSK_TYPE_PATH, G_PARAM_READWRITE);
  pspecs[PROP_PATH] = g_param_spec_boxed ("path", NULL, NULL,
                                          GSK_TYPE_PATH, G_PARAM_READABLE);
  pspecs[PROP_OUTLINE_STEP] = g_param_spec_uint ("outline-step", NULL, NULL,
                                                 0, G_MAXUINT, 0, G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, pspecs);
}

GtkWidget *
glyph_demo_new (void)
{
  return g_object_new (glyph_demo_get_type (), NULL);
}

static gboolean
control_cb (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_QUAD:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      gsk_path_builder_line_to (builder, pts[2].x, pts[2].y);
      break;
    case GSK_PATH_CUBIC:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      gsk_path_builder_line_to (builder, pts[2].x, pts[2].y);
      gsk_path_builder_line_to (builder, pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
init_demo_from_paths (GlyphDemo *self)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_rect_t bounds;

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->control_path, gsk_path_unref);
  g_clear_pointer (&self->short_path, gsk_path_unref);

  switch (self->operation)
    {
    case GLYPH_DEMO_ORIGINAL:
      builder = gsk_path_builder_new ();
      if (self->orig_path1)
        gsk_path_builder_add_path (builder, self->orig_path1);
      if (self->orig_path2)
        gsk_path_builder_add_path (builder, self->orig_path2);
      self->path = gsk_path_builder_free_to_path (builder);
      break;
    case GLYPH_DEMO_REVERSE:
      builder = gsk_path_builder_new ();
      if (self->orig_path1)
        gsk_path_builder_add_reverse_path (builder, self->orig_path1);
      if (self->orig_path2)
        gsk_path_builder_add_reverse_path (builder, self->orig_path2);
      self->path = gsk_path_builder_free_to_path (builder);
      break;
    case GLYPH_DEMO_SIMPLIFIED:
      builder = gsk_path_builder_new ();
      if (self->orig_path1)
        {
          path = gsk_path_simplify (self->orig_path1, self->fill_rule);
          gsk_path_builder_add_path (builder, path);
          gsk_path_unref (path);
        }
      if (self->orig_path2)
        {
          path = gsk_path_simplify (self->orig_path2, self->fill_rule);
          gsk_path_builder_add_path (builder, path);
          gsk_path_unref (path);
        }
      self->path = gsk_path_builder_free_to_path (builder);
      break;
    case GLYPH_DEMO_UNION:
      if (self->orig_path1 && self->orig_path2)
        self->path = gsk_path_union (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    case GLYPH_DEMO_INTERSECTION:
      if (self->orig_path1 && self->orig_path2)
        self->path = gsk_path_intersection (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    case GLYPH_DEMO_DIFFERENCE:
      if (self->orig_path1 && self->orig_path2)
        self->path = gsk_path_difference (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    case GLYPH_DEMO_SYMMETRIC_DIFFERENCE:
      if (self->orig_path1 && self->orig_path2)
        self->path = gsk_path_symmetric_difference (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    default:
      g_assert_not_reached ();
    }

  builder = gsk_path_builder_new ();
  gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC, control_cb, builder);
  self->control_path = gsk_path_builder_free_to_path (builder);

  if (!self->orig_path1 || !gsk_path_get_bounds (self->orig_path1, &bounds))
    graphene_rect_init (&bounds, 0, 0, 0, 0);
  self->bounds = bounds;
  if (!self->orig_path2 || !gsk_path_get_bounds (self->orig_path2, &bounds))
    graphene_rect_init (&bounds, 0, 0, 0, 0);
  graphene_rect_union (&bounds, &self->bounds, &self->bounds);
  if (!self->control_path || !gsk_path_get_bounds (self->control_path, &bounds))
    graphene_rect_init (&bounds, 0, 0, 0, 0);
  graphene_rect_union (&bounds, &self->bounds, &self->bounds);

  self->outline_step = 10000;
  update_short_path (self);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH]);
}
