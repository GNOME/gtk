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

static GdkSurfaceState
gdk_toplevel_default_get_state (GdkToplevel *toplevel)
{
  return 0;
}

static void
gdk_toplevel_default_set_title (GdkToplevel *toplevel,
                                const char  *title)
{
}

static void
gdk_toplevel_default_set_startup_id (GdkToplevel *toplevel,
                                     const char  *startup_id)
{
}

static void
gdk_toplevel_default_set_transient_for (GdkToplevel *toplevel,
                                        GdkSurface  *parent)
{
}

static void
gdk_toplevel_default_set_icon_list (GdkToplevel *toplevel,
                                    GList       *surfaces)
{
}

static void
gdk_toplevel_default_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_toplevel_default_present;
  iface->get_state = gdk_toplevel_default_get_state;
  iface->set_title = gdk_toplevel_default_set_title;
  iface->set_startup_id = gdk_toplevel_default_set_startup_id;
  iface->set_transient_for = gdk_toplevel_default_set_transient_for;
  iface->set_icon_list = gdk_toplevel_default_set_icon_list;
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


GdkSurfaceState
gdk_toplevel_get_state (GdkToplevel *toplevel)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), 0);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->get_state (toplevel);
}

/**
 * gdk_toplevel_set_title:
 * @toplevel: a #GdkToplevel
 * @title: title of @surface
 *
 * Sets the title of a toplevel surface, to be displayed in the titlebar,
 * in lists of windows, etc.
 */
void
gdk_toplevel_set_title (GdkToplevel *toplevel,
                        const char  *title)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));
 
  GDK_TOPLEVEL_GET_IFACE (toplevel)->set_title (toplevel, title); 
}

/**
 * gdk_toplevel_set_startup_id:
 * @toplevel: a #GdkToplevel
 * @startup_id: a string with startup-notification identifier
 *
 * When using GTK, typically you should use gtk_window_set_startup_id()
 * instead of this low-level function.
 */
void
gdk_toplevel_set_startup_id (GdkToplevel *toplevel,
                             const char  *startup_id)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  GDK_TOPLEVEL_GET_IFACE (toplevel)->set_startup_id (toplevel, startup_id); 
}

/**
 * gdk_toplevel_set_transient_for:
 * @toplevel: a #GdkToplevel
 * @parent: another toplevel #GdkSurface
 *
 * Indicates to the window manager that @surface is a transient dialog
 * associated with the application surface @parent. This allows the
 * window manager to do things like center @surface on @parent and
 * keep @surface above @parent.
 *
 * See gtk_window_set_transient_for() if you’re using #GtkWindow or
 * #GtkDialog.
 */
void
gdk_toplevel_set_transient_for (GdkToplevel *toplevel,
                                GdkSurface  *parent)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  GDK_TOPLEVEL_GET_IFACE (toplevel)->set_transient_for (toplevel, parent); 
}


/**
 * gdk_toplevel_set_icon_list:
 * @toplevel: a #GdkToplevel
 * @surfaces: (transfer none) (element-type GdkTexture):
 *     A list of textures to use as icon, of different sizes
 *
 * Sets a list of icons for the surface.
 *
 * One of these will be used to represent the surface in iconic form.
 * The icon may be shown in window lists or task bars. Which icon
 * size is shown depends on the window manager. The window manager
 * can scale the icon but setting several size icons can give better
 * image quality.
 *
 * Note that some platforms don't support surface icons.
 */
void
gdk_toplevel_set_icon_list (GdkToplevel *toplevel,
                            GList       *surfaces)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  GDK_TOPLEVEL_GET_IFACE (toplevel)->set_icon_list (toplevel, surfaces); 
}
