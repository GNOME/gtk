/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-quartz.h: Private Quartz specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
 * Copyright © 2014  Red Hat, Int
 * Copyright © 2014  Brion Vibber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_QUARTZ_GL_CONTEXT__
#define __GDK_QUARTZ_GL_CONTEXT__

#include "gdkglcontextprivate.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS

GdkGLContext *  gdk_quartz_window_create_gl_context                (GdkWindow         *window,
                                                                    gboolean           attach,
                                                                    GdkGLContext      *share,
                                                                    GError           **error);

G_END_DECLS

#endif /* __GDK_QUARTZ_GL_CONTEXT__ */
