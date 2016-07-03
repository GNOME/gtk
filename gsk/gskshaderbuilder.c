#include "config.h"

#include "gskshaderbuilderprivate.h"

#include "gskdebugprivate.h"

#include <gdk/gdk.h>
#include <epoxy/gl.h>

struct _GskShaderBuilder
{
  GObject parent_instance;

  char *resource_base_path;

  int version;

  int program_id;

  GPtrArray *defines;
  GPtrArray *uniforms;
  GPtrArray *attributes;

  GHashTable *uniform_locations;
  GHashTable *attribute_locations;
};

G_DEFINE_TYPE (GskShaderBuilder, gsk_shader_builder, G_TYPE_OBJECT)

static void
gsk_shader_builder_finalize (GObject *gobject)
{
  GskShaderBuilder *self = GSK_SHADER_BUILDER (gobject);

  g_free (self->resource_base_path);

  g_clear_pointer (&self->defines, g_ptr_array_unref);
  g_clear_pointer (&self->uniforms, g_ptr_array_unref);
  g_clear_pointer (&self->attributes, g_ptr_array_unref);

  g_clear_pointer (&self->uniform_locations, g_hash_table_unref);
  g_clear_pointer (&self->attribute_locations, g_hash_table_unref);

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
  self->uniforms = g_ptr_array_new_with_free_func (g_free);
  self->attributes = g_ptr_array_new_with_free_func (g_free);

  self->uniform_locations = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->attribute_locations = g_hash_table_new (g_direct_hash, g_direct_equal);
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
  g_return_if_fail (define_value != NULL && *define_name != '\0');

  g_ptr_array_add (builder->defines, g_strdup (define_name));
  g_ptr_array_add (builder->defines, g_strdup (define_value));
}

GQuark
gsk_shader_builder_add_uniform (GskShaderBuilder *builder,
                                const char       *uniform_name)
{
  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), 0);
  g_return_val_if_fail (uniform_name != NULL, 0);

  g_ptr_array_add (builder->uniforms, g_strdup (uniform_name));

  return g_quark_from_string (uniform_name);
}

GQuark
gsk_shader_builder_add_attribute (GskShaderBuilder *builder,
                                  const char       *attribute_name)
{
  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), 0);
  g_return_val_if_fail (attribute_name != NULL, 0);

  g_ptr_array_add (builder->attributes, g_strdup (attribute_name));

  return g_quark_from_string (attribute_name);
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

int
gsk_shader_builder_compile_shader (GskShaderBuilder *builder,
                                   int               shader_type,
                                   const char       *shader_preamble,
                                   const char       *shader_source,
                                   GError          **error)
{
  GString *code;
  char *source;
  int shader_id;
  int status;
  int i;

  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), -1);
  g_return_val_if_fail (shader_source != NULL, -1);

  code = g_string_new (NULL);

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

      if (i == builder->defines->len - 2)
        g_string_append_c (code, '\n');
    }

  if (!lookup_shader_code (code, builder->resource_base_path, shader_preamble, error))
    {
      g_string_free (code, TRUE);
      return -1;
    }

  g_string_append_c (code, '\n');

  if (!lookup_shader_code (code, builder->resource_base_path, shader_source, error))
    {
      g_string_free (code, TRUE);
      return -1;
    }

  source = g_string_free (code, FALSE);

  shader_id = glCreateShader (shader_type);
  glShaderSource (shader_id, 1, (const GLchar **) &source, NULL);
  glCompileShader (shader_id);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (OPENGL))
    {
      g_print ("*** Compiling %s shader ***\n"
               "%s\n",
               shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
               source);
    }
#endif

  g_free (source);

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

static void
gsk_shader_builder_cache_uniforms (GskShaderBuilder *builder,
                                   int               program_id)
{
  int i;

  for (i = 0; i < builder->uniforms->len; i++)
    {
      const char *uniform = g_ptr_array_index (builder->uniforms, i);
      int loc = glGetUniformLocation (program_id, uniform);

      g_hash_table_insert (builder->uniform_locations,
                           GINT_TO_POINTER (g_quark_from_string (uniform)),
                           GINT_TO_POINTER (loc));
    }
}

static void
gsk_shader_builder_cache_attributes (GskShaderBuilder *builder,
                                     int               program_id)
{
  int i;

  for (i = 0; i < builder->attributes->len; i++)
    {
      const char *attribute = g_ptr_array_index (builder->attributes, i);
      int loc = glGetAttribLocation (program_id, attribute);

      g_hash_table_insert (builder->attribute_locations,
                           GINT_TO_POINTER (g_quark_from_string (attribute)),
                           GINT_TO_POINTER (loc));
    }
}

int
gsk_shader_builder_create_program (GskShaderBuilder *builder,
                                   int               vertex_id,
                                   int               fragment_id,
                                   GError          **error)
{
  int program_id;
  int status;

  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), -1);
  g_return_val_if_fail (vertex_id > 0, -1);
  g_return_val_if_fail (fragment_id > 0, -1);

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

  gsk_shader_builder_cache_uniforms (builder, program_id);
  gsk_shader_builder_cache_attributes (builder, program_id);

  builder->program_id = program_id;

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (OPENGL))
    {
      GHashTableIter iter;
      gpointer name_p, location_p;

      g_hash_table_iter_init (&iter, builder->uniform_locations);
      while (g_hash_table_iter_next (&iter, &name_p, &location_p))
        {
          g_print ("Uniform '%s' - location: %d\n",
                   g_quark_to_string (GPOINTER_TO_INT (name_p)),
                   GPOINTER_TO_INT (location_p));
        }

      g_hash_table_iter_init (&iter, builder->attribute_locations);
      while (g_hash_table_iter_next (&iter, &name_p, &location_p))
        {
          g_print ("Attribute '%s' - location: %d\n",
                   g_quark_to_string (GPOINTER_TO_INT (name_p)),
                   GPOINTER_TO_INT (location_p));
        }
    }
#endif

out:
  glDetachShader (program_id, vertex_id);
  glDetachShader (program_id, fragment_id);

  return program_id;
}

int
gsk_shader_builder_get_uniform_location (GskShaderBuilder *builder,
                                         GQuark            uniform_quark)
{
  gpointer loc_p = NULL;

  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), -1);

  if (builder->program_id < 0)
    return -1;

  if (builder->uniforms->len == 0)
    return -1;

  if (g_hash_table_lookup_extended (builder->uniform_locations, GINT_TO_POINTER (uniform_quark), NULL, &loc_p))
    return GPOINTER_TO_INT (loc_p);

  return -1;
}

int
gsk_shader_builder_get_attribute_location (GskShaderBuilder *builder,
                                           GQuark            attribute_quark)
{
  gpointer loc_p = NULL;

  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), -1);

  if (builder->program_id < 0)
    return -1;

  if (builder->attributes->len == 0)
    return -1;

  if (g_hash_table_lookup_extended (builder->attribute_locations, GINT_TO_POINTER (attribute_quark), NULL, &loc_p))
    return GPOINTER_TO_INT (loc_p);

  return -1;
}

int
gsk_shader_builder_get_program (GskShaderBuilder *builder)
{
  g_return_val_if_fail (GSK_IS_SHADER_BUILDER (builder), -1);

  return builder->program_id;
}
