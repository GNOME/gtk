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
      gsize src_stride;
      gsize dst_stride;
      gsize width, height;
      guint64 elapsed;

      width = i;
      height = i;
      src_stride = width * 4;
      dst_stride = width * 8;

      src_data = g_malloc (src_stride * height);
      fill_with_random_data (src_data, src_stride * height);
      dst_data = g_malloc (dst_stride * height);

      elapsed = 0;
      for (int k = 0; k < rounds; k++)
        {
          guint64 before = g_get_monotonic_time ();
          gdk_memory_convert (dst_data, dst_stride,
                              GDK_MEMORY_R16G16B16A16,
                              gdk_color_state_get_srgb (),
                              src_data, src_stride,
                              GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
                              gdk_color_state_get_srgb_linear (),
                              width, height);
          elapsed += g_get_monotonic_time () - before;
        }

      g_print ("%zu, %zu, %zu, %f\n", width, height, width * height, ((double)(elapsed / rounds)) / 1000.0);
    }
}
