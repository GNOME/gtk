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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_X11_H__
#define __GDK_PRIVATE_X11_H__

#include <gdk/gdkprivate.h>
#include <gdk/x11/gdkwindow-x11.h>
#include <gdk/x11/gdkpixmap-x11.h>

#include "gdkinternals.h"

#include <config.h>

#if HAVE_XFT
#include <X11/extensions/Xrender.h>
#endif

#define GDK_TYPE_GC_X11              (_gdk_gc_x11_get_type ())
#define GDK_GC_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_X11, GdkGCX11))
#define GDK_GC_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_X11, GdkGCX11Class))
#define GDK_IS_GC_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_X11))
#define GDK_IS_GC_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_X11))
#define GDK_GC_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_X11, GdkGCX11Class))

typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;
typedef struct _GdkGCX11      GdkGCX11;
typedef struct _GdkGCX11Class GdkGCX11Class;

struct _GdkGCX11
{
  GdkGC parent_instance;
  
  GC xgc;
  Display *xdisplay;
  GdkRegion *clip_region;
  guint dirty_mask;

#ifdef HAVE_XFT  
  Picture fg_picture;
  XRenderColor fg_picture_color; 
#endif  
  gulong fg_pixel;
};

struct _GdkGCX11Class
{
  GdkGCClass parent_class;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  Display *xdisplay;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
  Visual *xvisual;
};

void          gdk_xid_table_insert     (XID             *xid,
					gpointer         data);
void          gdk_xid_table_remove     (XID              xid);
gint          gdk_send_xevent          (Window           window,
					gboolean         propagate,
					glong            event_mask,
					XEvent          *event_send);

GType _gdk_gc_x11_get_type (void);

#ifdef HAVE_XFT
gboolean _gdk_x11_have_render       (void);
Picture  _gdk_x11_gc_get_fg_picture (GdkGC *gc);
#endif /* HAVE_XFT */

GdkGC *_gdk_x11_gc_new                  (GdkDrawable     *drawable,
					 GdkGCValues     *values,
					 GdkGCValuesMask  values_mask);

GdkColormap * gdk_colormap_lookup      (Colormap         xcolormap);
GdkVisual *   gdk_visual_lookup        (Visual          *xvisual);

void gdk_window_add_colormap_windows (GdkWindow *window);

GdkImage *_gdk_x11_copy_to_image       (GdkDrawable *drawable,
					GdkImage    *image,
					gint         src_x,
					gint         src_y,
					gint         dest_x,
					gint         dest_y,
					gint         width,
					gint         height);
Pixmap   _gdk_x11_image_get_shm_pixmap (GdkImage    *image);

/* Please see gdkwindow.c for comments on how to use */ 
Window gdk_window_xid_at        (Window    base,
				 gint      bx,
				 gint      by,
				 gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);
Window gdk_window_xid_at_coords (gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);

/* Routines from gdkgeometry-x11.c */
void _gdk_window_init_position     (GdkWindow     *window);
void _gdk_window_move_resize_child (GdkWindow     *window,
                                    gint           x,
                                    gint           y,
                                    gint           width,
                                    gint           height);
void _gdk_window_process_expose    (GdkWindow     *window,
                                    gulong         serial,
                                    GdkRectangle  *area);

void     _gdk_selection_window_destroyed   (GdkWindow            *window);
gboolean _gdk_selection_filter_clear_event (XSelectionClearEvent *event);

void     _gdk_region_get_xrectangles       (GdkRegion            *region,
                                            gint                  x_offset,
                                            gint                  y_offset,
                                            XRectangle          **rects,
                                            gint                 *n_rects);

void     _gdk_moveresize_handle_event      (XEvent *event);
void     _gdk_moveresize_configure_done    (void);

void     _gdk_keymap_state_changed         (void);
gint     _gdk_x11_get_group_for_state      (GdkModifierType state);

GC _gdk_x11_gc_flush (GdkGC *gc);

void _gdk_x11_initialize_locale (void);

void _gdk_xgrab_check_unmap   (GdkWindow *window,
			       gulong     serial);
void _gdk_xgrab_check_destroy (GdkWindow *window);

extern GdkDrawableClass  _gdk_x11_drawable_class;
extern Window		 _gdk_root_window;
extern gboolean	         _gdk_use_xshm;
extern Atom		 _gdk_wm_window_protocols[];
extern const int         _gdk_nenvent_masks;
extern const int         _gdk_event_mask_table[];
extern gint		 _gdk_screen;
extern GdkAtom		 _gdk_selection_property;
extern gchar		*_gdk_display_name;

extern Window		 _gdk_leader_window;

/* Used to detect not-up-to-date keymap */
extern guint _gdk_keymap_serial;

#ifdef HAVE_XKB
extern gboolean _gdk_use_xkb;
extern gboolean _gdk_have_xkb_autorepeat;
extern gint _gdk_xkb_event_type;
#endif

/* Whether we were able to turn on detectable-autorepeat using
 * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
 * to checking the next event with XPending().
 */
extern gboolean _gdk_have_xkb_autorepeat;

extern GdkWindow *_gdk_moveresize_window;

#endif /* __GDK_PRIVATE_X11_H__ */
