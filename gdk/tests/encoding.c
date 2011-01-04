#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

static void
test_to_text_list (void)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    {
      GdkAtom encoding;
      gint format;
      const guchar *text;
      gint length;
      gchar **list;
      gint n;

      encoding = gdk_atom_intern ("UTF8_STRING", FALSE);
      format = 8;
      text = (const guchar*)"abcdef \304\201 \304\205\0ABCDEF \304\200 \304\204";
      length = 25;
      n = gdk_x11_display_text_property_to_text_list (display, encoding, format, text, length, &list);
      g_assert_cmpint (n, ==, 2);
      g_assert (g_str_has_prefix (list[0], "abcdef "));
      g_assert (g_str_has_prefix (list[1], "ABCDEF "));

      gdk_x11_free_text_list (list);
    }
#endif
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gdk_init (&argc, &argv);

  g_test_add_func ("/encoding/to-text-list", test_to_text_list);

  return g_test_run ();
}
