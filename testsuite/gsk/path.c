/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <gtk/gtk.h>

#include "path-utils.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskboundingboxprivate.h"

static void
test_distance (void)
{
  GskPath *path;
  GskPathMeasure *measure;
  GskPathPoint point = { .contour = 0, .idx = 1, .t = 1 };
  float distance;

  path = gsk_path_parse ("M 20 6.66666677e+30 C 20 60, 50 90, 100 60 C 150 30, 180 30, 180 60");
  measure = gsk_path_measure_new (path);

  distance = gsk_path_point_get_distance (&point, measure);

  /* We just need to get here */
  if (g_test_verbose ())
    g_test_message ("distance is %f", distance);

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_circle_bounds (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_rect_t bounds;
  GskStroke *stroke;
  gboolean ret;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (10, 10), 5);
  path = gsk_path_builder_free_to_path (builder);

  ret = gsk_path_get_bounds (path, &bounds);
  g_assert_true (ret);
  g_assert_true (graphene_rect_equal (&bounds, &GRAPHENE_RECT_INIT (5, 5, 10, 10)));

  stroke = gsk_stroke_new (2);
  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);
  g_assert_true (graphene_rect_equal (&bounds, &GRAPHENE_RECT_INIT (4, 4, 12, 12)));
  gsk_stroke_free (stroke);

  gsk_path_unref (path);
}

static void
test_rect_bounds (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_rect_t rect = GRAPHENE_RECT_INIT (10, 10, 20, 30);
  float line_width = 2;
  graphene_rect_t bounds;
  GskStroke *stroke;
  gboolean ret;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &rect);
  path = gsk_path_builder_free_to_path (builder);

  ret = gsk_path_get_bounds (path, &bounds);
  g_assert_true (ret);
  g_assert_true (graphene_rect_equal (&bounds, &rect));

  stroke = gsk_stroke_new (line_width);

  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);

  graphene_rect_inset (&rect, - line_width / 2, - line_width / 2);
  g_assert_true (graphene_rect_contains_rect (&bounds, &rect));

  gsk_stroke_free (stroke);
  gsk_path_unref (path);
}

static void
get_offset_point (const graphene_point_t *p,
                  float                   angle,
                  float                   r,
                  graphene_point_t       *q)
{
  q->x = p->x + r * cos (angle);
  q->y = p->y + r * sin (angle);
}

static void
test_general_bounds (void)
{
  float line_width = 2;
  GskStroke *stroke;

  stroke = gsk_stroke_new (line_width);

  for (unsigned int i = 0; i < 100; i++)
    {
      GskPath *path;
      GskPathPoint point;
      GskBoundingBox bounds;
      GskBoundingBox stroke_bounds;
      graphene_point_t pos, pos2;

      path = create_random_path (G_MAXUINT);

      if (!gsk_path_get_start_point (path, &point))
        {
          gsk_path_unref (path);
          continue;
        }

      gsk_path_get_bounding_box (path, &bounds);
      gsk_path_get_stroke_bounding_box (path, stroke, &stroke_bounds);

      do {
        gsk_path_point_get_position (&point, path, &pos);
        g_assert_true (gsk_bounding_box_contains_point (&bounds, &pos));

        for (unsigned int j = 0; j < 8; j++)
          {
            float angle = j * 2 * M_PI / 8;

            get_offset_point (&pos, angle, line_width / 2, &pos2);
            g_assert_true (gsk_bounding_box_contains_point (&stroke_bounds, &pos2));
          }
      } while (gsk_path_get_next (path, &point));

      gsk_path_unref (path);
    }

  gsk_stroke_free (stroke);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/measure/distance", test_distance);
  g_test_add_func ("/path/bounds/circle", test_circle_bounds);
  g_test_add_func ("/path/bounds/rect", test_rect_bounds);
  g_test_add_func ("/path/bounds/general", test_general_bounds);

  return g_test_run ();
}
