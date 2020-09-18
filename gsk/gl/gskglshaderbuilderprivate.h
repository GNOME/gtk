#ifndef __GSK_SHADER_BUILDER_PRIVATE_H__
#define __GSK_SHADER_BUILDER_PRIVATE_H__

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

typedef struct
{
  GBytes *preamble;
  GBytes *vs_preamble;
  GBytes *fs_preamble;

  int version;

  guint debugging: 1;
  guint gles: 1;
  guint gl3: 1;
  guint legacy: 1;

} GskGLShaderBuilder;


void   gsk_gl_shader_builder_init             (GskGLShaderBuilder  *self,
                                               const char          *common_preamble_resource_path,
                                               const char          *vs_preamble_resource_path,
                                               const char          *fs_preamble_resource_path);
void   gsk_gl_shader_builder_finish           (GskGLShaderBuilder  *self);

void   gsk_gl_shader_builder_set_glsl_version (GskGLShaderBuilder  *self,
                                               int                  version);

int    gsk_gl_shader_builder_create_program   (GskGLShaderBuilder  *self,
                                               const char          *resource_path,
                                               const char          *extra_fragment_snippet,
                                               gsize                extra_fragment_length,
                                               GError             **error);

G_END_DECLS

#endif /* __GSK_SHADER_BUILDER_PRIVATE_H__ */
