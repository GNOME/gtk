/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <math.h>

#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"

/* for the use of round() */
#include "fallback-c89.c"

/**
 * SECTION:gdkdevice
 * @Short_description: Object representing an input device
 * @Title: GdkDevice
 * @See_also: #GdkDeviceManager
 *
 * The #GdkDevice object represents a single input device, such
 * as a keyboard, a mouse, a touchpad, etc.
 *
 * See the #GdkDeviceManager documentation for more information
 * about the various kinds of master and slave devices, and their
 * relationships.
 */

typedef struct _GdkAxisInfo GdkAxisInfo;

struct _GdkAxisInfo
{
  GdkAtom label;
  GdkAxisUse use;

  gdouble min_axis;
  gdouble max_axis;
  gdouble min_value;
  gdouble max_value;
  gdouble resolution;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


static void gdk_device_dispose      (GObject      *object);
static void gdk_device_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void gdk_device_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);


G_DEFINE_ABSTRACT_TYPE (GdkDevice, gdk_device, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_DEVICE_MANAGER,
  PROP_NAME,
  PROP_ASSOCIATED_DEVICE,
  PROP_TYPE,
  PROP_INPUT_SOURCE,
  PROP_INPUT_MODE,
  PROP_HAS_CURSOR,
  PROP_N_AXES
};


static void
gdk_device_class_init (GdkDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_device_dispose;
  object_class->set_property = gdk_device_set_property;
  object_class->get_property = gdk_device_get_property;

  /**
   * GdkDevice:display:
   *
   * The #GdkDisplay the #GdkDevice pertains to.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        P_("Device Display"),
                                                        P_("Display which the device belongs to"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GdkDevice:device-manager:
   *
   * The #GdkDeviceManager the #GdkDevice pertains to.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_DEVICE_MANAGER,
                                   g_param_spec_object ("device-manager",
                                                        P_("Device manager"),
                                                        P_("Device manager which the device belongs to"),
                                                        GDK_TYPE_DEVICE_MANAGER,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GdkDevice:name:
   *
   * The device name.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("Device name"),
                                                        P_("Device name"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GdkDevice:type:
   *
   * Device role in the device manager.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_TYPE,
                                   g_param_spec_enum ("type",
                                                      P_("Device type"),
                                                      P_("Device role in the device manager"),
                                                      GDK_TYPE_DEVICE_TYPE,
                                                      GDK_DEVICE_TYPE_MASTER,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));
  /**
   * GdkDevice:associated-device:
   *
   * Associated pointer or keyboard with this device, if any. Devices of type #GDK_DEVICE_TYPE_MASTER
   * always come in keyboard/pointer pairs. Other device types will have a %NULL associated device.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_ASSOCIATED_DEVICE,
                                   g_param_spec_object ("associated-device",
                                                        P_("Associated device"),
                                                        P_("Associated pointer or keyboard with this device"),
                                                        GDK_TYPE_DEVICE,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GdkDevice:input-source:
   *
   * Source type for the device.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_INPUT_SOURCE,
                                   g_param_spec_enum ("input-source",
                                                      P_("Input source"),
                                                      P_("Source type for the device"),
                                                      GDK_TYPE_INPUT_SOURCE,
                                                      GDK_SOURCE_MOUSE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GdkDevice:input-mode:
   *
   * Input mode for the device.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_INPUT_MODE,
                                   g_param_spec_enum ("input-mode",
                                                      P_("Input mode for the device"),
                                                      P_("Input mode for the device"),
                                                      GDK_TYPE_INPUT_MODE,
                                                      GDK_MODE_DISABLED,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GdkDevice:has-cursor:
   *
   * Whether the device is represented by a cursor on the screen. Devices of type
   * %GDK_DEVICE_TYPE_MASTER will have %TRUE here.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_CURSOR,
                                   g_param_spec_boolean ("has-cursor",
                                                         P_("Whether the device has a cursor"),
                                                         P_("Whether there is a visible cursor following device motion"),
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
  /**
   * GdkDevice:n-axes:
   *
   * Number of axes in the device.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_N_AXES,
                                   g_param_spec_uint ("n-axes",
                                                      P_("Number of axes in the device"),
                                                      P_("Number of axes in the device"),
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkDevice::changed:
   * @device: the #GdkDevice that changed.
   *
   * The ::changed signal is emitted either when the #GdkDevice
   * has changed the number of either axes or keys. For example
   * In X this will normally happen when the slave device routing
   * events through the master device changes (for example, user
   * switches from the USB mouse to a tablet), in that case the
   * master device will change to reflect the new slave device
   * axes and keys.
   */
  signals[CHANGED] =
    g_signal_new (g_intern_static_string ("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
gdk_device_init (GdkDevice *device)
{
  device->axes = g_array_new (FALSE, TRUE, sizeof (GdkAxisInfo));
}

static void
gdk_device_dispose (GObject *object)
{
  GdkDevice *device = GDK_DEVICE (object);

  if (device->type == GDK_DEVICE_TYPE_SLAVE)
    _gdk_device_remove_slave (device->associated, device);

  if (device->associated)
    {
      if (device->type == GDK_DEVICE_TYPE_MASTER)
        _gdk_device_set_associated_device (device->associated, NULL);

      g_object_unref (device->associated);
      device->associated = NULL;
    }

  if (device->axes)
    {
      g_array_free (device->axes, TRUE);
      device->axes = NULL;
    }

  g_free (device->name);
  g_free (device->keys);

  device->name = NULL;
  device->keys = NULL;

  G_OBJECT_CLASS (gdk_device_parent_class)->dispose (object);
}

static void
gdk_device_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      device->display = g_value_get_object (value);
      break;
    case PROP_DEVICE_MANAGER:
      device->manager = g_value_get_object (value);
      break;
    case PROP_NAME:
      g_free (device->name);

      device->name = g_value_dup_string (value);
      break;
    case PROP_TYPE:
      device->type = g_value_get_enum (value);
      break;
    case PROP_INPUT_SOURCE:
      device->source = g_value_get_enum (value);
      break;
    case PROP_INPUT_MODE:
      gdk_device_set_mode (device, g_value_get_enum (value));
      break;
    case PROP_HAS_CURSOR:
      device->has_cursor = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, device->display);
      break;
    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, device->manager);
      break;
    case PROP_ASSOCIATED_DEVICE:
      g_value_set_object (value, device->associated);
      break;
    case PROP_NAME:
      g_value_set_string (value, device->name);
      break;
    case PROP_TYPE:
      g_value_set_enum (value, device->type);
      break;
    case PROP_INPUT_SOURCE:
      g_value_set_enum (value, device->source);
      break;
    case PROP_INPUT_MODE:
      g_value_set_enum (value, device->mode);
      break;
    case PROP_HAS_CURSOR:
      g_value_set_boolean (value, device->has_cursor);
      break;
    case PROP_N_AXES:
      g_value_set_uint (value, device->axes->len);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_device_get_state: (skip)
 * @device: a #GdkDevice.
 * @window: a #GdkWindow.
 * @axes: (nullable) (array): an array of doubles to store the values of
 * the axes of @device in, or %NULL.
 * @mask: (optional) (out): location to store the modifiers, or %NULL.
 *
 * Gets the current state of a pointer device relative to @window. As a slave
 * device’s coordinates are those of its master pointer, this
 * function may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them. See gdk_device_grab().
 */
void
gdk_device_get_state (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE ||
                    gdk_display_device_is_grabbed (gdk_device_get_display (device), device));

  if (GDK_DEVICE_GET_CLASS (device)->get_state)
    GDK_DEVICE_GET_CLASS (device)->get_state (device, window, axes, mask);
}

/**
 * gdk_device_get_position_double:
 * @device: pointer device to query status about.
 * @screen: (out) (transfer none) (allow-none): location to store the #GdkScreen
 *          the @device is on, or %NULL.
 * @x: (out) (allow-none): location to store root window X coordinate of @device, or %NULL.
 * @y: (out) (allow-none): location to store root window Y coordinate of @device, or %NULL.
 *
 * Gets the current location of @device in double precision. As a slave device's
 * coordinates are those of its master pointer, this function
 * may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them. See gdk_device_grab().
 *
 * Since: 3.10
 **/
void
gdk_device_get_position_double (GdkDevice        *device,
                                GdkScreen       **screen,
                                gdouble          *x,
                                gdouble          *y)
{
  GdkDisplay *display;
  gdouble tmp_x, tmp_y;
  GdkScreen *default_screen;
  GdkWindow *root;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);

  display = gdk_device_get_display (device);

  g_return_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE ||
                    gdk_display_device_is_grabbed (display, device));

  default_screen = gdk_display_get_default_screen (display);

  _gdk_device_query_state (device,
                           gdk_screen_get_root_window (default_screen),
                           &root, NULL,
                           &tmp_x, &tmp_y,
                           NULL, NULL, NULL);

  if (screen)
    *screen = gdk_window_get_screen (root);
  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
}

/**
 * gdk_device_get_position:
 * @device: pointer device to query status about.
 * @screen: (out) (transfer none) (allow-none): location to store the #GdkScreen
 *          the @device is on, or %NULL.
 * @x: (out) (allow-none): location to store root window X coordinate of @device, or %NULL.
 * @y: (out) (allow-none): location to store root window Y coordinate of @device, or %NULL.
 *
 * Gets the current location of @device. As a slave device
 * coordinates are those of its master pointer, This function
 * may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them, see gdk_device_grab().
 *
 * Since: 3.0
 **/
void
gdk_device_get_position (GdkDevice        *device,
                         GdkScreen       **screen,
                         gint             *x,
                         gint             *y)
{
  gdouble tmp_x, tmp_y;

  gdk_device_get_position_double (device, screen, &tmp_x, &tmp_y);
  if (x)
    *x = round (tmp_x);
  if (y)
    *y = round (tmp_y);
}


/**
 * gdk_device_get_window_at_position_double:
 * @device: pointer #GdkDevice to query info to.
 * @win_x: (out) (allow-none): return location for the X coordinate of the device location,
 *         relative to the window origin, or %NULL.
 * @win_y: (out) (allow-none): return location for the Y coordinate of the device location,
 *         relative to the window origin, or %NULL.
 *
 * Obtains the window underneath @device, returning the location of the device in @win_x and @win_y in
 * double precision. Returns %NULL if the window tree under @device is not known to GDK (for example,
 * belongs to another application).
 *
 * As a slave device coordinates are those of its master pointer, This
 * function may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them, see gdk_device_grab().
 *
 * Returns: (nullable) (transfer none): the #GdkWindow under the
 *   device position, or %NULL.
 *
 * Since: 3.0
 **/
GdkWindow *
gdk_device_get_window_at_position_double (GdkDevice  *device,
                                          gdouble    *win_x,
                                          gdouble    *win_y)
{
  gdouble tmp_x, tmp_y;
  GdkWindow *window;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE ||
                        gdk_display_device_is_grabbed (gdk_device_get_display (device), device), NULL);

  window = _gdk_device_window_at_position (device, &tmp_x, &tmp_y, NULL, FALSE);

  /* This might need corrections, as the native window returned
     may contain client side children */
  if (window)
    window = _gdk_window_find_descendant_at (window,
                                             tmp_x, tmp_y,
                                             &tmp_x, &tmp_y);

  if (win_x)
    *win_x = tmp_x;
  if (win_y)
    *win_y = tmp_y;

  return window;
}

/**
 * gdk_device_get_window_at_position:
 * @device: pointer #GdkDevice to query info to.
 * @win_x: (out) (allow-none): return location for the X coordinate of the device location,
 *         relative to the window origin, or %NULL.
 * @win_y: (out) (allow-none): return location for the Y coordinate of the device location,
 *         relative to the window origin, or %NULL.
 *
 * Obtains the window underneath @device, returning the location of the device in @win_x and @win_y. Returns
 * %NULL if the window tree under @device is not known to GDK (for example, belongs to another application).
 *
 * As a slave device coordinates are those of its master pointer, This
 * function may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them, see gdk_device_grab().
 *
 * Returns: (nullable) (transfer none): the #GdkWindow under the
 * device position, or %NULL.
 *
 * Since: 3.0
 **/
GdkWindow *
gdk_device_get_window_at_position (GdkDevice  *device,
                                   gint       *win_x,
                                   gint       *win_y)
{
  gdouble tmp_x, tmp_y;
  GdkWindow *window;

  window =
    gdk_device_get_window_at_position_double (device, &tmp_x, &tmp_y);

  if (win_x)
    *win_x = round (tmp_x);
  if (win_y)
    *win_y = round (tmp_y);

  return window;
}

/**
 * gdk_device_get_history: (skip)
 * @device: a #GdkDevice
 * @window: the window with respect to which which the event coordinates will be reported
 * @start: starting timestamp for range of events to return
 * @stop: ending timestamp for the range of events to return
 * @events: (array length=n_events) (out) (transfer full) (optional):
 *   location to store a newly-allocated array of #GdkTimeCoord, or
 *   %NULL
 * @n_events: (out) (optional): location to store the length of
 *   @events, or %NULL
 *
 * Obtains the motion history for a pointer device; given a starting and
 * ending timestamp, return all events in the motion history for
 * the device in the given range of time. Some windowing systems
 * do not support motion history, in which case, %FALSE will
 * be returned. (This is not distinguishable from the case where
 * motion history is supported and no events were found.)
 *
 * Note that there is also gdk_window_set_event_compression() to get
 * more motion events delivered directly, independent of the windowing
 * system.
 *
 * Returns: %TRUE if the windowing system supports motion history and
 *  at least one event was found.
 **/
gboolean
gdk_device_get_history (GdkDevice      *device,
                        GdkWindow      *window,
                        guint32         start,
                        guint32         stop,
                        GdkTimeCoord ***events,
                        gint           *n_events)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (n_events)
    *n_events = 0;

  if (events)
    *events = NULL;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  if (!GDK_DEVICE_GET_CLASS (device)->get_history)
    return FALSE;

  return GDK_DEVICE_GET_CLASS (device)->get_history (device, window,
                                                     start, stop,
                                                     events, n_events);
}

GdkTimeCoord **
_gdk_device_allocate_history (GdkDevice *device,
                              gint       n_events)
{
  GdkTimeCoord **result = g_new (GdkTimeCoord *, n_events);
  gint i;

  for (i = 0; i < n_events; i++)
    result[i] = g_malloc (sizeof (GdkTimeCoord) -
                          sizeof (double) * (GDK_MAX_TIMECOORD_AXES - device->axes->len));
  return result;
}

/**
 * gdk_device_free_history: (skip)
 * @events: (array length=n_events): an array of #GdkTimeCoord.
 * @n_events: the length of the array.
 *
 * Frees an array of #GdkTimeCoord that was returned by gdk_device_get_history().
 */
void
gdk_device_free_history (GdkTimeCoord **events,
                         gint           n_events)
{
  gint i;

  for (i = 0; i < n_events; i++)
    g_free (events[i]);

  g_free (events);
}

/**
 * gdk_device_get_name:
 * @device: a #GdkDevice
 *
 * Determines the name of the device.
 *
 * Returns: a name
 *
 * Since: 2.20
 **/
const gchar *
gdk_device_get_name (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->name;
}

/**
 * gdk_device_get_has_cursor:
 * @device: a #GdkDevice
 *
 * Determines whether the pointer follows device motion.
 * This is not meaningful for keyboard devices, which don't have a pointer.
 *
 * Returns: %TRUE if the pointer follows device motion
 *
 * Since: 2.20
 **/
gboolean
gdk_device_get_has_cursor (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  return device->has_cursor;
}

/**
 * gdk_device_get_source:
 * @device: a #GdkDevice
 *
 * Determines the type of the device.
 *
 * Returns: a #GdkInputSource
 *
 * Since: 2.20
 **/
GdkInputSource
gdk_device_get_source (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->source;
}

/**
 * gdk_device_get_mode:
 * @device: a #GdkDevice
 *
 * Determines the mode of the device.
 *
 * Returns: a #GdkInputSource
 *
 * Since: 2.20
 **/
GdkInputMode
gdk_device_get_mode (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->mode;
}

/**
 * gdk_device_set_mode:
 * @device: a #GdkDevice.
 * @mode: the input mode.
 *
 * Sets a the mode of an input device. The mode controls if the
 * device is active and whether the device’s range is mapped to the
 * entire screen or to a single window.
 *
 * Note: This is only meaningful for floating devices, master devices (and
 * slaves connected to these) drive the pointer cursor, which is not limited
 * by the input mode.
 *
 * Returns: %TRUE if the mode was successfully changed.
 **/
gboolean
gdk_device_set_mode (GdkDevice    *device,
                     GdkInputMode  mode)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (device->mode == mode)
    return TRUE;

  if (mode == GDK_MODE_DISABLED &&
      gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
    return FALSE;

  device->mode = mode;
  g_object_notify (G_OBJECT (device), "input-mode");

  return TRUE;
}

/**
 * gdk_device_get_n_keys:
 * @device: a #GdkDevice
 *
 * Returns the number of keys the device currently has.
 *
 * Returns: the number of keys.
 *
 * Since: 2.24
 **/
gint
gdk_device_get_n_keys (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_keys;
}

/**
 * gdk_device_get_key:
 * @device: a #GdkDevice.
 * @index_: the index of the macro button to get.
 * @keyval: (out): return value for the keyval.
 * @modifiers: (out): return value for modifiers.
 *
 * If @index_ has a valid keyval, this function will return %TRUE
 * and fill in @keyval and @modifiers with the keyval settings.
 *
 * Returns: %TRUE if keyval is set for @index.
 *
 * Since: 2.20
 **/
gboolean
gdk_device_get_key (GdkDevice       *device,
                    guint            index_,
                    guint           *keyval,
                    GdkModifierType *modifiers)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (index_ < device->num_keys, FALSE);

  if (!device->keys[index_].keyval &&
      !device->keys[index_].modifiers)
    return FALSE;

  if (keyval)
    *keyval = device->keys[index_].keyval;

  if (modifiers)
    *modifiers = device->keys[index_].modifiers;

  return TRUE;
}

/**
 * gdk_device_set_key:
 * @device: a #GdkDevice
 * @index_: the index of the macro button to set
 * @keyval: the keyval to generate
 * @modifiers: the modifiers to set
 *
 * Specifies the X key event to generate when a macro button of a device
 * is pressed.
 **/
void
gdk_device_set_key (GdkDevice      *device,
                    guint           index_,
                    guint           keyval,
                    GdkModifierType modifiers)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index_ < device->num_keys);

  device->keys[index_].keyval = keyval;
  device->keys[index_].modifiers = modifiers;
}

/**
 * gdk_device_get_axis_use:
 * @device: a pointer #GdkDevice.
 * @index_: the index of the axis.
 *
 * Returns the axis use for @index_.
 *
 * Returns: a #GdkAxisUse specifying how the axis is used.
 *
 * Since: 2.20
 **/
GdkAxisUse
gdk_device_get_axis_use (GdkDevice *device,
                         guint      index_)
{
  GdkAxisInfo *info;

  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_AXIS_IGNORE);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, GDK_AXIS_IGNORE);
  g_return_val_if_fail (index_ < device->axes->len, GDK_AXIS_IGNORE);

  info = &g_array_index (device->axes, GdkAxisInfo, index_);

  return info->use;
}

/**
 * gdk_device_set_axis_use:
 * @device: a pointer #GdkDevice
 * @index_: the index of the axis
 * @use: specifies how the axis is used
 *
 * Specifies how an axis of a device is used.
 **/
void
gdk_device_set_axis_use (GdkDevice   *device,
                         guint        index_,
                         GdkAxisUse   use)
{
  GdkAxisInfo *info;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);
  g_return_if_fail (index_ < device->axes->len);

  info = &g_array_index (device->axes, GdkAxisInfo, index_);
  info->use = use;

  switch (use)
    {
    case GDK_AXIS_X:
    case GDK_AXIS_Y:
      info->min_axis = 0;
      info->max_axis = 0;
      break;
    case GDK_AXIS_XTILT:
    case GDK_AXIS_YTILT:
      info->min_axis = -1;
      info->max_axis = 1;
      break;
    default:
      info->min_axis = 0;
      info->max_axis = 1;
      break;
    }
}

/**
 * gdk_device_get_display:
 * @device: a #GdkDevice
 *
 * Returns the #GdkDisplay to which @device pertains.
 *
 * Returns: (transfer none): a #GdkDisplay. This memory is owned
 *          by GTK+, and must not be freed or unreffed.
 *
 * Since: 3.0
 **/
GdkDisplay *
gdk_device_get_display (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->display;
}

/**
 * gdk_device_get_associated_device:
 * @device: a #GdkDevice
 *
 * Returns the associated device to @device, if @device is of type
 * %GDK_DEVICE_TYPE_MASTER, it will return the paired pointer or
 * keyboard.
 *
 * If @device is of type %GDK_DEVICE_TYPE_SLAVE, it will return
 * the master device to which @device is attached to.
 *
 * If @device is of type %GDK_DEVICE_TYPE_FLOATING, %NULL will be
 * returned, as there is no associated device.
 *
 * Returns: (nullable) (transfer none): The associated device, or
 *   %NULL
 *
 * Since: 3.0
 **/
GdkDevice *
gdk_device_get_associated_device (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->associated;
}

static void
_gdk_device_set_device_type (GdkDevice     *device,
                             GdkDeviceType  type)
{
  if (device->type != type)
    {
      device->type = type;

      g_object_notify (G_OBJECT (device), "type");
    }
}

void
_gdk_device_set_associated_device (GdkDevice *device,
                                   GdkDevice *associated)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (associated == NULL || GDK_IS_DEVICE (associated));

  if (device->associated == associated)
    return;

  if (device->associated)
    {
      g_object_unref (device->associated);
      device->associated = NULL;
    }

  if (associated)
    device->associated = g_object_ref (associated);

  if (device->type != GDK_DEVICE_TYPE_MASTER)
    {
      if (device->associated)
        _gdk_device_set_device_type (device, GDK_DEVICE_TYPE_SLAVE);
      else
        _gdk_device_set_device_type (device, GDK_DEVICE_TYPE_FLOATING);
    }
}

/**
 * gdk_device_list_slave_devices:
 * @device: a #GdkDevice
 *
 * If the device if of type %GDK_DEVICE_TYPE_MASTER, it will return
 * the list of slave devices attached to it, otherwise it will return
 * %NULL
 *
 * Returns: (nullable) (transfer container) (element-type GdkDevice):
 *          the list of slave devices, or %NULL. The list must be
 *          freed with g_list_free(), the contents of the list are
 *          owned by GTK+ and should not be freed.
 **/
GList *
gdk_device_list_slave_devices (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER, NULL);

  return g_list_copy (device->slaves);
}

void
_gdk_device_add_slave (GdkDevice *device,
                       GdkDevice *slave)
{
  g_return_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER);
  g_return_if_fail (gdk_device_get_device_type (slave) != GDK_DEVICE_TYPE_MASTER);

  if (!g_list_find (device->slaves, slave))
    device->slaves = g_list_prepend (device->slaves, slave);
}

void
_gdk_device_remove_slave (GdkDevice *device,
                          GdkDevice *slave)
{
  GList *elem;

  g_return_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER);
  g_return_if_fail (gdk_device_get_device_type (slave) != GDK_DEVICE_TYPE_MASTER);

  elem = g_list_find (device->slaves, slave);

  if (!elem)
    return;

  device->slaves = g_list_delete_link (device->slaves, elem);
}

/**
 * gdk_device_get_device_type:
 * @device: a #GdkDevice
 *
 * Returns the device type for @device.
 *
 * Returns: the #GdkDeviceType for @device.
 *
 * Since: 3.0
 **/
GdkDeviceType
gdk_device_get_device_type (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_DEVICE_TYPE_MASTER);

  return device->type;
}

/**
 * gdk_device_get_n_axes:
 * @device: a pointer #GdkDevice
 *
 * Returns the number of axes the device currently has.
 *
 * Returns: the number of axes.
 *
 * Since: 3.0
 **/
gint
gdk_device_get_n_axes (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, 0);

  return device->axes->len;
}

/**
 * gdk_device_list_axes:
 * @device: a pointer #GdkDevice
 *
 * Returns a #GList of #GdkAtoms, containing the labels for
 * the axes that @device currently has.
 *
 * Returns: (transfer container) (element-type GdkAtom):
 *     A #GList of #GdkAtoms, free with g_list_free().
 *
 * Since: 3.0
 **/
GList *
gdk_device_list_axes (GdkDevice *device)
{
  GList *axes = NULL;
  gint i;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);

  for (i = 0; i < device->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (device->axes, GdkAxisInfo, i);
      axes = g_list_prepend (axes, GDK_ATOM_TO_POINTER (axis_info.label));
    }

  return g_list_reverse (axes);
}

/**
 * gdk_device_get_axis_value: (skip)
 * @device: a pointer #GdkDevice.
 * @axes: (array): pointer to an array of axes
 * @axis_label: #GdkAtom with the axis label.
 * @value: (out): location to store the found value.
 *
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis label, as returned
 * by gdk_device_list_axes()
 *
 * Returns: %TRUE if the given axis use was found, otherwise %FALSE.
 *
 * Since: 3.0
 **/
gboolean
gdk_device_get_axis_value (GdkDevice *device,
                           gdouble   *axes,
                           GdkAtom    axis_label,
                           gdouble   *value)
{
  gint i;

  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, FALSE);

  if (axes == NULL)
    return FALSE;

  for (i = 0; i < device->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (device->axes, GdkAxisInfo, i);

      if (axis_info.label != axis_label)
        continue;

      if (value)
        *value = axes[i];

      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_device_get_axis: (skip)
 * @device: a #GdkDevice
 * @axes: (array): pointer to an array of axes
 * @use: the use to look for
 * @value: (out): location to store the found value.
 *
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis use.
 *
 * Returns: %TRUE if the given axis use was found, otherwise %FALSE
 **/
gboolean
gdk_device_get_axis (GdkDevice  *device,
                     gdouble    *axes,
                     GdkAxisUse  use,
                     gdouble    *value)
{
  gint i;

  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, FALSE);

  if (axes == NULL)
    return FALSE;

  g_return_val_if_fail (device->axes != NULL, FALSE);

  for (i = 0; i < device->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (device->axes, GdkAxisInfo, i);

      if (axis_info.use != use)
        continue;

      if (value)
        *value = axes[i];

      return TRUE;
    }

  return FALSE;
}

static GdkEventMask
get_native_grab_event_mask (GdkEventMask grab_mask)
{
  /* Similar to the above but for pointer events only */
  return
    GDK_POINTER_MOTION_MASK |
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
    GDK_SCROLL_MASK |
    (grab_mask &
     ~(GDK_POINTER_MOTION_HINT_MASK |
       GDK_BUTTON_MOTION_MASK |
       GDK_BUTTON1_MOTION_MASK |
       GDK_BUTTON2_MOTION_MASK |
       GDK_BUTTON3_MOTION_MASK));
}

/**
 * gdk_device_grab:
 * @device: a #GdkDevice. To get the device you can use gtk_get_current_event_device()
 *   or gdk_event_get_device() if the grab is in reaction to an event. Also, you can use
 *   gdk_device_manager_get_client_pointer() but only in code that isn’t triggered by a
 *   #GdkEvent and there aren’t other means to get a meaningful #GdkDevice to operate on.
 * @window: the #GdkWindow which will own the grab (the grab window)
 * @grab_ownership: specifies the grab ownership.
 * @owner_events: if %FALSE then all device events are reported with respect to
 *                @window and are only reported if selected by @event_mask. If
 *                %TRUE then pointer events for this application are reported
 *                as normal, but pointer events outside this application are
 *                reported with respect to @window and only if selected by
 *                @event_mask. In either mode, unreported events are discarded.
 * @event_mask: specifies the event mask, which is used in accordance with
 *              @owner_events.
 * @cursor: (allow-none): the cursor to display while the grab is active if the device is
 *          a pointer. If this is %NULL then the normal cursors are used for
 *          @window and its descendants, and the cursor for @window is used
 *          elsewhere.
 * @time_: the timestamp of the event which led to this pointer grab. This
 *         usually comes from the #GdkEvent struct, though %GDK_CURRENT_TIME
 *         can be used if the time isn’t known.
 *
 * Grabs the device so that all events coming from this device are passed to
 * this application until the device is ungrabbed with gdk_device_ungrab(),
 * or the window becomes unviewable. This overrides any previous grab on the device
 * by this client.
 *
 * Device grabs are used for operations which need complete control over the
 * given device events (either pointer or keyboard). For example in GTK+ this
 * is used for Drag and Drop operations, popup menus and such.
 *
 * Note that if the event mask of an X window has selected both button press
 * and button release events, then a button press event will cause an automatic
 * pointer grab until the button is released. X does this automatically since
 * most applications expect to receive button press and release events in pairs.
 * It is equivalent to a pointer grab on the window with @owner_events set to
 * %TRUE.
 *
 * If you set up anything at the time you take the grab that needs to be
 * cleaned up when the grab ends, you should handle the #GdkEventGrabBroken
 * events that are emitted when the grab ends unvoluntarily.
 *
 * Returns: %GDK_GRAB_SUCCESS if the grab was successful.
 *
 * Since: 3.0
 **/
GdkGrabStatus
gdk_device_grab (GdkDevice        *device,
                 GdkWindow        *window,
                 GdkGrabOwnership  grab_ownership,
                 gboolean          owner_events,
                 GdkEventMask      event_mask,
                 GdkCursor        *cursor,
                 guint32           time_)
{
  GdkGrabStatus res;
  GdkWindow *native;

  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_GRAB_SUCCESS);
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_GRAB_SUCCESS);

  native = gdk_window_get_toplevel (window);

  while (native->window_type == GDK_WINDOW_OFFSCREEN)
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
          (!_gdk_window_has_impl (native) &&
           !gdk_window_is_viewable (native)))
        return GDK_GRAB_NOT_VIEWABLE;

      native = gdk_window_get_toplevel (native);
    }

  if (native == NULL || GDK_WINDOW_DESTROYED (native))
    return GDK_GRAB_NOT_VIEWABLE;

  res = GDK_DEVICE_GET_CLASS (device)->grab (device,
                                             native,
                                             owner_events,
                                             get_native_grab_event_mask (event_mask),
                                             NULL,
                                             cursor,
                                             time_);

  if (res == GDK_GRAB_SUCCESS)
    {
      GdkDisplay *display;
      gulong serial;

      display = gdk_window_get_display (window);
      serial = _gdk_display_get_next_serial (display);

      _gdk_display_add_device_grab (display,
                                    device,
                                    window,
                                    native,
                                    grab_ownership,
                                    owner_events,
                                    event_mask,
                                    serial,
                                    time_,
                                    FALSE);
    }

  return res;
}

/**
 * gdk_device_ungrab:
 * @device: a #GdkDevice
 * @time_: a timestap (e.g. %GDK_CURRENT_TIME).
 *
 * Release any grab on @device.
 *
 * Since: 3.0
 */
void
gdk_device_ungrab (GdkDevice  *device,
                   guint32     time_)
{
  g_return_if_fail (GDK_IS_DEVICE (device));

  GDK_DEVICE_GET_CLASS (device)->ungrab (device, time_);
}

/**
 * gdk_device_warp:
 * @device: the device to warp.
 * @screen: the screen to warp @device to.
 * @x: the X coordinate of the destination.
 * @y: the Y coordinate of the destination.
 *
 * Warps @device in @display to the point @x,@y on
 * the screen @screen, unless the device is confined
 * to a window by a grab, in which case it will be moved
 * as far as allowed by the grab. Warping the pointer
 * creates events as if the user had moved the mouse
 * instantaneously to the destination.
 *
 * Note that the pointer should normally be under the
 * control of the user. This function was added to cover
 * some rare use cases like keyboard navigation support
 * for the color picker in the #GtkColorSelectionDialog.
 *
 * Since: 3.0
 **/
void
gdk_device_warp (GdkDevice  *device,
                 GdkScreen  *screen,
                 gint        x,
                 gint        y)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (gdk_device_get_display (device) == gdk_screen_get_display (screen));

  GDK_DEVICE_GET_CLASS (device)->warp (device, screen, x, y);
}

/* Private API */
void
_gdk_device_reset_axes (GdkDevice *device)
{
  gint i;

  for (i = device->axes->len - 1; i >= 0; i--)
    g_array_remove_index (device->axes, i);

  g_object_notify (G_OBJECT (device), "n-axes");
}

guint
_gdk_device_add_axis (GdkDevice   *device,
                      GdkAtom      label_atom,
                      GdkAxisUse   use,
                      gdouble      min_value,
                      gdouble      max_value,
                      gdouble      resolution)
{
  GdkAxisInfo axis_info;
  guint pos;

  axis_info.use = use;
  axis_info.label = label_atom;
  axis_info.min_value = min_value;
  axis_info.max_value = max_value;
  axis_info.resolution = resolution;

  switch (use)
    {
    case GDK_AXIS_X:
    case GDK_AXIS_Y:
      axis_info.min_axis = 0;
      axis_info.max_axis = 0;
      break;
    case GDK_AXIS_XTILT:
    case GDK_AXIS_YTILT:
      axis_info.min_axis = -1;
      axis_info.max_axis = 1;
      break;
    default:
      axis_info.min_axis = 0;
      axis_info.max_axis = 1;
      break;
    }

  device->axes = g_array_append_val (device->axes, axis_info);
  pos = device->axes->len - 1;

  g_object_notify (G_OBJECT (device), "n-axes");

  return pos;
}

void
_gdk_device_get_axis_info (GdkDevice   *device,
			   guint        index_,
			   GdkAtom      *label_atom,
			   GdkAxisUse   *use,
			   gdouble      *min_value,
			   gdouble      *max_value,
			   gdouble      *resolution)
{
  GdkAxisInfo *info;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index_ < device->axes->len);

  info = &g_array_index (device->axes, GdkAxisInfo, index_);

  *label_atom = info->label;
  *use = info->use;
  *min_value = info->min_value;
  *max_value = info->max_value;
  *resolution = info->resolution;
}

void
_gdk_device_set_keys (GdkDevice *device,
                      guint      num_keys)
{
  g_free (device->keys);

  device->num_keys = num_keys;
  device->keys = g_new0 (GdkDeviceKey, num_keys);
}

static GdkAxisInfo *
find_axis_info (GArray     *array,
                GdkAxisUse  use)
{
  GdkAxisInfo *info;
  gint i;

  for (i = 0; i < GDK_AXIS_LAST; i++)
    {
      info = &g_array_index (array, GdkAxisInfo, i);

      if (info->use == use)
        return info;
    }

  return NULL;
}

gboolean
_gdk_device_translate_window_coord (GdkDevice *device,
                                    GdkWindow *window,
                                    guint      index_,
                                    gdouble    value,
                                    gdouble   *axis_value)
{
  GdkAxisInfo axis_info;
  GdkAxisInfo *axis_info_x, *axis_info_y;
  gdouble device_width, device_height;
  gdouble x_offset, y_offset;
  gdouble x_scale, y_scale;
  gdouble x_min, y_min;
  gdouble x_resolution, y_resolution;
  gdouble device_aspect;
  gint window_width, window_height;

  if (index_ >= device->axes->len)
    return FALSE;

  axis_info = g_array_index (device->axes, GdkAxisInfo, index_);

  if (axis_info.use != GDK_AXIS_X &&
      axis_info.use != GDK_AXIS_Y)
    return FALSE;

  if (axis_info.use == GDK_AXIS_X)
    {
      axis_info_x = &axis_info;
      axis_info_y = find_axis_info (device->axes, GDK_AXIS_Y);
    }
  else
    {
      axis_info_x = find_axis_info (device->axes, GDK_AXIS_X);
      axis_info_y = &axis_info;
    }

  device_width = axis_info_x->max_value - axis_info_x->min_value;
  device_height = axis_info_y->max_value - axis_info_y->min_value;

  if (device_width > 0)
    x_min = axis_info_x->min_value;
  else
    {
      device_width = gdk_screen_get_width (gdk_window_get_screen (window));
      x_min = 0;
    }

  if (device_height > 0)
    y_min = axis_info_y->min_value;
  else
    {
      device_height = gdk_screen_get_height (gdk_window_get_screen (window));
      y_min = 0;
    }

  window_width = gdk_window_get_width (window);
  window_height = gdk_window_get_height (window);

  x_resolution = axis_info_x->resolution;
  y_resolution = axis_info_y->resolution;

  /*
   * Some drivers incorrectly report the resolution of the device
   * as zero (in partiular linuxwacom < 0.5.3 with usb tablets).
   * This causes the device_aspect to become NaN and totally
   * breaks windowed mode.  If this is the case, the best we can
   * do is to assume the resolution is non-zero is equal in both
   * directions (which is true for many devices).  The absolute
   * value of the resolution doesn't matter since we only use the
   * ratio.
   */
  if (x_resolution == 0 || y_resolution == 0)
    {
      x_resolution = 1;
      y_resolution = 1;
    }

  device_aspect = (device_height * y_resolution) /
    (device_width * x_resolution);

  if (device_aspect * window_width >= window_height)
    {
      /* device taller than window */
      x_scale = window_width / device_width;
      y_scale = (x_scale * x_resolution) / y_resolution;

      x_offset = 0;
      y_offset = - (device_height * y_scale - window_height) / 2;
    }
  else
    {
      /* window taller than device */
      y_scale = window_height / device_height;
      x_scale = (y_scale * y_resolution) / x_resolution;

      y_offset = 0;
      x_offset = - (device_width * x_scale - window_width) / 2;
    }

  if (axis_value)
    {
      if (axis_info.use == GDK_AXIS_X)
        *axis_value = x_offset + x_scale * (value - x_min);
      else
        *axis_value = y_offset + y_scale * (value - y_min);
    }

  return TRUE;
}

gboolean
_gdk_device_translate_screen_coord (GdkDevice *device,
                                    GdkWindow *window,
                                    gdouble    window_root_x,
                                    gdouble    window_root_y,
                                    guint      index_,
                                    gdouble    value,
                                    gdouble   *axis_value)
{
  GdkAxisInfo axis_info;
  gdouble axis_width, scale, offset;

  if (device->mode != GDK_MODE_SCREEN)
    return FALSE;

  if (index_ >= device->axes->len)
    return FALSE;

  axis_info = g_array_index (device->axes, GdkAxisInfo, index_);

  if (axis_info.use != GDK_AXIS_X &&
      axis_info.use != GDK_AXIS_Y)
    return FALSE;

  axis_width = axis_info.max_value - axis_info.min_value;

  if (axis_info.use == GDK_AXIS_X)
    {
      if (axis_width > 0)
        scale = gdk_screen_get_width (gdk_window_get_screen (window)) / axis_width;
      else
        scale = 1;

      offset = - window_root_x - window->abs_x;
    }
  else
    {
      if (axis_width > 0)
        scale = gdk_screen_get_height (gdk_window_get_screen (window)) / axis_width;
      else
        scale = 1;

      offset = - window_root_y - window->abs_y;
    }

  if (axis_value)
    *axis_value = offset + scale * (value - axis_info.min_value);

  return TRUE;
}

gboolean
_gdk_device_translate_axis (GdkDevice *device,
                            guint      index_,
                            gdouble    value,
                            gdouble   *axis_value)
{
  GdkAxisInfo axis_info;
  gdouble axis_width, out;

  if (index_ >= device->axes->len)
    return FALSE;

  axis_info = g_array_index (device->axes, GdkAxisInfo, index_);

  if (axis_info.use == GDK_AXIS_X ||
      axis_info.use == GDK_AXIS_Y)
    return FALSE;

  axis_width = axis_info.max_value - axis_info.min_value;
  out = (axis_info.max_axis * (value - axis_info.min_value) +
         axis_info.min_axis * (axis_info.max_value - value)) / axis_width;

  if (axis_value)
    *axis_value = out;

  return TRUE;
}

void
_gdk_device_query_state (GdkDevice        *device,
                         GdkWindow        *window,
                         GdkWindow       **root_window,
                         GdkWindow       **child_window,
                         gdouble          *root_x,
                         gdouble          *root_y,
                         gdouble          *win_x,
                         gdouble          *win_y,
                         GdkModifierType  *mask)
{
  GDK_DEVICE_GET_CLASS (device)->query_state (device,
                                              window,
                                              root_window,
                                              child_window,
                                              root_x,
                                              root_y,
                                              win_x,
                                              win_y,
                                              mask);
}

GdkWindow *
_gdk_device_window_at_position (GdkDevice        *device,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask,
                                gboolean          get_toplevel)
{
  return GDK_DEVICE_GET_CLASS (device)->window_at_position (device,
                                                            win_x,
                                                            win_y,
                                                            mask,
                                                            get_toplevel);
}

/**
 * gdk_device_get_last_event_window:
 * @device: a #GdkDevice, with a source other than %GDK_SOURCE_KEYBOARD
 *
 * Gets information about which window the given pointer device is in, based on
 * that have been received so far from the display server. If another application
 * has a pointer grab, or this application has a grab with owner_events = %FALSE,
 * %NULL may be returned even if the pointer is physically over one of this
 * application's windows.
 *
 * Returns: (transfer none) (allow-none): the last window the device
 *
 * Since: 3.12
 */
GdkWindow *
gdk_device_get_last_event_window (GdkDevice *device)
{
  GdkDisplay *display;
  GdkPointerWindowInfo *info;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);

  display = gdk_device_get_display (device);
  info = _gdk_display_get_pointer_info (display, device);

  return info->window_under_pointer;
}
