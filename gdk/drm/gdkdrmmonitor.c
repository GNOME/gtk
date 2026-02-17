/* gdkdrmmonitor.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include <string.h>

#include <gdk/gdk.h>

#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmsurface-private.h"

struct _GdkDrmMonitorClass
{
  GdkMonitorClass parent_class;
};

G_DEFINE_FINAL_TYPE (GdkDrmMonitor, gdk_drm_monitor, GDK_TYPE_MONITOR)

static void
gdk_drm_monitor_dispose (GObject *object)
{
  G_OBJECT_CLASS (gdk_drm_monitor_parent_class)->dispose (object);
}

static void
gdk_drm_monitor_class_init (GdkDrmMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_drm_monitor_dispose;
}

static void
gdk_drm_monitor_init (GdkDrmMonitor *self)
{
}

GdkDrmMonitor *
_gdk_drm_monitor_new (GdkDrmDisplay           *display,
                      const GdkRectangle      *geometry,
                      guint32                  connector_id,
                      guint32                  crtc_id,
                      const drmModeModeInfo   *mode)
{
  GdkDrmMonitor *self;
  char *connector_name;

  g_return_val_if_fail (GDK_IS_DRM_DISPLAY (display), NULL);
  g_return_val_if_fail (geometry != NULL, NULL);

  connector_name = g_strdup_printf ("DRM-%u", connector_id);
  self = g_object_new (GDK_TYPE_DRM_MONITOR,
                       "display", display,
                       NULL);
  gdk_monitor_set_connector (GDK_MONITOR (self), connector_name);
  gdk_monitor_set_geometry (GDK_MONITOR (self), geometry);
  gdk_monitor_set_physical_size (GDK_MONITOR (self), 0, 0);
  if (mode && mode->vrefresh)
    gdk_monitor_set_refresh_rate (GDK_MONITOR (self), mode->vrefresh);
  else
    gdk_monitor_set_refresh_rate (GDK_MONITOR (self), 60000);
  g_free (connector_name);

  self->connector_id = connector_id;
  self->crtc_id = crtc_id;
  if (mode)
    self->mode = *mode;
  else
    memset (&self->mode, 0, sizeof (self->mode));

  return g_steal_pointer (&self);
}

guint32
_gdk_drm_monitor_get_crtc_id (GdkDrmMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_DRM_MONITOR (monitor), 0);
  return monitor->crtc_id;
}

guint32
_gdk_drm_monitor_get_connector_id (GdkDrmMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_DRM_MONITOR (monitor), 0);
  return monitor->connector_id;
}

const drmModeModeInfo *
_gdk_drm_monitor_get_mode (GdkDrmMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_DRM_MONITOR (monitor), NULL);
  return &monitor->mode;
}

void
_gdk_drm_monitor_get_workarea (GdkMonitor   *monitor,
                               GdkRectangle *workarea)
{
  gdk_monitor_get_geometry (monitor, workarea);
}
