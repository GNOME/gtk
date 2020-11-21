#include "curve-editor.h"

#include <gtk/gtk.h>

/* Set q to the projection of p onto the line through a and b */
static void
closest_point (const graphene_point_t *p,
               const graphene_point_t *a,
               const graphene_point_t *b,
               graphene_point_t       *q)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  t = graphene_vec2_dot (&ap, &n) / graphene_vec2_dot (&n, &n);

  q->x = a->x + t * (b->x - a->x);
  q->y = a->y + t * (b->y - a->y);
}

/* Determine if p is on the line through a and b */
static gboolean
collinear (const graphene_point_t *p,
           const graphene_point_t *a,
           const graphene_point_t *b)
{
  graphene_point_t q;

  closest_point (p, a, b, &q);

  return graphene_point_near (p, &q, 0.0001);
}

/* Set q to the point on the line through p and a that is
 * at a distance of d from p, on the opposite side
 */
static void
opposite_point (const graphene_point_t *p,
                const graphene_point_t *a,
                float                   d,
                graphene_point_t       *q)
{
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  t = - sqrt (d * d / graphene_vec2_dot (&ap, &ap));

  q->x = p->x + t * (a->x - p->x);
  q->y = p->y + t * (a->y - p->y);
}

#define DRAW_RADIUS 5
#define CLICK_RADIUS 8

typedef enum
{
  MOVE,
  LINE,
  CURVE
} Operation;

static const char *
op_to_string (Operation op)
{
  switch (op)
    {
    case MOVE:
      return "move";
    case LINE:
      return "line";
    case CURVE:
      return "curve";
    default:
      g_assert_not_reached ();
    }
}

static Operation
op_from_string (const char *s)
{
  if (strcmp (s, "move") == 0)
    return MOVE;
  else if (strcmp (s, "line") == 0)
    return LINE;
  else if (strcmp (s, "curve") == 0)
    return CURVE;
  else
    g_assert_not_reached ();
}

typedef struct
{
  Operation op;
  gboolean edit;
  gboolean smooth;
} PointData;

struct _CurveEditor
{
  GtkWidget parent_instance;
  graphene_point_t *points;
  int n_points;
  PointData *point_data; /* length is n_points / 3 */
  int dragged;
  int context;
  gboolean symmetric;
  gboolean edit;

  GtkWidget *menu;
  GActionMap *actions;
};

struct _CurveEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CurveEditor, curve_editor, GTK_TYPE_WIDGET)

static void
drag_begin (GtkGestureDrag *gesture,
            double          start_x,
            double          start_y,
            CurveEditor     *self)
{
  int i;
  graphene_point_t p = GRAPHENE_POINT_INIT (start_x, start_y);

  if (self->edit)
    for (i = 0; i < self->n_points; i++)
      {
        if (graphene_point_distance (&self->points[i], &p, NULL, NULL) < CLICK_RADIUS)
          {
            self->dragged = i;
            self->symmetric = (gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture)) & GDK_CONTROL_MASK) == 0;

            gtk_widget_queue_draw (GTK_WIDGET (self));
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
  double x, y;
  double dx, dy;
  graphene_point_t *c, *p, *d;
  double l1, l2;

  if (self->dragged == -1)
    return;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  gtk_gesture_drag_get_start_point (gesture, &x, &y);

  x += offset_x;
  y += offset_y;

  d = &self->points[self->dragged];

  /* before moving the point, record the distances to its neighbors, since
   * we may want to preserve those
   */
  c = &self->points[(self->dragged - 1 + self->n_points) % self->n_points];
  l1 = graphene_point_distance (d, c, NULL, NULL);
  c = &self->points[(self->dragged + 1) % self->n_points];
  l2 = graphene_point_distance (d, c, NULL, NULL);

  dx = x - d->x;
  dy = y - d->y;

  if (self->dragged % 3 == 0)
    {
      /* dragged point is on curve */

      Operation op, op1, op2;

      /* first move the point itself */
      d->x = x;
      d->y = y;

      /* adjust control points as needed */
      op = self->point_data[self->dragged / 3].op;
      op1 = self->point_data[((self->dragged - 1 + self->n_points) % self->n_points) / 3].op;

      if (op1 == LINE)
        {
          /* the other endpoint of the line */
          p = &self->points[(self->dragged - 3 + self->n_points) % self->n_points];

          if (op == CURVE && self->point_data[self->dragged / 3].smooth)
            {
              /* adjust the control point after the line segment */
              c = &self->points[(self->dragged + 1) % self->n_points];
              opposite_point (d, p, l2, c);
            }
          else
            {
              c = &self->points[(self->dragged + 1) % self->n_points];
              c->x += dx;
              c->y += dy;
            }

          c = &self->points[(self->dragged - 1 + self->n_points) % self->n_points];
          c->x += dx;
          c->y += dy;

          op2 = self->point_data[((self->dragged - 4 + self->n_points) % self->n_points) / 3].op;
          if (op2 == CURVE && self->point_data[((self->dragged - 3 + self->n_points) % self->n_points) / 3].smooth)
            {
              double l;

              /* adjust the control point before the line segment */
              c = &self->points[((self->dragged - 4 + self->n_points) % self->n_points)];

              l = graphene_point_distance (c, p, NULL, NULL);
              opposite_point (p, d, l, c);
            }
        }
      if (op == LINE)
        {
          /* the other endpoint of the line */
          p = &self->points[(self->dragged + 3) % self->n_points];

          if (op1 == CURVE && self->point_data[self->dragged / 3].smooth)
            {
              /* adjust the control point before the line segment */
              c = &self->points[(self->dragged - 1 + self->n_points) % self->n_points];
              opposite_point (d, p, l1, c);
            }
          else
            {
              c = &self->points[(self->dragged -1 + self->n_points) % self->n_points];
              c->x += dx;
              c->y += dy;
            }

          c = &self->points[(self->dragged + 1) % self->n_points];
          c->x += dx;
          c->y += dy;

          op2 = self->point_data[((self->dragged + 3) % self->n_points) / 3].op;
          if (op2 == CURVE && self->point_data[((self->dragged + 3) % self->n_points) / 3].smooth)
            {
              double l;

              /* adjust the control point after the line segment */
              c = &self->points[((self->dragged + 4) % self->n_points)];

              l = graphene_point_distance (c, p, NULL, NULL);
              opposite_point (p, d, l, c);
            }
        }
      if (op1 != LINE && op != LINE)
        {
          self->points[(self->dragged - 1 + self->n_points) % self->n_points].x += dx;
          self->points[(self->dragged - 1 + self->n_points) % self->n_points].y += dy;

          self->points[(self->dragged + 1) % self->n_points].x += dx;
          self->points[(self->dragged + 1) % self->n_points].y += dy;
        }
    }
  else
    {
      /* dragged point is a control point */

      int point;
      graphene_point_t *p1;
      Operation op, op1;

      if (self->dragged % 3 == 1)
        {
          point = (self->dragged - 1 + self->n_points) % self->n_points;
          c = &self->points[(self->dragged - 2 + self->n_points) % self->n_points];
          p = &self->points[point];

          op = self->point_data[point / 3].op;
          op1 = self->point_data[((self->dragged - 4 + self->n_points) % self->n_points) / 3].op;
          p1 = &self->points[(self->dragged - 4 + self->n_points) % self->n_points];
        }
      else if (self->dragged % 3 == 2)
        {
          point = (self->dragged + 1) % self->n_points;
          c = &self->points[(self->dragged + 2) % self->n_points];
          p = &self->points[point];

          op = self->point_data[self->dragged / 3].op;
          op1 = self->point_data[point / 3].op;
          p1 = &self->points[(self->dragged + 4) % self->n_points];
        }
      else
        g_assert_not_reached ();

      if (op == CURVE && self->point_data[point / 3].smooth)
        {
          if (op1 == CURVE)
            {
              double l;

              /* first move the point itself */
              d->x = x;
              d->y = y;

              /* then adjust the other control point */
              if (self->symmetric)
                l = graphene_point_distance (d, p, NULL, NULL);
              else
                l = graphene_point_distance (c, p, NULL, NULL);

              opposite_point (p, d, l, c);
            }
          else if (op1 == LINE)
            {
              graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
              closest_point (&m, p, p1, d);
            }
          else
            {
              d->x = x;
              d->y = y;
            }
        }
      else
        {
          d->x = x;
          d->y = y;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
drag_end (GtkGestureDrag *gesture,
          double          offset_x,
          double          offset_y,
          CurveEditor     *self)
{
  drag_update (gesture, offset_x, offset_y, self);
  self->dragged = -1;
  self->symmetric = FALSE;
}

static void
maintain_smoothness (CurveEditor *self,
                     int         point)
{
  gboolean smooth;
  Operation op, op1;

  smooth = self->point_data[point / 3].smooth;

  op = self->point_data[point / 3].op;
  op1 = self->point_data[((point - 1 + self->n_points) % self->n_points) / 3].op;

  if (smooth)
    {
      graphene_point_t *p;

      p = &self->points[point];

      if (op == CURVE && op1 == CURVE)
        {
          graphene_point_t *c, *c2;
          float d;

          c = &self->points[(point - 1 + self->n_points) % self->n_points];
          c2 = &self->points[(point + 1) % self->n_points];

          d = graphene_point_distance (c, p, NULL, NULL);
          opposite_point (p, c2, d, c);
        }
      else if (op == CURVE && op1 == LINE)
        {
          graphene_point_t *c, *p2;
          float d;

          c = &self->points[(point + 1) % self->n_points];
          p2 = &self->points[(point - 3 + self->n_points) % self->n_points];

          d = graphene_point_distance (c, p, NULL, NULL);
          opposite_point (p, p2, d, c);
        }
      else if (op == LINE && op1 == CURVE)
        {
          graphene_point_t *c, *p2;
          float d;

          c = &self->points[(point - 1 + self->n_points) % self->n_points];
          p2 = &self->points[(point + 3) % self->n_points];

          d = graphene_point_distance (c, p, NULL, NULL);
          opposite_point (p, p2, d, c);
        }
    }
}

static void
toggle_smooth (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  self->point_data[self->context / 3].smooth = g_variant_get_boolean (value);

  maintain_smoothness (self, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_operation (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  self->point_data[self->context / 3].op = op_from_string (g_variant_get_string (value, NULL));

  maintain_smoothness (self, self->context);
  maintain_smoothness (self, (self->context + 3) % self->n_points);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_point (GSimpleAction *action,
              GVariant      *value,
              gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  int i;

  for (i = self->context / 3; i + 1 < self->n_points / 3; i++)
    self->point_data[i] = self->point_data[i + 1];

  self->point_data = g_realloc (self->point_data, sizeof (PointData) * (self->n_points / 3 - 1));

  self->points[(self->context - 1 + self->n_points) % self->n_points] = self->points[self->context + 2];

  for (i = self->context; i + 3 < self->n_points; i++)
    self->points[i] = self->points[i + 3];

  self->points = g_realloc (self->points, sizeof (graphene_point_t) * (self->n_points - 3));

  self->n_points -= 3;
}

static void
find_line_point (graphene_point_t *a,
                 graphene_point_t *b,
                 graphene_point_t *p,
                 double           *t,
                 graphene_point_t *pp,
                 double           *d)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float tt;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  tt = graphene_vec2_dot (&ap, &n) / graphene_vec2_dot (&n, &n);

  if (tt < 0)
    {
      *pp = *a;
      *t = 0;
      *d = graphene_point_distance (a, p, NULL, NULL);
    }
  else if (tt > 1)
    {
      *pp = *b;
      *t = 1;
      *d = graphene_point_distance (b, p, NULL, NULL);
    }
  else
    {
      pp->x = a->x + tt * (b->x - a->x);
      pp->y = a->y + tt * (b->y - a->y);
      *t = tt;
      *d = graphene_point_distance (pp, p, NULL, NULL);
    }
}

static void
gsk_split_get_coefficients (graphene_point_t       coeffs[4],
                            const graphene_point_t pts[4])
{
  coeffs[0] = GRAPHENE_POINT_INIT (pts[3].x - 3.0f * pts[2].x + 3.0f * pts[1].x - pts[0].x,
                                   pts[3].y - 3.0f * pts[2].y + 3.0f * pts[1].y - pts[0].y);
  coeffs[1] = GRAPHENE_POINT_INIT (3.0f * pts[2].x - 6.0f * pts[1].x + 3.0f * pts[0].x,
                                   3.0f * pts[2].y - 6.0f * pts[1].y + 3.0f * pts[0].y);
  coeffs[2] = GRAPHENE_POINT_INIT (3.0f * pts[1].x - 3.0f * pts[0].x,
                                   3.0f * pts[1].y - 3.0f * pts[0].y);
  coeffs[3] = pts[0];
}

static void
gsk_spline_get_point_cubic (const graphene_point_t  pts[4],
                            float                   progress,
                            graphene_point_t       *pos,
                            graphene_vec2_t        *tangent)
{
  graphene_point_t c[4];

  gsk_split_get_coefficients (c, pts);
  if (pos)
    *pos = GRAPHENE_POINT_INIT (((c[0].x * progress + c[1].x) * progress +c[2].x) * progress + c[3].x,
                                ((c[0].y * progress + c[1].y) * progress +c[2].y) * progress + c[3].y);
  if (tangent)
    {
      graphene_vec2_init (tangent,
                          (3.0f * c[0].x * progress + 2.0f * c[1].x) * progress + c[2].x,
                          (3.0f * c[0].y * progress + 2.0f * c[1].y) * progress + c[2].y);
      graphene_vec2_normalize (tangent, tangent);
    }
}

static void
find_curve_point (graphene_point_t *points,
                  graphene_point_t *p,
                  double           *t,
                  graphene_point_t *pp,
                  double           *d)
{
  graphene_point_t q;
  graphene_point_t best_p;
  double best_d;
  double best_t;
  double dd;
  double tt;
  int i;

  best_d = G_MAXDOUBLE;
  best_t = 0;

  for (i = 0; i < 20; i++)
    {
      tt = i / 20.0;
      gsk_spline_get_point_cubic (points, tt, &q, NULL);
      dd = graphene_point_distance (&q, p, NULL, NULL);
      if (dd < best_d)
        {
          best_d = dd;
          best_t = tt;
          best_p = q;
        }
    }

  /* TODO: bisect from here */

  *t = best_t;
  *pp = best_p;
  *d = best_d;
}

static void
find_closest_point (CurveEditor      *self,
                    graphene_point_t *p,
                    int              *point,
                    double           *t,
                    double           *d)
{
  int i;
  int best_i;
  double best_d;
  double best_t;
  double tt;
  double dd;
  graphene_point_t pp;

  best_i = -1;
  best_d = G_MAXDOUBLE;
  best_t = 0;

  for (i = 0; i < self->n_points; i++)
    {
      if (i % 3 != 0)
        continue;

      switch (self->point_data[i / 3].op)
        {
        case MOVE:
          continue;
        case LINE:
          find_line_point (&self->points[i], &self->points[(i + 3) % self->n_points], p, &tt, &pp, &dd);
          if (dd < best_d)
            {
              best_i = i;
              best_d = dd;
              best_t = tt;
            }
          break;
        case CURVE:
          {
            graphene_point_t points[4];
            int k;

            for (k = 0; k < 4; k++)
              points[k] = self->points[(i + k) % self->n_points];

            find_curve_point (points, p, &tt, &pp, &dd);
            if (dd < best_d)
              {
                best_i = i;
                best_d = dd;
                best_t = tt;
              }
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

  *point = best_i;
  *t = best_t;
  *d = best_d;
}

static void
split_bezier (graphene_point_t *points,
              int               length,
              float             t,
              graphene_point_t *left,
              int              *left_pos,
              graphene_point_t *right,
              int              *right_pos)
{
  if (length == 1)
    {
      left[*left_pos] = points[0];
      (*left_pos)++;
      right[*right_pos] = points[0];
      (*right_pos)++;
    }
  else
    {
      graphene_point_t *newpoints;
      int i;

      newpoints = g_alloca (sizeof (graphene_point_t) * (length - 1));
      for (i = 0; i < length - 1; i++)
        {
          if (i == 0)
            {
              left[*left_pos] = points[i];
              (*left_pos)++;
            }
          if (i == length - 2)
            {
              right[*right_pos] = points[i + 1];
              (*right_pos)++;
            }
          graphene_point_interpolate (&points[i], &points[i+1], t, &newpoints[i]);
        }
      split_bezier (newpoints, length - 1, t, left, left_pos, right, right_pos);
    }
}

static void
insert_point (CurveEditor *self,
              int          point,
              double       pos)
{
  Operation op = self->point_data[point / 3].op;
  int i;
  graphene_point_t points[4];
      int k;

  if (op == MOVE)
    return;

  for (k = 0; k < 4; k++)
    points[k] = self->points[(point + k) % self->n_points];

  self->point_data = g_realloc (self->point_data, sizeof (PointData) * (self->n_points / 3 + 1));
  for (i = self->n_points / 3; i > point / 3; i--)
    self->point_data[i] = self->point_data[i - 1];

  self->points = g_realloc (self->points, sizeof (graphene_point_t) * (self->n_points + 3));
  for (i = self->n_points + 2; i > point + 4; i--)
    self->points[i] = self->points[i - 3];

  self->n_points += 3;

  if (op == LINE)
    {
      graphene_point_t p;
      graphene_point_t q;

      graphene_point_interpolate (&self->points[point], &self->points[(point + 6) % self->n_points], pos, &p);
      self->points[point + 3] = p;

      graphene_point_interpolate (&p, &self->points[(point + 6) % self->n_points], 0.33, &q);

      self->points[point + 4] = q;

      graphene_point_interpolate (&p, &self->points[(point + 6) % self->n_points], 0.66, &q);

      self->points[point + 5] = q;
      self->point_data[point / 3 + 1].smooth = TRUE;
      self->point_data[point / 3 + 1].op = LINE;
    }
  else if (op == CURVE)
    {
      graphene_point_t left[4];
      graphene_point_t right[4];
      int left_pos = 0;
      int right_pos = 0;

      split_bezier (points, 4, pos, left, &left_pos, right, &right_pos);

      for (k = 0; k < 4; k++)
        {
          self->points[(point + k) % self->n_points] = left[k];
          self->points[(point + 3 + k) % self->n_points] = right[3 - k];
        }

      self->point_data[point / 3 + 1].smooth = TRUE;
      self->point_data[point / 3 + 1].op = CURVE;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
maybe_insert_point (CurveEditor *self,
                    double       x,
                    double       y)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int point;
  double t;
  double d;

  find_closest_point (self, &m, &point, &t, &d);

  if (d > CLICK_RADIUS)
    return;

  insert_point (self, point, t);
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

  if (!self->edit)
    return;

  if (button != GDK_BUTTON_SECONDARY)
    return;

  for (i = 0; i < self->n_points; i++)
    {
      if (i % 3 != 0)
        continue;

      if (graphene_point_distance (&self->points[i], &m, NULL, NULL) < CLICK_RADIUS)
        {
          GAction *action;

          self->context = i;

          action = g_action_map_lookup_action (self->actions, "smooth");
          g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (self->point_data[i / 3].smooth));

          action = g_action_map_lookup_action (self->actions, "operation");

          g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (op_to_string (self->point_data[i / 3].op)));

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

  for (i = 0; i < self->n_points; i++)
    {
      if (graphene_point_distance (&self->points[i], &m, NULL, NULL) < CLICK_RADIUS)
        {
          if (i % 3 == 0)
            {
              if (button == GDK_BUTTON_PRIMARY)
                {
                  self->point_data[i / 3].edit = !self->point_data[i / 3].edit;
                  return;
                }
              else if (button == GDK_BUTTON_SECONDARY)
                {
                  self->context = i;
                  self->point_data[i / 3].smooth = !self->point_data[i / 3].smooth;
                  if (self->point_data[i / 3].smooth)
                    {
                      graphene_point_t *p, *c, *c2;
                      float d;

                      p = &self->points[i];
                      c = &self->points[(i - 1 + self->n_points) % self->n_points];
                      c2 = &self->points[(i + 1 + self->n_points) % self->n_points];

                      d = graphene_point_distance (c, p, NULL, NULL);
                      opposite_point (p, c2, d, c);
                    }

                  return;
                }
            }
        }
    }

  if (button == GDK_BUTTON_PRIMARY)
    maybe_insert_point (self, x, y);
}

static void
curve_editor_init (CurveEditor *self)
{
  GtkGesture *gesture;
  GMenu *menu;
  GMenu *section;
  GMenuItem *item;
  GSimpleAction *action;

  self->dragged = -1;
  self->edit = FALSE;

  gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect (gesture, "drag-update", G_CALLBACK (drag_update), self);
  g_signal_connect (gesture, "drag-end", G_CALLBACK (drag_end), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed", G_CALLBACK (pressed), self);
  g_signal_connect (gesture, "released", G_CALLBACK (released), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  self->points = NULL;
  self->point_data = NULL;
  self->n_points = 0;

  self->actions = G_ACTION_MAP (g_simple_action_group_new ());

  action = g_simple_action_new_stateful ("smooth", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (toggle_smooth), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  action = g_simple_action_new_stateful ("operation", G_VARIANT_TYPE_STRING, g_variant_new_string ("curve"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_operation), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("remove", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (remove_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  menu = g_menu_new ();

  item = g_menu_item_new ("Smooth", "point.smooth");
  g_menu_append_item (menu, item);
  g_object_unref (item);

  section = g_menu_new ();

  item = g_menu_item_new ("Move", "point.operation::move");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Line", "point.operation::line");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Curve", "point.operation::curve");
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  item = g_menu_item_new ("Remove", "point.remove");
  g_menu_append_item (section, item);
  g_object_unref (item);

  self->menu = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  g_object_unref (menu);

  gtk_widget_set_parent (self->menu, GTK_WIDGET (self));
}

static void
curve_editor_add_path (CurveEditor    *self,
                       GskPathBuilder *builder)
{
  int i;

  gsk_path_builder_move_to (builder, self->points[0].x, self->points[0].y);
  for (i = 1; i < self->n_points; i += 3)
    {
      switch (self->point_data[i / 3].op)
        {
        case MOVE:
          gsk_path_builder_move_to (builder,
                                    self->points[(i + 2) % self->n_points].x, self->points[(i + 2) % self->n_points].y);
          break;

        case LINE:
          gsk_path_builder_line_to (builder,
                                    self->points[(i + 2) % self->n_points].x, self->points[(i + 2) % self->n_points].y);
          break;

        case CURVE:
          gsk_path_builder_curve_to (builder,
                                     self->points[i].x, self->points[i].y,
                                     self->points[(i + 1) % self->n_points].x, self->points[(i + 1) % self->n_points].y,
                                     self->points[(i + 2) % self->n_points].x, self->points[(i + 2) % self->n_points].y);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
curve_editor_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  CurveEditor *self = (CurveEditor *)widget;
  GskPathBuilder *builder;
  GskPath *path;
  GskStroke *stroke;
  int i, j;
  float width;
  float height;

  if (self->n_points == 0)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  builder = gsk_path_builder_new ();

  if (self->edit)
    {
      /* Add the skeleton */

      gsk_path_builder_move_to (builder, self->points[0].x, self->points[0].y);
      for (i = 1; i < self->n_points; i++)
        {
          gboolean edit;
          gboolean line;

          if (i % 3 == 2)
            edit = self->point_data[((i + 3) % self->n_points) / 3].edit;
          else
            edit = self->point_data[i / 3].edit;

          if (i % 3 == 0)
            line = self->point_data[((i - 1 + self->n_points) % self->n_points) / 3].op != CURVE;
          else
            line = self->point_data[i / 3].op != CURVE;

          if (edit)
            {
              if (i % 3 == 2 || line)
                gsk_path_builder_move_to (builder, self->points[i].x, self->points[i].y);
              else
                gsk_path_builder_line_to (builder, self->points[i].x, self->points[i].y);
            }
        }
      if (self->point_data[0].edit)
        gsk_path_builder_line_to (builder, self->points[0].x, self->points[0].y);
    }

  /* Add the curve itself */

  curve_editor_add_path (self, builder);

  /* Stroke everything we have so far */

  path = gsk_path_builder_free_to_path (builder);
  stroke = gsk_stroke_new (1);
  gtk_snapshot_push_stroke (snapshot, path, stroke);
  gsk_stroke_free (stroke);
  gsk_path_unref (path);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA){ 0, 0, 0, 1 },
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);

  if (self->edit)
    {
      /* Draw the circles, in several passes, one for each color */

      const char *colors[] = {
        "red",
        "green",
        "blue"
      };
      GdkRGBA color;

      for (j = 0; j < 3; j++)
        {
          builder = gsk_path_builder_new ();

          for (i = 0; i < self->n_points; i++)
            {
              switch (j)
                {
                case 0:
                  if (!(i % 3 == 0 &&
                        self->point_data[i / 3].smooth))
                    continue;
                  break;

                case 1:
                  if (!(i % 3 == 0 &&
                        !self->point_data[i / 3].smooth))
                    continue;
                  break;

                case 2:
                  if (i % 3 == 1)
                    {
                      if (!(self->point_data[i / 3].edit &&
                            self->point_data[i / 3].op == CURVE))
                        continue;
                    }
                  else if (i % 3 == 2)
                    {
                      if (!(self->point_data[((i + 3) % self->n_points) / 3].edit &&
                            self->point_data[i / 3].op == CURVE))
                        continue;
                    }
                  else
                    continue;
                  break;

                default:
                  g_assert_not_reached ();
                }

              gsk_path_builder_add_circle (builder, &self->points[i], DRAW_RADIUS);
            }

          path = gsk_path_builder_free_to_path (builder);

          gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
          gdk_rgba_parse (&color, colors[j]);
          gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gdk_rgba_parse (&color, "black");
          gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }
}

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

  gtk_native_check_resize (GTK_NATIVE (self->menu));
}

static void
curve_editor_dispose (GObject *object)
{
  CurveEditor *self = CURVE_EDITOR (object);

  g_clear_pointer (&self->points, g_free);
  g_clear_pointer (&self->point_data, g_free);
  g_clear_pointer (&self->menu, gtk_widget_unparent);

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

GtkWidget *
curve_editor_new (void)
{
  return g_object_new (curve_editor_get_type (), NULL);
}

void
curve_editor_set_edit (CurveEditor *self,
                       gboolean     edit)
{
  int i;

  self->edit = edit;
  if (!self->edit)
    {
      for (i = 0; i < self->n_points / 3; i++)
        self->point_data[i].edit = FALSE;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

typedef struct
{
  int count;
  graphene_point_t first;
  graphene_point_t last;
  gboolean has_close;
  gboolean has_initial_move;
} CountSegmentData;

static gboolean
count_segments (GskPathOperation        op,
                const graphene_point_t *pts,
                gsize                   n_pts,
                gpointer                data)
{
  CountSegmentData *d = data;

  if (d->count == 0)
    {
      d->first = pts[0];
      if (op == GSK_PATH_MOVE)
        d->has_initial_move = TRUE;
    }

  d->last = pts[n_pts - 1];
  d->count++;

  if (op == GSK_PATH_CLOSE)
    d->has_close = TRUE;

  return TRUE;
}

typedef struct
{
  CurveEditor *editor;
  int pos;
} CopySegmentData;

static gboolean
copy_segments (GskPathOperation        op,
               const graphene_point_t *pts,
               gsize                   n_pts,
               gpointer                data)
{
  CopySegmentData *d = data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (d->pos != 0)
        {
          d->editor->point_data[d->pos / 3].op = MOVE;
          d->editor->point_data[d->pos / 3].smooth = FALSE;

          d->editor->points[d->pos++] = pts[0];
          d->editor->points[d->pos++] = pts[0];
          if (d->pos < d->editor->n_points)
            d->editor->points[d->pos++] = pts[0];
        }
      break;
    case GSK_PATH_CLOSE:
      break;
    case GSK_PATH_LINE:
      d->editor->point_data[d->pos / 3].op = LINE;
      d->editor->point_data[d->pos / 3].smooth = FALSE;

      if (d->pos == 0)
        d->editor->points[d->pos++] = pts[0];

      d->editor->points[d->pos++] = pts[1];
      d->editor->points[d->pos++] = pts[1];
      if (d->pos < d->editor->n_points)
        d->editor->points[d->pos++] = pts[1];
      break;
    case GSK_PATH_CURVE:
      d->editor->point_data[d->pos / 3].op = CURVE;
      d->editor->point_data[d->pos / 3].smooth = FALSE;

      if (d->pos == 0)
        d->editor->points[d->pos++] = pts[0];

      d->editor->points[d->pos++] = pts[1];
      d->editor->points[d->pos++] = pts[2];

      if (d->pos < d->editor->n_points)
        d->editor->points[d->pos++] = pts[3];
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

/* Check if the points arount point currently satisy
 * smoothness conditions. Set PointData.smooth accordingly.
 */
static void
update_smoothness (CurveEditor *self,
                   int          point)
{
  Operation op, op1;
  graphene_point_t *p, *p2, *p1;

  p = &self->points[point];
  op = self->point_data[point / 3].op;
  op1 = self->point_data[((point - 1 + self->n_points) % self->n_points) / 3].op;

  if (op == CURVE)
    p2 = &self->points[(point + 1) % self->n_points];
  else if (op == LINE)
    p2 = &self->points[(point + 3) % self->n_points];
  else
    p2 = NULL;

  if (op1 == CURVE)
    p1 = &self->points[(point - 1 + self->n_points) % self->n_points];
  else if (op1 == LINE)
    p1 = &self->points[(point - 3 + self->n_points) % self->n_points];
  else
    p1 = NULL;

  if (p1 && p2)
    self->point_data[point / 3].smooth = collinear (p, p1, p2);
  else
    self->point_data[point / 3].smooth = TRUE;
}

void
curve_editor_set_path (CurveEditor *self,
                       GskPath     *path)
{
  CountSegmentData data;
  CopySegmentData data2;
  int i;

  g_clear_pointer (&self->points, g_free);
  g_clear_pointer (&self->point_data, g_free);
  self->n_points = 0;

  data.count = 0;
  data.has_close = FALSE;
  gsk_path_foreach (path, count_segments, &data);

  if (data.has_initial_move)
    data.count--;

  if (!graphene_point_near (&data.first, &data.last, 0.0001) && !data.has_close)
    data.count++;

  self->n_points = data.count * 3;
  self->points = g_new0 (graphene_point_t, self->n_points);
  self->point_data = g_new0 (PointData, data.count);

  data2.editor = self;
  data2.pos = 0;
  gsk_path_foreach (path, copy_segments, &data2);

  for (i = 0; i < self->n_points; i += 3)
    update_smoothness (self, i);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

GskPath *
curve_editor_get_path (CurveEditor *self)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  curve_editor_add_path (self, builder);

  return gsk_path_builder_free_to_path (builder);
}
