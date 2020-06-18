#include <gtk/gtk.h>

static void
test_list_seats (void)
{
  GdkDisplay *display;
  GdkSeat *seat0, *seat;
  GList *list, *l;
  gboolean found_default;

  display = gdk_display_get_default ();
  seat0 = gdk_display_get_default_seat (display);
  if (seat0 != NULL)
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

  if (seat0 != NULL)
    g_assert_true (found_default);
  else
    g_assert_true (list == NULL);

  g_list_free (list);
}

static void
test_default_seat (void)
{
  GdkDisplay *display;
  GdkSeat *seat0;
  GdkSeatCapabilities caps;
  GdkDevice *pointer0, *keyboard0, *device;
  GList *physical_devices, *l;

  display = gdk_display_get_default ();
  seat0 = gdk_display_get_default_seat (display);

  if (seat0 == NULL)
    {
      g_test_skip ("Display has no seats");
      return;
    }

  g_assert_true (GDK_IS_SEAT (seat0));

  caps = gdk_seat_get_capabilities (seat0);

  g_assert (caps != GDK_SEAT_CAPABILITY_NONE);

  pointer0 = gdk_seat_get_pointer (seat0);
  physical_devices = gdk_seat_get_physical_devices (seat0, GDK_SEAT_CAPABILITY_POINTER);

  if ((caps & GDK_SEAT_CAPABILITY_POINTER) != 0)
    {
      g_assert_nonnull (pointer0);
      g_assert (gdk_device_get_device_type (pointer0) == GDK_DEVICE_TYPE_LOGICAL);
      g_assert (gdk_device_get_display (pointer0) == display);
      g_assert (gdk_device_get_seat (pointer0) == seat0);

      g_assert_nonnull (physical_devices);
      for (l = physical_devices; l; l = l->next)
        {
          device = l->data;
          g_assert (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_PHYSICAL);
          g_assert (gdk_device_get_display (device) == display);
          g_assert (gdk_device_get_seat (device) == seat0);
        }
      g_list_free (physical_devices);
    }
  else
    {
      g_assert_null (pointer0);
      g_assert_null (physical_devices);
    }

  keyboard0 = gdk_seat_get_keyboard (seat0);
  physical_devices = gdk_seat_get_physical_devices (seat0, GDK_SEAT_CAPABILITY_KEYBOARD);

  if ((caps & GDK_SEAT_CAPABILITY_KEYBOARD) != 0)
    {
      g_assert_nonnull (keyboard0);
      g_assert (gdk_device_get_device_type (keyboard0) == GDK_DEVICE_TYPE_LOGICAL);
      g_assert (gdk_device_get_display (keyboard0) == display);
      g_assert (gdk_device_get_seat (keyboard0) == seat0);
      g_assert (gdk_device_get_source (keyboard0) == GDK_SOURCE_KEYBOARD);

      g_assert_nonnull (physical_devices);
      for (l = physical_devices; l; l = l->next)
        {
          device = l->data;
          g_assert (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_PHYSICAL);
          g_assert (gdk_device_get_display (device) == display);
          g_assert (gdk_device_get_seat (device) == seat0);
          g_assert (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD);
        }
      g_list_free (physical_devices);
    }
  else
    {
      g_assert_null (keyboard0);
      g_assert_null (physical_devices);
    }

}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/seat/list", test_list_seats);
  g_test_add_func ("/seat/default", test_default_seat);

  return g_test_run ();
}
