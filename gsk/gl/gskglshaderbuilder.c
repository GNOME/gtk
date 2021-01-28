#include "config.h"

#include "gskglshaderbuilderprivate.h"

#include "gskdebugprivate.h"

#include <gdk/gdk.h>
#include <epoxy/gl.h>

void
gsk_gl_shader_builder_init (GskGLShaderBuilder *self,
                            const char         *common_preamble_resource_path,
                            const char         *vs_preamble_resource_path,
                            const char         *fs_preamble_resource_path)
{
  memset (self, 0, sizeof (*self));

  self->preamble = g_resources_lookup_data (common_preamble_resource_path, 0, NULL);
  self->vs_preamble = g_resources_lookup_data (vs_preamble_resource_path, 0, NULL);
  self->fs_preamble = g_resources_lookup_data (fs_preamble_resource_path, 0, NULL);

  g_assert (self->preamble);
  g_assert (self->vs_preamble);
  g_assert (self->fs_preamble);
}

void
gsk_gl_shader_builder_finish (GskGLShaderBuilder *self)
{
  g_bytes_unref (self->preamble);
  g_bytes_unref (self->vs_preamble);
  g_bytes_unref (self->fs_preamble);
}

void
gsk_gl_shader_builder_set_glsl_version (GskGLShaderBuilder *self,
                                        int                 version)
{
  self->version = version;
}

static void
prepend_line_numbers (char    *code,
                      GString *s)
{
  char *p;
  int line;

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
}

static gboolean
check_shader_error (int          shader_id,
                    int          shader_type,
                    const char  *resource_path,
                    GError     **error)
{
  int status;
  int log_len;
  char *buffer;
  int code_len;
  char *code;
  GString *s;

  glGetShaderiv (shader_id, GL_COMPILE_STATUS, &status);

  if (G_LIKELY (status == GL_TRUE))
    return TRUE;

  glGetShaderiv (shader_id, GL_INFO_LOG_LENGTH, &log_len);
  buffer = g_malloc0 (log_len + 1);
  glGetShaderInfoLog (shader_id, log_len, NULL, buffer);

  glGetShaderiv (shader_id, GL_SHADER_SOURCE_LENGTH, &code_len);
  code = g_malloc0 (code_len + 1);
  glGetShaderSource (shader_id, code_len, NULL, code);

  s = g_string_new ("");
  prepend_line_numbers (code, s);

  g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_COMPILATION_FAILED,
               "Compilation failure in %s shader %s.\nSource Code:\n%s\n\nError Message:\n%s\n\n",
               (shader_type == GL_FRAGMENT_SHADER ? "fragment" : "vertex"),
               resource_path,
               s->str,
               buffer);

  g_string_free (s, TRUE);
  g_free (buffer);
  g_free (code);

  return FALSE;
}

static void
print_shader_info (const char *prefix,
                   int         shader_id,
                   const char *resource_path)
{
  if (GSK_DEBUG_CHECK(SHADERS))
    {
      int code_len;
      char *code;
      GString *s;

      glGetShaderiv (shader_id, GL_SHADER_SOURCE_LENGTH, &code_len);
      code = g_malloc0 (code_len + 1);
      glGetShaderSource (shader_id, code_len, NULL, code);

      s = g_string_new ("");
      prepend_line_numbers (code, s);

      g_message ("%s %d, %s:\n%s", prefix, shader_id, resource_path, s->str);
      g_string_free (s,  TRUE);
      g_free (code);
    }
}

int
gsk_gl_shader_builder_create_program (GskGLShaderBuilder  *self,
                                      const char          *resource_path,
                                      const char          *extra_fragment_snippet,
                                      gsize                extra_fragment_length,
                                      GError             **error)
{

  GBytes *source_bytes = g_resources_lookup_data (resource_path, 0, NULL);
  char version_buffer[64];
  const char *source;
  const char *vertex_shader_start;
  const char *fragment_shader_start;
  int vertex_id;
  int fragment_id;
  int program_id = -1;
  int status;

  g_assert (source_bytes);

  source = g_bytes_get_data (source_bytes, NULL);
  vertex_shader_start = strstr (source, "VERTEX_SHADER");
  fragment_shader_start = strstr (source, "FRAGMENT_SHADER");

  g_assert (vertex_shader_start);
  g_assert (fragment_shader_start);

  /* They both start at the next newline */
  vertex_shader_start = strstr (vertex_shader_start, "\n");
  fragment_shader_start = strstr (fragment_shader_start, "\n");

  g_snprintf (version_buffer, sizeof (version_buffer),
              "#version %d\n", self->version);

  vertex_id = glCreateShader (GL_VERTEX_SHADER);
  glShaderSource (vertex_id, 8,
                  (const char *[]) {
                    version_buffer,
                    self->debugging ? "#define GSK_DEBUG 1\n" : "",
                    self->legacy ? "#define GSK_LEGACY 1\n" : "",
                    self->gl3 ? "#define GSK_GL3 1\n" : "",
                    self->gles ? "#define GSK_GLES 1\n" : "",
                    g_bytes_get_data (self->preamble, NULL),
                    g_bytes_get_data (self->vs_preamble, NULL),
                    vertex_shader_start
                  },
                  (int[]) {
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    fragment_shader_start - vertex_shader_start
                  });
  glCompileShader (vertex_id);

  if (!check_shader_error (vertex_id, GL_VERTEX_SHADER, resource_path, error))
    {
      glDeleteShader (vertex_id);
      goto out;
    }

  print_shader_info ("Vertex shader", vertex_id, resource_path);

  fragment_id = glCreateShader (GL_FRAGMENT_SHADER);
  glShaderSource (fragment_id, 9,
                  (const char *[]) {
                    version_buffer,
                    self->debugging ? "#define GSK_DEBUG 1\n" : "",
                    self->legacy ? "#define GSK_LEGACY 1\n" : "",
                    self->gl3 ? "#define GSK_GL3 1\n" : "",
                    self->gles ? "#define GSK_GLES 1\n" : "",
                    g_bytes_get_data (self->preamble, NULL),
                    g_bytes_get_data (self->fs_preamble, NULL),
                    fragment_shader_start,
                    extra_fragment_snippet ? extra_fragment_snippet : ""
                  },
                  (int[]) {
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    -1,
                    extra_fragment_length,
                  });
  glCompileShader (fragment_id);

  if (!check_shader_error (fragment_id, GL_FRAGMENT_SHADER, resource_path, error))
    {
      glDeleteShader (fragment_id);
      goto out;
    }

  print_shader_info ("Fragment shader", fragment_id, resource_path);

  program_id = glCreateProgram ();
  glAttachShader (program_id, vertex_id);
  glAttachShader (program_id, fragment_id);
  glBindAttribLocation (program_id, 0, "aPosition");
  glBindAttribLocation (program_id, 1, "aUv");
  glLinkProgram (program_id);
  glDetachShader (program_id, vertex_id);
  glDetachShader (program_id, fragment_id);

  glGetProgramiv (program_id, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      char *buffer = NULL;
      int log_len = 0;

      glGetProgramiv (program_id, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc0 (log_len + 1);
      glGetProgramInfoLog (program_id, log_len, NULL, buffer);

      g_warning ("Linking failure in shader:\n%s", buffer);
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_LINK_FAILED,
                   "Linking failure in shader: %s", buffer);

      g_free (buffer);

      glDeleteProgram (program_id);
      program_id = -1;
    }

  glDeleteShader (vertex_id);
  glDeleteShader (fragment_id);

out:
  g_bytes_unref (source_bytes);

  return program_id;
}

