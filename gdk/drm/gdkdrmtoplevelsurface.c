/* gdkdrmtoplevelsurface.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkdrmtoplevelsurface-private.h"

#include "gdkframeclockidleprivate.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdktoplevelsizeprivate.h"

#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmsurface-private.h"

static gboolean
_gdk_drm_toplevel_surface_compute_size (GdkSurface *surface)
{
  GdkMonitor *monitor;
  GdkRectangle monitor_geometry;
  GdkToplevelSize size;
  int bounds_width;
  int bounds_height;
  int width;
  int height;

  g_assert (GDK_IS_DRM_TOPLEVEL_SURFACE (surface));

  /* Place at 0,0 so we get the right monitor for bounds */
  _gdk_drm_surface_move_resize (GDK_DRM_SURFACE (surface), 0, 0, -1, -1);

  monitor = _gdk_drm_surface_get_best_monitor (GDK_DRM_SURFACE (surface));
  if (monitor != NULL)
    {
      gdk_monitor_get_geometry (monitor, &monitor_geometry);
      bounds_width = monitor_geometry.width;
      bounds_height = monitor_geometry.height;
    }
  else
    {
      bounds_width = G_MAXINT;
      bounds_height = G_MAXINT;
    }

  gdk_toplevel_size_init (&size, bounds_width, bounds_height);
  gdk_toplevel_notify_compute_size (GDK_TOPLEVEL (surface), &size);

  width = CLAMP (size.width, size.min_width, bounds_width);
  height = CLAMP (size.height, size.min_height, bounds_height);

  /* Ensure we have at least 1x1 to avoid zero-sized surfaces */
  width = MAX (width, 1);
  height = MAX (height, 1);

  surface->width = width;
  surface->height = height;

  _gdk_drm_surface_move_resize (GDK_DRM_SURFACE (surface), 0, 0, width, height);

  return FALSE;
}

static void
_gdk_drm_toplevel_surface_present (GdkToplevel       *toplevel,
                                   GdkToplevelLayout *layout)
{
  GdkSurface *surface = GDK_SURFACE (toplevel);

  g_assert (GDK_IS_DRM_TOPLEVEL_SURFACE (toplevel));

  gdk_surface_request_layout (surface);
  if (!GDK_SURFACE_IS_MAPPED (surface))
    {
      gdk_surface_set_is_mapped (surface, TRUE);
      _gdk_drm_surface_show (GDK_DRM_SURFACE (surface));
    }
  gdk_surface_invalidate_rect (surface, NULL);
}

static gboolean
_gdk_drm_toplevel_surface_minimize (GdkToplevel *toplevel)
{
  return FALSE;
}

static gboolean
_gdk_drm_toplevel_surface_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
_gdk_drm_toplevel_surface_focus (GdkToplevel *toplevel,
                                 guint32      timestamp)
{
}

static void
_gdk_drm_toplevel_surface_begin_resize (GdkToplevel    *toplevel,
                                        GdkSurfaceEdge  edge,
                                        GdkDevice      *device,
                                        int             button,
                                        double          root_x,
                                        double          root_y,
                                        guint32         timestamp)
{
  g_assert (GDK_IS_DRM_SURFACE (toplevel));
}

static void
_gdk_drm_toplevel_surface_begin_move (GdkToplevel *toplevel,
                                      GdkDevice   *device,
                                      int          button,
                                      double       root_x,
                                      double       root_y,
                                      guint32      timestamp)
{
  g_assert (GDK_IS_DRM_SURFACE (toplevel));
}

static void
toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = _gdk_drm_toplevel_surface_present;
  iface->minimize = _gdk_drm_toplevel_surface_minimize;
  iface->lower = _gdk_drm_toplevel_surface_lower;
  iface->focus = _gdk_drm_toplevel_surface_focus;
  iface->begin_resize = _gdk_drm_toplevel_surface_begin_resize;
  iface->begin_move = _gdk_drm_toplevel_surface_begin_move;
}

G_DEFINE_TYPE_WITH_CODE (GdkDrmToplevelSurface, _gdk_drm_toplevel_surface, GDK_TYPE_DRM_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL, toplevel_iface_init))

enum {
  PROP_0,
  LAST_PROP
};

static void
_gdk_drm_toplevel_surface_hide (GdkSurface *surface)
{
  GDK_SURFACE_CLASS (_gdk_drm_toplevel_surface_parent_class)->hide (surface);
}

static void
_gdk_drm_toplevel_surface_request_layout (GdkSurface *surface)
{
}

static void
_gdk_drm_toplevel_surface_destroy (GdkSurface *surface,
                                   gboolean    foreign_destroy)
{
  GDK_SURFACE_CLASS (_gdk_drm_toplevel_surface_parent_class)->destroy (surface, foreign_destroy);
}

static void
_gdk_drm_toplevel_surface_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gdk_drm_toplevel_surface_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gdk_drm_toplevel_surface_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkFrameClock *frame_clock;

  frame_clock = _gdk_frame_clock_idle_new ();
  gdk_surface_set_frame_clock (surface, frame_clock);
  g_object_unref (frame_clock);

  G_OBJECT_CLASS (_gdk_drm_toplevel_surface_parent_class)->constructed (object);
}

static void
_gdk_drm_toplevel_surface_class_init (GdkDrmToplevelSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = _gdk_drm_toplevel_surface_constructed;
  object_class->get_property = _gdk_drm_toplevel_surface_get_property;
  object_class->set_property = _gdk_drm_toplevel_surface_set_property;

  surface_class->destroy = _gdk_drm_toplevel_surface_destroy;
  surface_class->hide = _gdk_drm_toplevel_surface_hide;
  surface_class->compute_size = _gdk_drm_toplevel_surface_compute_size;
  surface_class->request_layout = _gdk_drm_toplevel_surface_request_layout;

  gdk_toplevel_install_properties (object_class, LAST_PROP);
}

static void
_gdk_drm_toplevel_surface_init (GdkDrmToplevelSurface *self)
{
}
