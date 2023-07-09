#include "cepath.h"

#include <gtk/gtk.h>

/* {{{ Misc. geometry */

static inline float
point_distance (const graphene_point_t *a,
                const graphene_point_t *b)
{
  return graphene_point_distance (a, b, NULL, NULL);
}

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
 * return new Bezier control points for two curves in left
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

  dist = point_distance (S, E);
  angle = atan2 (E->y - S->y, E->x - S->x) - atan2 (B->y - S->y, B->x - S->x);
  bc = (angle < 0 || angle > M_PI ? -1 : 1) * dist / 3;
  de1 = t * bc;
  de2 = (1 - t) * bc;

  circle_through_points (S, B, E, &c);

  t0.x = B->x - (B->y - c.y);
  t0.y = B->y + (B->x - c.x);
  t1.x = B->x + (B->y - c.y);
  t1.y = B->y - (B->x - c.x);

  tlength = point_distance (&t0, &t1);
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
 * Bezier curve given bY S, C1, C2, E.
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

  d1 = point_distance (S, B);
  d2 = point_distance (E, B);
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
/* {{{ GObject boilerplate */

/* A curve describes a Bézier curve and the
 * on-curve point where it starts.
 */
struct _CEPathCurve
{
  GskPathOperation op;
  graphene_point_t p[4];
  float weight;
  CEPathConstraint constraint;
  CEPathCurve *prev;
  CEPathCurve *next;
  int mark;
};

struct _CEPath
{
  GObject parent_instance;

  GPtrArray *curves;
};

struct _CEPathClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (CEPath, ce_path, G_TYPE_OBJECT)

static void
ce_path_init (CEPath *self)
{
  self->curves = g_ptr_array_new_with_free_func (g_free);
}

static void
ce_path_finalize (GObject *object)
{
  CEPath *self = CE_PATH (object);

  g_ptr_array_unref (self->curves);

  G_OBJECT_CLASS (ce_path_parent_class)->finalize (object);
}

static void
ce_path_class_init (CEPathClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = ce_path_finalize;
}

/* }}} */
/* {{{ Constraints */

static void
maintain_smoothness (CEPath        *self,
                     CEPathCurve *seg1,
                     CEPathCurve *seg)
{
  const graphene_point_t *p, *p2;
  graphene_point_t *c, *c2;
  float d;

  g_return_if_fail (seg1 != NULL);
  g_return_if_fail (seg != NULL);

  if (seg->constraint == CE_PATH_CUSP)
    return;

  if (seg->op == GSK_PATH_MOVE || seg1->op == GSK_PATH_MOVE)
    return;

  if (seg->op == GSK_PATH_LINE && seg1->op == GSK_PATH_LINE)
    return;

  p = &seg->p[0];
  c = &seg1->p[2];
  c2 = &seg->p[1];

  if (seg->op == GSK_PATH_CUBIC && seg1->op == GSK_PATH_CUBIC)
    {
      d = point_distance (c, p);
      opposite_point (p, c2, d, c);
    }
  else if (seg->op == GSK_PATH_CUBIC)
    {
      if (seg1->op == GSK_PATH_LINE)
        p2 = &seg1->p[0];
      else if (seg1->op == GSK_PATH_CONIC || seg1->op == GSK_PATH_QUAD)
        p2 = &seg1->p[1];
      else
        g_assert_not_reached ();
      d = point_distance (c2, p);
      opposite_point (p, p2, d, c2);
    }
  else if (seg1->op == GSK_PATH_CUBIC)
    {
      if (seg->op == GSK_PATH_LINE)
        p2 = &seg->p[3];
      else if (seg->op == GSK_PATH_CONIC || seg->op == GSK_PATH_QUAD)
        p2 = &seg->p[1];
      else
        g_assert_not_reached ();
      d = point_distance (c, p);
      opposite_point (p, p2, d, c);
    }
  else if ((seg->op == GSK_PATH_CONIC && seg1->op == GSK_PATH_CONIC) ||
           (seg->op == GSK_PATH_QUAD && seg1->op == GSK_PATH_QUAD))
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
maintain_symmetry (CEPath      *self,
                   CEPathCurve *seg1,
                   CEPathCurve *seg)
{
  const graphene_point_t *p;
  graphene_point_t *c, *c2;
  double l1, l2, l;

  g_return_if_fail (seg1 != NULL);
  g_return_if_fail (seg != NULL);

  if (seg->op == GSK_PATH_MOVE || seg1->op == GSK_PATH_MOVE)
    return;

  if (seg->constraint != CE_PATH_SYMMETRIC)
    return;

  if (seg->op != GSK_PATH_CUBIC || seg1->op != GSK_PATH_CUBIC)
    return;

  p = &seg->p[0];
  c = &seg1->p[2];
  c2 = &seg->p[1];

  l1 = point_distance (p, c);
  l2 = point_distance (p, c2);

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
update_automatic (CEPath      *self,
                  CEPathCurve *seg1,
                  CEPathCurve *seg)
{
  const graphene_point_t *p, *p1, *p2;
  double l1, l2;
  graphene_point_t a;
  graphene_point_t *c1, *c2;

  if (!seg1 || !seg)
    return;

  if (seg1->op != GSK_PATH_CUBIC || seg->op != GSK_PATH_CUBIC)
    return;

  if (seg->constraint != CE_PATH_AUTOMATIC)
    return;

  p = &seg->p[0];
  c1 = &seg1->p[2];
  c2 = &seg->p[1];

  p1 = &seg1->p[0];
  p2 = &seg->p[3];

  l1 = point_distance (p, p1);
  l2 = point_distance (p, p2);

  a.x = p2->x + (p->x - p1->x);
  a.y = p2->y + (p->y - p1->y);

  scale_point (p, &a, l2/3, c2);
  opposite_point (p, &a, l1/3, c1);
}

static void
maintain_automatic (CEPath      *self,
                    CEPathCurve *seg11,
                    CEPathCurve *seg1,
                    CEPathCurve *seg,
                    CEPathCurve *seg2)
{
  g_return_if_fail (seg1 != NULL);
  g_return_if_fail (seg != NULL);

  update_automatic (self, seg1, seg);
  update_automatic (self, seg11, seg1);
  update_automatic (self, seg, seg2);
}

static void
update_conic (CEPathCurve *seg)
{
  graphene_point_t p[3];

  g_return_if_fail (seg != NULL);

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
check_smoothness (CEPath      *self,
                  CEPathCurve *seg)
{
  GskPathOperation op, op1;
  const graphene_point_t *p, *p1, *p2;
  CEPathCurve *seg1;

  seg1 = seg->prev;
  if (seg1 == NULL)
    return;

  p = &seg->p[0];

  op = seg->op;
  op1 = seg1->op;

  if (op == GSK_PATH_CUBIC)
    p2 = &seg->p[1];
  else if (op == GSK_PATH_LINE)
    p2 = &seg->p[3];
  else
    p2 = NULL;

  if (op1 == GSK_PATH_CUBIC)
    p1 = &seg1->p[2];
  else if (op1 == GSK_PATH_LINE)
    p1 = &seg1->p[0];
  else
    p1 = NULL;

  if (!p1 || !p2 || !collinear (p, p1, p2))
    seg->constraint = CE_PATH_CUSP;
  else
    seg->constraint = CE_PATH_SMOOTH;
}

/* }}} */
/* {{{ Basics */

CEPath *
ce_path_new (void)
{
  return g_object_new (CE_TYPE_PATH, NULL);
}

int
ce_path_get_n_curves (CEPath *self)
{
  g_return_val_if_fail (CE_IS_PATH (self), 0);

  return self->curves->len;
}

CEPathCurve *
ce_path_get_curve (CEPath *self,
                   int     idx)
{
  g_return_val_if_fail (CE_IS_PATH (self), NULL);
  g_return_val_if_fail (0 <= idx && idx < self->curves->len, NULL);

  return g_ptr_array_index (self->curves, idx);
}

CEPathCurve *
ce_path_previous_curve (CEPath      *self,
                        CEPathCurve *seg)
{
  g_return_val_if_fail (CE_IS_PATH (self), NULL);
  g_return_val_if_fail (seg != NULL, NULL);

  return seg->prev;
}

CEPathCurve *
ce_path_next_curve (CEPath      *self,
                    CEPathCurve *seg)
{
  g_return_val_if_fail (CE_IS_PATH (self), NULL);
  g_return_val_if_fail (seg != NULL, NULL);

  return seg->next;
}

/* }}} */
/* {{{ GskPath interaction */

static void
path_builder_add_curve (CEPath         *self,
                        CEPathCurve    *seg,
                        GskPathBuilder *builder,
                        gboolean        need_move_to)
{
  if (need_move_to)
    gsk_path_builder_move_to (builder, seg->p[0].x, seg->p[0].y);

  switch (seg->op)
    {
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, seg->p[3].x, seg->p[3].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder,
                                seg->p[1].x, seg->p[1].y,
                                seg->p[3].x, seg->p[3].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder,
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
path_builder_add_path (CEPath         *self,
                       GskPathBuilder *builder)
{
  CEPathCurve *next, *seg, *seg2;

  for (int i = 0; i < self->curves->len; i++)
    {
      seg = g_ptr_array_index (self->curves, i);
      seg->mark = 1;
    }

  do
    {
      next = NULL;
      for (int i = 0; i < self->curves->len; i++)
        {
          seg = g_ptr_array_index (self->curves, i);
          if (seg->mark && seg->prev == NULL)
            {
              seg2 = seg;
              while (seg2->prev != NULL && seg2 != seg)
                seg2 = seg2->prev;
              next = seg2;
              break;
            }
        }

      if (next)
        {
          seg = next;
          gsk_path_builder_move_to (builder, seg->p[0].x, seg->p[0].y);
          do
            {
              if (seg->mark == 0)
                {
                  gsk_path_builder_close (builder);
                  break;
                }

              path_builder_add_curve (self, seg, builder, FALSE);

              seg->mark = 0;
              seg = seg->next;
            }
          while (seg);
        }
    }
  while (next);
}

typedef struct
{
  CEPath *self;
  CEPathCurve *first;
  CEPathCurve *last;
} CollectData;

static gboolean
copy_curves (GskPathOperation        op,
             const graphene_point_t *pts,
             gsize                   n_pts,
             float                   weight,
             gpointer                data)
{
  CollectData *cd = data;
  CEPath *self = cd->self;
  CEPathCurve *seg;

  if (op == GSK_PATH_MOVE)
    {
      cd->first = NULL;
      cd->last = NULL;
      return TRUE;
    }

  seg = g_new0 (CEPathCurve, 1);
  seg->op = op;

  seg->prev = cd->last;
  if (cd->last)
    cd->last->next = seg;
  seg->next = NULL;
  cd->last = seg;

  switch (op)
    {
    case GSK_PATH_CLOSE:
      seg->p[0] = pts[0];
      seg->p[3] = pts[1];
      seg->next = cd->first;
      if (cd->first)
        cd->first->prev = seg;
      cd->first = NULL;
      cd->last = NULL;
      return TRUE;

    case GSK_PATH_LINE:
      seg->p[0] = pts[0];
      seg->p[3] = pts[1];
      break;

    case GSK_PATH_QUAD:
      seg->p[0] = pts[0];
      seg->p[1] = pts[1];
      seg->p[3] = pts[2];
      break;

    case GSK_PATH_CUBIC:
      seg->p[0] = pts[0];
      seg->p[1] = pts[1];
      seg->p[2] = pts[2];
      seg->p[3] = pts[3];
      break;

    case GSK_PATH_CONIC:
      {
        seg->p[0] = pts[0];
        seg->p[1] = pts[1];
        seg->p[3] = pts[2];
        seg->weight = weight;

        get_conic_shoulder_point (pts, weight, &seg->p[2]);
      }
      break;

    case GSK_PATH_MOVE:
    default:
      g_assert_not_reached ();
    }

  g_ptr_array_add (self->curves, seg);

  return TRUE;
}

void
ce_path_set_gsk_path (CEPath  *self,
                      GskPath *path)
{
  CollectData cd;

  g_ptr_array_set_size (self->curves, 0);

  cd.self = self;
  cd.first = NULL;
  cd.last = NULL;

  gsk_path_foreach (path, -1, copy_curves, &cd);

  if (self->curves->len == 0)
    return;

#if 0
  /* Currently, the code assumes there is a 1-1 relationship
   * between curves and line points. So add an artificial
   * move curve at the end, as holder for the endpoint
   */
  first = get_curve (self, 0);
  last = get_curve (self, self->curves->len - 1);
  if (last->op == GSK_PATH_CLOSE)
    {
      if (graphene_point_near (&last->p[0], &last->p[3], 0.001))
        g_array_remove_index (self->curves, self->curves->len - 1);
      else
        last->op = GSK_PATH_LINE;
    }
  else
    {
      Segment seg;

      memset (&seg, 0, sizeof (Segment));

      seg.op = GSK_PATH_MOVE;
      seg.p[0] = last->p[3];
      seg.p[3] = first->p[0];
      g_array_append_val (self->curves, seg);
    }
#endif

  for (int i = 0; i < self->curves->len; i++)
    check_smoothness (self, g_ptr_array_index (self->curves, i));
}

GskPath *
ce_path_get_gsk_path (CEPath *self)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  path_builder_add_path (self, builder);

  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Segment juggling */

void
ce_path_split_curve (CEPath      *self,
                     CEPathCurve *seg,
                     float        pos)
{
  CEPathCurve *seg1, *seg11, *seg2;
  CEPathCurve *ns;

  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);
  g_return_if_fail (0 < pos && pos < 1);

  if (seg->op == GSK_PATH_MOVE)
    return;

  ns = g_new0 (CEPathCurve, 1);
  ns->prev = seg;
  ns->next = seg->next;
  seg->next->prev = ns;
  seg->next = ns;
  g_ptr_array_add (self->curves, ns);

  seg1 = seg->next;
  seg2 = seg1->next;
  seg11 = seg->prev;

  seg1->constraint = CE_PATH_SMOOTH;

  switch (seg->op)
    {
    case GSK_PATH_LINE:
      seg1->op = GSK_PATH_LINE;

      graphene_point_interpolate (&seg->p[0], &seg->p[3], pos, &seg1->p[0]);
      seg->p[3] = seg->p[0];
      seg1->p[3] = seg2->p[0];
      break;

    case GSK_PATH_QUAD:
      {
        graphene_point_t points[3];
        graphene_point_t left[3];
        graphene_point_t right[3];
        int left_pos = 0;
        int right_pos = 0;

        seg1->op = GSK_PATH_QUAD;

        points[0] = seg->p[0];
        points[1] = seg->p[1];
        points[2] = seg->p[3];
        split_bezier (points, 3, pos, left, &left_pos, right, &right_pos);

        seg->p[0] = left[0];
        seg->p[1] = left[1];
        seg->p[3] = left[2];
        seg1->p[0] = right[2];
        seg1->p[1] = right[1];
        seg1->p[3] = right[0];
      }
      break;

    case GSK_PATH_CUBIC:
      {
        graphene_point_t left[4];
        graphene_point_t right[4];
        int left_pos = 0;
        int right_pos = 0;

        seg1->op = GSK_PATH_CUBIC;

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

  if (seg1 && seg)
    {
      maintain_smoothness (self, seg1, seg);
      if (seg2)
        maintain_smoothness (self, seg1, seg2);
      maintain_automatic (self, seg11, seg, seg1, seg2);
    }
}

void
ce_path_remove_curve (CEPath      *self,
                      CEPathCurve *seg)
{
  CEPathCurve *seg1 = NULL, *prev;
  graphene_point_t c, p;

  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);

  if (seg->op == GSK_PATH_MOVE)
    {
      if (seg->prev)
        seg1 = seg->prev->prev;
      if (seg1)
        {
          c = seg1->p[2];
          p = seg1->p[3];
          seg->p[0] = p;
        }
      seg = seg->prev;
      if (seg == NULL)
        return;
    }
  else
    {
      c = seg->p[2];
      p = seg->p[3];
    }

  prev = seg->prev;
  g_ptr_array_remove_fast (self->curves, seg);
  seg = prev;
  if (seg)
    {
      seg->p[2] = c;
      seg->p[3] = p;
      seg1 = seg->prev;
    }

  if (seg && seg1)
    {
      maintain_smoothness (self, seg1, seg);
      maintain_automatic (self, seg1->prev, seg1, seg, seg->next);
    }
}

/* }}} */
/* {{{ Drag implementation */

void
ce_path_drag_curve (CEPath                 *self,
                    CEPathCurve            *seg,
                    const graphene_point_t *pos)
{
  graphene_point_t *S, *E;
  graphene_point_t B, C1, C2;
  double l;
  CEPathCurve *seg1, *seg2;

  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);
  g_return_if_fail (pos != NULL);

  seg1 = seg->next;
  seg2 = seg->prev;

  if (seg->op == GSK_PATH_CONIC)
    {
      /* FIXME */
      return;
    }

  S = &seg->p[0];
  B = *pos;
  E = &seg->p[3];

  bezier_through (S, &B, E, &C1, &C2);

  seg->p[1] = C1;
  seg->p[2] = C2;

  /* When the neighboring curves are lines, we can't actually
   * use C1 and C2 as-is, since we need control points to lie
   * on the line. So we just use their distance. This makes our
   * point B not quite match anymore, but we're overconstrained.
   */
  if (seg2 && seg2->op == GSK_PATH_LINE)
    {
      l = point_distance (&seg->p[3], &C1);
      if (three_point_angle (&seg2->p[3], &seg2->p[0], &B) > 0)
        scale_point (&seg2->p[3], &seg2->p[0], l, &seg->p[1]);
      else
        opposite_point (&seg2->p[3], &seg2->p[0], l, &seg->p[1]);
    }

  if (seg1 && seg1->op == GSK_PATH_LINE)
    {
      l = point_distance (&seg->p[0], &C2);
      if (three_point_angle (&seg1->p[0], &seg1->p[3], &B) > 0)
        scale_point (&seg1->p[0], &seg1->p[3], l, &seg->p[2]);
      else
        opposite_point (&seg1->p[0], &seg1->p[3], l, &seg->p[2]);
    }

  /* Maintain smoothness and symmetry */
  if (seg->constraint != CE_PATH_CUSP)
    {
      if (seg->constraint == CE_PATH_SYMMETRIC)
        l = point_distance (&seg->p[0], &seg->p[1]);
      else
        l = point_distance (&seg->p[0], &seg2->p[2]);
      opposite_point (&seg->p[0], &seg->p[1], l, &seg2->p[2]);
    }

  if (seg1 && seg1->constraint != CE_PATH_CUSP)
    {
      if (seg1->constraint == CE_PATH_SYMMETRIC)
        l = point_distance (&seg->p[3], &seg->p[2]);
      else
        l = point_distance (&seg->p[3], &seg1->p[1]);
      opposite_point (&seg->p[3], &seg->p[2], l, &seg1->p[1]);
    }
}

/* }}} */
/* {{{ Hit testing */

CEPathCurve *
ce_path_find_closest_curve (CEPath           *self,
                            graphene_point_t *point,
                            float             threshold,
                            graphene_point_t *p,
                            float            *pos)
{
  graphene_point_t pp;
  float t;
  CEPathCurve *best;
  int i;

  best = NULL;
  for (i = 0; i < self->curves->len; i++)
    {
      CEPathCurve *seg = g_ptr_array_index (self->curves, i);
      GskPathBuilder *builder;
      GskPath *path;
      GskPathMeasure *measure;
      GskPathPoint *cp;
      float t1;
      graphene_point_t pp1;

      builder = gsk_path_builder_new ();
      path_builder_add_curve (self, seg, builder, TRUE);
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new (path);

      cp = gsk_path_measure_get_closest_point (measure, point, threshold);
      if (cp)
        {
          gsk_path_point_get_position (cp, &pp1);
          t1 = gsk_path_point_get_distance (cp);
          threshold = point_distance (&pp1, point);
          gsk_path_point_unref (cp);

          best = seg;
          t = t1 / gsk_path_measure_get_length (measure);
          pp = pp1;
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }

  if (best)
    {
      if (pos)
        *pos = t;
      if (p)
        *p = pp;
    }

  return best;
}

/* }}} */
/* {{{ Moving points */

static void
set_line_point (CEPath                 *self,
                CEPathCurve            *seg,
                const graphene_point_t *pos)
{
  /* dragged point is on curve */
  CEPathCurve *seg1, *seg2, *seg11;
  const graphene_point_t *d, *p;
  graphene_point_t *c;
  float l1, l2, dx, dy;

  seg1 = seg->prev;
  seg2 = seg->next;
  seg11 = seg1 ? seg1->prev : NULL;

  d = &seg->p[0];

  /* before moving the point, record the distances to its neighbors, since
   * we may want to preserve those
   */

  if (seg1)
    l1 = point_distance (d, &seg1->p[2]);
  else
    l1 = 0;
  l2 = point_distance (d, &seg->p[1]);

  dx = pos->x - d->x;
  dy = pos->y - d->y;

  /* first move the point itself */
  seg->p[0] = *pos;
  if (seg1)
    seg1->p[3] = *pos;

  /* adjust control points as needed */

  if (seg1 && seg1->op == GSK_PATH_LINE)
    {
      /* the other endpoint of the line */
      p = &seg1->p[0];
      c = &seg->p[1];

      if (seg->op == GSK_PATH_CUBIC && seg->constraint != CE_PATH_CUSP)
        {
          opposite_point (d, p, l2, c);
        }
      else if (seg->op == GSK_PATH_CONIC && seg->constraint != CE_PATH_CUSP)
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

          update_conic (seg);
        }
      else
        {
          c->x += dx;
          c->y += dy;
        }

      /* always move the other control point along */
      c = &seg1->p[2];
      c->x += dx;
      c->y += dy;

      /* handle the far end of the line */

      if (seg11 && seg11->op == GSK_PATH_CUBIC && seg1->constraint != CE_PATH_CUSP)
        {
          double l;
          const graphene_point_t *p2;
          graphene_point_t *c2;

          p2 = &seg1->p[0];
          c2 = &seg11->p[2];
          /* adjust the control point before the line curve */
          l = point_distance (c2, p2);
          opposite_point (p2, d, l, c2);
        }
      else if (seg11 && seg11->op == GSK_PATH_CONIC && seg1->constraint != CE_PATH_CUSP)
        {
          graphene_point_t u;
          line_intersection (&seg11->p[0], &seg11->p[1], &seg1->p[3], &seg1->p[0], &u);
          if (u.x != NAN)
            seg11->p[1] = u;

          update_conic (seg11);
        }
    }

  if (seg->op == GSK_PATH_LINE)
    {
      /* the other endpoint of the line */
      p = &seg->p[3];
      if (seg1)
        c = &seg1->p[2];

      if (seg1 && seg1->op == GSK_PATH_CUBIC && seg->constraint != CE_PATH_CUSP)
        {
          /* adjust the control point before the line curve */
          opposite_point (d, p, l1, c);
        }
      else if (seg1 && seg1->op == GSK_PATH_CONIC && seg->constraint != CE_PATH_CUSP)
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

          update_conic (seg11);
        }
      else if (seg1 && seg1->op == GSK_PATH_CUBIC)
        {
          c->x += dx;
          c->y += dy;
        }

      /* always move the other control point along */
      seg->p[1].x += dx;
      seg->p[1].y += dy;

      /* handle the other end of the line */
      if (seg2 && seg2->op == GSK_PATH_CUBIC && seg2->constraint != CE_PATH_CUSP)
        {
          double l;

          /* adjust the control point after the line curve */
          c = &seg2->p[1];
          l = point_distance (c, p);
          opposite_point (p, d, l, c);
        }
      else if (seg2 && seg2->op == GSK_PATH_CONIC && seg2->constraint != CE_PATH_CUSP)
        {
          graphene_point_t u;
          line_intersection (&seg->p[0], &seg->p[3], &seg2->p[1], &seg2->p[3], &u);
          if (u.x != NAN)
            seg2->p[1] = u;

          update_conic (seg2);
        }
    }

  if (seg1 && seg1->op != GSK_PATH_LINE && seg->op != GSK_PATH_LINE)
    {
      if (seg1->op == GSK_PATH_CUBIC)
        {
          c = &seg1->p[2];
          c->x += dx;
          c->y += dy;
        }
      else if (seg1->op == GSK_PATH_CONIC && seg->constraint != CE_PATH_CUSP)
        {
          graphene_point_t a, b;

          a.x = seg1->p[1].x + dx;
          a.y = seg1->p[1].y + dy;
          line_intersection (&seg->p[0], &a, &seg1->p[0], &seg1->p[1], &b);
          seg1->p[1] = b;
        }

      if (seg->op == GSK_PATH_CUBIC)
        {
          c = &seg->p[1];
          c->x += dx;
          c->y += dy;
        }
      else if (seg->op == GSK_PATH_CONIC && seg->constraint != CE_PATH_CUSP)
        {
          graphene_point_t a, b;

          a.x = seg->p[1].x + dx;
          a.y = seg->p[1].y + dy;
          line_intersection (&seg->p[3], &seg->p[1], &a, &seg->p[0], &b);
          seg->p[1] = b;
        }
    }

  if (seg1 && seg)
    {
      maintain_smoothness (self, seg1, seg);
      maintain_automatic (self, seg11, seg1, seg, seg2);
    }
  update_conic (seg);
  if (seg2)
    update_conic (seg2);
}

static void
set_conic_point (CEPath                 *self,
                 CEPathCurve            *seg,
                 int                     point,
                 const graphene_point_t *pos)
{
  CEPathCurve *seg1, *seg2;
  graphene_point_t *d, *c1;
  float l;

  g_assert (seg->op == GSK_PATH_CONIC || seg->op == GSK_PATH_QUAD);
  d = &seg->p[point];

  seg1 = seg->next;
  seg2 = seg->prev;

  if (point == 1)
    {
      if (seg->constraint != CE_PATH_CUSP && seg2 && seg2->op == GSK_PATH_LINE)
        {
          /* control point must be on the line of seg2 */

          if (seg1 && seg1->constraint != CE_PATH_CUSP && seg1->op == GSK_PATH_LINE)
            {
              graphene_point_t c;

              line_intersection (&seg1->p[0], &seg1->p[3], &seg2->p[3], &seg2->p[0], &c);
              if (c.x != NAN)
                *d = c; /* unmoveable */
              else
                {
                  closest_point (pos, &seg1->p[0], &seg1->p[3], &c);
                  *d = c;
                }
            }
          else
            {
              graphene_point_t c;

              closest_point (pos, &seg2->p[0], &seg2->p[3], &c);
              *d = c;

              if (seg1 && seg1->constraint != CE_PATH_CUSP)
                {
                  l = point_distance (&seg1->p[0], &seg1->p[1]);
                  opposite_point (&seg1->p[0], d, l, &seg1->p[1]);
                }
            }
        }
      else if (seg1 && seg1->constraint != CE_PATH_CUSP && seg1->op == GSK_PATH_LINE)
        {
          graphene_point_t c;

          closest_point (pos, &seg1->p[0], &seg1->p[3], &c);
          *d = c;

          if (seg2 && seg2->constraint != CE_PATH_CUSP)
            {
              if (seg2->op == GSK_PATH_CUBIC)
                c1 = &seg2->p[2];
              else if (seg2->op == GSK_PATH_CONIC)
                c1 = &seg2->p[1];
              else
                g_assert_not_reached ();
              l = point_distance (&seg2->p[3], c1);
              opposite_point (&seg2->p[3], d, l, c1);
            }
        }
      else
        {
          /* unconstrained */
          d->x = pos->x;
          d->y = pos->y;

          if (seg1 && seg1->constraint != CE_PATH_CUSP)
            {
              l = point_distance (&seg1->p[0], &seg1->p[1]);
              opposite_point (&seg1->p[0], d, l, &seg1->p[1]);
            }

          if (seg2 && seg2->constraint != CE_PATH_CUSP)
            {
              if (seg2->op == GSK_PATH_CUBIC)
                c1 = &seg2->p[2];
              else if (seg2->op == GSK_PATH_CONIC)
                c1 = &seg2->p[1];
              else
                g_assert_not_reached ();
              l = point_distance (&seg2->p[3], c1);
              opposite_point (&seg2->p[3], d, l, c1);
            }
        }
    }
  else if (point == 2)
    {
      /* dragging the shoulder point */
      graphene_point_t m;
      float t;

      graphene_point_interpolate (&seg->p[0], &seg->p[3], 0.5, &m);
      find_point_on_line (&m, &seg->p[1], pos, &t);
      t = CLAMP (t, 0, 0.9);
      seg->weight = - t / (t - 1);
    }

  update_conic (seg);
}

static void
set_control_point (CEPath                 *self,
                   CEPathCurve            *seg,
                   int                     point,
                   const graphene_point_t *pos)
{
  /* dragged point is a control point */
  CEPathCurve *seg1;
  const graphene_point_t *p, *p1;
  graphene_point_t *c, *d;
  CEPathConstraint constraint;

  g_assert (seg->op == GSK_PATH_CUBIC);
  d = &seg->p[point];

  if (point == 2)
    {
      seg1 = seg->next;
      if (seg1)
        {
          p = &seg1->p[0];
          c = &seg1->p[1];
          constraint = seg1->constraint;
          p1 = &seg1->p[3];
        }
      else
        constraint = CE_PATH_CUSP;
    }
  else if (point == 1)
    {
      seg1 = seg->prev;
      if (seg1)
        {
          if (seg1->op == GSK_PATH_CONIC)
            c = &seg1->p[1];
          else
            c = &seg1->p[2];
          p = &seg->p[0];
          constraint = seg->constraint;
          p1 = &seg1->p[0];
       }
     else
       constraint = CE_PATH_CUSP;
    }
  else
    g_assert_not_reached ();

  if (constraint != CE_PATH_CUSP)
    {
      if (seg1->op == GSK_PATH_CUBIC)
        {
          double l;

          /* first move the point itself */
          d->x = pos->x;
          d->y = pos->y;

          /* then adjust the other control point */
          if (constraint == CE_PATH_SYMMETRIC)
            l = point_distance (d, p);
          else
            l = point_distance (c, p);

          opposite_point (p, d, l, c);
        }
      else if (seg1->op == GSK_PATH_CONIC)
        {
          graphene_point_t u;

          d->x = pos->x;
          d->y = pos->y;
          line_intersection (p1, c, p, d, &u);
          *c = u;

          update_conic (seg1);
        }
      else if (seg1->op == GSK_PATH_LINE)
        {
          closest_point (pos, p, p1, d);
        }
      else
        {
          d->x = pos->x;
          d->y = pos->y;
        }
    }
  else
    {
      d->x = pos->x;
      d->y = pos->y;
    }
}

void
ce_path_set_point (CEPath                 *self,
                   CEPathCurve            *seg,
                   int                     point,
                   const graphene_point_t *pos)
{
  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);
  g_return_if_fail (0 <= point && point < 4);
  g_return_if_fail (pos != NULL);

  if (point == 0)
    set_line_point (self, seg, pos);
  else if (seg->op == GSK_PATH_CONIC || seg->op == GSK_PATH_QUAD)
    set_conic_point (self, seg, point, pos);
  else
    set_control_point (self, seg, point, pos);
}

/* }}} */
/* {{{ Simple getters and setters */

void
ce_path_get_point (CEPath           *self,
                   CEPathCurve      *seg,
                   int               c,
                   graphene_point_t *point)
{
  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);
  g_return_if_fail (0 <= c && c < 4);

  *point = seg->p[c];
}

GskPathOperation
ce_path_get_operation (CEPath      *self,
                       CEPathCurve *seg)
{
  g_return_val_if_fail (CE_IS_PATH (self), GSK_PATH_LINE);
  g_return_val_if_fail (seg != NULL, GSK_PATH_LINE);

  return seg->op;
}

CEPathConstraint
ce_path_get_constraint (CEPath      *self,
                        CEPathCurve *seg)
{
  g_return_val_if_fail (CE_IS_PATH (self), CE_PATH_CUSP);
  g_return_val_if_fail (seg != NULL, CE_PATH_CUSP);

  return seg->constraint;
}

void
ce_path_set_constraint (CEPath           *self,
                        CEPathCurve      *seg,
                        CEPathConstraint  constraint)
{
  CEPathCurve *seg1;

  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);

  if (seg->constraint == constraint)
    return;

  seg->constraint = constraint;

  seg1 = seg->prev;

  if (seg && seg1)
    {
      maintain_smoothness (self, seg1, seg);
      maintain_symmetry (self, seg1, seg);
      maintain_automatic (self, seg1->prev, seg1, seg, seg->next);
    }
}

void
ce_path_set_operation (CEPath           *self,
                       CEPathCurve      *seg,
                       GskPathOperation  op)
{
  CEPathCurve *seg1, *seg2;

  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);

  if (seg->op == op)
    return;

  seg->op = op;

  if (seg->op == GSK_PATH_CONIC && seg->weight == 0)
    seg->weight = 1;

  update_conic (seg);

  seg1 = seg->prev;
  seg2 = seg->next;

  if (seg1 && seg)
    maintain_smoothness (self, seg1, seg);
  if (seg && seg2)
    maintain_smoothness (self, seg, seg2);
  if (seg1 && seg)
    maintain_symmetry (self, seg1, seg);
  if (seg && seg2)
    maintain_symmetry (self, seg, seg2);
}

void
ce_path_set_weight (CEPath      *self,
                    CEPathCurve *seg,
                    float        weight)
{
  g_return_if_fail (CE_IS_PATH (self));
  g_return_if_fail (seg != NULL);
  g_return_if_fail (0 < weight);

  seg->weight = 1;
  update_conic (seg);
}

float
ce_path_get_weight (CEPath        *self,
                     CEPathCurve *seg)
{
  g_return_val_if_fail (CE_IS_PATH (self), 1.f);
  g_return_val_if_fail (seg != NULL, 1.f);

  return seg->weight;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
