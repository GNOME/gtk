#ifndef __GDK_GL_TEXTURE_PRIVATE_H__
#define __GDK_GL_TEXTURE_PRIVATE_H__

#include "gdkgltexture.h"

#include "gdktextureprivate.h"

G_BEGIN_DECLS

GdkGLContext *          gdk_gl_texture_get_context      (GdkGLTexture           *self);
guint                   gdk_gl_texture_get_id           (GdkGLTexture           *self);
gpointer                gdk_gl_texture_get_sync         (GdkGLTexture           *self);

GDK_AVAILABLE_IN_4_10
GdkTexture *            gdk_gl_texture_new_with_sync    (GdkGLContext    *context,
                                                         guint            id,
                                                         gpointer         sync,
                                                         int              width,
                                                         int              height,
                                                         GDestroyNotify   destroy,
                                                         gpointer         data);

G_END_DECLS

#endif /* __GDK_GL_TEXTURE_PRIVATE_H__ */
