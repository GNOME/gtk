#ifndef __GSK_GL_RENDERER_PRIVATE_H__
#define __GSK_GL_RENDERER_PRIVATE_H__

#include "gskglrenderer.h"

G_BEGIN_DECLS

gboolean gsk_gl_renderer_try_compile_gl_shader (GskGLRenderer    *self,
                                                GskGLShader      *shader,
                                                GError          **error);

G_END_DECLS

#endif /* __GSK_GL_RENDERER_PRIVATE_H__ */
