#include "config.h"

#include "gskglpipelineprivate.h"

#include "gskdebugprivate.h"
#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpushaderflagsprivate.h"
#include "gskgpushaderopprivate.h"

#include "gdkprofilerprivate.h"

struct _GskGLPipeline
{
  GskGpuCached parent;

  const GskGpuShaderOpClass *op_class;
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;

  GLuint program_id;
};

static void
gsk_gl_pipeline_finalize (GskGpuCached *cached)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);
  GskGLPipeline *self = (GskGLPipeline *) cached;

  g_hash_table_remove (priv->pipeline_cache, self);

  glDeleteProgram (self->program_id);
}

static gboolean
gsk_gl_pipeline_should_collect (GskGpuCached *cached,
                                gint64        cache_timeout,
                                gint64        timestamp)
{
  return gsk_gpu_cached_is_old (cached, cache_timeout, timestamp);
}

static const GskGpuCachedClass GSK_GL_PIPELINE_CLASS =
{
  sizeof (GskGLPipeline),
  "GLPipeline",
  FALSE,
  gsk_gpu_cached_print_no_stats,
  gsk_gl_pipeline_finalize,
  gsk_gl_pipeline_should_collect
};

static guint
gsk_gl_pipeline_hash (gconstpointer data)
{
  const GskGLPipeline *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         ((key->flags << 11) | (key->flags >> 21)) ^
         ((key->variation << 21) | ( key->variation >> 11)) ^
         key->color_states;
}

static gboolean
gsk_gl_pipeline_equal (gconstpointer a,
                       gconstpointer b)
{
  const GskGLPipeline *keya = a;
  const GskGLPipeline *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->flags == keyb->flags && 
         keya->color_states == keyb->color_states && 
         keya->variation == keyb->variation;
}

void
gsk_gl_pipeline_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->pipeline_cache = g_hash_table_new (gsk_gl_pipeline_hash, gsk_gl_pipeline_equal);
}

void
gsk_gl_pipeline_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_assert (g_hash_table_size (priv->pipeline_cache) == 0);
  g_hash_table_unref (priv->pipeline_cache);
}

static char *
prepend_line_numbers (char *code)
{
  GString *s;
  char *p;
  int line;

  s = g_string_new ("");
  p = code;
  line = 1;
  while (*p)
    {
      char *end = strchr (p, '\n');
      if (end)
        end = end + 1; /* Include newline */
      else
        end = p + strlen (p);

      g_string_append_printf (s, "%3d| ", line++);
      g_string_append_len (s, p, end - p);

      p = end;
    }

  g_free (code);

  return g_string_free (s, FALSE);
}

static gboolean
gsk_gl_pipeline_check_shader_error (const char  *name,
                                    int          shader_id,
                                    GError     **error)
{
  GLint status;
  GLint log_len;
  GLint code_len;
  char *log;
  char *code;

  glGetShaderiv (shader_id, GL_COMPILE_STATUS, &status);

  if G_LIKELY (status == GL_TRUE)
    return TRUE;

  glGetShaderiv (shader_id, GL_INFO_LOG_LENGTH, &log_len);
  log = g_malloc0 (log_len + 1);
  glGetShaderInfoLog (shader_id, log_len, NULL, log);

  glGetShaderiv (shader_id, GL_SHADER_SOURCE_LENGTH, &code_len);
  code = g_malloc0 (code_len + 1);
  glGetShaderSource (shader_id, code_len, NULL, code);

  code = prepend_line_numbers (code);

  g_set_error (error,
               GDK_GL_ERROR,
               GDK_GL_ERROR_COMPILATION_FAILED,
               "Compilation failure in shader %s.\n"
               "Source Code:\n"
               "%s\n"
               "\n"
               "Error Message:\n"
               "%s\n"
               "\n",
               name,
               code,
               log);

  g_free (code);
  g_free (log);

  return FALSE;
}

static void
print_shader_info (const char *prefix,
                   GLuint      shader_id,
                   const char *name)
{
  if (GSK_DEBUG_CHECK (SHADERS))
    {
      int code_len;

      glGetShaderiv (shader_id, GL_SHADER_SOURCE_LENGTH, &code_len);

      if (code_len > 0)
        {
          char *code;

          code = g_malloc0 (code_len + 1);
          glGetShaderSource (shader_id, code_len, NULL, code);

          code = prepend_line_numbers (code);

          g_message ("%s %d, %s:\n%s",
                     prefix, shader_id,
                     name ? name : "unnamed",
                     code);
          g_free (code);
        }
    }
}

static GLuint
gsk_gl_pipeline_load_shader (GskGLDevice       *device,
                             const char        *program_name,
                             GLenum             shader_type,
                             GskGpuShaderFlags  flags,
                             GskGpuColorStates  color_states,
                             guint32            variation,
                             GError           **error)
{
  GString *preamble;
  char *resource_name;
  GBytes *bytes;
  GLuint shader_id;

  preamble = g_string_new (NULL);

  g_string_append (preamble, gsk_gl_device_get_version_string (device));
  g_string_append (preamble, "\n");
  if (gsk_gl_device_get_gl_api (device) == GDK_GL_API_GLES)
    {
      if (gsk_gl_device_has_gl_feature (device, GDK_GL_FEATURE_BLEND_FUNC_EXTENDED))
        g_string_append (preamble, "#extension GL_EXT_blend_func_extended : require\n");
      if (gsk_gpu_shader_flags_has_external_textures (flags))
        {
          g_string_append (preamble, "#extension GL_OES_EGL_image_external_essl3 : require\n");
          g_string_append (preamble, "#extension GL_OES_EGL_image_external : require\n");
        }
      g_string_append (preamble, "#define GSK_GLES 1\n");
    }
  else
    {
      g_assert (!gsk_gpu_shader_flags_has_external_textures (flags));
    }

  /* work with AMD's compiler, too */
  if (gsk_gpu_shader_flags_has_external_texture0 (flags))
    g_string_append (preamble, "#define GSK_TEXTURE0_IS_EXTERNAL 1\n");
  if (gsk_gpu_shader_flags_has_external_texture1 (flags))
    g_string_append (preamble, "#define GSK_TEXTURE1_IS_EXTERNAL 1\n");

  switch (shader_type)
    {
      case GL_VERTEX_SHADER:
        g_string_append (preamble, "#define GSK_VERTEX_SHADER 1\n");
        break;

      case GL_FRAGMENT_SHADER:
        g_string_append (preamble, "#define GSK_FRAGMENT_SHADER 1\n");
        break;

      default:
        g_assert_not_reached ();
        return 0;
    }

  g_string_append_printf (preamble, "#define GSK_FLAGS %uu\n", flags);
  /* We define this here so the bad shader compilers don't have to parse complex #if checks */
  if (gsk_gpu_shader_flags_has_clip_mask (flags))
    g_string_append (preamble, "#define GSK_GL_HAS_CLIP_MASK\n");
  g_string_append_printf (preamble, "#define GSK_COLOR_STATES %uu\n", color_states);
  g_string_append_printf (preamble, "#define GSK_VARIATION %uu\n", variation);

  resource_name = g_strconcat ("/org/gtk/libgsk/shaders/gl/", program_name, "-generated.glsl", NULL);
  bytes = g_resources_lookup_data (resource_name, 0, error);
  g_free (resource_name);
  if (bytes == NULL)
    return 0;

  shader_id = glCreateShader (shader_type);

  glShaderSource (shader_id,
                  2,
                  (const char *[]) {
                    preamble->str,
                    g_bytes_get_data (bytes, NULL),
                  },
                  NULL);

  g_bytes_unref (bytes);
  g_string_free (preamble, TRUE);

  glCompileShader (shader_id);

  print_shader_info (shader_type == GL_FRAGMENT_SHADER ? "fragment" : "vertex", shader_id, program_name);

  if (!gsk_gl_pipeline_check_shader_error (program_name, shader_id, error))
    {
      glDeleteShader (shader_id);
      return 0;
    }

  return shader_id;
}

static GLuint
gsk_gl_pipeline_load_program (GskGLDevice               *device,
                              const GskGpuShaderOpClass *op_class,
                              GskGpuShaderFlags          flags,
                              GskGpuColorStates          color_states,
                              guint32                    variation,
                              GError                   **error)
{
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  GLuint vertex_shader_id, fragment_shader_id, program_id;
  GLint link_status;

  vertex_shader_id = gsk_gl_pipeline_load_shader (device, op_class->shader_name, GL_VERTEX_SHADER, flags, color_states, variation, error);
  if (vertex_shader_id == 0)
    return 0;

  fragment_shader_id = gsk_gl_pipeline_load_shader (device, op_class->shader_name, GL_FRAGMENT_SHADER, flags, color_states, variation, error);
  if (fragment_shader_id == 0)
    return 0;

  program_id = glCreateProgram ();

  glAttachShader (program_id, vertex_shader_id);
  glAttachShader (program_id, fragment_shader_id);

  op_class->setup_attrib_locations (program_id);

  glLinkProgram (program_id);

  glGetProgramiv (program_id, GL_LINK_STATUS, &link_status);

  glDetachShader (program_id, vertex_shader_id);
  glDeleteShader (vertex_shader_id);
  glDetachShader (program_id, fragment_shader_id);
  glDeleteShader (fragment_shader_id);

  if (link_status == GL_FALSE)
    {
      char *buffer = NULL;
      int log_len = 0;

      glGetProgramiv (program_id, GL_INFO_LOG_LENGTH, &log_len);

      if (log_len > 0)
        {
          /* log_len includes NULL */
          buffer = g_malloc0 (log_len);
          glGetProgramInfoLog (program_id, log_len, NULL, buffer);
        }

      g_set_error (error,
                   GDK_GL_ERROR,
                   GDK_GL_ERROR_LINK_FAILED,
                   "Linking failure in shader: %s",
                   buffer ? buffer : "");

      g_free (buffer);

      glDeleteProgram (program_id);

      return 0;
    }

  gdk_profiler_end_markf (begin_time,
                          "Compile Program",
                          "name=%s id=%u frag=%u vert=%u",
                          op_class->shader_name, program_id, fragment_shader_id, vertex_shader_id);

  return program_id;
}

void
gsk_gl_pipeline_use (GskGLDevice               *device,
                     const GskGpuShaderOpClass *op_class,
                     GskGpuShaderFlags          flags,
                     GskGpuColorStates          color_states,
                     guint32                    variation)
{
  GskGpuCache *cache = gsk_gpu_device_get_cache (GSK_GPU_DEVICE (device));
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GError *error = NULL;
  GLuint program_id;
  GskGLPipeline *result;

  result = g_hash_table_lookup (priv->pipeline_cache,
                                &(GskGLPipeline) {
                                    .op_class = op_class,
                                    .flags = flags,
                                    .color_states = color_states,
                                    .variation = variation,
                                });
  if (result)
    {
      glUseProgram (result->program_id);
      gsk_gpu_cached_use ((GskGpuCached *) result);
      return;
    }

  program_id = gsk_gl_pipeline_load_program (device, op_class, flags, color_states, variation, &error);
  if (program_id == 0)
    {
      g_critical ("Failed to load shader program: %s", error->message);
      g_clear_error (&error);
      return;
    }
  
  glUseProgram (program_id);

  /* space by 3 because external textures may need 3 texture units */
  if (op_class->n_textures >= 1)
    {
      glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE0"), 0);
      if (!gsk_gpu_shader_flags_has_external_texture0 (flags))
        {
          glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE0_1"), 1);
          glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE0_2"), 2);
        }
    }
  if (op_class->n_textures >= 2)
    {
      glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE1"), 3);
      if (!gsk_gpu_shader_flags_has_external_texture1 (flags))
        {
          glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE1_1"), 4);
          glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE1_2"), 5);
        }
    }
  if (gsk_gpu_shader_flags_has_clip_mask (flags))
    glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE_MASK"), 6);

  result = gsk_gpu_cached_new (cache, &GSK_GL_PIPELINE_CLASS);
  result->op_class = op_class,
  result->flags = flags,
  result->color_states = color_states,
  result->variation = variation,
  result->program_id = program_id;

  g_hash_table_insert (priv->pipeline_cache, result, result);

  gsk_gpu_cached_use ((GskGpuCached *) result);
}

