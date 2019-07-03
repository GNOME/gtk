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

  guint8 surface_type;

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
  GObjectClass parent_class;

  cairo_surface_t *
               (* ref_cairo_surface)    (GdkSurface       *surface);

  void         (* show)                 (GdkSurface       *surface,
                                         gboolean         already_mapped);
  void         (* hide)                 (GdkSurface       *surface);
  void         (* withdraw)             (GdkSurface       *surface);
  void         (* raise)                (GdkSurface       *surface);
  void         (* lower)                (GdkSurface       *surface);
  void         (* restack_toplevel)     (GdkSurface       *surface,
                                         GdkSurface       *sibling,
                                         gboolean        above);

  void         (* move_resize)          (GdkSurface       *surface,
                                         gboolean         with_move,
                                         gint             x,
                                         gint             y,
                                         gint             width,
                                         gint             height);
  void         (* move_to_rect)         (GdkSurface       *surface,
                                         const GdkRectangle *rect,
                                         GdkGravity       rect_anchor,
                                         GdkGravity       surface_anchor,
                                         GdkAnchorHints   anchor_hints,
                                         gint             rect_anchor_dx,
                                         gint             rect_anchor_dy);

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

  void         (* input_shape_combine_region) (GdkSurface       *surface,
                                               const cairo_region_t *shape_region,
                                               gint             offset_x,
                                               gint             offset_y);

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

  void         (* focus)                (GdkSurface       *surface,
                                         guint32          timestamp);
  void         (* set_type_hint)        (GdkSurface       *surface,
                                         GdkSurfaceTypeHint hint);
  GdkSurfaceTypeHint (* get_type_hint)   (GdkSurface       *surface);
  void         (* set_modal_hint)       (GdkSurface *surface,
                                         gboolean   modal);
  void         (* set_geometry_hints)   (GdkSurface         *surface,
                                         const GdkGeometry *geometry,
                                         GdkSurfaceHints     geom_mask);
  void         (* set_title)            (GdkSurface   *surface,
                                         const gchar *title);
  void         (* set_startup_id)       (GdkSurface   *surface,
                                         const gchar *startup_id);
  void         (* set_transient_for)    (GdkSurface *surface,
                                         GdkSurface *parent);
  void         (* get_frame_extents)    (GdkSurface    *surface,
                                         GdkRectangle *rect);
  void         (* set_accept_focus)     (GdkSurface *surface,
                                         gboolean accept_focus);
  void         (* set_focus_on_map)     (GdkSurface *surface,
                                         gboolean focus_on_map);
  void         (* set_icon_list)        (GdkSurface *surface,
                                         GList     *pixbufs);
  void         (* set_icon_name)        (GdkSurface   *surface,
                                         const gchar *name);
  void         (* iconify)              (GdkSurface *surface);
  void         (* deiconify)            (GdkSurface *surface);
  void         (* stick)                (GdkSurface *surface);
  void         (* unstick)              (GdkSurface *surface);
  void         (* maximize)             (GdkSurface *surface);
  void         (* unmaximize)           (GdkSurface *surface);
  void         (* fullscreen)           (GdkSurface *surface);
  void         (* fullscreen_on_monitor) (GdkSurface  *surface,
                                          GdkMonitor *monitor);
  void         (* apply_fullscreen_mode) (GdkSurface *surface);
  void         (* unfullscreen)         (GdkSurface *surface);
  void         (* set_keep_above)       (GdkSurface *surface,
                                         gboolean   setting);
  void         (* set_keep_below)       (GdkSurface *surface,
                                         gboolean   setting);
  void         (* set_decorations)      (GdkSurface      *surface,
                                         GdkWMDecoration decorations);
  gboolean     (* get_decorations)      (GdkSurface       *surface,
                                         GdkWMDecoration *decorations);
  void         (* set_functions)        (GdkSurface    *surface,
                                         GdkWMFunction functions);
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
  void         (* set_opacity)          (GdkSurface *surface,
                                         gdouble    opacity);
  void         (* destroy_notify)       (GdkSurface *surface);
  void         (* register_dnd)         (GdkSurface *surface);
  GdkDrag * (*drag_begin)               (GdkSurface        *surface,
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
  gboolean     (* show_window_menu)       (GdkSurface      *surface,
                                           GdkEvent       *event);
  GdkGLContext *(*create_gl_context)      (GdkSurface      *surface,
                                           gboolean        attached,
                                           GdkGLContext   *share,
                                           GError        **error);
  gboolean     (* supports_edge_constraints)(GdkSurface    *surface);
};

void gdk_surface_set_state (GdkSurface      *surface,
                            GdkSurfaceState  new_state);

G_END_DECLS

#endif /* __GDK_SURFACE_PRIVATE_H__ */
