#include <gdk/gdk.h>

static void
test_list_seats (void)
{
  GdkDisplay *display;
  GdkSeat *seat0, *seat;
  GList *list, *l;
  gboolean found_default;

  display = gdk_display_get_default ();
  seat0 = gdk_display_get_default_seat (display);

  g_assert_true (GDK_IS_SEAT (seat0));

  found_default = FALSE;
  list = gdk_display_list_seats (display);
  for (l = list; l; l = l->next)
    {
      seat = l->data;

      g_assert_true (GDK_IS_SEAT (seat));
      g_assert (gdk_seat_get_display (seat) == display);

      if (seat == seat0)
        found_default = TRUE;
    }
  g_list_free (list);

  g_assert_true (found_default);
}

static void
test_default_seat (void)
{
  GdkDisplay *display;
  GdkSeat *seat0;
  GdkSeatCapabilities caps;
  GdkDevice *pointer0, *keyboard0, *device;
  GList *slaves, *l;

  display = gdk_display_get_default ();
  seat0 = gdk_display_get_default_seat (display);

  g_assert_true (GDK_IS_SEAT (seat0));

  caps = gdk_seat_get_capabilities (seat0);

  g_assert (caps != GDK_SEAT_CAPABILITY_NONE);

  pointer0 = gdk_seat_get_pointer (seat0);
  slaves = gdk_seat_get_slaves (seat0, GDK_SEAT_CAPABILITY_POINTER);

  if ((caps & GDK_SEAT_CAPABILITY_POINTER) != 0)
    {
      g_assert_nonnull (pointer0);
      g_assert (gdk_device_get_device_type (pointer0) == GDK_DEVICE_TYPE_MASTER);
      g_assert (gdk_device_get_display (pointer0) == display);
      g_assert (gdk_device_get_seat (pointer0) == seat0);

      g_assert_nonnull (slaves);
      for (l = slaves; l; l = l->next)
        {
          device = l->data;
          g_assert (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_SLAVE);
          g_assert (gdk_device_get_display (device) == display);
          g_assert (gdk_device_get_seat (device) == seat0);
        }
      g_list_free (slaves);
    }
  else
    {
      g_assert_null (pointer0);
      g_assert_null (slaves);
    }

  keyboard0 = gdk_seat_get_keyboard (seat0);
  slaves = gdk_seat_get_slaves (seat0, GDK_SEAT_CAPABILITY_KEYBOARD);

  if ((caps & GDK_SEAT_CAPABILITY_KEYBOARD) != 0)
    {
      g_assert_nonnull (keyboard0);
      g_assert (gdk_device_get_device_type (keyboard0) == GDK_DEVICE_TYPE_MASTER);
      g_assert (gdk_device_get_display (keyboard0) == display);
      g_assert (gdk_device_get_seat (keyboard0) == seat0);
      g_assert (gdk_device_get_source (keyboard0) == GDK_SOURCE_KEYBOARD);

      g_assert_nonnull (slaves);
      for (l = slaves; l; l = l->next)
        {
          device = l->data;
          g_assert (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_SLAVE);
          g_assert (gdk_device_get_display (device) == display);
          g_assert (gdk_device_get_seat (device) == seat0);
          g_assert (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD);
        }
      g_list_free (slaves);
    }
  else
    {
      g_assert_null (keyboard0);
      g_assert_null (slaves);
    }

}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  gdk_init (NULL, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/seat/list", test_list_seats);
  g_test_add_func ("/seat/default", test_default_seat);

  return g_test_run ();
}
