#include "curve-editor.h"

#include <gtk/gtk.h>

#define DRAW_RADIUS 5
#define CLICK_RADIUS 8

/* {{{ Types and structures */

static const char *
op_to_string (GskPathOperation op)
{
  switch (op)
    {
    case GSK_PATH_MOVE:
      return "move";
    case GSK_PATH_LINE:
      return "line";
    case GSK_PATH_CURVE:
      return "curve";
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
  else if (strcmp (s, "curve") == 0)
    return GSK_PATH_CURVE;
  else if (strcmp (s, "conic") == 0)
    return GSK_PATH_CONIC;
  else if (strcmp (s, "close") == 0)
    return GSK_PATH_CLOSE;
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

typedef struct
{
  GskPathOperation op;
  graphene_point_t p[4];
  float weight;
  PointType type;
  int dragged;
  int hovered;
} Segment;

struct _CurveEditor
{
  GtkWidget parent_instance;
  GArray *segments;
  int context;
  float context_pos;
  gboolean edit;
  int edited_point;
  int edited_segment;
  int molded;
  int dragged;

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

static void
find_point_on_line (const graphene_point_t *p1,
                    const graphene_point_t *p2,
                    const graphene_point_t *q,
                    float                  *t)
{
  float tx = p2->x - p1->x;
  float ty = p2->y - p1->y;
  float sx = q->x - p1->x;
  float sy = q->y - p1->y;

  *t = (tx*sx + ty*sy) / (tx*tx + ty*ty);
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

/* conics */

static void
get_conic_shoulder_point (const graphene_point_t  p[3],
                          float                   w,
                          graphene_point_t       *q)
{
  graphene_point_t m;

  graphene_point_interpolate (&p[0], &p[2], 0.5, &m);
  graphene_point_interpolate (&m, &p[1], w / (1 + w), q);
}

static void
split_bezier3d_recurse (const graphene_point3d_t *p,
                        int                       l,
                        float                     t,
                        graphene_point3d_t       *left,
                        graphene_point3d_t       *right,
                        int                      *lpos,
                        int                      *rpos)
{
  if (l == 1)
    {
      left[*lpos] = p[0];
      right[*rpos] = p[0];
    }
  else
    {
      graphene_point3d_t *np;
      int i;

      np = g_alloca (sizeof (graphene_point3d_t) * (l - 1));
      for (i = 0; i < l - 1; i++)
        {
          if (i == 0)
            {
              left[*lpos] = p[i];
              (*lpos)++;
            }
          if (i + 1 == l - 1)
            {
              right[*rpos] = p[i + 1];
              (*rpos)--;
            }
          graphene_point3d_interpolate (&p[i], &p[i + 1], t, &np[i]);
        }
      split_bezier3d_recurse (np, l - 1, t, left, right, lpos, rpos);
    }
}

static void
split_bezier3d (const graphene_point3d_t *p,
                int                       l,
                float                     t,
                graphene_point3d_t       *left,
                graphene_point3d_t       *right)
{
  int lpos = 0;
  int rpos = l - 1;
  split_bezier3d_recurse (p, l, t, left, right, &lpos, &rpos);
}

static void
split_conic (const graphene_point_t points[3], float weight,
             float t,
             graphene_point_t lp[3], float *lw,
             graphene_point_t rp[3], float *rw)
{
  /* Given control points and weight for a rational quadratic
   * Bezier and t, create two sets of the same that give the
   * same curve as the original and split the curve at t.
   */
  graphene_point3d_t p[3];
  graphene_point3d_t l[3], r[3];
  int i;

  /* do de Casteljau in homogeneous coordinates... */
  for (i = 0; i < 3; i++)
    {
      p[i].x = points[i].x;
      p[i].y = points[i].y;
      p[i].z = 1;
    }

  p[1].x *= weight;
  p[1].y *= weight;
  p[1].z *= weight;

  split_bezier3d (p, 3, t, l, r);

  /* then project the control points down */
  for (i = 0; i < 3; i++)
    {
      lp[i].x = l[i].x / l[i].z;
      lp[i].y = l[i].y / l[i].z;
      rp[i].x = r[i].x / r[i].z;
      rp[i].y = r[i].y / r[i].z;
    }

  /* normalize the outer weights to be 1 by using
   * the fact that weights w_i and c*w_i are equivalent
   * for any nonzero constant c
   */
  for (i = 0; i < 3; i++)
    {
      l[i].z /= l[0].z;
      r[i].z /= r[2].z;
    }

  /* normalize the inner weight to be 1 by using
   * the fact that w_0*w_2/w_1^2 is a constant for
   * all equivalent weights.
   */
  *lw = l[1].z / sqrt (l[2].z);
  *rw = r[1].z / sqrt (r[0].z);
}

/* }}} */
/* {{{ Utilities */

static Segment *
get_segment (CurveEditor *self,
             int          idx)
{
  idx = idx % (int)self->segments->len;
  if (idx < 0)
    idx += (int)self->segments->len;
  return &g_array_index (self->segments, Segment, idx);
}

static void
set_segment_start (CurveEditor      *self,
                   int               idx,
                   graphene_point_t *p)
{
  Segment *seg = get_segment (self, idx);
  Segment *seg1 = get_segment (self, idx - 1);

  seg->p[0] = *p;
  seg1->p[3] = *p;
}

static const graphene_point_t *
get_line_point (CurveEditor *self,
                int          idx)
{
  Segment *seg = get_segment (self, idx);
  return &seg->p[0];
}

static graphene_point_t *
get_left_control_point (CurveEditor *self,
                        int          idx)
{
  Segment *seg = get_segment (self, idx - 1);
  return &seg->p[2];
}

static graphene_point_t *
get_right_control_point (CurveEditor *self,
                         int          idx)
{
  Segment *seg = get_segment (self, idx);
  return &seg->p[1];
}

static gboolean
point_is_visible (CurveEditor *self,
                  int          point,
                  int          point1)
{
  Segment *seg;

  if (!self->edit)
    return FALSE;

  seg = get_segment (self, point);
  switch (point1)
    {
    case 0: /* point on curve */
      return TRUE;

    case 1:
      if (self->edited_segment == point &&
          seg->op != GSK_PATH_LINE)
        return TRUE;
      if (seg->op == GSK_PATH_CONIC &&
          (self->edited_point == point + 1 ||
           (self->edited_point == 0 && point + 1 == self->segments->len)))
        return TRUE;
      if (self->edited_point == point &&
          (seg->op == GSK_PATH_CURVE || seg->op == GSK_PATH_CONIC))
        return TRUE;
      break;

    case 2:
      if (self->edited_segment == point &&
          seg->op != GSK_PATH_LINE)
        return TRUE;
      if (seg->op == GSK_PATH_CURVE &&
          (self->edited_point == point + 1 ||
           (self->edited_point == 0 && point + 1 == self->segments->len)))
        return TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  return FALSE;
}

static void
maintain_smoothness (CurveEditor *self,
                     int          point)
{
  Segment *seg, *seg1;
  const graphene_point_t *p, *p2;
  graphene_point_t *c, *c2;
  float d;

  seg = get_segment (self, point);
  seg1 = get_segment (self, point - 1);

  if (seg->type == CUSP)
    return;

  if (seg->op == GSK_PATH_LINE && seg1->op == GSK_PATH_LINE)
    return;

  p = &seg->p[0];
  c = &seg1->p[2];
  c2 = &seg->p[1];

  if (seg->op == GSK_PATH_CURVE && seg1->op == GSK_PATH_CURVE)
    {
      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, c2, d, c);
    }
  else if (seg->op == GSK_PATH_CURVE)
    {
      if (seg1->op == GSK_PATH_LINE)
        p2 = &seg1->p[0];
      else if (seg1->op == GSK_PATH_CONIC)
        p2 = &seg1->p[1];
      else
        g_assert_not_reached ();
      d = graphene_point_distance (c2, p, NULL, NULL);
      opposite_point (p, p2, d, c2);
    }
  else if (seg1->op == GSK_PATH_CURVE)
    {
      if (seg->op == GSK_PATH_LINE)
        p2 = &seg->p[3];
      else if (seg->op == GSK_PATH_CONIC)
        p2 = &seg->p[1];
      else
        g_assert_not_reached ();
      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, p2, d, c);
    }
  else if (seg->op == GSK_PATH_CONIC && seg1->op == GSK_PATH_CONIC)
    {
      graphene_point_t h, a, b;

      h.x = seg->p[0].x + seg->p[1].x - seg1->p[1].x;
      h.y = seg->p[0].y + seg->p[1].y - seg1->p[1].y;
      line_intersection (&seg->p[0], &h, &seg1->p[0], &seg1->p[1], &a);
      line_intersection (&seg->p[0], &h, &seg->p[1], &seg->p[3], &b);

      seg1->p[1] = a;
      seg->p[1] = b;
    }
}

static void
maintain_symmetry (CurveEditor *self,
                   int          point)
{
  Segment *seg, *seg1;
  const graphene_point_t *p;
  graphene_point_t *c, *c2;
  double l1, l2, l;

  seg = get_segment (self, point);
  seg1 = get_segment (self, point - 1);

  if (seg->type != SYMMETRIC)
    return;

  if (seg->op != GSK_PATH_CURVE || seg1->op != GSK_PATH_CURVE)
    return;

  p = &seg->p[0];
  c = &seg1->p[2];
  c2 = &seg->p[1];

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
  Segment *seg;
  const graphene_point_t *p, *p1, *p2;
  double l1, l2;
  graphene_point_t a;
  graphene_point_t *c1, *c2;

  seg = get_segment (self, point);

  if (seg->type != AUTO)
    return;

  if (seg->op != GSK_PATH_CURVE || get_segment (self, point - 1)->op != GSK_PATH_CURVE)
    return;

  p = get_line_point (self, point);
  c1 = get_left_control_point (self, point);
  c2 = get_right_control_point (self, point);

  p1 = get_line_point (self, point - 1);
  p2 = get_line_point (self, point + 1);

  l1 = graphene_point_distance (p, p1, NULL, NULL);
  l2 = graphene_point_distance (p, p2, NULL, NULL);

  a.x = p2->x + (p->x - p1->x);
  a.y = p2->y + (p->y - p1->y);

  scale_point (p, &a, l2/3, c2);
  opposite_point (p, &a, l1/3, c1);
}

static void
maintain_automatic (CurveEditor *self,
                    int          point)
{
  if (get_segment (self, point)->op != GSK_PATH_CURVE ||
      get_segment (self, point - 1)->op != GSK_PATH_CURVE)
    return;

  update_automatic (self, point);
  update_automatic (self, point - 1);
  update_automatic (self, point + 1);
}

static void
maintain_conic (CurveEditor *self,
                int          idx)
{
  Segment *seg = get_segment (self, idx);
  graphene_point_t p[3];

  if (seg->op != GSK_PATH_CONIC)
    return;

  p[0] = seg->p[0];
  p[1] = seg->p[1];
  p[2] = seg->p[3];

  get_conic_shoulder_point (p, seg->weight, &seg->p[2]);
}

/* Check if the points arount point currently satisfy
 * smoothness conditions. Set PointData.type accordingly.
 */
static void
check_smoothness (CurveEditor *self,
                  int          point)
{
  GskPathOperation op, op1;
  const graphene_point_t *p, *p1, *p2;
  Segment *seg, *seg1;

  seg = get_segment (self, point);
  seg1 = get_segment (self, point - 1);
  p = get_line_point (self, point);

  op = seg->op;
  op1 = seg1->op;

  if (op == GSK_PATH_CURVE)
    p2 = get_right_control_point (self, point);
  else if (op == GSK_PATH_LINE)
    p2 = get_line_point (self, point + 1);
  else
    p2 = NULL;

  if (op1 == GSK_PATH_CURVE)
    p1 = get_left_control_point (self, point);
  else if (op1 == GSK_PATH_LINE)
    p1 = get_line_point (self, point - 1);
  else
    p1 = NULL;

  if (!p1 || !p2 || !collinear (p, p1, p2))
    seg->type = CUSP;
  else
    seg->type = SMOOTH;
}

static void
insert_point (CurveEditor *self,
              int          point,
              double       pos)
{
  Segment *seg, *seg1, *seg2;
  Segment ns = { 0, };;

  seg = get_segment (self, point);
  if (seg->op == GSK_PATH_MOVE)
    return;

  g_array_insert_val (self->segments, point + 1, ns);

  seg = get_segment (self, point);
  seg1 = get_segment (self, point + 1);
  seg2 = get_segment (self, point + 2);

  seg1->type = SMOOTH;
  seg1->hovered = -1;
  seg1->dragged = -1;

  switch (seg->op)
    {
    case GSK_PATH_LINE:
      seg1->op = GSK_PATH_LINE;

      graphene_point_interpolate (&seg->p[0], &seg->p[3], pos, &seg1->p[0]);
      seg->p[3] = seg->p[0];
      seg1->p[3] = seg2->p[0];
      break;

    case GSK_PATH_CURVE:
      {
        graphene_point_t left[4];
        graphene_point_t right[4];
        int left_pos = 0;
        int right_pos = 0;

        seg1->op = GSK_PATH_CURVE;

        split_bezier (seg->p, 4, pos, left, &left_pos, right, &right_pos);

        seg->p[0] = left[0];
        seg->p[1] = left[1];
        seg->p[2] = left[2];
        seg->p[3] = left[3];
        seg1->p[0] = right[3];
        seg1->p[1] = right[2];
        seg1->p[2] = right[1];
        seg1->p[3] = right[0];
      }
      break;

    case GSK_PATH_CONIC:
      {
        graphene_point_t points[3];
        graphene_point_t left[3];
        graphene_point_t right[3];
        float lw, rw;

        seg1->op = GSK_PATH_CONIC;

        points[0] = seg->p[0];
        points[1] = seg->p[1];
        points[2] = seg->p[3];
        split_conic (points, seg->weight, pos, left, &lw, right, &rw);

        seg->p[0] = left[0];
        seg->p[1] = left[1];
        seg->p[3] = left[2];
        seg1->p[0] = right[0];
        seg1->p[1] = right[1];
        seg1->p[3] = right[2];

        seg->weight = lw;
        seg1->weight = rw;

        get_conic_shoulder_point (seg->p, seg->weight, &seg->p[2]);
        get_conic_shoulder_point (seg1->p, seg1->weight, &seg1->p[2]);
      }
      break;

    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
      break;
    }

  maintain_smoothness (self, point + 1);
  maintain_automatic (self, point + 1);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_point (CurveEditor *self,
              int          point)
{
  Segment *seg;
  graphene_point_t c, p;

  seg = get_segment (self, point);
  c = seg->p[2];
  p = seg->p[3];

  g_array_remove_index (self->segments, point);

  seg = get_segment (self, point - 1);
  seg->p[2] = c;
  seg->p[3] = p;

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
  Segment *seg;

  seg = get_segment (self, point);

  gsk_path_builder_move_to (builder, seg->p[0].x, seg->p[0].y);

  switch (seg->op)
    {
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, seg->p[3].x, seg->p[3].y);
      break;

    case GSK_PATH_CURVE:
      gsk_path_builder_curve_to (builder,
                                 seg->p[1].x, seg->p[1].y,
                                 seg->p[2].x, seg->p[2].y,
                                 seg->p[3].x, seg->p[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder,
                                 seg->p[1].x, seg->p[1].y,
                                 seg->p[3].x, seg->p[3].y,
                                 seg->weight);
      break;

    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      break;
    }
}

static void
curve_editor_add_path (CurveEditor    *self,
                       GskPathBuilder *builder)
{
  int i;

  for (i = 0; i < self->segments->len; i++)
    {
      Segment *seg = get_segment (self, i);

      if (i == 0)
        gsk_path_builder_move_to (builder, seg->p[0].x, seg->p[0].y);

      switch (seg->op)
        {
        case GSK_PATH_MOVE:
          gsk_path_builder_move_to (builder, seg->p[3].x, seg->p[3].y);
          break;

        case GSK_PATH_LINE:
          gsk_path_builder_line_to (builder, seg->p[3].x, seg->p[3].y);
          break;

        case GSK_PATH_CURVE:
          gsk_path_builder_curve_to (builder,
                                     seg->p[1].x, seg->p[1].y,
                                     seg->p[2].x, seg->p[2].y,
                                     seg->p[3].x, seg->p[3].y);
          break;

        case GSK_PATH_CONIC:
          gsk_path_builder_conic_to (builder,
                                     seg->p[1].x, seg->p[1].y,
                                     seg->p[3].x, seg->p[3].y,
                                     seg->weight);
          break;

        case GSK_PATH_CLOSE:
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

  for (i = 0; i < self->segments->len; i++)
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
  int idx;

  if (!self->edit)
    return;

  for (i = 0; i < self->segments->len; i++)
    {
      Segment *seg = get_segment (self, i);

      for (j = 0; j < 3; j++)
        {
          if (graphene_point_distance (&seg->p[j], &p, NULL, NULL) < CLICK_RADIUS)
            {
              if (point_is_visible (self, i, j))
                {
                  self->dragged = i;
                  seg->dragged = j;
                  gtk_widget_queue_draw (GTK_WIDGET (self));
                }
              return;
            }
        }
    }

  if (find_closest_segment (self, &p, CLICK_RADIUS, NULL, &idx, &t))
    {
      /* Can't bend a straight line */
      get_segment (self, idx)->op = GSK_PATH_CURVE;
      self->molded = idx;
      return;
    }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}

static void
drag_line_point (CurveEditor *self,
                 double       x,
                 double       y)
{
  /* dragged point is on curve */
  Segment *seg, *seg1, *seg2, *seg11;
  const graphene_point_t *d, *p;
  graphene_point_t *c;
  float l1, l2, dx, dy;

  seg = get_segment (self, self->dragged);
  d = get_line_point (self, self->dragged);

  /* before moving the point, record the distances to its neighbors, since
   * we may want to preserve those
   */
  l1 = graphene_point_distance (d, get_left_control_point (self, self->dragged), NULL, NULL);
  l2 = graphene_point_distance (d, get_right_control_point (self, self->dragged), NULL, NULL);

  dx = x - d->x;
  dy = y - d->y;

  /* first move the point itself */
  set_segment_start (self, self->dragged, &GRAPHENE_POINT_INIT (x, y));

  /* adjust control points as needed */
  seg1 = get_segment (self, self->dragged - 1);
  seg2 = get_segment (self, self->dragged + 1);

  if (seg1->op == GSK_PATH_LINE)
    {
      /* the other endpoint of the line */
      p = get_line_point (self, self->dragged - 1);
      c = get_right_control_point (self, self->dragged);

      if (seg->op == GSK_PATH_CURVE && seg->type != CUSP)
        {
          opposite_point (d, p, l2, c);
        }
      else if (seg->op == GSK_PATH_CONIC && seg->type != CUSP)
        {
          graphene_point_t u;
          line_intersection (&seg1->p[0], &seg1->p[3], &seg->p[3], &seg->p[1], &u);
          if (u.x != NAN)
            seg->p[1] = u;
          else
            {
              seg->p[1].x += dx;
              seg->p[1].y += dy;
            }

          maintain_conic (self, self->dragged);
        }
      else
        {
          c->x += dx;
          c->y += dy;
        }

      /* always move the other control point along */
      c = get_left_control_point (self, self->dragged);
      c->x += dx;
      c->y += dy;

      /* handle the far end of the line */
      seg11 = get_segment (self, self->dragged - 2);

      if (seg11->op == GSK_PATH_CURVE && seg1->type != CUSP)
        {
          double l;
          const graphene_point_t *p2;
          graphene_point_t *c2;

          p2 = get_line_point (self, self->dragged - 1);
          c2 = get_left_control_point (self, self->dragged - 1);
          /* adjust the control point before the line segment */
          l = graphene_point_distance (c2, p2, NULL, NULL);
          opposite_point (p2, d, l, c2);
        }
      else if (seg11->op == GSK_PATH_CONIC && seg1->type != CUSP)
        {
          graphene_point_t u;
          line_intersection (&seg11->p[0], &seg11->p[1], &seg1->p[3], &seg1->p[0], &u);
          if (u.x != NAN)
            seg11->p[1] = u;

          maintain_conic (self, self->dragged - 2);
        }
    }

  if (seg->op == GSK_PATH_LINE)
    {
      /* the other endpoint of the line */
      p = get_line_point (self, self->dragged + 1);
      c = get_left_control_point (self, self->dragged);

      if (seg1->op == GSK_PATH_CURVE && seg->type != CUSP)
        {
          /* adjust the control point before the line segment */
          opposite_point (d, p, l1, c);
        }
      else if (seg1->op == GSK_PATH_CONIC && seg->type != CUSP)
        {
          graphene_point_t u;
          line_intersection (&seg1->p[0], &seg1->p[1], &seg->p[0], &seg->p[3], &u);
          if (u.x != NAN)
            seg1->p[1] = u;
          else
            {
              seg1->p[1].x += dx;
              seg1->p[1].y += dy;
            }

          maintain_conic (self, self->dragged);
        }
      else if (seg1->op == GSK_PATH_CURVE)
        {
          c->x += dx;
          c->y += dy;
        }

      /* always move the other control point along */
      c = get_right_control_point (self, self->dragged);
      c->x += dx;
      c->x += dy;

      /* handle the other end of the line */
      if (seg2->op == GSK_PATH_CURVE && seg2->type != CUSP)
        {
          double l;

          /* adjust the control point after the line segment */
          c = get_right_control_point (self, self->dragged + 1);
          l = graphene_point_distance (c, p, NULL, NULL);
          opposite_point (p, d, l, c);
        }
      else if (seg2->op == GSK_PATH_CONIC && seg2->type != CUSP)
        {
          graphene_point_t u;
          line_intersection (&seg->p[0], &seg->p[3], &seg2->p[1], &seg2->p[3], &u);
          if (u.x != NAN)
            seg2->p[1] = u;

          maintain_conic (self, self->dragged + 1);
        }
    }

  if (seg1->op != GSK_PATH_LINE && seg->op != GSK_PATH_LINE)
    {
      if (seg1->op == GSK_PATH_CURVE)
        {
          c = &seg1->p[2];
          c->x += dx;
          c->y += dy;
        }
      else if (seg1->op == GSK_PATH_CONIC && seg->type != CUSP)
        {
          graphene_point_t a, b;

          a.x = seg1->p[1].x + dx;
          a.y = seg1->p[1].y + dy;
          line_intersection (&seg->p[0], &a, &seg1->p[0], &seg1->p[1], &b);
          seg1->p[1] = b;
        }

      if (seg->op == GSK_PATH_CURVE)
        {
          c = &seg->p[1];
          c->x += dx;
          c->y += dy;
        }
      else if (seg->op == GSK_PATH_CONIC && seg->type != CUSP)
        {
          graphene_point_t a, b;

          a.x = seg->p[1].x + dx;
          a.y = seg->p[1].y + dy;
          line_intersection (&seg->p[3], &seg->p[1], &a, &seg->p[0], &b);
          seg->p[1] = b;
        }
    }

  maintain_smoothness (self, self->dragged);
  maintain_automatic (self, self->dragged);
  maintain_conic (self, self->dragged);
  maintain_conic (self, self->dragged - 1);
}

static void
drag_conic_point (CurveEditor *self,
                  float        x,
                  float        y)
{
  Segment *seg, *seg1, *seg2;
  graphene_point_t *d, *c1;
  float l;

  seg = get_segment (self, self->dragged);
  g_assert (seg->op == GSK_PATH_CONIC);
  d = &seg->p[seg->dragged];

  seg1 = get_segment (self, self->dragged + 1);
  seg2 = get_segment (self, self->dragged - 1);

  if (seg->dragged == 1)
    {
      if (seg->type != CUSP && seg2->op == GSK_PATH_LINE)
        {
          /* control point must be on the line of seg2 */

          if (seg1->type != CUSP && seg1->op == GSK_PATH_LINE)
            {
              graphene_point_t c;

              line_intersection (&seg1->p[0], &seg1->p[3], &seg2->p[3], &seg2->p[0], &c);
              if (c.x != NAN)
                *d = c; /* unmoveable */
              else
                {
                  closest_point (&GRAPHENE_POINT_INIT (x, y), &seg1->p[0], &seg1->p[3], &c);
                  *d = c;
                }
            }
          else
            {
              graphene_point_t c;

              closest_point (&GRAPHENE_POINT_INIT (x, y), &seg2->p[0], &seg2->p[3], &c);
              *d = c;

              if (seg1->type != CUSP)
                {
                  l = graphene_point_distance (&seg1->p[0], &seg1->p[1], NULL, NULL);
                  opposite_point (&seg1->p[0], d, l, &seg1->p[1]);
                }
            }
        }
      else if (seg1->type != CUSP && seg1->op == GSK_PATH_LINE)
        {
          graphene_point_t c;

          closest_point (&GRAPHENE_POINT_INIT (x, y), &seg1->p[0], &seg1->p[3], &c);
          *d = c;

          if (seg2->type != CUSP)
            {
              if (seg2->op == GSK_PATH_CURVE)
                c1 = &seg2->p[2];
              else if (seg2->op == GSK_PATH_CONIC)
                c1 = &seg2->p[1];
              else
                g_assert_not_reached ();
              l = graphene_point_distance (&seg2->p[3], c1, NULL, NULL);
              opposite_point (&seg2->p[3], d, l, c1);
            }
        }
      else
        {
          /* unconstrained */
          d->x = x;
          d->y = y;

          if (seg1->type != CUSP)
            {
              l = graphene_point_distance (&seg1->p[0], &seg1->p[1], NULL, NULL);
              opposite_point (&seg1->p[0], d, l, &seg1->p[1]);
            }

          if (seg2->type != CUSP)
            {
              if (seg2->op == GSK_PATH_CURVE)
                c1 = &seg2->p[2];
              else if (seg2->op == GSK_PATH_CONIC)
                c1 = &seg2->p[1];
              else
                g_assert_not_reached ();
              l = graphene_point_distance (&seg2->p[3], c1, NULL, NULL);
              opposite_point (&seg2->p[3], d, l, c1);
            }
        }
    }
  else if (seg->dragged == 2)
    {
      /* dragging the shoulder point */
      graphene_point_t m;
      float t;

      graphene_point_interpolate (&seg->p[0], &seg->p[3], 0.5, &m);
      find_point_on_line (&m, &seg->p[1], &GRAPHENE_POINT_INIT (x, y), &t);
      t = CLAMP (t, 0, 0.9);
      seg->weight = - t / (t - 1);
    }

  maintain_conic (self, self->dragged);
}

static void
drag_control_point (CurveEditor *self,
                    float        x,
                    float        y)
{
  /* dragged point is a control point */
  Segment *seg, *seg1;
  const graphene_point_t *p, *p1;
  graphene_point_t *c, *d;
  PointType type;

  seg = get_segment (self, self->dragged);
  g_assert (seg->op == GSK_PATH_CURVE);
  d = &seg->p[seg->dragged];

  if (seg->dragged == 2)
    {
      seg1 = get_segment (self, self->dragged + 1);
      p = &seg1->p[0];
      c = &seg1->p[1];
      type = seg1->type;
      p1 = get_line_point (self, self->dragged + 2);
    }
  else if (seg->dragged == 1)
    {
      seg1 = get_segment (self, self->dragged - 1);
      if (seg1->op == GSK_PATH_CONIC)
        c = &seg1->p[1];
      else
        c = &seg1->p[2];
      p = &seg->p[0];
      type = seg->type;
      p1 = &seg1->p[0];
    }
  else
    g_assert_not_reached ();

  if (type != CUSP)
    {
      if (seg1->op == GSK_PATH_CURVE)
        {
          double l;

          /* first move the point itself */
          d->x = x;
          d->y = y;

          /* then adjust the other control point */
          if (type == SYMMETRIC)
            l = graphene_point_distance (d, p, NULL, NULL);
          else
            l = graphene_point_distance (c, p, NULL, NULL);

          opposite_point (p, d, l, c);
        }
      else if (seg1->op == GSK_PATH_CONIC)
        {
          graphene_point_t u;

          d->x = x;
          d->y = y;
          line_intersection (p1, c, p, d, &u);
          *c = u;

          maintain_conic (self, self->dragged - 1);
          maintain_conic (self, self->dragged + 1);
        }
      else if (seg1->op == GSK_PATH_LINE)
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

static void
drag_point (CurveEditor *self,
            double       x,
            double       y)
{
  Segment *seg = get_segment (self, self->dragged);

  if (seg->dragged == 0)
    drag_line_point (self, x, y);
  else if (seg->op == GSK_PATH_CONIC)
    drag_conic_point (self, x, y);
  else
    drag_control_point (self, x, y);
}

static void
drag_curve (CurveEditor *self,
            double       x,
            double       y)
{
  graphene_point_t *S, *E;
  graphene_point_t B, C1, C2;
  double l;
  Segment *seg, *seg1, *seg2;

  seg = get_segment (self, self->molded);
  seg1 = get_segment (self, self->molded + 1);
  seg2 = get_segment (self, self->molded - 1);

  if (seg->op == GSK_PATH_CONIC)
    {
      /* FIXME */
      return;
    }

  S = &seg->p[0];
  B = GRAPHENE_POINT_INIT (x, y);
  E = &seg->p[3];

  bezier_through (S, &B, E, &C1, &C2);

  seg->p[1] = C1;
  seg->p[2] = C2;

  /* When the neighboring segments are lines, we can't actually
   * use C1 and C2 as-is, since we need control points to lie
   * on the line. So we just use their distance. This makes our
   * point B not quite match anymore, but we're overconstrained.
   */
  if (seg2->op == GSK_PATH_LINE)
    {
      l = graphene_point_distance (&seg->p[3], &C1, NULL, NULL);
      if (three_point_angle (&seg2->p[3], &seg2->p[0], &B) > 0)
        scale_point (&seg2->p[3], &seg2->p[0], l, &seg->p[1]);
      else
        opposite_point (&seg2->p[3], &seg2->p[0], l, &seg->p[1]);
    }

  if (seg1->op == GSK_PATH_LINE)
    {
      l = graphene_point_distance (&seg->p[0], &C2, NULL, NULL);
      if (three_point_angle (&seg1->p[0], &seg1->p[3], &B) > 0)
        scale_point (&seg1->p[0], &seg1->p[3], l, &seg->p[2]);
      else
        opposite_point (&seg1->p[0], &seg1->p[3], l, &seg->p[2]);
    }

  /* Maintain smoothness and symmetry */
  if (seg->type != CUSP)
    {
      if (seg->type == SYMMETRIC)
        l = graphene_point_distance (&seg->p[0], &seg->p[1], NULL, NULL);
      else
        l = graphene_point_distance (&seg->p[0], &seg2->p[2], NULL, NULL);
      opposite_point (&seg->p[0], &seg->p[1], l, &seg2->p[2]);
    }

  if (seg1->type != CUSP)
    {
      if (seg1->type == SYMMETRIC)
        l = graphene_point_distance (&seg->p[3], &seg->p[2], NULL, NULL);
      else
        l = graphene_point_distance (&seg->p[3], &seg1->p[1], NULL, NULL);
      opposite_point (&seg->p[3], &seg->p[2], l, &seg1->p[1]);
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
      drag_point (self, x, y);
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

  get_segment (self, self->context)->type = point_type_from_string (g_variant_get_string (value, NULL));

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
  Segment *seg = get_segment (self, self->context);

  seg->op = op_from_string (g_variant_get_string (value, NULL));

  if (seg->op == GSK_PATH_CONIC && seg->weight == 0)
    seg->weight = 1;

  maintain_conic (self, self->context);

  maintain_smoothness (self, self->context);
  maintain_smoothness (self, self->context + 1);
  maintain_symmetry (self, self->context);
  maintain_symmetry (self, self->context + 1);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
insert_new_point (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  insert_point (self, self->context, self->context_pos);

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

static void
toggle_edit_point (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->edited_point == self->context)
    self->edited_point = -1;
  else
    {
      self->edited_point = self->context;
      self->edited_segment = -1;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
toggle_edit_segment (GSimpleAction *action,
                     GVariant      *value,
                     gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->edited_segment == self->context)
    self->edited_segment = -1;
  else
    {
      self->edited_segment = self->context;
      self->edited_point = -1;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
reset_weight (GSimpleAction *action,
              GVariant      *value,
              gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  Segment *seg = get_segment (self, self->context);

  seg->weight = 1;
  maintain_conic (self, self->context);
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
  float t;

  if (!self->edit)
    return;

  if (button == GDK_BUTTON_SECONDARY)
    {
      for (i = 0; i < self->segments->len; i++)
        {
          Segment *seg = get_segment (self, i);
          const graphene_point_t *p = get_line_point (self, i);

          if (graphene_point_distance (p, &m, NULL, NULL) < CLICK_RADIUS)
            {
              GAction *action;

              self->context = i;

              action = g_action_map_lookup_action (self->actions, "set-segment-type");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "add-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "remove-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

              action = g_action_map_lookup_action (self->actions, "reset-weight");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "set-point-type");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (point_type_to_string (seg->type)));

              action = g_action_map_lookup_action (self->actions, "edit-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (self->edited_point == i));

              action = g_action_map_lookup_action (self->actions, "edit-segment");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                                           &(const GdkRectangle){ x, y, 1, 1 });
              gtk_popover_popup (GTK_POPOVER (self->menu));
              return;
            }
        }

      if (find_closest_segment (self, &m, CLICK_RADIUS, NULL, &i, &t))
        {
          GAction *action;

          self->context = i;
          self->context_pos = t;

          action = g_action_map_lookup_action (self->actions, "set-point-type");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

          action = g_action_map_lookup_action (self->actions, "edit-point");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

          action = g_action_map_lookup_action (self->actions, "remove-point");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

          action = g_action_map_lookup_action (self->actions, "add-point");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

          action = g_action_map_lookup_action (self->actions, "edit-segment");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
          g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (self->edited_segment == i));

          action = g_action_map_lookup_action (self->actions, "reset-weight");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                       get_segment (self, i)->op == GSK_PATH_CONIC);

          action = g_action_map_lookup_action (self->actions, "set-segment-type");
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
          g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (op_to_string (get_segment (self, i)->op)));

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

  for (i = 0; i < self->segments->len; i++)
    {
      const graphene_point_t *p = get_line_point (self, i);

      if (graphene_point_distance (p, &m, NULL, NULL) < CLICK_RADIUS)
        {
          if (button == GDK_BUTTON_PRIMARY)
            {
              if (self->edited_point == i)
                self->edited_point = -1;
              else
                {
                  self->edited_point = i;
                  self->edited_segment = -1;
                }
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
      for (i = 0; i < self->segments->len; i++)
        {
          Segment *seg = get_segment (self, i);
          int hovered = -1;

          for (j = 0; j < 3; j++)
            {
              const graphene_point_t *q = &seg->p[j];

              if (!point_is_visible (self, i, j))
                continue;

              if (graphene_point_distance (q, &m, NULL, NULL) < CLICK_RADIUS)
                {
                  hovered = j;
                  break;
                }
            }
          if (seg->hovered != hovered)
            {
              seg->hovered = hovered;
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

  for (i = 0; i < self->segments->len; i++)
    {
      Segment *seg = get_segment (self, i);
      if (seg->hovered != -1)
        {
          seg->hovered = -1;
          changed = TRUE;
        }
    }

  if (changed)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

/* }}} */
/* {{{ Snapshot */

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

  if (self->segments->len == 0)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  /* Add the curve itself */

  builder = gsk_path_builder_new ();

  curve_editor_add_path (self, builder);

  path = gsk_path_builder_free_to_path (builder);

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

      if (self->edited_point != -1)
        {
          /* Add the skeleton */
          Segment *seg = get_segment (self, self->edited_point);
          Segment *seg1 = get_segment (self, self->edited_point - 1);
          const graphene_point_t *p = get_line_point (self, self->edited_point);

          if (seg1->op == GSK_PATH_CURVE)
            {
              graphene_point_t *c = &seg1->p[2];
              gsk_path_builder_move_to (builder, c->x, c->y);
              gsk_path_builder_line_to (builder, p->x, p->y);
            }
          else if (seg1->op == GSK_PATH_CONIC)
            {
              graphene_point_t *c = &seg1->p[1];
              gsk_path_builder_move_to (builder, c->x, c->y);
              gsk_path_builder_line_to (builder, p->x, p->y);
            }

          if (seg->op == GSK_PATH_CURVE)
            {
              graphene_point_t *c = &seg->p[1];
              gsk_path_builder_move_to (builder, c->x, c->y);
              gsk_path_builder_line_to (builder, p->x, p->y);
            }
          else if (seg->op == GSK_PATH_CONIC)
            {
              graphene_point_t *c = &seg->p[1];
              gsk_path_builder_move_to (builder, p->x, p->y);
              gsk_path_builder_line_to (builder, c->x, c->y);
            }
        }

      if (self->edited_segment != -1)
        {
          Segment *seg = get_segment (self, self->edited_segment);

          if (seg->op == GSK_PATH_CURVE)
            {
              gsk_path_builder_move_to (builder, seg->p[0].x, seg->p[0].y);
              gsk_path_builder_line_to (builder, seg->p[1].x, seg->p[1].y);
              gsk_path_builder_line_to (builder, seg->p[2].x, seg->p[2].y);
              gsk_path_builder_line_to (builder, seg->p[3].x, seg->p[3].y);
            }
          else if (seg->op == GSK_PATH_CONIC)
            {
              gsk_path_builder_move_to (builder, seg->p[0].x, seg->p[0].y);
              gsk_path_builder_line_to (builder, seg->p[1].x, seg->p[1].y);
              gsk_path_builder_line_to (builder, seg->p[3].x, seg->p[3].y);
            }
        }

      path = gsk_path_builder_free_to_path (builder);

      if (self->edited_point != -1 || self->edited_segment != -1)
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

          for (i = 0; i < self->segments->len; i++)
            {
              Segment *seg = get_segment (self, i);

              for (j = 0; j < 3; j++)
                {
                  graphene_point_t *p = &seg->p[j];

                  if (!point_is_visible (self, i, j))
                    continue;

                  if ((k == 0 && j != seg->hovered) ||
                      (k == 1 && j == seg->hovered))
                    continue;

                  if (j != 0)
                    {
                      gsk_path_builder_add_circle (builder, p, DRAW_RADIUS);
                    }
                  else
                    {
                      switch (seg->type)
                        {
                        case CUSP:
                          add_diamond (builder, p, DRAW_RADIUS);
                          break;

                        case SMOOTH:
                          add_square (builder, p, DRAW_RADIUS);
                          break;
                        case SYMMETRIC:
                        case AUTO:
                          gsk_path_builder_add_circle (builder, p, DRAW_RADIUS);
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

  g_clear_pointer (&self->segments, g_array_unref);
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

  self->segments = g_array_new (FALSE, FALSE, sizeof (Segment));
  self->dragged = -1;
  self->molded = -1;
  self->edited_point = -1;
  self->edited_segment = -1;
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

  action = g_simple_action_new_stateful ("set-point-type", G_VARIANT_TYPE_STRING, g_variant_new_string ("smooth"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_point_type), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  action = g_simple_action_new_stateful ("set-segment-type", G_VARIANT_TYPE_STRING, g_variant_new_string ("curve"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_operation), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new_stateful ("edit-point", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (toggle_edit_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new_stateful ("edit-segment", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (toggle_edit_segment), self);
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

  item = g_menu_item_new ("Cusp", "path.set-point-type::cusp");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Smooth", "path.set-point-type::smooth");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Symmetric", "path.set-point-type::symmetric");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Automatic", "path.set-point-type::auto");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();

  item = g_menu_item_new ("Line", "path.set-segment-type::line");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Curve", "path.set-segment-type::curve");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Conic", "path.set-segment-type::conic");
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

  item = g_menu_item_new ("Edit", "path.edit-segment");
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
  self->edited_point = -1;
  self->edited_segment = -1;

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
  Segment seg;

  seg.op = op;
  seg.hovered = -1;
  seg.dragged = -1;

  switch (op)
    {
    case GSK_PATH_MOVE:
      break;

    case GSK_PATH_CLOSE:
      seg.p[0] = pts[0];
      seg.p[3] = pts[1];
      g_array_append_val (self->segments, seg);
      break;

    case GSK_PATH_LINE:
      seg.p[0] = pts[0];
      seg.p[3] = pts[1];
      g_array_append_val (self->segments, seg);
      break;

    case GSK_PATH_CURVE:
      seg.p[0] = pts[0];
      seg.p[1] = pts[1];
      seg.p[2] = pts[2];
      seg.p[3] = pts[3];
      g_array_append_val (self->segments, seg);
      break;

    case GSK_PATH_CONIC:
      {
        seg.p[0] = pts[0];
        seg.p[1] = pts[1];
        seg.p[3] = pts[2];
        seg.weight = weight;

        get_conic_shoulder_point (pts, weight, &seg.p[2]);

        g_array_append_val (self->segments, seg);
      }
      break;

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
  Segment *first, *last;

  g_array_set_size (self->segments, 0);

  gsk_path_foreach (path, GSK_PATH_FOREACH_ALLOW_CURVE | GSK_PATH_FOREACH_ALLOW_CONIC, copy_segments, self);

  first = get_segment (self, 0);
  last = get_segment (self, self->segments->len - 1);
  if (last->op == GSK_PATH_CLOSE)
    {
      if (graphene_point_near (&last->p[0], &last->p[3], 0.001))
        g_array_remove_index (self->segments, self->segments->len - 1);
      else
        last->op = GSK_PATH_LINE;
    }
  else
    {
      Segment seg;

      seg.op = GSK_PATH_MOVE;
      seg.p[0] = last->p[3];
      seg.p[3] = first->p[0];
      g_array_append_val (self->segments, seg);
    }

  for (i = 0; i < self->segments->len; i++)
    check_smoothness (self, i);

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
