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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_WINDOW_IMPL_H__
#define __GDK_WINDOW_IMPL_H__

#include <gdk/gdkwindow.h>

G_BEGIN_DECLS

#define GDK_TYPE_WINDOW_IMPL           (gdk_window_impl_get_type ())
#define GDK_WINDOW_IMPL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL, GdkWindowImpl))
#define GDK_WINDOW_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL, GdkWindowImplClass))
#define GDK_IS_WINDOW_IMPL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL))
#define GDK_IS_WINDOW_IMPL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL))
#define GDK_WINDOW_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL, GdkWindowImplClass))

typedef struct _GdkWindowImpl       GdkWindowImpl;
typedef struct _GdkWindowImplClass  GdkWindowImplClass;

struct _GdkWindowImpl
{
  GObject parent;
};

struct _GdkWindowImplClass
{
  GObjectClass parent_class;

  cairo_surface_t *
               (* ref_cairo_surface)    (GdkWindow       *window);

  void         (* show)                 (GdkWindow       *window,
					 gboolean         already_mapped);
  void         (* hide)                 (GdkWindow       *window);
  void         (* withdraw)             (GdkWindow       *window);
  void         (* raise)                (GdkWindow       *window);
  void         (* lower)                (GdkWindow       *window);
  void         (* restack_under)        (GdkWindow       *window,
					 GList           *native_siblings);
  void         (* restack_toplevel)     (GdkWindow       *window,
					 GdkWindow       *sibling,
					 gboolean        above);

  void         (* move_resize)          (GdkWindow       *window,
                                         gboolean         with_move,
                                         gint             x,
                                         gint             y,
                                         gint             width,
                                         gint             height);
  void         (* set_background)       (GdkWindow       *window,
                                         cairo_pattern_t *pattern);

  GdkEventMask (* get_events)           (GdkWindow       *window);
  void         (* set_events)           (GdkWindow       *window,
                                         GdkEventMask     event_mask);
  
  gboolean     (* reparent)             (GdkWindow       *window,
                                         GdkWindow       *new_parent,
                                         gint             x,
                                         gint             y);

  void         (* set_device_cursor)    (GdkWindow       *window,
                                         GdkDevice       *device,
                                         GdkCursor       *cursor);

  void         (* get_geometry)         (GdkWindow       *window,
                                         gint            *x,
                                         gint            *y,
                                         gint            *width,
                                         gint            *height,
                                         gint            *depth);
  gint         (* get_root_coords)      (GdkWindow       *window,
					 gint             x,
					 gint             y,
                                         gint            *root_x,
                                         gint            *root_y);
  gboolean     (* get_device_state)     (GdkWindow       *window,
                                         GdkDevice       *device,
                                         gint            *x,
                                         gint            *y,
                                         GdkModifierType *mask);

  cairo_region_t * (* get_shape)        (GdkWindow       *window);
  cairo_region_t * (* get_input_shape)  (GdkWindow       *window);
  void         (* shape_combine_region) (GdkWindow       *window,
                                         const cairo_region_t *shape_region,
                                         gint             offset_x,
                                         gint             offset_y);
  void         (* input_shape_combine_region) (GdkWindow       *window,
					       const cairo_region_t *shape_region,
					       gint             offset_x,
					       gint             offset_y);

  gboolean     (* set_static_gravities) (GdkWindow       *window,
				         gboolean         use_static);

  /* Called before processing updates for a window. This gives the windowing
   * layer a chance to save the region for later use in avoiding duplicate
   * exposes. The return value indicates whether the function has a saved
   * the region; if the result is TRUE, then the windowing layer is responsible
   * for destroying the region later.
   */
  gboolean     (* queue_antiexpose)     (GdkWindow       *window,
					 cairo_region_t  *update_area);

  /* Called to move @area inside @window by @dx x @dy pixels. @area is 
   * guaranteed to be inside @window. If part of @area is not invisible or
   * invalid, it is this function's job to queue expose events in those 
   * areas.
   */
  void         (* translate)            (GdkWindow       *window,
					 cairo_region_t  *area,
					 gint            dx,
					 gint            dy);

/* Called to do the windowing system specific part of gdk_window_destroy(),
 *
 * window: The window being destroyed
 * recursing: If TRUE, then this is being called because a parent
 *            was destroyed. This generally means that the call to the windowing system
 *            to destroy the window can be omitted, since it will be destroyed as a result
 *            of the parent being destroyed. Unless @foreign_destroy
 *            
 * foreign_destroy: If TRUE, the window or a parent was destroyed by some external 
 *            agency. The window has already been destroyed and no windowing
 *            system calls should be made. (This may never happen for some
 *            windowing systems.)
 */
  void         (* destroy)              (GdkWindow       *window,
					 gboolean         recursing,
					 gboolean         foreign_destroy);

  cairo_surface_t * (* resize_cairo_surface) (GdkWindow       *window,
                                              cairo_surface_t *surface,
                                              gint             width,
                                              gint             height);

  /* optional */
  gboolean     (* beep)                 (GdkWindow       *window);
};

/* Interface Functions */
GType gdk_window_impl_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GDK_WINDOW_IMPL_H__ */
