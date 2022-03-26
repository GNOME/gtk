/*
 * Copyright © 2022 Red Hat, Inc.
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

#include "path-utils.h"

typedef enum
{
  OP_UNION,
  OP_INTERSECTION,
  OP_DIFFERENCE,
  OP_SYMMETRIC_DIFFERENCE,
} Op;

static void
test_ops_simple (void)
{
  struct {
    const char *in1;
    const char *in2;
    Op op;
    const char *out;
  } tests[] = {
    /* partially overlapping edge */
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 150 150 L 150 250 L 250 250 Z",
      OP_UNION,
      "M 100 100 L 100 200 L 150 200 L 150 250 L 250 250 L 200 200 L 150 150 L 100 100 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 150 150 L 150 250 L 250 250 Z",
      OP_INTERSECTION,
      "M 150 200 L 200 200 L 150 150 L 150 200 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 150 150 L 150 250 L 250 250 Z",
      OP_DIFFERENCE,
      "M 100 100 L 100 200 L 150 200 L 150 150 L 100 100 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 150 150 L 150 250 L 250 250 Z",
      OP_SYMMETRIC_DIFFERENCE,
      "M 100 100 L 100 200 L 150 200 L 150 150 L 100 100 Z M 200 200 L 150 200 L 150 250 "
      "L 250 250 L 200 200 Z" },
    /* two triangles in general position */
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 170 120 L 100 240 L 170 240 Z",
      OP_UNION,
      "M 100 100 L 100 200 L 123.33333587646484 200 L 100 240 L 170 240 L 170 200 L 200 200 "
      "L 170 170 L 170 120 L 151.57894897460938 151.57894897460938 L 100 100 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 170 120 L 100 240 L 170 240 Z",
      OP_INTERSECTION,
      "M 123.33333587646484 200 L 170 200 L 170 170 L 151.57894897460938 151.57894897460938 "
      "L 123.33332824707031 200 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 170 120 L 100 240 L 170 240 Z",
      OP_DIFFERENCE,
      "M 100 100 L 100 200 L 123.33333587646484 200 L 151.57894897460938 151.57894897460938 "
      "L 100 100 Z M 170 200 L 200 200 L 170 170 L 170 200 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 170 120 L 100 240 L 170 240 Z",
      OP_SYMMETRIC_DIFFERENCE,
      "M 100 100 L 100 200 L 123.33333587646484 200 L 151.57894897460938 151.57894897460938 "
      "L 100 100 Z M 170 200 L 123.33333587646484 200 L 100 240 L 170 240 L 170 200 Z "
      "M 170 200 L 200 200 L 170 170 L 170 200 Z M 151.57894897460938 151.57894897460938 "
      "L 170 170 L 170 120 L 151.57894897460938 151.57894897460938 Z" },
    /* nested contours, oriented in opposite direction */
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 170 190 L 120 190 Z",
      OP_UNION,
      "M 100 100 L 100 200 L 200 200 L 100 100 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 170 190 L 120 190 Z",
      OP_INTERSECTION,
      "M 170 190 L 120 140 L 120 190 L 170 190 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 170 190 L 120 190 Z",
      OP_DIFFERENCE,
      "M 100 100 L 100 200 L 200 200 L 100 100 Z M 120 140 L 170 190 L 120 190 L 120 140 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 170 190 L 120 190 Z",
      OP_SYMMETRIC_DIFFERENCE,
      "M 100 100 L 100 200 L 200 200 L 100 100 Z M 120 140 L 170 190 L 120 190 L 120 140 Z" },
    /* nested contours, oriented in opposite direction, other way around */
    { "M 100 100 L 200 200 L 100 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_UNION,
      "M 200 200 L 100 100 L 100 200 L 200 200 Z" },
    { "M 100 100 L 200 200 L 100 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_INTERSECTION,
      "M 120 140 L 120 190 L 170 190 L 120 140 Z" },
    { "M 100 100 L 200 200 L 100 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_DIFFERENCE,
      "M 200 200 L 100 100 L 100 200 L 200 200 Z M 120 190 L 120 140 L 170 190 L 120 190 Z" },
    { "M 100 100 L 200 200 L 100 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_SYMMETRIC_DIFFERENCE,
      "M 200 200 L 100 100 L 100 200 L 200 200 Z M 120 190 L 120 140 L 170 190 L 120 190 Z" },
    /* nested contours, oriented in the same direction */
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_UNION,
      "M 100 100 L 100 200 L 200 200 L 100 100 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_INTERSECTION,
      "M 120 140 L 120 190 L 170 190 L 120 140 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_DIFFERENCE,
      "M 100 100 L 100 200 L 200 200 L 100 100 Z M 120 190 L 120 140 L 170 190 L 120 190 Z" },
    { "M 100 100 L 100 200 L 200 200 Z",
      "M 120 140 L 120 190 L 170 190 Z",
      OP_SYMMETRIC_DIFFERENCE,
      "M 100 100 L 100 200 L 200 200 L 100 100 Z M 120 190 L 120 140 L 170 190 L 120 190 Z" },
    /* a 3-way intersection */
    { "M 100 200 L 150 104 L 145 104 L 200 200 Z",
      "M 100 108.571 L 200 108.571 L 200 50 L 100 50 Z",
      OP_UNION,
      "M 147.61904907226562 108.57142639160156 L 100 200 L 200 200 "
      "L 147.61904907226562 108.57142639160156 Z M 100 108.57099914550781 "
      "L 147.61927795410156 108.57099914550781 L 200 108.57099914550781 L 200 50 "
      "L 100 50 L 100 108.57099914550781 Z" },
    { "M 100 200 L 150 104 L 145 104 L 200 200 Z",
      "M 100 108.571 L 200 108.571 L 200 50 L 100 50 Z",
      OP_INTERSECTION,
      "M 147.61904907226562 108.57142639160156 L 150 104 L 145 104 "
      "L 147.61904907226562 108.57142639160156 Z" },
    { "M 100 200 L 150 104 L 145 104 L 200 200 Z",
      "M 100 108.571 L 200 108.571 L 200 50 L 100 50 Z",
      OP_DIFFERENCE,
      "M 147.61904907226562 108.57142639160156 L 100 200 L 200 200 "
      "L 147.61904907226562 108.57142639160156 Z" },
    { "M 100 200 L 150 104 L 145 104 L 200 200 Z",
      "M 100 108.571 L 200 108.571 L 200 50 L 100 50 Z",
      OP_SYMMETRIC_DIFFERENCE,
      "M 147.61904907226562 108.57142639160156 L 100 200 L 200 200 "
      "L 147.61904907226562 108.57142639160156 Z M 150 104 "
      "L 147.61904907226562 108.57142639160156 L 200 108.57099914550781 "
      "L 200 50 L 100 50 L 100 108.57099914550781 L 147.61927795410156 108.57099914550781 "
      "L 145 104 L 150 104 Z" },
     /* touching quadratics */
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 200 Q 150 100 200 200 Z",
       OP_UNION,
       "M 100 100 "
       "Q 124.987984 149.975967, 149.975967 149.999985 "
       "Q 174.987976 150.024033, 200 100 "
       "L 100 100 "
       "Z "
       "M 149.975967 150 "
       "Q 124.987984 150.024033, 100 200 "
       "L 200 200 "
       "Q 174.987976 149.975967, 149.975967 150.000015 "
       "Z" },
     /* overlapping quadratics, two intersections, different orientations */
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 Q 150 80 200 180 Z",
       OP_UNION,
       "M 100 100 "
       "Q 113.819313 127.638626, 127.638626 139.999374 "
       "Q 113.819695 152.360611, 100 180 "
       "L 200 180 "
       "Q 186.180313 152.360611, 172.360611 139.999939 "
       "Q 186.180298 127.639389, 200 100 "
       "L 100 100 "
       "Z" },
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 Q 150 80 200 180 Z",
       OP_INTERSECTION,
       "M 127.638626 139.99939 "
       "Q 149.999619 160.000275, 172.360611 140.000061 "
       "Q 150 120.000061, 127.639389 139.999939 "
       "Z" },
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 Q 150 80 200 180 Z",
       OP_DIFFERENCE,
       "M 100 100 "
       "Q 113.819313 127.638626, 127.638626 139.999374 "
       "Q 150 120.000061, 172.360611 139.999939 "
       "Q 186.180298 127.639389, 200 100 "
       "L 100 100 "
       "Z" },
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 Q 150 80 200 180 Z",
       OP_SYMMETRIC_DIFFERENCE,
       "M 100 100 "
       "Q 113.819313 127.638626, 127.638626 139.999374 "
       "Q 150 120.000061, 172.360611 139.999939 "
       "Q 186.180298 127.639389, 200 100 "
       "L 100 100 "
       "Z "
       "M 172.360611 140.000061 "
       "Q 149.999619 160.000275, 127.638626 139.999374 "
       "Q 113.819695 152.360611, 100 180 "
       "L 200 180 "
       "Q 186.180313 152.360611, 172.360611 139.999939 "
       "Z" },
     /* overlapping quadratics, two intersections, same orientation */
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 L 200 180 Q 150 80 100 180 Z",
       OP_UNION,
       "M 100 100 "
       "Q 113.819313 127.638626, 127.638626 139.999374 "
       "Q 113.819695 152.360611, 100 180 "
       "L 200 180 "
       "Q 186.180695 152.361374, 172.361389 140.000626 "
       "Q 186.180298 127.639389, 200 100 "
       "L 100 100 "
       "Z" },
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 L 200 180 Q 150 80 100 180 Z",
       OP_INTERSECTION,
       "M 127.638626 139.99939 "
       "Q 149.999619 160.000275, 172.360611 140.000061 "
       "Q 150.000397 119.999725, 127.639397 139.999939 "
       "Z" },
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 L 200 180 Q 150 80 100 180 Z",
       OP_DIFFERENCE,
       "M 100 100 "
       "Q 113.819313 127.638626, 127.638626 139.999374 "
       "Q 150.000397 119.999725, 172.361389 140.000626 "
       "Q 186.180298 127.639389, 200 100 "
       "L 100 100 "
       "Z" },
     { "M 100 100 Q 150 200 200 100 Z",
       "M 100 180 L 200 180 Q 150 80 100 180 Z",
       OP_SYMMETRIC_DIFFERENCE,
       "M 100 100 "
       "Q 113.819313 127.638626, 127.638626 139.999374 "
       "Q 150.000397 119.999725, 172.361389 140.000626 "
       "Q 186.180298 127.639389, 200 100 "
       "L 100 100 "
       "Z "
       "M 172.360611 140.000061 "
       "Q 149.999619 160.000275, 127.638626 139.999374 "
       "Q 113.819695 152.360611, 100 180 "
       "L 200 180 "
       "Q 186.180695 152.361374, 172.361389 140.000626 "
       "Z" },
     /* two polygons with near edges */
     { "M 100 100 L 100 200 L 400 200 L 400 100 Z",
       "M 150 103 L 250 100 L 300 103 L 250 180 Z",
       OP_UNION,
       "M 100 100 L 100 200 L 400 200 L 400 100 L 250 100 L 100 100 Z" },
     { "M 100 100 L 100 200 L 400 200 L 400 100 Z",
       "M 150 103 L 250 100 L 300 103 L 250 180 Z",
       OP_INTERSECTION,
      "M 250 100 L 150 103 L 250 180 L 300 103 L 250 100 Z" },
     { "M 100 100 L 100 200 L 400 200 L 400 100 Z",
       "M 150 103 L 250 100 L 300 103 L 250 180 Z",
       OP_DIFFERENCE,
       "M 100 100 L 100 200 L 400 200 L 400 100 L 250 100 L 300 103 L 250 180 L 150 103 L 250 100 L 100 100 Z" },
     { "M 100 100 L 100 200 L 400 200 L 400 100 Z",
       "M 150 103 L 250 100 L 300 103 L 250 180 Z",
       OP_SYMMETRIC_DIFFERENCE,
       "M 100 100 L 100 200 L 400 200 L 400 100 L 250 100 L 300 103 L 250 180 L 150 103 L 250 100 L 100 100 Z" },
     /* Collinear line segments */
     { "M 100 100 L 200 100 L 250 100 L 100 200 Z",
       "M 150 100 L 300 100 L 300 200 Z",
       OP_UNION,
       "M 150 100 "
       "L 100 100 "
       "L 100 200 "
       "L 200 133.333328 "
       "L 300 200 "
       "L 300 100 "
       "L 250 100 "
       "L 200 100 "
       "L 150 100 "
       "Z" },
     { "M 100 100 L 200 100 L 250 100 L 100 200 Z",
       "M 150 100 L 300 100 L 300 200 Z",
       OP_INTERSECTION,
       "M 200 100 "
       "L 150 100 "
       "L 200 133.333328 "
       "L 250 100 "
       "L 200 100 "
       "Z" },
     { "M 100 100 L 200 100 L 250 100 L 100 200 Z",
       "M 150 100 L 300 100 L 300 200 Z",
       OP_DIFFERENCE,
       "M 150 100 L 100 100 L 100 200 L 200 133.33332824707031 L 150 100 Z" },
     { "M 100 100 L 200 100 L 250 100 L 100 200 Z",
       "M 150 100 L 300 100 L 300 200 Z",
       OP_SYMMETRIC_DIFFERENCE,
       "M 150 100 L 100 100 L 100 200 L 200 133.33332824707031 L 150 100 Z "
       "M 250 100 L 200 133.33332824707031 L 300 200 L 300 100 L 250 100 Z" },
     /* a complicated union */
     { "M 175 100 L 175 400 L 300 400 L 300 100 z",
       "M 100 100 C 200 200 200 300 100 400 L 0 400 C 233.3333334 300 233.3333334 200 0 100 Z",
       OP_UNION,
       "M 175 100 "
       "L 175 250 "
       "L 175 400 "
       "L 300 400 "
       "L 300 100 "
       "L 175 100 "
       "Z "
       "M 175 250 "
       "Q 175 175, 100 100 "
       "L 0 100 "
       "Q 174.955811 174.981064, 174.999985 249.962112 "
       "Z "
       "M 100 400 "
       "Q 175 325, 175 250 "
       "Q 175.044189 324.981049, 0 400 "
       "L 100 400 "
       "Z" },
 };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GskPath *p1, *p2, *p3, *p;

      if (g_test_verbose ())
        {
          const char *opname[] = { "union", "intersection", "difference", "symmetric-difference" };
          g_test_message ("testcase %d op %s \"%s\" \"%s\"", i, opname[tests[i].op], tests[i].in1, tests[i].in2);
        }

      p1 = gsk_path_parse (tests[i].in1);
      p2 = gsk_path_parse (tests[i].in2);
      switch (tests[i].op)
        {
        case OP_UNION:
          p = gsk_path_union (p1, p2, GSK_FILL_RULE_WINDING);
          break;
        case OP_INTERSECTION:
          p = gsk_path_intersection (p1, p2, GSK_FILL_RULE_WINDING);
          break;
        case OP_DIFFERENCE:
          p = gsk_path_difference (p1, p2, GSK_FILL_RULE_WINDING);
          break;
        case OP_SYMMETRIC_DIFFERENCE:
          p = gsk_path_symmetric_difference (p1, p2, GSK_FILL_RULE_WINDING);
          break;
        default:
          g_assert_not_reached ();
        }

      g_assert_nonnull (p);
      p3 = gsk_path_parse (tests[i].out);
      assert_path_equal_with_epsilon (p, p3, 0.0001);

      gsk_path_unref (p);
      gsk_path_unref (p1);
      gsk_path_unref (p2);
      gsk_path_unref (p3);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/ops/simple", test_ops_simple);

  return g_test_run ();
}
