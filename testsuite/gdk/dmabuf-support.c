#include "config.h"

#include <gtk/gtk.h>
#include "gdk/gdkdmabuffourccprivate.h"
#include "udmabuf.h"

static void
test_dmabuf_no_gpu (void)
{
  guchar buffer[4];
  GdkTexture *texture;
  GError *error = NULL;
  GdkTextureDownloader *downloader;
  gsize stride;
  GBytes *bytes;
  const guchar *data;

  if (!udmabuf_initialize (&error))
    {
      g_test_fail_printf ("%s", error->message);
      g_error_free (error);
      return;
    }

  buffer[0] = 255;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 255;

  bytes = g_bytes_new_static (buffer, 4);

  texture = udmabuf_texture_new (1, 1,
                                 DRM_FORMAT_RGBA8888,
                                 gdk_color_state_get_srgb (),
                                 FALSE,
                                 bytes,
                                 4,
                                 &error);
  g_assert_no_error (error);

  g_bytes_unref (bytes);

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, gdk_texture_get_format (texture));
  gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));

  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);

  gdk_texture_downloader_free (downloader);

  data = g_bytes_get_data (bytes, NULL);
  g_assert_true (memcmp (data, buffer, 4) == 0);

  g_bytes_unref (bytes);

  g_object_unref (texture);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dmabuf/no-gpu", test_dmabuf_no_gpu);

  return g_test_run ();
}
