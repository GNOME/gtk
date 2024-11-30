/* gdkdrmmonitor-private.h
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

#pragma once

#include "gdkdrmdisplay.h"
#include "gdkdrmmonitor.h"

#include "gdkmonitorprivate.h"

#include <xf86drmMode.h>

G_BEGIN_DECLS

struct _GdkDrmMonitor
{
  GdkMonitor parent_instance;

  guint32 connector_id;
  guint32 crtc_id;
  drmModeModeInfo mode;
};

GdkDrmMonitor *_gdk_drm_monitor_new          (GdkDrmDisplay      *display,
                                              const GdkRectangle *geometry,
                                              guint32             connector_id,
                                              guint32             crtc_id,
                                              const drmModeModeInfo *mode);
void           _gdk_drm_monitor_get_workarea (GdkMonitor         *monitor,
                                              GdkRectangle       *workarea);

guint32        _gdk_drm_monitor_get_crtc_id       (GdkDrmMonitor *monitor);
guint32        _gdk_drm_monitor_get_connector_id  (GdkDrmMonitor *monitor);
const drmModeModeInfo * _gdk_drm_monitor_get_mode (GdkDrmMonitor *monitor);

G_END_DECLS
