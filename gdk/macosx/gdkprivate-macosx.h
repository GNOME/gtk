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

#ifndef __GDK_PRIVATE_MacOSX_H__
#define __GDK_PRIVATE_MacOSX_H__

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/macosx/gdkwindow-macosx.h>
#include <gdk/macosx/gdkpixmap-macosx.h>
#include <gdk/macosx/gdkdisplay-macosx.h>

#include <ApplicationServices/ApplicationServices.h>

#include "gdkinternals.h"

#include <config.h>

#define GDK_TYPE_GC_MACOSX              (_gdk_gc_macosx_get_type ())
#define GDK_GC_MACOSX(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_MACOSX, GdkGCMacOSX))
#define GDK_GC_MACOSX_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_MACOSX, GdkGCMacOSXClass))
#define GDK_IS_GC_MACOSX(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_MACOSX))
#define GDK_IS_GC_MACOSX_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_MACOSX))
#define GDK_GC_MACOSX_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_MACOSX, GdkGCMacOSXClass))

typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;
typedef struct _GdkGCMacOSX      GdkGCMacOSX;
typedef struct _GdkGCMacOSXClass GdkGCMacOSXClass;

struct _GdkGCMacOSX
{
	GdkGC parent_instance;
	
	CGContextRef cggc;
	GdkScreen *screen;
	GdkRegion *clip_region;
	guint16 dirty_mask;
	guint have_clip_mask : 1;
	guint depth : 8;
	
	GdkFill fill;
	GdkBitmap *stipple;
	GdkPixmap *tile;
	
	//cture fg_picture;
	//enderColor fg_picture_color; 
	CGColorRef stroke_color;
	CGColorRef fill_color;
};

struct _GdkGCMacOSXClass
{
  GdkGCClass parent_class;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  GdkDisplay *display;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
	//Visual *xvisual;
  GdkScreen *screen;
};

/*
void _gdk_xid_table_insert (GdkDisplay *display,
			    XID        *xid,
			    gpointer    data);
void _gdk_xid_table_remove (GdkDisplay *display,
			    XID         xid);
gint _gdk_send_xevent      (GdkDisplay *display,
			    Window      window,
			    gboolean    propagate,
			    glong       event_mask,
			    XEvent     *event_send);
*/
GType _gdk_gc_macosx_get_type (void);

gboolean _gdk_macosx_have_render                 (GdkDisplay *display);
gboolean _gdk_macosx_have_render_with_trapezoids (GdkDisplay *display);

/*
Picture  _gdk_macosx_gc_get_fg_picture   (GdkGC      *gc);
void     _gdk_gc_macosx_get_fg_xft_color (GdkGC      *gc,
				       XftColor   *xftcolor);
*/

GdkGC *_gdk_macosx_gc_new                  (GdkDrawable     *drawable,
					 GdkGCValues     *values,
					 GdkGCValuesMask  values_mask);

GdkImage *_gdk_macosx_copy_to_image       (GdkDrawable *drawable,
					GdkImage    *image,
					gint         src_x,
					gint         src_y,
					gint         dest_x,
					gint         dest_y,
					gint         width,
					gint         height);
//Pixmap   _gdk_macosx_image_get_shm_pixmap (GdkImage    *image);

/* Routines from gdkgeometry-macosx.c */
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
//gboolean _gdk_selection_filter_clear_event (XSelectionClearEvent *event);

/*
void     _gdk_region_get_xrectangles       (GdkRegion            *region,
                                            gint                  x_offset,
                                            gint                  y_offset,
                                            XRectangle          **rects,
                                            gint                 *n_rects);
*/
//gboolean _gdk_moveresize_handle_event   (XEvent     *event);
gboolean _gdk_moveresize_configure_done (GdkDisplay *display,
					 GdkWindow  *window);

void _gdk_keymap_state_changed    (GdkDisplay      *display);
void _gdk_keymap_keys_changed     (GdkDisplay      *display);
gint _gdk_macosx_get_group_for_state (GdkDisplay      *display,
				   GdkModifierType  state);

CGContextRef _gdk_macosx_gc_flush (GdkGC *gc);

void _gdk_macosx_initialize_locale (void);

/*
void _gdk_xgrab_check_unmap   (GdkWindow *window,
			       gulong     serial);
void _gdk_xgrab_check_destroy (GdkWindow *window);

gboolean _gdk_macosx_display_is_root_window (GdkDisplay *display,
					  Window      xroot_window);

void _gdk_macosx_precache_atoms (GdkDisplay          *display,
			      const gchar * const *atom_names,
			      gint                 n_atoms);
*/

void _gdk_macosx_events_init_screen   (GdkScreen *screen);
void _gdk_macosx_events_uninit_screen (GdkScreen *screen);

void _gdk_events_init           (GdkDisplay *display);
void _gdk_windowing_window_init (GdkScreen *screen);
void _gdk_visual_init           (GdkScreen *screen);
void _gdk_dnd_init		(GdkDisplay *display);
void _gdk_windowing_image_init  (GdkDisplay *display);
void _gdk_input_init            (GdkDisplay *display);

/*
PangoRenderer *_gdk_macosx_renderer_get (GdkDrawable *drawable,
				      GdkGC       *gc);
*/

extern GdkDrawableClass  _gdk_macosx_drawable_class;
extern gboolean	         _gdk_use_xshm;
extern const int         _gdk_nenvent_masks;
extern const int         _gdk_event_mask_table[];
extern GdkAtom		 _gdk_selection_property;
extern gboolean          _gdk_synchronize;

#define GDK_PIXMAP_SCREEN(pix)	      (GDK_DRAWABLE_IMPL_MACOSX (((GdkPixmapObject *)pix)->impl)->screen)
#define GDK_PIXMAP_DISPLAY(pix)       (GDK_SCREEN_MACOSX (GDK_PIXMAP_SCREEN (pix))->display)
#define GDK_PIXMAP_XROOTWIN(pix)      (GDK_SCREEN_MACOSX (GDK_PIXMAP_SCREEN (pix))->xroot_window)
#define GDK_DRAWABLE_DISPLAY(win)     (GDK_IS_WINDOW (win) ? GDK_WINDOW_DISPLAY (win) : GDK_PIXMAP_DISPLAY (win))
#define GDK_DRAWABLE_SCREEN(win)      (GDK_IS_WINDOW (win) ? GDK_WINDOW_SCREEN (win) : GDK_PIXMAP_SCREEN (win))
#define GDK_DRAWABLE_XROOTWIN(win)    (GDK_IS_WINDOW (win) ? GDK_WINDOW_XROOTWIN (win) : GDK_PIXMAP_XROOTWIN (win))
#define GDK_SCREEN_DISPLAY(screen)    (GDK_SCREEN_MACOSX (screen)->display)
#define GDK_SCREEN_XROOTWIN(screen)   (GDK_SCREEN_MACOSX (screen)->xroot_window)
#define GDK_WINDOW_SCREEN(win)	      (GDK_DRAWABLE_IMPL_MACOSX (((GdkWindowObject *)win)->impl)->screen)
#define GDK_WINDOW_DISPLAY(win)       (GDK_SCREEN_MACOSX (GDK_WINDOW_SCREEN (win))->display)
#define GDK_WINDOW_XROOTWIN(win)      (GDK_SCREEN_MACOSX (GDK_WINDOW_SCREEN (win))->xroot_window)
#define GDK_GC_DISPLAY(gc)            (GDK_SCREEN_DISPLAY (GDK_GC_MACOSX(gc)->screen))

#endif /* __GDK_PRIVATE_MACOSX_H__ */
