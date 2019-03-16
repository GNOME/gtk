/* gdkquartz-gtk-only.h
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#ifndef __GDK_QUARTZ_GTK_ONLY_H__
#define __GDK_QUARTZ_GTK_ONLY_H__

#if !(defined (GTK_COMPILATION) || defined (GDK_COMPILATION))
#error "This API is for use only in Gtk internal code."
#endif

#include <Appkit/Appkit.h>
#include <gdk/gdk.h>
#include <gdk/quartz/gdkquartz.h>

/* Drag and Drop/Clipboard */
GDK_AVAILABLE_IN_ALL
GdkAtom   gdk_quartz_pasteboard_type_to_atom_libgtk_only        (NSString       *type);
GDK_AVAILABLE_IN_ALL
NSString *gdk_quartz_target_to_pasteboard_type_libgtk_only      (const gchar    *target);
GDK_AVAILABLE_IN_ALL
NSString *gdk_quartz_atom_to_pasteboard_type_libgtk_only        (GdkAtom         atom);

/* Utilities */
GDK_AVAILABLE_IN_ALL
NSImage  *gdk_quartz_pixbuf_to_ns_image_libgtk_only (GdkPixbuf *pixbuf);
GDK_AVAILABLE_IN_ALL
NSEvent  *gdk_quartz_event_get_nsevent              (GdkEvent  *event);

/* Window */
GDK_AVAILABLE_IN_ALL
NSWindow *gdk_quartz_window_get_nswindow            (GdkWindow *window);
GDK_AVAILABLE_IN_ALL
NSView   *gdk_quartz_window_get_nsview              (GdkWindow *window);

#endif
