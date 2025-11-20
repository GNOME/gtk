/*
 * Copyright © 2025 Benjamin Otte
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

#include "config.h"

#include <gtk/gtk.h>
#include <gsk/gskrectprivate.h>

#if 0
static void
gsk_rect_init_random (graphene_rect_t *rect,
                      float            min,
                      float            max)
{
  float x1, x2, y1, y2;

  x1 = g_test_rand_double_range (min, max);
  do {
    x2 = g_test_rand_double_range (min, max);
  } while (x1 == x2);
  y1 = g_test_rand_double_range (min, max);
  do {
    y2 = g_test_rand_double_range (min, max);
  } while (y1 == y2);

  *rect = GRAPHENE_RECT_INIT (MIN (x1, x2),
                              MIN (y1, y2),
                              MAX (x2, x1) - MIN (x2, x1),
                              MAX (y2, y1) - MIN (y2, y1));
}
#endif

static void
gsk_rect_init_random_int (graphene_rect_t *rect,
                          int              min,
                          int              max)
{
  float x1, x2, y1, y2;

  x1 = g_test_rand_int_range (min, max);
  do {
    x2 = g_test_rand_int_range (min, max);
  } while (x1 == x2);
  y1 = g_test_rand_int_range (min, max);
  do {
    y2 = g_test_rand_int_range (min, max);
  } while (y1 == y2);

  *rect = GRAPHENE_RECT_INIT (MIN (x1, x2),
                              MIN (y1, y2),
                              MAX (x2, x1) - MIN (x2, x1),
                              MAX (y2, y1) - MIN (y2, y1));
}

#define N_RUNS 100

static void
test_subtract (void)
{
  graphene_rect_t m, s, res;
  gsize i;

  for (i = 0; i < N_RUNS; i++)
    {
      gsk_rect_init_random_int (&m, -1000, 1000);
      gsk_rect_init_random_int (&s, -1000, 1000);

      if (gsk_rect_subtract (&m, &s, &res))
        {
          g_assert_true (gsk_rect_contains_rect (&m, &res));
          g_assert_false (gsk_rect_intersects (&s, &res));
        }
      else
        {
          g_assert_true (gsk_rect_contains_rect (&s, &m));
        }

      
    }
}

static gboolean
my_rect_subtract (const graphene_rect_t *m,
                  const graphene_rect_t *s,
                  graphene_rect_t       *res)
{
  float x[4], y[4];
  guint xi, yi, xj, yj;
  graphene_rect_t tmp, best;
  float best_size = 0;

  x[0] = m->origin.x;
  x[1] = m->origin.x + m->size.width;
  x[2] = s->origin.x;
  x[3] = s->origin.x + s->size.width;
  y[0] = m->origin.y;
  y[1] = m->origin.y + m->size.height;
  y[2] = s->origin.y;
  y[3] = s->origin.y + s->size.height;

  for (yi = 0; yi < 4; yi++)
    for (yj = yi + 1; yj < 4; yj++)
      for (xi = 0; xi < 4; xi++)
        for (xj = xi + 1; xj < 4; xj++)
          {
            tmp = GRAPHENE_RECT_INIT (MIN (x[xi], x[xj]),
                                      MIN (y[yi], y[yj]),
                                      ABS (x[xi] - x[xj]),
                                      ABS (y[yi] - y[yj]));
            if (!gsk_rect_contains_rect (m, &tmp) ||
                gsk_rect_intersects (s, &tmp))
              continue;

            if (tmp.size.width * tmp.size.height > best_size)
              {
                best = tmp;
                best_size = tmp.size.width * tmp.size.height;
              }
          }

  if (best_size <= 0)
    return FALSE;

  *res = best;
  return TRUE;
}

static void
test_my_subtract (void)
{
  graphene_rect_t m, s, res = GRAPHENE_RECT_INIT (0, 0, 0, 0), my_res = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  gboolean found, my_found;
  gsize i;

  for (i = 0; i < N_RUNS; i++)
    {
      gsk_rect_init_random_int (&m, -1000, 1000);
      gsk_rect_init_random_int (&s, -1000, 1000);

      found = gsk_rect_subtract (&m, &s, &res);
      my_found = my_rect_subtract (&m, &s, &my_res);

      g_assert_cmpint (found, ==, my_found);

      if (found)
        {
          g_assert_cmpfloat_with_epsilon (res.size.width * res.size.height,
                                          my_res.size.width * my_res.size.height,
                                          0.001);
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/rect/subtract", test_subtract);
  g_test_add_func ("/rect/my_subtract", test_my_subtract);

  return g_test_run ();
}
