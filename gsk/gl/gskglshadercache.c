#include "config.h"

#include "gskglshadercacheprivate.h"

#include "gskdebugprivate.h"

#include <epoxy/gl.h>

struct _GskGLShaderCache
{
  GObject parent_instance;

  GHashTable *shader_cache;
};

G_DEFINE_TYPE (GskGLShaderCache, gsk_gl_shader_cache, G_TYPE_OBJECT)

static void
gsk_gl_shader_cache_finalize (GObject *gobject)
{
  GskGLShaderCache *self = GSK_GL_SHADER_CACHE (gobject);

  g_clear_pointer (&self->shader_cache, g_hash_table_unref);

  G_OBJECT_CLASS (gsk_gl_shader_cache_parent_class)->finalize (gobject);
}

static void
gsk_gl_shader_cache_class_init (GskGLShaderCacheClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_gl_shader_cache_finalize;
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
  if (cache->shader_cache == NULL)
    cache->shader_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free,
                                                 NULL);

  char *shasum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, source, -1);

  int shader_id = GPOINTER_TO_INT (g_hash_table_lookup (cache->shader_cache, shasum));

  if (shader_id != 0)
    {
      GSK_NOTE (SHADERS,
                g_debug ("*** Cache hit for %s shader (checksum: %s) ***\n"
                         "%*s%s\n",
                         shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                         shasum,
                         64, source,
                         strlen (source) < 64 ? "" : "..."));
      g_free (shasum);
      return shader_id;
    }

  shader_id = glCreateShader (shader_type);
  glShaderSource (shader_id, 1, (const GLchar **) &source, NULL);
  glCompileShader (shader_id);

  GSK_NOTE (SHADERS,
            g_debug ("*** Compiling %s shader ***\n"
                     "%*s%s\n",
                     shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                     64, source,
                     strlen (source) < 64 ? "" : "..."));

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
  else
    {
      g_hash_table_insert (cache->shader_cache,
                           shasum,
                           GINT_TO_POINTER (shader_id));
    }

  return shader_id;
}

int
gsk_gl_shader_cache_link_program (GskGLShaderCache  *cache,
                                  int                vertex_shader,
                                  int                fragment_shader,
                                  GError           **error)
{
  g_return_val_if_fail (vertex_shader > 0 && fragment_shader > 0, -1);

  int program_id = glCreateProgram ();

  GSK_NOTE (SHADERS,
            g_debug ("*** Linking %d, %d shaders ***\n",
                     vertex_shader,
                     fragment_shader));

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
  else
    {
      glDetachShader (program_id, vertex_shader);
      glDetachShader (program_id, fragment_shader);
    }

  return program_id;
}
