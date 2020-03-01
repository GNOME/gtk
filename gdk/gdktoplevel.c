/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gdkintl.h"
#include "gdk-private.h"
#include "gdktoplevelprivate.h"

/**
 * SECTION:gdktoplevel
 * @Short_description: Interface for toplevel surfaces
 * @Title: Toplevels
 *
 * A #GdkToplevel is a freestanding toplevel surface.
 */


/* FIXME: this can't have GdkSurface as a prerequisite
 * as long as GdkSurface implements this interface itself
 */
G_DEFINE_INTERFACE (GdkToplevel, gdk_toplevel, G_TYPE_OBJECT)

static gboolean
gdk_toplevel_default_present (GdkToplevel       *toplevel,
                              int                width,
                              int                height,
                              GdkToplevelLayout *layout)
{
  return FALSE;
}

static void
gdk_toplevel_default_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_toplevel_default_present;

  g_object_interface_install_property (iface,
      g_param_spec_flags ("state",
                          P_("State"),
                          P_("State"),
                          GDK_TYPE_SURFACE_STATE, GDK_SURFACE_STATE_WITHDRAWN,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

guint
gdk_toplevel_install_properties (GObjectClass *object_class,
                                 guint         first_prop)
{
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_STATE, "state");

  return GDK_TOPLEVEL_NUM_PROPERTIES;
}

/**
 * gdk_toplevel_present:
 * @toplevel: the #GdkToplevel to show
 * @width: the unconstrained toplevel width to layout
 * @height: the unconstrained toplevel height to layout
 * @layout: the #GdkToplevelLayout object used to layout
 *
 * Present @toplevel after having processed the #GdkToplevelLayout rules.
 * If the toplevel was previously now showing, it will be showed,
 * otherwise it will change layout according to @layout.
 *
 * Presenting may fail.
 *
 * Returns: %FALSE if @toplevel failed to be presented, otherwise %TRUE.
 */
gboolean
gdk_toplevel_present (GdkToplevel       *toplevel,
                      int                width,
                      int                height,
                      GdkToplevelLayout *layout)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);
  g_return_val_if_fail (width > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);
  g_return_val_if_fail (layout != NULL, FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->present (toplevel, width, height, layout);
}

/**
 * gdk_toplevel_get_state:
 * @toplevel: a #GdkToplevel
 *
 * Gets the bitwise OR of the currently active surface state flags,
 * from the #GdkSurfaceState enumeration.
 *
 * Returns: surface state bitfield
 */
GdkSurfaceState
gdk_toplevel_get_state (GdkToplevel *toplevel)
{
  GdkSurfaceState state;

  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), 0);

  g_object_get (toplevel, "state", &state, NULL);

  return state;
}
