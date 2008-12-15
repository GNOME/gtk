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
#define GDK_WINDOW_IMPL(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WINDOW_IMPL, GdkWindowImpl))
#define GDK_IS_WINDOW_IMPL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WINDOW_IMPL))
#define GDK_WINDOW_IMPL_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDK_TYPE_WINDOW_IMPL, GdkWindowImplIface))

typedef struct _GdkWindowImpl       GdkWindowImpl;      /* dummy */
typedef struct _GdkWindowImplIface  GdkWindowImplIface;

struct _GdkWindowImplIface
{
  GTypeInterface g_iface;

  void         (* show)                 (GdkWindow       *window,
                                         gboolean         raise);
  void         (* hide)                 (GdkWindow       *window);
  void         (* withdraw)             (GdkWindow       *window);
  void         (* raise)                (GdkWindow       *window);
  void         (* lower)                (GdkWindow       *window);

  void         (* move_resize)          (GdkWindow       *window,
                                         gboolean         with_move,
                                         gint             x,
                                         gint             y,
                                         gint             width,
                                         gint             height);
  void         (* set_background)       (GdkWindow       *window,
                                         const GdkColor  *color);
  void         (* set_back_pixmap)      (GdkWindow       *window,
                                         GdkPixmap       *pixmap);

  GdkEventMask (* get_events)           (GdkWindow       *window);
  void         (* set_events)           (GdkWindow       *window,
                                         GdkEventMask     event_mask);
  
  gboolean     (* reparent)             (GdkWindow       *window,
                                         GdkWindow       *new_parent,
                                         gint             x,
                                         gint             y);
  
  void         (* set_cursor)           (GdkWindow       *window,
                                         GdkCursor       *cursor);

  void         (* get_geometry)         (GdkWindow       *window,
                                         gint            *x,
                                         gint            *y,
                                         gint            *width,
                                         gint            *height,
                                         gint            *depth);
  gint         (* get_origin)           (GdkWindow       *window,
                                         gint            *x,
                                         gint            *y);
  gint         (* get_deskrelative_origin) (GdkWindow       *window,
                                         gint            *x,
                                         gint            *y);

  void         (* shape_combine_mask)   (GdkWindow       *window,
                                         GdkBitmap       *mask,
                                         gint             x,
                                         gint             y);
  void         (* shape_combine_region) (GdkWindow       *window,
                                         const GdkRegion *shape_region,
                                         gint             offset_x,
                                         gint             offset_y);
  void         (* set_child_shapes)     (GdkWindow       *window);
  void         (* merge_child_shapes)   (GdkWindow       *window);

  gboolean     (* set_static_gravities) (GdkWindow       *window,
				         gboolean         use_static);

  /* Called before processing updates for a window. This gives the windowing
   * layer a chance to save the region for later use in avoiding duplicate
   * exposes. The return value indicates whether the function has a saved
   * the region; if the result is TRUE, then the windowing layer is responsible
   * for destroying the region later.
   */
  gboolean     (* queue_antiexpose)     (GdkWindow       *window,
					 GdkRegion       *update_area);
  void         (* queue_translation)    (GdkWindow       *window,
					 GdkRegion       *area,
					 gint            dx,
					 gint            dy);
  
};

/* Interface Functions */
GType gdk_window_impl_get_type (void) G_GNUC_CONST;

/* private definitions from gdkwindow.h */

struct _GdkWindowRedirect
{
  GdkWindowObject *redirected;
  GdkDrawable *pixmap;

  gint src_x;
  gint src_y;
  gint dest_x;
  gint dest_y;
  gint width;
  gint height;

  GdkRegion *damage;
  guint damage_idle;
};

G_END_DECLS

#endif /* __GDK_WINDOW_IMPL_H__ */
