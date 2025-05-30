#include "config.h"

#include "gskgldeviceprivate.h"

#include "gskdebugprivate.h"
#include "gskgpushaderflagsprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskglbufferprivate.h"
#include "gskglimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"

#include <glib/gi18n-lib.h>

struct _GskGLDevice
{
  GskGpuDevice parent_instance;

  GHashTable *gl_programs;
  const char *version_string;
  GdkGLAPI api;

  guint sampler_ids[GSK_GPU_SAMPLER_N_SAMPLERS];
};

struct _GskGLDeviceClass
{
  GskGpuDeviceClass parent_class;
};

typedef struct _GLProgramKey GLProgramKey;

struct _GLProgramKey
{
  const GskGpuShaderOpClass *op_class;
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;
};

G_DEFINE_TYPE (GskGLDevice, gsk_gl_device, GSK_TYPE_GPU_DEVICE)

static guint
gl_program_key_hash (gconstpointer data)
{
  const GLProgramKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         ((key->flags << 11) | (key->flags >> 21)) ^
         ((key->variation << 21) | ( key->variation >> 11)) ^
         key->color_states;
}

static gboolean
gl_program_key_equal (gconstpointer a,
                      gconstpointer b)
{
  const GLProgramKey *keya = a;
  const GLProgramKey *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->flags == keyb->flags && 
         keya->color_states == keyb->color_states && 
         keya->variation == keyb->variation;
}

static GskGpuImage *
gsk_gl_device_create_offscreen_image (GskGpuDevice   *device,
                                      gboolean        with_mipmap,
                                      GdkMemoryFormat format,
                                      gboolean        is_srgb,
                                      gsize           width,
                                      gsize           height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           format,
                           is_srgb,
                           GSK_GPU_IMAGE_RENDERABLE | GSK_GPU_IMAGE_FILTERABLE,
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_upload_image (GskGpuDevice     *device,
                                   gboolean          with_mipmap,
                                   GdkMemoryFormat   format,
                                   GskGpuConversion  conv,
                                   gsize             width,
                                   gsize             height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           format,
                           conv == GSK_GPU_CONVERSION_SRGB,
                           0,
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_download_image (GskGpuDevice   *device,
                                     GdkMemoryDepth  depth,
                                     gsize           width,
                                     gsize           height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           gdk_memory_depth_get_format (depth),
                           gdk_memory_depth_is_srgb (depth),
                           GSK_GPU_IMAGE_RENDERABLE | GSK_GPU_IMAGE_DOWNLOADABLE,
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_atlas_image (GskGpuDevice *device,
                                  gsize         width,
                                  gsize         height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           GDK_MEMORY_DEFAULT,
                           FALSE,
                           GSK_GPU_IMAGE_RENDERABLE,
                           width,
                           height);
}

static void
gsk_gl_device_make_current (GskGpuDevice *device)
{
  gdk_gl_context_make_current (gdk_display_get_gl_context (gsk_gpu_device_get_display (device)));
}

static void
gsk_gl_device_finalize (GObject *object)
{
  GskGLDevice *self = GSK_GL_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-gl-device");

  gdk_gl_context_make_current (gdk_display_get_gl_context (gsk_gpu_device_get_display (device)));

  g_hash_table_unref (self->gl_programs);
  glDeleteSamplers (G_N_ELEMENTS (self->sampler_ids), self->sampler_ids);

  G_OBJECT_CLASS (gsk_gl_device_parent_class)->finalize (object);
}

static void
gsk_gl_device_class_init (GskGLDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_gl_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_gl_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_gl_device_create_upload_image;
  gpu_device_class->create_download_image = gsk_gl_device_create_download_image;
  gpu_device_class->make_current = gsk_gl_device_make_current;

  object_class->finalize = gsk_gl_device_finalize;
}

static void
free_gl_program (gpointer program)
{
  glDeleteProgram (GPOINTER_TO_UINT (program));
}

static void
gsk_gl_device_init (GskGLDevice *self)
{
  self->gl_programs = g_hash_table_new_full (gl_program_key_hash, gl_program_key_equal, g_free, free_gl_program);
}

static void
gsk_gl_device_setup_samplers (GskGLDevice *self)
{
  struct {
    GLuint min_filter;
    GLuint mag_filter;
    GLuint wrap;
  } sampler_flags[GSK_GPU_SAMPLER_N_SAMPLERS] = {
    [GSK_GPU_SAMPLER_DEFAULT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_CLAMP_TO_EDGE,
    },
    [GSK_GPU_SAMPLER_TRANSPARENT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_CLAMP_TO_BORDER,
    },
    [GSK_GPU_SAMPLER_REPEAT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_REPEAT,
    },
    [GSK_GPU_SAMPLER_NEAREST] = {
      .min_filter = GL_NEAREST,
      .mag_filter = GL_NEAREST,
      .wrap = GL_CLAMP_TO_EDGE,
    },
    [GSK_GPU_SAMPLER_MIPMAP_DEFAULT] = {
      .min_filter = GL_LINEAR_MIPMAP_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_CLAMP_TO_EDGE,
    }
  };
  guint i;

  glGenSamplers (G_N_ELEMENTS (self->sampler_ids), self->sampler_ids);

  for (i = 0; i < G_N_ELEMENTS (self->sampler_ids); i++)
    {
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_MIN_FILTER, sampler_flags[i].min_filter);
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_MAG_FILTER, sampler_flags[i].mag_filter);
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_WRAP_S, sampler_flags[i].wrap);
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_WRAP_T, sampler_flags[i].wrap);
    }
}

GskGpuDevice *
gsk_gl_device_get_for_display (GdkDisplay  *display,
                               GError     **error)
{
  GskGLDevice *self;
  GdkGLContext *context;
  GLint max_texture_size, globals_alignment;

  self = g_object_get_data (G_OBJECT (display), "-gsk-gl-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_prepare_gl (display, error))
    return NULL;

  context = gdk_display_get_gl_context (display);

  /* GLES 2 is not supported */
  if (!gdk_gl_context_check_version (context, "3.0", "3.0"))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   _("OpenGL ES 3.0 is not supported by this renderer."));
      return NULL;
    }

  self = g_object_new (GSK_TYPE_GL_DEVICE, NULL);

  gdk_gl_context_make_current (context);

  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
  glGetIntegerv (GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &globals_alignment);

  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        max_texture_size,
                        GSK_GPU_DEVICE_DEFAULT_TILE_SIZE,
                        globals_alignment);

  self->version_string = gdk_gl_context_get_glsl_version_string (context);
  self->api = gdk_gl_context_get_api (context);
  gsk_gl_device_setup_samplers (self);

  g_object_set_data (G_OBJECT (display), "-gsk-gl-device", self);

  return GSK_GPU_DEVICE (self);
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
gsk_gl_device_check_shader_error (const char  *name,
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
gsk_gl_device_load_shader (GskGLDevice       *self,
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

  g_string_append (preamble, self->version_string);
  g_string_append (preamble, "\n");
  if (self->api == GDK_GL_API_GLES)
    {
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
  g_string_append_printf (preamble, "#define GSK_COLOR_STATES %uu\n", color_states);
  g_string_append_printf (preamble, "#define GSK_VARIATION %uu\n", variation);

  resource_name = g_strconcat ("/org/gtk/libgsk/shaders/gl/", program_name, ".glsl", NULL);
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

  if (!gsk_gl_device_check_shader_error (program_name, shader_id, error))
    {
      glDeleteShader (shader_id);
      return 0;
    }

  return shader_id;
}

static GLuint
gsk_gl_device_load_program (GskGLDevice               *self,
                            const GskGpuShaderOpClass *op_class,
                            GskGpuShaderFlags          flags,
                            GskGpuColorStates          color_states,
                            guint32                    variation,
                            GError                   **error)
{
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  GLuint vertex_shader_id, fragment_shader_id, program_id;
  GLint link_status;

  vertex_shader_id = gsk_gl_device_load_shader (self, op_class->shader_name, GL_VERTEX_SHADER, flags, color_states, variation, error);
  if (vertex_shader_id == 0)
    return 0;

  fragment_shader_id = gsk_gl_device_load_shader (self, op_class->shader_name, GL_FRAGMENT_SHADER, flags, color_states, variation, error);
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
gsk_gl_device_use_program (GskGLDevice               *self,
                           const GskGpuShaderOpClass *op_class,
                           GskGpuShaderFlags          flags,
                           GskGpuColorStates          color_states,
                           guint32                    variation)
{
  GError *error = NULL;
  GLuint program_id;
  GLProgramKey key = {
    .op_class = op_class,
    .flags = flags,
    .color_states = color_states,
    .variation = variation,
  };

  program_id = GPOINTER_TO_UINT (g_hash_table_lookup (self->gl_programs, &key));
  if (program_id)
    {
      glUseProgram (program_id);
      return;
    }

  program_id = gsk_gl_device_load_program (self, op_class, flags, color_states, variation, &error);
  if (program_id == 0)
    {
      g_critical ("Failed to load shader program: %s", error->message);
      g_clear_error (&error);
      return;
    }
  
  g_hash_table_insert (self->gl_programs, g_memdup2 (&key, sizeof (GLProgramKey)), GUINT_TO_POINTER (program_id));

  glUseProgram (program_id);

  /* space by 3 because external textures may need 3 texture units */
  glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE0"), 0);
  if (!gsk_gpu_shader_flags_has_external_texture0 (flags))
    {
      glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE0_1"), 1);
      glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE0_2"), 2);
    }
  glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE1"), 3);
  if (!gsk_gpu_shader_flags_has_external_texture1 (flags))
    {
      glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE1_1"), 4);
      glUniform1i (glGetUniformLocation (program_id, "GSK_TEXTURE1_2"), 5);
    }
}

GLuint
gsk_gl_device_get_sampler_id (GskGLDevice   *self,
                              GskGpuSampler  sampler)
{
  g_return_val_if_fail (sampler < G_N_ELEMENTS (self->sampler_ids), 0);

  return self->sampler_ids[sampler];
}

static gboolean
gsk_gl_device_get_format_flags (GskGLDevice      *self,
                                GdkGLContext     *context,
                                GdkMemoryFormat   format,
                                GdkSwizzle        swizzle,
                                GskGpuImageFlags *out_flags)
{
  GdkGLMemoryFlags gl_flags;

  *out_flags = 0;

  gl_flags = gdk_gl_context_get_format_flags (context, format);

  if (!(gl_flags & GDK_GL_FORMAT_USABLE))
    return FALSE;

  if (gdk_shader_op_get_n_shaders (gdk_memory_format_get_default_shader_op (format)) <= 1)
    *out_flags |= GSK_GPU_IMAGE_DOWNLOADABLE;

  if ((gl_flags & GDK_GL_FORMAT_RENDERABLE) && gdk_swizzle_is_framebuffer_compatible (swizzle))
    *out_flags |= GSK_GPU_IMAGE_RENDERABLE;
  if (gl_flags & GDK_GL_FORMAT_FILTERABLE)
    *out_flags |= GSK_GPU_IMAGE_FILTERABLE;
  if ((gl_flags & (GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE)) == (GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE))
    {
      *out_flags |= GSK_GPU_IMAGE_CAN_MIPMAP;
      if (gdk_swizzle_is_identity (swizzle))
        *out_flags |= GSK_GPU_IMAGE_BLIT;
    }

  return TRUE;
}

void
gsk_gl_device_find_gl_format (GskGLDevice      *self,
                              GdkMemoryFormat   format,
                              GskGpuImageFlags  required_flags,
                              GdkMemoryFormat  *out_format,
                              GskGpuImageFlags *out_flags,
                              GLint            *out_gl_internal_format,
                              GLint            *out_gl_internal_srgb_format,
                              GLenum           *out_gl_format,
                              GLenum           *out_gl_type,
                              GdkSwizzle       *out_swizzle)
{
  GdkGLContext *context = gdk_gl_context_get_current ();
  GskGpuImageFlags flags;
  GdkMemoryFormat alt_format;
  const GdkMemoryFormat *fallbacks;
  gsize i;
  GdkSwizzle swizzle;

  /* First, try the actual format */
  if (gdk_memory_format_gl_format (format,
                                   0,
                                   gdk_gl_context_get_use_es (context),
                                   out_gl_internal_format,
                                   out_gl_internal_srgb_format,
                                   out_gl_format,
                                   out_gl_type,
                                   out_swizzle) &&
      gsk_gl_device_get_format_flags (self, context, format, *out_swizzle, &flags) &&
      ((flags & required_flags) == required_flags))
    {
      *out_format = format;
      *out_flags = flags;
      return;
    }

  /* Second, try the potential RGBA format */
  if (gdk_memory_format_get_rgba_format (format,
                                         &alt_format,
                                         &swizzle) &&
      gdk_memory_format_gl_format (alt_format,
                                   0,
                                   gdk_gl_context_get_use_es (context),
                                   out_gl_internal_format,
                                   out_gl_internal_srgb_format,
                                   out_gl_format,
                                   out_gl_type,
                                   out_swizzle))
    {
      *out_swizzle = swizzle;
      if (gsk_gl_device_get_format_flags (self, context, alt_format, *out_swizzle, &flags) &&
          ((flags & required_flags) == required_flags))
        {
          *out_format = format;
          *out_flags = flags;
          return;
        }
    }

  /* Next, try the fallbacks */
  fallbacks = gdk_memory_format_get_fallbacks (format);
  for (i = 0; fallbacks[i] != -1; i++)
    {
      if (gdk_memory_format_gl_format (fallbacks[i],
                                       0,
                                       gdk_gl_context_get_use_es (context),
                                       out_gl_internal_format,
                                       out_gl_internal_srgb_format,
                                       out_gl_format,
                                       out_gl_type,
                                       out_swizzle) &&
          gsk_gl_device_get_format_flags (self, context, fallbacks[i], *out_swizzle, &flags) &&
          ((flags & required_flags) == required_flags))
        {
          *out_format = fallbacks[i];
          *out_flags = flags;
          return;
        }
    }

  /* fallbacks will always fallback to a supported format */
  g_assert_not_reached ();
}

