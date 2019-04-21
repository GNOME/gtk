/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2019 Red Hat, Inc.
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

/* Uninstalled header defining types and functions internal to GDK */

#ifndef __GDK_SURFACE_PRIVATE_H__
#define __GDK_SURFACE_PRIVATE_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdksurfaceimpl.h"
#include "gdkenumtypes.h"
#include "gdksurface.h"

G_BEGIN_DECLS

struct _GdkSurface
{
  GObject parent_instance;

  GdkDisplay *display;

  GdkSurfaceImpl *impl; /* window-system-specific delegate object */

  GdkSurface *transient_for;

  gpointer widget;

  gint x;
  gint y;

  guint8 surface_type;

  guint8 resize_count;

  GdkGLContext *gl_paint_context;

  cairo_region_t *update_area;
  guint update_freeze_count;
  /* This is the update_area that was in effect when the current expose
     started. It may be smaller than the expose area if we'e painting
     more than we have to, but it represents the "true" damage. */
  cairo_region_t *active_update_area;

  GdkSurfaceState old_state;
  GdkSurfaceState state;

  guint8 alpha;
  guint8 fullscreen_mode;

  guint modal_hint : 1;

  guint destroyed : 2;

  guint accept_focus : 1;
  guint focus_on_map : 1;
  guint support_multidevice : 1;
  guint viewable : 1; /* mapped and all parents mapped */
  guint in_update : 1;
  guint frame_clock_events_paused : 1;

  /* The GdkSurface that has the impl, ref:ed if another surface.
   * This ref is required to keep the wrapper of the impl surface alive
   * for as long as any GdkSurface references the impl. */
  GdkSurface *impl_surface;

  guint update_and_descendants_freeze_count;

  gint width, height;
  gint shadow_top;
  gint shadow_left;
  gint shadow_right;
  gint shadow_bottom;

  GdkCursor *cursor;
  GHashTable *device_cursor;

  cairo_region_t *input_shape;

  GList *devices_inside;

  GdkFrameClock *frame_clock; /* NULL to use from parent or default */

  GSList *draw_contexts;
  GdkDrawContext *paint_context;

  cairo_region_t *opaque_region;
};

struct _GdkSurfaceClass
{
  GObjectClass      parent_class;

  /* Padding for future expansion */
  void (*_gdk_reserved1) (void);
  void (*_gdk_reserved2) (void);
  void (*_gdk_reserved3) (void);
  void (*_gdk_reserved4) (void);
  void (*_gdk_reserved5) (void);
  void (*_gdk_reserved6) (void);
  void (*_gdk_reserved7) (void);
  void (*_gdk_reserved8) (void);
};

void gdk_surface_set_state (GdkSurface      *surface,
                            GdkSurfaceState  new_state);

G_END_DECLS

#endif /* __GDK_SURFACE_PRIVATE_H__ */
