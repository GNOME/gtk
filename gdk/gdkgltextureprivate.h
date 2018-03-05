#ifndef __GDK_GL_TEXTURE_PRIVATE_H__
#define __GDK_GL_TEXTURE_PRIVATE_H__

#include "gdkgltexture.h"

#include "gdktextureprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_GL_TEXTURE (gdk_gl_texture_get_type ())

G_DECLARE_FINAL_TYPE (GdkGLTexture, gdk_gl_texture, GDK, GL_TEXTURE, GdkTexture)

GdkGLContext *          gdk_gl_texture_get_context      (GdkGLTexture           *self);
guint                   gdk_gl_texture_get_id           (GdkGLTexture           *self);

G_END_DECLS

#endif /* __GDK_GL_TEXTURE_PRIVATE_H__ */
