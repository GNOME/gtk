/* Copyright (C) 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>
#include <gtk/gtkcolorutilsprivate.h>

static void
test_roundtrips_rgb_hsv (void)
{
  struct {
    float r, g, b;
    float h, s, v;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 0, 0, 1 },
    { 1, 0, 0, 0, 1, 1 },
    { 1, 1, 0, 1.0 / 6.0, 1, 1 },
    { 0, 1, 0, 2.0 / 6.0, 1, 1 },
    { 0, 1, 1, 3.0 / 6.0, 1, 1 },
    { 0, 0, 1, 4.0 / 6.0, 1, 1 },
    { 1, 0, 1, 5.0 / 6.0, 1, 1 },
  };
  const float EPSILON = 1e-6;

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float r, g, b;
      float h, s, v;

      gtk_hsv_to_rgb (tests[i].h, tests[i].s, tests[i].v, &r, &g, &b);
      g_assert_cmpfloat_with_epsilon (r, tests[i].r, EPSILON);
      g_assert_cmpfloat_with_epsilon (g, tests[i].g, EPSILON);
      g_assert_cmpfloat_with_epsilon (b, tests[i].b, EPSILON);

      gtk_rgb_to_hsv (tests[i].r, tests[i].g, tests[i].b, &h, &s, &v);
      g_assert_cmpfloat_with_epsilon (h, tests[i].h, EPSILON);
      g_assert_cmpfloat_with_epsilon (s, tests[i].s, EPSILON);
      g_assert_cmpfloat_with_epsilon (v, tests[i].v, EPSILON);
    }
}

static void
test_roundtrips_rgb_hwb (void)
{
  struct {
    float r, g, b;
    float hue, white, black;
  } tests[] = {
    { 0, 0, 0, 0, 0, 1 },
    { 1, 1, 1, 0, 1, 0 },
    { 1, 0, 0, 0, 0, 0 },
    { 1, 1, 0, 60, 0, 0 },
    { 0, 1, 0, 120, 0, 0 },
    { 0, 1, 1, 180, 0, 0 },
    { 0, 0, 1, 240, 0, 0 },
    { 1, 0, 1, 300, 0, 0 },
    { 0.5, 0.5, 0.5, 0, 0.5, 0.5 },
  };
  const float EPSILON = 1e-6;

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float r, g, b;
      float hue, white, black;

      gtk_hwb_to_rgb (tests[i].hue, tests[i].white, tests[i].black, &r, &g, &b);
      g_assert_cmpfloat_with_epsilon (r, tests[i].r, EPSILON);
      g_assert_cmpfloat_with_epsilon (g, tests[i].g, EPSILON);
      g_assert_cmpfloat_with_epsilon (b, tests[i].b, EPSILON);

      gtk_rgb_to_hwb (tests[i].r, tests[i].g, tests[i].b, &hue, &white, &black);
      g_assert_cmpfloat_with_epsilon (hue, tests[i].hue, EPSILON);
      g_assert_cmpfloat_with_epsilon (white, tests[i].white, EPSILON);
      g_assert_cmpfloat_with_epsilon (black, tests[i].black, EPSILON);
    }
}

static void
test_roundtrips_rgb_oklab (void)
{
  struct {
    float red, green, blue;
    float L, a, b;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 0, 0 },
    { 1, 0, 0, 0.62796, 0.22486, 0.12585 },
    { 1, 1, 0, 0.96798, -0.07137, 0.19857 },
    { 0, 1, 0, 0.86644, -0.23389, 0.17950 },
    { 0, 1, 1, 0.90540, -0.14944, -0.03940 },
    { 0, 0, 1, 0.45201, -0.03246, -0.31153 },
    { 1, 0, 1, 0.70167, 0.27457, -0.16916 },
    { 0.5, 0.5, 0.5, 0.598181, 0.00000, 0.00000 },
  };
  const float EPSILON = 1e-3;

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float red, green, blue;
      float L, a, b;

      gtk_oklab_to_rgb (tests[i].L, tests[i].a, tests[i].b, &red, &green, &blue);
      g_assert_cmpfloat_with_epsilon (red, tests[i].red, EPSILON);
      g_assert_cmpfloat_with_epsilon (green, tests[i].green, EPSILON);
      g_assert_cmpfloat_with_epsilon (blue, tests[i].blue, EPSILON);

      gtk_rgb_to_oklab (tests[i].red, tests[i].green, tests[i].blue, &L, &a, &b);
      g_assert_cmpfloat_with_epsilon (L, tests[i].L, EPSILON);
      g_assert_cmpfloat_with_epsilon (a, tests[i].a, EPSILON);
      g_assert_cmpfloat_with_epsilon (b, tests[i].b, EPSILON);
    }
}

static void
test_roundtrips_rgb_linear_srgb (void)
{
  struct {
    float red, green, blue;
    float linear_red, linear_green, linear_blue;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1 },
    { 0.691, 0.139, 0.26, 0.435, 0.017, 0.055 },
    { 0.25, 0.5, 0.75, 0.0508, 0.214, 0.522 },
  };
  const float EPSILON = 1e-3;

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float red, green, blue;

      gtk_linear_srgb_to_rgb (tests[i].linear_red,
                              tests[i].linear_green,
                              tests[i].linear_blue,
                              &red, &green, &blue);
      g_assert_cmpfloat_with_epsilon (red, tests[i].red, EPSILON);
      g_assert_cmpfloat_with_epsilon (green, tests[i].green, EPSILON);
      g_assert_cmpfloat_with_epsilon (blue, tests[i].blue, EPSILON);

      gtk_rgb_to_linear_srgb (tests[i].red, tests[i].green, tests[i].blue,
                              &red, &green, &blue);
      g_assert_cmpfloat_with_epsilon (red, tests[i].linear_red, EPSILON);
      g_assert_cmpfloat_with_epsilon (green, tests[i].linear_green, EPSILON);
      g_assert_cmpfloat_with_epsilon (blue, tests[i].linear_blue, EPSILON);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/color/roundtrips/rgb-hsv", test_roundtrips_rgb_hsv);
  g_test_add_func ("/color/roundtrips/rgb-hwb", test_roundtrips_rgb_hwb);
  g_test_add_func ("/color/roundtrips/rgb-oklab", test_roundtrips_rgb_oklab);
  g_test_add_func ("/color/roundtrips/rgb-linear-srgb", test_roundtrips_rgb_linear_srgb);

  return g_test_run();
}
