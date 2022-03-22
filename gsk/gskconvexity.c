#include "config.h"

#include "gskconvexityprivate.h"
#include "gskcontourprivate.h"

typedef enum
{
  GSK_DIR_CHANGE_UNKNOWN,
  GSK_DIR_CHANGE_LEFT,
  GSK_DIR_CHANGE_RIGHT,
  GSK_DIR_CHANGE_STRAIGHT,
  GSK_DIR_CHANGE_REVERSE,
  GSK_DIR_CHANGE_INVALID,
} GskDirChange;

typedef enum
{
  GSK_PATH_DIRECTION_UNKNOWN,
  GSK_PATH_DIRECTION_CLOCKWISE,
  GSK_PATH_DIRECTION_COUNTERCLOCKWISE,
} GskPathDirection;

typedef struct _GskConvexityChecker GskConvexityChecker;
struct _GskConvexityChecker
{
  graphene_point_t first_point;
  graphene_vec2_t first_vec;
  graphene_point_t last_point;
  graphene_vec2_t last_vec;
  GskDirChange expected_direction;
  GskPathDirection first_direction;
  int reversals;
  gboolean finite;
  GskConvexity convexity;
  int xsign;
  int ysign;
  int dx;
  int dy;
};

static float
cross_product (graphene_vec2_t *a,
               graphene_vec2_t *b)
{
  float fa[2];
  float fb[2];
  graphene_vec2_to_float (a, fa);
  graphene_vec2_to_float (b, fb);
  return fa[0] * fb[1] - fa[1] * fb[0];
}

static GskDirChange
direction_change (GskConvexityChecker    *c,
                  graphene_vec2_t *v)
{
  float cross = cross_product (&(c->last_vec), v);
  if (!finite (cross))
    return GSK_DIR_CHANGE_UNKNOWN;

  if (cross == 0)
    return graphene_vec2_dot (&(c->last_vec), v) < 0
           ? GSK_DIR_CHANGE_REVERSE
           : GSK_DIR_CHANGE_STRAIGHT;

  return cross > 0
         ? GSK_DIR_CHANGE_RIGHT
         : GSK_DIR_CHANGE_LEFT;
}

static gboolean
add_vec (GskConvexityChecker *c,
         graphene_vec2_t     *v)
{
  GskDirChange dir;
  int sign;

  dir = direction_change (c, v);
  switch (dir)
    {
    case GSK_DIR_CHANGE_LEFT:
    case GSK_DIR_CHANGE_RIGHT:
      if (c->expected_direction == GSK_DIR_CHANGE_INVALID)
        {
          c->expected_direction = dir;
          c->first_direction = (dir == GSK_DIR_CHANGE_RIGHT)
                               ? GSK_PATH_DIRECTION_CLOCKWISE
                               : GSK_PATH_DIRECTION_COUNTERCLOCKWISE;
        }
      else if (c->expected_direction != dir)
        {
          c->first_direction = GSK_PATH_DIRECTION_UNKNOWN;
          c->convexity = GSK_CONVEXITY_CONCAVE;
          return FALSE;
        }
      graphene_vec2_init_from_vec2 (&c->last_vec, v);
      break;

    case GSK_DIR_CHANGE_STRAIGHT:
      break;

    case GSK_DIR_CHANGE_REVERSE:
      graphene_vec2_init_from_vec2 (&c->last_vec, v);
      c->reversals++;
      if (c->reversals > 2)
        c->convexity = GSK_CONVEXITY_CONCAVE;
      return c->reversals < 3;

    case GSK_DIR_CHANGE_UNKNOWN:
      c->finite = FALSE;
      return FALSE;

    case GSK_DIR_CHANGE_INVALID:
    default:
      g_assert_not_reached ();
    }

  if (graphene_vec2_get_x (v) > 0)
    sign = 1;
  else if (graphene_vec2_get_x (v) < 0)
    sign = -1;
  else
    sign = 0;

  if (sign != 0)
    {
      if (c->xsign != 42)
        {
          if (c->xsign != sign)
            c->dx++;

          if (c->dx > 2)
            {
              c->convexity = GSK_CONVEXITY_CONCAVE;
              return FALSE;
            }
        }
      c->xsign = sign;
    }

  if (graphene_vec2_get_y (v) > 0)
    sign = 1;
  else if (graphene_vec2_get_y (v) < 0)
    sign = -1;
  else
    sign = 0;

  if (sign != 0)
    {
      if (c->ysign != 42)
        {
          if (c->ysign != sign)
            c->dy++;

          if (c->dy > 2)
            {
              c->convexity = GSK_CONVEXITY_CONCAVE;
              return FALSE;
            }
        }
      c->ysign = sign;
    }

  return TRUE;
}

static void
gsk_convexity_checker_init (GskConvexityChecker *c)
{
  c->first_point = GRAPHENE_POINT_INIT(0,0);
  c->last_point = GRAPHENE_POINT_INIT(0,0);
  graphene_vec2_init (&c->first_vec, 0, 0);
  graphene_vec2_init (&c->last_vec, 0, 0);
  c->expected_direction = GSK_DIR_CHANGE_INVALID;
  c->first_direction = GSK_PATH_DIRECTION_UNKNOWN;
  c->reversals = 0;
  c->finite = TRUE;
  c->convexity = GSK_CONVEXITY_UNKNOWN;
  c->xsign = 42;
  c->ysign = 42;
  c->dx = 0;
  c->dy = 0;
}

static void
gsk_convexity_checker_move (GskConvexityChecker    *c,
                            const graphene_point_t *p)
{
  c->first_point = c->last_point = *p;
  c->expected_direction = GSK_DIR_CHANGE_INVALID;
  c->convexity = GSK_CONVEXITY_CONVEX;
}

static gboolean
gsk_convexity_checker_add_point (GskConvexityChecker    *c,
                                 const graphene_point_t *p)
{
  graphene_vec2_t v;

  if (graphene_point_equal (&c->last_point, p))
    return TRUE;

  graphene_vec2_init (&v,
                      p->x - c->last_point.x,
                      p->y - c->last_point.y);

  if (graphene_point_equal (&c->first_point, &c->last_point) &&
      c->expected_direction == GSK_DIR_CHANGE_INVALID)
    {
      graphene_vec2_init_from_vec2 (&c->last_vec, &v);
      graphene_vec2_init_from_vec2 (&c->first_vec, &v);
    }
  else if (!add_vec (c, &v))
    {
      c->convexity = GSK_CONVEXITY_CONCAVE;
      return FALSE;
    }

  c->last_point = *p;
  return TRUE;
}

static gboolean
gsk_convexity_checker_close (GskConvexityChecker *c)
{
  if (!(gsk_convexity_checker_add_point (c, &c->first_point) &&
        add_vec (c, &c->first_vec)))
    {
      c->convexity = GSK_CONVEXITY_CONCAVE;
      return FALSE;
    }

  return TRUE;
}

static gboolean
convex_cb (GskPathOperation        op,
           const graphene_point_t *pts,
           gsize                   n_pts,
           float                   weight,
           gpointer                user_data)
{
  GskConvexityChecker *c = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_convexity_checker_move (c, &pts[0]);
      break;
    case GSK_PATH_CLOSE:
      if (!gsk_convexity_checker_close (c))
        return FALSE;
      break;
    case GSK_PATH_LINE:
      if (!gsk_convexity_checker_add_point (c, &pts[1]))
        return FALSE;
      break;
    case GSK_PATH_CURVE:
      if (!gsk_convexity_checker_add_point (c, &pts[1]) ||
          !gsk_convexity_checker_add_point (c, &pts[2]) ||
          !gsk_convexity_checker_add_point (c, &pts[3]))
        return FALSE;
      break;
    case GSK_PATH_CONIC:
      if (!gsk_convexity_checker_add_point (c, &pts[1]) ||
          !gsk_convexity_checker_add_point (c, &pts[3]))
        return FALSE;
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

GskConvexity
gsk_contour_compute_convexity (const GskContour *contour)
{
  GskConvexityChecker c;

  gsk_convexity_checker_init (&c);
  gsk_contour_foreach (contour, 0.001, convex_cb, &c);

  g_assert (c.convexity != GSK_CONVEXITY_UNKNOWN);

  return c.convexity;
}
