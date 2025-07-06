#include <gtk/gtk.h>

#define assert_path_point_equal(point,_contour,_idx,_t) \
  g_assert_cmpint ((point)->contour, ==, (_contour)); \
  g_assert_cmpint ((point)->idx, ==, (_idx)); \
  g_assert_cmpfloat_with_epsilon ((point)->t, (_t), 0.0001);

typedef struct
{
  GskPathPoint point1[20];
  GskPathPoint point2[20];
  GskPathIntersection kind[20];
  int found;
} CollectData;

static gboolean
collect_cb (GskPath             *path1,
            const GskPathPoint  *point1,
            GskPath             *path2,
            const GskPathPoint  *point2,
            GskPathIntersection  kind,
            gpointer             data)
{
  CollectData *res = data;
#if 0
  const char *kn[] = { "none", "normal", "start", "end" };

  g_print ("%s idx1 %lu t1 %f idx2 %lu t2 %f\n",
           kn[kind],
           point1->idx, point1->t, point2->idx, point2->t);
#endif

  g_assert_true (res->found < 20);

  res->point1[res->found] = *point1;
  res->point2[res->found] = *point2;
  res->kind[res->found] = kind;
  res->found++;

  return TRUE;
}

static void
test_intersect_simple (void)
{
  GskPath *path1, *path2;
  graphene_point_t p1, p2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 150 150 h 200 v 100 h -200 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  assert_path_point_equal (&res.point1[0], 0, 2, 0.5);
  assert_path_point_equal (&res.point1[1], 0, 3, 0.75);
  assert_path_point_equal (&res.point2[0], 0, 1, 0.75);
  assert_path_point_equal (&res.point2[1], 0, 4, 0.5);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_point_get_position (&res.point1[0], path1, &p1);
  gsk_path_point_get_position (&res.point2[0], path2, &p2);
  g_assert_true (graphene_point_equal (&p1, &p2));

  gsk_path_point_get_position (&res.point1[1], path1, &p1);
  gsk_path_point_get_position (&res.point2[1], path2, &p2);
  g_assert_true (graphene_point_near (&p1, &p2, 0.001));

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_simple2 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_simple3 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 300 100 h -200 v 100 h 200 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_reverse (void)
{
  GskPath *path1, *path2;
  graphene_point_t p1, p2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 150 150 v 100 h 200 v -100 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  assert_path_point_equal (&res.point1[0], 0, 2, 0.5);
  assert_path_point_equal (&res.point1[1], 0, 3, 0.75);
  assert_path_point_equal (&res.point2[0], 0, 4, 0.25);
  assert_path_point_equal (&res.point2[1], 0, 1, 0.5);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_point_get_position (&res.point1[0], path1, &p1);
  gsk_path_point_get_position (&res.point2[0], path2, &p2);
  g_assert_true (graphene_point_equal (&p1, &p2));

  gsk_path_point_get_position (&res.point1[1], path1, &p1);
  gsk_path_point_get_position (&res.point2[1], path2, &p2);
  g_assert_true (graphene_point_equal (&p1, &p2));

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_line_box (void)
{
  GskPath *path1, *path2;
  graphene_point_t p1, p2;
  CollectData res;

  path1 = gsk_path_parse ("M 50 150 l 300 0");
  path2 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  assert_path_point_equal (&res.point1[0], 0, 1, 50.f/300.f);
  assert_path_point_equal (&res.point1[1], 0, 1, 250.f/300.f);
  assert_path_point_equal (&res.point2[0], 0, 4, 0.5);
  assert_path_point_equal (&res.point2[1], 0, 2, 0.5);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_point_get_position (&res.point1[0], path1, &p1);
  gsk_path_point_get_position (&res.point2[0], path2, &p2);
  g_assert_true (graphene_point_equal (&p1, &p2));

  gsk_path_point_get_position (&res.point1[1], path1, &p1);
  gsk_path_point_get_position (&res.point2[1], path2, &p2);
  g_assert_true (graphene_point_equal (&p1, &p2));

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_xplus (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 0 0 L 100 100 M 0 100 L 100 0");
  path2 = gsk_path_parse ("M 0 50 L 100 50 M 50 0 L 50 100");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 4);
  assert_path_point_equal (&res.point1[0], 0, 1, 0.5);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.5);
  assert_path_point_equal (&res.point1[2], 1, 1, 0.5);
  assert_path_point_equal (&res.point1[3], 1, 1, 0.5);

  assert_path_point_equal (&res.point2[0], 0, 1, 0.5);
  assert_path_point_equal (&res.point2[1], 1, 1, 0.5);
  assert_path_point_equal (&res.point2[2], 0, 1, 0.5);
  assert_path_point_equal (&res.point2[3], 1, 1, 0.5);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_point (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 0 50");
  path2 = gsk_path_parse ("M 0 0 L 0 100");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);

  g_assert_true (res.found == 1);
  assert_path_point_equal (&res.point1[0], 0, 0, 1);
  assert_path_point_equal (&res.point2[0], 0, 1, 0.5);

  res.found = 0;
  gsk_path_foreach_intersection (path2, path1, collect_cb, &res);

  g_assert_true (res.found == 1);
  assert_path_point_equal (&res.point1[0], 0, 1, 0.5);
  assert_path_point_equal (&res.point2[0], 0, 0, 1);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path1, collect_cb, &res);

  g_assert_true (res.found == 1);
  assert_path_point_equal (&res.point1[0], 0, 0, 1);
  assert_path_point_equal (&res.point2[0], 0, 0, 1);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_contours (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 0 100 L 200 100");
  path2 = gsk_path_parse ("M 150 0 150 200 M 50 0 50 200");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  assert_path_point_equal (&res.point1[0], 0, 1, 0.25f);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.75f);
  assert_path_point_equal (&res.point2[0], 1, 1, 0.5f);
  assert_path_point_equal (&res.point2[1], 0, 1, 0.5f);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_contours2 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 0 100 L 200 100");
  path2 = gsk_path_parse ("M 150 0 L 150 200 M 50 0 L 50 200 M 60 100 L 140 100");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 4);
  assert_path_point_equal (&res.point1[0], 0, 1, 0.25f);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.3f);
  assert_path_point_equal (&res.point2[0], 1, 1, 0.5f);
  assert_path_point_equal (&res.point2[1], 2, 1, 0.f);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_contours3 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 150 0 L 150 200 M 50 0 L 50 200 M 60 100 L 140 100");
  path2 = gsk_path_parse ("M 0 100 L 200 100");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 4);
  assert_path_point_equal (&res.point1[0], 0, 1, 0.5f);
  assert_path_point_equal (&res.point1[1], 1, 1, 0.5f);
  assert_path_point_equal (&res.point2[0], 0, 1, 0.75f);
  assert_path_point_equal (&res.point2[1], 0, 1, 0.25f);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_coincide (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 150 100 h 100 v 50 h -100 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  assert_path_point_equal (&res.point1[0], 0, 1, 0.25);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.75);
  assert_path_point_equal (&res.point2[0], 0, 1, 0);
  assert_path_point_equal (&res.point2[1], 0, 1, 1);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_coincide2 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 150 100 h 100 v 50 h -100 z");
  path2 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);
  assert_path_point_equal (&res.point1[0], 0, 1, 0);
  assert_path_point_equal (&res.point1[1], 0, 1, 1);
  assert_path_point_equal (&res.point2[0], 0, 1, 0.25);
  assert_path_point_equal (&res.point2[1], 0, 1, 0.75);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_coincide3 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 150 100 h 100 v 50 h -25 v -50 h -50 v 50 h -25 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 4);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_END);

  assert_path_point_equal (&res.point1[0], 0, 1, 0.25);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.375);
  assert_path_point_equal (&res.point1[2], 0, 1, 0.625);
  assert_path_point_equal (&res.point1[3], 0, 1, 0.75);

  assert_path_point_equal (&res.point2[0], 0, 1, 0);
  assert_path_point_equal (&res.point2[1], 0, 5, 1);
  assert_path_point_equal (&res.point2[2], 0, 5, 0);
  assert_path_point_equal (&res.point2[3], 0, 1, 1);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_coincide4 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 100 100 h 200 v 100 h -200 z");
  path2 = gsk_path_parse ("M 150 100 h 100 v 50 h -25 v -100 h -50 v 100 h -25 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 4);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_END);

  assert_path_point_equal (&res.point1[0], 0, 1, 0.25);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.375);
  assert_path_point_equal (&res.point1[2], 0, 1, 0.625);
  assert_path_point_equal (&res.point1[3], 0, 1, 0.75);

  assert_path_point_equal (&res.point2[0], 0, 1, 0);
  assert_path_point_equal (&res.point2[1], 0, 6, 0.5);
  assert_path_point_equal (&res.point2[2], 0, 4, 0.5);
  assert_path_point_equal (&res.point2[3], 0, 1, 1);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

/* the next few tests explore overlapping segments */
static void
test_intersect_coincide5 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 150 100 h 100 v 100 h -100 z");
  path2 = gsk_path_parse ("M 100 100 h 200 v 50 h -100 v -50 h 25 v -50 h -100 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 5);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[4] == GSK_PATH_INTERSECTION_NORMAL);

  assert_path_point_equal (&res.point1[0], 0, 1, 0);
  assert_path_point_equal (&res.point1[1], 0, 1, 0.5);
  assert_path_point_equal (&res.point1[2], 0, 1, 0.75);
  assert_path_point_equal (&res.point1[3], 0, 1, 1);
  assert_path_point_equal (&res.point1[4], 0, 2, 0.5);

  assert_path_point_equal (&res.point2[0], 0, 1, 0.25);
  assert_path_point_equal (&res.point2[1], 0, 5, 0);
  assert_path_point_equal (&res.point2[2], 0, 5, 1);
  assert_path_point_equal (&res.point2[3], 0, 1, 0.75);
  assert_path_point_equal (&res.point2[4], 0, 3, 0.5);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_coincide6 (void)
{
  GskPath *path1, *path2;
  CollectData res;

  path1 = gsk_path_parse ("M 150 100 h 75 l 25 50 v 50 h -100 z");
  path2 = gsk_path_parse ("M 100 100 h 200 v 50 h -100 v -50 h 50 v -50 h -125 z");

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 5);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[4] == GSK_PATH_INTERSECTION_NORMAL);

  assert_path_point_equal (&res.point1[0], 0, 1, 0);
  assert_path_point_equal (&res.point1[1], 0, 1, 2./3.);
  assert_path_point_equal (&res.point1[2], 0, 1, 1);
  assert_path_point_equal (&res.point1[3], 0, 1, 1);
  assert_path_point_equal (&res.point1[4], 0, 3, 0);

  assert_path_point_equal (&res.point2[0], 0, 1, 0.25);
  assert_path_point_equal (&res.point2[1], 0, 5, 0);
  assert_path_point_equal (&res.point2[2], 0, 1, 0.625);
  assert_path_point_equal (&res.point2[3], 0, 5, 0.5);
  assert_path_point_equal (&res.point2[4], 0, 3, 0.5);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static int
circle_intersect (const graphene_point_t *center1,
                  float                   radius1,
                  const graphene_point_t *center2,
                  float                   radius2,
                  graphene_point_t        points[2])
{
  float d;
  float a, h;
  graphene_point_t m;
  graphene_vec2_t n;

  g_assert (radius1 >= 0);
  g_assert (radius2 >= 0);

  d = graphene_point_distance (center1, center2, NULL, NULL);

  if (d < fabsf (radius1 - radius2))
    return 0;

  if (d > radius1 + radius2)
    return 0;

  if (d == radius1 + radius2)
    {
      graphene_point_interpolate (center1, center2, radius1 / (radius1 + radius2), &points[0]);
      return 1;
    }

  /*
  a + b = d;

  a^2 + h^2 = r1^2
  b^2 + h^2 = r2^2

  a^2 - (d - a)^2 = r1^2 - r2^2
  a^2 - (d^2 - 2ad + a2) = r1^2 - r2^2

  2ad -d^2 = r1^2 - r2^2
  2ad = r1^2 - r2^2 + d^2
  a = (r1^2 - r2^2 + d^2) / (2d)

  p1/2 = c1 + a/d * (c2 - c1) +/- h * n(c1,c2);
  */

  a = (radius1*radius1 - radius2*radius2 + d*d)/(2*d);
  h = sqrtf (radius1*radius1 - a*a);

  graphene_point_interpolate (center1, center2, a/d, &m);
  graphene_vec2_init (&n, center2->y - center1->y, center1->x - center2->x);
  graphene_vec2_normalize (&n, &n);

  graphene_point_init (&points[0], m.x + graphene_vec2_get_x (&n) * h,
                                   m.y + graphene_vec2_get_y (&n) * h);
  graphene_point_init (&points[1], m.x - graphene_vec2_get_x (&n) * h,
                                   m.y - graphene_vec2_get_y (&n) * h);

  return 2;
}

static void
test_intersect_circle (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 0), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (1, 1), 10);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 0);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_circle2 (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 0), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 25), 10);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 0);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_circle3 (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 0), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 22), 10);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 1);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_circle4 (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;
  graphene_point_t p[2], pos;
  int n;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 0), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 18), 10);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);

  n = circle_intersect (&GRAPHENE_POINT_INIT (0, 0), 12,
                        &GRAPHENE_POINT_INIT (0, 18), 10,
                        p);

  g_assert_true (n == 2);
  gsk_path_point_get_position (&res.point1[0], path1, &pos);
  g_assert_true (graphene_point_near (&p[0], &pos, 0.01));
  gsk_path_point_get_position (&res.point1[1], path1, &pos);
  g_assert_true (graphene_point_near (&p[1], &pos, 0.01));

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_circle5 (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;
  graphene_point_t p[2], pos;
  int n;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 0), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (10, 10), 10);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);

  n = circle_intersect (&GRAPHENE_POINT_INIT (0, 0), 12,
                        &GRAPHENE_POINT_INIT (10, 10), 10,
                        p);

  g_assert_true (n == 2);
  gsk_path_point_get_position (&res.point1[0], path1, &pos);
  g_assert_true (graphene_point_near (&p[0], &pos, 0.01));
  gsk_path_point_get_position (&res.point1[1], path1, &pos);
  g_assert_true (graphene_point_near (&p[1], &pos, 0.01));

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_circle6 (void)
{
  GskPathBuilder *builder;
  GskPath *path1;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path1, collect_cb, &res);
  g_assert_true (res.found == 2);

  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
}

static void
test_intersect_circle7 (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 12);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path1);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 2);

  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_circle_rounded_rect (void)
{
  GskRoundedRect rr;
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  rr.bounds = GRAPHENE_RECT_INIT (10, 10, 100, 100);
  rr.corner[GSK_CORNER_TOP_LEFT] = GRAPHENE_SIZE_INIT (20, 20);
  rr.corner[GSK_CORNER_TOP_RIGHT] = GRAPHENE_SIZE_INIT (20, 20);
  rr.corner[GSK_CORNER_BOTTOM_RIGHT] = GRAPHENE_SIZE_INIT (20, 20);
  rr.corner[GSK_CORNER_BOTTOM_LEFT] = GRAPHENE_SIZE_INIT (20, 20);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, &rr);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (30, 30), 20);
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (90, 30), 20);
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (90, 90), 20);
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (30, 90), 20);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 8);

  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[4] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[5] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[6] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[7] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_loop_line (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 20, 0);
  gsk_path_builder_cubic_to (builder, 120, 100, -20, 100, 80, 0);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 50, 0);
  gsk_path_builder_line_to (builder, 50, 150);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 3);

  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_intersect_mix_segment (void)
{
  GskPathBuilder *builder;
  GskPath *path1, *path2;
  CollectData res;

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 10, 10);
  gsk_path_builder_line_to (builder, 100, 10);
  gsk_path_builder_line_to (builder, 100, 100);
  gsk_path_builder_line_to (builder, 10, 100);
  gsk_path_builder_close (builder);
  path1 = gsk_path_builder_free_to_path (builder);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 100, 0);
  gsk_path_builder_line_to (builder, 100, 50);
  gsk_path_builder_quad_to (builder, 100, 100, 125, 100);
  gsk_path_builder_quad_to (builder, 150, 100, 150, 75);
  gsk_path_builder_quad_to (builder, 150, 30, 70, 30);
  path2 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, path2, collect_cb, &res);
  g_assert_true (res.found == 3);

  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
test_self_intersect_loop (void)
{
  GskPathBuilder *builder;
  GskPath *path1;
  CollectData res;
  graphene_point_t p0, p1;

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 20, 0);
  gsk_path_builder_cubic_to (builder, 120, 100, -20, 100, 80, 0);
  path1 = gsk_path_builder_free_to_path (builder);

  res.found = 0;
  gsk_path_foreach_intersection (path1, NULL, collect_cb, &res);

  g_assert_cmpint (res.found, ==, 2);
  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);

  gsk_path_point_get_position (&res.point1[0], path1, &p0);
  gsk_path_point_get_position (&res.point1[1], path1, &p1);
  g_assert_true (graphene_point_near (&p0, &p1, 0.001));

  gsk_path_unref (path1);
}

static void
test_self_intersect_lollipop (void)
{
  GskPath *path1;
  CollectData res;

  path1 = gsk_path_parse ("M 150 0 L 150 100 Q 150 200 200 200 Q 250 200 250 100 Q 250 50 150 50 L 100 50 Q 50 50 50 100 Q 50 150 100 150 Q 150 150 150 100 L 150 0");

  res.found = 0;
  gsk_path_foreach_intersection (path1, NULL, collect_cb, &res);
  g_assert_cmpint (res.found, ==, 8);

  g_assert_true (res.kind[0] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[1] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[2] == GSK_PATH_INTERSECTION_END);
  g_assert_true (res.kind[3] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[4] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[5] == GSK_PATH_INTERSECTION_START);
  g_assert_true (res.kind[6] == GSK_PATH_INTERSECTION_NORMAL);
  g_assert_true (res.kind[7] == GSK_PATH_INTERSECTION_END);

  gsk_path_unref (path1);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  g_test_set_nonfatal_assertions ();

  g_test_add_func ("/path/intersect/simple", test_intersect_simple);
  g_test_add_func ("/path/intersect/simple2", test_intersect_simple2);
  g_test_add_func ("/path/intersect/simple3", test_intersect_simple3);
  g_test_add_func ("/path/intersect/reverse", test_intersect_reverse);
  g_test_add_func ("/path/intersect/line-box", test_intersect_line_box);
  g_test_add_func ("/path/intersect/xplus", test_intersect_xplus);
  g_test_add_func ("/path/intersect/point", test_intersect_point);
  g_test_add_func ("/path/intersect/contours", test_intersect_contours);
  g_test_add_func ("/path/intersect/contours2", test_intersect_contours2);
  g_test_add_func ("/path/intersect/contours3", test_intersect_contours3);
  g_test_add_func ("/path/intersect/coincide", test_intersect_coincide);
  g_test_add_func ("/path/intersect/coincide2", test_intersect_coincide2);
  g_test_add_func ("/path/intersect/coincide3", test_intersect_coincide3);
  g_test_add_func ("/path/intersect/coincide4", test_intersect_coincide4);
  g_test_add_func ("/path/intersect/coincide5", test_intersect_coincide5);
  g_test_add_func ("/path/intersect/coincide6", test_intersect_coincide6);
  g_test_add_func ("/path/intersect/circle", test_intersect_circle);
  g_test_add_func ("/path/intersect/circle2", test_intersect_circle2);
  g_test_add_func ("/path/intersect/circle3", test_intersect_circle3);
  g_test_add_func ("/path/intersect/circle4", test_intersect_circle4);
  g_test_add_func ("/path/intersect/circle5", test_intersect_circle5);
  g_test_add_func ("/path/intersect/circle6", test_intersect_circle6);
  g_test_add_func ("/path/intersect/circle7", test_intersect_circle7);
  g_test_add_func ("/path/intersect/circle-rounded-rect", test_intersect_circle_rounded_rect);
  g_test_add_func ("/path/intersect/loop-line", test_intersect_loop_line);
  g_test_add_func ("/path/intersect/mix-segment", test_intersect_mix_segment);
  g_test_add_func ("/path/self-intersect/loop", test_self_intersect_loop);
  g_test_add_func ("/path/self-intersect/lollipop", test_self_intersect_lollipop);

  return g_test_run ();
}
