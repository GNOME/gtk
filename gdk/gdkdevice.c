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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkdevice.h"

#include "gdkdeviceprivate.h"
#include "gdkintl.h"
#include "gdkinternals.h"


typedef struct _GdkDeviceKey GdkDeviceKey;

struct _GdkDeviceKey
{
  guint keyval;
  GdkModifierType modifiers;
};

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

struct _GdkDevicePrivate
{
  gchar *name;
  GdkInputSource source;
  GdkInputMode mode;
  gboolean has_cursor;
  gint num_keys;
  GdkDeviceKey *keys;
  GdkDeviceManager *device_manager;
  GdkDisplay *display;
  GdkDevice *associated;
  GdkDeviceType type;
  GArray *axes;
};

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
                                                      G_PARAM_STATIC_STRINGS));
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
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
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

  g_type_class_add_private (object_class, sizeof (GdkDevicePrivate));
}

static void
gdk_device_init (GdkDevice *device)
{
  GdkDevicePrivate *priv;

  device->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                     GDK_TYPE_DEVICE,
                                                     GdkDevicePrivate);

  priv->axes = g_array_new (FALSE, TRUE, sizeof (GdkAxisInfo));
}

static void
gdk_device_dispose (GObject *object)
{
  GdkDevicePrivate *priv;
  GdkDevice *device;

  device = GDK_DEVICE (object);
  priv = device->priv;

  if (priv->associated)
    {
      _gdk_device_set_associated_device (priv->associated, NULL);
      g_object_unref (priv->associated);
      priv->associated = NULL;
    }

  if (priv->axes)
    {
      g_array_free (priv->axes, TRUE);
      priv->axes = NULL;
    }

  g_free (priv->name);
  g_free (priv->keys);

  priv->name = NULL;
  priv->keys = NULL;

  G_OBJECT_CLASS (gdk_device_parent_class)->dispose (object);
}

static void
gdk_device_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);
  GdkDevicePrivate *priv = device->priv;

  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      break;
    case PROP_DEVICE_MANAGER:
      priv->device_manager = g_value_get_object (value);
      break;
    case PROP_NAME:
      if (priv->name)
        g_free (priv->name);

      priv->name = g_value_dup_string (value);
      break;
    case PROP_TYPE:
      priv->type = g_value_get_enum (value);
      break;
    case PROP_INPUT_SOURCE:
      priv->source = g_value_get_enum (value);
      break;
    case PROP_INPUT_MODE:
      gdk_device_set_mode (device, g_value_get_enum (value));
      break;
    case PROP_HAS_CURSOR:
      priv->has_cursor = g_value_get_boolean (value);
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
  GdkDevicePrivate *priv = device->priv;

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, priv->device_manager);
      break;
    case PROP_ASSOCIATED_DEVICE:
      g_value_set_object (value, priv->associated);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_TYPE:
      g_value_set_enum (value, priv->type);
      break;
    case PROP_INPUT_SOURCE:
      g_value_set_enum (value, priv->source);
      break;
    case PROP_INPUT_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_HAS_CURSOR:
      g_value_set_boolean (value, priv->has_cursor);
      break;
    case PROP_N_AXES:
      g_value_set_uint (value, priv->axes->len);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_device_get_state:
 * @device: a #GdkDevice.
 * @window: a #GdkWindow.
 * @axes: an array of doubles to store the values of the axes of @device in,
 * or %NULL.
 * @mask: location to store the modifiers, or %NULL.
 *
 * Gets the current state of a device relative to @window.
 */
void
gdk_device_get_state (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_DEVICE_GET_CLASS (device)->get_state)
    GDK_DEVICE_GET_CLASS (device)->get_state (device, window, axes, mask);
}

/**
 * gdk_device_get_history:
 * @device: a #GdkDevice
 * @window: the window with respect to which which the event coordinates will be reported
 * @start: starting timestamp for range of events to return
 * @stop: ending timestamp for the range of events to return
 * @events: (array length=n_events) (out) (transfer none): location to store a newly-allocated array of #GdkTimeCoord, or %NULL
 * @n_events: location to store the length of @events, or %NULL
 *
 * Obtains the motion history for a device; given a starting and
 * ending timestamp, return all events in the motion history for
 * the device in the given range of time. Some windowing systems
 * do not support motion history, in which case, %FALSE will
 * be returned. (This is not distinguishable from the case where
 * motion history is supported and no events were found.)
 *
 * Return value: %TRUE if the windowing system supports motion history and
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
			  sizeof (double) * (GDK_MAX_TIMECOORD_AXES - device->priv->axes->len));
  return result;
}

/**
 * gdk_device_free_history:
 * @events: (inout) (transfer none): an array of #GdkTimeCoord.
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
 * Return value: a name
 *
 * Since: 2.20
 **/
const gchar *
gdk_device_get_name (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->priv->name;
}

/**
 * gdk_device_get_has_cursor:
 * @device: a #GdkDevice
 *
 * Determines whether the pointer follows device motion.
 *
 * Return value: %TRUE if the pointer follows device motion
 *
 * Since: 2.20
 **/
gboolean
gdk_device_get_has_cursor (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  return device->priv->has_cursor;
}

/**
 * gdk_device_get_source:
 * @device: a #GdkDevice
 *
 * Determines the type of the device.
 *
 * Return value: a #GdkInputSource
 *
 * Since: 2.20
 **/
GdkInputSource
gdk_device_get_source (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->priv->source;
}

/**
 * gdk_device_set_source:
 * @device: a #GdkDevice.
 * @source: the source type.
 *
 * Sets the source type for an input device.
 **/
void
gdk_device_set_source (GdkDevice      *device,
		       GdkInputSource  source)
{
  g_return_if_fail (GDK_IS_DEVICE (device));

  device->priv->source = source;
  g_object_notify (G_OBJECT (device), "input-source");
}

/**
 * gdk_device_get_mode:
 * @device: a #GdkDevice
 *
 * Determines the mode of the device.
 *
 * Return value: a #GdkInputSource
 *
 * Since: 2.20
 **/
GdkInputMode
gdk_device_get_mode (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->priv->mode;
}

/**
 * gdk_device_set_mode:
 * @device: a #GdkDevice.
 * @mode: the input mode.
 *
 * Sets a the mode of an input device. The mode controls if the
 * device is active and whether the device's range is mapped to the
 * entire screen or to a single window.
 *
 * Returns: %TRUE if the mode was successfully changed.
 **/
gboolean
gdk_device_set_mode (GdkDevice    *device,
                     GdkInputMode  mode)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (device->priv->mode == mode)
    return TRUE;

  if (mode == GDK_MODE_DISABLED &&
      gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
    return FALSE;

  /* FIXME: setting has_cursor when mode is window? */

  device->priv->mode = mode;
  g_object_notify (G_OBJECT (device), "input-mode");

  if (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER)
    _gdk_input_check_extension_events (device);

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

  return device->priv->num_keys;
}

/**
 * gdk_device_get_key:
 * @device: a #GdkDevice.
 * @index_: the index of the macro button to get.
 * @keyval: return value for the keyval.
 * @modifiers: return value for modifiers.
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
  g_return_val_if_fail (index_ < device->priv->num_keys, FALSE);

  if (!device->priv->keys[index_].keyval &&
      !device->priv->keys[index_].modifiers)
    return FALSE;

  if (keyval)
    *keyval = device->priv->keys[index_].keyval;

  if (modifiers)
    *modifiers = device->priv->keys[index_].modifiers;

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
  g_return_if_fail (index_ < device->priv->num_keys);

  device->priv->keys[index_].keyval = keyval;
  device->priv->keys[index_].modifiers = modifiers;
}

/**
 * gdk_device_get_axis_use:
 * @device: a #GdkDevice.
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
  g_return_val_if_fail (index_ < device->priv->axes->len, GDK_AXIS_IGNORE);

  info = &g_array_index (device->priv->axes, GdkAxisInfo, index_);

  return info->use;
}

/**
 * gdk_device_set_axis_use:
 * @device: a #GdkDevice
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
  GdkDevicePrivate *priv;
  GdkAxisInfo *info;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index_ < device->priv->axes->len);

  priv = device->priv;
  info = &g_array_index (priv->axes, GdkAxisInfo, index_);
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
  GdkDevicePrivate *priv;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  priv = device->priv;

  return priv->display;
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
 * Returns: (transfer none): The associated device, or %NULL
 *
 * Since: 3.0
 **/
GdkDevice *
gdk_device_get_associated_device (GdkDevice *device)
{
  GdkDevicePrivate *priv;

  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  priv = device->priv;

  return priv->associated;
}

void
_gdk_device_set_associated_device (GdkDevice *device,
                                   GdkDevice *associated)
{
  GdkDevicePrivate *priv;

  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (associated == NULL || GDK_IS_DEVICE (associated));

  priv = device->priv;

  if (priv->associated == associated)
    return;

  if (priv->associated)
    {
      g_object_unref (priv->associated);
      priv->associated = NULL;
    }

  if (associated)
    priv->associated = g_object_ref (associated);
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
  GdkDevicePrivate *priv;

  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_DEVICE_TYPE_MASTER);

  priv = device->priv;

  return priv->type;
}

/**
 * gdk_device_get_n_axes:
 * @device: a #GdkDevice
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

  return device->priv->axes->len;
}

/**
 * gdk_device_list_axes:
 * @device: a #GdkDevice
 *
 * Returns a #GList of #GdkAtom<!-- -->s, containing the labels for
 * the axes that @device currently has.
 *
 * Returns: (transfer container) (element-type GdkAtom):
 *     A #GList of #GdkAtom<!-- -->s, free with g_list_free().
 *
 * Since: 3.0
 **/
GList *
gdk_device_list_axes (GdkDevice *device)
{
  GdkDevicePrivate *priv;
  GList *axes = NULL;
  gint i;

  priv = device->priv;

  for (i = 0; i < priv->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (priv->axes, GdkAxisInfo, i);
      axes = g_list_prepend (axes, GDK_ATOM_TO_POINTER (axis_info.label));
    }

  return g_list_reverse (axes);
}

/**
 * gdk_device_get_axis_value:
 * @device: a #GdkDevice.
 * @axes: pointer to an array of axes
 * @axis_label: #GdkAtom with the axis label.
 * @value: location to store the found value.
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
  GdkDevicePrivate *priv;
  gint i;

  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (axes == NULL)
    return FALSE;

  priv = device->priv;

  for (i = 0; i < priv->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (priv->axes, GdkAxisInfo, i);

      if (axis_info.label != axis_label)
        continue;

      if (value)
        *value = axes[i];

      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_device_get_axis:
 * @device: a #GdkDevice
 * @axes: pointer to an array of axes
 * @use: the use to look for
 * @value: location to store the found value.
 *
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis use.
 *
 * Return value: %TRUE if the given axis use was found, otherwise %FALSE
 **/
gboolean
gdk_device_get_axis (GdkDevice  *device,
                     gdouble    *axes,
                     GdkAxisUse  use,
                     gdouble    *value)
{
  GdkDevicePrivate *priv;
  gint i;

  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (axes == NULL)
    return FALSE;

  priv = device->priv;

  g_return_val_if_fail (priv->axes != NULL, FALSE);

  for (i = 0; i < priv->axes->len; i++)
    {
      GdkAxisInfo axis_info;

      axis_info = g_array_index (priv->axes, GdkAxisInfo, i);

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
 * @device: a #GdkDevice
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
 * @cursor: the cursor to display while the grab is active if the device is
 *          a pointer. If this is %NULL then the normal cursors are used for
 *          @window and its descendants, and the cursor for @window is used
 *          elsewhere.
 * @time_: the timestamp of the event which led to this pointer grab. This
 *         usually comes from the #GdkEvent struct, though %GDK_CURRENT_TIME
 *         can be used if the time isn't known.
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

  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (_gdk_native_windows)
    native = window;
  else
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

  res = _gdk_windowing_device_grab (device,
                                    window,
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
      serial = _gdk_windowing_window_get_next_serial (display);

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

/* Private API */
void
_gdk_device_reset_axes (GdkDevice *device)
{
  GdkDevicePrivate *priv;
  gint i;

  priv = device->priv;

  for (i = priv->axes->len - 1; i >= 0; i--)
    g_array_remove_index (priv->axes, i);

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
  GdkDevicePrivate *priv;
  GdkAxisInfo axis_info;
  guint pos;

  priv = device->priv;

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

  priv->axes = g_array_append_val (priv->axes, axis_info);
  pos = device->priv->axes->len - 1;

  g_object_notify (G_OBJECT (device), "n-axes");

  return pos;
}

void
_gdk_device_set_keys (GdkDevice *device,
                      guint      num_keys)
{
  if (device->priv->keys)
    g_free (device->priv->keys);

  device->priv->num_keys = num_keys;
  device->priv->keys = g_new0 (GdkDeviceKey, num_keys);
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

GdkAxisUse
_gdk_device_get_axis_use (GdkDevice *device,
                          guint      index_)
{
  GdkDevicePrivate *priv;
  GdkAxisInfo info;

  priv = device->priv;

  info = g_array_index (priv->axes, GdkAxisInfo, index_);
  return info.use;
}

gboolean
_gdk_device_translate_window_coord (GdkDevice *device,
                                    GdkWindow *window,
                                    guint      index_,
                                    gdouble    value,
                                    gdouble   *axis_value)
{
  GdkDevicePrivate *priv;
  GdkAxisInfo axis_info;
  GdkAxisInfo *axis_info_x, *axis_info_y;
  gdouble device_width, device_height;
  gdouble x_offset, y_offset;
  gdouble x_scale, y_scale;
  gdouble x_min, y_min;
  gdouble x_resolution, y_resolution;
  gdouble device_aspect;
  gint window_width, window_height;

  priv = device->priv;

  if (index_ >= priv->axes->len)
    return FALSE;

  axis_info = g_array_index (priv->axes, GdkAxisInfo, index_);

  if (axis_info.use != GDK_AXIS_X &&
      axis_info.use != GDK_AXIS_Y)
    return FALSE;

  if (axis_info.use == GDK_AXIS_X)
    {
      axis_info_x = &axis_info;
      axis_info_y = find_axis_info (priv->axes, GDK_AXIS_Y);
    }
  else
    {
      axis_info_x = find_axis_info (priv->axes, GDK_AXIS_X);
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
                                    gint       window_root_x,
                                    gint       window_root_y,
                                    guint      index_,
                                    gdouble    value,
                                    gdouble   *axis_value)
{
  GdkDevicePrivate *priv = device->priv;
  GdkAxisInfo axis_info;
  gdouble axis_width, scale, offset;

  if (priv->mode != GDK_MODE_SCREEN)
    return FALSE;

  if (index_ >= priv->axes->len)
    return FALSE;

  axis_info = g_array_index (priv->axes, GdkAxisInfo, index_);

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
  GdkDevicePrivate *priv;
  GdkAxisInfo axis_info;
  gdouble axis_width, out;

  priv = device->priv;

  if (index_ >= priv->axes->len)
    return FALSE;

  axis_info = g_array_index (priv->axes, GdkAxisInfo, index_);

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
