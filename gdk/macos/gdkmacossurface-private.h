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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GDK_MACOS_SURFACE_PRIVATE_H__
#define __GDK_MACOS_SURFACE_PRIVATE_H__

#include <AppKit/AppKit.h>
#include <cairo.h>

#include "gdksurfaceprivate.h"

#include "gdkmacosdisplay.h"
#include "gdkmacossurface.h"

#import "GdkMacosWindow.h"

G_BEGIN_DECLS

#define GDK_MACOS_SURFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MACOS_SURFACE, GdkMacosSurfaceClass))
#define GDK_IS_MACOS_SURFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MACOS_SURFACE))
#define GDK_MACOS_SURFACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MACOS_SURFACE, GdkMacosSurfaceClass))

struct _GdkMacosSurface
{
  GdkSurface parent_instance;

  GList main;
  GList sorted;
  GList frame;

  GdkMacosWindow *window;
  GPtrArray *monitors;
  cairo_region_t *input_region;
  cairo_region_t *opaque_region;
  char *title;

  int root_x;
  int root_y;

  int shadow_top;
  int shadow_right;
  int shadow_bottom;
  int shadow_left;

  gint64 pending_frame_counter;

  guint did_initial_present : 1;
  guint geometry_dirty : 1;
};

struct _GdkMacosSurfaceClass
{
  GdkSurfaceClass parent_class;
};

GdkMacosSurface   *_gdk_macos_surface_new                     (GdkMacosDisplay    *display,
                                                               GdkSurfaceType      surface_type,
                                                               GdkSurface         *parent,
                                                               int                 x,
                                                               int                 y,
                                                               int                 width,
                                                               int                 height);
NSWindow          *_gdk_macos_surface_get_native              (GdkMacosSurface    *self);
CGDirectDisplayID  _gdk_macos_surface_get_screen_id           (GdkMacosSurface    *self);
const char        *_gdk_macos_surface_get_title               (GdkMacosSurface    *self);
void               _gdk_macos_surface_set_title               (GdkMacosSurface    *self,
                                                               const char         *title);
void               _gdk_macos_surface_get_shadow              (GdkMacosSurface    *self,
                                                               int                *top,
                                                               int                *right,
                                                               int                *bottom,
                                                               int                *left);
void               _gdk_macos_surface_set_shadow              (GdkMacosSurface    *self,
                                                               int                 top,
                                                               int                 right,
                                                               int                 bottom,
                                                               int                 left);
NSView            *_gdk_macos_surface_get_view                (GdkMacosSurface    *self);
gboolean           _gdk_macos_surface_get_modal_hint          (GdkMacosSurface    *self);
void               _gdk_macos_surface_set_modal_hint          (GdkMacosSurface    *self,
                                                               gboolean            modal_hint);
void               _gdk_macos_surface_set_geometry_hints      (GdkMacosSurface    *self,
                                                               const GdkGeometry  *geometry,
                                                               GdkSurfaceHints     geom_mask);
void               _gdk_macos_surface_resize                  (GdkMacosSurface    *self,
                                                               int                 width,
                                                               int                 height);
void               _gdk_macos_surface_update_fullscreen_state (GdkMacosSurface    *self);
void               _gdk_macos_surface_update_position         (GdkMacosSurface    *self);
void               _gdk_macos_surface_show                    (GdkMacosSurface    *self);
void               _gdk_macos_surface_publish_timings         (GdkMacosSurface    *self,
                                                               gint64              predicted_presentation_time,
                                                               gint64              refresh_interval);
CGContextRef       _gdk_macos_surface_acquire_context         (GdkMacosSurface    *self,
                                                               gboolean            clear_scale,
                                                               gboolean            antialias);
void               _gdk_macos_surface_release_context         (GdkMacosSurface    *self,
                                                               CGContextRef        cg_context);
void               _gdk_macos_surface_synthesize_null_key     (GdkMacosSurface    *self);
void               _gdk_macos_surface_move                    (GdkMacosSurface    *self,
                                                               int                 x,
                                                               int                 y);
void               _gdk_macos_surface_move_resize             (GdkMacosSurface    *self,
                                                               int                 x,
                                                               int                 y,
                                                               int                 width,
                                                               int                 height);
gboolean           _gdk_macos_surface_is_tracking             (GdkMacosSurface    *self,
                                                               NSTrackingArea     *area);
void               _gdk_macos_surface_monitor_changed         (GdkMacosSurface    *self);
GdkMonitor        *_gdk_macos_surface_get_best_monitor        (GdkMacosSurface    *self);
void               _gdk_macos_surface_reposition_children     (GdkMacosSurface    *self);
void               _gdk_macos_surface_set_opacity             (GdkMacosSurface    *self,
                                                               double              opacity);
void               _gdk_macos_surface_get_root_coords         (GdkMacosSurface    *self,
                                                               int                *x,
                                                               int                *y);

G_END_DECLS

#endif /* __GDK_MACOS_SURFACE_PRIVATE_H__ */
