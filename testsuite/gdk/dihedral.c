#include <gtk.h>

#include "gdk/gdkdihedralprivate.h"

static void
test_invariants (void)
{
  g_test_summary ("Verifies inverse dihedrals");

  for (GdkDihedral d = GDK_DIHEDRAL_NORMAL; d <= GDK_DIHEDRAL_FLIPPED_270; d++)
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
    [N90]   = { N90,  N180, N270, N0,   F90,  F180, F270, F0   },
    [N180]  = { N180, N270, N0,   N90,  F180, F270, F0,   F90  },
    [N270]  = { N270, N0,   N90,  N180, F270, F0,   F90,  F180 },
    [F0]    = { F0,   F270, F180, F90,  N0,   N270, N180, N90  },
    [F90]   = { F90,  F0,   F270, F180, N90,  N0,   N270, N180 },
    [F180]  = { F180, F90,  F0,   F270, N180, N90,  N0,   N270 },
    [F270]  = { F270, F180, F90,  F0,   N270, N180, N90,  N0   },
  };

  GdkDihedral d1, d2;

  g_test_summary ("Tests that gdk_dihedral_combine multiplies dihedrals correctly");

  for (d1 = GDK_DIHEDRAL_NORMAL; d1 <= GDK_DIHEDRAL_FLIPPED_270; d1++)
    {
      for (d2 = GDK_DIHEDRAL_NORMAL; d2 <= GDK_DIHEDRAL_FLIPPED_270; d2++)
        {
          if (gdk_dihedral_combine (d1, d2) != expected[d1][d2])
            {
              g_test_message ("d1 = %s, d2 = %s, expected %s, got %s\n",
                              gdk_dihedral_get_name (d1),
                              gdk_dihedral_get_name (d2),
                              gdk_dihedral_get_name (expected[d1][d2]),
                              gdk_dihedral_get_name (gdk_dihedral_combine (d1, d2)));
            }
          g_assert_cmpint (gdk_dihedral_combine (d1, d2), ==, expected[d1][d2]);
        }
    }
}

static void
test_inversions (void)
{
  g_test_summary ("Tests that gdk_dihedral_invert yields the expected results");

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
  g_test_summary ("Tests that gdk_dihedral_swap yields the expected results");

  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_NORMAL));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_90));
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_180));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_270));
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED_90));
  g_assert_false (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED_180));
  g_assert_true (gdk_dihedral_swaps_xy (GDK_DIHEDRAL_FLIPPED_270));
}

static void
test_associative (void)
{
  g_test_summary ("Verifies that gdk_dihedral_combine is associative");

  for (GdkDihedral d1 = GDK_DIHEDRAL_NORMAL; d1 <= GDK_DIHEDRAL_FLIPPED_270; d1++)
    for (GdkDihedral d2 = GDK_DIHEDRAL_NORMAL; d2 <= GDK_DIHEDRAL_FLIPPED_270; d2++)
      for (GdkDihedral d3 = GDK_DIHEDRAL_NORMAL; d3 <= GDK_DIHEDRAL_FLIPPED_270; d3++)
        g_assert_cmpint (gdk_dihedral_combine (gdk_dihedral_combine (d1, d2), d3), ==,
                         gdk_dihedral_combine (d1, gdk_dihedral_combine (d2, d3)));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dihedral/inversions", test_inversions);
  g_test_add_func ("/dihedral/swaps", test_swaps);
  g_test_add_func ("/dihedral/combinations", test_combinations);
  g_test_add_func ("/dihedral/invariants", test_invariants);
  g_test_add_func ("/dihedral/associative", test_associative);

  return g_test_run ();
}
