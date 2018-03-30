#pragma once

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GSK_TYPE_GL_SHADER_CACHE (gsk_gl_shader_cache_get_type ())

G_DECLARE_FINAL_TYPE (GskGLShaderCache, gsk_gl_shader_cache, GSK, GL_SHADER_CACHE, GObject)

GskGLShaderCache *      gsk_gl_shader_cache_new                 (void);

int                     gsk_gl_shader_cache_compile_shader      (GskGLShaderCache  *cache,
                                                                 int                shader_type,
                                                                 const char        *code,
                                                                 GError           **error);
int                     gsk_gl_shader_cache_link_program        (GskGLShaderCache  *cache,
                                                                 int                vertex_shader,
                                                                 int                fragment_shader,
                                                                 GError           **error);

G_END_DECLS
