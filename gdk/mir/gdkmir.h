/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_MIR_H__
#define __GDK_MIR_H__

#include <gdk/gdk.h>
#include <mir_toolkit/mir_client_library.h>

#define GDK_TYPE_MIR_DISPLAY              (gdk_mir_display_get_type ())
#define GDK_IS_MIR_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_DISPLAY))

#define GDK_TYPE_MIR_GL_CONTEXT           (gdk_mir_gl_context_get_type ())
#define GDK_MIR_IS_GL_CONTEXT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_MIR_GL_CONTEXT))

#define GDK_TYPE_MIR_WINDOW               (gdk_mir_window_get_type ())
#define GDK_IS_WINDOW_MIR(object)         (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_WINDOW))

GDK_AVAILABLE_IN_3_10
GType gdk_mir_display_get_type (void);

GDK_AVAILABLE_IN_3_10
MirConnection *gdk_mir_display_get_mir_connection (GdkDisplay *display);

GDK_AVAILABLE_IN_3_10
GType gdk_mir_window_get_type (void);

GDK_AVAILABLE_IN_3_16
MirSurface *gdk_mir_window_get_mir_surface (GdkWindow *window);

GDK_AVAILABLE_IN_3_16
GType gdk_mir_gl_context_get_type (void) G_GNUC_CONST;

#endif /* __GDK_MIR_H__ */
