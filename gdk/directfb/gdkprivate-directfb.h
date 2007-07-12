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
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#ifndef __GDK_PRIVATE_DIRECTFB_H__
#define __GDK_PRIVATE_DIRECTFB_H__

//#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>
#include "gdkinternals.h"
#include "gdkcursor.h"
#include "gdkdisplay-directfb.h"
#include <cairo.h>



#define GDK_TYPE_DRAWABLE_IMPL_DIRECTFB       (gdk_drawable_impl_directfb_get_type ())
#define GDK_DRAWABLE_IMPL_DIRECTFB(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE_IMPL_DIRECTFB, GdkDrawableImplDirectFB))
#define GDK_IS_DRAWABLE_IMPL_DIRECTFB(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_DIRECTFB))

#define GDK_TYPE_WINDOW_IMPL_DIRECTFB         (gdk_window_impl_directfb_get_type ())
#define GDK_WINDOW_IMPL_DIRECTFB(object)      (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_DIRECTFB, GdkWindowImplDirectFB))
#define GDK_IS_WINDOW_IMPL_DIRECTFB(object)   (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_DIRECTFB))

#define GDK_TYPE_PIXMAP_IMPL_DIRECTFB         (gdk_pixmap_impl_directfb_get_type ())
#define GDK_PIXMAP_IMPL_DIRECTFB(object)      (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXMAP_IMPL_DIRECTFB, GdkPixmapImplDirectFB))
#define GDK_IS_PIXMAP_IMPL_DIRECTFB(object)   (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXMAP_IMPL_DIRECTFB))


typedef struct _GdkDrawableImplDirectFB GdkDrawableImplDirectFB;
typedef struct _GdkWindowImplDirectFB   GdkWindowImplDirectFB;
typedef struct _GdkPixmapImplDirectFB   GdkPixmapImplDirectFB;


struct _GdkDrawableImplDirectFB
{
  GdkDrawable             parent_object;

  GdkDrawable            *wrapper;

  gboolean                buffered;

  GdkRegion              *paint_region;
  gint                    paint_depth;
  gint                    width;
  gint                    height;
  gint                    abs_x;
  gint                    abs_y;

  GdkColormap            *colormap;

  IDirectFBSurface       *surface;
  DFBSurfacePixelFormat   format;
  cairo_surface_t *  cairo_surface;

};

typedef struct
{
  GdkDrawableClass  parent_class;
} GdkDrawableImplDirectFBClass;

GType      gdk_drawable_impl_directfb_get_type (void);

void       _gdk_directfb_draw_rectangle (GdkDrawable *drawable,
                                         GdkGC       *gc,
                                         gint         filled,
                                         gint         x,
                                         gint         y,
                                         gint         width,
                                         gint         height);

void       _gdk_directfb_update         (GdkDrawableImplDirectFB *impl,
                                         DFBRegion               *region);

GdkEvent *  gdk_directfb_event_make     (GdkWindow               *window,
                                         GdkEventType             type);


/*
 * Pixmap
 */

struct _GdkPixmapImplDirectFB
{
  GdkDrawableImplDirectFB parent_instance;
};

typedef struct
{
  GdkDrawableImplDirectFBClass parent_class;
} GdkPixmapImplDirectFBClass;

GType gdk_pixmap_impl_directfb_get_type (void);



/*
 * Window
 */

typedef struct
{
  gulong   length;
  GdkAtom  type;
  gint     format;
  guchar   data[1];
} GdkWindowProperty;


struct _GdkWindowImplDirectFB
{
  GdkDrawableImplDirectFB drawable;
  GdkWindow             *gdkWindow;

  IDirectFBWindow        *window;

  DFBWindowID             dfb_id;

  GdkCursor              *cursor;
  GHashTable             *properties;

  guint8                  opacity;

  GdkWindowTypeHint       type_hint;
};

typedef struct
{
  GdkDrawableImplDirectFBClass parent_class;
} GdkWindowImplDirectFBClass;

GType gdk_window_impl_directfb_get_type        (void);

void  gdk_directfb_window_send_crossing_events (GdkWindow       *src,
                                                GdkWindow       *dest,
                                                GdkCrossingMode  mode);

void  _gdk_directfb_calc_abs                   (GdkWindow       *window);

GdkWindow * gdk_directfb_window_find_toplevel  (GdkWindow       *window);


void        gdk_directfb_window_id_table_insert (DFBWindowID  dfb_id,
                                                 GdkWindow   *window);
void        gdk_directfb_window_id_table_remove (DFBWindowID  dfb_id);
GdkWindow * gdk_directfb_window_id_table_lookup (DFBWindowID  dfb_id);


typedef struct
{
  GdkCursor         cursor;

  gint              hot_x;
  gint              hot_y;
  IDirectFBSurface *shape;
} GdkCursorDirectFB;

typedef struct
{
  GdkVisual              visual;
  DFBSurfacePixelFormat  format;
} GdkVisualDirectFB;

typedef struct
{
  IDirectFBSurface *surface;
} GdkImageDirectFB;


#define GDK_TYPE_GC_DIRECTFB       (gdk_gc_directfb_get_type ())
#define GDK_GC_DIRECTFB(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_DIRECTFB, GdkGCDirectFB))
#define GDK_IS_GC_DIRECTFB(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_DIRECTFB))

typedef struct
{
  GdkGC             parent_instance;

  GdkRegion        *clip_region;

  GdkGCValuesMask   values_mask;
  GdkGCValues       values;
} GdkGCDirectFB;

typedef struct
{
  GdkGCClass        parent_class;
} GdkGCDirectFBClass;

GType     gdk_gc_directfb_get_type (void);

GdkGC *  _gdk_directfb_gc_new      (GdkDrawable     *drawable,
                                    GdkGCValues     *values,
                                    GdkGCValuesMask  values_mask);

GdkImage* _gdk_directfb_copy_to_image (GdkDrawable  *drawable,
                                       GdkImage     *image,
                                       gint          src_x,
                                       gint          src_y,
                                       gint          dest_x,
                                       gint          dest_y,
                                       gint          width,
                                       gint          height);

void       gdk_directfb_event_windows_add (GdkWindow *window);
#if (DIRECTFB_MAJOR_VERSION >= 1)
void       gdk_directfb_event_windows_remove (GdkWindow *window);
#endif

GdkGrabStatus gdk_directfb_pointer_grab   (GdkWindow           *window,
                                           gint                 owner_events,
                                           GdkEventMask         event_mask,
                                           GdkWindow           *confine_to,
                                           GdkCursor           *cursor,
                                           guint32              time,
                                           gboolean             implicit_grab);
void          gdk_directfb_pointer_ungrab (guint32              time,
                                           gboolean             implicit_grab);

guint32       gdk_directfb_get_time       (void);

GdkWindow * gdk_directfb_pointer_event_window  (GdkWindow    *window,
                                                GdkEventType  type);
GdkWindow * gdk_directfb_keyboard_event_window (GdkWindow    *window,
                                                GdkEventType  type);
GdkWindow * gdk_directfb_other_event_window    (GdkWindow    *window,
                                                GdkEventType  type);
void       _gdk_selection_window_destroyed    (GdkWindow       *window);

void       _gdk_directfb_move_resize_child (GdkWindow *window,
                                            gint       x,
                                            gint       y,
                                            gint       width,
                                            gint       height);

GdkWindow * gdk_directfb_child_at          (GdkWindow *window,
                                            gint      *x,
                                            gint      *y);

GdkWindow * gdk_directfb_window_find_focus (void);

void        gdk_directfb_change_focus      (GdkWindow *new_focus_window);

void        gdk_directfb_mouse_get_info    (gint            *x,
                                            gint            *y,
                                            GdkModifierType *mask);

/**********************/
/*  Global variables  */
/**********************/

extern GdkDisplayDFB *_gdk_display;

/* Pointer grab info */
extern GdkWindow           * _gdk_directfb_pointer_grab_window;
extern gboolean              _gdk_directfb_pointer_grab_owner_events;
extern GdkWindow           * _gdk_directfb_pointer_grab_confine;
extern GdkEventMask          _gdk_directfb_pointer_grab_events;
extern GdkCursor           * _gdk_directfb_pointer_grab_cursor;

/* Keyboard grab info */
extern GdkWindow           * _gdk_directfb_keyboard_grab_window;
extern GdkEventMask          _gdk_directfb_keyboard_grab_events;
extern gboolean              _gdk_directfb_keyboard_grab_owner_events;

extern GdkScreen  *  _gdk_screen;

extern GdkAtom               _gdk_selection_property;


IDirectFBPalette * gdk_directfb_colormap_get_palette (GdkColormap *colormap);


/* these are Linux-FB specific functions used for window decorations */

typedef gboolean (* GdkWindowChildChanged) (GdkWindow *window,
                                            gint       x,
                                            gint       y,
                                            gint       width,
                                            gint       height,
                                            gpointer   user_data);
typedef void     (* GdkWindowChildGetPos)  (GdkWindow *window,
                                            gint      *x,
                                            gint      *y,
                                            gpointer   user_data);

void gdk_fb_window_set_child_handler (GdkWindow              *window,
                                      GdkWindowChildChanged  changed,
                                      GdkWindowChildGetPos   get_pos,
                                      gpointer               user_data);


#endif /* __GDK_PRIVATE_DIRECTFB_H__ */
