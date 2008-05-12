/* gdkquartz.h
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_QUARTZ_H__
#define __GDK_QUARTZ_H__

#include <AppKit/AppKit.h>
#include <gdk/gdkprivate.h>

G_BEGIN_DECLS

NSWindow *gdk_quartz_window_get_nswindow                        (GdkWindow      *window);
NSView   *gdk_quartz_window_get_nsview                          (GdkWindow      *window);
NSImage  *gdk_quartz_pixbuf_to_ns_image_libgtk_only             (GdkPixbuf      *pixbuf);
id        gdk_quartz_drag_context_get_dragging_info_libgtk_only (GdkDragContext *context);
NSEvent  *gdk_quartz_event_get_nsevent                          (GdkEvent       *event);

G_END_DECLS

#endif /* __GDK_QUARTZ_H__ */
