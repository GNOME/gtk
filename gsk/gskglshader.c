/* GSK - The GTK Scene Kit
 *
 * Copyright 2020, Red Hat Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:GskGLShader
 * @Title: GskGLShader
 * @Short_description: Fragment shaders for GSK
 *
 * A #GskGLShader is a snippet of GLSL that is meant to run in the
 * fragment shader of the rendering pipeline. A fragment shader
 * gets the coordinates being rendered as input and produces the
 * pixel values for that particular pixel. Additionally, the
 * shader can declare a set of other input arguments, called
 * uniforms (as they are uniform over all the calls to your shader in
 * each instance of use). A shader can also receive up to 4
 * textures that it can use as input when producing the pixel data.
 *
 * The typical way a #GskGLShader is used is as an argument to a
 * #GskGLShaderNode in the rendering hierarchy, and then the textures
 * it gets as input are constructed by rendering the child nodes to
 * textures before rendering the shader node itself. (Although you can
 * also pass texture nodes as children if you want to directly use a
 * texture as input). Note that the #GskGLShaderNode API is a bit
 * lowlevel, and highlevel code normally uses
 * gtk_snapshot_push_glshader() to produce the nodes.
 *
 * The actual shader code is GLSL code that gets combined with
 * some other code into the fragment shader. Since the exact
 * capabilities of the GPU driver differs between different OpenGL
 * drivers and hardware, GTK adds some defines that you can use
 * to ensure your GLSL code runs on as many drivers as it can.
 *
 * If the OpenGL driver is GLES, then the shader language version
 * is set to 100, and GSK_GLES will be defined in the shader.
 *
 * Otherwise, if the OpenGL driver does not support the 3.2 core profile,
 * then the shader will run with language version 110 for GL2 and 130 for GL3,
 * and GSK_LEGACY will be defined in the shader.
 *
 * If the OpenGL driver supports the 3.2 code profile, it will be used,
 * the shader language version is set to 150, and GSK_GL3 will be defined
 * in the shader.
 *
 * The main function the shader should implement is:
 *
 * |[<!-- language="plain" -->
 *  void mainImage(out vec4 fragColor,
 *                 in vec2 fragCoord,
 *                 in vec2 resolution,
 *                 in vec2 uv)
 * ]|
 * 
 * Where the input @fragCoord is the coordinate of the pixel we're
 * currently rendering, relative to the boundary rectangle that was
 * specified in the #GskGLShaderNode, and @resolution is the width and
 * height of that rectangle. This is in the typical GTK coordinate
 * system with the origin in the top left. @uv contains the u and v
 * coordinates that can be used to index a texture at the
 * corresponding point. These coordinates are in the [0..1]x[0..1]
 * region, with 0, 0 being in the lower left cornder (which is typical
 * for OpenGL).
 *
 * The output @fragColor should be a RGBA color (with
 * non-premultiplied alpha) that will be used as the output for the
 * specified pixel location. Note that this output will be
 * automatically clipped to the clip region of the glshader node.
 *
 * In addition to the function arguments the shader can define
 * up to 4 uniforms for tetxures which must be called u_textureN
 * (i.e. u_texture1 to u_texture4) as well as any custom uniforms
 * you want of types int, uint, bool, float, vec2, vec3 or vec4.
 *
 * Note that GTK parses the uniform declarations, so each uniform has to
 * be on a line by itself with no other code, like so:
 *
 * |[<!-- language="plain" -->
 * uniform float u_time;
 * uniform vec3 u_color;
 * uniform sampler2D u_texture1;
 * uniform sampler2D u_texture2;
 * ]|
 *
 * GTK uses the the "gsk" namespace in the symbols it uses in the
 * shader, so your code should not use any symbols with the prefix gsk
 * or GSK. There are some helper functions declared that you can use:
 *
 * |[<!-- language="plain" -->
 * vec4 GskTexture(sampler2D sampler, vec2 texCoords);
 * ]|
 *
 * This samples a texture (e.g. u_texture1) at the specified
 * coordinates, and containes some helper ifdefs to ensure that
 * it works on all OpenGL versions.
 */

#include "config.h"
#include "gskglshader.h"
#include "gskglshaderprivate.h"
#include "gskdebugprivate.h"

static GskGLUniformType
uniform_type_from_glsl (const char *str)
{
  if (strcmp (str, "int") == 0)
    return GSK_GLUNIFORM_TYPE_INT;
  if (strcmp (str, "uint") == 0)
    return GSK_GLUNIFORM_TYPE_UINT;
  if (strcmp (str, "bool") == 0)
    return GSK_GLUNIFORM_TYPE_BOOL;
  if (strcmp (str, "float") == 0)
    return GSK_GLUNIFORM_TYPE_FLOAT;
  if (strcmp (str, "vec2") == 0)
    return GSK_GLUNIFORM_TYPE_VEC2;
  if (strcmp (str, "vec3") == 0)
    return GSK_GLUNIFORM_TYPE_VEC3;
  if (strcmp (str, "vec4") == 0)
    return GSK_GLUNIFORM_TYPE_VEC4;

  return  GSK_GLUNIFORM_TYPE_NONE;
}

static const char *
uniform_type_name (GskGLUniformType type)
{
  switch (type)
    {
    case GSK_GLUNIFORM_TYPE_FLOAT:
      return "float";

    case GSK_GLUNIFORM_TYPE_INT:
      return "int";

    case GSK_GLUNIFORM_TYPE_UINT:
      return "uint";

    case GSK_GLUNIFORM_TYPE_BOOL:
      return "bool";

    case GSK_GLUNIFORM_TYPE_VEC2:
      return "vec2";

    case GSK_GLUNIFORM_TYPE_VEC3:
      return "vec3";

    case GSK_GLUNIFORM_TYPE_VEC4:
      return "vec4";

    case GSK_GLUNIFORM_TYPE_NONE:
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static int
uniform_type_size (GskGLUniformType type)
{
  switch (type)
    {
    case GSK_GLUNIFORM_TYPE_FLOAT:
      return sizeof (float);

    case GSK_GLUNIFORM_TYPE_INT:
      return sizeof (gint32);

    case GSK_GLUNIFORM_TYPE_UINT:
    case GSK_GLUNIFORM_TYPE_BOOL:
      return sizeof (guint32);

    case GSK_GLUNIFORM_TYPE_VEC2:
      return sizeof (float) * 2;

    case GSK_GLUNIFORM_TYPE_VEC3:
      return sizeof (float) * 3;

    case GSK_GLUNIFORM_TYPE_VEC4:
      return sizeof (float) * 4;

    case GSK_GLUNIFORM_TYPE_NONE:
    default:
      g_assert_not_reached ();
      return 0;
    }
}

struct _GskGLShader
{
  GObject parent_instance;
  GBytes *bytes;
  char *resource;
  int n_required_textures;
  int uniforms_size;
  GArray *uniforms;
};

G_DEFINE_TYPE (GskGLShader, gsk_gl_shader, G_TYPE_OBJECT)

enum {
  GLSHADER_PROP_0,
  GLSHADER_PROP_BYTES,
  GLSHADER_PROP_RESOURCE,
  GLSHADER_N_PROPS
};
static GParamSpec *gsk_gl_shader_properties[GLSHADER_N_PROPS];

static void
gsk_gl_shader_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GskGLShader *shader = GSK_GL_SHADER (object);

  switch (prop_id)
    {
    case GLSHADER_PROP_BYTES:
      g_value_set_boxed (value, shader->bytes);
      break;

    case GLSHADER_PROP_RESOURCE:
      g_value_set_string (value, shader->resource);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gsk_gl_shader_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GskGLShader *shader = GSK_GL_SHADER (object);

  switch (prop_id)
    {
    case GLSHADER_PROP_BYTES:
      g_clear_pointer (&shader->bytes, g_bytes_unref);
      shader->bytes = g_value_dup_boxed (value);
      break;

    case GLSHADER_PROP_RESOURCE:
      {
        GError *error = NULL;
        GBytes *bytes;
        const char *resource;

        resource = g_value_get_string (value);
        if (resource == NULL)
          break;

        bytes = g_resources_lookup_data (resource, 0, &error);
        if (bytes)
          {
            g_clear_pointer (&shader->bytes, g_bytes_unref);
            shader->bytes = bytes;
            shader->resource = g_strdup (resource);
          }
        else
          {
            g_critical ("Unable to load resource %s for glshader: %s", resource, error->message);
            g_error_free (error);
            if (shader->bytes == NULL)
              shader->bytes = g_bytes_new_static ("", 1);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gsk_gl_shader_finalize (GObject *object)
{
  GskGLShader *shader = GSK_GL_SHADER (object);

  g_bytes_unref (shader->bytes);
  g_free (shader->resource);
  for (int i = 0; i < shader->uniforms->len; i ++)
    g_free (g_array_index (shader->uniforms, GskGLUniform, i).name);
  g_array_free (shader->uniforms, TRUE);

  G_OBJECT_CLASS (gsk_gl_shader_parent_class)->finalize (object);
}

static GRegex *uniform_regexp = NULL; /* Initialized in class_init */


static void
gsk_gl_shader_add_uniform (GskGLShader     *shader,
                           const char      *name,
                           GskGLUniformType type)
{
  GskGLUniform uniform = {
    g_strdup (name),
    type,
    shader->uniforms_size
  };

  shader->uniforms_size += uniform_type_size (type);

  g_array_append_val (shader->uniforms, uniform);
}

static void
gsk_gl_shader_constructed (GObject *object)
{
  GskGLShader *shader = GSK_GL_SHADER (object);
  gsize string_len;
  const char *string = g_bytes_get_data (shader->bytes, &string_len);
  GMatchInfo *match_info;
  int max_texture_seen = 0;

  g_regex_match_full (uniform_regexp,
                      string, string_len, 0, 0,
                      &match_info, NULL);
  while (g_match_info_matches (match_info))
    {
      char *type = g_match_info_fetch (match_info, 1);
      char *name = g_match_info_fetch (match_info, 2);

      if (strcmp (type, "sampler2D") == 0)
        {
          /* Textures are special cased */

          if (g_str_has_prefix (name, "u_texture") &&
              strlen (name) == strlen ("u_texture")+1)
            {
              char digit = name[strlen("u_texture")];
              if (digit >= '1' && digit <= '9')
                max_texture_seen = MAX(max_texture_seen, digit - '0');
            }
          else
            g_debug ("Unhandled shader texture uniform '%s', use uniforms of name 'u_texture[1..9]'", name);
        }
      else
        {
          GskGLUniformType utype = uniform_type_from_glsl (type);
          g_assert (utype != GSK_GLUNIFORM_TYPE_NONE); // Shouldn't have matched the regexp
          gsk_gl_shader_add_uniform (shader, name, utype);
        }

      g_free (type);
      g_free (name);

      g_match_info_next (match_info, NULL);
    }

  g_match_info_free (match_info);

  shader->n_required_textures = max_texture_seen;

  if (GSK_DEBUG_CHECK(SHADERS))
    {
      GString *s;

      s = g_string_new ("");
      for (int i = 0; i < shader->uniforms->len; i++)
        {
          GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
          if (i > 0)
            g_string_append (s, ", ");
          g_string_append_printf (s, "%s %s", uniform_type_name (u->type), u->name);
        }
      g_message ("Shader constructed: %d textures, %d uniforms (%s)",
                 shader->n_required_textures, shader->uniforms->len,
                 s->str);
      g_string_free (s, TRUE);
    }
}

#define SPACE_RE "[ \\t]+" // Don't use \s, we don't want to match newlines
#define OPT_SPACE_RE "[ \\t]*"
#define UNIFORM_TYPE_RE "(int|uint|bool|float|vec2|vec3|vec4|sampler2D)"
#define UNIFORM_NAME_RE "([\\w]+)"
#define OPT_INIT_VALUE_RE "[\\w(),. ]+" // This is a bit simple, but will match most initializers
#define OPT_INITIALIZER_RE  "(" OPT_SPACE_RE "=" OPT_SPACE_RE  OPT_INIT_VALUE_RE ")?"
#define UNIFORM_MATCHER_RE "^uniform" SPACE_RE UNIFORM_TYPE_RE SPACE_RE UNIFORM_NAME_RE OPT_INITIALIZER_RE OPT_SPACE_RE ";" OPT_SPACE_RE "$"

static void
gsk_gl_shader_class_init (GskGLShaderClass *klass)
{
   GObjectClass *object_class = G_OBJECT_CLASS (klass);
   uniform_regexp = g_regex_new (UNIFORM_MATCHER_RE,
                                 G_REGEX_MULTILINE | G_REGEX_RAW | G_REGEX_OPTIMIZE,
                                 0, NULL);

  object_class->get_property = gsk_gl_shader_get_property;
  object_class->set_property = gsk_gl_shader_set_property;
  object_class->finalize = gsk_gl_shader_finalize;
  object_class->constructed = gsk_gl_shader_constructed;

  /**
   * GskGLShader:sourcecode:
   *
   * The source code for the shader, as a #GBytes.
   */
  gsk_gl_shader_properties[GLSHADER_PROP_BYTES] =
    g_param_spec_boxed ("bytes",
                        "Bytes",
                        "The sourcecode code for the shader",
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  /**
   * GskGLShader:resource:
   *
   * resource containing the source code for the shader.
   */
  gsk_gl_shader_properties[GLSHADER_PROP_RESOURCE] =
    g_param_spec_string ("resource",
                         "Resources",
                         "Resource path to the source code",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, GLSHADER_N_PROPS, gsk_gl_shader_properties);
}

static void
gsk_gl_shader_init (GskGLShader *shader)
{
  shader->uniforms = g_array_new (FALSE, FALSE, sizeof (GskGLUniform));
}

/**
 * gsk_gl_shader_new_from_bytes:
 * @sourcecode: The sourcecode for the shader, as a #GBytes
 *
 * Creates a #GskGLShader that will render pixels using the specified code.
 *
 * Returns: (transfer full): A new #GskGLShader
 */
GskGLShader *
gsk_gl_shader_new_from_bytes (GBytes *sourcecode)
{
  g_return_val_if_fail (sourcecode != NULL, NULL);

  return g_object_new (GSK_TYPE_GLSHADER,
                       "bytes", sourcecode,
                       NULL);
}

/**
 * gsk_gl_shader_new_from_resource:
 * @resource_path: valid path to a resource that contains the sourcecode for the shader
 *
 * Creates a #GskGLShader that will render pixels using the specified code.
 *
 * Returns: (transfer full): A new #GskGLShader
 */
GskGLShader *
gsk_gl_shader_new_from_resource (const char      *resource_path)
{
  g_return_val_if_fail (resource_path != NULL, NULL);

  return g_object_new (GSK_TYPE_GLSHADER,
                       "resource", resource_path,
                       NULL);
}

/**
 * gsk_gl_shader_get_sourcecode:
 * @shader: A #GskGLShader
 *
 * Get the source code being used to render this shader.
 *
 * Returns: (transfer none): The source code for the shader
 */
GBytes *
gsk_gl_shader_get_bytes (GskGLShader *shader)
{
  return shader->bytes;
}

/**
 * gsk_gl_shader_get_n_required_textures:
 * @shader: A #GskGLShader
 *
 * Returns the number of textures that the shader requires. This can be used
 * to check that the a passed shader works in your usecase. This
 * is determined by looking at the highest u_textureN value that the
 * shader defines.
 *
 * Returns: The nr of texture input this shader requires.
 */
int
gsk_gl_shader_get_n_required_textures (GskGLShader  *shader)
{
  return shader->n_required_textures;
}

/**
 * gsk_gl_shader_get_n_uniforms:
 * @shader: A #GskGLShader
 *
 * Get the number of declared uniforms for this shader.
 *
 * Returns: The nr of declared uniforms
 */
int
gsk_gl_shader_get_n_uniforms (GskGLShader     *shader)
{
  return shader->uniforms->len;
}

/**
 * gsk_gl_shader_get_uniform_name:
 * @shader: A #GskGLShader
 * @idx: A zero-based index of the uniforms
 *
 * Get the name of a declared uniforms for this shader at index @indx.
 *
 * Returns: (transfer none): The name of the declared uniform
 */
const char *
gsk_gl_shader_get_uniform_name (GskGLShader     *shader,
                                int              idx)
{
  return g_array_index (shader->uniforms, GskGLUniform, idx).name;
}

/**
 * gsk_gl_shader_find_uniform_by_name:
 * @shader: A #GskGLShader
 * @name: A uniform name
 *
 * Looks for a uniform by the name @name, and returns the index
 * of the unifor, or -1 if it was not found.
 *
 * Returns: The index of the uniform, or -1
 */
int
gsk_gl_shader_find_uniform_by_name (GskGLShader      *shader,
                                    const char       *name)
{
  for (int i = 0; i < shader->uniforms->len; i++)
    {
      const GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
      if (strcmp (u->name, name) == 0)
        return i;
    }

  return -1;
}

/**
 * gsk_gl_shader_get_uniform_type:
 * @shader: A #GskGLShader
 * @idx: A zero-based index of the uniforms
 *
 * Get the type of a declared uniforms for this shader at index @indx.
 *
 * Returns: The type of the declared uniform
 */
GskGLUniformType
gsk_gl_shader_get_uniform_type (GskGLShader     *shader,
                                int              idx)
{
  return g_array_index (shader->uniforms, GskGLUniform, idx).type;
}

/**
 * gsk_gl_shader_get_uniform_offset:
 * @shader: A #GskGLShader
 * @idx: A zero-based index of the uniforms
 *
 * Get the offset into the data block where data for this uniforms is stored.
 *
 * Returns: The data offset
 */
int
gsk_gl_shader_get_uniform_offset (GskGLShader     *shader,
                                  int              idx)
{
  return g_array_index (shader->uniforms, GskGLUniform, idx).offset;
}

const GskGLUniform *
gsk_gl_shader_get_uniforms (GskGLShader *shader,
                            int         *n_uniforms)
{
  *n_uniforms = shader->uniforms->len;
  return &g_array_index (shader->uniforms, GskGLUniform, 0);
}

/**
 * gsk_gl_shader_get_uniforms_size:
 * @shader: A #GskGLShader
 *
 * Get the size of the data block used to specify uniform data for this shader.
 *
 * Returns: The size of the data block
 */
int
gsk_gl_shader_get_uniforms_size (GskGLShader *shader)
{
  return shader->uniforms_size;
}

static const GskGLUniform *
gsk_gl_shader_find_uniform (GskGLShader *shader,
                            const char *name)
{
  for (int i = 0; i < shader->uniforms->len; i++)
    {
      const GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
      if (strcmp (u->name, name) == 0)
        return u;
    }

  return NULL;
}

/**
 * gsk_gl_shader_get_uniform_data_float:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of float type.
 *
 * Returns: The value
 */
float
gsk_gl_shader_get_uniform_data_float (GskGLShader *shader,
                                      GBytes       *uniform_data,
                                      int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_FLOAT);

  args_src = data + u->offset;
  return *(float *)args_src;
}

/**
 * gsk_gl_shader_get_uniform_data_int:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of int type.
 *
 * Returns: The value
 */
gint32
gsk_gl_shader_get_uniform_data_int (GskGLShader *shader,
                                    GBytes       *uniform_data,
                                    int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_INT);

  args_src = data + u->offset;
  return *(gint32 *)args_src;
}

/**
 * gsk_gl_shader_get_uniform_data_uint:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of uint type.
 *
 * Returns: The value
 */
guint32
gsk_gl_shader_get_uniform_data_uint (GskGLShader *shader,
                                     GBytes       *uniform_data,
                                     int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_UINT);

  args_src = data + u->offset;
  return *(guint32 *)args_src;
}

/**
 * gsk_gl_shader_get_uniform_data_bool:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of bool type.
 *
 * Returns: The value
 */
gboolean
gsk_gl_shader_get_uniform_data_bool (GskGLShader *shader,
                                     GBytes       *uniform_data,
                                     int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_BOOL);

  args_src = data + u->offset;
  return *(guint32 *)args_src;
}

/**
 * gsk_gl_shader_get_uniform_data_vec2:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 * @out_value: Location to store set the uniform value too
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of vec2 type.
 */
void
gsk_gl_shader_get_uniform_data_vec2 (GskGLShader           *shader,
                                     GBytes                *uniform_data,
                                     int                    idx,
                                     graphene_vec2_t       *out_value)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_VEC2);

  args_src = data + u->offset;
  graphene_vec2_init_from_float (out_value, (float *)args_src);
}

/**
 * gsk_gl_shader_get_uniform_data_vec3:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 * @out_value: Location to store set the uniform value too
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of vec3 type.
 */
void
gsk_gl_shader_get_uniform_data_vec3 (GskGLShader           *shader,
                                     GBytes                *uniform_data,
                                     int                    idx,
                                     graphene_vec3_t       *out_value)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_VEC3);

  args_src = data + u->offset;
  graphene_vec3_init_from_float (out_value, (float *)args_src);
}

/**
 * gsk_gl_shader_get_uniform_data_vec4:
 * @shader: A #GskGLShader
 * @uniform_data: The uniform data block
 * @idx: The index of the uniform
 * @out_value: Location to store set the uniform value too
 *
 * Gets the value of the uniform @idx in the @uniform_data block.
 * The uniform must be of vec4 type.
 */
void
gsk_gl_shader_get_uniform_data_vec4 (GskGLShader           *shader,
                                     GBytes                *uniform_data,
                                     int                    idx,
                                     graphene_vec4_t       *out_value)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (uniform_data, &size);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_VEC4);

  args_src = data + u->offset;
  graphene_vec4_init_from_float (out_value, (float *)args_src);
}

/**
 * gsk_gl_shader_format_uniform_data_va:
 * @shader: A #GskGLShader
 * @uniforms: Name-Value pairs for the uniforms of @shader, ending with a %NULL name, all values are passed by reference.
 *
 * Formats the uniform data as needed for feeding the named uniforms values into the shader.
 * The argument list is a list of pairs of names, and pointers to data of the types
 * that match the declared uniforms (i.e. `float *` for float uniforms and `graphene_vec4_t *` f
 * or vec3 uniforms).
 *
 * Returns: (transfer full): A newly allocated block of data which can be passed to gsk_gl_shader_node_new().
 */
GBytes *
gsk_gl_shader_format_uniform_data_va (GskGLShader *shader,
                                      va_list uniforms)
{
  guchar *args = g_malloc0 (shader->uniforms_size);
  const char *name;

  while ((name = va_arg (uniforms, const char *)) != NULL)
    {
      const GskGLUniform *u;
      gpointer value = va_arg (uniforms, gpointer);
      guchar *args_dest;

      u = gsk_gl_shader_find_uniform (shader, name);
      if (u == NULL)
        {
          /* This isn't really an error, because we can easily imaging
             a shader interface that have input which isn't needed for
             a particular shader */
          g_debug ("No uniform named `%s` in shader", name);
          continue;
        }

      args_dest = args + u->offset;

      /* We use pointers-to-value so that all values are the same
         size, otherwise we couldn't handle the missing uniform case above */

      switch (u->type)
        {
        case GSK_GLUNIFORM_TYPE_FLOAT:
          *(float *)args_dest = *(float *)value;
          break;

        case GSK_GLUNIFORM_TYPE_INT:
          *(gint32 *)args_dest = *(gint32 *)value;
          break;

        case GSK_GLUNIFORM_TYPE_UINT:
          *(guint32 *)args_dest = *(guint32 *)value;
          break;

        case GSK_GLUNIFORM_TYPE_BOOL:
          *(guint32 *)args_dest = *(gboolean *)value;
          break;

        case GSK_GLUNIFORM_TYPE_VEC2:
          graphene_vec2_to_float ((const graphene_vec2_t *)value,
                                  (float *)args_dest);
          break;

        case GSK_GLUNIFORM_TYPE_VEC3:
          graphene_vec3_to_float ((const graphene_vec3_t *)value,
                                  (float *)args_dest);
          break;

        case GSK_GLUNIFORM_TYPE_VEC4:
          graphene_vec4_to_float ((const graphene_vec4_t *)value,
                                  (float *)args_dest);
          break;

        case GSK_GLUNIFORM_TYPE_NONE:
        default:
          g_assert_not_reached ();
        }
    }

  return g_bytes_new_take (args, shader->uniforms_size);
}

struct _GskUniformDataBuilder {
  GskGLShader *shader;
  guchar *data;
};

G_DEFINE_BOXED_TYPE (GskUniformDataBuilder, gsk_uniform_data_builder,
                     gsk_uniform_data_builder_copy,
                     gsk_uniform_data_builder_free);


/**
 * gsk_gl_shader_build_uniform_data:
 * @shader: A #GskGLShader
 *
 * Allocates a builder that can be used to construct a new uniform data
 * chunk.
 *
 * Returns: (transfer full): The newly allocated builder, free with gsk_uniform_data_builder_free()
 */
GskUniformDataBuilder *
gsk_gl_shader_build_uniform_data (GskGLShader *shader)
{
  GskUniformDataBuilder *builder = g_new0 (GskUniformDataBuilder, 1);
  builder->shader = g_object_ref (shader);
  builder->data = g_malloc0 (shader->uniforms_size);

  return builder;
}

/**
 * gsk_uniform_data_builder_finish:
 * @builder: A #GskUniformDataBuilder
 *
 * Finishes building the uniform data and returns it as a GBytes. Once this
 * is called the builder can not be used anymore.
 *
 * Returns: (transfer full): The newly allocated builder, free with gsk_uniform_data_builder_free()
 */
GBytes *
gsk_uniform_data_builder_finish (GskUniformDataBuilder *builder)
{
  return g_bytes_new_take (g_steal_pointer (&builder->data),
                           builder->shader->uniforms_size);
}

/**
 * gsk_uniform_data_builder_free:
 * @builder: A #GskUniformDataBuilder
 *
 * Frees the builder.
 */
void
gsk_uniform_data_builder_free (GskUniformDataBuilder *builder)
{
  g_object_unref (builder->shader);
  g_free (builder->data);
  g_free (builder);
}

/**
 * gsk_uniform_data_builder_copy:
 * @builder: A #GskUniformDataBuilder
 *
 * Makes a copy of the builder.
 *
 * Returns: (transfer full): A copy of the builder, free with gsk_uniform_data_builder_free().
 */
GskUniformDataBuilder *
gsk_uniform_data_builder_copy (GskUniformDataBuilder *builder)
{
  GskUniformDataBuilder *new = g_new0 (GskUniformDataBuilder, 1);
  new->data = g_memdup (builder->data, builder->shader->uniforms_size);
  new->shader = g_object_ref (builder->shader);
  return new;
}

/**
 * gsk_uniform_data_builder_set_float:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of float type.
 */
void
gsk_uniform_data_builder_set_float (GskUniformDataBuilder *builder,
                                    int                    idx,
                                    float                  value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_FLOAT);

  args_dest = builder->data + u->offset;
  *(float *)args_dest = value;
}

/**
 * gsk_uniform_data_builder_set_int:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of int type.
 */
void
gsk_uniform_data_builder_set_int (GskUniformDataBuilder *builder,
                                  int                    idx,
                                  gint32                 value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_INT);

  args_dest = builder->data + u->offset;
  *(gint32 *)args_dest = value;
}

/**
 * gsk_uniform_data_builder_set_uint:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of uint type.
 */
void
gsk_uniform_data_builder_set_uint (GskUniformDataBuilder *builder,
                                   int                    idx,
                                   guint32                value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_UINT);

  args_dest = builder->data + u->offset;
  *(guint32 *)args_dest = value;
}

/**
 * gsk_uniform_data_builder_set_bool:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of bool type.
 */
void
gsk_uniform_data_builder_set_bool (GskUniformDataBuilder *builder,
                                   int                    idx,
                                   gboolean               value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_BOOL);

  args_dest = builder->data + u->offset;
  *(guint32 *)args_dest = !!value;
}

/**
 * gsk_uniform_data_builder_set_vec2:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of vec2 type.
 */
void
gsk_uniform_data_builder_set_vec2 (GskUniformDataBuilder *builder,
                                   int                    idx,
                                   const graphene_vec2_t *value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_VEC2);

  args_dest = builder->data + u->offset;
  graphene_vec2_to_float (value, (float *)args_dest);
}

/**
 * gsk_uniform_data_builder_set_vec3:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of vec3 type.
 */
void
gsk_uniform_data_builder_set_vec3 (GskUniformDataBuilder *builder,
                                   int                    idx,
                                   const graphene_vec3_t *value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_VEC3);

  args_dest = builder->data + u->offset;
  graphene_vec3_to_float (value, (float *)args_dest);
}

/**
 * gsk_uniform_data_builder_set_vec4:
 * @builder: A #GskUniformDataBuilder
 * @idx: The index of the uniform
 * @value: The value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 * The uniform must be of vec4 type.
 */
void
gsk_uniform_data_builder_set_vec4 (GskUniformDataBuilder *builder,
                                   int                    idx,
                                   const graphene_vec4_t *value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GLUNIFORM_TYPE_VEC4);

  args_dest = builder->data + u->offset;
  graphene_vec4_to_float (value, (float *)args_dest);
}
