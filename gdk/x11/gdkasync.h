/* GTK - The GIMP Toolkit
 * gdkasync.h: Utility functions using the Xlib asynchronous interfaces
 * Copyright (C) 2003, Red Hat, Inc.
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

#ifndef __GDK_ASYNC_H__
#define __GDK_ASYNC_H__

#include <X11/Xlib.h>
#include "gdk.h"

G_BEGIN_DECLS

void _gdk_x11_set_input_focus_safe (GdkDisplay             *display,
				    Window                  window,
				    int                     revert_to,
				    Time                    time);

G_END_DECLS

#endif /* __GDK_ASYNC_H__ */
