/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_SURFACE_IMPL_H__
#define __GDK_SURFACE_IMPL_H__

#include <gdk/gdksurface.h>
#include <gdk/gdkproperty.h>

G_BEGIN_DECLS

#define GDK_TYPE_SURFACE_IMPL           (gdk_surface_impl_get_type ())
#define GDK_SURFACE_IMPL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SURFACE_IMPL, GdkSurfaceImpl))
#define GDK_SURFACE_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SURFACE_IMPL, GdkSurfaceImplClass))
#define GDK_IS_SURFACE_IMPL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SURFACE_IMPL))
#define GDK_IS_SURFACE_IMPL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SURFACE_IMPL))
#define GDK_SURFACE_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SURFACE_IMPL, GdkSurfaceImplClass))

typedef struct _GdkSurfaceImpl       GdkSurfaceImpl;
typedef struct _GdkSurfaceImplClass  GdkSurfaceImplClass;

struct _GdkSurfaceImpl
{
  GObject parent;
};

struct _GdkSurfaceImplClass
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

/* Interface Functions */
GType gdk_surface_impl_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_SURFACE_IMPL_H__ */
