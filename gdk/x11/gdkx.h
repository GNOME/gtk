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

#include <gdk/gdk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

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


#define __GDKX_H_INSIDE__

#include <gdk/x11/gdkx11cursor.h>
#include <gdk/x11/gdkx11display.h>
#include <gdk/x11/gdkx11screen.h>
#include <gdk/x11/gdkx11selection.h>
#include <gdk/x11/gdkx11visual.h>
#include <gdk/x11/gdkx11window.h>

#undef __GDKX_H_INSIDE__

G_BEGIN_DECLS

#ifndef GDK_MULTIHEAD_SAFE
Window   gdk_x11_get_default_root_xwindow (void);
Display *gdk_x11_get_default_xdisplay     (void);
#endif

#ifndef GDK_MULTIHEAD_SAFE
/**
 * GDK_ROOT_WINDOW:
 *
 * Obtains the Xlib window id of the root window of the current screen.
 */
#define GDK_ROOT_WINDOW()             (gdk_x11_get_default_root_xwindow ())
#endif

#ifndef GDK_MULTIHEAD_SAFE
void          gdk_x11_grab_server    (void);
void          gdk_x11_ungrab_server  (void);
#endif


/* Functions to get the X Atom equivalent to the GdkAtom */
Atom                  gdk_x11_atom_to_xatom_for_display (GdkDisplay  *display,
                                                         GdkAtom      atom);
GdkAtom               gdk_x11_xatom_to_atom_for_display (GdkDisplay  *display,
                                                         Atom         xatom);
Atom                  gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
                                                             const gchar *atom_name);
G_CONST_RETURN gchar *gdk_x11_get_xatom_name_for_display (GdkDisplay  *display,
                                                          Atom         xatom);
#ifndef GDK_MULTIHEAD_SAFE
Atom                  gdk_x11_atom_to_xatom     (GdkAtom      atom);
GdkAtom               gdk_x11_xatom_to_atom     (Atom         xatom);
Atom                  gdk_x11_get_xatom_by_name (const gchar *atom_name);
G_CONST_RETURN gchar *gdk_x11_get_xatom_name    (Atom         xatom);
#endif

G_END_DECLS

#endif /* __GDK_X_H__ */
