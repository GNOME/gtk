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

#include <gdk/gdkwindow.h>
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
               (* ref_cairo_surface)    (GdkSurface       *window);
  cairo_surface_t *
               (* create_similar_image_surface) (GdkSurface *     window,
                                                 cairo_format_t  format,
                                                 int             width,
                                                 int             height);

  void         (* show)                 (GdkSurface       *window,
					 gboolean         already_mapped);
  void         (* hide)                 (GdkSurface       *window);
  void         (* withdraw)             (GdkSurface       *window);
  void         (* raise)                (GdkSurface       *window);
  void         (* lower)                (GdkSurface       *window);
  void         (* restack_toplevel)     (GdkSurface       *window,
					 GdkSurface       *sibling,
					 gboolean        above);

  void         (* move_resize)          (GdkSurface       *window,
                                         gboolean         with_move,
                                         gint             x,
                                         gint             y,
                                         gint             width,
                                         gint             height);
  void         (* move_to_rect)         (GdkSurface       *window,
                                         const GdkRectangle *rect,
                                         GdkGravity       rect_anchor,
                                         GdkGravity       window_anchor,
                                         GdkAnchorHints   anchor_hints,
                                         gint             rect_anchor_dx,
                                         gint             rect_anchor_dy);

  GdkEventMask (* get_events)           (GdkSurface       *window);
  void         (* set_events)           (GdkSurface       *window,
                                         GdkEventMask     event_mask);

  void         (* get_geometry)         (GdkSurface       *window,
                                         gint            *x,
                                         gint            *y,
                                         gint            *width,
                                         gint            *height);
  void         (* get_root_coords)      (GdkSurface       *window,
					 gint             x,
					 gint             y,
                                         gint            *root_x,
                                         gint            *root_y);
  gboolean     (* get_device_state)     (GdkSurface       *window,
                                         GdkDevice       *device,
                                         gdouble         *x,
                                         gdouble         *y,
                                         GdkModifierType *mask);
  gboolean    (* begin_paint)           (GdkSurface       *window);
  void        (* end_paint)             (GdkSurface       *window);

  void         (* shape_combine_region) (GdkSurface       *window,
                                         const cairo_region_t *shape_region,
                                         gint             offset_x,
                                         gint             offset_y);
  void         (* input_shape_combine_region) (GdkSurface       *window,
					       const cairo_region_t *shape_region,
					       gint             offset_x,
					       gint             offset_y);

  /* Called before processing updates for a window. This gives the windowing
   * layer a chance to save the region for later use in avoiding duplicate
   * exposes.
   */
  void     (* queue_antiexpose)     (GdkSurface       *window,
                                     cairo_region_t  *update_area);

/* Called to do the windowing system specific part of gdk_surface_destroy(),
 *
 * window: The window being destroyed
 * recursing: If TRUE, then this is being called because a parent
 *     was destroyed. This generally means that the call to the windowing
 *     system to destroy the window can be omitted, since it will be
 *     destroyed as a result of the parent being destroyed.
 *     Unless @foreign_destroy
 * foreign_destroy: If TRUE, the window or a parent was destroyed by some
 *     external agency. The window has already been destroyed and no
 *     windowing system calls should be made. (This may never happen
 *     for some windowing systems.)
 */
  void         (* destroy)              (GdkSurface       *window,
					 gboolean         recursing,
					 gboolean         foreign_destroy);


  /* optional */
  gboolean     (* beep)                 (GdkSurface       *window);

  void         (* focus)                (GdkSurface       *window,
					 guint32          timestamp);
  void         (* set_type_hint)        (GdkSurface       *window,
					 GdkSurfaceTypeHint hint);
  GdkSurfaceTypeHint (* get_type_hint)   (GdkSurface       *window);
  void         (* set_modal_hint)       (GdkSurface *window,
					 gboolean   modal);
  void         (* set_skip_taskbar_hint) (GdkSurface *window,
					  gboolean   skips_taskbar);
  void         (* set_skip_pager_hint)  (GdkSurface *window,
					 gboolean   skips_pager);
  void         (* set_urgency_hint)     (GdkSurface *window,
					 gboolean   urgent);
  void         (* set_geometry_hints)   (GdkSurface         *window,
					 const GdkGeometry *geometry,
					 GdkSurfaceHints     geom_mask);
  void         (* set_title)            (GdkSurface   *window,
					 const gchar *title);
  void         (* set_role)             (GdkSurface   *window,
					 const gchar *role);
  void         (* set_startup_id)       (GdkSurface   *window,
					 const gchar *startup_id);
  void         (* set_transient_for)    (GdkSurface *window,
					 GdkSurface *parent);
  void         (* get_frame_extents)    (GdkSurface    *window,
					 GdkRectangle *rect);
  void         (* set_accept_focus)     (GdkSurface *window,
					 gboolean accept_focus);
  void         (* set_focus_on_map)     (GdkSurface *window,
					 gboolean focus_on_map);
  void         (* set_icon_list)        (GdkSurface *window,
					 GList     *pixbufs);
  void         (* set_icon_name)        (GdkSurface   *window,
					 const gchar *name);
  void         (* iconify)              (GdkSurface *window);
  void         (* deiconify)            (GdkSurface *window);
  void         (* stick)                (GdkSurface *window);
  void         (* unstick)              (GdkSurface *window);
  void         (* maximize)             (GdkSurface *window);
  void         (* unmaximize)           (GdkSurface *window);
  void         (* fullscreen)           (GdkSurface *window);
  void         (* fullscreen_on_monitor) (GdkSurface  *window,
                                          GdkMonitor *monitor);
  void         (* apply_fullscreen_mode) (GdkSurface *window);
  void         (* unfullscreen)         (GdkSurface *window);
  void         (* set_keep_above)       (GdkSurface *window,
					 gboolean   setting);
  void         (* set_keep_below)       (GdkSurface *window,
					 gboolean   setting);
  GdkSurface *  (* get_group)            (GdkSurface *window);
  void         (* set_group)            (GdkSurface *window,
					 GdkSurface *leader);
  void         (* set_decorations)      (GdkSurface      *window,
					 GdkWMDecoration decorations);
  gboolean     (* get_decorations)      (GdkSurface       *window,
					 GdkWMDecoration *decorations);
  void         (* set_functions)        (GdkSurface    *window,
					 GdkWMFunction functions);
  void         (* begin_resize_drag)    (GdkSurface     *window,
                                         GdkSurfaceEdge  edge,
                                         GdkDevice     *device,
                                         gint           button,
                                         gint           root_x,
                                         gint           root_y,
                                         guint32        timestamp);
  void         (* begin_move_drag)      (GdkSurface *window,
                                         GdkDevice     *device,
                                         gint       button,
                                         gint       root_x,
                                         gint       root_y,
                                         guint32    timestamp);
  void         (* enable_synchronized_configure) (GdkSurface *window);
  void         (* configure_finished)   (GdkSurface *window);
  void         (* set_opacity)          (GdkSurface *window,
					 gdouble    opacity);
  void         (* destroy_notify)       (GdkSurface *window);
  void         (* register_dnd)         (GdkSurface *window);
  GdkDragContext * (*drag_begin)        (GdkSurface        *window,
                                         GdkDevice        *device,
                                         GdkContentProvider*content,
                                         GdkDragAction     actions,
                                         gint              dx,
                                         gint              dy);

  void         (*process_updates_recurse) (GdkSurface      *window,
                                           cairo_region_t *region);

  gint         (* get_scale_factor)       (GdkSurface      *window);
  void         (* get_unscaled_size)      (GdkSurface      *window,
                                           int            *unscaled_width,
                                           int            *unscaled_height);

  void         (* set_opaque_region)      (GdkSurface      *window,
                                           cairo_region_t *region);
  void         (* set_shadow_width)       (GdkSurface      *window,
                                           gint            left,
                                           gint            right,
                                           gint            top,
                                           gint            bottom);
  gboolean     (* show_window_menu)       (GdkSurface      *window,
                                           GdkEvent       *event);
  GdkGLContext *(*create_gl_context)      (GdkSurface      *window,
					   gboolean        attached,
                                           GdkGLContext   *share,
                                           GError        **error);
  gboolean     (* supports_edge_constraints)(GdkSurface    *window);
};

/* Interface Functions */
GType gdk_surface_impl_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_SURFACE_IMPL_H__ */
