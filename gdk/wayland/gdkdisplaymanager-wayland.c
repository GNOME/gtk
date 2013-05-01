/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-wayland.c
 *
 * Copyright 2010 Red Hat, Inc.
 *
 * Author: Matthias clasen
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

#include "gdkdisplaymanagerprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkwayland.h"
#include "gdkinternals.h"

struct _GdkWaylandDisplayManager
{
  GdkDisplayManager parent;

  GSList *displays;

  gboolean init_failed;
};

struct _GdkWaylandDisplayManagerClass
{
  GdkDisplayManagerClass parent_class;
};

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWaylandDisplayManager, gdk_wayland_display_manager, GDK_TYPE_DISPLAY_MANAGER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

static gboolean
gdk_wayland_display_manager_initable_init (GInitable     *initable,
                                           GCancellable  *cancellable,
                                           GError       **error)
{
  struct wl_display *wl_display;

  /* Set by the compositor when launching a special client - and it gets reset
   * by wl_display_connect so we must avoid calling it twice
   */
  if (g_getenv ("WAYLAND_SOCKET"))
    return TRUE;

  /* check that a connection to the default display is possible */
  wl_display = wl_display_connect (gdk_get_display_arg_name ());

  if (!wl_display)
    {
      GDK_WAYLAND_DISPLAY_MANAGER (initable)->init_failed = TRUE;
      return FALSE;
    }

  wl_display_disconnect (wl_display);

  return TRUE;
}

void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = gdk_wayland_display_manager_initable_init;
}

static void
gdk_wayland_display_manager_finalize (GObject *object)
{
  if (GDK_WAYLAND_DISPLAY_MANAGER (object)->init_failed == FALSE)
    g_error ("A GdkWaylandDisplayManager object was finalized. This should not happen");

  G_OBJECT_CLASS (gdk_wayland_display_manager_parent_class)->finalize (object);
}

static void
gdk_wayland_display_manager_class_init (GdkWaylandDisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_wayland_display_manager_finalize;
}

static void
gdk_wayland_display_manager_init (GdkWaylandDisplayManager *manager)
{
}

