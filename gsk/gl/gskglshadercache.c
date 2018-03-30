#include "config.h"

#include "gskglshadercacheprivate.h"

#include "gskdebugprivate.h"

#include <epoxy/gl.h>

struct _GskGLShaderCache
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskGLShaderCache, gsk_gl_shader_cache, G_TYPE_OBJECT)

static void
gsk_gl_shader_cache_class_init (GskGLShaderCacheClass *klass)
{
}

static void
gsk_gl_shader_cache_init (GskGLShaderCache *self)
{
}

GskGLShaderCache *
gsk_gl_shader_cache_new (void)
{
  return g_object_new (GSK_TYPE_GL_SHADER_CACHE, NULL);
}

int
gsk_gl_shader_cache_compile_shader (GskGLShaderCache  *cache,
                                    int                shader_type,
                                    const char        *source,
                                    GError           **error)
{
  int shader_id = glCreateShader (shader_type);
  glShaderSource (shader_id, 1, (const GLchar **) &source, NULL);
  glCompileShader (shader_id);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (SHADERS))
    {
      g_print ("*** Compiling %s shader ***\n"
               "%s\n",
               shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
               source);
    }
#endif

  int status = GL_FALSE;
  glGetShaderiv (shader_id, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len = 0;
      glGetShaderiv (shader_id, GL_INFO_LOG_LENGTH, &log_len);

      char *buffer = g_malloc0 (log_len + 1);
      glGetShaderInfoLog (shader_id, log_len, NULL, buffer);

      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_COMPILATION_FAILED,
                   "Compilation failure in %s shader:\n%s",
                   shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                   buffer);

      g_free (buffer);

      glDeleteShader (shader_id);
      shader_id = -1;
    }

  return shader_id;
}

int
gsk_gl_shader_cache_link_program (GskGLShaderCache  *cache,
                                  int                vertex_shader,
                                  int                fragment_shader,
                                  GError           **error)
{
  int program_id = glCreateProgram ();

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (SHADERS))
    {
      g_print ("*** Linking %d, %d shaders ***\n",
               vertex_shader,
               fragment_shader);
    }
#endif

  glAttachShader (program_id, vertex_shader);
  glAttachShader (program_id, fragment_shader);
  glLinkProgram (program_id);

  int status = GL_FALSE;
  glGetProgramiv (program_id, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len = 0;
      glGetProgramiv (program_id, GL_INFO_LOG_LENGTH, &log_len);

      char *buffer = g_malloc0 (log_len + 1);
      glGetProgramInfoLog (program_id, log_len, NULL, buffer);

      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_LINK_FAILED,
                   "Linking failure in shader:\n%s", buffer);

      g_free (buffer);

      glDeleteProgram (program_id);
      program_id = -1;
    }

  if (vertex_shader > 0)
    {
      if (program_id > 0)
        glDetachShader (program_id, vertex_shader);

      glDeleteShader (vertex_shader);
    }

  if (fragment_shader > 0)
    {
      if (program_id > 0)
        glDetachShader (program_id, fragment_shader);

      glDeleteShader (fragment_shader);
    }

  return program_id;
}
