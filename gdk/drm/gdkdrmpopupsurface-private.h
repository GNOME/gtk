/* gdkdrmpopupsurface-private.h
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

#include "gdkdrmsurface-private.h"

G_BEGIN_DECLS

typedef struct _GdkDrmPopupSurface      GdkDrmPopupSurface;
typedef struct _GdkDrmPopupSurfaceClass GdkDrmPopupSurfaceClass;

#define GDK_TYPE_DRM_POPUP_SURFACE       (_gdk_drm_popup_surface_get_type())
#define GDK_DRM_POPUP_SURFACE(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRM_POPUP_SURFACE, GdkDrmPopupSurface))
#define GDK_IS_DRM_POPUP_SURFACE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRM_POPUP_SURFACE))

GType          _gdk_drm_popup_surface_get_type (void);
GdkDrmSurface *_gdk_drm_popup_surface_new      (GdkDrmDisplay *display,
                                                GdkSurface    *parent,
                                                GdkFrameClock *frame_clock);

G_END_DECLS
