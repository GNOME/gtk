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

#ifndef __GDK_X_H__
#define __GDK_X_H__

#include <gdk/gdkprivate.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_BEGIN_DECLS


/**
 * SECTION:x_interaction
 * @Short_description: X backend-specific functions
 * @Title: X Window System Interaction
 *
 * The functions in this section are specific to the GDK X11 backend.
 * To use them, you need to include the <literal>&lt;gdk/gdkx.h&gt;</literal>
 * header and use the X11-specific pkg-config files to build your
 * application (either <literal>gdk-x11-3.0</literal> or
 * <literal>gtk+-x11-3.0</literal>).
 *
 * To make your code compile with other GDK backends, guard backend-specific
 * calls by an ifdef as follows:
 * <informalexample><programlisting>
 * #ifdef GDK_WINDOWING_X11
 *   /<!---->* X11-specific calls here... *<!---->/
 * #endif
 * </programlisting></informalexample>
 */


Display *gdk_x11_drawable_get_xdisplay    (GdkDrawable *drawable);
XID      gdk_x11_drawable_get_xid         (GdkDrawable *drawable);
GdkDrawable *gdk_x11_window_get_drawable_impl (GdkWindow *window);
Display *gdk_x11_cursor_get_xdisplay      (GdkCursor   *cursor);
Cursor   gdk_x11_cursor_get_xcursor       (GdkCursor   *cursor);
Display *gdk_x11_display_get_xdisplay     (GdkDisplay  *display);
Visual * gdk_x11_visual_get_xvisual       (GdkVisual   *visual);
Screen * gdk_x11_screen_get_xscreen       (GdkScreen   *screen);
int      gdk_x11_screen_get_screen_number (GdkScreen   *screen);
void     gdk_x11_window_set_user_time     (GdkWindow   *window,
					   guint32      timestamp);
void     gdk_x11_window_move_to_current_desktop (GdkWindow   *window);

const char* gdk_x11_screen_get_window_manager_name (GdkScreen *screen);

#ifndef GDK_MULTIHEAD_SAFE
Window   gdk_x11_get_default_root_xwindow (void);
Display *gdk_x11_get_default_xdisplay     (void);
gint     gdk_x11_get_default_screen       (void);
#endif

/**
 * GDK_CURSOR_XDISPLAY:
 * @cursor: a #GdkCursor.
 *
 * Returns the display of a #GdkCursor.
 *
 * Returns: an Xlib <type>Display*</type>.
 */
#define GDK_CURSOR_XDISPLAY(cursor)   (gdk_x11_cursor_get_xdisplay (cursor))

/**
 * GDK_CURSOR_XCURSOR:
 * @cursor: a #GdkCursor.
 *
 * Returns the X cursor belonging to a #GdkCursor.
 *
 * Returns: an Xlib <type>Cursor</type>.
 */
#define GDK_CURSOR_XCURSOR(cursor)    (gdk_x11_cursor_get_xcursor (cursor))


#ifdef GDK_COMPILATION

#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"

/**
 * GDK_DISPLAY_XDISPLAY:
 * @display: a #GdkDisplay.
 *
 * Returns the display of a #GdkDisplay.
 */
#define GDK_DISPLAY_XDISPLAY(display) (GDK_DISPLAY_X11(display)->xdisplay)

/**
 * GDK_WINDOW_XDISPLAY:
 * @win: a #GdkWindow.
 *
 * Returns the display of a #GdkWindow.
 *
 * Returns: an Xlib <type>Display*</type>.
 */
#define GDK_WINDOW_XDISPLAY(win)      (GDK_SCREEN_X11 (GDK_WINDOW_SCREEN (win))->xdisplay)
#define GDK_WINDOW_XID(win)           (GDK_DRAWABLE_IMPL_X11(((GdkWindowObject *)win)->impl)->xid)

/**
 * GDK_DRAWABLE_XDISPLAY:
 * @win: a #GdkDrawable.
 *
 * Returns the display of a #GdkDrawable.
 *
 * Returns: an Xlib <type>Display*</type>.
 */
#define GDK_DRAWABLE_XDISPLAY(win)    (GDK_WINDOW_XDISPLAY (win))

/**
 * GDK_DRAWABLE_XID:
 * @win: a #GdkDrawable.
 *
 * Returns the X resource (window or pixmap) belonging to a #GdkDrawable.
 *
 * Returns: the ID of @win's X resource.
 */
#define GDK_DRAWABLE_XID(win)         (GDK_WINDOW_XID (win))

/**
 * GDK_SCREEN_XDISPLAY:
 * @screen: a #GdkScreen.
 *
 * Returns the display of a #GdkScreen.
 *
 * Returns: an Xlib <type>Display*</type>.
 */
#define GDK_SCREEN_XDISPLAY(screen)   (GDK_SCREEN_X11 (screen)->xdisplay)

/**
 * GDK_SCREEN_XSCREEN:
 * @screen: a #GdkScreen
 *
 * Returns the screen of a #GdkScreen.
 *
 * Returns: an Xlib <type>Screen*</type>.
 */
#define GDK_SCREEN_XSCREEN(screen)    (GDK_SCREEN_X11 (screen)->xscreen)
#define GDK_SCREEN_XNUMBER(screen)    (GDK_SCREEN_X11 (screen)->screen_num) 
#define GDK_WINDOW_XWINDOW	      GDK_DRAWABLE_XID

#else /* GDK_COMPILATION */

#ifndef GDK_MULTIHEAD_SAFE
/**
 * GDK_ROOT_WINDOW:
 *
 * Obtains the Xlib window id of the root window of the current screen.
 */
#define GDK_ROOT_WINDOW()             (gdk_x11_get_default_root_xwindow ())
#endif

#define GDK_DISPLAY_XDISPLAY(display) (gdk_x11_display_get_xdisplay (display))

#define GDK_WINDOW_XDISPLAY(win)      (gdk_x11_drawable_get_xdisplay (gdk_x11_window_get_drawable_impl (win)))

/**
 * GDK_WINDOW_XID:
 * @win: a #GdkWindow.
 *
 * Returns the X window belonging to a #GdkWindow.
 *
 * Returns: the Xlib <type>Window</type> of @win.
 */
#define GDK_WINDOW_XID(win)           (gdk_x11_drawable_get_xid (win))

/**
 * GDK_WINDOW_XWINDOW:
 *
 * Another name for GDK_DRAWABLE_XID().
 */
#define GDK_WINDOW_XWINDOW(win)       (gdk_x11_drawable_get_xid (win))
#define GDK_DRAWABLE_XDISPLAY(win)    (gdk_x11_drawable_get_xdisplay (win))
#define GDK_DRAWABLE_XID(win)         (gdk_x11_drawable_get_xid (win))
#define GDK_SCREEN_XDISPLAY(screen)   (gdk_x11_display_get_xdisplay (gdk_screen_get_display (screen)))
#define GDK_SCREEN_XSCREEN(screen)    (gdk_x11_screen_get_xscreen (screen))

/**
 * GDK_SCREEN_XNUMBER:
 * @screen: a #GdkScreen
 *
 * Returns the index of a #GdkScreen.
 *
 * Returns: the position of @screen among the screens of
 *  its display.
 */
#define GDK_SCREEN_XNUMBER(screen)    (gdk_x11_screen_get_screen_number (screen))

#endif /* GDK_COMPILATION */

#define GDK_VISUAL_XVISUAL(visual)    (gdk_x11_visual_get_xvisual (visual))

GdkVisual* gdk_x11_screen_lookup_visual (GdkScreen *screen,
					 VisualID   xvisualid);
#ifndef GDK_MULTIHEAD_SAFE
GdkVisual* gdkx_visual_get            (VisualID   xvisualid);
#endif

     /* Return the Gdk* for a particular XID */
gpointer      gdk_xid_table_lookup_for_display (GdkDisplay *display,
						XID         xid);
guint32       gdk_x11_get_server_time  (GdkWindow       *window);
guint32       gdk_x11_display_get_user_time (GdkDisplay *display);

G_CONST_RETURN gchar *gdk_x11_display_get_startup_notification_id (GdkDisplay *display);
void          gdk_x11_display_set_startup_notification_id         (GdkDisplay  *display,
                                                                   const gchar *startup_id);

void          gdk_x11_display_set_cursor_theme (GdkDisplay  *display,
						const gchar *theme,
						const gint   size);

void gdk_x11_display_broadcast_startup_message (GdkDisplay *display,
						const char *message_type,
						...) G_GNUC_NULL_TERMINATED;

/* returns TRUE if we support the given WM spec feature */
gboolean gdk_x11_screen_supports_net_wm_hint (GdkScreen *screen,
					      GdkAtom    property);

XID      gdk_x11_screen_get_monitor_output   (GdkScreen *screen,
                                              gint       monitor_num);

#ifndef GDK_MULTIHEAD_SAFE
gpointer      gdk_xid_table_lookup   (XID              xid);
gboolean      gdk_net_wm_supports    (GdkAtom    property);
void          gdk_x11_grab_server    (void);
void          gdk_x11_ungrab_server  (void);
#endif

GdkDisplay   *gdk_x11_lookup_xdisplay (Display *xdisplay);


/* Functions to get the X Atom equivalent to the GdkAtom */
Atom	              gdk_x11_atom_to_xatom_for_display (GdkDisplay  *display,
							 GdkAtom      atom);
GdkAtom		      gdk_x11_xatom_to_atom_for_display (GdkDisplay  *display,
							 Atom	      xatom);
Atom		      gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
							     const gchar *atom_name);
G_CONST_RETURN gchar *gdk_x11_get_xatom_name_for_display (GdkDisplay  *display,
							  Atom         xatom);
#ifndef GDK_MULTIHEAD_SAFE
Atom                  gdk_x11_atom_to_xatom     (GdkAtom      atom);
GdkAtom               gdk_x11_xatom_to_atom     (Atom         xatom);
Atom                  gdk_x11_get_xatom_by_name (const gchar *atom_name);
G_CONST_RETURN gchar *gdk_x11_get_xatom_name    (Atom         xatom);
#endif

void	    gdk_x11_display_grab	      (GdkDisplay *display);
void	    gdk_x11_display_ungrab	      (GdkDisplay *display);

void                           gdk_x11_display_error_trap_push        (GdkDisplay *display);
/* warn unused because you could use pop_ignored otherwise */
G_GNUC_WARN_UNUSED_RESULT gint gdk_x11_display_error_trap_pop         (GdkDisplay *display);
void                           gdk_x11_display_error_trap_pop_ignored (GdkDisplay *display);

void        gdk_x11_register_standard_event_type (GdkDisplay *display,
						  gint        event_base,
						  gint        n_events);


G_END_DECLS

#endif /* __GDK_X_H__ */
