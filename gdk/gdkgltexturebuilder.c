/*
 * Copyright © 2023 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkgltexturebuilder.h"

#include "gdkglcontext.h"
#include "gdkgltexture.h"

struct _GdkGLTextureBuilder
{
  GObject parent_instance;

  GdkGLContext *context;
  guint id;
  int width;
  int height;
  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkGLTextureBuilderClass
{
  GObjectClass parent_class;
};

/**
 * GdkGLTextureBuilder:
 *
 * `GdkGLTextureBuilder` is a buider used to construct [class@Gdk.Texture] objects from
 * GL textures.
 *
 * The operation is quite simple: Create a texture builder, set all the necessary
 * properties - keep in mind that the properties [property@Gdk.GLTextureBuilder:context],
 * [property@Gdk.GLTextureBuilder:id], [property@Gdk.GLTextureBuilder:width], and 
 * [property@Gdk.GLTextureBuilder:height] are mandatory - and then call
 * [method@Gdk.GLTextureBuilder.build] to create the new texture.
 *
 * `GdkGLTextureBuilder` can be used for quick one-shot construction of
 * textures as well as kept around and reused to construct multiple textures.
 *
 * Since: 4.12
 */

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_HEIGHT,
  PROP_ID,
  PROP_WIDTH,

  N_PROPS
};

G_DEFINE_TYPE (GdkGLTextureBuilder, gdk_gl_texture_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gdk_gl_texture_builder_dispose (GObject *object)
{
  GdkGLTextureBuilder *self = GDK_GL_TEXTURE_BUILDER (object);

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gdk_gl_texture_builder_parent_class)->dispose (object);
}

static void
gdk_gl_texture_builder_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GdkGLTextureBuilder *self = GDK_GL_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    case PROP_ID:
      g_value_set_uint (value, self->id);
      break;

    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_gl_texture_builder_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GdkGLTextureBuilder *self = GDK_GL_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      gdk_gl_texture_builder_set_context (self, g_value_get_object (value));
      break;

    case PROP_HEIGHT:
      gdk_gl_texture_builder_set_height (self, g_value_get_int (value));
      break;

    case PROP_ID:
      gdk_gl_texture_builder_set_id (self, g_value_get_uint (value));
      break;

    case PROP_WIDTH:
      gdk_gl_texture_builder_set_width (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_gl_texture_builder_class_init (GdkGLTextureBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_gl_texture_builder_dispose;
  gobject_class->get_property = gdk_gl_texture_builder_get_property;
  gobject_class->set_property = gdk_gl_texture_builder_set_property;

  /**
   * GdkGLTextureBuilder:context: (attributes org.gdk.Property.get=gdk_gl_texture_builder_get_context org.gdk.Property.set=gdk_gl_texture_builder_set_context)
   *
   * The context owning the texture.
   *
   * Since: 4.12
   */
  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:height: (attributes org.gdk.Property.get=gdk_gl_texture_builder_get_height org.gdk.Property.set=gdk_gl_texture_builder_set_height)
   *
   * The height of the texture.
   *
   * Since: 4.12
   */
  properties[PROP_HEIGHT] =
    g_param_spec_int ("height", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:id: (attributes org.gdk.Property.get=gdk_gl_texture_builder_get_id org.gdk.Property.set=gdk_gl_texture_builder_set_id)
   *
   * The texture ID to use.
   *
   * Since: 4.12
   */
  properties[PROP_ID] =
    g_param_spec_uint ("id", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:width: (attributes org.gdk.Property.get=gdk_gl_texture_builder_get_width org.gdk.Property.set=gdk_gl_texture_builder_set_width)
   *
   * The width of the texture.
   *
   * Since: 4.12
   */
  properties[PROP_WIDTH] =
    g_param_spec_int ("width", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_gl_texture_builder_init (GdkGLTextureBuilder *self)
{
}

/**
 * gdk_gl_texture_builder_new: (constructor):
 *
 * Creates a new texture builder.
 *
 * Returns: the new `GdkTextureBuilder`
 *
 * Since: 4.12
 **/
GdkGLTextureBuilder *
gdk_gl_texture_builder_new (void)
{
  return g_object_new (GDK_TYPE_GL_TEXTURE_BUILDER, NULL);
}

/**
 * gdk_gl_texture_builder_get_context: (attributes org.gdk.Method.get_property=context)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the context previously set via gdk_gl_texture_builder_set_context() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The context
 *
 * Since: 4.12
 */
GdkGLContext *
gdk_gl_texture_builder_get_context (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);

  return self->context;
}

/**
 * gdk_gl_texture_builder_set_context: (attributes org.gdk.Method.set_property=context)
 * @self: a `GdkGLTextureBuilder`
 * @context: (nullable): The context the texture beongs to or %NULL to unset
 *
 * Sets the context to be used for the texture. This is the context that owns
 * the texture.
 *
 * The context must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_context (GdkGLTextureBuilder *self,
                                    GdkGLContext        *context)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));
  g_return_if_fail (context == NULL || GDK_IS_GL_CONTEXT (context));

  if (!g_set_object (&self->context, context))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
}

/**
 * gdk_gl_texture_builder_get_height: (attributes org.gdk.Method.get_property=height)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the height previously set via gdk_gl_texture_builder_set_height() or
 * 0 if the height wasn't set.
 *
 * Returns: The height
 *
 * Since: 4.12
 */
int
gdk_gl_texture_builder_get_height (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), 0);

  return self->height;
}

/**
 * gdk_gl_texture_builder_set_height: (attributes org.gdk.Method.set_property=height)
 * @self: a `GdkGLTextureBuilder`
 * @height: The texture's height or 0 to unset
 *
 * Sets the height of the texture.
 *
 * The height must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_height (GdkGLTextureBuilder *self,
                                   int                  height)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->height == height)
    return;

  self->height = height;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEIGHT]);
}

/**
 * gdk_gl_texture_builder_get_id: (attributes org.gdk.Method.get_property=id)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the texture id previously set via gdk_gl_texture_builder_set_id() or
 * 0 if the id wasn't set.
 *
 * Returns: The id
 *
 * Since: 4.12
 */
guint
gdk_gl_texture_builder_get_id (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), 0);

  return self->id;
}

/**
 * gdk_gl_texture_builder_set_id: (attributes org.gdk.Method.set_property=id)
 * @self: a `GdkGLTextureBuilder`
 * @id: The texture id to be used for creating the texture
 *
 * Sets the texture id of the texture. The texture id must remain unmodified
 * until the texture was finalized. See [method@Gdk.GLTextureBuilder.set_notify]
 * for a longer discussion.
 *
 * The id must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_id (GdkGLTextureBuilder *self,
                               guint                id)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->id == id)
    return;

  self->id = id;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

/**
 * gdk_gl_texture_builder_get_width: (attributes org.gdk.Method.get_property=width)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the width previously set via gdk_gl_texture_builder_set_width() or
 * 0 if the width wasn't set.
 *
 * Returns: The width
 *
 * Since: 4.12
 */
int
gdk_gl_texture_builder_get_width (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), 0);

  return self->width;
}

/**
 * gdk_gl_texture_builder_set_width: (attributes org.gdk.Method.set_property=width)
 * @self: a `GdkGLTextureBuilder`
 * @width: The texture's width or 0 to unset
 *
 * Sets the width of the texture.
 *
 * The width must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_width (GdkGLTextureBuilder *self,
                                  int                  width)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->width == width)
    return;

  self->width = width;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDTH]);
}

/**
 * gdk_gl_texture_builder_set_notify:
 * @self: a `GdkGLTextureBuilder`
 * @destroy: (nullable): destroy function to be called when a texture is
 *   released
 * @data: user data to pass to the destroy function
 *
 * Sets the funciton to be called when the texture built with
 * [method@Gdk.GLTextureBuilder.build] gets released, either when the
 * texture is finalized or by an explicit call to [method@Gdk.Texture.release].
 *
 * This function should release all GL resources associated with the texture,
 * such as the [property@Gdk.GLTextureBuilder:id].
 **/
void
gdk_gl_texture_builder_set_notify (GdkGLTextureBuilder *self,
                                   GDestroyNotify       destroy,
                                   gpointer             data)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));
  g_return_if_fail (destroy == NULL || data != NULL);

  self->destroy = destroy;
  self->data = data;
}

/**
 * gdk_gl_texture_builder_build:
 * @self: a `GdkGLTextureBuilder`
 *
 * Builds a new `GdkTexture` with the values set up in the builder.
 *
 * Note that it is a programming error if any mandatory property has not been set.
 *
 * It is possible to call this function multiple times to create multiple textures,
 * possibly with changing properties in between.
 *
 * Returns: (transfer full): a newly built `GdkTexture`
 *
 * Since: 4.12
 */
GdkTexture *
gdk_gl_texture_builder_build (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);
  g_return_val_if_fail (self->context != NULL, NULL);
  g_return_val_if_fail (self->id != 0, NULL);
  g_return_val_if_fail (self->width > 0, NULL);
  g_return_val_if_fail (self->height > 0, NULL);

  return gdk_gl_texture_new (self->context,
                             self->id,
                             self->width,
                             self->height,
                             self->destroy,
                             self->data);
}

