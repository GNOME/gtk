/* gdktexture.h
 *
 * Copyright 2016  Benjamin Otte
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

#ifndef __GDK_GL_TEXTURE_H__
#define __GDK_GL_TEXTURE_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkglcontext.h>
#include <gdk/gdktexture.h>

G_BEGIN_DECLS

#define GDK_TYPE_GL_TEXTURE (gdk_gl_texture_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GdkGLTexture, gdk_gl_texture, GDK, GL_TEXTURE)

GDK_AVAILABLE_IN_ALL
GdkTexture *            gdk_gl_texture_new                     (GdkGLContext    *context,
                                                                guint            id,
                                                                int              width,
                                                                int              height,
                                                                GDestroyNotify   destroy,
                                                                gpointer         data);

GDK_AVAILABLE_IN_ALL
void                    gdk_gl_texture_release                 (GdkGLTexture    *self);


G_END_DECLS

#endif /* __GDK_GL_TEXTURE_H__ */
