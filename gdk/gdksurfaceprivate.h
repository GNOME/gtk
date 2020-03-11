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
#include "gdkenumtypes.h"
#include "gdksurface.h"

G_BEGIN_DECLS

typedef enum
{
  GDK_SURFACE_TOPLEVEL,
  GDK_SURFACE_TEMP,
  GDK_SURFACE_POPUP
} GdkSurfaceType;

struct _GdkSurface
{
  GObject parent_instance;

  GdkDisplay *display;

  GdkSurface *transient_for; /* for toplevels */
  GdkSurface *parent;        /* for popups */
  GList *children;           /* popups */

  gpointer widget;

  gint x;
  gint y;

  guint8 resize_count;

  GdkGLContext *gl_paint_context;

  cairo_region_t *update_area;
  guint update_freeze_count;
  gboolean pending_schedule_update;
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
  guint autohide : 1;

  struct {
    GdkGravity surface_anchor;
    GdkGravity rect_anchor;
  } popup;

  guint update_and_descendants_freeze_count;

  gint width, height;
  gint shadow_top;
  gint shadow_left;
  gint shadow_right;
  gint shadow_bottom;

  GdkCursor *cursor;
  GHashTable *device_cursor;

  cairo_region_t *input_region;

  GList *devices_inside;

  GdkFrameClock *frame_clock; /* NULL to use from parent or default */

  GSList *draw_contexts;
  GdkDrawContext *paint_context;

  cairo_region_t *opaque_region;
};

struct _GdkSurfaceClass
{
  GObjectClass parent_class;

  cairo_surface_t *
               (* ref_cairo_surface)    (GdkSurface       *surface);

  void         (* hide)                 (GdkSurface       *surface);
  void         (* get_geometry)         (GdkSurface       *surface,
                                         gint            *x,
                                         gint            *y,
                                         gint            *width,
                                         gint            *height);
  void         (* get_root_coords)      (GdkSurface       *surface,
                                         gint             x,
                                         gint             y,
                                         gint            *root_x,
                                         gint            *root_y);
  gboolean     (* get_device_state)     (GdkSurface       *surface,
                                         GdkDevice       *device,
                                         gdouble         *x,
                                         gdouble         *y,
                                         GdkModifierType *mask);

  void         (* set_input_region)     (GdkSurface      *surface,
                                         cairo_region_t  *shape_region);

  /* Called to do the windowing system specific part of gdk_surface_destroy(),
   *
   * surface: The window being destroyed
   * foreign_destroy: If TRUE, the surface or a parent was destroyed by some
   *     external agency. The surface has already been destroyed and no
   *     windowing system calls should be made. (This may never happen
   *     for some windowing systems.)
   */
  void         (* destroy)              (GdkSurface       *surface,
                                         gboolean         foreign_destroy);


  /* optional */
  gboolean     (* beep)                 (GdkSurface       *surface);

  void         (* apply_fullscreen_mode) (GdkSurface *surface);
  void         (* begin_resize_drag)    (GdkSurface     *surface,
                                         GdkSurfaceEdge  edge,
                                         GdkDevice     *device,
                                         gint           button,
                                         gint           root_x,
                                         gint           root_y,
                                         guint32        timestamp);
  void         (* begin_move_drag)      (GdkSurface *surface,
                                         GdkDevice     *device,
                                         gint       button,
                                         gint       root_x,
                                         gint       root_y,
                                         guint32    timestamp);
  void         (* destroy_notify)       (GdkSurface *surface);
  GdkDrag *    (* drag_begin)           (GdkSurface        *surface,
                                         GdkDevice        *device,
                                         GdkContentProvider*content,
                                         GdkDragAction     actions,
                                         gint              dx,
                                         gint              dy);

  gint         (* get_scale_factor)       (GdkSurface      *surface);
  void         (* get_unscaled_size)      (GdkSurface      *surface,
                                           int            *unscaled_width,
                                           int            *unscaled_height);

  void         (* set_opaque_region)      (GdkSurface      *surface,
                                           cairo_region_t *region);
  void         (* set_shadow_width)       (GdkSurface      *surface,
                                           gint            left,
                                           gint            right,
                                           gint            top,
                                           gint            bottom);
  GdkGLContext *(*create_gl_context)      (GdkSurface      *surface,
                                           gboolean        attached,
                                           GdkGLContext   *share,
                                           GError        **error);
  gboolean     (* supports_edge_constraints)(GdkSurface    *surface);
};

void gdk_surface_set_state (GdkSurface      *surface,
                            GdkSurfaceState  new_state);

void gdk_surface_layout_popup_helper (GdkSurface     *surface,
                                      int             width,
                                      int             height,
                                      GdkPopupLayout *layout,
                                      GdkRectangle   *out_final_rect);

static inline GdkGravity
gdk_gravity_flip_horizontally (GdkGravity anchor)
{
  switch (anchor)
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_NORTH_EAST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_SOUTH_WEST;
    }

  g_assert_not_reached ();
}

static inline GdkGravity
gdk_gravity_flip_vertically (GdkGravity anchor)
{
  switch (anchor)
    {
    default:
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_SOUTH_WEST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_NORTH_EAST;
    }

  g_assert_not_reached ();
}

G_END_DECLS

#endif /* __GDK_SURFACE_PRIVATE_H__ */
