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

#include "gdkdevicetoolprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"


G_DEFINE_TYPE (GdkDeviceTool, gdk_device_tool, G_TYPE_OBJECT)

enum {
  TOOL_PROP_0,
  TOOL_PROP_SERIAL,
  TOOL_PROP_TOOL_TYPE,
  TOOL_PROP_AXES,
  TOOL_PROP_HARDWARE_ID,
  N_TOOL_PROPS
};

GParamSpec *tool_props[N_TOOL_PROPS] = { 0 };

static void
gdk_device_tool_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdkDeviceTool *tool = GDK_DEVICE_TOOL (object);

  switch (prop_id)
    {
    case TOOL_PROP_SERIAL:
      tool->serial = g_value_get_uint64 (value);
      break;
    case TOOL_PROP_TOOL_TYPE:
      tool->type = g_value_get_enum (value);
      break;
    case TOOL_PROP_AXES:
      tool->tool_axes = g_value_get_flags (value);
      break;
    case TOOL_PROP_HARDWARE_ID:
      tool->hw_id = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_tool_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdkDeviceTool *tool = GDK_DEVICE_TOOL (object);

  switch (prop_id)
    {
    case TOOL_PROP_SERIAL:
      g_value_set_uint64 (value, tool->serial);
      break;
    case TOOL_PROP_TOOL_TYPE:
      g_value_set_enum (value, tool->type);
      break;
    case TOOL_PROP_AXES:
      g_value_set_flags (value, tool->tool_axes);
      break;
    case TOOL_PROP_HARDWARE_ID:
      g_value_set_uint64 (value, tool->hw_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_tool_class_init (GdkDeviceToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_device_tool_set_property;
  object_class->get_property = gdk_device_tool_get_property;

  tool_props[TOOL_PROP_SERIAL] = g_param_spec_uint64 ("serial",
                                                      "Serial",
                                                      "Serial number",
                                                      0, G_MAXUINT64, 0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY);
  tool_props[TOOL_PROP_TOOL_TYPE] = g_param_spec_enum ("tool-type",
                                                       "Tool type",
                                                       "Tool type",
                                                       GDK_TYPE_DEVICE_TOOL_TYPE,
                                                       GDK_DEVICE_TOOL_TYPE_UNKNOWN,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY);
  tool_props[TOOL_PROP_AXES] = g_param_spec_flags ("axes",
                                                   "Axes",
                                                   "Tool axes",
                                                   GDK_TYPE_AXIS_FLAGS, 0,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT_ONLY);
  tool_props[TOOL_PROP_HARDWARE_ID] = g_param_spec_uint64 ("hardware-id",
                                                           "Hardware ID",
                                                           "Hardware ID",
                                                           0, G_MAXUINT64, 0,
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_TOOL_PROPS, tool_props);
}

static void
gdk_device_tool_init (GdkDeviceTool *tool)
{
}

GdkDeviceTool *
gdk_device_tool_new (guint64           serial,
                     guint64           hw_id,
                     GdkDeviceToolType type,
                     GdkAxisFlags      tool_axes)
{
  return g_object_new (GDK_TYPE_DEVICE_TOOL,
                       "serial", serial,
                       "hardware-id", hw_id,
                       "tool-type", type,
                       "axes", tool_axes,
                       NULL);
}

/**
 * gdk_device_tool_get_serial:
 * @tool: a #GdkDeviceTool
 *
 * Gets the serial of this tool, this value can be used to identify a
 * physical tool (eg. a tablet pen) across program executions.
 *
 * Returns: The serial ID for this tool
 *
 * Since: 3.22
 **/
guint64
gdk_device_tool_get_serial (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, 0);

  return tool->serial;
}

/**
 * gdk_device_tool_get_hardware_id:
 * @tool: a #GdkDeviceTool
 *
 * Gets the hardware ID of this tool, or 0 if it's not known. When
 * non-zero, the identificator is unique for the given tool model,
 * meaning that two identical tools will share the same @hardware_id,
 * but will have different serial numbers (see gdk_device_tool_get_serial()).
 *
 * This is a more concrete (and device specific) method to identify
 * a #GdkDeviceTool than gdk_device_tool_get_tool_type(), as a tablet
 * may support multiple devices with the same #GdkDeviceToolType,
 * but having different hardware identificators.
 *
 * Returns: The hardware identificator of this tool.
 *
 * Since: 3.22
 **/
guint64
gdk_device_tool_get_hardware_id (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, 0);

  return tool->hw_id;
}

/**
 * gdk_device_tool_get_tool_type:
 * @tool: a #GdkDeviceTool
 *
 * Gets the #GdkDeviceToolType of the tool.
 *
 * Returns: The physical type for this tool. This can be used to figure out what
 * sort of pen is being used, such as an airbrush or a pencil.
 *
 * Since: 3.22
 **/
GdkDeviceToolType
gdk_device_tool_get_tool_type (GdkDeviceTool *tool)
{
  g_return_val_if_fail (tool != NULL, GDK_DEVICE_TOOL_TYPE_UNKNOWN);

  return tool->type;
}
