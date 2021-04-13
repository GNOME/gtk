/*
 * Copyright © 2019 Benjamin Otte
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

#define EPSILON (1.f / 1024 / 32) /* 2^-15 */

/* macros stolen from graphene testsuite, so they get to keep their names */

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
/* Use typeof on GCC */
# define graphene_fuzzy_equals(n1,n2,epsilon) \
  G_GNUC_EXTENSION({ \
    __auto_type _n1 = (n1); \
    __auto_type _n2 = (n2); \
    __auto_type _epsilon = (epsilon); \
    ((_n1 > _n2 ? (_n1 - _n2 ) : (_n2 - _n1)) <= _epsilon); \
  })

#else
/* fallback for Visual Studio, typeof not supported */
# define graphene_fuzzy_equals(n1,n2,epsilon) \
  (((n1) > (n2) ? ((n1) - (n2)) : ((n2) - (n1))) <= (epsilon))

#endif /* __GNUC__ */

#define graphene_assert_fuzzy_matrix_cell_equal(row,col,n1,n2,epsilon) \
  G_STMT_START { \
    if (graphene_fuzzy_equals (n1, n2, epsilon)) ; else { \
      char *s = g_strdup_printf ("[%d][%d]: " #n1 " == " #n2 " (+/- " #epsilon "): (%.7g == %.7g)", \
                                 row, col, n1, n2); \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, s); \
      g_free (s); \
    } \
  } G_STMT_END

#define graphene_assert_fuzzy_matrix_equal(m1,m2,epsilon) \
  G_STMT_START { \
    unsigned int __i, __j; \
    float __m1[16], __m2[16]; \
    graphene_matrix_to_float ((m1), __m1); \
    graphene_matrix_to_float ((m2), __m2); \
    for (__i = 0; __i < 4; __i++) { \
      for (__j = 0; __j < 4; __j++) { \
        unsigned int __idx = __i * 4 + __j; \
        graphene_assert_fuzzy_matrix_cell_equal (__i, __j, __m1[__idx], __m2[__idx], epsilon); \
      } \
    } \
  } G_STMT_END

#define graphene_assert_fuzzy_transform_equal(t1,t2,epsilon) \
  G_STMT_START { \
    graphene_matrix_t __mat1, __mat2; \
    gsk_transform_to_matrix ((t1), &__mat1); \
    gsk_transform_to_matrix ((t2), &__mat2); \
    graphene_assert_fuzzy_matrix_equal (&__mat1, &__mat2, (epsilon)); \
  } G_STMT_END

static struct {
  GskTransformCategory category;
} test_transforms[] = {
  { GSK_TRANSFORM_CATEGORY_IDENTITY },
  { GSK_TRANSFORM_CATEGORY_IDENTITY },
  { GSK_TRANSFORM_CATEGORY_2D_TRANSLATE },
  { GSK_TRANSFORM_CATEGORY_3D },
  { GSK_TRANSFORM_CATEGORY_2D },
  { GSK_TRANSFORM_CATEGORY_3D },
  { GSK_TRANSFORM_CATEGORY_2D_AFFINE },
  { GSK_TRANSFORM_CATEGORY_3D },
  { GSK_TRANSFORM_CATEGORY_ANY },
};

static GskTransform *
apply_test_transform (GskTransform *transform,
                      guint         i)
{
  switch (i)
  {
    case 0:
      return transform ? transform : gsk_transform_new ();

    case 1:
      return gsk_transform_transform (transform, NULL);

    case 2:
      return gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (3, 5));

    case 3:
      return gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (3, 5, 7));

    case 4:
      return gsk_transform_rotate (transform, 90);

    case 5:
      return gsk_transform_rotate_3d (transform, 90, graphene_vec3_y_axis ());

    case 6:
      return gsk_transform_scale (transform, 2, 3);

    case 7:
      return gsk_transform_scale_3d (transform, 2, 3, 5);

    case 8:
      return gsk_transform_perspective (transform, 5);

    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static GskTransformCategory
categorize_matrix (const graphene_matrix_t *matrix)
{
  if (!graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 0, 3), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 1, 3), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 2, 3), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 3, 3), 1, EPSILON))
    return GSK_TRANSFORM_CATEGORY_ANY;

  if (!graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 0, 2), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 1, 2), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 2, 2), 1, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 3, 2), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 2, 0), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 2, 1), 0, EPSILON))
    return GSK_TRANSFORM_CATEGORY_3D;

  if (!graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 0, 1), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 1, 0), 0, EPSILON))
    return GSK_TRANSFORM_CATEGORY_2D;

  if (!graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 0, 0), 1, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 1, 1), 1, EPSILON))
    return GSK_TRANSFORM_CATEGORY_2D_AFFINE;

  if (!graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 3, 0), 0, EPSILON) ||
      !graphene_fuzzy_equals (graphene_matrix_get_value (matrix, 3, 1), 0, EPSILON))
    return GSK_TRANSFORM_CATEGORY_2D_TRANSLATE;

  return GSK_TRANSFORM_CATEGORY_IDENTITY;
}

static void
check_conversions (GskTransform         *transform,
                   GskTransformCategory  expected_category)
{
  graphene_matrix_t matrix, test;
  float f[16] = { 1, 0, 0, 0,
                  0, 1, 0, 0,
                  0, 0, 1, 0,
                  0, 0, 0, 1 };

  g_assert_cmpint (gsk_transform_get_category (transform), ==, expected_category);
  gsk_transform_to_matrix (transform, &matrix);
  /* we don't insist on getting simplifications right.
   * The matrix "scale(2) scale(0.5)" would be categorized as identity,
   * but the transform might not do that.
   */
  g_assert_cmpint (gsk_transform_get_category (transform), <=, categorize_matrix (&matrix));

  switch (expected_category)
  {
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      break;

    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_to_translate (transform,
                                  &f[4 * 3 + 0], &f[4 * 3 + 1]);
      graphene_matrix_init_from_float (&test, f);
      graphene_assert_fuzzy_matrix_equal (&matrix, &test, EPSILON);

      G_GNUC_FALLTHROUGH;
    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      gsk_transform_to_affine (transform,
                               &f[4 * 0 + 0], &f[4 * 1 + 1],
                               &f[4 * 3 + 0], &f[4 * 3 + 1]);
      graphene_matrix_init_from_float (&test, f);
      graphene_assert_fuzzy_matrix_equal (&matrix, &test, EPSILON);

      G_GNUC_FALLTHROUGH;
    case GSK_TRANSFORM_CATEGORY_2D:
      gsk_transform_to_2d (transform,
                           &f[4 * 0 + 0], &f[4 * 0 + 1],
                           &f[4 * 1 + 0], &f[4 * 1 + 1],
                           &f[4 * 3 + 0], &f[4 * 3 + 1]);
      graphene_matrix_init_from_float (&test, f);
      graphene_assert_fuzzy_matrix_equal (&matrix, &test, EPSILON);
      break;

    default:
      g_assert_not_reached ();
   }
}

static void
test_conversions_simple (void)
{
  GskTransform *transform;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test_transforms); i++)
    {
      transform = apply_test_transform (NULL, i);
      check_conversions (transform, test_transforms[i].category);
      gsk_transform_unref (transform);
    }
}

static void
test_conversions_transformed (void)
{
  GskTransform *transform;
  guint i, j, k;

  for (i = 0; i < G_N_ELEMENTS (test_transforms); i++)
    {
      for (j = 0; j < G_N_ELEMENTS (test_transforms); j++)
        {
          for (k = 0; k < G_N_ELEMENTS (test_transforms); k++)
            {
              transform = apply_test_transform (NULL, i);
              transform = apply_test_transform (transform, j);
              transform = apply_test_transform (transform, k);
              check_conversions (transform, MIN (test_transforms[i].category, MIN (test_transforms[j].category, test_transforms[k].category)));
              gsk_transform_unref (transform);
            }
        }
    }
}

static void
test_invert (void)
{
  GskTransform *transform, *inverse, *identity;
  guint i, j, k;

  for (i = 0; i < G_N_ELEMENTS (test_transforms); i++)
    {
      for (j = 0; j < G_N_ELEMENTS (test_transforms); j++)
        {
          for (k = 0; k < G_N_ELEMENTS (test_transforms); k++)
            {
              transform = apply_test_transform (NULL, i);
              transform = apply_test_transform (transform, j);
              transform = apply_test_transform (transform, k);
              inverse = gsk_transform_invert (gsk_transform_ref (transform));
              g_assert_true (inverse != NULL || transform == NULL);

              identity = gsk_transform_transform (gsk_transform_ref (transform), inverse);
              graphene_assert_fuzzy_transform_equal (identity, NULL, EPSILON);
              gsk_transform_unref (identity);

              inverse = gsk_transform_invert (inverse);
              graphene_assert_fuzzy_transform_equal (transform, inverse, EPSILON);

              gsk_transform_unref (transform);
              gsk_transform_unref (inverse);
            }
        }
    }
}

/* some trivialities around identity transforms */
static void
test_identity (void)
{
  GskTransform *s, *t, *u, *v, *w, *x;
  char *string;
  float a, b, c, d, tx, ty;
  gboolean res;

  s = gsk_transform_new ();
  t = gsk_transform_new ();
  u = gsk_transform_transform (gsk_transform_ref (s), NULL);

  g_assert_cmpint (gsk_transform_get_category (s), ==, GSK_TRANSFORM_CATEGORY_IDENTITY);
  g_assert_cmpint (gsk_transform_get_category (t), ==, GSK_TRANSFORM_CATEGORY_IDENTITY);
  g_assert_cmpint (gsk_transform_get_category (u), ==, GSK_TRANSFORM_CATEGORY_IDENTITY);

  g_assert_true (gsk_transform_equal (s, t));
  g_assert_true (gsk_transform_equal (t, u));
  g_assert_true (gsk_transform_equal (s, u));

  v = gsk_transform_transform (gsk_transform_ref (s), t);

  g_assert_cmpint (gsk_transform_get_category (v), ==, GSK_TRANSFORM_CATEGORY_IDENTITY);

  w = gsk_transform_invert (gsk_transform_ref (v));
  g_assert_cmpint (gsk_transform_get_category (w), ==, GSK_TRANSFORM_CATEGORY_IDENTITY);

  string = gsk_transform_to_string (s);
  res = gsk_transform_parse (string, &x);

  g_assert_true (res);
  g_assert_cmpint (gsk_transform_get_category (x), ==, GSK_TRANSFORM_CATEGORY_IDENTITY);

  gsk_transform_to_2d (s, &a, &b, &c, &d, &tx, &ty);

  g_assert_cmpfloat (a, ==, 1.0f);
  g_assert_cmpfloat (b, ==, 0.0f);
  g_assert_cmpfloat (c, ==, 0.0f);
  g_assert_cmpfloat (d, ==, 1.0f);
  g_assert_cmpfloat (tx, ==, 0.0f);
  g_assert_cmpfloat (ty, ==, 0.f);

  gsk_transform_unref (s);
  gsk_transform_unref (t);
  gsk_transform_unref (u);
  gsk_transform_unref (v);
  gsk_transform_unref (w);
  gsk_transform_unref (x);

  g_free (string);
}

static void
test_identity_equal (void)
{
  GskTransform *id = gsk_transform_new ();
  GskTransform *t;

  g_assert_true (gsk_transform_equal (NULL, NULL));
  g_assert_true (gsk_transform_equal (id, NULL));
  g_assert_true (gsk_transform_equal (NULL, id));
  g_assert_true (gsk_transform_equal (id, id));

  t = gsk_transform_transform (NULL, NULL);
  g_assert_true (gsk_transform_equal (t, NULL));
  gsk_transform_unref (t);
  t = gsk_transform_transform (gsk_transform_new (), NULL);
  g_assert_true (gsk_transform_equal (t, NULL));
  gsk_transform_unref (t);
  t = gsk_transform_transform (NULL, id);
  g_assert_true (gsk_transform_equal (t, NULL));
  gsk_transform_unref (t);
  t = gsk_transform_transform (gsk_transform_new (), id);
  g_assert_true (gsk_transform_equal (t, NULL));
  gsk_transform_unref (t);
  t = gsk_transform_new ();
  t = gsk_transform_transform (t, t);
  g_assert_true (gsk_transform_equal (t, NULL));
  gsk_transform_unref (t);

  gsk_transform_unref (id);
}

static void
test_print_parse (void)
{
  GskTransform *transform, *parsed;
  guint i, j, k;
  char *str1, *str2;
  gboolean ret;

  for (i = 0; i < G_N_ELEMENTS (test_transforms); i++)
    {
      for (j = 0; j < G_N_ELEMENTS (test_transforms); j++)
        {
          for (k = 0; k < G_N_ELEMENTS (test_transforms); k++)
            {
              transform = apply_test_transform (NULL, i);
              transform = apply_test_transform (transform, j);
              transform = apply_test_transform (transform, k);

              str1 = gsk_transform_to_string (transform);
              g_assert_nonnull (str1);
              g_assert_true (strlen (str1) > 0);

              str2 = gsk_transform_to_string (transform);
              g_assert_cmpstr (str1, ==, str2);
              g_free (str2);

              ret = gsk_transform_parse (str1, &parsed);
              g_assert_true (ret);
              graphene_assert_fuzzy_transform_equal (parsed, transform, EPSILON);

              str2 = gsk_transform_to_string (parsed);
              g_assert_cmpstr (str1, ==, str2);
              g_free (str2);

              g_free (str1);
              gsk_transform_unref (parsed);
              gsk_transform_unref (transform);
            }
        }
    }
}

static void
gsk_matrix_transform_rect (const graphene_matrix_t *m,
                           const graphene_rect_t   *r,
                           graphene_quad_t         *res)
{
  graphene_point_t ret[4];
  graphene_rect_t rr;

  graphene_rect_normalize_r (r, &rr);

#define TRANSFORM_POINT(matrix, rect, corner, out_p)   do {\
  graphene_vec4_t __s; \
  graphene_point_t __p; \
  float w; \
  graphene_rect_get_ ## corner (rect, &__p); \
  graphene_vec4_init (&__s, __p.x, __p.y, 0.f, 1.f); \
  graphene_matrix_transform_vec4 (matrix, &__s, &__s); \
  w = graphene_vec4_get_w (&__s); \
  out_p.x = graphene_vec4_get_x (&__s) / w; \
  out_p.y = graphene_vec4_get_y (&__s) / w;           } while (0)

  TRANSFORM_POINT (m, &rr, top_left, ret[0]);
  TRANSFORM_POINT (m, &rr, top_right, ret[1]);
  TRANSFORM_POINT (m, &rr, bottom_right, ret[2]);
  TRANSFORM_POINT (m, &rr, bottom_left, ret[3]);

#undef TRANSFORM_POINT

  graphene_quad_init (res, &ret[0], &ret[1], &ret[2], &ret[3]);
}

/* This is an auxiliary function used in the GL renderer to
 * determine if transforming an axis-aligned rectangle produces
 * axis-aligned output, to decide whether to use linear
 * interpolation or not.
 *
 * Keep this in sync with gsk/gl/gskglrenderer.c
 */
static gboolean
result_is_axis_aligned (GskTransform          *transform,
                        const graphene_rect_t *bounds)
{
  graphene_matrix_t m;
  graphene_quad_t q;
  graphene_rect_t b;
  graphene_point_t b1, b2;
  const graphene_point_t *p;
  int i;

  gsk_transform_to_matrix (transform, &m);
  gsk_matrix_transform_rect (&m, bounds, &q);
  graphene_quad_bounds (&q, &b);
  graphene_rect_get_top_left (&b, &b1);
  graphene_rect_get_bottom_right (&b, &b2);

  for (i = 0; i < 4; i++)
    {
      p = graphene_quad_get_point (&q, i);
      if (fabs (p->x - b1.x) > FLT_EPSILON && fabs (p->x - b2.x) > FLT_EPSILON)
        return FALSE;
      if (fabs (p->y - b1.y) > FLT_EPSILON && fabs (p->y - b2.y) > FLT_EPSILON)
        return FALSE;
    }

  return TRUE;
}

static void
test_axis_aligned (void)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0, 0, 10, 10);
  GskTransform *transform = NULL;

  transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (10, 10));
  g_assert_true (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  transform = gsk_transform_translate_3d (NULL, &GRAPHENE_POINT3D_INIT(0, 10, 10));
  g_assert_true (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  transform = gsk_transform_rotate (NULL, 90);
  g_assert_true (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  transform = gsk_transform_scale (NULL, 2, 3);
  g_assert_true (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  /* rotating around the y axis does not affect axis alignedness,
   * as long as we don't involve perspective
   */
  transform = gsk_transform_rotate_3d (NULL, 45, graphene_vec3_y_axis ());
  g_assert_true (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  /* rotating by 45 around the z axis, not axis aligned */
  transform = gsk_transform_rotate (NULL, 45);
  g_assert_false (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  /* perspective is harmless as long as we stay in the z=0 plane */
  transform = gsk_transform_perspective (NULL, 100);
  g_assert_true (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);

  /* a complex transform that makes things look '3d' */
  transform = gsk_transform_translate_3d (NULL, &GRAPHENE_POINT3D_INIT (0, 0, 50));
  transform = gsk_transform_perspective (transform, 170);
  transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (50, 0, 50));
  transform = gsk_transform_rotate (transform, 20);
  transform = gsk_transform_rotate_3d (transform, 20, graphene_vec3_y_axis ());
  g_assert_false (result_is_axis_aligned (transform, &r));
  gsk_transform_unref (transform);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/transform/conversions/simple", test_conversions_simple);
  g_test_add_func ("/transform/conversions/transformed", test_conversions_transformed);
  g_test_add_func ("/transform/identity", test_identity);
  g_test_add_func ("/transform/identity-equal", test_identity_equal);
  g_test_add_func ("/transform/invert", test_invert);
  g_test_add_func ("/transform/print-parse", test_print_parse);
  g_test_add_func ("/transform/check-axis-aligneness", test_axis_aligned);

  return g_test_run ();
}
