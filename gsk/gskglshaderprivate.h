#ifndef __GSK_GLSHADER_PRIVATE_H__
#define __GSK_GLSHADER_PRIVATE_H__

#include <gsk/gskglshader.h>

G_BEGIN_DECLS

typedef struct {
  char *name;
  GskGLUniformType type;
  gsize offset;
} GskGLUniform;

const GskGLUniform *gsk_gl_shader_get_uniforms (GskGLShader *shader,
                                                int         *n_uniforms);

G_END_DECLS

#endif /* __GSK_GLSHADER_PRIVATE_H__ */
