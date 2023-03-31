#pragma once

#include "gdkgltexture.h"

#include "gdktextureprivate.h"

G_BEGIN_DECLS

GdkGLContext *          gdk_gl_texture_get_context      (GdkGLTexture           *self);
guint                   gdk_gl_texture_get_id           (GdkGLTexture           *self);
gboolean                gdk_gl_texture_has_mipmap       (GdkGLTexture           *self);

G_END_DECLS

