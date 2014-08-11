/* GDK - The GIMP Drawing Kit
 *
 * gdkglpixelformat.c: GL pixel formats
 * 
 * Copyright © 2014  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gdkglpixelformat
 * @Title: GdkGLPixelFormat
 * @Short_description: Specify the pixel format for GL contexts
 *
 * The #GdkGLPixelFormat class is used to specify the types and sizes of
 * buffers to be used by a #GdkGLContext, as well as other configuration
 * parameters.
 *
 * Once created, a #GdkGLPixelFormat is immutable, and can only be used
 * to create a #GdkGLContext, or for validation.
 *
 * Once a #GdkGLPixelFormat has been validated, either directly through
 * gdk_display_validate_gl_pixel_format() or indirectly through the
 * creation of a #GdkGLContext, it is possible to query its properties
 * for the values found during validation.
 *
 * ## Using GdkGLPixelFormat ##
 *
 * Typically, you will create a new #GdkGLPixelFormat using the given
 * constructor and properties:
 *
 * |[<!-- language="C" -->
 *   GdkGLPixelFormat *format;
 *
 *   // Ask for a pixel format with double buffering and
 *   // a depth buffer with a size of 32 bits
 *   format = gdk_gl_pixel_format_new ("double-buffer", TRUE,
 *                                     "depth-size", 32,
 *                                     NULL);
 * ]|
 *
 * After creating a pixel format, you can validate it using a #GdkDisplay
 * and use the result of gdk_display_validate_gl_pixel_format() to provide
 * an alternative pixel format:
 *
 * |[<!-- language="C" -->
 *   GError *error = NULL;
 *
 *   // The "display" variable is set elsewhere.
 *   // The "format" variable is the one we set previously.
 *   if (!gdk_display_validate_gl_pixel_format (display, format, &error))
 *     {
 *       // print "error" or create a new pixel format to validate
 *     }
 * ]|
 *
 * You can also create a #GdkGLContext for the given display and
 * pixel format:
 *
 * |[<!-- language="C" -->
 *   GdkGLContext *context;
 *   GError *error = NULL;
 *
 *   context = gdk_display_get_gl_context (display, format, NULL, &error);
 *   if (error != NULL)
 *     {
 *       // print error
 *     }
 * ]|
 *
 * Once a #GdkGLContext has been created with a #GdkGLPixelFormat, the
 * context will acquire a reference on the pixel format, so it's safe to
 * release the reference created at construction.
 */

#include "config.h"

#include <gio/gio.h>

#include "gdkglpixelformatprivate.h"

#include "gdkdisplayprivate.h"
#include "gdkenumtypes.h"

#include "gdkintl.h"

enum {
  PROP_0,

  /* bool */
  PROP_DOUBLE_BUFFER,
  PROP_MULTI_SAMPLE,

  /* uint */
  PROP_AUX_BUFFERS,
  PROP_COLOR_SIZE,
  PROP_ALPHA_SIZE,
  PROP_DEPTH_SIZE,
  PROP_STENCIL_SIZE,
  PROP_ACCUM_SIZE,
  PROP_SAMPLE_BUFFERS,
  PROP_SAMPLES,

  /* enum */
  PROP_PROFILE,

  LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

G_DEFINE_QUARK (gdk-gl-pixel-format-error-quark, gdk_gl_pixel_format_error)

G_DEFINE_TYPE (GdkGLPixelFormat, gdk_gl_pixel_format, G_TYPE_OBJECT)

static void
gdk_gl_pixel_format_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GdkGLPixelFormat *self = GDK_GL_PIXEL_FORMAT (gobject);

  switch (prop_id)
    {
    case PROP_DOUBLE_BUFFER:
      self->double_buffer = g_value_get_boolean (value);
      break;

    case PROP_MULTI_SAMPLE:
      self->multi_sample = g_value_get_boolean (value);
      break;

    case PROP_AUX_BUFFERS:
      self->aux_buffers = g_value_get_int (value);
      break;

    case PROP_COLOR_SIZE:
      self->color_size = g_value_get_int (value);
      break;

    case PROP_ALPHA_SIZE:
      self->alpha_size = g_value_get_int (value);
      break;

    case PROP_DEPTH_SIZE:
      self->depth_size = g_value_get_int (value);
      break;

    case PROP_STENCIL_SIZE:
      self->stencil_size = g_value_get_int (value);
      break;

    case PROP_ACCUM_SIZE:
      self->accum_size = g_value_get_int (value);
      break;

    case PROP_SAMPLE_BUFFERS:
      self->sample_buffers = g_value_get_int (value);
      break;

    case PROP_SAMPLES:
      self->samples = g_value_get_int (value);
      break;

    case PROP_PROFILE:
      self->profile = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_gl_pixel_format_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GdkGLPixelFormat *self = GDK_GL_PIXEL_FORMAT (gobject);

  switch (prop_id)
    {
    case PROP_DOUBLE_BUFFER:
      g_value_set_boolean (value, self->double_buffer);
      break;

    case PROP_MULTI_SAMPLE:
      g_value_set_boolean (value, self->multi_sample);
      break;

    case PROP_AUX_BUFFERS:
      g_value_set_int (value, self->aux_buffers);
      break;

    case PROP_COLOR_SIZE:
      g_value_set_int (value, self->color_size);
      break;

    case PROP_ALPHA_SIZE:
      g_value_set_int (value, self->alpha_size);
      break;

    case PROP_DEPTH_SIZE:
      g_value_set_int (value, self->depth_size);
      break;

    case PROP_STENCIL_SIZE:
      g_value_set_int (value, self->stencil_size);
      break;

    case PROP_ACCUM_SIZE:
      g_value_set_int (value, self->accum_size);
      break;

    case PROP_SAMPLE_BUFFERS:
      g_value_set_int (value, self->sample_buffers);
      break;

    case PROP_SAMPLES:
      g_value_set_int (value, self->samples);
      break;

    case PROP_PROFILE:
      g_value_set_enum (value, self->profile);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_gl_pixel_format_class_init (GdkGLPixelFormatClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdk_gl_pixel_format_set_property;
  gobject_class->get_property = gdk_gl_pixel_format_get_property;

  /**
   * GdkGLPixelFormat:double-buffer:
   *
   * Whether the pixel format should enable double buffering.
   *
   * Since: 3.14
   */
  obj_props[PROP_DOUBLE_BUFFER] =
    g_param_spec_boolean ("double-buffer",
                          P_("Double Buffer"),
                          P_("Whether the pixel format should ask for double buffering"),
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:multi-sample:
   *
   * Whether the pixel format should enable multi-sampling.
   *
   * See also the #GdkGLPixelFormat:sample-buffers and
   * #GdkGLPixelFormat:samples properties.
   *
   * Since: 3.14
   */
  obj_props[PROP_MULTI_SAMPLE] =
    g_param_spec_boolean ("multi-sample",
                          P_("Multi Sample"),
                          P_("Whether the pixel format should enable multi-sampling"),
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:aux-buffers:
   *
   * A positive integer indicating the number of auxiliary buffers
   * for the pixel format.
   *
   * If set to -1, the default will be used.
   *
   * Since: 3.14
   */
  obj_props[PROP_AUX_BUFFERS] =
    g_param_spec_int ("aux-buffers",
                      P_("Auxiliary Buffers"),
                      P_("The number of auxiliary buffers"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:color-size:
   *
   * A positive integer indicating the size of each color buffer.
   *
   * If set to -1, the default will be used.
   *
   * Since: 3.14
   */
  obj_props[PROP_COLOR_SIZE] =
    g_param_spec_int ("color-size",
                      P_("Color Size"),
                      P_("The size of each color buffer"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:alpha-size:
   *
   * A positive integer indicating the size of the alpha buffer.
   *
   * If set to 0, the alpha channel will be ignored.
   *
   * If set the -1, the default will be used.
   *
   * Since: 3.14
   */
  obj_props[PROP_ALPHA_SIZE] =
    g_param_spec_int ("alpha-size",
                      P_("Alpha Size"),
                      P_("The size of the alpha buffer"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:depth-size:
   *
   * A positive integer indicating the size of the depth buffer.
   *
   * If set to -1, the default will be used.
   *
   * Since: 3.14
   */
  obj_props[PROP_DEPTH_SIZE] =
    g_param_spec_int ("depth-size",
                      P_("Depth Size"),
                      P_("The size of the depth buffer"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:stencil-size:
   *
   * A positive integer indicating the size of the stencil buffer.
   *
   * If set to -1, the default will be used.
   *
   * Since: 3.14
   */
  obj_props[PROP_STENCIL_SIZE] =
    g_param_spec_int ("stencil-size",
                      P_("Stencil Size"),
                      P_("The size of the stencil buffer"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:accum-size:
   *
   * A positive integer indicating the size of the accumulation buffer.
   *
   * If set to -1, the default will be used.
   *
   * Since: 3.14
   */
  obj_props[PROP_ACCUM_SIZE] =
    g_param_spec_int ("accum-size",
                      P_("Accumulation Size"),
                      P_("The size of the accumulation buffer"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:sample-buffers:
   *
   * A positive integer indicating the number of multi-sample buffers.
   *
   * If set to -1, the default will be used.
   *
   * This property is only used if #GdkGLPixelFormat:multi-sample is set
   * to %TRUE.
   *
   * Since: 3.14
   */
  obj_props[PROP_SAMPLE_BUFFERS] =
    g_param_spec_int ("sample-buffers",
                      P_("Sample Buffers"),
                      P_("The number of multi-sample buffers"),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:samples:
   *
   * A positive integer indicating the number of samples for each
   * multi-sample buffer.
   *
   * If set to -1, the default will be used.
   *
   * This property is only used if #GdkGLPixelFormat:multi-sample is set
   * to %TRUE.
   *
   * Since: 3.14
   */
  obj_props[PROP_SAMPLES] =
    g_param_spec_int ("samples",
                      P_("Samples"),
                      P_(""),
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLPixelFormat:profile:
   *
   * The GL profile to be used when creating a #GdkGLContext.
   *
   * Since: 3.14
   */
  obj_props[PROP_PROFILE] =
    g_param_spec_enum ("profile",
                       P_("Profile"),
                       P_(""),
                       GDK_TYPE_GL_PIXEL_FORMAT_PROFILE,
                       GDK_GL_PIXEL_FORMAT_PROFILE_DEFAULT,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_props);
}

static void
gdk_gl_pixel_format_init (GdkGLPixelFormat *self)
{
}

/**
 * gdk_gl_pixel_format_new:
 * @first_property: the first property to set
 * @...: the value of the @first_property, followed by a %NULL terminated
 *   set of property, value pairs
 *
 * Creates a new #GdkGLPixelFormat with the given list of properties.
 *
 * Returns: the newly created #GdkGLPixelFormat
 *
 * Since: 3.14
 */
GdkGLPixelFormat *
gdk_gl_pixel_format_new (const char *first_property,
                         ...)
{
  GdkGLPixelFormat *res;
  va_list args;

  va_start (args, first_property);

  res = (GdkGLPixelFormat *) g_object_new_valist (GDK_TYPE_GL_PIXEL_FORMAT, first_property, args);

  va_end (args);

  return res;
}
