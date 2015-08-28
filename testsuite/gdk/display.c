#include <stdlib.h>

#include <gdk/gdk.h>

static void
test_unset_display_subprocess1 (void)
{
  GdkDisplayManager *manager;

  g_unsetenv ("DISPLAY");

  g_assert (!gdk_init_check (NULL, NULL));
  manager = gdk_display_manager_get ();
  g_assert (manager != NULL);
  g_assert (gdk_display_manager_get_default_display (manager) == NULL);
}

static void
test_unset_display_subprocess2 (void)
{
  g_unsetenv ("DISPLAY");

  gdk_init (NULL, NULL);
}

static void
test_unset_display (void)
{
  g_test_trap_subprocess ("/display/unset-display/subprocess/1", 0, 0);
  g_test_trap_assert_passed ();

  g_test_trap_subprocess ("/display/unset-display/subprocess/2", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*cannot open display*");
}

static void
test_bad_display_subprocess1 (void)
{
  GdkDisplayManager *manager;

  g_setenv ("DISPLAY", "poo", TRUE);

  g_assert (!gdk_init_check (NULL, NULL));
  manager = gdk_display_manager_get ();
  g_assert (manager != NULL);
  g_assert (gdk_display_manager_get_default_display (manager) == NULL);
}

static void
test_bad_display_subprocess2 (void)
{
  g_setenv ("DISPLAY", "poo", TRUE);
  gdk_init (NULL, NULL);
}

static void
test_bad_display (void)
{
  g_test_trap_subprocess ("/display/bad-display/subprocess/1", 0, 0);
  g_test_trap_assert_passed ();

  g_test_trap_subprocess ("/display/bad-display/subprocess/2", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*cannot open display*");
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  gdk_set_allowed_backends ("x11");

  g_test_add_func ("/display/unset-display", test_unset_display);
  g_test_add_func ("/display/unset-display/subprocess/1", test_unset_display_subprocess1);
  g_test_add_func ("/display/unset-display/subprocess/2", test_unset_display_subprocess2);
  g_test_add_func ("/display/bad-display", test_bad_display);
  g_test_add_func ("/display/bad-display/subprocess/1", test_bad_display_subprocess1);
  g_test_add_func ("/display/bad-display/subprocess/2", test_bad_display_subprocess2);

  return g_test_run ();
}
