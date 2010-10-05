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

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkinternals.h>
#include "gdkwindow-x11.h"
#include "gdkdisplay-x11.h"
#include <cairo-xlib.h>

typedef struct _GdkCursorPrivate       GdkCursorPrivate;

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  GdkDisplay *display;
  gchar *name;
  guint serial;
};

void _gdk_x11_error_handler_push (void);
void _gdk_x11_error_handler_pop  (void);

Colormap _gdk_visual_get_x11_colormap (GdkVisual *visual);

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

/* Routines from gdkgeometry-x11.c */
void _gdk_window_move_resize_child (GdkWindow     *window,
                                    gint           x,
                                    gint           y,
                                    gint           width,
                                    gint           height);
void _gdk_window_process_expose    (GdkWindow     *window,
                                    gulong         serial,
                                    GdkRectangle  *area);

gboolean _gdk_x11_window_queue_antiexpose  (GdkWindow *window,
					    cairo_region_t *area);
void     _gdk_x11_window_translate         (GdkWindow *window,
					    cairo_region_t *area,
					    gint       dx,
					    gint       dy);

void     _gdk_selection_window_destroyed   (GdkWindow            *window);
gboolean _gdk_selection_filter_clear_event (XSelectionClearEvent *event);

cairo_region_t* _xwindow_get_shape              (Display *xdisplay,
                                            Window window,
                                            gint shape_type);

void     _gdk_region_get_xrectangles       (const cairo_region_t      *region,
                                            gint                  x_offset,
                                            gint                  y_offset,
                                            XRectangle          **rects,
                                            gint                 *n_rects);

gboolean _gdk_moveresize_handle_event   (XEvent     *event);
gboolean _gdk_moveresize_configure_done (GdkDisplay *display,
					 GdkWindow  *window);

void _gdk_keymap_state_changed    (GdkDisplay      *display,
				   XEvent          *event);
void _gdk_keymap_keys_changed     (GdkDisplay      *display);
gint _gdk_x11_get_group_for_state (GdkDisplay      *display,
				   GdkModifierType  state);
void _gdk_keymap_add_virtual_modifiers_compat (GdkKeymap       *keymap,
                                               GdkModifierType *modifiers);
gboolean _gdk_keymap_key_is_modifier   (GdkKeymap       *keymap,
					guint            keycode);

void _gdk_x11_initialize_locale (void);

void _gdk_xgrab_check_unmap        (GdkWindow *window,
				    gulong     serial);
void _gdk_xgrab_check_destroy      (GdkWindow *window);

gboolean _gdk_x11_display_is_root_window (GdkDisplay *display,
					  Window      xroot_window);

void _gdk_x11_precache_atoms (GdkDisplay          *display,
			      const gchar * const *atom_names,
			      gint                 n_atoms);

void _gdk_screen_x11_events_init   (GdkScreen *screen);

void _gdk_events_init           (GdkDisplay *display);
void _gdk_events_uninit         (GdkDisplay *display);
void _gdk_windowing_window_init (GdkScreen *screen);
void _gdk_visual_init           (GdkScreen *screen);
void _gdk_dnd_init		(GdkDisplay *display);

void _gdk_x11_cursor_update_theme (GdkCursor *cursor);
void _gdk_x11_cursor_display_finalize (GdkDisplay *display);

gboolean _gdk_x11_get_xft_setting (GdkScreen   *screen,
				   const gchar *name,
				   GValue      *value);

GdkGrabStatus _gdk_x11_convert_grab_status (gint status);

cairo_surface_t * _gdk_x11_window_create_bitmap_surface (GdkWindow *window,
                                                         int        width,
                                                         int        height);

extern GdkDrawableClass  _gdk_x11_drawable_class;
extern gboolean	         _gdk_use_xshm;
extern const int         _gdk_nenvent_masks;
extern const int         _gdk_event_mask_table[];
extern GdkAtom		 _gdk_selection_property;
extern gboolean          _gdk_synchronize;

#define GDK_DRAWABLE_XROOTWIN(win)    (GDK_WINDOW_XROOTWIN (win))
#define GDK_SCREEN_DISPLAY(screen)    (GDK_SCREEN_X11 (screen)->display)
#define GDK_SCREEN_XROOTWIN(screen)   (GDK_SCREEN_X11 (screen)->xroot_window)
#define GDK_WINDOW_SCREEN(win)	      (GDK_DRAWABLE_IMPL_X11 (((GdkWindowObject *)win)->impl)->screen)
#define GDK_WINDOW_DISPLAY(win)       (GDK_SCREEN_X11 (GDK_WINDOW_SCREEN (win))->display)
#define GDK_WINDOW_XROOTWIN(win)      (GDK_SCREEN_X11 (GDK_WINDOW_SCREEN (win))->xroot_window)
#define GDK_GC_DISPLAY(gc)            (GDK_SCREEN_DISPLAY (GDK_GC_X11(gc)->screen))
#define GDK_WINDOW_IS_X11(win)        (GDK_IS_WINDOW_IMPL_X11 (((GdkWindowObject *)win)->impl))

#endif /* __GDK_PRIVATE_X11_H__ */
