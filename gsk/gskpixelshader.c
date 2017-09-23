/* GSK - The GTK Scene Kit
 *
 * Copyright 2017 © Benjamin Otte
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
 * SECTION:GskPixelShader
 * @Title: GskPixelShader
 * @Short_description: A pixel shader
 *
 * #GskPixelShader is the object used to create pixel shaders. The language
 * used is GLSL with a few extensions.
 *
 * #GskPixelShader is an immutable object: That means you cannot change
 * anything about it other than increasing the reference count via
 * g_object_ref().
 */

#include "config.h"

#include "gskpixelshaderprivate.h"

#include "gskdebugprivate.h"
#include "gskslprogram.h"

#include "gdk/gdkinternals.h"

/**
 * GskPixelShader:
 *
 * The `GskPixelShader` structure contains only private data.
 *
 * Since: 3.90
 */

enum {
  PROP_0,
  PROP_N_TEXTURES,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (GskPixelShader, gsk_pixel_shader, G_TYPE_OBJECT)

static void
gsk_pixel_shader_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
 {
  /* GskPixelShader *self = GSK_PIXEL_SHADER (gobject); */

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gsk_pixel_shader_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GskPixelShader *self = GSK_PIXEL_SHADER (gobject);

  switch (prop_id)
    {
    case PROP_N_TEXTURES:
      g_value_set_uint (value, self->n_textures);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gsk_pixel_shader_dispose (GObject *object)
{    
  GskPixelShader *self = GSK_PIXEL_SHADER (object);

  g_object_unref (self->program);

  G_OBJECT_CLASS (gsk_pixel_shader_parent_class)->dispose (object);
}

static void
gsk_pixel_shader_class_init (GskPixelShaderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gsk_pixel_shader_set_property;
  gobject_class->get_property = gsk_pixel_shader_get_property;
  gobject_class->dispose = gsk_pixel_shader_dispose;

  /**
   * GskPixelShader:n-textures:
   *
   * The number of input textures to the shader.
   *
   * Since: 3.92
   */
  properties[PROP_N_TEXTURES] =
    g_param_spec_uint ("n-textures",
                       "n textures",
                       "The number of input textures",
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gsk_pixel_shader_init (GskPixelShader *self)
{
}

GskPixelShader *
gsk_pixel_shader_new_for_data (GBytes             *source,
                               GskShaderErrorFunc  error_func,
                               gpointer            error_data)
{
  GskPixelShader *shader;
  GskSlProgram *program;

  g_return_val_if_fail (source != NULL, NULL);

  program = gsk_sl_program_new (source, NULL);
  if (program == NULL)
    return NULL;

  shader = g_object_new (GSK_TYPE_PIXEL_SHADER, NULL);

  shader->program = program;

  return shader;
}

void
gsk_pixel_shader_print (GskPixelShader *shader,
                        GString        *string)
{
  g_return_if_fail (GSK_IS_PIXEL_SHADER (shader));
  g_return_if_fail (string != NULL);

  gsk_sl_program_print (shader->program, string);
}

char *
gsk_pixel_shader_to_string (GskPixelShader *shader)
{
  GString *string;

  g_return_val_if_fail (GSK_IS_PIXEL_SHADER (shader), NULL);

  string = g_string_new (NULL);

  gsk_pixel_shader_print (shader, string);

  return g_string_free (string, FALSE);
}

