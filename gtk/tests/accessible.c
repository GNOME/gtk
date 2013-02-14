#include <gtk/gtk.h>

static void
test_type (GType t)
{
  GtkWidget *w;
  AtkObject *a;

  if (g_type_is_a (t, GTK_TYPE_WIDGET))
    {
      w = (GtkWidget *)g_object_new (t, NULL);
      a = gtk_widget_get_accessible (w);

      g_assert (GTK_IS_ACCESSIBLE (a));
      g_assert (gtk_accessible_get_widget (GTK_ACCESSIBLE (a)) == w);

      g_object_unref (w);
    }
}

int
main (int argc, char *argv[])
{
  GType *tp;
  gint i;

  gtk_init (&argc, &argv);

  tp = g_new0 (GType, 1000);
#undef GDK_WINDOWING_X11
#include "../gtktypefuncs.c"
  *tp = 0;

  for (i = 0; tp[i]; i++)
    test_type (tp[i]);

  return 0;
}
