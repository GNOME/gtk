#include <gtk.h>

#include "gdk/gdkdihedralprivate.h"

static void
test_invariants (void)
{
  GdkDihedral d;

  for (d = GDK_DIHEDRAL_NORMAL; d <= GDK_DIHEDRAL_FLIPPED_270; d++)
    {
      g_assert_cmpint (gdk_dihedral_combine (d, GDK_DIHEDRAL_NORMAL), ==, d);
      g_assert_cmpint (gdk_dihedral_combine (d, gdk_dihedral_invert (d)), ==, GDK_DIHEDRAL_NORMAL);
      g_assert_cmpint (gdk_dihedral_combine (gdk_dihedral_invert (d), d), ==, GDK_DIHEDRAL_NORMAL);
      g_assert_cmpint (gdk_dihedral_invert (gdk_dihedral_invert (d)), ==, d);
    }
}

static void
test_combinations (void)
{
#define N0 GDK_DIHEDRAL_NORMAL
#define N90 GDK_DIHEDRAL_90
#define N180 GDK_DIHEDRAL_180
#define N270 GDK_DIHEDRAL_270
#define F0 GDK_DIHEDRAL_FLIPPED
#define F90 GDK_DIHEDRAL_FLIPPED_90
#define F180 GDK_DIHEDRAL_FLIPPED_180
#define F270 GDK_DIHEDRAL_FLIPPED_270

  static GdkDihedral expected[8][8] = {
             /* N0,   N90,  N180, N270, F0,   F90,  F180, F270 */
    [N0]    = { N0,   N90,  N180, N270, F0,   F90,  F180, F270 },
    [N90]   = { N90,  N180, N270, N0,   F270, F0,   F90,  F180 },
    [N180]  = { N180, N270, N0,   N90,  F180, F270, F0,   F90  },
    [N270]  = { N270, N0,   N90,  N180, F90,  F180, F270, F0   },
    [F0]    = { F0,   F90,  F180, F270, N0,   N90,  N180, N270 },
    [F90]   = { F90,  F180, F270, F0,   N270, N0,   N90,  N180 },
    [F180]  = { F180, F270, F0,   F90,  N180, N270, N0,   N90  },
    [F270]  = { F270, F0,   F90,  F180, N90,  N180, N270, N0   },
  };

  GdkDihedral d1, d2;

  for (d1 = GDK_DIHEDRAL_NORMAL; d1 <= GDK_DIHEDRAL_FLIPPED_270; d1++)
    {
      for (d2 = GDK_DIHEDRAL_NORMAL; d2 <= GDK_DIHEDRAL_FLIPPED_270; d2++)
        {
          g_assert_cmpint (gdk_dihedral_combine (d1, d2), ==, expected[d1][d2]);
        }
    }
}

static void
test_inversions (void)
{
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_NORMAL),      ==, GDK_DIHEDRAL_NORMAL);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_90),          ==, GDK_DIHEDRAL_270);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_180),         ==, GDK_DIHEDRAL_180);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_270),         ==, GDK_DIHEDRAL_90);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_FLIPPED),     ==, GDK_DIHEDRAL_FLIPPED);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_FLIPPED_90),  ==, GDK_DIHEDRAL_FLIPPED_90);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_FLIPPED_180), ==, GDK_DIHEDRAL_FLIPPED_180);
  g_assert_cmpint (gdk_dihedral_invert (GDK_DIHEDRAL_FLIPPED_270), ==, GDK_DIHEDRAL_FLIPPED_270);
}

static void
test_swaps (void)
{
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_NORMAL));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_90));
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_180));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_270));
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED_90));
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED_180));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED_270));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dihedral/inversions", test_inversions);
  g_test_add_func ("/dihedral/swaps", test_swaps);
  g_test_add_func ("/dihedral/combinations", test_combinations);
  g_test_add_func ("/dihedral/invariants", test_invariants);

  return g_test_run ();
}
