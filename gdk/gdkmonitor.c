/*
 * gdkmonitor.c
 *
 * Copyright 2016 Red Hat, Inc.
 *
 * Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkmonitorprivate.h"
#include "gdkdisplay.h"
#include "gdkenumtypes.h"
#include "gdkrectangle.h"

#include <math.h>

/**
 * GdkMonitor:
 *
 * `GdkMonitor` objects represent the individual outputs that are
 * associated with a `GdkDisplay`.
 *
 * `GdkDisplay` keeps a `GListModel` to enumerate and monitor
 * monitors with [method@Gdk.Display.get_monitors]. You can use
 * [method@Gdk.Display.get_monitor_at_surface] to find a particular
 * monitor.
 */

enum {
  GDK_MONITOR_PROP_0,
  GDK_MONITOR_PROP_DESCRIPTION,
  GDK_MONITOR_PROP_DISPLAY,
  GDK_MONITOR_PROP_MANUFACTURER,
  GDK_MONITOR_PROP_MODEL,
  GDK_MONITOR_PROP_CONNECTOR,
  GDK_MONITOR_PROP_SCALE_FACTOR,
  GDK_MONITOR_PROP_SCALE,
  GDK_MONITOR_PROP_GEOMETRY,
  GDK_MONITOR_PROP_WIDTH_MM,
  GDK_MONITOR_PROP_HEIGHT_MM,
  GDK_MONITOR_PROP_REFRESH_RATE,
  GDK_MONITOR_PROP_SUBPIXEL_LAYOUT,
  GDK_MONITOR_PROP_VALID,
  GDK_MONITOR_LAST_PROP
};

static GParamSpec *gdk_monitor_properties[GDK_MONITOR_LAST_PROP] = { NULL, };

enum {
  GDK_MONITOR_INVALIDATE,
  GDK_MONITOR_LAST_SIGNAL
};

static guint gdk_monitor_signals[GDK_MONITOR_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkMonitor, gdk_monitor, G_TYPE_OBJECT)

static void
gdk_monitor_init (GdkMonitor *monitor)
{
  monitor->scale_factor = 1;
  monitor->scale = 1.0;
  monitor->scale_set = FALSE;
  monitor->valid = TRUE;
}

static void
gdk_monitor_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkMonitor *monitor = GDK_MONITOR (object);

  switch (prop_id)
    {
    case GDK_MONITOR_PROP_DESCRIPTION:
      g_value_set_string (value, monitor->description);
      break;

    case GDK_MONITOR_PROP_DISPLAY:
      g_value_set_object (value, monitor->display);
      break;

    case GDK_MONITOR_PROP_MANUFACTURER:
      g_value_set_string (value, monitor->manufacturer);
      break;

    case GDK_MONITOR_PROP_MODEL:
      g_value_set_string (value, monitor->model);
      break;

    case GDK_MONITOR_PROP_CONNECTOR:
      g_value_set_string (value, monitor->connector);
      break;

    case GDK_MONITOR_PROP_SCALE_FACTOR:
      g_value_set_int (value, monitor->scale_factor);
      break;

    case GDK_MONITOR_PROP_SCALE:
      g_value_set_double (value, monitor->scale);
      break;

    case GDK_MONITOR_PROP_GEOMETRY:
      g_value_set_boxed (value, &monitor->geometry);
      break;

    case GDK_MONITOR_PROP_WIDTH_MM:
      g_value_set_int (value, monitor->width_mm);
      break;

    case GDK_MONITOR_PROP_HEIGHT_MM:
      g_value_set_int (value, monitor->height_mm);
      break;

    case GDK_MONITOR_PROP_REFRESH_RATE:
      g_value_set_int (value, monitor->refresh_rate);
      break;

    case GDK_MONITOR_PROP_SUBPIXEL_LAYOUT:
      g_value_set_enum (value, monitor->subpixel_layout);
      break;

    case GDK_MONITOR_PROP_VALID:
      g_value_set_boolean (value, monitor->valid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_monitor_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdkMonitor *monitor = GDK_MONITOR (object);

  switch (prop_id)
    {
    case GDK_MONITOR_PROP_DISPLAY:
      monitor->display = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_monitor_finalize (GObject *object)
{
  GdkMonitor *monitor = GDK_MONITOR (object);

  g_free (monitor->description);
  g_free (monitor->connector);
  g_free (monitor->manufacturer);
  g_free (monitor->model);

  G_OBJECT_CLASS (gdk_monitor_parent_class)->finalize (object);
}

static void
gdk_monitor_class_init (GdkMonitorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_monitor_finalize;
  object_class->get_property = gdk_monitor_get_property;
  object_class->set_property = gdk_monitor_set_property;

  /**
   * GdkMonitor:description:
   *
   * A short description of the monitor, meant for display to the user.
   *
   * Since: 4.10
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:display:
   *
   * The `GdkDisplay` of the monitor.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:manufacturer:
   *
   * The manufacturer name.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_MANUFACTURER] =
    g_param_spec_string ("manufacturer", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:model:
   *
   * The model name.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_MODEL] =
    g_param_spec_string ("model", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:connector:
   *
   * The connector name.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_CONNECTOR] =
    g_param_spec_string ("connector", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:scale-factor:
   *
   * The scale factor.
   *
   * The scale factor is the next larger integer,
   * compared to [property@Gdk.Surface:scale].
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_SCALE_FACTOR] =
    g_param_spec_int ("scale-factor", NULL, NULL,
                      1, G_MAXINT,
                      1,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:scale:
   *
   * The scale of the monitor.
   *
   * Since: 4.14
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_SCALE] =
      g_param_spec_double ("scale", NULL, NULL,
                        1., G_MAXDOUBLE, 1.,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:geometry:
   *
   * The geometry of the monitor.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_GEOMETRY] =
    g_param_spec_boxed ("geometry", NULL, NULL,
                        GDK_TYPE_RECTANGLE,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:width-mm:
   *
   * The width of the monitor, in millimeters.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_WIDTH_MM] =
    g_param_spec_int ("width-mm", NULL, NULL,
                      0, G_MAXINT,
                      0,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:height-mm:
   *
   * The height of the monitor, in millimeters.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_HEIGHT_MM] =
    g_param_spec_int ("height-mm", NULL, NULL,
                      0, G_MAXINT,
                      0,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:refresh-rate:
   *
   * The refresh rate, in milli-Hertz.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_REFRESH_RATE] =
    g_param_spec_int ("refresh-rate", NULL, NULL,
                      0, G_MAXINT,
                      0,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:subpixel-layout:
   *
   * The subpixel layout.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_SUBPIXEL_LAYOUT] =
    g_param_spec_enum ("subpixel-layout", NULL, NULL,
                       GDK_TYPE_SUBPIXEL_LAYOUT,
                       GDK_SUBPIXEL_LAYOUT_UNKNOWN,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMonitor:valid: (getter is_valid)
   *
   * Whether the object is still valid.
   */
  gdk_monitor_properties[GDK_MONITOR_PROP_VALID] =
    g_param_spec_boolean ("valid", NULL, NULL,
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, GDK_MONITOR_LAST_PROP, gdk_monitor_properties);

  /**
   * GdkMonitor::invalidate:
   * @monitor: the object on which this signal was emitted
   *
   * Emitted when the output represented by @monitor gets disconnected.
   */
  gdk_monitor_signals[GDK_MONITOR_INVALIDATE] = g_signal_new (g_intern_static_string ("invalidate"),
                                      G_TYPE_FROM_CLASS (object_class),
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 0);
}

/**
 * gdk_monitor_get_display:
 * @monitor: a `GdkMonitor`
 *
 * Gets the display that this monitor belongs to.
 *
 * Returns: (transfer none): the display
 */
GdkDisplay *
gdk_monitor_get_display (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->display;
}

/**
 * gdk_monitor_get_geometry:
 * @monitor: a `GdkMonitor`
 * @geometry: (out): a `GdkRectangle` to be filled with the monitor geometry
 *
 * Retrieves the size and position of the monitor within the
 * display coordinate space.
 *
 * The returned geometry is in  ”application pixels”, not in
 * ”device pixels” (see [method@Gdk.Monitor.get_scale]).
 */
void
gdk_monitor_get_geometry (GdkMonitor   *monitor,
                          GdkRectangle *geometry)
{
  g_return_if_fail (GDK_IS_MONITOR (monitor));
  g_return_if_fail (geometry != NULL);

  *geometry = monitor->geometry;
}

/**
 * gdk_monitor_get_width_mm:
 * @monitor: a `GdkMonitor`
 *
 * Gets the width in millimeters of the monitor.
 *
 * Returns: the physical width of the monitor
 */
int
gdk_monitor_get_width_mm (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), 0);

  return monitor->width_mm;
}

/**
 * gdk_monitor_get_height_mm:
 * @monitor: a `GdkMonitor`
 *
 * Gets the height in millimeters of the monitor.
 *
 * Returns: the physical height of the monitor
 */
int
gdk_monitor_get_height_mm (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), 0);

  return monitor->height_mm;
}

/**
 * gdk_monitor_get_connector:
 * @monitor: a `GdkMonitor`
 *
 * Gets the name of the monitor's connector, if available.
 *
 * These are strings such as "eDP-1", or "HDMI-2". They depend
 * on software and hardware configuration, and should not be
 * relied on as stable identifiers of a specific monitor.
 *
 * Returns: (transfer none) (nullable): the name of the connector
 */
const char *
gdk_monitor_get_connector (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->connector;
}

/**
 * gdk_monitor_get_manufacturer:
 * @monitor: a `GdkMonitor`
 *
 * Gets the name or PNP ID of the monitor's manufacturer.
 *
 * Note that this value might also vary depending on actual
 * display backend.
 *
 * The PNP ID registry is located at
 * [https://uefi.org/pnp_id_list](https://uefi.org/pnp_id_list).
 *
 * Returns: (transfer none) (nullable): the name of the manufacturer
 */
const char *
gdk_monitor_get_manufacturer (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->manufacturer;
}

/**
 * gdk_monitor_get_model:
 * @monitor: a `GdkMonitor`
 *
 * Gets the string identifying the monitor model, if available.
 *
 * Returns: (transfer none) (nullable): the monitor model
 */
const char *
gdk_monitor_get_model (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->model;
}

/**
 * gdk_monitor_get_scale_factor:
 * @monitor: a `GdkMonitor`
 *
 * Gets the internal scale factor that maps from monitor coordinates
 * to device pixels.
 *
 * On traditional systems this is 1, but on very high density outputs
 * it can be a higher value (often 2).
 *
 * This can be used if you want to create pixel based data for a
 * particular monitor, but most of the time you’re drawing to a surface
 * where it is better to use [method@Gdk.Surface.get_scale_factor] instead.
 *
 * Returns: the scale factor
 */
int
gdk_monitor_get_scale_factor (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), 1);

  return monitor->scale_factor;
}

/**
 * gdk_monitor_get_scale:
 * @monitor: a `GdkMonitor`
 *
 * Gets the internal scale factor that maps from monitor coordinates
 * to device pixels.
 *
 * This can be used if you want to create pixel based data for a
 * particular monitor, but most of the time you’re drawing to a surface
 * where it is better to use [method@Gdk.Surface.get_scale] instead.
 *
 * Returns: the scale
 *
 * Since: 4.14
 */
double
gdk_monitor_get_scale (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), 1.);

  return monitor->scale;
}

/**
 * gdk_monitor_get_refresh_rate:
 * @monitor: a `GdkMonitor`
 *
 * Gets the refresh rate of the monitor, if available.
 *
 * The value is in milli-Hertz, so a refresh rate of 60Hz
 * is returned as 60000.
 *
 * Returns: the refresh rate in milli-Hertz, or 0
 */
int
gdk_monitor_get_refresh_rate (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), 0);

  return monitor->refresh_rate;
}

/**
 * gdk_monitor_get_subpixel_layout:
 * @monitor: a `GdkMonitor`
 *
 * Gets information about the layout of red, green and blue
 * primaries for pixels.
 *
 * Returns: the subpixel layout
 */
GdkSubpixelLayout
gdk_monitor_get_subpixel_layout (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), GDK_SUBPIXEL_LAYOUT_UNKNOWN);

  return monitor->subpixel_layout;
}

GdkMonitor *
gdk_monitor_new (GdkDisplay *display)
{
  return GDK_MONITOR (g_object_new (GDK_TYPE_MONITOR,
                                    "display", display,
                                    NULL));
}

void
gdk_monitor_set_manufacturer (GdkMonitor *monitor,
                              const char *manufacturer)
{
  g_free (monitor->manufacturer);
  monitor->manufacturer = g_strdup (manufacturer);

  g_object_notify (G_OBJECT (monitor), "manufacturer");
}

void
gdk_monitor_set_model (GdkMonitor *monitor,
                       const char *model)
{
  g_free (monitor->model);
  monitor->model = g_strdup (model);

  g_object_notify (G_OBJECT (monitor), "model");
}

void
gdk_monitor_set_connector (GdkMonitor *monitor,
                           const char *connector)
{
  g_free (monitor->connector);
  monitor->connector = g_strdup (connector);

  g_object_notify (G_OBJECT (monitor), "connector");
}

void
gdk_monitor_set_geometry (GdkMonitor *monitor,
                          const GdkRectangle *geometry)
{
  if (gdk_rectangle_equal (&monitor->geometry, geometry))
    return;

  monitor->geometry = *geometry;
  g_object_notify (G_OBJECT (monitor), "geometry");
}

void
gdk_monitor_set_physical_size (GdkMonitor *monitor,
                               int         width_mm,
                               int         height_mm)
{
  g_object_freeze_notify (G_OBJECT (monitor));

  if (monitor->width_mm != width_mm)
    {
      monitor->width_mm = width_mm;
      g_object_notify (G_OBJECT (monitor), "width-mm");
    }

  if (monitor->height_mm != height_mm)
    {
      monitor->height_mm = height_mm;
      g_object_notify (G_OBJECT (monitor), "height-mm");
    }

  g_object_thaw_notify (G_OBJECT (monitor));
}

void
gdk_monitor_set_scale_factor (GdkMonitor *monitor,
                              int         scale_factor)
{
  g_return_if_fail (scale_factor >= 1);

  if (monitor->scale_set)
    return;

  if (monitor->scale_factor == scale_factor)
    return;

  monitor->scale_factor = scale_factor;
  monitor->scale = scale_factor;

  g_object_notify (G_OBJECT (monitor), "scale-factor");
  g_object_notify (G_OBJECT (monitor), "scale");
}

void
gdk_monitor_set_scale (GdkMonitor *monitor,
                       double      scale)
{
  g_return_if_fail (scale > 0.);

  monitor->scale_set = TRUE;

  if (monitor->scale == scale)
    return;

  monitor->scale = scale;
  monitor->scale_factor = (int) ceil (scale);

  g_object_notify (G_OBJECT (monitor), "scale");
  g_object_notify (G_OBJECT (monitor), "scale-factor");
}

void
gdk_monitor_set_refresh_rate (GdkMonitor *monitor,
                              int         refresh_rate)
{
  if (monitor->refresh_rate == refresh_rate)
    return;

  monitor->refresh_rate = refresh_rate;

  g_object_notify (G_OBJECT (monitor), "refresh-rate");
}

void
gdk_monitor_set_subpixel_layout (GdkMonitor        *monitor,
                                 GdkSubpixelLayout  subpixel_layout)
{
  if (monitor->subpixel_layout == subpixel_layout)
    return;

  monitor->subpixel_layout = subpixel_layout;

  g_object_notify (G_OBJECT (monitor), "subpixel-layout");
}

void
gdk_monitor_invalidate (GdkMonitor *monitor)
{
  monitor->valid = FALSE;
  g_object_notify (G_OBJECT (monitor), "valid");
  g_signal_emit (monitor, gdk_monitor_signals[GDK_MONITOR_INVALIDATE], 0);
}

/**
 * gdk_monitor_is_valid: (get-property valid)
 * @monitor: a `GdkMonitor`
 *
 * Returns %TRUE if the @monitor object corresponds to a
 * physical monitor.
 *
 * The @monitor becomes invalid when the physical monitor
 * is unplugged or removed.
 *
 * Returns: %TRUE if the object corresponds to a physical monitor
 */
gboolean
gdk_monitor_is_valid (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), FALSE);

  return monitor->valid;
}

/**
 * gdk_monitor_get_description:
 * @monitor: a `GdkMonitor`
 *
 * Gets a string describing the monitor, if available.
 *
 * This can be used to identify a monitor in the UI.
 *
 * Returns: (transfer none) (nullable): the monitor description
 *
 * Since: 4.10
 */
const char *
gdk_monitor_get_description (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->description;
}

void
gdk_monitor_set_description (GdkMonitor *monitor,
                             const char *description)
{
  g_free (monitor->description);
  monitor->description = g_strdup (description);
  g_object_notify_by_pspec (G_OBJECT (monitor), gdk_monitor_properties[GDK_MONITOR_PROP_DESCRIPTION]);
}

#define MM_PER_INCH 25.4

double
gdk_monitor_get_dpi (GdkMonitor *monitor)
{
  return MAX ((monitor->geometry.width * monitor->scale) / (monitor->width_mm / MM_PER_INCH),
              (monitor->geometry.height * monitor->scale) / (monitor->height_mm / MM_PER_INCH));
}
