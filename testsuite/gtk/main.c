#include <gtk/gtk.h>
#include <locale.h>

static void
test_init (void)
{
  g_assert (gtk_is_initialized () == FALSE);
  g_assert (gtk_init_check ());
  g_assert (gtk_is_initialized () == TRUE);
}

int
main (int argc, char *argv[])
{
  /* Don't use gtk_test_init here because it implicitely initializes GTK+. */
  g_test_init (&argc, &argv, NULL);
  gtk_disable_setlocale();
  setlocale (LC_ALL, "C");

  g_test_add_func ("/main/init", test_init);

  return g_test_run ();
}
