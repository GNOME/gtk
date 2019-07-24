#include <locale.h>

#include "gtk/gtk.h"

static void
timing_function_linear (void)
{
  GtkTimingFunction *tm = gtk_linear ();

  g_assert_cmpfloat_with_epsilon (gtk_timing_function_transform_time (tm, 0.0, 1.0), 0.0, 0.0001);
  g_assert_cmpfloat_with_epsilon (gtk_timing_function_transform_time (tm, 0.1, 1.0), 0.1, 0.0001);
  g_assert_cmpfloat_with_epsilon (gtk_timing_function_transform_time (tm, 0.5, 1.0), 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (gtk_timing_function_transform_time (tm, 0.9, 1.0), 0.9, 0.0001);
  g_assert_cmpfloat_with_epsilon (gtk_timing_function_transform_time (tm, 1.0, 1.0), 1.0, 0.0001);

  gtk_timing_function_unref (tm);
}

static void
timing_function_parse (void)
{
  struct {
    const char *str;
    GtkTimingFunction *tm;
  } defs[] = {
    { "linear", gtk_linear () },
    { "cubic-bezier(0, 0, 1, 1)", gtk_cubic_bezier (0, 0, 1, 1) },
  };

  for (int i = 0; i < G_N_ELEMENTS (defs); i++)
    {
      GtkTimingFunction *tm;

      g_assert_true (gtk_timing_function_parse (defs[i].str, &tm));
      g_assert_true (gtk_timing_function_equal (defs[i].tm, tm));

      gtk_timing_function_unref (tm);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/timing-function/linear", timing_function_linear);
  g_test_add_func ("/timing-function/parse", timing_function_parse);

  return g_test_run ();

}
