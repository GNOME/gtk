/*
 * Copyright © 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include <gtk/gtk.h>
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

static gboolean
add_segment (GskPathOperation        op,
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
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder,
                                pts[1].x, pts[1].y,
                                pts[2].x, pts[2].y);
      break;
    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 weight);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static GskPath *
convert_to_standard_contour (GskPath *path)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();
  gsk_path_foreach (path, -1, add_segment, builder);
  return gsk_path_builder_free_to_path (builder);
}

static void
test_circle_winding (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path1, *path2;
  const GskContour *contour, *contour1, *contour2;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 33);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  path1 = convert_to_standard_contour (path);
  contour1 = gsk_path_get_contour (path1, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path);
  path2 = gsk_path_builder_free_to_path (builder);
  contour2 = gsk_path_get_contour (path2, 0);

  g_assert_true (gsk_contour_get_winding (contour, &GRAPHENE_POINT_INIT (100, 100)) == 1);
  g_assert_true (gsk_contour_get_winding (contour1, &GRAPHENE_POINT_INIT (100, 100)) == 1);
  g_assert_true (gsk_contour_get_winding (contour2, &GRAPHENE_POINT_INIT (100, 100)) == -1);

  gsk_path_unref (path2);
  gsk_path_unref (path1);
  gsk_path_unref (path);
}

static void
test_rounded_rect_winding (void)
{
  GskRoundedRect rr;
  GskPathBuilder *builder;
  GskPath *path, *path1, *path2;
  const GskContour *contour, *contour1, *contour2;

  rr.bounds = GRAPHENE_RECT_INIT (100, 100, 200, 150);
  rr.corner[GSK_CORNER_TOP_LEFT] = GRAPHENE_SIZE_INIT (10, 10);
  rr.corner[GSK_CORNER_TOP_RIGHT] = GRAPHENE_SIZE_INIT (20, 10);
  rr.corner[GSK_CORNER_BOTTOM_RIGHT] = GRAPHENE_SIZE_INIT (20, 0);
  rr.corner[GSK_CORNER_BOTTOM_LEFT] = GRAPHENE_SIZE_INIT (0, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, &rr);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  path1 = convert_to_standard_contour (path);
  contour1 = gsk_path_get_contour (path1, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path);
  path2 = gsk_path_builder_free_to_path (builder);
  contour2 = gsk_path_get_contour (path2, 0);

  g_assert_true (gsk_contour_get_winding (contour, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour1, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour2, &GRAPHENE_POINT_INIT (150, 150)) == -1);

  gsk_path_unref (path2);
  gsk_path_unref (path1);
}

static void
test_rect_winding (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path1, *path2, *path3;
  const GskContour *contour, *contour1, *contour2, *contour3;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (100, 100, 200, 150));
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  path1 = convert_to_standard_contour (path);
  contour1 = gsk_path_get_contour (path1, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path);
  path2 = gsk_path_builder_free_to_path (builder);
  contour2 = gsk_path_get_contour (path2, 0);

  path3 = convert_to_standard_contour (path2);
  contour3 = gsk_path_get_contour (path3, 0);

  g_assert_true (gsk_contour_get_winding (contour, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour1, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour2, &GRAPHENE_POINT_INIT (150, 150)) == -1);
  g_assert_true (gsk_contour_get_winding (contour3, &GRAPHENE_POINT_INIT (150, 150)) == -1);

  gsk_path_unref (path3);
  gsk_path_unref (path2);
  gsk_path_unref (path1);
}

static void
test_zero_length (void)
{
  GskPath *path;
  const GskContour *contour;
  GskStroke *stroke;
  graphene_rect_t bounds;
  gboolean ret;

  stroke = gsk_stroke_new (10);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);

  path = gsk_path_parse ("M10,10z");
  g_assert_true (gsk_path_get_n_contours (path) == 1);
  contour = gsk_path_get_contour (path, 0);
  g_assert_true (gsk_contour_get_flags (contour) & GSK_PATH_ZERO_LENGTH);
  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);
  g_assert_true (bounds.size.width >= 10 && bounds.size.height >= 10);
  gsk_path_unref (path);

  path = gsk_path_parse ("M10,10L10,10");
  g_assert_true (gsk_path_get_n_contours (path) == 1);
  contour = gsk_path_get_contour (path, 0);
  g_assert_true (gsk_contour_get_flags (contour) & GSK_PATH_ZERO_LENGTH);
  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);
  g_assert_true (bounds.size.width >= 10 && bounds.size.height >= 10);
  gsk_path_unref (path);

  path = gsk_path_parse ("M10,10L12,12");
  g_assert_true (gsk_path_get_n_contours (path) == 1);
  contour = gsk_path_get_contour (path, 0);
  g_assert_false (gsk_contour_get_flags (contour) & GSK_PATH_ZERO_LENGTH);
  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);
  g_assert_true (bounds.size.width >= 10 && bounds.size.height >= 10);
  gsk_path_unref (path);

  gsk_stroke_free (stroke);
}

typedef struct
{
  const char *path_str;
  GskPathClassification expected;
} ClassifyTestData;

static void
classify_test_data_free (gpointer data)
{
  ClassifyTestData *d = data;
  g_free ((char *) d->path_str);
  g_free (d);
}

static void
classify_test_path (gconstpointer data)
{
  const ClassifyTestData *d = data;
  GskPath *path;
  GskPathClassification result;
  GskRoundedRect rect;
  const char *kind[] = {
    [GSK_PATH_EMPTY] = "empty",
    [GSK_PATH_RECT] = "rect",
    [GSK_PATH_ROUNDED_RECT] = "rounded",
    [GSK_PATH_CIRCLE] = "circle",
    [GSK_PATH_APPROXIMATE_ROUNDED_RECT] = "approx rounded",
    [GSK_PATH_APPROXIMATE_CIRCLE] = "approx circle",
    [GSK_PATH_GENERAL] = "general"
  };

  if (g_test_verbose ())
    g_test_message ("Classify %s", d->path_str);

  path = gsk_path_parse (d->path_str);

  result = gsk_path_classify (path, &rect);
  if (result != d->expected)
    g_test_fail_printf ("Expected '%s', got '%s'", kind[d->expected], kind[result]);

  gsk_path_unref (path);
}

static void
add_classify_test (int                    pos,
                   const char            *path_str,
                   GskPathClassification  c)
{
  ClassifyTestData *data;
  char *test_name;

  data = g_new (ClassifyTestData, 1);
  data->path_str = g_strdup (path_str);
  data->expected = c;

  test_name = g_strdup_printf ("/path/classify/%d", pos);
  g_test_add_data_func_full (test_name, data, classify_test_path, classify_test_data_free);
  g_free (test_name);
}

static void
add_classify_path_test (int                    pos,
                        GskPath               *path,
                        GskPathClassification  c)
{
  char *path_str;

  path_str = gsk_path_to_string (path);
  add_classify_test (pos, path_str, c);
  g_free (path_str);
}

static void
add_classify_rounded_rect_test (int                    pos,
                                GskRoundedRect        *rr,
                                GskPathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, rr);
  path = gsk_path_builder_free_to_path (builder);
  add_classify_path_test (pos, path, c);
  gsk_path_unref (path);
}

static void
add_classify_rect_test (int                    pos,
                        graphene_rect_t       *rect,
                        GskPathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, rect);
  path = gsk_path_builder_free_to_path (builder);
  add_classify_path_test (pos, path, c);
  gsk_path_unref (path);
}

static void
add_classify_circle_test (int                    pos,
                          graphene_rect_t       *rect,
                          GskPathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_point_t center;

  builder = gsk_path_builder_new ();
  graphene_rect_get_center (rect, &center);
  gsk_path_builder_add_circle (builder, &center, rect->size.width / 2);
  path = gsk_path_builder_free_to_path (builder);
  add_classify_path_test (pos, path, c);
  gsk_path_unref (path);
}

#define RR(x,y,w,h,r) \
  (GskRoundedRect) { \
  .bounds = { \
    .origin = { x, y, }, \
    .size = { w, h, }, \
  }, \
  .corner[0] = { r, r, }, \
  .corner[1] = { r, r, }, \
  .corner[2] = { r, r, }, \
  .corner[3] = { r, r, }, \
}

static ClassifyTestData tests[] = {
  { "", GSK_PATH_EMPTY },
  { "M10,10h20M20,20", GSK_PATH_GENERAL },
  /* the following are from org.gnome.Loupe.svg */
  { "m 2 2 h 124 v 124 h -124 z m 0 0", GSK_PATH_RECT },
  { "M 106 20 L 22 20 C 20.8945312 20, 20 20.8945312, 20 22 L 20 86 C 20 87.1054688, 20.8945312 88, 22 88 L 106 88 C 107.105469 88, 108 87.1054688, 108 86 L 108 22 C 108 20.8945312, 107.105469 20, 106 20 Z M 106 20", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 0 66 L 98 66 L 98 152 L 0 152 Z M 0 66", GSK_PATH_RECT },
  { "M 0 0 h 192 v 152 h -192 z", GSK_PATH_RECT },
  { "M 93 67"
    "C 87.4765625 67"
    "  83 62.5234375"
    "  83 57"
    "C 83 51.4765625"
    "  87.4765625 47"
    "  93 47"
    "C 98.5234375 47"
    "  103 51.4765625"
    "  103 57"
    "C 103 62.5234375"
    "  98.5234375 67"
    "  93 67"
    "Z"
    "M 94 67", GSK_PATH_APPROXIMATE_CIRCLE },
  { "M 52 44.125 C 35.4648438 44.125, 22.0625 57.5273438, 22.0625 74.0625 C 22.0625 90.5976562, 35.4648438 104, 52 104 C 68.5351562 104, 81.9375 90.5976562, 81.9375 74.0625 C 81.9375 57.5273438, 68.5351562 44.125, 52 44.125 Z M 52 44.125", GSK_PATH_APPROXIMATE_CIRCLE },
  /* from Adwaitas x-package-repository.svg */
  { "M 64.9921875 74"
    "L 67 74"
    "C 67.5507812 74"
    "  68 74.4492188"
    "  68 75"
    "C 68 75.5507812"
    "  67.5507812 76"
    "  67 76"
    "L 64.9921875 76"
    "C 64.4414062 76"
    "  63.9921875 75.5507812"
    "  63.9921875 75"
    "C 63.9921875 74.4492188"
    "  64.4414062 74"
    "  64.9921875 74"
    "Z"
    "M 64.9921875 74", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 58.9453125 44 L 69.0546875 44 C 70.6875 44, 72 45.3125, 72 46.9453125 L 72 59.0546875 C 72 60.6875, 70.6875 62, 69.0546875 62 L 58.9453125 62 C 57.3125 62, 56 60.6875, 56 59.0546875 L 56 46.9453125 C 56 45.3125, 57.3125 44, 58.9453125 44 Z M 58.9453125 44", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  /* from panel-right-symbolic.svg */
  { "m 104 502 c 0 0.550781 -0.449219 1 -1 1 s -1 -0.449219 -1 -1 s 0.449219 -1 1 -1 s 1 0.449219 1 1 z m 0 0", GSK_PATH_APPROXIMATE_CIRCLE },

  /* from network-wireless-signal-weak-symbolic.svg
   * This is approximating a circle with 5 cubics - too weird to bother with
   */
  { "M 8 10 C 7.48828077 10, 6.97656202 10.1953115, 6.58593798 10.5859385 C 5.80468798 11.3671885, 5.80468798 12.6328115, 6.58593798 13.4140615 C 7.36718798 14.1953115, 8.63281155 14.1953115, 9.41406155 13.4140615 C 10.1953115 12.6328115, 10.1953115 11.3671885, 9.41406155 10.5859385 C 9.02343845 10.1953115, 8.51171875 10, 8 10 Z  M 8 10", GSK_PATH_GENERAL },
 /* from org.gnome.SystemMonitor.Devel.svg */
 { "M 113 62 C 113 61.4500008, 113.449997 61, 114 61 C 114.550003 61, 115 61.4500008, 115 62 C 115 62.5499992, 114.550003 63, 114 63 C 113.449997 63, 113 62.5499992, 113 62 Z M 113 62", GSK_PATH_APPROXIMATE_CIRCLE },
  /* not from anywhere */
  { "M 69.0546875 44 C 70.6875 44, 72 45.3125, 72 46.9453125 L 72 59.0546875 C 72 60.6875, 70.6875 62, 69.0546875 62 L 58.9453125 62 C 57.3125 62, 56 60.6875, 56 59.0546875 L 56 46.9453125 C 56 45.3125, 57.3125 44, 58.9453125 44 L 58.9453125 44 Z M 58.9453125 44", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 23.078125 17 L 61.3671875 17 C 62.8007812 17, 63.9609375 18.1601562, 63.9609375 19.5898438 L 63.9609375 56.4101562 C 63.9609375 57.8398438, 62.8007812 59, 61.3671875 59 L 23.078125 59 C 21.6445312 59, 20.484375 57.8398438, 20.484375 56.4101562 L 20.484375 19.5898438 C 20.484375 18.1601562, 21.6445312 17, 23.078125 17 Z M 23.078125 17", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 66.8554688 42.4921875"
    "L 109.113281 42.4921875"
    "C 113.972656 42.4921875"
    "  117.914062 46.4335938"
    "  117.914062 51.296875"
    "L 117.914062 81.2578125"
    "C 117.914062 86.1210938"
    "  113.972656 90.0625"
    "  109.113281 90.0625"
    "L 66.8554688 90.0625"
    "C 61.9921875 90.0625"
    "  58.0507812 86.1210938"
    "  58.0507812 81.2578125"
    "L 58.0507812 51.296875"
    "C 58.0507812 46.4335938"
    "  61.9921875 42.4921875"
    "  66.8554688 42.4921875"
    "Z"
    "M 66.8554688 42.4921875", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 87.984375 8.0625"
    "C 104.515625 8.0625"
    "  117.914062 21.4648438"
    "  117.914062 37.9960938"
    "L 117.914062 60.1289062"
    "C 117.914062 76.6601562"
    "  104.515625 90.0625"
    "  87.984375 90.0625"
    "C 71.453125 90.0625"
    "  58.0507812 76.6601562"
    "  58.0507812 60.1289062"
    "L 58.0507812 37.9960938"
    "C 58.0507812 21.4648438"
    "  71.453125 8.0625"
    "  87.984375 8.0625"
    "Z"
    "M 87.984375 8.0625", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 72.359375 111.140625"
    "L 103.519531 111.140625"
    "C 105.960938 111.140625"
    "  107.9375 113.121094"
    "  107.9375 115.5625"
    "C 107.9375 118.003906"
    "  105.960938 119.984375"
    "  103.519531 119.984375"
    "L 72.359375 119.984375"
    "C 69.9179688 119.984375"
    "  67.9375 118.003906"
    "  67.9375 115.562"
    "C 67.9375 113.121094"
    "  69.9179688 111.140625"
    "  72.359375 111.140625"
    "  Z"
    "M 72.359375 111.140625", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 20 12"
    "L 108 12"
    "C 112.417969 12"
    "  116 15.7304688"
    "  116 20.3320312"
    "L 116 103.667969"
    "C 116 108.269531"
    "  112.417969 112"
    "  108 112"
    "L 20 112"
    "C 15.5820312 112"
    "  12 108.269531"
    "  12 103.667969"
    "L 12 20.3320312"
    "C 12 15.7304688"
    "  15.5820312 12"
    "  20 12"
    "Z"
    "M 20 12", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 20 12"
    "C 15.5820312 12"
    "  12 15.7304688"
    "  12 20.3320312"
    "L 12 103.667969"
    "C 12 108.269531"
    "  15.5820312 112"
    "  20 112"
    "L 108 112"
    "C 112.417969 112"
    "  116 108.269531"
    "  116 103.667969"
    "L 116 20.3320312"
    "C 116 15.7304688"
    "  112.417969 12"
    "  108 12"
    "L 20 12"
    "Z", GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 12 -94.03125 C 7.0283670425415039 -94.03125, 2.96875 -89.971633911132812, 2.96875 -85 L 2.96875 -14 C 2.96875 -9.0283670425415039, 7.0283670425415039 -4.96875, 12 -4.96875 L 84 -4.96875 C 88.971633911132812 -4.96875, 93.03125 -9.0283670425415039, 93.03125 -14 L 93.03125 -85 C 93.03125 -89.971633911132812, 88.971633911132812 -94.03125, 84 -94.03125 L 12 -94.03125 Z",
    GSK_PATH_APPROXIMATE_ROUNDED_RECT },
  { "M 12 -95.03125\
             C 6.4889097213745117 -95.03125, 1.96875 -90.511093139648438, 1.96875 -85\
             L 1.96875 -14\
             C 1.96875 -8.4889097213745117, 6.4889097213745117 -3.96875, 12 -3.96875\
             L 84 -3.96875\
             C 89.511093139648438 -3.96875, 94.03125 -8.4889097213745117, 94.03125 -14\
             L 94.03125 -85\
             C 94.03125 -90.511093139648438, 89.511093139648438 -95.03125, 84 -95.03125\
             L 12 -95.03125 Z",
    GSK_PATH_APPROXIMATE_ROUNDED_RECT },
};

static void
add_classify_tests (void)
{
  GskRoundedRect r;

  unsigned int i = 1;

  for (i = 1; i <= G_N_ELEMENTS (tests); i++)
    add_classify_test (i, tests[i - 1].path_str, tests[i - 1].expected);

  add_classify_rect_test (i++, &GRAPHENE_RECT_INIT (0, 0, 10, 10), GSK_PATH_RECT);
  add_classify_circle_test (i++, &GRAPHENE_RECT_INIT (0, 0, 10, 10), GSK_PATH_CIRCLE);
  add_classify_circle_test (i++, &GRAPHENE_RECT_INIT (10, 20, 30, 30), GSK_PATH_CIRCLE);
  add_classify_rounded_rect_test (i++, &RR (0, 0, 10, 10, 1), GSK_PATH_ROUNDED_RECT);
  add_classify_rounded_rect_test (i++, &RR (0, 0, 10, 10, 0), GSK_PATH_GENERAL);

  r = GSK_ROUNDED_RECT_INIT (0, 0, 10, 10);
  r.corner[0].width = r.corner[1].height = 1;

  add_classify_rounded_rect_test (i++, &r, GSK_PATH_GENERAL);
}

static void
test_angles (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  GskRoundedRect rect;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (5, 5), 5);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_CLOSED | GSK_PATH_OBTUSE | GSK_PATH_SMOOTH));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (5, 5), 0);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_CLOSED | GSK_PATH_ZERO_LENGTH | GSK_PATH_OBTUSE | GSK_PATH_SMOOTH));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 0, 10, 10));
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_CLOSED | GSK_PATH_FLAT | GSK_PATH_OBTUSE));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, &GSK_ROUNDED_RECT_INIT (0, 0, 10, 10));
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_CLOSED | GSK_PATH_OBTUSE));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_rounded_rect_init_from_rect (&rect, &GRAPHENE_RECT_INIT (0, 0, 10, 10), 5);
  gsk_path_builder_add_rounded_rect (builder, &rect);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_CLOSED | GSK_PATH_OBTUSE | GSK_PATH_SMOOTH));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_rounded_rect_init (&rect, &GRAPHENE_RECT_INIT (0, 0, 10, 10),
                                &GRAPHENE_SIZE_INIT (5, 5),
                                &GRAPHENE_SIZE_INIT (0, 0),
                                &GRAPHENE_SIZE_INIT (5, 5),
                                &GRAPHENE_SIZE_INIT (5, 5));
  gsk_path_builder_add_rounded_rect (builder, &rect);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_CLOSED | GSK_PATH_OBTUSE));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 5, 5);
  gsk_path_builder_line_to (builder, 10, 10);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_FLAT | GSK_PATH_OBTUSE | GSK_PATH_SMOOTH));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 5, 5);
  gsk_path_builder_line_to (builder, 10, 10);
  gsk_path_builder_line_to (builder, 20, 20);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_FLAT | GSK_PATH_OBTUSE | GSK_PATH_SMOOTH));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 5, 5);
  gsk_path_builder_line_to (builder, 10, 5);
  gsk_path_builder_line_to (builder, 20, 20);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_FLAT | GSK_PATH_OBTUSE));
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 5, 5);
  gsk_path_builder_line_to (builder, 10, 5);
  gsk_path_builder_line_to (builder, 5, 10);
  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == GSK_PATH_FLAT);
  gsk_path_unref (path);

  /* obtuse until we close */
  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 0, 0);
  gsk_path_builder_line_to (builder, 10, 0);
  gsk_path_builder_line_to (builder, 0, 10);
  gsk_path_builder_line_to (builder, 5, 10);
  gsk_path_builder_line_to (builder, 5, 1);
  gsk_path_builder_close (builder);

  path = gsk_path_builder_free_to_path (builder);
  g_assert_true (gsk_path_get_flags (path) == (GSK_PATH_FLAT | GSK_PATH_CLOSED));
  gsk_path_unref (path);

  for (unsigned int i = 0; i < 360; i++)
    {
      GskTransform *t = gsk_transform_rotate (NULL, i);
      graphene_point_t p;

      builder = gsk_path_builder_new ();

      gsk_transform_transform_point (t, &GRAPHENE_POINT_INIT (0, 0), &p);
      gsk_path_builder_move_to (builder, p.x, p.y);
      gsk_transform_transform_point (t, &GRAPHENE_POINT_INIT (10, 0), &p);
      gsk_path_builder_line_to (builder, p.x, p.y);
      gsk_transform_transform_point (t, &GRAPHENE_POINT_INIT (10, 10), &p);
      gsk_path_builder_line_to (builder, p.x, p.y);
      gsk_transform_transform_point (t, &GRAPHENE_POINT_INIT (0, 10), &p);
      gsk_path_builder_line_to (builder, p.x, p.y);
      gsk_path_builder_close (builder);

      path = gsk_path_builder_free_to_path (builder);
      g_assert_true ((gsk_path_get_flags (path) & GSK_PATH_OBTUSE) != 0);
      gsk_path_unref (path);

      gsk_transform_unref (t);
    }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/circle/winding", test_circle_winding);
  g_test_add_func ("/path/rounded-rect/winding", test_rounded_rect_winding);
  g_test_add_func ("/path/rect/winding", test_rect_winding);
  g_test_add_func ("/path/zero-length", test_zero_length);
  g_test_add_func ("/path/angles", test_angles);

  add_classify_tests ();

  return g_test_run ();
}
