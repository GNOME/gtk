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
#include "gdkdevicetool.h"
#include "gdkdisplayprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"

/* for the use of round() */
#include "fallback-c89.c"

/**
 * SECTION:gdkdevice
 * @Short_description: Object representing an input device
 * @Title: GdkDevice
 * @See_also: #GdkSeat
 *
 * The #GdkDevice object represents a single input device, such
 * as a keyboard, a mouse, a touchpad, etc.
 *
 * See the #GdkSeat documentation for more information
 * about the various kinds of master and slave devices, and their
 * relationships.
 */

/**
 * GdkDevice:
 *
 * The GdkDevice struct contains only private fields and
 * should not be accessed directly.
 */

typedef struct _GdkAxisInfo GdkAxisInfo;

struct _GdkAxisInfo
{
  char *label;
  GdkAxisUse use;

  gdouble min_axis;
  gdouble max_axis;
  gdouble min_value;
  gdouble max_value;
  gdouble resolution;
};

enum {
  CHANGED,
  TOOL_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


static void gdk_device_finalize     (GObject      *object);
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
  PROP_NAME,
  PROP_ASSOCIATED_DEVICE,
  PROP_TYPE,
  PROP_INPUT_SOURCE,
  PROP_INPUT_MODE,
  PROP_HAS_CURSOR,
  PROP_N_AXES,
  PROP_VENDOR_ID,
  PROP_PRODUCT_ID,
  PROP_SEAT,
  PROP_NUM_TOUCHES,
  PROP_AXES,
  PROP_TOOL,
  LAST_PROP
};

static GParamSpec *device_props[LAST_PROP] = { NULL, };

static void
gdk_device_class_init (GdkDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_finalize;
  object_class->dispose = gdk_device_dispose;
  object_class->set_property = gdk_device_set_property;
  object_class->get_property = gdk_device_get_property;

  /**
   * GdkDevice:display:
   *
   * The #GdkDisplay the #GdkDevice pertains to.
   */
  device_props[PROP_DISPLAY] =
      g_param_spec_object ("display",
                           P_("Device Display"),
                           P_("Display which the device belongs to"),
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:name:
   *
   * The device name.
   */
  device_props[PROP_NAME] =
      g_param_spec_string ("name",
                           P_("Device name"),
                           P_("Device name"),
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);
  /**
   * GdkDevice:type:
   *
   * Device role in the device manager.
   */
  device_props[PROP_TYPE] =
      g_param_spec_enum ("type",
                         P_("Device type"),
                         P_("Device role in the device manager"),
                         GDK_TYPE_DEVICE_TYPE,
                         GDK_DEVICE_TYPE_MASTER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:associated-device:
   *
   * Associated pointer or keyboard with this device, if any. Devices of type #GDK_DEVICE_TYPE_MASTER
   * always come in keyboard/pointer pairs. Other device types will have a %NULL associated device.
   */
  device_props[PROP_ASSOCIATED_DEVICE] =
      g_param_spec_object ("associated-device",
                           P_("Associated device"),
                           P_("Associated pointer or keyboard with this device"),
                           GDK_TYPE_DEVICE,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:input-source:
   *
   * Source type for the device.
   */
  device_props[PROP_INPUT_SOURCE] =
      g_param_spec_enum ("input-source",
                         P_("Input source"),
                         P_("Source type for the device"),
                         GDK_TYPE_INPUT_SOURCE,
                         GDK_SOURCE_MOUSE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /*
   * GdkDevice:input-mode:
   *
   * Input mode for the device.
   */
  device_props[PROP_INPUT_MODE] =
      g_param_spec_enum ("input-mode",
                         P_("Input mode for the device"),
                         P_("Input mode for the device"),
                         GDK_TYPE_INPUT_MODE,
                         GDK_MODE_DISABLED,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDevice:has-cursor:
   *
   * Whether the device is represented by a cursor on the screen. Devices of type
   * %GDK_DEVICE_TYPE_MASTER will have %TRUE here.
   */
  device_props[PROP_HAS_CURSOR] =
      g_param_spec_boolean ("has-cursor",
                            P_("Whether the device has a cursor"),
                            P_("Whether there is a visible cursor following device motion"),
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:n-axes:
   *
   * Number of axes in the device.
   */
  device_props[PROP_N_AXES] =
      g_param_spec_uint ("n-axes",
                         P_("Number of axes in the device"),
                         P_("Number of axes in the device"),
                         0, G_MAXUINT,
                         0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:vendor-id:
   *
   * Vendor ID of this device, see gdk_device_get_vendor_id().
   */
  device_props[PROP_VENDOR_ID] =
      g_param_spec_string ("vendor-id",
                           P_("Vendor ID"),
                           P_("Vendor ID"),
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:product-id:
   *
   * Product ID of this device, see gdk_device_get_product_id().
   */
  device_props[PROP_PRODUCT_ID] =
      g_param_spec_string ("product-id",
                           P_("Product ID"),
                           P_("Product ID"),
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:seat:
   *
   * #GdkSeat of this device.
   */
  device_props[PROP_SEAT] =
      g_param_spec_object ("seat",
                           P_("Seat"),
                           P_("Seat"),
                           GDK_TYPE_SEAT,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

  /**
   * GdkDevice:num-touches:
   *
   * The maximal number of concurrent touches on a touch device.
   * Will be 0 if the device is not a touch device or if the number
   * of touches is unknown.
   */
  device_props[PROP_NUM_TOUCHES] =
      g_param_spec_uint ("num-touches",
                         P_("Number of concurrent touches"),
                         P_("Number of concurrent touches"),
                         0, G_MAXUINT,
                         0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);
  /**
   * GdkDevice:axes:
   *
   * The axes currently available for this device.
   */
  device_props[PROP_AXES] =
    g_param_spec_flags ("axes",
                        P_("Axes"),
                        P_("Axes"),
                        GDK_TYPE_AXIS_FLAGS, 0,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  device_props[PROP_TOOL] =
    g_param_spec_object ("tool",
                         P_("Tool"),
                         P_("The tool that is currently used with this device"),
                         GDK_TYPE_DEVICE_TOOL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, device_props);

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

  /**
   * GdkDevice::tool-changed:
   * @device: the #GdkDevice that changed.
   * @tool: The new current tool
   *
   * The ::tool-changed signal is emitted on pen/eraser
   * #GdkDevices whenever tools enter or leave proximity.
   */
  signals[TOOL_CHANGED] =
    g_signal_new (g_intern_static_string ("tool-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GDK_TYPE_DEVICE_TOOL);
}

static void
gdk_device_axis_info_clear (gpointer data)
{
  GdkAxisInfo *info = data;

  g_free (info->label);
}

static void
gdk_device_init (GdkDevice *device)
{
  device->axes = g_array_new (FALSE, TRUE, sizeof (GdkAxisInfo));
  g_array_set_clear_func (device->axes, gdk_device_axis_info_clear);
}

static void
gdk_device_finalize (GObject *object)
{
  GdkDevice *device = GDK_DEVICE (object);

  if (device->axes)
    {
      g_array_free (device->axes, TRUE);
      device->axes = NULL;
    }

  g_clear_pointer (&device->name, g_free);
  g_clear_pointer (&device->keys, g_free);
  g_clear_pointer (&device->vendor_id, g_free);
  g_clear_pointer (&device->product_id, g_free);

  G_OBJECT_CLASS (gdk_device_parent_class)->finalize (object);
}

static void
gdk_device_dispose (GObject *object)
{
  GdkDevice *device = GDK_DEVICE (object);
  GdkDevice *associated = device->associated;

  if (associated && device->type == GDK_DEVICE_TYPE_SLAVE)
    _gdk_device_remove_slave (associated, device);

  if (associated)
    {
      device->associated = NULL;

      if (device->type == GDK_DEVICE_TYPE_MASTER &&
          associated->associated == device)
        _gdk_device_set_associated_device (associated, NULL);

      g_object_unref (associated);
    }

  g_clear_object (&device->last_tool);

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
    case PROP_VENDOR_ID:
      device->vendor_id = g_value_dup_string (value);
      break;
    case PROP_PRODUCT_ID:
      device->product_id = g_value_dup_string (value);
      break;
    case PROP_SEAT:
      device->seat = g_value_get_object (value);
      break;
    case PROP_NUM_TOUCHES:
      device->num_touches = g_value_get_uint (value);
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
    case PROP_VENDOR_ID:
      g_value_set_string (value, device->vendor_id);
      break;
    case PROP_PRODUCT_ID:
      g_value_set_string (value, device->product_id);
      break;
    case PROP_SEAT:
      g_value_set_object (value, device->seat);
      break;
    case PROP_NUM_TOUCHES:
      g_value_set_uint (value, device->num_touches);
      break;
    case PROP_AXES:
      g_value_set_flags (value, device->axis_flags);
      break;
    case PROP_TOOL:
      g_value_set_object (value, device->last_tool);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_device_get_state: (skip)
 * @device: a #GdkDevice.
 * @surface: a #GdkSurface.
 * @axes: (nullable) (array): an array of doubles to store the values of
 * the axes of @device in, or %NULL.
 * @mask: (optional) (out): location to store the modifiers, or %NULL.
 *
 * Gets the current state of a pointer device relative to @surface. As a slave
 * device’s coordinates are those of its master pointer, this
 * function may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them. See gdk_device_grab().
 */
void
gdk_device_get_state (GdkDevice       *device,
                      GdkSurface       *surface,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE ||
                    gdk_display_device_is_grabbed (gdk_device_get_display (device), device));

  if (GDK_DEVICE_GET_CLASS (device)->get_state)
    GDK_DEVICE_GET_CLASS (device)->get_state (device, surface, axes, mask);
}

/**
 * gdk_device_get_position:
 * @device: pointer device to query status about.
 * @x: (out) (allow-none): location to store root window X coordinate of @device, or %NULL.
 * @y: (out) (allow-none): location to store root window Y coordinate of @device, or %NULL.
 *
 * Gets the current location of @device in double precision. As a slave device's
 * coordinates are those of its master pointer, this function
 * may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them. See gdk_device_grab().
 **/
void
gdk_device_get_position (GdkDevice *device,
                         double    *x,
                         double    *y)
{
  GdkDisplay *display;
  gdouble tmp_x, tmp_y;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);

  display = gdk_device_get_display (device);

  g_return_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE ||
                    gdk_display_device_is_grabbed (display, device));

  _gdk_device_query_state (device,
                           NULL,
                           NULL,
                           &tmp_x, &tmp_y,
                           NULL, NULL, NULL);

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
}

/**
 * gdk_device_get_surface_at_position:
 * @device: pointer #GdkDevice to query info to.
 * @win_x: (out) (allow-none): return location for the X coordinate of the device location,
 *         relative to the surface origin, or %NULL.
 * @win_y: (out) (allow-none): return location for the Y coordinate of the device location,
 *         relative to the surface origin, or %NULL.
 *
 * Obtains the surface underneath @device, returning the location of the device in @win_x and @win_y in
 * double precision. Returns %NULL if the surface tree under @device is not known to GDK (for example,
 * belongs to another application).
 *
 * As a slave device coordinates are those of its master pointer, This
 * function may not be called on devices of type %GDK_DEVICE_TYPE_SLAVE,
 * unless there is an ongoing grab on them, see gdk_device_grab().
 *
 * Returns: (nullable) (transfer none): the #GdkSurface under the
 *   device position, or %NULL.
 **/
GdkSurface *
gdk_device_get_surface_at_position (GdkDevice *device,
                                    double    *win_x,
                                    double    *win_y)
{
  gdouble tmp_x, tmp_y;
  GdkSurface *surface;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE ||
                        gdk_display_device_is_grabbed (gdk_device_get_display (device), device), NULL);

  surface = _gdk_device_surface_at_position (device, &tmp_x, &tmp_y, NULL, FALSE);

  if (win_x)
    *win_x = tmp_x;
  if (win_y)
    *win_y = tmp_y;

  return surface;
}

/**
 * gdk_device_get_history: (skip)
 * @device: a #GdkDevice
 * @surface: the surface with respect to which which the event coordinates will be reported
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
 * Note that there is also gdk_surface_set_event_compression() to get
 * more motion events delivered directly, independent of the windowing
 * system.
 *
 * Returns: %TRUE if the windowing system supports motion history and
 *  at least one event was found.
 **/
gboolean
gdk_device_get_history (GdkDevice      *device,
                        GdkSurface      *surface,
                        guint32         start,
                        guint32         stop,
                        GdkTimeCoord ***events,
                        gint           *n_events)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, FALSE);
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  if (n_events)
    *n_events = 0;

  if (events)
    *events = NULL;

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  if (!GDK_DEVICE_GET_CLASS (device)->get_history)
    return FALSE;

  return GDK_DEVICE_GET_CLASS (device)->get_history (device, surface,
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
 * entire screen or to a single surface.
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
  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_INPUT_MODE]);

  return TRUE;
}

/**
 * gdk_device_get_n_keys:
 * @device: a #GdkDevice
 *
 * Returns the number of keys the device currently has.
 *
 * Returns: the number of keys.
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

  switch ((guint) use)
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

      g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_TYPE]);
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
 *     A #GList of strings, free with g_list_free().
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
      axes = g_list_prepend (axes, axis_info.label);
    }

  return g_list_reverse (axes);
}

/**
 * gdk_device_get_axis_value: (skip)
 * @device: a pointer #GdkDevice.
 * @axes: (array): pointer to an array of axes
 * @axis_label: name of the label
 * @value: (out): location to store the found value.
 *
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis label, as returned
 * by gdk_device_list_axes()
 *
 * Returns: %TRUE if the given axis use was found, otherwise %FALSE.
 **/
gboolean
gdk_device_get_axis_value (GdkDevice  *device,
                           gdouble    *axes,
                           const char *axis_label,
                           gdouble    *value)
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

      if (!g_str_equal (axis_info.label, axis_label))
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
     ~(GDK_BUTTON_MOTION_MASK |
       GDK_BUTTON1_MOTION_MASK |
       GDK_BUTTON2_MOTION_MASK |
       GDK_BUTTON3_MOTION_MASK));
}

GdkGrabStatus
gdk_device_grab (GdkDevice        *device,
                 GdkSurface       *surface,
                 GdkGrabOwnership  grab_ownership,
                 gboolean          owner_events,
                 GdkEventMask      event_mask,
                 GdkCursor        *cursor,
                 guint32           time_)
{
  GdkGrabStatus res;

  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_GRAB_FAILED);
  g_return_val_if_fail (GDK_IS_SURFACE (surface), GDK_GRAB_FAILED);
  g_return_val_if_fail (gdk_surface_get_display (surface) == gdk_device_get_display (device), GDK_GRAB_FAILED);

  if (GDK_SURFACE_DESTROYED (surface))
    return GDK_GRAB_NOT_VIEWABLE;

  res = GDK_DEVICE_GET_CLASS (device)->grab (device,
                                             surface,
                                             owner_events,
                                             get_native_grab_event_mask (event_mask),
                                             NULL,
                                             cursor,
                                             time_);

  if (res == GDK_GRAB_SUCCESS)
    {
      GdkDisplay *display;
      gulong serial;

      display = gdk_surface_get_display (surface);
      serial = _gdk_display_get_next_serial (display);

      _gdk_display_add_device_grab (display,
                                    device,
                                    surface,
                                    grab_ownership,
                                    owner_events,
                                    event_mask,
                                    serial,
                                    time_,
                                    FALSE);
    }

  return res;
}

void
gdk_device_ungrab (GdkDevice  *device,
                   guint32     time_)
{
  g_return_if_fail (GDK_IS_DEVICE (device));

  GDK_DEVICE_GET_CLASS (device)->ungrab (device, time_);
}

/* Private API */
void
_gdk_device_reset_axes (GdkDevice *device)
{
  gint i;

  for (i = device->axes->len - 1; i >= 0; i--)
    g_array_remove_index (device->axes, i);

  device->axis_flags = 0;

  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_N_AXES]);
  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_AXES]);
}

guint
_gdk_device_add_axis (GdkDevice   *device,
                      const char  *label_name,
                      GdkAxisUse   use,
                      gdouble      min_value,
                      gdouble      max_value,
                      gdouble      resolution)
{
  GdkAxisInfo axis_info;
  guint pos;

  axis_info.use = use;
  axis_info.label = g_strdup (label_name);
  axis_info.min_value = min_value;
  axis_info.max_value = max_value;
  axis_info.resolution = resolution;

  switch ((guint) use)
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

  device->axis_flags |= (1 << use);

  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_N_AXES]);
  g_object_notify_by_pspec (G_OBJECT (device), device_props[PROP_AXES]);

  return pos;
}

void
_gdk_device_get_axis_info (GdkDevice   *device,
			   guint        index_,
			   const char **label_name,
			   GdkAxisUse   *use,
			   gdouble      *min_value,
			   gdouble      *max_value,
			   gdouble      *resolution)
{
  GdkAxisInfo *info;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index_ < device->axes->len);

  info = &g_array_index (device->axes, GdkAxisInfo, index_);

  *label_name = info->label;
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
_gdk_device_translate_surface_coord (GdkDevice *device,
                                    GdkSurface *surface,
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
  gint surface_width, surface_height;

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

  x_min = axis_info_x->min_value;
  y_min = axis_info_y->min_value;

  surface_width = gdk_surface_get_width (surface);
  surface_height = gdk_surface_get_height (surface);

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

  if (device_aspect * surface_width >= surface_height)
    {
      /* device taller than surface */
      x_scale = surface_width / device_width;
      y_scale = (x_scale * x_resolution) / y_resolution;

      x_offset = 0;
      y_offset = - (device_height * y_scale - surface_height) / 2;
    }
  else
    {
      /* surface taller than device */
      y_scale = surface_height / device_height;
      x_scale = (y_scale * y_resolution) / x_resolution;

      y_offset = 0;
      x_offset = - (device_width * x_scale - surface_width) / 2;
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
                                    GdkSurface *surface,
                                    gdouble    surface_root_x,
                                    gdouble    surface_root_y,
                                    gdouble    screen_width,
                                    gdouble    screen_height,
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
        scale = screen_width / axis_width;
      else
        scale = 1;

      offset = - surface_root_x;
    }
  else
    {
      if (axis_width > 0)
        scale = screen_height / axis_width;
      else
        scale = 1;

      offset = - surface_root_y;
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
                         GdkSurface        *surface,
                         GdkSurface       **child_surface,
                         gdouble          *root_x,
                         gdouble          *root_y,
                         gdouble          *win_x,
                         gdouble          *win_y,
                         GdkModifierType  *mask)
{
  GDK_DEVICE_GET_CLASS (device)->query_state (device,
                                              surface,
                                              child_surface,
                                              root_x,
                                              root_y,
                                              win_x,
                                              win_y,
                                              mask);
}

GdkSurface *
_gdk_device_surface_at_position (GdkDevice        *device,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask,
                                gboolean          get_toplevel)
{
  return GDK_DEVICE_GET_CLASS (device)->surface_at_position (device,
                                                            win_x,
                                                            win_y,
                                                            mask,
                                                            get_toplevel);
}

/**
 * gdk_device_get_last_event_surface:
 * @device: a #GdkDevice, with a source other than %GDK_SOURCE_KEYBOARD
 *
 * Gets information about which surface the given pointer device is in, based on events
 * that have been received so far from the display server. If another application
 * has a pointer grab, or this application has a grab with owner_events = %FALSE,
 * %NULL may be returned even if the pointer is physically over one of this
 * application's surfaces.
 *
 * Returns: (transfer none) (allow-none): the last surface the device
 */
GdkSurface *
gdk_device_get_last_event_surface (GdkDevice *device)
{
  GdkDisplay *display;
  GdkPointerSurfaceInfo *info;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);

  display = gdk_device_get_display (device);
  info = _gdk_display_get_pointer_info (display, device);

  return info->surface_under_pointer;
}

/**
 * gdk_device_get_vendor_id:
 * @device: a slave #GdkDevice
 *
 * Returns the vendor ID of this device, or %NULL if this information couldn't
 * be obtained. This ID is retrieved from the device, and is thus constant for
 * it.
 *
 * This function, together with gdk_device_get_product_id(), can be used to eg.
 * compose #GSettings paths to store settings for this device.
 *
 * |[<!-- language="C" -->
 *  static GSettings *
 *  get_device_settings (GdkDevice *device)
 *  {
 *    const gchar *vendor, *product;
 *    GSettings *settings;
 *    GdkDevice *device;
 *    gchar *path;
 *
 *    vendor = gdk_device_get_vendor_id (device);
 *    product = gdk_device_get_product_id (device);
 *
 *    path = g_strdup_printf ("/org/example/app/devices/%s:%s/", vendor, product);
 *    settings = g_settings_new_with_path (DEVICE_SCHEMA, path);
 *    g_free (path);
 *
 *    return settings;
 *  }
 * ]|
 *
 * Returns: (nullable): the vendor ID, or %NULL
 */
const gchar *
gdk_device_get_vendor_id (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER, NULL);

  return device->vendor_id;
}

/**
 * gdk_device_get_product_id:
 * @device: a slave #GdkDevice
 *
 * Returns the product ID of this device, or %NULL if this information couldn't
 * be obtained. This ID is retrieved from the device, and is thus constant for
 * it. See gdk_device_get_vendor_id() for more information.
 *
 * Returns: (nullable): the product ID, or %NULL
 */
const gchar *
gdk_device_get_product_id (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER, NULL);

  return device->product_id;
}

void
gdk_device_set_seat (GdkDevice *device,
                     GdkSeat   *seat)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (!seat || GDK_IS_SEAT (seat));

  if (device->seat == seat)
    return;

  device->seat = seat;
  g_object_notify (G_OBJECT (device), "seat");
}

/**
 * gdk_device_get_seat:
 * @device: A #GdkDevice
 *
 * Returns the #GdkSeat the device belongs to.
 *
 * Returns: (transfer none): A #GdkSeat. This memory is owned by GTK+ and
 *          must not be freed.
 **/
GdkSeat *
gdk_device_get_seat (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->seat;
}

/**
 * gdk_device_get_axes:
 * @device: a #GdkDevice
 *
 * Returns the axes currently available on the device.
 **/
GdkAxisFlags
gdk_device_get_axes (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->axis_flags;
}

void
gdk_device_update_tool (GdkDevice     *device,
                        GdkDeviceTool *tool)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER);

  if (g_set_object (&device->last_tool, tool))
    {
      g_object_notify (G_OBJECT (device), "tool");
      g_signal_emit (device, signals[TOOL_CHANGED], 0, tool);
    }
}

GdkInputMode
gdk_device_get_input_mode (GdkDevice *device)
{
  return device->mode;
}
