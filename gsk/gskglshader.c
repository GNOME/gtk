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
 * @Short_description: A description of GskGLShader
 *
 * A #GskGLShader is a snippet of GLSL that is meant to run in the
 * fragment shader of the rendering pipeline. A fragment shaader it
 * gets the coordinates being rendered as input and produces a the
 * pixel values for that particular pixel. Additionally the
 * shader can declare a set of other input arguments, called
 * uniforms (as they are uniform over all the calls to your shader in
 * each instance of use). A shader can also receieve up to 4
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
 * drivers and hardware Gtk adds some defines that you can use
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
 * height of that rectangle. This is in the typical Gtk+ coordinate
 * system with the origin in the top left. @uv contains the u and v
 * coordinates that can be used to index a texture at the
 * corresponding point. These coordinates are in the [0..1]x[0..1]
 * region, with 0, 0 being in the lower left cornder (which is typical
 * for OpenGL).
 *
 * The output @fragColor should be a RGBA color (and alpha) that will
 * be used as the output for the specified pixel location. Note that
 * this output will be automatically clipped to the clip region of the
 * glshader node.
 *
 * In addition to the function arguments the shader can use one of the
 * pre-defined uniforms for tetxure sources (u_source, u_source2,
 * u_source3 or u_source4), as well as any custom uniforms you
 * declare.
 *
 * Gtk uses the the "gsk" namespace in the symbols it uses in the
 * shader, so your code should not use any symbols with the prefix gsk
 * or GSK. There are some helper functions declared that you can use:
 *
 * |[<!-- language="plain" -->
 * vec4 GskTexture(sampler2D sampler, vec2 texCoords);
 * ]|
 *
 * This samples a texture (e.g. u_source) at the specified
 * coordinates, and containes some helper ifdefs to ensure that
 * it works on all OpenGL versions.
 */

#include "config.h"
#include "gskglshader.h"
#include "gskglshaderprivate.h"

struct _GskGLShader
{
  GObject parent_instance;
  char *sourcecode;
  int n_required_sources;
  int uniforms_size;
  GArray *uniforms;
};

G_DEFINE_TYPE (GskGLShader, gsk_glshader, G_TYPE_OBJECT)

enum {
  GLSHADER_PROP_0,
  GLSHADER_PROP_SOURCECODE,
  GLSHADER_PROP_N_REQUIRED_SOURCES,
  GLSHADER_N_PROPS
};
static GParamSpec *gsk_glshader_properties[GLSHADER_N_PROPS];

static void
gsk_glshader_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GskGLShader *shader = GSK_GLSHADER (object);

  switch (prop_id)
    {
    case GLSHADER_PROP_SOURCECODE:
      g_value_set_string (value, shader->sourcecode);
      break;

    case GLSHADER_PROP_N_REQUIRED_SOURCES:
      g_value_set_int (value, shader->n_required_sources);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gsk_glshader_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GskGLShader *shader = GSK_GLSHADER (object);

  switch (prop_id)
    {
    case GLSHADER_PROP_SOURCECODE:
      g_free (shader->sourcecode);
      shader->sourcecode = g_value_dup_string (value);
      break;

    case GLSHADER_PROP_N_REQUIRED_SOURCES:
      gsk_glshader_set_n_required_sources (shader, g_value_get_int (value));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gsk_glshader_finalize (GObject *object)
{
  GskGLShader *shader = GSK_GLSHADER (object);

  g_free (shader->sourcecode);
  for (int i = 0; i < shader->uniforms->len; i ++)
    g_free (g_array_index (shader->uniforms, GskGLUniform, i).name);
  g_array_free (shader->uniforms, TRUE);

  G_OBJECT_CLASS (gsk_glshader_parent_class)->finalize (object);
}

static void
gsk_glshader_class_init (GskGLShaderClass *klass)
{
   GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gsk_glshader_get_property;
  object_class->set_property = gsk_glshader_set_property;
  object_class->finalize = gsk_glshader_finalize;

  /**
   * GskGLShader:sourcecode:
   *
   * The source code for the shader.
   */
  gsk_glshader_properties[GLSHADER_PROP_SOURCECODE] =
    g_param_spec_string ("sourcecode",
                         "Sourcecode",
                         "The sourcecode code for the shader",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GskGLShader:n-required-sources:
   *
   * The number of texture sources (u_source*) that are required to be set for the
   * shader to work.
   */
  gsk_glshader_properties[GLSHADER_PROP_N_REQUIRED_SOURCES] =
    g_param_spec_int ("n-required-sources",
                      "Number of required sources",
                      "Minimum number of source textures required for the shader to work",
                      0, 4, 0,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, GLSHADER_N_PROPS, gsk_glshader_properties);
}

static void
gsk_glshader_init (GskGLShader *shader)
{
  shader->uniforms = g_array_new (FALSE, FALSE, sizeof (GskGLUniform));
}

/**
 * gsk_glshader_new:
 * @sourcecode: The sourcecode for the shader
 *
 * Creates a #GskGLShader that will render pixels using the specified code.
 *
 * Returns: (transfer full): A new #GskGLShader
 */
GskGLShader *
gsk_glshader_new (const char *sourcecode)
{
   GskGLShader *shader = g_object_new (GSK_TYPE_GLSHADER,
                                       "sourcecode", sourcecode,
                                       NULL);
   return shader;
}

/**
 * gsk_glshader_get_sourcecode:
 * @shader: A #GskGLShader
 *
 * Get the source code being used to render this shader.
 *
 * Returns: (transfer none): The source code for the shader
 */
const char *
gsk_glshader_get_sourcecode (GskGLShader *shader)
{
  return shader->sourcecode;
}

/**
 * gsk_glshader_get_n_required_sources:
 * @shader: A #GskGLShader
 *
 * Returns the number of texture sources that the shader requires. This can be set
 * with gsk_glshader_set_n_required_sources() and can be used to check that the
 * a passed shader works in your usecase.
 *
 * Returns: (transfer none): The set nr of texture sources this shader requires.
 */
int
gsk_glshader_get_n_required_sources (GskGLShader  *shader)
{
  return shader->n_required_sources;
}

/**
 * gsk_glshader_set_n_required_sources:
 * @shader: A #GskGLShader
 * @n_required_sources: The number of sources.
 *
 * Sets the number of texture sources that the shader requires. This can be used
 * to signal to other users that this shader needs this much input. 
 */
void
gsk_glshader_set_n_required_sources (GskGLShader  *shader,
                                     int           n_required_sources)
{
  shader->n_required_sources = n_required_sources;
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

/**
 * gsk_glshader_add_uniform:
 * @shader: A #GskGLShader
 * @name: Then name of the uniform.
 * @type: The uniform type
 *
 * This declares that the shader uses a uniform of a particular @name
 * and @type. You need to declare each uniform that your shader
 * uses or you will not be able to pass data to it.
 */
void
gsk_glshader_add_uniform (GskGLShader     *shader,
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

/**
 * gsk_glshader_get_n_uniforms:
 * @shader: A #GskGLShader
 *
 * Get the number of declared uniforms for this shader.
 *
 * Returns: The nr of declared uniforms
 */
int
gsk_glshader_get_n_uniforms (GskGLShader     *shader)
{
  return shader->uniforms->len;
}

/**
 * gsk_glshader_get_uniform_name:
 * @shader: A #GskGLShader
 * @idx: A zero-based index of the uniforms
 *
 * Get the name of a declared uniforms for this shader at index @indx.
 *
 * Returns: (transfer none): The name of the declared uniform
 */
const char *
gsk_glshader_get_uniform_name (GskGLShader     *shader,
                                int              idx)
{
  return g_array_index (shader->uniforms, GskGLUniform, idx).name;
}

/**
 * gsk_glshader_get_uniform_type:
 * @shader: A #GskGLShader
 * @idx: A zero-based index of the uniforms
 *
 * Get the type of a declared uniforms for this shader at index @indx.
 *
 * Returns: The type of the declared uniform
 */
GskGLUniformType
gsk_glshader_get_uniform_type (GskGLShader     *shader,
                                  int              idx)
{
  return g_array_index (shader->uniforms, GskGLUniform, idx).type;
}

/**
 * gsk_glshader_get_uniform_offset:
 * @shader: A #GskGLShader
 * @idx: A zero-based index of the uniforms
 *
 * Get the offset into the data block where data for this uniforms is stored.
 *
 * Returns: The data offset
 */
int
gsk_glshader_get_uniform_offset (GskGLShader     *shader,
                                 int              idx)
{
  return g_array_index (shader->uniforms, GskGLUniform, idx).offset;
}

const GskGLUniform *
gsk_glshader_get_uniforms (GskGLShader *shader,
                           int         *n_uniforms)
{
  *n_uniforms = shader->uniforms->len;
  return &g_array_index (shader->uniforms, GskGLUniform, 0);
}

/**
 * gsk_glshader_get_uniforms_size:
 * @shader: A #GskGLShader
 *
 * Get the size of the data block used to specify uniform data for this shader.
 *
 * Returns: The size of the data block
 */
int
gsk_glshader_get_uniforms_size (GskGLShader *shader)
{
  return shader->uniforms_size;
}

static const GskGLUniform *
gsk_glshader_find_uniform (GskGLShader *shader,
                           const char *name)
{
  int i;
  for (i = 0; i < shader->uniforms->len; i++)
    {
      const GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
      if (strcmp (u->name, name) == 0)
        return u;
    }

  return NULL;
}

/**
 * gsk_glshader_format_uniform_data_va:
 * @shader: A #GskGLShader
 * @uniforms: %NULL terminated va_list with (name, pointer-to-data) arguments pairs
 *
 * Formats the uniform data as needed for feeding the named uniforms values into the shader.
 * The argument list is a list of pairs of names, and pointers to data of the types
 * that match the declared uniforms (i.e. `float *` for float uniforms and `graphene_vec4_t *` f
 * or vec3 uniforms).
 *
 * Returns: (transfer full): A newly allocated block of data which can be passed to gsk_glshader_node_new().
 */
guchar *
gsk_glshader_format_uniform_data_va (GskGLShader *shader,
                                     va_list uniforms)
{
  guchar *args = g_malloc0 (shader->uniforms_size);
  const char *name;

  while ((name = va_arg (uniforms, const char *)) != NULL)
    {
      const GskGLUniform *u;
      gpointer value = va_arg (uniforms, gpointer);
      guchar *args_dest;

      u = gsk_glshader_find_uniform (shader, name);
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

  return args;
}
