#include <gtk.h>

#include "gdk/gdkdihedralprivate.h"

static void
test_dihedral (void)
{
  GdkDihedral d;

  for (d = GDK_DIHEDRAL_NORMAL; d <= GDK_DIHEDRAL_FLIPPED_270; d++)
    {
      g_assert_true (gdk_dihedral_combine (d, GDK_DIHEDRAL_NORMAL) == d);
      g_assert_true (gdk_dihedral_combine (d, gdk_dihedral_invert (d)) == GDK_DIHEDRAL_NORMAL);
      g_assert_true (gdk_dihedral_combine (gdk_dihedral_invert (d), d) == GDK_DIHEDRAL_NORMAL);
    }

  g_assert_true (gdk_dihedral_combine (GDK_DIHEDRAL_90, GDK_DIHEDRAL_90) == GDK_DIHEDRAL_180);
  g_assert_true (gdk_dihedral_combine (GDK_DIHEDRAL_90, GDK_DIHEDRAL_180) == GDK_DIHEDRAL_270);
  g_assert_true (gdk_dihedral_combine (GDK_DIHEDRAL_90, GDK_DIHEDRAL_270) == GDK_DIHEDRAL_NORMAL);
  g_assert_true (gdk_dihedral_combine (GDK_DIHEDRAL_FLIPPED, GDK_DIHEDRAL_FLIPPED) == GDK_DIHEDRAL_NORMAL);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/transform/dihedral", test_dihedral);

  return g_test_run ();
}
