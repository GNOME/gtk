#include <gtk/gtk.h>
#include "gsk/gskcurveprivate.h"

static void
test_line_line_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1, t2;
  graphene_point_t p;
  GskPathIntersection kind;
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 10, 0);
  graphene_point_init ((graphene_point_t *) &p1[1], 10, 100);
  graphene_point_init ((graphene_point_t *) &p2[0], 0, 10);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, &kind, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1, 0.1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2, 0.1, 0.0001);
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (10, 10), 0.0001));
  g_assert_cmpint (kind, ==, GSK_PATH_INTERSECTION_NORMAL);
}

static void
test_line_line_end_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1, t2;
  graphene_point_t p;
  GskPathIntersection kind;
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 10, 0);
  graphene_point_init ((graphene_point_t *) &p1[1], 10, 100);
  graphene_point_init ((graphene_point_t *) &p2[0], 10, 100);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, &kind, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1, 1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2, 0, 0.0001);
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (10, 100), 0.0001));
  g_assert_cmpint (kind, ==, GSK_PATH_INTERSECTION_NORMAL);
}

static void
test_line_line_none_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1, t2;
  graphene_point_t p;
  GskPathIntersection kind;
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p1[1], 10, 0);
  graphene_point_init ((graphene_point_t *) &p2[0], 20, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 30, 0);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, &kind, 1);

  g_assert_cmpint (n, ==, 0);

  graphene_point_init ((graphene_point_t *) &p1[0], 247.103424, 95.7965317);
  graphene_point_init ((graphene_point_t *) &p1[1], 205.463974, 266.758484);
  graphene_point_init ((graphene_point_t *) &p2[0], 183.735962, 355.968689);
  graphene_point_init ((graphene_point_t *) &p2[1], 121.553253, 611.27655);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, &kind, 1);

  g_assert_cmpint (n, ==, 0);
}

static void
test_line_line_parallel (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1[2], t2[2];
  graphene_point_t p[2];
  GskPathIntersection kind[2];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 10, 10);
  graphene_point_init ((graphene_point_t *) &p1[1], 110, 10);
  graphene_point_init ((graphene_point_t *) &p2[0], 20, 10);
  graphene_point_init ((graphene_point_t *) &p2[1], 120, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 2);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.1f, 0.01);
  g_assert_cmpfloat_with_epsilon (t1[1], 1.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.9f, 0.01);
  g_assert_cmpint (kind[0], ==, GSK_PATH_INTERSECTION_START);
  g_assert_cmpint (kind[1], ==, GSK_PATH_INTERSECTION_END);
}

static void
test_line_line_same (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1[2], t2[2];
  graphene_point_t p[2];
  GskPathIntersection kind[2];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 10, 10);
  graphene_point_init ((graphene_point_t *) &p1[1], 100, 10);
  graphene_point_init ((graphene_point_t *) &p2[0], 10, 10);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 2);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t1[1], 1.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[1], 1.f, 0.01);
  g_assert_cmpint (kind[0], ==, GSK_PATH_INTERSECTION_START);
  g_assert_cmpint (kind[1], ==, GSK_PATH_INTERSECTION_END);
}

static void
test_line_line_opposite (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1[2], t2[2];
  graphene_point_t p[2];
  GskPathIntersection kind[2];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 10, 10);
  graphene_point_init ((graphene_point_t *) &p1[1], 100, 10);
  graphene_point_init ((graphene_point_t *) &p2[0], 100, 10);
  graphene_point_init ((graphene_point_t *) &p2[1], 10, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 2);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t1[1], 1.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[0], 1.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.f, 0.01);
  g_assert_cmpint (kind[0], ==, GSK_PATH_INTERSECTION_START);
  g_assert_cmpint (kind[1], ==, GSK_PATH_INTERSECTION_END);
}

static void
test_line_line_opposite2 (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[2], p2[2];
  float t1[2], t2[2];
  graphene_point_t p[2];
  GskPathIntersection kind[2];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 100, 200);
  graphene_point_init ((graphene_point_t *) &p1[1], 100, 100);
  graphene_point_init ((graphene_point_t *) &p2[0], 100, 100);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 200);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 2);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t1[1], 1.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[0], 1.f, 0.01);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.f, 0.01);
  g_assert_cmpint (kind[0], ==, GSK_PATH_INTERSECTION_START);
  g_assert_cmpint (kind[1], ==, GSK_PATH_INTERSECTION_END);
}

static void
test_line_curve_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;
  GskBoundingBox b;

  graphene_point_init ((graphene_point_t *) &p1[0], 0, 100);
  graphene_point_init ((graphene_point_t *) &p1[1], 50, 100);
  graphene_point_init ((graphene_point_t *) &p1[2], 50, 0);
  graphene_point_init ((graphene_point_t *) &p1[3], 100, 0);
  graphene_point_init ((graphene_point_t *) &p2[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.5, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (50, 50), 0.0001));

  gsk_curve_get_tight_bounds (&c1, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);

  gsk_curve_get_tight_bounds (&c2, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);
}

static void
test_line_curve_multiple_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  graphene_point_t pp;
  int n;
  GskBoundingBox b1, b2;

  graphene_point_init ((graphene_point_t *) &p1[0], 100, 200);
  graphene_point_init ((graphene_point_t *) &p1[1], 350, 100);
  graphene_point_init ((graphene_point_t *) &p1[2], 100, 350);
  graphene_point_init ((graphene_point_t *) &p1[3], 400, 300);

  graphene_point_init ((graphene_point_t *) &p2[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 3);

  g_assert_cmpint (n, ==, 0);

  graphene_point_init ((graphene_point_t *) &p2[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 200, 200);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 3);

  g_assert_cmpint (n, ==, 1);

  g_assert_cmpfloat_with_epsilon (t1[0], 0.136196628, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.88487947, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (176.975891, 176.975891), 0.001));

  gsk_curve_get_point (&c1, t1[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_tight_bounds (&c1, &b1);
  gsk_curve_get_tight_bounds (&c2, &b2);

  gsk_bounding_box_contains_point (&b1, &p[0]);
  gsk_bounding_box_contains_point (&b2, &p[0]);

  graphene_point_init ((graphene_point_t *) &p2[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 280, 280);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 3);

  g_assert_cmpint (n, ==, 2);

  g_assert_cmpfloat_with_epsilon (t1[0], 0.136196628, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.632056773, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (176.975891, 176.975891), 0.001));

  gsk_curve_get_point (&c1, t1[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  g_assert_cmpfloat_with_epsilon (t1[1], 0.499999911, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.825892806, 0.0001);
  g_assert_true (graphene_point_near (&p[1], &GRAPHENE_POINT_INIT (231.25, 231.25), 0.001));

  gsk_curve_get_point (&c1, t1[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  gsk_curve_get_tight_bounds (&c1, &b1);
  gsk_curve_get_tight_bounds (&c2, &b2);

  gsk_bounding_box_contains_point (&b1, &p[0]);
  gsk_bounding_box_contains_point (&b1, &p[1]);
  gsk_bounding_box_contains_point (&b2, &p[0]);
  gsk_bounding_box_contains_point (&b2, &p[1]);

  graphene_point_init ((graphene_point_t *) &p2[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 1000, 1000);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 3);

  g_assert_cmpint (n, ==, 3);

  g_assert_cmpfloat_with_epsilon (t1[0], 0.863803446, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.305377066, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (305.377075, 305.377075), 0.001));

  gsk_curve_get_point (&c1, t1[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  g_assert_cmpfloat_with_epsilon (t1[1], 0.136196628, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.176975891, 0.0001);
  g_assert_true (graphene_point_near (&p[1], &GRAPHENE_POINT_INIT (176.975891, 176.975891), 0.001));

  gsk_curve_get_point (&c1, t1[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  g_assert_cmpfloat_with_epsilon (t1[2], 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[2], 0.231249988, 0.0001);
  g_assert_true (graphene_point_near (&p[2], &GRAPHENE_POINT_INIT (231.249985, 231.249985), 0.001));

  gsk_curve_get_point (&c1, t1[2], &pp);
  g_assert_true (graphene_point_near (&p[2], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[2], &pp);
  g_assert_true (graphene_point_near (&p[2], &pp, 0.001));

  gsk_curve_get_tight_bounds (&c1, &b1);
  gsk_curve_get_tight_bounds (&c2, &b2);

  gsk_bounding_box_contains_point (&b1, &p[0]);
  gsk_bounding_box_contains_point (&b1, &p[1]);
  gsk_bounding_box_contains_point (&b1, &p[2]);
  gsk_bounding_box_contains_point (&b2, &p[0]);
  gsk_bounding_box_contains_point (&b2, &p[1]);
  gsk_bounding_box_contains_point (&b2, &p[2]);
}

static void
test_line_curve_end_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 0, 100);
  graphene_point_init ((graphene_point_t *) &p1[1], 50, 100);
  graphene_point_init ((graphene_point_t *) &p1[2], 50, 0);
  graphene_point_init ((graphene_point_t *) &p1[3], 100, 0);
  graphene_point_init ((graphene_point_t *) &p2[0], 100, 0);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (100, 0), 0.0001));
}

static void
test_line_curve_none_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 333, 78);
  graphene_point_init ((graphene_point_t *) &p1[1], 415, 78);
  graphene_point_init ((graphene_point_t *) &p1[2], 463, 131);
  graphene_point_init ((graphene_point_t *) &p1[3], 463, 223);

  graphene_point_init ((graphene_point_t *) &p2[0], 520, 476);
  graphene_point_init ((graphene_point_t *) &p2[1], 502, 418);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 1);

  g_assert_cmpint (n, ==, 0);
}

static void
test_curve_curve_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;
  GskBoundingBox b;

  graphene_point_init ((graphene_point_t *) &p1[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p1[1], 33.333, 100);
  graphene_point_init ((graphene_point_t *) &p1[2], 66.667, 0);
  graphene_point_init ((graphene_point_t *) &p1[3], 100, 100);
  graphene_point_init ((graphene_point_t *) &p2[0], 0, 50);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 0);
  graphene_point_init ((graphene_point_t *) &p2[2], 20, 0); // weight 20
  graphene_point_init ((graphene_point_t *) &p2[3], 50, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CONIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 9);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat (t1[0], <, 0.5);
  g_assert_cmpfloat (t1[1], >, 0.5);
  g_assert_cmpfloat (t2[0], <, 0.5);
  g_assert_cmpfloat (t2[1], >, 0.5);

  gsk_curve_get_tight_bounds (&c1, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);

  gsk_curve_get_tight_bounds (&c2, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);
}

static void
test_curve_curve_end_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 0, 0);
  graphene_point_init ((graphene_point_t *) &p1[1], 33.333, 100);
  graphene_point_init ((graphene_point_t *) &p1[2], 66.667, 0);
  graphene_point_init ((graphene_point_t *) &p1[3], 100, 100);
  graphene_point_init ((graphene_point_t *) &p2[0], 100, 100);
  graphene_point_init ((graphene_point_t *) &p2[1], 100, 0);
  graphene_point_init ((graphene_point_t *) &p2[2], 20, 0);
  graphene_point_init ((graphene_point_t *) &p2[3], 10, 0);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CONIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 9);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0, 0.0001);
}

static void
test_curve_curve_end_intersection2 (void)
{
  GskCurve c, c1, c2;
  GskAlignedPoint p1[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 200, 100);
  graphene_point_init ((graphene_point_t *) &p1[1], 300, 300);
  graphene_point_init ((graphene_point_t *) &p1[2], 100, 300);
  graphene_point_init ((graphene_point_t *) &p1[3], 300, 100);

  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p1));

  gsk_curve_split (&c, 0.5, &c1, &c2);

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 9);

  g_assert_cmpint (n, ==, 2);
}

static void
test_curve_curve_max_intersection (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 106, 100);
  graphene_point_init ((graphene_point_t *) &p1[1], 118, 264);
  graphene_point_init ((graphene_point_t *) &p1[2], 129,   4);
  graphene_point_init ((graphene_point_t *) &p1[3], 128, 182);

  graphene_point_init ((graphene_point_t *) &p2[0],  54, 135);
  graphene_point_init ((graphene_point_t *) &p2[1], 263, 136);
  graphene_point_init ((graphene_point_t *) &p2[2],   2, 143);
  graphene_point_init ((graphene_point_t *) &p2[3], 141, 150);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CUBIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 9);

  g_assert_cmpint (n, ==, 9);
}

/* This showed up as artifacts in the stroker when our
 * intersection code failed to find intersections with
 * horizontal lines
 */
static void
test_curve_intersection_horizontal_line (void)
{
  GskCurve c1, c2;
  GskAlignedPoint p1[4];
  graphene_point_t p2[2];
  float t1, t2;
  graphene_point_t p;
  GskPathIntersection kind;
  int n;

  graphene_point_init ((graphene_point_t *) &p1[0], 200, 165);
  graphene_point_init ((graphene_point_t *) &p1[1], 220.858, 165);
  graphene_point_init ((graphene_point_t *) &p1[2], 1.4142, 0);
  graphene_point_init ((graphene_point_t *) &p1[3], 292.929, 92.929);


  graphene_point_init (&p2[0], 300, 110);
  graphene_point_init (&p2[1], 100, 110);

  gsk_curve_init (&c1,
                  gsk_pathop_encode (GSK_PATH_CONIC, p1));
  gsk_curve_init_foreach (&c2, GSK_PATH_LINE, p2, 2, 0);

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, &kind, 1);

  g_assert_true (n == 1);
}

static void
test_quad_overlap (void)
{
  graphene_point_t p1[3];
  GskCurve c1, c2;
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init (&p1[0], 0, 0);
  graphene_point_init (&p1[1], 100, 0);
  graphene_point_init (&p1[2], 100, 100);

  gsk_curve_init_foreach (&c1, GSK_PATH_QUAD, p1, 3, 0);
  gsk_curve_segment (&c1, 0.25, 0.75, &c2);

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 9);
  g_assert_cmpint (n, ==, 2);
  g_assert_cmpint (kind[0], ==, GSK_PATH_INTERSECTION_START);
  g_assert_cmpint (kind[1], ==, GSK_PATH_INTERSECTION_END);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.25, 0.001);
  g_assert_cmpfloat_with_epsilon (t1[1], 0.75, 0.001);
}

static void
test_cubic_overlap (void)
{
  graphene_point_t p1[4];
  GskCurve c1, c2;
  float t1[9], t2[9];
  graphene_point_t p[9];
  GskPathIntersection kind[9];
  int n;

  graphene_point_init (&p1[0], 0, 0);
  graphene_point_init (&p1[1], 100, 0);
  graphene_point_init (&p1[2], 0, 100);
  graphene_point_init (&p1[3], 100, 100);

  gsk_curve_init_foreach (&c1, GSK_PATH_CUBIC, p1, 4, 0);
  gsk_curve_segment (&c1, 0.25, 0.75, &c2);

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, kind, 9);
  g_assert_cmpint (n, ==, 2);
  g_assert_cmpint (kind[0], ==, GSK_PATH_INTERSECTION_START);
  g_assert_cmpint (kind[1], ==, GSK_PATH_INTERSECTION_END);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.25, 0.001);
  g_assert_cmpfloat_with_epsilon (t1[1], 0.75, 0.001);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/curve/intersection/line-line", test_line_line_intersection);
  g_test_add_func ("/curve/intersection/line-line-none", test_line_line_none_intersection);
  g_test_add_func ("/curve/intersection/line-line-end", test_line_line_end_intersection);
  g_test_add_func ("/curve/intersection/line-line-parallel", test_line_line_parallel);
  g_test_add_func ("/curve/intersection/line-line-same", test_line_line_same);
  g_test_add_func ("/curve/intersection/line-line-opposite", test_line_line_opposite);
  g_test_add_func ("/curve/intersection/line-line-opposite2", test_line_line_opposite2);
  g_test_add_func ("/curve/intersection/line-curve", test_line_curve_intersection);
  g_test_add_func ("/curve/intersection/line-curve-end", test_line_curve_end_intersection);
  g_test_add_func ("/curve/intersection/line-curve-none", test_line_curve_none_intersection);
  g_test_add_func ("/curve/intersection/line-curve-multiple", test_line_curve_multiple_intersection);
  g_test_add_func ("/curve/intersection/curve-curve", test_curve_curve_intersection);
  g_test_add_func ("/curve/intersection/curve-curve-end", test_curve_curve_end_intersection);
  g_test_add_func ("/curve/intersection/curve-curve-end2", test_curve_curve_end_intersection2);
  g_test_add_func ("/curve/intersection/curve-curve-max", test_curve_curve_max_intersection);
  g_test_add_func ("/curve/intersection/horizontal-line", test_curve_intersection_horizontal_line);
  g_test_add_func ("/curve/intersection/quad-overlap", test_quad_overlap);
  g_test_add_func ("/curve/intersection/cubic-overlap", test_cubic_overlap);

  return g_test_run ();
}
