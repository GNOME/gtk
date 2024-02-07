#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkdmabuffourccprivate.h>
#include <gdk/gdkdmabuftextureprivate.h>
#include <gdk/gdkdmabufformatsprivate.h>
#include <gdk/gdkdmabufformatsbuilderprivate.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

static void
test_dmabuf_formats_basic (void)
{
  GdkDisplay *display;
  GdkDmabufFormats *formats;

  display = gdk_display_get_default ();

  formats = gdk_display_get_dmabuf_formats (display);

#ifdef HAVE_DMABUF
  /* We always have basic linear formats */
  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats) >= 6);

  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_BGRA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_ABGR16161616F, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_RGB888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_BGR888, DRM_FORMAT_MOD_LINEAR));
#else
  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats) == 0);
#endif
}

static void
test_dmabuf_formats_builder (void)
{
  GdkDmabufFormatsBuilder *builder;
  GdkDmabufFormats *formats1, *formats2;
  guint32 fourcc;
  guint64 modifier;

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR);
  formats1 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_contains (formats1, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats1, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_false (gdk_dmabuf_formats_contains (formats1, DRM_FORMAT_BGRA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats1) == 2);
  gdk_dmabuf_formats_get_format (formats1, 0, &fourcc, &modifier);
  g_assert_true (fourcc == DRM_FORMAT_ARGB8888 || fourcc == DRM_FORMAT_RGBA8888);
  g_assert_true (modifier == DRM_FORMAT_MOD_LINEAR);

  g_assert_false (gdk_dmabuf_formats_equal (formats1, NULL));

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_formats (builder, formats1);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats2);

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats2);

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_formats (builder, formats1);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGB888, DRM_FORMAT_MOD_LINEAR);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_false (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats2);
  gdk_dmabuf_formats_unref (formats1);
}

#define AAAA fourcc_code ('A', 'A', 'A', 'A')
#define BBBB fourcc_code ('B', 'B', 'B', 'B')
#define CCCC fourcc_code ('C', 'C', 'C', 'C')
#define DDDD fourcc_code ('D', 'D', 'D', 'D')

static gboolean
dmabuf_format_matches (const GdkDmabufFormat *f1, guint32 fourcc, guint64 modifier, gsize next_priority)
{
  return f1->fourcc == fourcc &&
         f1->modifier == modifier &&
         f1->next_priority == next_priority;
}

/* Test that sorting respects priorities, and the highest priority instance
 * of duplicates is kept.
 */
static void
test_priorities (void)
{
  GdkDmabufFormatsBuilder *builder;
  GdkDmabufFormats *formats;
  const GdkDmabufFormat *f;

  builder = gdk_dmabuf_formats_builder_new ();

  gdk_dmabuf_formats_builder_add_format (builder, AAAA, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, BBBB, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, AAAA, I915_FORMAT_MOD_X_TILED);
  gdk_dmabuf_formats_builder_next_priority (builder);
  gdk_dmabuf_formats_builder_add_format (builder, DDDD, I915_FORMAT_MOD_X_TILED);
  gdk_dmabuf_formats_builder_add_format (builder, BBBB, I915_FORMAT_MOD_X_TILED);
  gdk_dmabuf_formats_builder_add_format (builder, CCCC, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, BBBB, DRM_FORMAT_MOD_LINEAR); // a duplicate

  formats = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats) == 6);

  f = gdk_dmabuf_formats_peek_formats (formats);

  g_assert_true (dmabuf_format_matches (&f[0], AAAA, DRM_FORMAT_MOD_LINEAR, 3));
  g_assert_true (dmabuf_format_matches (&f[1], AAAA, I915_FORMAT_MOD_X_TILED, 3));
  g_assert_true (dmabuf_format_matches (&f[2], BBBB, DRM_FORMAT_MOD_LINEAR, 3));
  g_assert_true (dmabuf_format_matches (&f[3], BBBB, I915_FORMAT_MOD_X_TILED, 6));
  g_assert_true (dmabuf_format_matches (&f[4], CCCC, DRM_FORMAT_MOD_LINEAR, 6));
  g_assert_true (dmabuf_format_matches (&f[5], DDDD, I915_FORMAT_MOD_X_TILED, 6));

  gdk_dmabuf_formats_unref (formats);
}

static void
test_wayland (void)
{
  GdkDmabufFormatsBuilder *builder;
  GdkDmabufFormats *formats1, *formats2;

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_format_for_device (builder,
                                                    DRM_FORMAT_RGBA8888,
                                                    0,
                                                    DRM_FORMAT_MOD_LINEAR,
                                                    0);
  gdk_dmabuf_formats_builder_add_format_for_device (builder,
                                                    DRM_FORMAT_ARGB8888,
                                                    0,
                                                    DRM_FORMAT_MOD_LINEAR,
                                                    1);
  formats1 = gdk_dmabuf_formats_builder_free_to_formats_for_device (builder, 2);

#ifdef GDK_WINDOWING_WAYLAND
  g_assert_true (gdk_wayland_dmabuf_formats_get_main_device (formats1) == 2);
  g_assert_true (gdk_wayland_dmabuf_formats_get_target_device (formats1, 0) == 0);
  g_assert_true (gdk_wayland_dmabuf_formats_get_target_device (formats1, 1) == 1);
  g_assert_false (gdk_wayland_dmabuf_formats_is_scanout (formats1, 0));
  g_assert_false (gdk_wayland_dmabuf_formats_is_scanout (formats1, 1));
#endif

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

#ifdef GDK_WINDOWING_WAYLAND
  g_assert_true (gdk_wayland_dmabuf_formats_get_main_device (formats2) == 0);
  g_assert_true (gdk_wayland_dmabuf_formats_get_target_device (formats2, 0) == 0);
  g_assert_true (gdk_wayland_dmabuf_formats_get_target_device (formats2, 1) == 0);
  g_assert_false (gdk_wayland_dmabuf_formats_is_scanout (formats2, 0));
  g_assert_false (gdk_wayland_dmabuf_formats_is_scanout (formats2, 1));
#endif

  g_assert_false (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats1);
  gdk_dmabuf_formats_unref (formats2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dmabuf/formats/basic", test_dmabuf_formats_basic);
  g_test_add_func ("/dmabuf/formats/builder", test_dmabuf_formats_builder);
  g_test_add_func ("/dmabuf/formats/priorities", test_priorities);
  g_test_add_func ("/dmabuf/formats/wayland", test_wayland);

  return g_test_run ();
}
