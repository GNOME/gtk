/* gdkprivate-quartz.h
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

#ifndef __GDK_PRIVATE_QUARTZ_H__
#define __GDK_PRIVATE_QUARTZ_H__


#include <gdk/gdk.h>

#include "gdkinternals.h"

#include "config.h"

#define GDK_WINDOW_IS_QUARTZ(win)        (GDK_IS_WINDOW_IMPL_QUARTZ (((GdkWindow *)win)->impl))
/* Cairo surface widths must be 4-pixel byte aligned so that the image will transfer to the CPU. */
#define GDK_WINDOW_QUARTZ_ALIGNMENT 16

/* Display */

GdkDisplay *    _gdk_quartz_display_open (const gchar *name);

/* Window Impl */
void _gdk_quartz_unref_cairo_surface (GdkWindow *window);

#endif /* __GDK_PRIVATE_QUARTZ_H__ */
