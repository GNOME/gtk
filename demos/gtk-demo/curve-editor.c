#include "curve-editor.h"
#include "cepath.h"

#include <gtk/gtk.h>

/* {{{ Widget stuff */

struct _CurveEditor
{
  GtkWidget parent_instance;
  CEPath *path;
  CEPathCurve *context;
  float context_pos;
  gboolean edit;
  CEPathCurve *edited_curve;
  CEPathCurve *edited_point;
  CEPathCurve *dragged_curve;
  CEPathCurve *molded_curve;
  CEPathCurve *hovered_curve;
  int dragged_point;
  int hovered_point;

  GtkWidget *menu;
  GActionMap *actions;
  GskStroke *stroke;
  GdkRGBA color;

  gboolean show_outline;
};

struct _CurveEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CurveEditor, curve_editor, GTK_TYPE_WIDGET)

/* }}} */
/* {{{ Utilities */

#define DRAW_RADIUS 5
#define CLICK_RADIUS 8

static const char *
op_to_string (GskPathOperation op)
{
  switch (op)
    {
    case GSK_PATH_MOVE:
      return "move";
    case GSK_PATH_LINE:
      return "line";
    case GSK_PATH_QUAD:
      return "quad";
    case GSK_PATH_CUBIC:
      return "cubic";
    case GSK_PATH_CONIC:
      return "conic";
    case GSK_PATH_CLOSE:
      return "close";
    default:
      g_assert_not_reached ();
    }
}

static GskPathOperation
op_from_string (const char *s)
{
  if (strcmp (s, "move") == 0)
    return GSK_PATH_MOVE;
  else if (strcmp (s, "line") == 0)
    return GSK_PATH_LINE;
  else if (strcmp (s, "quad") == 0)
    return GSK_PATH_QUAD;
  else if (strcmp (s, "cubic") == 0)
    return GSK_PATH_CUBIC;
  else if (strcmp (s, "conic") == 0)
    return GSK_PATH_CONIC;
  else if (strcmp (s, "close") == 0)
    return GSK_PATH_CLOSE;
  else
    g_assert_not_reached ();
}

static const char *
constraint_to_string (CEPathConstraint constraint)
{
  switch (constraint)
    {
    case CE_PATH_CUSP:
      return "cusp";
    case CE_PATH_SMOOTH:
      return "smooth";
    case CE_PATH_SYMMETRIC:
      return "symmetric";
    case CE_PATH_AUTOMATIC:
      return "automatic";
    default:
      g_assert_not_reached ();
    }
}

static CEPathConstraint
constraint_from_string (const char *s)
{
  if (strcmp (s, "cusp") == 0)
    return CE_PATH_CUSP;
  else if (strcmp (s, "smooth") == 0)
    return CE_PATH_SMOOTH;
  else if (strcmp (s, "symmetric") == 0)
    return CE_PATH_SYMMETRIC;
  else if (strcmp (s, "automatic") == 0)
    return CE_PATH_AUTOMATIC;
  else
    g_assert_not_reached ();
}

static gboolean
point_is_visible (CurveEditor *self,
                  CEPathCurve *seg,
                  int          point)
{
  CEPathCurve *next;
  GskPathOperation op;

  if (!self->edit)
    return FALSE;

  next = ce_path_next_curve (self->path, seg);
  op = ce_path_get_operation (self->path, seg);
  switch (point)
    {
    case 0: /* point on curve */
      return TRUE;

    case 1:
      if (self->edited_curve == seg &&
          op != GSK_PATH_LINE)
        return TRUE;
      if ((op == GSK_PATH_CONIC || op == GSK_PATH_QUAD) &&
          self->edited_point == next)
        return TRUE;
      if (self->edited_curve == seg &&
          (op == GSK_PATH_CUBIC ||
           op == GSK_PATH_CONIC ||
           op == GSK_PATH_QUAD))
        return TRUE;
      break;

    case 2:
      if (self->edited_curve == seg &&
          (op != GSK_PATH_LINE && op != GSK_PATH_QUAD))
        return TRUE;
      if (op == GSK_PATH_CUBIC &&
          self->edited_point == next)
        return TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  return FALSE;
}

/* }}} */
/* {{{ Action callbacks */

static void
set_constraint (GSimpleAction *action,
                GVariant      *value,
                gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  CEPathConstraint constraint;

  constraint = constraint_from_string (g_variant_get_string (value, NULL));
  ce_path_set_constraint (self->path, self->context, constraint);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_operation (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  GskPathOperation op;

  op = op_from_string (g_variant_get_string (value, NULL));
  ce_path_set_operation (self->path, self->context, op);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
insert_new_point (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  ce_path_split_curve (self->path, self->context, self->context_pos);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_current_point (GSimpleAction *action,
                      GVariant      *value,
                      gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  ce_path_remove_curve (self->path, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
toggle_edit_point (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->edited_point == self->context)
    self->edited_point = NULL;
  else
    {
      self->edited_point = self->context;
      self->edited_curve = NULL;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
toggle_edit_curve (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->edited_curve == self->context)
    self->edited_curve = NULL;
  else
    {
      self->edited_curve = self->context;
      self->edited_point = NULL;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
reset_weight (GSimpleAction *action,
              GVariant      *value,
              gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  ce_path_set_weight (self->path, self->context, 1.f);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/* }}} */
/* {{{ Event handlers */

static void
drag_begin (GtkGestureDrag *gesture,
            double          start_x,
            double          start_y,
            CurveEditor     *self)
{
  int i, j;
  graphene_point_t point = GRAPHENE_POINT_INIT (start_x, start_y);
  float t;
  CEPathCurve *seg;

  if (!self->edit)
    return;

  for (i = 0; i < ce_path_get_n_curves (self->path); i++)
    {
      seg = ce_path_get_curve (self->path, i);
      for (j = 0; j < 3; j++)
        {
          graphene_point_t q;

          ce_path_get_point (self->path, seg, j, &q);
          if (graphene_point_distance (&q, &point, NULL, NULL) < CLICK_RADIUS)
            {
              if (point_is_visible (self, seg, j))
                {
                  self->dragged_curve = seg;
                  self->dragged_point = j;
                  gtk_widget_queue_draw (GTK_WIDGET (self));
                  return;
                }
            }
        }
    }

  seg = ce_path_find_closest_curve (self->path, &point, CLICK_RADIUS, NULL, &t);
  if (seg)
    {
      /* Can't bend a straight line */
      if (ce_path_get_operation (self->path, seg) == GSK_PATH_CUBIC)
        {
          self->molded_curve = seg;
          return;
        }
    }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}

static void
drag_update (GtkGestureDrag *gesture,
             double          offset_x,
             double          offset_y,
             CurveEditor     *self)
{
  graphene_point_t pos;
  double x, y;

  gtk_gesture_drag_get_start_point (gesture, &x, &y);

  pos.x = x + offset_x;
  pos.y = y + offset_y;

  if (self->dragged_curve != NULL)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      ce_path_set_point (self->path, self->dragged_curve, self->dragged_point, &pos);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
  else if (self->molded_curve != NULL)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      ce_path_drag_curve (self->path, self->molded_curve, &pos);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
drag_end (GtkGestureDrag *gesture,
          double          offset_x,
          double          offset_y,
          CurveEditor     *self)
{
  drag_update (gesture, offset_x, offset_y, self);
  self->dragged_curve = NULL;
  self->dragged_point = -1;
  self->molded_curve = NULL;
}

static void
pressed (GtkGestureClick *gesture,
         int              n_press,
         double           x,
         double           y,
         CurveEditor      *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int i;
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  float t;
  CEPathCurve *seg;

  if (!self->edit)
    return;

  if (button == GDK_BUTTON_SECONDARY)
    {
      for (i = 0; i < ce_path_get_n_curves (self->path); i++)
        {
          graphene_point_t p;
          CEPathConstraint constraint;

          seg = ce_path_get_curve (self->path, i);
          ce_path_get_point (self->path, seg, 0, &p);
          constraint = ce_path_get_constraint (self->path, seg);

          if (graphene_point_distance (&p, &m, NULL, NULL) < CLICK_RADIUS)
            {
              GAction *action;

              self->context = seg;

              action = g_action_map_lookup_action (self->actions, "set-curve-type");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "add-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "remove-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

              action = g_action_map_lookup_action (self->actions, "reset-weight");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "set-constraint");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (constraint_to_string (constraint)));

              action = g_action_map_lookup_action (self->actions, "edit-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (self->edited_point == seg));

              action = g_action_map_lookup_action (self->actions, "edit-curve");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                                           &(const GdkRectangle){ x, y, 1, 1 });
              gtk_popover_popup (GTK_POPOVER (self->menu));
              return;
            }
        }

      seg = ce_path_find_closest_curve (self->path, &m, CLICK_RADIUS, NULL, &t);
      if (seg)
        {
          GAction *action;
          GskPathOperation op;

          op = ce_path_get_operation (self->path, seg);

          self->context = seg;
          self->context_pos = t;

          action = g_action_map_lookup_action (self->actions, "set-constraint");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

          action = g_action_map_lookup_action (self->actions, "edit-point");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

          action = g_action_map_lookup_action (self->actions, "remove-point");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

          action = g_action_map_lookup_action (self->actions, "add-point");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

          action = g_action_map_lookup_action (self->actions, "edit-curve");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
          g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (self->edited_curve == seg));

          action = g_action_map_lookup_action (self->actions, "reset-weight");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), op == GSK_PATH_CONIC);

          action = g_action_map_lookup_action (self->actions, "set-curve-type");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
          g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (op_to_string (op)));

          gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                                           &(const GdkRectangle){ x, y, 1, 1 });
          gtk_popover_popup (GTK_POPOVER (self->menu));
          return;
        }
    }
}

static void
released (GtkGestureClick *gesture,
          int              n_press,
          double           x,
          double           y,
          CurveEditor      *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  int i;

  if (!self->edit)
    return;

  for (i = 0; i < ce_path_get_n_curves (self->path); i++)
    {
      CEPathCurve *seg = ce_path_get_curve (self->path, i);
      graphene_point_t p;

      ce_path_get_point (self->path, seg, 0, &p);
      if (graphene_point_distance (&p, &m, NULL, NULL) < CLICK_RADIUS)
        {
          if (button == GDK_BUTTON_PRIMARY)
            {
              if (self->edited_point == seg)
                self->edited_point = NULL;
              else
                {
                  self->edited_point = seg;
                  self->edited_curve = NULL;
                }
              gtk_widget_queue_draw (GTK_WIDGET (self));
              return;
            }
        }
    }

  if (button == GDK_BUTTON_PRIMARY)
    {
      CEPathCurve *seg;
      float t;

      seg = ce_path_find_closest_curve (self->path, &m, CLICK_RADIUS, NULL, &t);
      if (seg)
        {
          self->dragged_curve = NULL;
          self->dragged_point = -1;
          self->molded_curve = NULL;
          ce_path_split_curve (self->path, seg, t);
        }
    }
}

static void
motion (GtkEventControllerMotion *controller,
        double                    x,
        double                    y,
        CurveEditor              *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int i, j;
  CEPathCurve *hovered_curve = NULL;
  int hovered_point = -1;

  if (self->edit)
    {
      for (i = 0; i < ce_path_get_n_curves (self->path); i++)
        {
          CEPathCurve *seg = ce_path_get_curve (self->path, i);
          int hovered = -1;

          for (j = 0; j < 3; j++)
            {
              graphene_point_t q;

              if (!point_is_visible (self, seg, j))
                continue;

              ce_path_get_point (self->path, seg, j, &q);

              if (graphene_point_distance (&q, &m, NULL, NULL) < CLICK_RADIUS)
                {
                  hovered = j;
                  break;
                }
            }
          if (hovered != -1)
            {
              hovered_curve = seg;
              hovered_point = hovered;
              break;
            }
        }

      if (self->hovered_curve != hovered_curve ||
          self->hovered_point != hovered_point)
        gtk_widget_queue_draw (GTK_WIDGET (self));

      self->hovered_curve = hovered_curve;
      self->hovered_point = hovered_point;
    }
}

static void
leave (GtkEventController *controller,
       CurveEditor        *self)
{

  self->hovered_curve = NULL;
  self->hovered_point = -1;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/* }}} */
/* {{{ Snaps hot */

static void
add_diamond (GskPathBuilder   *builder,
             graphene_point_t *center,
             float             radius)
{
  float r = radius * 2 / (1 + M_SQRT2);

  gsk_path_builder_move_to (builder, center->x, center->y - r * M_SQRT2);
  gsk_path_builder_line_to (builder, center->x + r * M_SQRT2, center->y);
  gsk_path_builder_line_to (builder, center->x, center->y + r * M_SQRT2);
  gsk_path_builder_line_to (builder, center->x - r * M_SQRT2, center->y);
  gsk_path_builder_close (builder);
}

static void
add_square (GskPathBuilder   *builder,
            graphene_point_t *center,
            float             radius)
{
  float r = radius * 2 / (1 + M_SQRT2);

  gsk_path_builder_move_to (builder, center->x - r, center->y - r);
  gsk_path_builder_line_to (builder, center->x + r, center->y - r);
  gsk_path_builder_line_to (builder, center->x + r, center->y + r);
  gsk_path_builder_line_to (builder, center->x - r, center->y + r);
  gsk_path_builder_close (builder);
}

static void
curve_editor_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  CurveEditor *self = (CurveEditor *)widget;
  GskPathBuilder *builder;
  GskPath *path;
  GskStroke *stroke;
  int i, j, k;
  float width;
  float height;

  if (ce_path_get_n_curves (self->path) == 0)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  /* Add the curve itself */

  path = ce_path_get_gsk_path (self->path);

  stroke = gsk_stroke_copy (self->stroke);
  if (self->show_outline)
    gsk_stroke_set_line_width (stroke, 1.0);
  gtk_snapshot_push_stroke (snapshot, path, stroke);
  gsk_stroke_free (stroke);

  gtk_snapshot_append_color (snapshot,
                             &self->color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);

  if (self->show_outline)
    {
      GskPath *path2;

      path2 = gsk_path_stroke (path, self->stroke);

      stroke = gsk_stroke_copy (self->stroke);
      gsk_stroke_set_line_width (stroke, 1.0);
      gsk_stroke_set_dash (stroke, NULL, 0);
      gtk_snapshot_push_stroke (snapshot, path2, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_unref (path2);
    }

  gsk_path_unref (path);

  if (self->edit)
    {
      builder = gsk_path_builder_new ();

      if (self->edited_point != NULL)
        {
          GskPathOperation op;
          graphene_point_t p, c;
          CEPathCurve *prev;
          prev = ce_path_previous_curve (self->path, self->edited_point);

          /* Add the skeleton */

          ce_path_get_point (self->path, self->edited_point, 0, &p);
          if (prev != NULL)
            {
              op = ce_path_get_operation (self->path, prev);

              if (op == GSK_PATH_CUBIC)
                {
                  ce_path_get_point (self->path, prev, 2, &c);
                  gsk_path_builder_move_to (builder, c.x, c.y);
                  gsk_path_builder_line_to (builder, p.x, p.y);
                }
              else if (op == GSK_PATH_CONIC || op == GSK_PATH_QUAD)
                {
                  ce_path_get_point (self->path, prev, 1, &c);
                  gsk_path_builder_move_to (builder, c.x, c.y);
                  gsk_path_builder_line_to (builder, p.x, p.y);
                }
            }

          op = ce_path_get_operation (self->path, self->edited_point);

          if (op == GSK_PATH_CUBIC || op == GSK_PATH_CONIC || op == GSK_PATH_QUAD)
            {
              ce_path_get_point (self->path, self->edited_point, 1, &c);
              gsk_path_builder_move_to (builder, p.x, p.y);
              gsk_path_builder_line_to (builder, c.x, c.y);
            }
        }

      if (self->edited_curve !=  NULL)
        {
          graphene_point_t p[4];
          GskPathOperation op;

          op = ce_path_get_operation (self->path, self->edited_curve);
          ce_path_get_point (self->path, self->edited_curve, 0, &p[0]);
          ce_path_get_point (self->path, self->edited_curve, 1, &p[1]);
          ce_path_get_point (self->path, self->edited_curve, 2, &p[2]);
          ce_path_get_point (self->path, self->edited_curve, 3, &p[3]);

          if (op == GSK_PATH_CUBIC)
            {
              gsk_path_builder_move_to (builder, p[0].x, p[0].y);
              gsk_path_builder_line_to (builder, p[1].x, p[1].y);
              gsk_path_builder_line_to (builder, p[2].x, p[2].y);
              gsk_path_builder_line_to (builder, p[3].x, p[3].y);
            }
          else if (op == GSK_PATH_CONIC)
            {
              gsk_path_builder_move_to (builder, p[0].x, p[0].y);
              gsk_path_builder_line_to (builder, p[1].x, p[1].y);
              gsk_path_builder_line_to (builder, p[3].x, p[3].y);
            }
          else if (op == GSK_PATH_QUAD)
            {
              gsk_path_builder_move_to (builder, p[0].x, p[0].y);
              gsk_path_builder_line_to (builder, p[1].x, p[1].y);
              gsk_path_builder_line_to (builder, p[3].x, p[3].y);
            }
        }

      path = gsk_path_builder_free_to_path (builder);

      if (self->edited_point != NULL || self->edited_curve != NULL)
        {
          stroke = gsk_stroke_new (1);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 0, 0, 0, 1 },
                                     &GRAPHENE_RECT_INIT (0, 0, width, height ));

          gtk_snapshot_pop (snapshot);
        }

      gsk_path_unref (path);

      /* Draw the circles, in several passes, one for each color */

      const char *colors[] = {
        "red", /* hovered */
        "white"   /* smooth curve points */
      };
      GdkRGBA color;

       for (k = 0; k < 2; k++)
        {
          builder = gsk_path_builder_new ();

          for (i = 0; i < ce_path_get_n_curves (self->path); i++)
            {
              CEPathCurve *seg = ce_path_get_curve (self->path, i);

              for (j = 0; j < 3; j++)
                {
                  graphene_point_t p;

                  if (!point_is_visible (self, seg, j))
                    continue;

                  if ((k == 0) != (self->hovered_curve == seg && self->hovered_point == j))
                    continue;

                  ce_path_get_point (self->path, seg, j, &p);

                  if (j != 0)
                    {
                      gsk_path_builder_add_circle (builder, &p, DRAW_RADIUS);
                    }
                  else
                    {
                      switch (ce_path_get_constraint (self->path, seg))
                        {
                        case CE_PATH_CUSP:
                          add_diamond (builder, &p, DRAW_RADIUS);
                          break;

                        case CE_PATH_SMOOTH:
                          add_square (builder, &p, DRAW_RADIUS);
                          break;
                        case CE_PATH_SYMMETRIC:
                        case CE_PATH_AUTOMATIC:
                          gsk_path_builder_add_circle (builder, &p, DRAW_RADIUS);
                          break;
                        default:
                          g_assert_not_reached ();
                        }
                    }
                }
            }

          path = gsk_path_builder_free_to_path (builder);

          gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
          gdk_rgba_parse (&color, colors[k]);
          gtk_snapshot_append_color (snapshot,
                                     &color,
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 0, 0, 0, 1 },
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }
}

/* }}} */
/* {{{ GtkWidget boilerplate */

static void
curve_editor_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum_size,
                      int            *natural_size,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  *minimum_size = 100;
  *natural_size = 200;
}

static void
curve_editor_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  CurveEditor *self = CURVE_EDITOR (widget);

  gtk_popover_present (GTK_POPOVER (self->menu));
}

/* }}} */
/* {{{ GObject boilerplate */

static void
curve_editor_dispose (GObject *object)
{
  CurveEditor *self = CURVE_EDITOR (object);

  g_clear_object (&self->path);
  g_clear_pointer (&self->menu, gtk_widget_unparent);
  g_clear_object (&self->actions);

  G_OBJECT_CLASS (curve_editor_parent_class)->dispose (object);
}

static void
curve_editor_class_init (CurveEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = curve_editor_dispose;

  widget_class->snapshot = curve_editor_snapshot;
  widget_class->measure = curve_editor_measure;
  widget_class->size_allocate = curve_editor_size_allocate;
}

 /* }}} */
/* {{{ Setup */

static void
curve_editor_init (CurveEditor *self)
{
  GtkEventController *controller;
  GMenu *menu;
  GMenu *section;
  GMenuItem *item;
  GSimpleAction *action;

  self->path = ce_path_new ();
  self->context = NULL;
  self->dragged_curve = NULL;
  self->dragged_point = -1;
  self->molded_curve = NULL;
  self->edited_curve = NULL;
  self->edited_point = NULL;
  self->edit = FALSE;
  self->hovered_curve = NULL;
  self->hovered_point = -1;
  self->stroke = gsk_stroke_new (1.0);
  self->color = (GdkRGBA){ 0, 0, 0, 1 };

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_drag_new ());
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), GDK_BUTTON_PRIMARY);
  g_signal_connect (controller, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect (controller, "drag-update", G_CALLBACK (drag_update), self);
  g_signal_connect (controller, "drag-end", G_CALLBACK (drag_end), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
  g_signal_connect (controller, "pressed", G_CALLBACK (pressed), self);
  g_signal_connect (controller, "released", G_CALLBACK (released), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion), self);
  g_signal_connect (controller, "leave", G_CALLBACK (leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  self->actions = G_ACTION_MAP (g_simple_action_group_new ());

  action = g_simple_action_new_stateful ("set-constraint", G_VARIANT_TYPE_STRING, g_variant_new_string ("smooth"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_constraint), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  action = g_simple_action_new_stateful ("set-curve-type", G_VARIANT_TYPE_STRING, g_variant_new_string ("cubic"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_operation), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new_stateful ("edit-point", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (toggle_edit_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new_stateful ("edit-curve", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (toggle_edit_curve), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("add-point", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (insert_new_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("remove-point", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (remove_current_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("reset-weight", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (reset_weight), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  gtk_widget_insert_action_group (GTK_WIDGET (self), "path", G_ACTION_GROUP (self->actions));

  menu = g_menu_new ();

  section = g_menu_new ();

  item = g_menu_item_new ("Cusp", "path.set-constraint::cusp");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Smooth", "path.set-constraint::smooth");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Symmetric", "path.set-constraint::symmetric");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Automatic", "path.set-constraint::automatic");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();

  item = g_menu_item_new ("Line", "path.set-curve-type::line");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Quadratic", "path.set-curve-type::quad");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Cubic", "path.set-curve-type::cubic");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Conic", "path.set-curve-type::conic");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();

  item = g_menu_item_new ("Edit", "path.edit-point");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Edit", "path.edit-curve");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();

  item = g_menu_item_new ("Add", "path.add-point");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Remove", "path.remove-point");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Reset", "path.reset-weight");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  self->menu = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  g_object_unref (menu);

  gtk_widget_set_parent (self->menu, GTK_WIDGET (self));
}

 /* }}} */
/* {{{ API */

GtkWidget *
curve_editor_new (void)
{
  return g_object_new (curve_editor_get_type (), NULL);
}

void
curve_editor_set_edit (CurveEditor *self,
                       gboolean     edit)
{
  self->edit = edit;
  self->edited_point = NULL;
  self->edited_curve = NULL;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
curve_editor_set_path (CurveEditor *self,
                       GskPath     *path)
{
  ce_path_set_gsk_path (self->path, path);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

GskPath *
curve_editor_get_path (CurveEditor *self)
{
  return ce_path_get_gsk_path (self->path);
}

void
curve_editor_set_stroke (CurveEditor *self,
                         GskStroke   *stroke)
{
  gsk_stroke_free (self->stroke);
  self->stroke = gsk_stroke_copy (stroke);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

const GskStroke *
curve_editor_get_stroke (CurveEditor *self)
{
  return self->stroke;
}

void
curve_editor_set_color (CurveEditor   *self,
                        const GdkRGBA *color)
{
  self->color = *color;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

const GdkRGBA *
curve_editor_get_color (CurveEditor *self)
{
  return &self->color;
}

void
curve_editor_set_show_outline (CurveEditor *self,
                               gboolean     show_outline)
{
  self->show_outline = show_outline;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

gboolean
curve_editor_get_show_outline (CurveEditor *self)
{
  return self->show_outline;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
