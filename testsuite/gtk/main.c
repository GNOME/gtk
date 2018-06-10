#include <gtk/gtk.h>
#include <locale.h>

static void
test_init (void)
{
  GThread *self = g_thread_self ();

  g_assert (gtk_is_initialized () == FALSE);
  g_assert (gtk_get_main_thread () == NULL);

  g_assert (gtk_init_check ());
  g_assert (gtk_is_initialized () == TRUE);

  g_assert (gtk_get_main_thread () == self);
}

int
main (int argc, char *argv[])
{
  /* Don't use gtk_test_init here because it implicitely initializes GTK+. */
  g_test_init (&argc, &argv, NULL);
  gtk_disable_setlocale();
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  g_test_add_func ("/main/init", test_init);

  return g_test_run ();
}
