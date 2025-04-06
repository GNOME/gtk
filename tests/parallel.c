#include <gtk/gtk.h>
#include "gdk/gdkmemoryformatprivate.h"

static void
fill_with_random_data (guchar *data, gsize size)
{
  for (gsize i = 0; i < size; i++)
    data[i] = g_random_int_range (0, 256);
}

int
main (int argc, char *argv[])
{
  int rounds = 10;

  gtk_init ();

  for (gsize i = 2; i < 500; i += 2)
    {
      guchar *src_data;
      guchar *dst_data;
      GdkMemoryLayout src_layout;
      GdkMemoryLayout dst_layout;
      gsize width, height;
      guint64 elapsed;

      width = i;
      height = i;

      gdk_memory_layout_init (&src_layout, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, width, height, 1);
      src_data = g_malloc (src_layout.size);
      fill_with_random_data (src_data, src_layout.size);
      gdk_memory_layout_init (&dst_layout, GDK_MEMORY_R16G16B16A16, width, height, 1);
      dst_data = g_malloc (dst_layout.size);

      elapsed = 0;
      for (int k = 0; k < rounds; k++)
        {
          guint64 before = g_get_monotonic_time ();
          gdk_memory_convert (dst_data,
                              &dst_layout,
                              gdk_color_state_get_srgb (),
                              src_data,
                              &src_layout,
                              gdk_color_state_get_srgb_linear ());
          elapsed += g_get_monotonic_time () - before;
        }

      g_print ("%zu, %zu, %zu, %f\n", width, height, width * height, ((double)(elapsed / rounds)) / 1000.0);
    }
}
