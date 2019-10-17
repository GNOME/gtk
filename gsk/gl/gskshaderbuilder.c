#include "config.h"

#include "gskshaderbuilderprivate.h"

#include "gskdebugprivate.h"

#include <gdk/gdk.h>
#include <epoxy/gl.h>

struct _GskShaderBuilder
{
  GObject parent_instance;

  char *resource_base_path;
  char *vertex_preamble;
  char *fragment_preamble;


  int common_vertex_shader_id;

  int version;

  GPtrArray *defines;

  /* We reuse this one for all the shaders */
  GString *shader_code;
};

G_DEFINE_TYPE (GskShaderBuilder, gsk_shader_builder, G_TYPE_OBJECT)

static void
gsk_shader_builder_finalize (GObject *gobject)
{
  GskShaderBuilder *self = GSK_SHADER_BUILDER (gobject);

  g_free (self->resource_base_path);
  g_free (self->vertex_preamble);
  g_free (self->fragment_preamble);
  g_string_free (self->shader_code, TRUE);

  g_clear_pointer (&self->defines, g_ptr_array_unref);

  if (self->common_vertex_shader_id > 0)
    glDeleteShader (self->common_vertex_shader_id);

  G_OBJECT_CLASS (gsk_shader_builder_parent_class)->finalize (gobject);
}

static void
gsk_shader_builder_class_init (GskShaderBuilderClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_shader_builder_finalize;
}

static void
gsk_shader_builder_init (GskShaderBuilder *self)
{
  self->defines = g_ptr_array_new_with_free_func (g_free);
  self->shader_code = g_string_new (NULL);
}

GskShaderBuilder *
gsk_shader_builder_new (void)
{
  return g_object_new (GSK_TYPE_SHADER_BUILDER, NULL);
}

void
gsk_shader_builder_set_resource_base_path (GskShaderBuilder *builder,
                                           const char       *base_path)
{
  g_return_if_fail (GSK_IS_SHADER_BUILDER (builder));

  g_free (builder->resource_base_path);
  builder->resource_base_path = g_strdup (base_path);
}

void
gsk_shader_builder_set_vertex_preamble (GskShaderBuilder *builder,
                                        const char       *vertex_preamble)
{
  g_return_if_fail (GSK_IS_SHADER_BUILDER (builder));

  g_free (builder->vertex_preamble);
  builder->vertex_preamble = g_strdup (vertex_preamble);
}

void
gsk_shader_builder_set_fragment_preamble (GskShaderBuilder *builder,
                                          const char       *fragment_preamble)
{
  g_return_if_fail (GSK_IS_SHADER_BUILDER (builder));

  g_free (builder->fragment_preamble);
  builder->fragment_preamble = g_strdup (fragment_preamble);
}

void
gsk_shader_builder_set_version (GskShaderBuilder *builder,
                                int               version)
{
  g_return_if_fail (GSK_IS_SHADER_BUILDER (builder));

  builder->version = version;
}

void
gsk_shader_builder_add_define (GskShaderBuilder *builder,
                               const char       *define_name,
                               const char       *define_value)
{
  g_return_if_fail (GSK_IS_SHADER_BUILDER (builder));
  g_return_if_fail (define_name != NULL && *define_name != '\0');
  g_return_if_fail (define_value != NULL && *define_value != '\0');

  g_ptr_array_add (builder->defines, g_strdup (define_name));
  g_ptr_array_add (builder->defines, g_strdup (define_value));
}

static gboolean
lookup_shader_code (GString *code,
                    const char *base_path,
                    const char *shader_file,
                    GError **error)
{
  GBytes *source;
  char *path;

  if (base_path != NULL)
    path = g_build_filename (base_path, shader_file, NULL);
  else
    path = g_strdup (shader_file);

  source = g_resources_lookup_data (path, 0, error);
  g_free (path);

  if (source == NULL)
    return FALSE;

  g_string_append (code, g_bytes_get_data (source, NULL));

  g_bytes_unref (source);

  return TRUE;
}

static int
gsk_shader_builder_compile_shader (GskShaderBuilder *builder,
                                   int               shader_type,
                                   const char       *shader_preamble,
                                   const char       *shader_source,
                                   GError          **error)
{
  GString *code;
  const char *source;
  int shader_id;
  int status;
  int i;

  /* Clear possibly previously set shader code */
  g_string_erase (builder->shader_code, 0, -1);
  code = builder->shader_code;

  if (builder->version > 0)
    {
      g_string_append_printf (code, "#version %d\n", builder->version);
      g_string_append_c (code, '\n');
    }

  for (i = 0; i < builder->defines->len; i += 2)
    {
      const char *name = g_ptr_array_index (builder->defines, i);
      const char *value = g_ptr_array_index (builder->defines, i + 1);

      g_string_append (code, "#define");
      g_string_append_c (code, ' ');
      g_string_append (code, name);
      g_string_append_c (code, ' ');
      g_string_append (code, value);
      g_string_append_c (code, '\n');
    }

  g_string_append_c (code, '\n');

  if (!lookup_shader_code (code, builder->resource_base_path, shader_preamble, error))
    {
      return -1;
    }

  g_string_append_c (code, '\n');

  if (!lookup_shader_code (code, builder->resource_base_path, shader_source, error))
    {
      return -1;
    }

  source = code->str;

  shader_id = glCreateShader (shader_type);
  glShaderSource (shader_id, 1, (const GLchar **) &source, NULL);
  glCompileShader (shader_id);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (SHADERS))
    {
      g_print ("*** Compiling %s shader from '%s' + '%s' ***\n"
               "%s\n",
               shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
               shader_preamble, shader_source,
               source);
    }
#endif

  glGetShaderiv (shader_id, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetShaderiv (shader_id, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc0 (log_len + 1);
      glGetShaderInfoLog (shader_id, log_len, NULL, buffer);

      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_COMPILATION_FAILED,
                   "Compilation failure in %s shader:\n%s",
                   shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                   buffer);
      g_free (buffer);

      glDeleteShader (shader_id);

      return -1;
    }

  return shader_id;
}

void
gsk_shader_builder_set_common_vertex_shader (GskShaderBuilder  *self,
                                             const char        *vertex_shader,
                                             GError           **error)
{
  int shader_id;


  shader_id = gsk_shader_builder_compile_shader (self,
                                                 GL_VERTEX_SHADER,
                                                 self->vertex_preamble,
                                                 vertex_shader,
                                                 error);

  g_assert (shader_id > 0);
  self->common_vertex_shader_id = shader_id;
}

int
gsk_shader_builder_create_program (GskShaderBuilder *builder,
                                   const char       *fragment_shader,
                                   const char       *vertex_shader,
                                   GError          **error)
{
  int vertex_id;
  int fragment_id;
  int program_id;
  int status;

  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), -1);
  g_return_val_if_fail (fragment_shader != NULL, -1);
  g_return_val_if_fail (builder->common_vertex_shader_id != 0, -1);

  if (vertex_shader == NULL)
    vertex_id = builder->common_vertex_shader_id;
  else
    vertex_id = gsk_shader_builder_compile_shader (builder, GL_VERTEX_SHADER,
                                                   builder->vertex_preamble,
                                                   vertex_shader,
                                                   error);
  if (vertex_id < 0)
      return -1;

  fragment_id = gsk_shader_builder_compile_shader (builder, GL_FRAGMENT_SHADER,
                                                   builder->fragment_preamble,
                                                   fragment_shader,
                                                   error);
  if (fragment_id < 0)
    {
      glDeleteShader (vertex_id);
      return -1;
    }

  program_id = glCreateProgram ();
  glAttachShader (program_id, vertex_id);
  glAttachShader (program_id, fragment_id);
  glLinkProgram (program_id);

  glGetProgramiv (program_id, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      char *buffer = NULL;
      int log_len = 0;

      glGetProgramiv (program_id, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc0 (log_len + 1);
      glGetProgramInfoLog (program_id, log_len, NULL, buffer);

      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_LINK_FAILED,
                   "Linking failure in shader:\n%s", buffer);
      g_free (buffer);

      glDeleteProgram (program_id);
      program_id = -1;

      goto out;
    }

out:
  if (vertex_id > 0)
    {
      /* We delete the common vertex shader when destroying the shader builder */
      glDetachShader (program_id, vertex_id);
    }

  if (fragment_id > 0)
    {
      glDetachShader (program_id, fragment_id);
      glDeleteShader (fragment_id);
    }

  return program_id;
}
