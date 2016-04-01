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

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_MANUFACTURER,
  PROP_MODEL,
  PROP_SCALE,
  PROP_GEOMETRY,
  PROP_WIDTH,
  PROP_HEIGHT,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GdkMonitor, gdk_monitor, G_TYPE_OBJECT)

static void
gdk_monitor_init (GdkMonitor *monitor)
{
  monitor->scale = 1;
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
    case PROP_DISPLAY:
      g_value_set_object (value, monitor->display);
      break;

    case PROP_MANUFACTURER:
      g_value_set_string (value, monitor->manufacturer);
      break;

    case PROP_MODEL:
      g_value_set_string (value, monitor->model);
      break;

    case PROP_SCALE:
      g_value_set_int (value, monitor->scale);
      break;

    case PROP_GEOMETRY:
      g_value_set_boxed (value, &monitor->geometry);
      break;

    case PROP_WIDTH:
      g_value_set_int (value, monitor->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, monitor->height);
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
    case PROP_DISPLAY:
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

  props[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "The display of the monitor",
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READABLE|G_PARAM_CONSTRUCT_ONLY);
  props[PROP_MANUFACTURER] =
    g_param_spec_string ("manufacturer",
                         "Manufacturer",
                         "The manufacturer name",
                         NULL,
                         G_PARAM_READABLE);
  props[PROP_MODEL] =
    g_param_spec_string ("model",
                         "Model",
                         "The model name",
                         NULL,
                         G_PARAM_READABLE);
  props[PROP_SCALE] =
    g_param_spec_int ("scale",
                      "Scale",
                      "The scale factor",
                      0, G_MAXINT,
                      1,
                      G_PARAM_READABLE);
  props[PROP_GEOMETRY] =
    g_param_spec_boxed ("geometry",
                        "Geometry",
                        "The geometry of the monitor",
                        GDK_TYPE_RECTANGLE,
                        G_PARAM_READABLE);
  props[PROP_WIDTH] =
    g_param_spec_int ("width",
                      "Width",
                      "The width of the monitor, in millimeters",
                      0, G_MAXINT,
                      0,
                      G_PARAM_READABLE);
  props[PROP_HEIGHT] =
    g_param_spec_int ("height",
                      "Height",
                      "The height of the monitor, in millimeters",
                      0, G_MAXINT,
                      0,
                      G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

GdkDisplay *
gdk_monitor_get_display (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->display;
}

void
gdk_monitor_get_geometry (GdkMonitor   *monitor,
                          GdkRectangle *geometry)
{
  g_return_if_fail (GDK_IS_MONITOR (monitor));
  g_return_if_fail (geometry != NULL);

  *geometry = monitor->geometry;
}

void
gdk_monitor_get_size (GdkMonitor *monitor,
                      int        *width_mm,
                      int        *height_mm)
{
  g_return_if_fail (GDK_IS_MONITOR (monitor));

  *width_mm = monitor->width;
  *height_mm = monitor->height;
}

const char *
gdk_monitor_get_manufacturer (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->manufacturer;
}

const char *
gdk_monitor_get_model (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

  return monitor->model;
}

int
gdk_monitor_get_scale (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_MONITOR (monitor), 0);

  return monitor->scale;
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
gdk_monitor_set_geometry (GdkMonitor *monitor,
                          int         x,
                          int         y,
                          int         width,
                          int         height)
{
  monitor->geometry.x = x;
  monitor->geometry.y = y;
  monitor->geometry.height = height;
  monitor->geometry.width = width;

  g_object_notify (G_OBJECT (monitor), "geometry");
}

void
gdk_monitor_set_size (GdkMonitor *monitor,
                      int         width_mm,
                      int         height_mm)
{
  monitor->width = width_mm;
  monitor->height = height_mm;

  g_object_notify (G_OBJECT (monitor), "width");
  g_object_notify (G_OBJECT (monitor), "height");
}

void
gdk_monitor_set_scale (GdkMonitor *monitor,
                       int         scale)
{
  monitor->scale = scale;

  g_object_notify (G_OBJECT (monitor), "scale");
}
