#include <gdk/gdk.h>
#include <gdk/gdkmemoryformatprivate.h>

static void
test_depth_merge (void)
{
  for (GdkMemoryDepth depth1 = GDK_MEMORY_U8; depth1 < GDK_N_DEPTHS; depth1++)
    {
      g_assert_cmpint (gdk_memory_depth_merge (depth1, depth1), ==, depth1);
      for (GdkMemoryDepth depth2 = GDK_MEMORY_U8; depth2 < depth1; depth2++)
        {
          g_assert_cmpint (gdk_memory_depth_merge (depth1, depth2), ==, gdk_memory_depth_merge (depth2, depth1));
        }
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/depth/merge", test_depth_merge);

  return g_test_run ();
}
