#include <gtk/gtk.h>

#include "gsk/ngl/fp16private.h"

static void
test_constants (void)
{
  struct {
    float f;
    guint16 h;
  } tests[] = {
    { 0.0, FP16_ZERO },
    { 1.0, FP16_ONE },
    { -1.0, FP16_MINUS_ONE },
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float f[4];
      guint16 h[4];

      memset (f, 0, sizeof (f));
      f[0] = tests[i].f;
      float_to_half4 (f, h);
      g_assert_cmpuint (h[0], ==, tests[i].h);


      memset (h, 0, sizeof (h));
      h[0] = tests[i].h;
      half_to_float4 (h, f);
      g_assert_cmpfloat (f[0], ==, tests[i].f);
    }
}

static void
test_roundtrip (void)
{
  for (int i = 0; i < 100; i++)
    {
      float f[4];
      float f2[4];
      guint16 h[4];

      do
        {
          /* generate a random float thats representable as fp16 */
          memset (h, 0, sizeof (h));
          h[0] = g_random_int_range (G_MININT16, G_MAXINT16);
          half_to_float4 (h, f2);
        }
      while (!isnormal (f2[0])); /* skip nans and infs since they don't compare well */

      memset (f, 0, sizeof (f));
      f[0] = f2[0];

      float_to_half4 (f, h);
      half_to_float4 (h, f2);

      g_assert_cmpfloat (f[0], ==, f2[0]);
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/half-float/constants", test_constants);
  g_test_add_func ("/half-float/roundtrip", test_roundtrip);

  return g_test_run ();
}
