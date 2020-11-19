#include "curve-editor.h"

#include <gtk/gtk.h>

#define DRAW_RADIUS 5
#define CLICK_RADIUS 8

/* {{{ Types and structures */
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

typedef enum
{
  CUSP,
  SMOOTH,
  SYMMETRIC,
  AUTO
} PointType;

static const char *
point_type_to_string (PointType type)
{
  switch (type)
    {
    case CUSP:
      return "cusp";
    case SMOOTH:
      return "smooth";
    case SYMMETRIC:
      return "symmetric";
    case AUTO:
      return "auto";
    default:
      g_assert_not_reached ();
    }
}

static PointType
point_type_from_string (const char *s)
{
  if (strcmp (s, "cusp") == 0)
    return CUSP;
  else if (strcmp (s, "smooth") == 0)
    return SMOOTH;
  else if (strcmp (s, "symmetric") == 0)
    return SYMMETRIC;
  else if (strcmp (s, "auto") == 0)
    return AUTO;
  else
    g_assert_not_reached ();
}

/* We don't store Bezier segments, but an array of points on
 * the line. Each point comes with its two neighboring control
 * points, so each Bezier segment contains p[1] and p[2] from
 * one point, and p[0] and p[1] from the next.
 *
 * The control points are irrelevant for MOVE and LINE segments.
 */
typedef struct
{
  /* 0 and 2 are control points, 1 is the point on the line */
  graphene_point_t p[3];
  PointType type;
  gboolean edit;
  int dragged;
  int hovered;
  /* refers to the segment following the point */
  Operation op;
} PointData;

struct _CurveEditor
{
  GtkWidget parent_instance;
  GArray *points;
  int dragged;
  int context;
  gboolean edit;
  int molded;

  GtkWidget *menu;
  GActionMap *actions;
  GskStroke *stroke;
  GdkRGBA color;
};

struct _CurveEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CurveEditor, curve_editor, GTK_TYPE_WIDGET)
/* }}} */
/* {{{ Misc. geometry */
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

/* Set q to the point on the line through p and a that is
 * at a distance of d from p, on the same side
 */
static void
scale_point (const graphene_point_t *p,
             const graphene_point_t *a,
             float                   d,
             graphene_point_t       *q)
{
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);
  t = sqrt (d * d / graphene_vec2_dot (&ap, &ap));

  q->x = p->x + t * (a->x - p->x);
  q->y = p->y + t * (a->y - p->y);
}

/* Set p to the intersection of the lines through a, b
 * and c, d
 */
static void
line_intersection (const graphene_point_t *a,
                   const graphene_point_t *b,
                   const graphene_point_t *c,
                   const graphene_point_t *d,
                   graphene_point_t       *p)
{
  double a1 = b->y - a->y;
  double b1 = a->x - b->x;
  double c1 = a1*a->x + b1*a->y;

  double a2 = d->y - c->y;
  double b2 = c->x - d->x;
  double c2 = a2*c->x+ b2*c->y;

  double det = a1*b2 - a2*b1;

  if (det == 0)
    {
      p->x = NAN;
      p->y = NAN;
    }
  else
    {
      p->x = (b2*c1 - b1*c2) / det;
      p->y = (a1*c2 - a2*c1) / det;
    }
}

/* Given 3 points, determine the center of a circle that
 * passes through all of them.
 */
static void
circle_through_points (const graphene_point_t *a,
                       const graphene_point_t *b,
                       const graphene_point_t *c,
                       graphene_point_t       *center)
{
  graphene_point_t ab;
  graphene_point_t ac;
  graphene_point_t ab2;
  graphene_point_t ac2;

  ab.x = (a->x + b->x) / 2;
  ab.y = (a->y + b->y) / 2;
  ac.x = (a->x + c->x) / 2;
  ac.y = (a->y + c->y) / 2;

  ab2.x = ab.x + a->y - b->y;
  ab2.y = ab.y + b->x - a->x;
  ac2.x = ac.x + a->y - c->y;
  ac2.y = ac.y + c->x - a->x;

  line_intersection (&ab, &ab2, &ac, &ac2, center);
}

/* Return the cosine of the angle between b1 - a and b2 - a */
static double
three_point_angle (const graphene_point_t *a,
                   const graphene_point_t *b1,
                   const graphene_point_t *b2)
{
  graphene_vec2_t u;
  graphene_vec2_t v;

  graphene_vec2_init (&u, b1->x - a->x, b1->y - a->y);
  graphene_vec2_init (&v, b2->x - a->x, b2->y - a->y);
  graphene_vec2_normalize (&u, &u);
  graphene_vec2_normalize (&v, &v);

  return graphene_vec2_dot (&u, &v);
}
/* }}} */
/* {{{ Misc. Bezier math */
/* Given Bezier control points and a t value between 0 and 1,
 * return new Bezier control points for two segments in left
 * and right that are obtained by splitting the curve at the
 * point for t.
 *
 * Note that the points in the right array are in returned in
 * reverse order.
 */
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
          if (i + 1 == length - 1)
            {
              right[*right_pos] = points[i + 1];
              (*right_pos)++;
            }
          graphene_point_interpolate (&points[i], &points[i + 1], t, &newpoints[i]);
        }
      split_bezier (newpoints, length - 1, t, left, left_pos, right, right_pos);
    }
}

static double
projection_ratio (double t)
{
  double top, bottom;

  if (t == 0 || t == 1)
    return t;

  top = pow (1 - t, 3),
  bottom = pow (t, 3) + top;

  return top / bottom;
}

static double
abc_ratio (double t)
{
  double top, bottom;

  if (t == 0 || t == 1)
    return t;

  bottom = pow (t, 3) + pow (1 - t, 3);
  top = bottom - 1;

  return fabs (top / bottom);
}

static void
find_control_points (double                  t,
                     const graphene_point_t *A,
                     const graphene_point_t *B,
                     const graphene_point_t *C,
                     const graphene_point_t *S,
                     const graphene_point_t *E,
                     graphene_point_t       *C1,
                     graphene_point_t       *C2)
{
  double angle;
  double dist;
  double bc;
  double de1;
  double de2;
  graphene_point_t c;
  graphene_point_t t0, t1;
  double tlength;
  double dx, dy;
  graphene_point_t e1, e2;
  graphene_point_t v1, v2;

  dist = graphene_point_distance (S, E, NULL, NULL);
  angle = atan2 (E->y - S->y, E->x - S->x) - atan2 (B->y - S->y, B->x - S->x);
  bc = (angle < 0 || angle > M_PI ? -1 : 1) * dist / 3;
  de1 = t * bc;
  de2 = (1 - t) * bc;

  circle_through_points (S, B, E, &c);

  t0.x = B->x - (B->y - c.y);
  t0.y = B->y + (B->x - c.x);
  t1.x = B->x + (B->y - c.y);
  t1.y = B->y - (B->x - c.x);

  tlength = graphene_point_distance (&t0, &t1, NULL, NULL);
  dx = (t1.x - t0.x) / tlength;
  dy = (t1.y - t0.y) / tlength;

  e1.x = B->x + de1 * dx;
  e1.y = B->y + de1 * dy;
  e2.x = B->x - de2 * dx;
  e2.y = B->y - de2 * dy;

  v1.x = A->x + (e1.x - A->x) / (1 - t);
  v1.y = A->y + (e1.y - A->y) / (1 - t);

  v2.x = A->x + (e2.x - A->x) / t;
  v2.y = A->y + (e2.y - A->y) / t;

  C1->x = S->x + (v1.x - S->x) / t;
  C1->y = S->y + (v1.y - S->y) / t;

  C2->x = E->x + (v2.x - E->x) / (1 - t);
  C2->y = E->y + (v2.y - E->y) / (1 - t);
}

/* Given points S, B, E, determine control
 * points C1, C2 such that B lies on the
 * Bezier segment given bY S, C1, C2, E.
 */
static void
bezier_through (const graphene_point_t *S,
                const graphene_point_t *B,
                const graphene_point_t *E,
                graphene_point_t       *C1,
                graphene_point_t       *C2)
{
  double d1, d2, t;
  double u, um, s;
  graphene_point_t A, C;

  d1 = graphene_point_distance (S, B, NULL, NULL);
  d2 = graphene_point_distance (E, B, NULL, NULL);
  t = d1 / (d1 + d2);

  u = projection_ratio (t);
  um = 1 - u;

  C.x = u * S->x + um * E->x;
  C.y = u * S->y + um * E->y;

  s = abc_ratio (t);

  A.x = B->x + (B->x - C.x) / s;
  A.y = B->y + (B->y - C.y) / s;

  find_control_points (t, &A, B, &C, S, E, C1, C2);
}
/* }}} */
/* {{{ Utilities */
static PointData *
get_point (CurveEditor *self,
           int          point)
{
  point = point % self->points->len;
  if (point < 0)
    point += self->points->len;
  return &g_array_index (self->points, PointData, point);
}

static gboolean
point_is_visible (CurveEditor *self,
                  int          point,
                  int          point1)
{
  PointData *pd;

  if (!self->edit)
    return FALSE;

  pd = get_point (self, point);
  switch (point1)
    {
    case 0:
      if (!pd->edit)
        return FALSE;
      else
        return get_point (self, point - 1)->op == CURVE;
    case 1: /* point on curve */
      return TRUE;
    case 2:
      if (!pd->edit)
        return FALSE;
      else
        return pd->op == CURVE;
    default:
      g_assert_not_reached ();
    }
}

static void
maintain_smoothness (CurveEditor *self,
                     int          point)
{
  PointData *pd;
  Operation op, op1;
  graphene_point_t *p, *c, *c2, *p2;
  float d;

  pd = get_point (self, point);

  if (pd->type == CUSP)
    return;

  op = pd->op;
  op1 = get_point (self, point - 1)->op;

  p = &pd->p[1];
  c = &pd->p[0];
  c2 = &pd->p[2];

  if (op == CURVE && op1 == CURVE)
    {
      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, c2, d, c);
    }
  else if (op == CURVE && op1 == LINE)
    {
      p2 = &get_point (self, point - 1)->p[1];
      d = graphene_point_distance (c2, p, NULL, NULL);
      opposite_point (p, p2, d, c2);
    }
  else if (op == LINE && op1 == CURVE)
    {
      p2 = &get_point (self, point + 1)->p[1];
      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, p2, d, c);
    }
}

static void
maintain_symmetry (CurveEditor *self,
                   int          point)
{
  PointData *pd;
  graphene_point_t *p, *c, *c2;
  double l1, l2, l;

  pd = get_point (self, point);

  if (pd->type != SYMMETRIC)
    return;

  c = &pd->p[0];
  p = &pd->p[1];
  c2 = &pd->p[2];

  l1 = graphene_point_distance (p, c, NULL, NULL);
  l2 = graphene_point_distance (p, c2, NULL, NULL);

  if (l1 != l2)
    {
      l = (l1 + l2) / 2;

      scale_point (p, c, l, c);
      scale_point (p, c2, l, c2);
    }
}

/* Make the line through the control points perpendicular
 * to the line bisecting the angle between neighboring
 * points, and make the lengths 1/3 of the distance to
 * the corresponding neighboring points.
 */
static void
update_automatic (CurveEditor *self,
                  int          point)
{
  PointData *pd, *pd1, *pd2;
  double l1, l2;
  graphene_point_t a;

  pd = get_point (self, point);

  if (pd->type != AUTO)
    return;

  pd1 = get_point (self, point - 1);
  pd2 = get_point (self, point + 1);

  l1 = graphene_point_distance (&pd->p[1], &pd1->p[1], NULL, NULL);
  l2 = graphene_point_distance (&pd->p[1], &pd2->p[1], NULL, NULL);

  a.x = pd2->p[1].x + (pd->p[1].x - pd1->p[1].x);
  a.y = pd2->p[1].y + (pd->p[1].y - pd1->p[1].y);

  scale_point (&pd->p[1], &a, l2/3, &pd->p[2]);
  opposite_point (&pd->p[1], &a, l1/3, &pd->p[0]);
}

static void
maintain_automatic (CurveEditor *self,
                    int          point)
{
  if (get_point (self, point)->op != CURVE ||
      get_point (self, point - 1)->op != CURVE)
    return;

  update_automatic (self, point);
  update_automatic (self, point - 1);
  update_automatic (self, point + 1);
}

/* Check if the points arount point currently satisfy
 * smoothness conditions. Set PointData.type accordingly.
 */
static void
check_smoothness (CurveEditor *self,
                  int          point)
{
  Operation op, op1;
  graphene_point_t *p1, *p2;
  PointData *pd;

  pd = get_point (self, point);
  op = pd->op;
  op1 = get_point (self, point - 1)->op;

  if (op == CURVE)
    p2 = &pd->p[2];
  else if (op == LINE)
    p2 = &get_point (self, point + 1)->p[1];
  else
    p2 = NULL;

  if (op1 == CURVE)
    p1 = &pd->p[0];
  else if (op1 == LINE)
    p1 = &get_point (self, point - 1)->p[1];
  else
    p1 = NULL;

  if (!p1 || !p2 || !collinear (&pd->p[1], p1, p2))
    pd->type = CUSP;
  else
    pd->type = SMOOTH;
}

static void
insert_point (CurveEditor *self,
              int          point,
              double       pos)
{
  PointData *pd, *pd1, *pd2;
  graphene_point_t points[4];
  PointData np;

  pd = get_point (self, point);
  if (pd->op == MOVE)
    return;

  pd1 = get_point (self, point + 1);
  points[0] = pd->p[1];
  points[1] = pd->p[2];
  points[2] = pd1->p[0];
  points[3] = pd1->p[1];

  g_array_insert_val (self->points, point + 1, np);

  pd = get_point (self, point);
  pd1 = get_point (self, point + 1);
  pd2 = get_point (self, point + 2);

  pd1->type = SMOOTH;
  pd1->hovered = -1;
  pd1->dragged = -1;

  if (pd->op == LINE)
    {
      pd1->op = LINE;
      graphene_point_interpolate (&points[0], &points[3], pos, &pd1->p[1]);
    }
  else if (pd->op == CURVE)
    {
      graphene_point_t left[4];
      graphene_point_t right[4];
      int left_pos = 0;
      int right_pos = 0;

      pd1->op = CURVE;

      split_bezier (points, 4, pos, left, &left_pos, right, &right_pos);

      pd->p[1] = left[0];
      pd->p[2] = left[1];
      pd1->p[0] = left[2];
      pd1->p[1] = left[3];
      pd1->p[2] = right[2];
      pd2->p[0] = right[1];
      pd2->p[1] = right[0];
    }
  else
    g_assert_not_reached ();

  maintain_smoothness (self, point + 1);
  maintain_automatic (self, point + 1);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_point (CurveEditor *self,
              int          point)
{
  g_array_remove_index (self->points, point);

  maintain_smoothness (self, point);
  maintain_automatic (self, point);
}

/* }}} */
/* {{{ GskPath helpers */ 
static void
curve_editor_add_segment (CurveEditor    *self,
                          GskPathBuilder *builder,
                          int             point)
{
  PointData *pd1, *pd;

  pd1 = get_point (self, point);
  pd = get_point (self, point + 1);

  gsk_path_builder_move_to (builder, pd1->p[1].x, pd1->p[1].y);

  switch (pd1->op)
    {
    case LINE:
      gsk_path_builder_line_to (builder, pd->p[1].x, pd->p[1].y);
      break;

    case CURVE:
      gsk_path_builder_curve_to (builder,
                                 pd1->p[2].x, pd1->p[2].y,
                                 pd->p[0].x, pd->p[0].y,
                                 pd->p[1].x, pd->p[1].y);
      break;

    case MOVE:
    default:
      break;
    }
}

static void
curve_editor_add_path (CurveEditor    *self,
                       GskPathBuilder *builder)
{
  int i;

  for (i = 0; i < self->points->len; i++)
    {
      PointData *pd1, *pd;

      pd1 = get_point (self, i);
      pd = get_point (self, i + 1);

      if (i == 0)
        gsk_path_builder_move_to (builder, pd1->p[1].x, pd1->p[1].y);

      switch (pd1->op)
        {
        case MOVE:
          gsk_path_builder_move_to (builder, pd->p[1].x, pd->p[1].y);
          break;

        case LINE:
          gsk_path_builder_line_to (builder, pd->p[1].x, pd->p[1].y);
          break;

        case CURVE:
          gsk_path_builder_curve_to (builder,
                                     pd1->p[2].x, pd1->p[2].y,
                                     pd->p[0].x, pd->p[0].y,
                                     pd->p[1].x, pd->p[1].y);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  gsk_path_builder_close (builder);
}

static gboolean
find_closest_segment (CurveEditor      *self,
                      graphene_point_t *point,
                      float             threshold,
                      graphene_point_t *p,
                      int              *segment,
                      float            *pos)
{
  graphene_point_t pp;
  float t;
  int seg;
  gboolean found = FALSE;
  int i;

  for (i = 0; i < self->points->len; i++)
    {
      GskPathBuilder *builder;
      GskPath *path;
      GskPathMeasure *measure;
      float t1;
      graphene_point_t pp1;

      builder = gsk_path_builder_new ();
      curve_editor_add_segment (self, builder, i);
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new (path);

      if (gsk_path_measure_get_closest_point_full (measure, point, threshold, &threshold, &pp1, &t1, NULL))
        {
          seg = i;
          t = t1 / gsk_path_measure_get_length (measure);
          pp = pp1;
          found = TRUE;
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }

  if (found)
    {
      if (segment)
        *segment = seg;
      if (pos)
        *pos = t;
      if (p)
        *p = pp;
    }

  return found;
}
/* }}} */
/* {{{ Drag implementation */
static void
drag_begin (GtkGestureDrag *gesture,
            double          start_x,
            double          start_y,
            CurveEditor     *self)
{
  int i, j;
  graphene_point_t p = GRAPHENE_POINT_INIT (start_x, start_y);
  float t;
  int point;

  if (!self->edit)
    return;

  for (i = 0; i < self->points->len; i++)
    {
      PointData *pd = get_point (self, i);

      for (j = 0; j < 3; j++)
        {
          if (graphene_point_distance (&pd->p[j], &p, NULL, NULL) < CLICK_RADIUS)
            {
              if (point_is_visible (self, i, j))
                {
                  self->dragged = i;
                  pd->dragged = j;
                  gtk_widget_queue_draw (GTK_WIDGET (self));
                }
              return;
            }
        }
    }

  if (find_closest_segment (self, &p, CLICK_RADIUS, NULL, &point, &t))
    {
      /* Can't bend a straight line */
      get_point (self, point)->op = CURVE;
      self->molded = point;
      return;
    }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}

static void
drag_control_point (CurveEditor *self,
                    double       x,
                    double       y)
{
  double dx, dy;
  graphene_point_t *c, *p, *d;
  double l1, l2;
  PointData *pd;

  pd = get_point (self, self->dragged);
  d = &pd->p[pd->dragged];

  /* before moving the point, record the distances to its neighbors, since
   * we may want to preserve those
   */
  l1 = graphene_point_distance (&pd->p[1], &pd->p[0], NULL, NULL);
  l2 = graphene_point_distance (&pd->p[1], &pd->p[2], NULL, NULL);

  dx = x - d->x;
  dy = y - d->y;

  if (pd->dragged == 1)
    {
      /* dragged point is on curve */

      Operation op, op1, op11, op2;
      PointData *pd1, *pd2;

      /* first move the point itself */
      d->x = x;
      d->y = y;

      /* adjust control points as needed */
      pd1 = get_point (self, self->dragged - 1);
      pd2 = get_point (self, self->dragged + 1);

      op = pd->op;
      op1 = pd1->op;
      op2 = pd2->op;

      if (op1 == LINE)
        {
          /* the other endpoint of the line */
          p = &pd1->p[1];

          if (op == CURVE && pd->type != CUSP)
            {
              /* adjust the control point after the line segment */
              opposite_point (d, p, l2, &pd->p[2]);
            }
          else
            {
              pd->p[2].x += dx;
              pd->p[2].y += dy;
            }

          pd->p[0].x += dx;
          pd->p[0].y += dy;

          op11 = get_point (self, self->dragged - 2)->op;

          if (op11 == CURVE && pd1->type != CUSP)
            {
              double l;

              /* adjust the control point before the line segment */
              l = graphene_point_distance (&pd1->p[0], p, NULL, NULL);
              opposite_point (p, d, l, &pd1->p[0]);
            }
        }

      if (op == LINE)
        {
          /* the other endpoint of the line */
          p = &pd2->p[1];

          if (op1 == CURVE && pd->type != CUSP)
            {
              /* adjust the control point before the line segment */
              opposite_point (d, p, l1, &pd->p[0]);
            }
          else
            {
              pd->p[0].x += dx;
              pd->p[0].y += dy;
            }

          pd->p[2].x += dx;
          pd->p[2].y += dy;

          if (op2 == CURVE && pd2->type != CUSP)
            {
              double l;

              /* adjust the control point after the line segment */
              l = graphene_point_distance (&pd2->p[2], p, NULL, NULL);
              opposite_point (p, d, l, &pd2->p[2]);
            }
        }

      if (op1 != LINE && op != LINE)
        {
          pd->p[0].x += dx;
          pd->p[0].y += dy;
          pd->p[2].x += dx;
          pd->p[2].y += dy;
        }

      maintain_automatic (self, self->dragged);
    }
  else
    {
      /* dragged point is a control point */

      graphene_point_t *p1;
      Operation op, op1;

      if (pd->dragged == 0)
        {
          c = &pd->p[2];
          p = &pd->p[1];

          op = get_point (self, self->dragged - 1)->op;
          op1 = get_point (self, self->dragged)->op;
          p1 = &get_point (self, self->dragged + 1)->p[1];
        }
      else if (pd->dragged == 2)
        {
          c = &pd->p[0];
          p = &pd->p[1];

          op = get_point (self, self->dragged)->op;
          op1 = get_point (self, self->dragged - 1)->op;
          p1 = &get_point (self, self->dragged - 1)->p[1];
        }
      else
        g_assert_not_reached ();

      if (op == CURVE && pd->type != CUSP)
        {
          if (op1 == CURVE)
            {
              double l;

              /* first move the point itself */
              d->x = x;
              d->y = y;

              /* then adjust the other control point */
              if (pd->type == SYMMETRIC)
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
}

static void
drag_curve (CurveEditor *self,
            double       x,
            double       y)
{
  PointData *pd, *pd1, *pd2, *pd3;
  graphene_point_t *S, *E;
  graphene_point_t B, C1, C2;
  double l;

  pd = get_point (self, self->molded);
  pd1 = get_point (self, self->molded + 1);
  pd2 = get_point (self, self->molded - 1);
  pd3 = get_point (self, self->molded + 2);

  S = &pd->p[1];
  B = GRAPHENE_POINT_INIT (x, y);
  E = &pd1->p[1];

  bezier_through (S, &B, E, &C1, &C2);

  pd->p[2] = C1;
  pd1->p[0] = C2;

  /* When the neighboring segments are lines, we can't actually
   * use C1 and C2 as-is, since we need control points to lie
   * on the line. So we just use their distance. This makes our
   * point B not quite match anymore, but we're overconstrained.
   */
  if (pd2->op == LINE)
    {
      l = graphene_point_distance (&pd->p[1], &pd->p[2], NULL, NULL);
      if (three_point_angle (&pd->p[1], &pd2->p[1], &B) > 0)
        scale_point (&pd->p[1], &pd2->p[1], l, &pd->p[2]);
      else
        opposite_point (&pd->p[1], &pd2->p[1], l, &pd->p[2]);
    }

  if (pd1->op == LINE)
    {
      l = graphene_point_distance (&pd1->p[1], &pd1->p[0], NULL, NULL);
      if (three_point_angle (&pd1->p[1], &pd3->p[1], &B) > 0)
        scale_point (&pd1->p[1], &pd3->p[1], l, &pd1->p[0]);
      else
        opposite_point (&pd1->p[1], &pd3->p[1], l, &pd1->p[0]);
    }

  /* Maintain smoothness and symmetry */
  if (pd->type != CUSP)
    {
      if (pd->type == SYMMETRIC)
        l = graphene_point_distance (&pd->p[1], &pd->p[2], NULL, NULL);
      else
        l = graphene_point_distance (&pd->p[1], &pd->p[0], NULL, NULL);
      opposite_point (&pd->p[1], &pd->p[2], l, &pd->p[0]);
    }

  if (pd1->type != CUSP)
    {
      if (pd1->type == SYMMETRIC)
        l = graphene_point_distance (&pd1->p[1], &pd1->p[0], NULL, NULL);
      else
        l = graphene_point_distance (&pd1->p[1], &pd1->p[2], NULL, NULL);
      opposite_point (&pd1->p[1], &pd1->p[0], l, &pd1->p[2]);
    }
}

static void
drag_update (GtkGestureDrag *gesture,
             double          offset_x,
             double          offset_y,
             CurveEditor     *self)
{
  double x, y;

  gtk_gesture_drag_get_start_point (gesture, &x, &y);

  x += offset_x;
  y += offset_y;

  if (self->dragged != -1)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      drag_control_point (self, x, y);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
  else if (self->molded != -1)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      drag_curve (self, x, y);
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
  self->dragged = -1;
  self->molded = -1;
}
/* }}} */
/* {{{ Action callbacks */
static void
set_point_type (GSimpleAction *action,
                GVariant      *value,
                gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  get_point (self, self->context)->type = point_type_from_string (g_variant_get_string (value, NULL));

  maintain_smoothness (self, self->context);
  maintain_symmetry (self, self->context);
  maintain_automatic (self, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_operation (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  get_point (self, self->context)->op = op_from_string (g_variant_get_string (value, NULL));

  maintain_smoothness (self, self->context);
  maintain_smoothness (self, self->context + 1);
  maintain_symmetry (self, self->context);
  maintain_symmetry (self, self->context + 1);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_current_point (GSimpleAction *action,
                      GVariant      *value,
                      gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  remove_point (self, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
 }
/* }}} */
/* {{{ Event handlers */
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

  if (button == GDK_BUTTON_SECONDARY)
    {
      for (i = 0; i < self->points->len; i++)
        {
          PointData *pd = get_point (self, i);

          if (graphene_point_distance (&pd->p[1], &m, NULL, NULL) < CLICK_RADIUS)
            {
              GAction *action;

              self->context = i;

              action = g_action_map_lookup_action (self->actions, "type");
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (point_type_to_string (pd->type)));

              action = g_action_map_lookup_action (self->actions, "operation");
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (op_to_string (pd->op)));

              gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                                           &(const GdkRectangle){ x, y, 1, 1 });
              gtk_popover_popup (GTK_POPOVER (self->menu));
              return;
            }
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

  for (i = 0; i < self->points->len; i++)
    {
      PointData *pd = get_point (self, i);

      if (graphene_point_distance (&pd->p[1], &m, NULL, NULL) < CLICK_RADIUS)
        {
          if (button == GDK_BUTTON_PRIMARY)
            {
              pd->edit = !pd->edit;
              gtk_widget_queue_draw (GTK_WIDGET (self));
              return;
            }
        }
    }

  if (button == GDK_BUTTON_PRIMARY)
    {
      float t;
      int point;

      if (find_closest_segment (self, &m, CLICK_RADIUS, NULL, &point, &t))
        {
          self->dragged = -1;
          self->molded = -1;
          insert_point (self, point, t);
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
  gboolean changed = FALSE;

  if (self->edit)
    {
      for (i = 0; i < self->points->len; i++)
        {
          PointData *pd = get_point (self, i);
          int hovered = -1;

          for (j = 0; j < 3; j++)
            {
              if (!point_is_visible (self, i, j))
                continue;

              if (graphene_point_distance (&pd->p[j], &m, NULL, NULL) < CLICK_RADIUS)
                {
                  hovered = j;
                  break;
                }
            }
          if (pd->hovered != hovered)
            {
              pd->hovered = hovered;
              changed = TRUE;
            }
        }
    }

  if (changed)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
leave (GtkEventController *controller,
       CurveEditor        *self)
{
  int i;
  gboolean changed = FALSE;

  for (i = 0; i < self->points->len; i++)
    {
      PointData *pd = get_point (self, i);
      if (pd->hovered != -1)
        {
          pd->hovered = -1;
          changed = TRUE;
        }
    }

  if (changed)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}
/* }}} */
/* {{{ Snapshot */
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

  if (self->points->len == 0)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  /* Add the curve itself */

  builder = gsk_path_builder_new ();

  curve_editor_add_path (self, builder);

  path = gsk_path_builder_free_to_path (builder);
  gtk_snapshot_push_stroke (snapshot, path, self->stroke);
  gsk_path_unref (path);

  gtk_snapshot_append_color (snapshot,
                             &self->color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);

  if (self->edit)
    {
      /* Add the skeleton */

      builder = gsk_path_builder_new ();

      for (i = 0; i < self->points->len; i++)
        {
          PointData *pd = get_point (self, i);
          gboolean need_move = TRUE;

          if (point_is_visible (self, i, 0))
            {
              gsk_path_builder_move_to (builder, pd->p[0].x, pd->p[0].y);
              gsk_path_builder_line_to (builder, pd->p[1].x, pd->p[1].y);
              need_move = FALSE;
            }
          if (point_is_visible (self, i, 2))
            {
              if (need_move)
                gsk_path_builder_move_to (builder, pd->p[1].x, pd->p[1].y);
              gsk_path_builder_line_to (builder, pd->p[2].x, pd->p[2].y);
            }
        }

      path = gsk_path_builder_free_to_path (builder);
      stroke = gsk_stroke_new (1);
      gtk_snapshot_push_stroke (snapshot, path, stroke);
      gsk_stroke_free (stroke);
      gsk_path_unref (path);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      /* Draw the circles, in several passes, one for each color */

      const char *colors[] = {
        "white", /* hovered */
        "red",   /* smooth curve points */
        "green", /* sharp curve points */
        "blue"   /* control points */
      };
      GdkRGBA color;

       for (k = 0; k < 4; k++)
        {
          builder = gsk_path_builder_new ();

          for (i = 0; i < self->points->len; i++)
            {
              PointData *pd = get_point (self, i);

              for (j = 0; j < 3; j++)
                {
                  switch (k)
                    {
                    case 0:
                      if (j != pd->hovered)
                        continue;
                      break;

                    case 1:
                      if (j == pd->hovered)
                        continue;

                      if (!(j == 1 && pd->type != CUSP))
                        continue;
                      break;

                    case 2:
                      if (j == pd->hovered)
                        continue;

                      if (!(j == 1 && pd->type == CUSP))
                        continue;
                      break;

                    case 3:
                      if (j == pd->hovered)
                        continue;

                      if (j == 1)
                        continue;

                      if (!point_is_visible (self, i, j))
                        continue;
                      break;

                    default:
                      g_assert_not_reached ();
                    }

                  gsk_path_builder_add_circle (builder, &pd->p[j], DRAW_RADIUS);
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

  gtk_native_check_resize (GTK_NATIVE (self->menu));
}
/* }}} */
/* {{{ GObject boilerplate */
static void
curve_editor_dispose (GObject *object)
{
  CurveEditor *self = CURVE_EDITOR (object);

  g_clear_pointer (&self->points, g_array_unref);
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

  self->points = g_array_new (FALSE, FALSE, sizeof (PointData));
  self->dragged = -1;
  self->molded = -1;
  self->edit = FALSE;
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

  action = g_simple_action_new_stateful ("type", G_VARIANT_TYPE_STRING, g_variant_new_string ("smooth"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_point_type), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  action = g_simple_action_new_stateful ("operation", G_VARIANT_TYPE_STRING, g_variant_new_string ("curve"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_operation), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("remove", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (remove_current_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  menu = g_menu_new ();

  section = g_menu_new ();

  item = g_menu_item_new ("Cusp", "point.type::cusp");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Smooth", "point.type::smooth");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Symmetric", "point.type::symmetric");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Automatic", "point.type::auto");
  g_menu_append_item (section, item);
  g_object_unref (item);


  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

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

  section = g_menu_new ();

  item = g_menu_item_new ("Remove", "point.remove");
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
  int i;

  self->edit = edit;
  if (!self->edit)
    {
      for (i = 0; i < self->points->len; i++)
        {
          PointData *pd = get_point (self, i);
          pd->edit = FALSE;
          pd->hovered = -1;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
copy_segments (GskPathOperation        op,
               const graphene_point_t *pts,
               gsize                   n_pts,
               float                   weight,
               gpointer                data)
{
  CurveEditor *self = data;
  PointData *pd;
  PointData *pd1;
  PointData np;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (self->points->len > 0)
        {
          pd = &g_array_index (self->points, PointData, self->points->len - 1);
          pd->op = MOVE;
        }

      np.p[1] = pts[0];
      g_array_append_val (self->points, np);
      break;

    case GSK_PATH_CLOSE:
      pd = &g_array_index (self->points, PointData, self->points->len - 1);
      pd1 = &g_array_index (self->points, PointData, 0);
      if (graphene_point_near (&pd->p[1], &pd1->p[1], 0.001))
        {
          pd1->p[0] = pd->p[0];
          g_array_remove_index (self->points, self->points->len - 1);
        }
      break;

    case GSK_PATH_LINE:
      pd = &g_array_index (self->points, PointData, self->points->len - 1);
      pd->op = LINE;
      np.p[1] = pts[1];
      g_array_append_val (self->points, np);
      break;

    case GSK_PATH_CURVE:
      pd = &g_array_index (self->points, PointData, self->points->len - 1);
      pd->op = CURVE;
      pd->p[2] = pts[1];
      np.p[0] = pts[2];
      np.p[1] = pts[3];
      g_array_append_val (self->points, np);
      break;

    case GSK_PATH_CONIC:
      /* FIXME */

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}
void
curve_editor_set_path (CurveEditor *self,
                       GskPath     *path)
{
  int i;

  g_array_set_size (self->points, 0);

  gsk_path_foreach (path, copy_segments, self);

  for (i = 0; i < self->points->len; i++)
    {
      PointData *pd = get_point (self, i);
      pd->hovered = -1;
      pd->dragged = -1;
      pd->edit= FALSE;
      check_smoothness (self, i);
    }

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
curve_editor_set_color (CurveEditor *self,
                        GdkRGBA     *color)
{
  self->color = *color;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

const GdkRGBA *
curve_editor_get_color (CurveEditor *self)
{
  return &self->color;
}
/* }}} */
/* vim:set foldmethod=marker expandtab: */
