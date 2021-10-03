#ifndef __GDK_GL_TEXTURE_PRIVATE_H__
#define __GDK_GL_TEXTURE_PRIVATE_H__

#include "gdkgltexture.h"

#include "gdktextureprivate.h"

G_BEGIN_DECLS

GdkGLContext *          gdk_gl_texture_get_context      (GdkGLTexture           *self);
guint                   gdk_gl_texture_get_id           (GdkGLTexture           *self);
GdkGLTextureFlags       gdk_gl_texture_get_flags        (GdkGLTexture           *self);

G_END_DECLS

#endif /* __GDK_GL_TEXTURE_PRIVATE_H__ */
