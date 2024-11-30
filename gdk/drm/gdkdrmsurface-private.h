/* gdkdrmsurface-private.h
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

#include "gdksurfaceprivate.h"

#include "gdkdrmdisplay.h"
#include "gdkdrmsurface.h"

G_BEGIN_DECLS

#define GDK_DRM_SURFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRM_SURFACE, GdkDrmSurfaceClass))
#define GDK_IS_DRM_SURFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRM_SURFACE))
#define GDK_DRM_SURFACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRM_SURFACE, GdkDrmSurfaceClass))

struct _GdkDrmSurface
{
  GdkSurface parent_instance;
  int root_x;
  int root_y;
};

struct _GdkDrmSurfaceClass
{
  GdkSurfaceClass parent_class;
};

void        _gdk_drm_surface_move             (GdkDrmSurface *surface,
                                               float          x,
                                               float          y);
void        _gdk_drm_surface_move_resize      (GdkDrmSurface *surface,
                                               float          x,
                                               float          y,
                                               float          width,
                                               float          height);
void        _gdk_drm_surface_get_root_coords  (GdkDrmSurface *self,
                                               int           *x,
                                               int           *y);
GdkMonitor *_gdk_drm_surface_get_best_monitor (GdkDrmSurface *self);
void        _gdk_drm_surface_show             (GdkDrmSurface *surface);

G_END_DECLS
