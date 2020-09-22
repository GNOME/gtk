#ifndef __GSK_GL_UNIFORM_H__
#define __GSK_GL_UNIFORM_H__

#include "gskglshader.h"

typedef struct
{
  char *name;
  GskGLUniformType type;
  int offset;
} GskGLUniform;


const GskGLUniform *gsk_glshader_get_uniforms (GskGLShader *shader,
                                               int         *n_uniforms);

#endif
