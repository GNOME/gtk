#include "curve-editor.h"
#include "gtkcontour.h"

#include <gtk/gtk.h>

#define DRAW_RADIUS 5
#define CLICK_RADIUS 8

/* {{{ Types and structures */

static const char *
op_to_string (GskPathOperation op)
{
  switch (op)
    {
    case GSK_PATH_LINE:
      return "line";
    case GSK_PATH_QUAD:
      return "quad";
    case GSK_PATH_CUBIC:
      return "cubic";
    case GSK_PATH_CONIC:
      return "conic";
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

static GskPathOperation
op_from_string (const char *s)
{
  if (strcmp (s, "line") == 0)
    return GSK_PATH_LINE;
  else if (strcmp (s, "quad") == 0)
    return GSK_PATH_QUAD;
  else if (strcmp (s, "cubic") == 0)
    return GSK_PATH_CUBIC;
  else if (strcmp (s, "conic") == 0)
    return GSK_PATH_CONIC;
  else
    g_assert_not_reached ();
}

typedef enum
{
  CUSP,
  SMOOTH,
  SYMMETRIC,
  AUTO
} Constraint;

static const char *
constraint_to_string (Constraint constraint)
{
  switch (constraint)
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

static Constraint
constraint_from_string (const char *s)
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

/* }}} */
/* {{{ Misc. geometry */

static inline double
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

  return graphene_point_near (p, &q, 0.01);
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
static gboolean
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
      return FALSE;
    }
  else
    {
      p->x = (b2*c1 - b1*c2) / det;
      p->y = (a1*c2 - a2*c1) / det;
      return TRUE;
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
compute_shoulder_point (graphene_point_t p[4],
                        float            w)
{
  get_conic_shoulder_point ((const graphene_point_t[3]) { p[0], p[1], p[3] }, w, &p[2]);
}

static void
compute_weight (const graphene_point_t  p[4],
                float                  *w)
{
  graphene_point_t m;
  float t;

  graphene_point_interpolate (&p[0], &p[3], 0.5, &m);
  find_point_on_line (&m, &p[1], &p[2], &t);
  t = CLAMP (t, 0, 0.9);
  *w = - t / (t - 1);
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
/* {{{ Widget stuff */

struct _CurveEditor
{
  GtkWidget parent_instance;

  GPtrArray *contours;

  GskStroke *stroke;
  GdkRGBA color;

  int context_contour;
  int context_point;
  int context_curve;
  float context_pos;

  int selected_contour;
  int selected_point;

  int edited_contour;
  int edited_curve;
  int edited_point;

  int hovered_contour;
  int hovered_curve;
  int hovered_point;

  int dragged_contour;
  int dragged_curve;
  int dragged_point;

  gboolean edit;
  gboolean show_outline;

  GtkWidget *menu;
  GActionMap *actions;
};

struct _CurveEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CurveEditor, curve_editor, GTK_TYPE_WIDGET)

/* }}} */
/* {{{ Utilities */

/* get/set_point wrappers that handle the conic shoulder point */
static void
contour_get_curve (GtkContour       *contour,
                   unsigned int      idx,
                   GskPathOperation *op,
                   graphene_point_t  p[4])
{
  GskPathOperation op2;
  float w;

  gtk_contour_get_curve (contour, idx, &op2, p, &w);

  if (op2 == GSK_PATH_CONIC && p != NULL)
    compute_shoulder_point (p, w);

  if (op)
    *op = op2;
}

static void
contour_set_curve_point (GtkContour             *contour,
                         unsigned int            idx,
                         unsigned int            point,
                         const graphene_point_t *m)
{

  switch (point)
    {
    case 0:
      gtk_contour_set_point (contour, idx, m);
      break;
    case 3:
      if (idx + 1 < gtk_contour_get_n_points (contour))
        gtk_contour_set_point (contour, idx + 1, m);
      else if (gtk_contour_is_closed (contour))
        gtk_contour_set_point (contour, 0, m);
      break;
    case 1:
    case 2:
      {
        GskPathOperation op;
        graphene_point_t p[4];
        float w;

        gtk_contour_get_curve (contour, idx, &op, p, &w);

        p[point] = *m;
        if (op == GSK_PATH_CONIC && point == 2)
          compute_weight (p, &w);

        gtk_contour_set_curve (contour, idx, op, p, w);
      }
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
contour_set_constraint (GtkContour   *contour,
                        unsigned int  idx,
                        Constraint    constraint)
{
  GArray *constraints;

  constraints = (GArray *) g_object_get_data (G_OBJECT (contour), "constraints");

  g_array_index (constraints, Constraint, idx) = constraint;
}

static Constraint
contour_get_constraint (GtkContour   *contour,
                        unsigned int  idx)
{
  GArray *constraints;

  constraints = (GArray *) g_object_get_data (G_OBJECT (contour), "constraints");

  return g_array_index (constraints, Constraint, idx);
}

static void
contour_insert_point (GtkContour   *contour,
                      unsigned int  idx,
                      float         t,
                      Constraint    constraint)
{
  GArray *constraints;

  gtk_contour_insert_point (contour, idx, t);

  constraints = (GArray *) g_object_get_data (G_OBJECT (contour), "constraints");
  g_array_insert_val (constraints, idx, constraint);
}

static void
contour_remove_point (GtkContour   *contour,
                      unsigned int  idx)
{
  GArray *constraints;

  gtk_contour_remove_point (contour, idx);

  constraints = (GArray *) g_object_get_data (G_OBJECT (contour), "constraints");
  g_array_remove_index (constraints, idx);
}

static void
contour_append (GtkContour *contour1,
                GtkContour *contour2)
{
  GArray *constraints1, *constraints2;

  constraints1 = (GArray *) g_object_get_data (G_OBJECT (contour1), "constraints");
  constraints2 = (GArray *) g_object_get_data (G_OBJECT (contour2), "constraints");
  for (unsigned int i = 0; i < constraints2->len; i++)
    {
      Constraint c = g_array_index (constraints2, Constraint, i);
      g_array_append_val (constraints1, c);
    }

  gtk_contour_append (contour1, contour2);

  g_assert (gtk_contour_get_n_points (contour1) == constraints1->len);
}

static int
contour_curve_start_point (GtkContour   *contour,
                           unsigned int  idx)
{
  return idx;
}

static int
contour_curve_end_point (GtkContour   *contour,
                         unsigned int  idx)
{
  if (idx + 1 < gtk_contour_get_n_points (contour))
    return idx + 1;
  else
    return 0;
}

static int
contour_curve_to_point (GtkContour   *contour,
                        unsigned int  idx)
{
  if (idx > 0)
    return idx - 1;
  else if (gtk_contour_is_closed (contour))
    return gtk_contour_get_n_curves (contour) - 1;
  else
    return -1;
}

static int
contour_curve_from_point (GtkContour   *contour,
                          unsigned int  idx)
{
  if (idx < gtk_contour_get_n_curves (contour))
    return idx;
  else if (gtk_contour_is_closed (contour))
    return 0;
  else
    return -1;
}

static int
contour_curve_to_curve (GtkContour    *contour,
                        unsigned int  idx)
{
  return contour_curve_to_point (contour, contour_curve_start_point (contour, idx));
}

static int
contour_curve_from_curve (GtkContour    *contour,
                          unsigned int  idx)
{
  return contour_curve_from_point (contour, contour_curve_end_point (contour, idx));
}

static gboolean
point_is_visible (CurveEditor *self,
                  int          contour_idx,
                  int          curve_idx,
                  int          point_idx)
{
  GtkContour *contour;
  GskPathOperation op;
  int n_curves, next;

  if (!self->edit)
    return FALSE;

  contour = g_ptr_array_index (self->contours, contour_idx);
  n_curves = gtk_contour_get_n_curves (contour);

  if (gtk_contour_is_closed (contour))
    next = (curve_idx + 1) % n_curves;
  else
    next = curve_idx + 1;

  gtk_contour_get_curve (contour, curve_idx, &op, NULL, NULL);

  switch (point_idx)
    {
    case 0:
      /* point on curve */
      return TRUE;

    case 1:
      if (self->edited_contour == contour_idx &&
          self->edited_curve == curve_idx &&
          op != GSK_PATH_LINE)
        return TRUE;
      if ((op == GSK_PATH_CONIC || op == GSK_PATH_QUAD) &&
          self->edited_contour == contour_idx &&
          (self->edited_point == contour_curve_start_point (contour, curve_idx) ||
           self->edited_point == contour_curve_end_point (contour, curve_idx)))
        return TRUE;
      if (op == GSK_PATH_CUBIC &&
          self->edited_contour == contour_idx &&
          self->edited_point == contour_curve_start_point (contour, curve_idx))
        return TRUE;
      break;

    case 2:
      if (self->edited_contour == contour_idx &&
          self->edited_curve == curve_idx &&
          (op == GSK_PATH_CUBIC || op == GSK_PATH_CONIC))
        return TRUE;
      if (op == GSK_PATH_CUBIC &&
          self->edited_contour == contour_idx &&
          self->edited_point == contour_curve_end_point (contour, curve_idx))
        return TRUE;
      break;

    case 3:
      return next == n_curves; /* only pick this up at the very end */

    default:
      g_assert_not_reached ();
    }

  return FALSE;
}

static gboolean
point_is_hovered (CurveEditor *self,
                  int          contour_idx,
                  int          curve_idx,
                  int          point_idx)
{
  if (contour_idx == -1)
    return FALSE;

  return self->hovered_contour == contour_idx &&
         self->hovered_curve == curve_idx &&
         self->hovered_point == point_idx;
}

static gboolean
point_is_selected (CurveEditor *self,
                   int          contour_idx,
                   int          curve_idx,
                   int          point_idx)
{
  GtkContour *contour;

  if (contour_idx == -1)
    return FALSE;

  if (self->selected_contour != contour_idx)
    return FALSE;

  contour = g_ptr_array_index (self->contours, contour_idx);

  if (point_idx == 0)
    return curve_idx == contour_curve_from_point (contour, self->selected_point);
  else if (point_idx == 3)
    return curve_idx == contour_curve_to_point (contour, self->selected_point);
  else
    return FALSE;
}

/*  }}} */
/* {{{ Constraints */

static gboolean
can_constrain (GtkContour *contour,
               int         point)
{
  int prev, next;
  GskPathOperation op;

  prev = contour_curve_to_point (contour, point);
  if (prev != -1)
    {
      gtk_contour_get_curve (contour, prev, &op, NULL, NULL);
      if (op != GSK_PATH_CUBIC)
        return FALSE;
    }

  next = contour_curve_from_point (contour, point);
  if (next != -1)
    {
      gtk_contour_get_curve (contour, next, &op, NULL, NULL);
      if (op != GSK_PATH_CUBIC)
        return FALSE;
    }

  return TRUE;
}

static void
update_constraint (GtkContour *contour,
                   int         point)
{
  if (!can_constrain (contour, point))
    contour_set_constraint (contour, point, CUSP);
}

typedef enum {
  CONSTRAINT_PRESERVE_EQUAL,
  CONSTRAINT_PRESERVE_IN,
  CONSTRAINT_PRESERVE_OUT,
} ConstraintPreserve;

/* Ensure that the tangents at the given point
 * have the same direction.
 */
static void
maintain_smoothness (GtkContour         *contour,
                     int                 point,
                     ConstraintPreserve  cp)
{
  Constraint constraint;
  GskPathOperation op1, op2;
  graphene_point_t p1[4], p2[4];
  const graphene_point_t *p, *pp;
  graphene_point_t *c, *c2;
  graphene_point_t d;
  int prev, next;
  float w1, w2;

  /* FIXME: this is broken between quadratics and conics */

  constraint = contour_get_constraint (contour, point);
  if (constraint == CUSP)
    return;

  prev = contour_curve_to_point (contour, point);
  next = contour_curve_from_point (contour, point);
  if (prev == -1 || next == -1)
    return;

  gtk_contour_get_curve (contour, prev, &op1, p1, &w1);
  gtk_contour_get_curve (contour, next, &op2, p2, &w2);

  p = &p2[0];

  if (op1 == GSK_PATH_CUBIC && op2 == GSK_PATH_CUBIC)
    {
      c = &p1[2];
      c2 = &p2[1];
      switch (cp)
        {
        case CONSTRAINT_PRESERVE_OUT:
          opposite_point (p, c2, point_distance (c, p), c);
          break;
        case CONSTRAINT_PRESERVE_IN:
          opposite_point (p, c, point_distance (c2, p), c2);
          break;
        case CONSTRAINT_PRESERVE_EQUAL:
          opposite_point (p, c, point_distance (c2, p), &d);
          graphene_point_interpolate (c2, &d, 0.5, &d);
          opposite_point (p, &d, point_distance (c, p), c);
          opposite_point (p, c, point_distance (c2, p), c2);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else if (op2 == GSK_PATH_CUBIC)
    {
      c2 = &p2[1];
      if (op1 == GSK_PATH_LINE)
        pp = &p1[0];
      else if (op1 == GSK_PATH_CONIC || op1 == GSK_PATH_QUAD)
        pp = &p1[1];
      else
        g_assert_not_reached ();
      opposite_point (p, pp, point_distance (c2, p), c2);
    }
  else if (op1 == GSK_PATH_CUBIC)
    {
      c = &p1[2];
      if (op2 == GSK_PATH_LINE)
        pp = &p2[3];
      else if (op2 == GSK_PATH_CONIC || op2 == GSK_PATH_QUAD)
        pp = &p2[1];
      else
        g_assert_not_reached ();
      opposite_point (p, pp, point_distance (c, p), c);
    }
  else if ((op1 == GSK_PATH_CONIC && op2 == GSK_PATH_CONIC) ||
           (op1 == GSK_PATH_QUAD && op2 == GSK_PATH_QUAD))
    {
      graphene_point_t h, a, b;

      h.x = p2[0].x + p2[1].x - p1[1].x;
      h.y = p2[0].y + p2[1].y - p1[1].y;
      line_intersection (&p2[0], &h, &p1[0], &p1[1], &a);
      line_intersection (&p2[0], &h, &p1[1], &p2[3], &b);

      p1[1] = a;
      p2[1] = b;
    }

  gtk_contour_set_curve (contour, prev, op1, p1, w1);
  gtk_contour_set_curve (contour, next, op2, p2, w2);
}

/* Ensure that the tangents at the given point
 * have the same length. This only works for
 * cubics.
 */
static void
maintain_symmetry (GtkContour         *contour,
                   int                 point,
                   ConstraintPreserve  cp)
{
  Constraint constraint;
  int prev, next;
  GskPathOperation op1, op2;
  graphene_point_t p1[4], p2[4];
  graphene_point_t *p, *c, *c2;
  float l1, l2;
  float w1, w2;

  constraint = contour_get_constraint (contour, point);
  if (constraint != SYMMETRIC)
    return;

  prev = contour_curve_to_point (contour, point);
  next = contour_curve_from_point (contour, point);

  if (prev == -1 || next == -1)
    return;

  gtk_contour_get_curve (contour, prev, &op1, p1, &w1);
  gtk_contour_get_curve (contour, next, &op2, p2, &w2);

  if (op1 != GSK_PATH_CUBIC || op2 != GSK_PATH_CUBIC)
    return;

  p = &p2[0];
  c = &p1[2];
  c2 = &p2[1];

  l1 = point_distance (p, c);
  l2 = point_distance (p, c2);

  if (l1 == l2)
    return;

  switch (cp)
    {
    case CONSTRAINT_PRESERVE_IN:
      l2 = l1;
      break;
    case CONSTRAINT_PRESERVE_OUT:
      l1 = l2;
      break;
    case CONSTRAINT_PRESERVE_EQUAL:
      l1 = l2 = (l1 + l2) / 2;
      break;
    default:
      g_assert_not_reached ();
    }

  scale_point (p, c, l1, c);
  scale_point (p, c2, l2, c2);

  gtk_contour_set_curve (contour, prev, op1, p1, w1);
  gtk_contour_set_curve (contour, next, op2, p2, w2);
}

/* Make the line through the control points perpendicular
 * to the line bisecting the angle between neighboring
 * points, and make the lengths 1/3 of the distance to
 * the corresponding neighboring points.
 */
static void
maintain_automatic (GtkContour *contour,
                    int          point)
{
  Constraint constraint;
  int prev, next;
  GskPathOperation op1, op2;
  graphene_point_t p1[4], p2[4];
  float w1, w2;
  graphene_point_t *p, *c1, *c2, *pa, *pb;
  float l1, l2;
  graphene_point_t a;

  constraint = contour_get_constraint (contour, point);
  if (constraint != AUTO)
    return;

  prev = contour_curve_to_point (contour, point);
  next = contour_curve_from_point (contour, point);

  if (prev != -1 && next != -1)
    {
      gtk_contour_get_curve (contour, prev, &op1, p1, &w1);
      gtk_contour_get_curve (contour, next, &op2, p2, &w2);

      if (op1 != GSK_PATH_CUBIC || op2 != GSK_PATH_CUBIC)
        return;

      p = &p1[3];
      c1 = &p1[2];
      c2 = &p2[1];

      pa = &p1[0];
      pb = &p2[3];

      l1 = point_distance (p, pa);
      l2 = point_distance (p, pb);

      a.x = pb->x + (p->x - pa->x);
      a.y = pb->y + (p->y - pa->y);

      scale_point (p, &a, l2/3, c2);
      opposite_point (p, &a, l1/3, c1);

      gtk_contour_set_curve (contour, prev, op1, p1, w1);
      gtk_contour_set_curve (contour, next, op2, p2, w2);
    }
  else if (prev != -1)
    {
      gtk_contour_get_curve (contour, prev, &op1, p1, &w1);

      if (op1 != GSK_PATH_CUBIC)
        return;

      graphene_point_interpolate (&p1[3], &p1[1], 0.5, &p1[2]);
      gtk_contour_set_curve (contour, prev, op1, p1, w1);
    }
  else if (next != -1)
    {
      gtk_contour_get_curve (contour, next, &op2, p2, &w2);

      if (op2 != GSK_PATH_CUBIC)
        return;

      graphene_point_interpolate (&p2[0], &p2[2], 0.5, &p2[1]);
      gtk_contour_set_curve (contour, next, op2, p2, w2);
    }
}

static void
drop_automatic (GtkContour *contour,
                int         point)
{
  if (contour_get_constraint (contour, point) == AUTO)
    contour_set_constraint (contour, point, SMOOTH);
}

/* }}} */
/* {{{ Drag implementation */

static void
drag_line_point (GtkContour            *contour,
                 unsigned int           idx,
                 const graphene_point_t *m)
{
  graphene_point_t d;
  int prev, next;

  gtk_contour_get_point (contour, idx, &d);
  gtk_contour_set_point (contour, idx, m);

  prev = contour_curve_to_point (contour, idx);
  next = contour_curve_from_point (contour, idx);

  if (prev != -1)
    {
      GskPathOperation op1;
      graphene_point_t p1[4];
      float w1;

      gtk_contour_get_curve (contour, prev, &op1, p1, &w1);
      if (op1 == GSK_PATH_CUBIC)
        {
           p1[2].x += m->x - d.x;
           p1[2].y += m->y - d.y;
           gtk_contour_set_curve (contour, prev, op1, p1, w1);
        }
    }

  if (next != -1)
    {
      GskPathOperation op2;
      graphene_point_t p2[4];
      float w2;

      gtk_contour_get_curve (contour, next, &op2, p2, &w2);
      if (op2 == GSK_PATH_CUBIC)
        {
           p2[1].x += m->x - d.x;
           p2[1].y += m->y - d.y;
           gtk_contour_set_curve (contour, next, op2, p2, w2);
        }
    }

  if (prev != -1)
    {
      int start = contour_curve_start_point (contour, prev);

      maintain_smoothness (contour, start, CONSTRAINT_PRESERVE_IN);
      maintain_symmetry (contour, start, CONSTRAINT_PRESERVE_IN);
      maintain_automatic (contour, start);
    }

  if (next != -1)
    {
      int end = contour_curve_end_point (contour, next);

      maintain_smoothness (contour, end, CONSTRAINT_PRESERVE_OUT);
      maintain_symmetry (contour, end, CONSTRAINT_PRESERVE_OUT);
      maintain_automatic (contour, end);
    }

  maintain_smoothness (contour, idx, CONSTRAINT_PRESERVE_EQUAL);
  maintain_symmetry (contour, idx, CONSTRAINT_PRESERVE_EQUAL);
  maintain_automatic (contour, idx);
}

static void
drag_control_point (GtkContour             *contour,
                    unsigned int            curve,
                    unsigned int            point,
                    const graphene_point_t *m)
{
  contour_set_curve_point (contour, curve, point, m);
  int start, end;

  start = contour_curve_start_point (contour, curve);
  end = contour_curve_end_point (contour, curve);

  if (point == 1)
    drop_automatic (contour, start);
  if (point == 2)
    drop_automatic (contour, end);

  maintain_smoothness (contour, start, CONSTRAINT_PRESERVE_OUT);
  maintain_symmetry (contour, start, CONSTRAINT_PRESERVE_OUT);

  maintain_smoothness (contour, end, CONSTRAINT_PRESERVE_IN);
  maintain_symmetry (contour, end, CONSTRAINT_PRESERVE_IN);
}

static void
contour_drag_point (GtkContour             *contour,
                    unsigned int            curve,
                    int                     point,
                    const graphene_point_t *m)
{
  if (point == 0)
    drag_line_point (contour, contour_curve_start_point (contour, curve), m);
  else if (point == 3)
    drag_line_point (contour, contour_curve_end_point (contour, curve), m);
  else
    drag_control_point (contour, curve, point, m);
}

static void
contour_drag_curve (GtkContour             *contour,
                    unsigned int            idx,
                    const graphene_point_t *m)
{
  GskPathOperation op;
  graphene_point_t p[4];
  float w;
  int prev, next;
  Constraint constraint;
  graphene_point_t S, E;
  graphene_point_t B, C1, C2;
  int start, end;

  gtk_contour_get_curve (contour, idx, &op, p, &w);

  if (op != GSK_PATH_CUBIC)
    {
      /* FIXME */
      return;
    }

  S = p[0];
  B = *m;
  E = p[3];

  bezier_through (&S, &B, &E, &C1, &C2);

  p[1] = C1;
  p[2] = C2;

  /* Maintain smoothness and symmetry
   *
   * When the neighboring segments are lines, we can't actually
   * use C1 and C2 as-is, since we need control points to lie
   * on the line. So we just use their distance. This makes our
   * point B not quite match anymore, but we're overconstrained.
   */

  start = contour_curve_start_point (contour, idx);
  end = contour_curve_end_point (contour, idx);

  drop_automatic (contour, start);
  drop_automatic (contour, end);

  constraint = contour_get_constraint (contour, start);
  prev = contour_curve_to_curve (contour, idx);

  if (constraint != CUSP && prev != -1)
    {
      GskPathOperation op2;
      graphene_point_t p2[4];
      float w2;

      gtk_contour_get_curve (contour, prev, &op2, p2, &w2);

      if (op2 != GSK_PATH_CUBIC)
        {
          graphene_point_t *a, *c, *d;

          if (op2 == GSK_PATH_LINE)
            a = &p2[0];
          else
            a = &p2[1];
          c = &p[0];
          d = &p[1];

          opposite_point (c, a, point_distance (c, d), d);
        }
      else
        {
          double l;

          if (constraint == SYMMETRIC)
            l = point_distance (&p[0], &p[1]);
          else
            l = point_distance (&p[0], &p2[2]);
          opposite_point (&p[0], &p[1], l, &p2[2]);

          gtk_contour_set_curve (contour, prev, op2, p2, w2);
        }
    }

  constraint = contour_get_constraint (contour, end);
  next = contour_curve_from_curve (contour, idx);

  if (constraint != CUSP && next != -1)
    {
      GskPathOperation op1;
      graphene_point_t p1[4];
      float w1;

      drop_automatic (contour, contour_curve_end_point (contour, next));

      gtk_contour_get_curve (contour, next, &op1, p1, &w1);

      if (op1 != GSK_PATH_CUBIC)
        {
          graphene_point_t *a, *c, *d;

          if (op1 == GSK_PATH_LINE)
            a = &p1[3];
          else
            a = &p1[1];
          c = &p[3];
          d = &p[2];

          opposite_point (c, a, point_distance (c, d), d);
        }
      else
        {
          double l;

          if (constraint == SYMMETRIC)
            l = point_distance (&p[3], &p[2]);
          else
            l = point_distance (&p[3], &p1[1]);
          opposite_point (&p[3], &p[2], l, &p1[1]);

          gtk_contour_set_curve (contour, next, op1, p1, w1);
        }
    }

  gtk_contour_set_curve (contour, idx, op, p, w);
}

/* }}} */
/* {{{ Action callbacks */

static void
set_constraint (GSimpleAction *action,
                GVariant      *value,
                gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  GtkContour *contour;
  Constraint constraint;

  contour = g_ptr_array_index (self->contours, self->context_contour);

  constraint = constraint_from_string (g_variant_get_string (value, NULL));
  contour_set_constraint (contour, self->context_point, constraint);

  maintain_smoothness (contour, self->context_point, CONSTRAINT_PRESERVE_EQUAL);
  maintain_symmetry (contour, self->context_point, CONSTRAINT_PRESERVE_EQUAL);
  maintain_automatic (contour, self->context_point);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_operation (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  GtkContour *contour;
  GskPathOperation op, new_op;
  graphene_point_t p[4];
  float weight;
  graphene_point_t q;
  int start, end;

  contour = g_ptr_array_index (self->contours, self->context_contour);
  gtk_contour_get_curve (contour, self->context_curve, &op, p, &weight);

  new_op = op_from_string (g_variant_get_string (value, NULL));
  if (new_op == op)
    return;

  switch (new_op)
    {
    case GSK_PATH_LINE:
      break;
    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      if (op == GSK_PATH_LINE)
        graphene_point_interpolate (&p[0], &p[3], 0.5, &p[1]);
      else if (op == GSK_PATH_CUBIC)
        {
          if (line_intersection (&p[0], &p[1], &p[3], &p[2], &q))
            p[1] = q;
          else
            {
              graphene_point_interpolate (&p[1], &p[3], 0.5, &q);
              p[1] = q;
            }
        }
      break;
    case GSK_PATH_CUBIC:
      if (op == GSK_PATH_LINE)
        graphene_point_interpolate (&p[0], &p[3], 0.5, &q);
      else
        q = p[1];
      graphene_point_interpolate (&p[0], &q, 0.667, &p[1]);
      graphene_point_interpolate (&p[3], &q, 0.667, &p[2]);
      break;
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }

  if (new_op == GSK_PATH_CONIC && weight == 0)
    weight = 1;

  gtk_contour_set_curve (contour, self->context_curve, new_op, p, weight);

  start = contour_curve_start_point (contour, self->context_curve);
  end = contour_curve_end_point (contour, self->context_curve);

  update_constraint (contour, start);
  update_constraint (contour, end);

  maintain_smoothness (contour, start, CONSTRAINT_PRESERVE_IN);
  maintain_smoothness (contour, end, CONSTRAINT_PRESERVE_OUT);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
insert_new_point (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  GtkContour *contour;

  contour = g_ptr_array_index (self->contours, self->context_contour);
  contour_insert_point (contour, self->context_curve, self->context_pos, SMOOTH);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_current_point (GSimpleAction *action,
                      GVariant      *value,
                      gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  GtkContour *contour;

  contour = g_ptr_array_index (self->contours, self->context_contour);
  contour_remove_point (contour, self->context_point);

  if (gtk_contour_get_n_curves (contour) == 0)
    {
      g_ptr_array_remove_index (self->contours, self->context_contour);
      self->context_contour = -1;
      /* FIXME update other state */
    }

  update_constraint (contour, contour_curve_end_point (contour, contour_curve_to_point (contour, self->context_point)));

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
select_current_point (GSimpleAction *action,
                      GVariant      *value,
                      gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  self->selected_contour = self->context_contour;
  self->selected_point = self->context_point;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
connect_current_point (GSimpleAction *action,
                       GVariant      *value,
                       gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->context_contour == self->selected_contour)
    {
      GtkContour *contour = g_ptr_array_index (self->contours, self->context_contour);
      gtk_contour_close (contour);
    }
  else
    {
      GtkContour *contour1 = g_ptr_array_index (self->contours, self->context_contour);
      GtkContour *contour2 = g_ptr_array_index (self->contours, self->selected_contour);

      if (self->context_point != 0 && self->selected_point == 0)
        {
          g_print ("appending selected to context\n");
          contour_append (contour1, g_object_ref (contour2));
          g_ptr_array_remove_index (self->contours, self->selected_contour);
        }
      else if (self->context_point == 0 && self->selected_point != 0)
        {
          g_print ("appending context to selected\n");
          contour_append (contour2, g_object_ref (contour1));
          g_ptr_array_remove_index (self->contours, self->context_contour);
        }
      else
        {
          /* FIXME */
        }
    }

  self->selected_contour = -1;
  self->selected_point = -1;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_current_curve (GSimpleAction *action,
                      GVariant      *value,
                      gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  GtkContour *contour, *c1, *c2;
  GArray *constraints;
  unsigned int start, end;

  contour = g_ptr_array_index (self->contours, self->context_contour);

  gtk_contour_split (contour, self->context_curve, &c1, &c2);

  start = contour_curve_start_point (contour, self->context_curve);
  end = contour_curve_end_point (contour, self->context_curve);

  if (gtk_contour_is_closed (contour))
    {
      constraints = g_array_new (FALSE, FALSE, sizeof (Constraint));
      g_object_set_data_full (G_OBJECT (c1), "constraints", constraints,
                              (GDestroyNotify) g_array_unref);
      g_array_set_size (constraints, gtk_contour_get_n_points (c1));

      for (unsigned int i = end; i < gtk_contour_get_n_points (contour); i++)
        contour_set_constraint (c1, i - end, contour_get_constraint (contour, i));
      for (unsigned int i = 1; i <= start; i++)
        contour_set_constraint (c1, i + (gtk_contour_get_n_points (contour) - end), contour_get_constraint (contour, i));
    }
  else
    {
      constraints = g_array_new (FALSE, FALSE, sizeof (Constraint));
      g_object_set_data_full (G_OBJECT (c1), "constraints", constraints,
                              (GDestroyNotify) g_array_unref);
      g_array_set_size (constraints, gtk_contour_get_n_points (c1));
      constraints = g_array_new (FALSE, FALSE, sizeof (Constraint));
      g_object_set_data_full (G_OBJECT (c2), "constraints", constraints,
                              (GDestroyNotify) g_array_unref);
      g_array_set_size (constraints, gtk_contour_get_n_points (c2));

      for (unsigned int i = 0; i < gtk_contour_get_n_points (c1); i++)
        contour_set_constraint (c1, i, contour_get_constraint (contour, i));

      for (unsigned int i = 0; i < gtk_contour_get_n_points (c2); i++)
        contour_set_constraint (c2, i, contour_get_constraint (contour, i + end));
    }

  g_ptr_array_remove_index (self->contours, self->context_contour);
  self->context_contour = -1;
  /* FIXME update other state */

  if (gtk_contour_get_n_curves (c1) > 0)
    g_ptr_array_add (self->contours, c1);
  else
    g_object_unref (c1);

  if (c2)
    {
      if (gtk_contour_get_n_curves (c2) > 0)
        g_ptr_array_add (self->contours, c2);
      else
        g_object_unref (c2);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
toggle_edit_point (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->edited_contour == self->context_contour &&
      self->edited_point == self->context_point)
    {
      self->edited_contour = -1;
      self->edited_point = -1;
    }
  else
    {
      self->edited_contour = self->context_contour;
      self->edited_point = self->context_point;
      self->edited_curve = -1;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
toggle_edit_curve (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  if (self->edited_contour == self->context_contour &&
      self->edited_curve == self->context_curve)
    {
      self->edited_contour = -1;
      self->edited_curve = -1;
    }
  else
    {
      self->edited_contour = self->context_contour;
      self->edited_curve = self->context_curve;
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
  GtkContour *contour;
  GskPathOperation op;
  graphene_point_t p[4];

  contour = g_ptr_array_index (self->contours, self->context_contour);
  gtk_contour_get_curve (contour, self->context_curve, &op, p, NULL);
  gtk_contour_set_curve (contour, self->context_curve, op, p, 1.f);

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
  graphene_point_t m = GRAPHENE_POINT_INIT (start_x, start_y);

  if (!self->edit)
    return;

  for (unsigned int i = 0; i < self->contours->len; i++)
    {
      GtkContour *contour = g_ptr_array_index (self->contours, i);
      unsigned int idx;
      float t;

      for (unsigned int j = 0; j < gtk_contour_get_n_curves (contour); j++)
        {
          GskPathOperation op;
          graphene_point_t p[4];

          contour_get_curve (contour, j, &op, p);

          for (int k = 0; k < 4; k++)
            {
              if (!point_is_visible (self, i, j, k))
                continue;

              if (point_distance (&p[k], &m) < CLICK_RADIUS)
                {
                  self->dragged_contour = i;
                  self->dragged_curve = j;
                  self->dragged_point = k;

                  gtk_widget_queue_draw (GTK_WIDGET (self));
                  return;
                }
            }
        }

      if (gtk_contour_find_closest_curve (contour, &m, CLICK_RADIUS, &idx, NULL, &t))
        {
          GskPathOperation op;
          graphene_point_t p[4];
          float weight;

          gtk_contour_get_curve (contour, idx, &op, p, &weight);
          if (op != GSK_PATH_CUBIC)
            gtk_contour_set_curve (contour, idx, GSK_PATH_CUBIC, p, weight);

          self->dragged_contour = i;
          self->dragged_curve = idx;
          self->dragged_point = -1;

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
  graphene_point_t m;
  double x, y;

  gtk_gesture_drag_get_start_point (gesture, &x, &y);

  m.x = x + offset_x;
  m.y = y + offset_y;

  if (self->dragged_contour != -1 && self->dragged_curve != -1)
    {
      GtkContour *contour;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      contour = g_ptr_array_index (self->contours, self->dragged_contour);
      if (self->dragged_point != -1)
        contour_drag_point (contour, self->dragged_curve, self->dragged_point, &m);
      else
        contour_drag_curve (contour, self->dragged_curve, &m);

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

  self->dragged_contour = -1;
  self->dragged_curve = -1;
  self->dragged_point = -1;
}

static void
pressed (GtkGestureClick *gesture,
         int              n_press,
         double           x,
         double           y,
         CurveEditor      *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  float t;

  if (!self->edit)
    return;

  if (button == GDK_BUTTON_SECONDARY)
    {
      for (unsigned int i = 0; i < self->contours->len; i++)
        {
          GtkContour *contour = g_ptr_array_index (self->contours, i);
          unsigned int idx;

          for (unsigned int j = 0; j < gtk_contour_get_n_points (contour); j++)
            {
              graphene_point_t p;
              Constraint constraint;
              gboolean is_endpoint;

              gtk_contour_get_point (contour, j, &p);
              constraint = contour_get_constraint (contour, j);

              if (gtk_contour_is_closed (contour))
                is_endpoint = FALSE;
              else
                is_endpoint = j == 0 || j == gtk_contour_get_n_points (contour) - 1;

              if (point_distance (&p, &m) < CLICK_RADIUS)
                {
                  GAction *action;
                  gboolean is_edited;

                  self->context_contour = i;
                  self->context_point = j;
                  self->context_curve = -1;

                  is_edited = (self->edited_contour == self->context_contour &&
                               self->edited_point == self->context_point);

                  action = g_action_map_lookup_action (self->actions, "set-curve-type");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

                  action = g_action_map_lookup_action (self->actions, "add-point");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

                  action = g_action_map_lookup_action (self->actions, "remove-point");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

                  action = g_action_map_lookup_action (self->actions, "select-point");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), is_endpoint);

                  action = g_action_map_lookup_action (self->actions, "connect-point");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), is_endpoint && self->selected_point != -1);

                  action = g_action_map_lookup_action (self->actions, "remove-curve");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

                  action = g_action_map_lookup_action (self->actions, "reset-weight");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

                  action = g_action_map_lookup_action (self->actions, "set-constraint");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_constrain (contour, j));
                  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (constraint_to_string (constraint)));

                  action = g_action_map_lookup_action (self->actions, "edit-point");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
                  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (is_edited));

                  action = g_action_map_lookup_action (self->actions, "edit-curve");
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

                  gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                                               &(const GdkRectangle){ x, y, 1, 1 });
                  gtk_popover_popup (GTK_POPOVER (self->menu));
                  return;
                }
            }

          if (gtk_contour_find_closest_curve (contour, &m, CLICK_RADIUS, &idx, NULL, &t))
            {
              GAction *action;
              GskPathOperation op;
              gboolean is_edited;

              gtk_contour_get_curve (contour, idx, &op, NULL, NULL);

              self->context_contour = i;
              self->context_point = -1;
              self->context_curve = idx;
              self->context_pos = t;

              is_edited = (self->edited_contour == self->context_contour &&
                           self->edited_curve == self->context_curve);

              action = g_action_map_lookup_action (self->actions, "set-constraint");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "edit-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "remove-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "select-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "connect-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

              action = g_action_map_lookup_action (self->actions, "add-point");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

              action = g_action_map_lookup_action (self->actions, "remove-curve");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

              action = g_action_map_lookup_action (self->actions, "edit-curve");
              g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (is_edited));

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
  GdkEventSequence *current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  GdkEvent *event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), current);
  GdkModifierType state = gdk_event_get_modifier_state (event);

  if (!self->edit)
    return;

  if (button == GDK_BUTTON_PRIMARY)
    {
      for (unsigned int i = 0; i < self->contours->len; i++)
        {
          GtkContour *contour = g_ptr_array_index (self->contours, i);
          unsigned int idx;
          float t;

          for (unsigned int j = 0; j < gtk_contour_get_n_points (contour); j++)
            {
              graphene_point_t p;

              gtk_contour_get_point (contour, j, &p);
              if (point_distance (&p, &m) < CLICK_RADIUS)
                {
                  if ((state & GDK_CONTROL_MASK) == 0)
                    {
                      if (self->edited_contour == i && self->edited_point == j)
                        {
                          self->edited_contour = -1;
                          self->edited_point = -1;
                          self->edited_curve = -1;
                        }
                      else
                        {
                          self->edited_contour = i;
                          self->edited_point = j;
                          self->edited_curve = -1;
                        }
                    }
                  else
                    {
                      if (self->selected_contour == i && self->selected_point == j)
                        {
                          self->selected_contour = -1;
                          self->selected_point = -1;
                        }
                      else
                        {
                          self->selected_contour = i;
                          self->selected_point = j;
                        }
                    }

                  gtk_widget_queue_draw (GTK_WIDGET (self));
                  return;
                }
            }

          if (gtk_contour_find_closest_curve (contour, &m, CLICK_RADIUS, &idx, NULL, &t))
            {
              self->dragged_contour = -1;
              self->dragged_curve = -1;
              self->dragged_point = -1;

              contour_insert_point (contour, idx, t, SMOOTH);

              gtk_widget_queue_draw (GTK_WIDGET (self));
              return;
            }
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

  if (self->edit)
    {
      int old_hovered_contour = self->hovered_contour;
      int old_hovered_curve = self->hovered_curve;
      int old_hovered_point = self->hovered_point;

      self->hovered_contour = -1;
      self->hovered_curve = -1;
      self->hovered_point = -1;

      for (unsigned int i = 0; i < self->contours->len; i++)
        {
          GtkContour *contour = g_ptr_array_index (self->contours, i);

          for (unsigned int j = 0; j < gtk_contour_get_n_curves (contour); j++)
            {
              graphene_point_t p[4];

              contour_get_curve (contour, j, NULL, p);

              for (int k = 0; k < 4; k++)
                {
                  if (!point_is_visible (self, i, j, k))
                    continue;

                  if (point_distance (&p[k], &m) < CLICK_RADIUS)
                    {
                      self->hovered_contour = i;
                      self->hovered_curve = j;
                      self->hovered_point = k;
                      goto done;
                    }
                }
            }
        }

done:
      if (old_hovered_contour != self->hovered_contour ||
          old_hovered_curve != self->hovered_curve ||
          old_hovered_point != self->hovered_point)
        gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
leave (GtkEventController *controller,
       CurveEditor        *self)
{
  self->hovered_contour = -1;
  self->hovered_curve = -1;
  self->hovered_point = -1;

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
  GskPath *path;
  GskStroke *stroke;
  float width;
  float height;

  if (self->contours->len == 0)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  /* Add the curve itself */
  path = curve_editor_get_path (self);

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

  if (self->edit && self->edited_contour != -1)
    {
      GtkContour *contour;
      int n_curves;
      GskPathBuilder *builder;
      GskPathOperation op;
      graphene_point_t p[4];
      float weight;

      builder = gsk_path_builder_new ();

      contour = g_ptr_array_index (self->contours, self->edited_contour);
      n_curves = gtk_contour_get_n_curves (contour);

      if (self->edited_point != -1)
        {
          int prev, next;

          /* Add the skeleton */
          if (gtk_contour_is_closed (contour))
            {
              prev = (self->edited_point + n_curves - 1) % n_curves;
              next = self->edited_point % n_curves;
            }
          else
            {
              prev = self->edited_point - 1;
              next = self->edited_point;
            }

          if (prev != -1)
            {
              gtk_contour_get_curve (contour, prev, &op, p, &weight);

              if (op == GSK_PATH_CUBIC)
                {
                  gsk_path_builder_move_to (builder, p[2].x, p[2].y);
                  gsk_path_builder_line_to (builder, p[3].x, p[3].y);
                }
              else if (op == GSK_PATH_CONIC || op == GSK_PATH_QUAD)
                {
                  gsk_path_builder_move_to (builder, p[1].x, p[1].y);
                  gsk_path_builder_line_to (builder, p[3].x, p[3].y);
                }
            }

          if (next != n_curves)
            {
              gtk_contour_get_curve (contour, next, &op, p, &weight);

              if (op == GSK_PATH_CUBIC || op == GSK_PATH_CONIC || op == GSK_PATH_QUAD)
                {
                  gsk_path_builder_move_to (builder, p[0].x, p[0].y);
                  gsk_path_builder_line_to (builder, p[1].x, p[1].y);
                }
            }
        }

      if (self->edited_curve != -1)
        {
          gtk_contour_get_curve (contour, self->edited_curve, &op, p, &weight);

          if (op == GSK_PATH_CUBIC)
            {
              gsk_path_builder_move_to (builder, p[0].x, p[0].y);
              gsk_path_builder_line_to (builder, p[1].x, p[1].y);
              gsk_path_builder_line_to (builder, p[2].x, p[2].y);
              gsk_path_builder_line_to (builder, p[3].x, p[3].y);
            }
          else if (op == GSK_PATH_QUAD || op == GSK_PATH_CONIC)
            {
              gsk_path_builder_move_to (builder, p[0].x, p[0].y);
              gsk_path_builder_line_to (builder, p[1].x, p[1].y);
              gsk_path_builder_line_to (builder, p[3].x, p[3].y);
            }
        }

       path = gsk_path_builder_free_to_path (builder);

      if (self->edited_point != -1 || self->edited_curve != -1)
        {
          stroke = gsk_stroke_new (1);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 0, 0, 0, 1 },
                                     &GRAPHENE_RECT_INIT (0, 0, width, height ));

          gtk_snapshot_pop (snapshot);
        }
    }

  if (self->edit)
    {
      /* Draw the circles, in several passes, one for each color */

      const char *colors[] = {
        "blue",  /* selected */
        "red",   /* hovered */
        "white", /* smooth curve points */
      };
      GdkRGBA color;

      for (int pass = 0; pass < G_N_ELEMENTS (colors); pass++)
        {
          GskPathBuilder *builder = gsk_path_builder_new ();

          for (unsigned int i = 0; i < self->contours->len; i++)
            {
              GtkContour *contour = g_ptr_array_index (self->contours, i);

              for (unsigned int j = 0; j < gtk_contour_get_n_curves (contour); j++)
                {
                  for (unsigned int k = 0; k < 4; k++)
                    {
                      GskPathOperation op;
                      graphene_point_t p[4];

                      if (!point_is_visible (self, i, j, k))
                        continue;

                      if (pass == 0 && !point_is_selected (self, i, j, k))
                        continue;

                      if (pass == 1 && !point_is_hovered (self, i, j, k))
                        continue;

                      if (pass == 2 &&
                          (point_is_selected (self, i, j, k) || point_is_hovered (self, i, j, k)))
                        continue;

                      contour_get_curve (contour, j, &op, p);

                      if (k == 1 || k == 2)
                        {
                          gsk_path_builder_add_circle (builder, &p[k], DRAW_RADIUS);
                        }
                      else
                       {
                          switch (contour_get_constraint (contour, j))
                            {
                            case CUSP:
                              add_diamond (builder, &p[k], DRAW_RADIUS);
                              break;

                            case SMOOTH:
                              add_square (builder, &p[k], DRAW_RADIUS);
                              break;
                            case SYMMETRIC:
                            case AUTO:
                              gsk_path_builder_add_circle (builder, &p[k], DRAW_RADIUS);
                              break;
                            default:
                              g_assert_not_reached ();
                            }
                        }
                    }
                }
            }

          path = gsk_path_builder_free_to_path (builder);

          gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
          gdk_rgba_parse (&color, colors[pass]);
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

  g_ptr_array_unref (self->contours);

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

  self->context_contour = -1;
  self->context_point = -1;
  self->context_curve = -1;

  self->edited_contour = -1;
  self->edited_point = -1;
  self->edited_curve = -1;

  self->hovered_contour = -1;
  self->hovered_curve = -1;
  self->hovered_point = -1;

  self->dragged_contour = -1;
  self->dragged_point = -1;
  self->dragged_curve = -1;

  self->contours = g_ptr_array_new_with_free_func (g_object_unref);
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

  action = g_simple_action_new ("select-point", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (select_current_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("connect-point", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (connect_current_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("remove-curve", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (remove_current_curve), self);
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

  item = g_menu_item_new ("Automatic", "path.set-constraint::auto");
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

  item = g_menu_item_new ("Select", "path.select-point");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Connect", "path.connect-point");
  g_menu_item_set_attribute_value (item, "hidden-when", g_variant_new_string ("action-disabled"));
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Remove", "path.remove-curve");
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

  self->edited_contour = -1;
  self->edited_point = -1;
  self->edited_curve = -1;

  self->selected_contour = -1;
  self->selected_point = -1;

  self->hovered_contour = -1;
  self->hovered_curve = -1;
  self->hovered_point = -1;

  self->dragged_contour = -1;
  self->dragged_curve = -1;
  self->dragged_point = -1;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
collect_curve (GskPathOperation        op,
               const graphene_point_t *p,
               gsize                  n_pts,
               float                   weight,
               gpointer                data)
{
  CurveEditor *self = data;
  GtkContour *contour;

  switch (op)
    {
    case GSK_PATH_MOVE:
      g_ptr_array_add (self->contours, gtk_contour_new (&p[0]));
      break;
    case GSK_PATH_CLOSE:
      contour = g_ptr_array_index (self->contours, self->contours->len - 1);
      gtk_contour_close (contour);
      break;
    case GSK_PATH_LINE:
      contour = g_ptr_array_index (self->contours, self->contours->len - 1);
      gtk_contour_line_to (contour, &p[1]);
      break;
    case GSK_PATH_QUAD:
      contour = g_ptr_array_index (self->contours, self->contours->len - 1);
      gtk_contour_quad_to (contour, &p[1], &p[2]);
      break;
    case GSK_PATH_CUBIC:
      contour = g_ptr_array_index (self->contours, self->contours->len - 1);
      gtk_contour_cubic_to (contour, &p[1], &p[2], &p[3]);
      break;
    case GSK_PATH_CONIC:
      contour = g_ptr_array_index (self->contours, self->contours->len - 1);
      gtk_contour_conic_to (contour, &p[1], weight, &p[2]);
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
  g_ptr_array_set_size (self->contours, 0);
  gsk_path_foreach (path, -1, collect_curve, self);

  for (unsigned int i = 0; i < self->contours->len; i++)
    {
      GtkContour *contour = g_ptr_array_index (self->contours, i);

      GArray *constraints = g_array_new (FALSE, FALSE, sizeof (Constraint));

      for (unsigned int j = 0; j < gtk_contour_get_n_points (contour); j++)
        {
          int prev, next;
          GskPathOperation op1, op2;
          graphene_point_t p1[4], p2[4];
          const graphene_point_t *c1, *c, *c2;
          Constraint constraint = CUSP;

          prev = contour_curve_to_point (contour, j);
          next = contour_curve_from_point (contour, j);

          if (prev != -1 && next != -1)
            {
              gtk_contour_get_curve (contour, prev, &op1, p1, NULL);
              gtk_contour_get_curve (contour, next, &op2, p2, NULL);

              if (op1 != GSK_PATH_LINE || op2 != GSK_PATH_LINE)
                {
                  c = &p1[3];
                  if (op1 == GSK_PATH_LINE)
                    c1 = &p1[0];
                  else if (op1 == GSK_PATH_CUBIC)
                    c1 = &p1[2];
                  else
                    c1 = &p1[1];

                  if (op2 == GSK_PATH_LINE)
                    c2 = &p2[3];
                  else
                    c2 = &p2[1];

                  if (collinear (c, c1, c2))
                    constraint = SMOOTH;
                  else
                    constraint = CUSP;
               }
            }

          g_array_append_val (constraints, constraint);
        }

      g_object_set_data_full (G_OBJECT (contour), "constraints", constraints,
                              (GDestroyNotify) g_array_unref);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

GskPath *
curve_editor_get_path (CurveEditor *self)
{
  GskPathBuilder *builder = gsk_path_builder_new ();

  for (unsigned int i = 0; i < self->contours->len; i++)
    {
      GtkContour *contour = g_ptr_array_index (self->contours, i);
      gtk_contour_add_to_path_builder (contour, builder);
    }

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
