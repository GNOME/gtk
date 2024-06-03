/*
 * Copyright © 2024 Benjamin Otte
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

#include "gdkmemorytexturebuilder.h"

#include "gdkcolorstate.h"
#include "gdkenumtypes.h"
#include "gdkmemorytextureprivate.h"

#include <cairo-gobject.h>

struct _GdkMemoryTextureBuilder
{
  GObject parent_instance;

  GBytes *bytes;
  gsize stride;
  int width;
  int height;
  GdkMemoryFormat format;
  GdkColorState *color_state;

  GdkTexture *update_texture;
  cairo_region_t *update_region;
};

struct _GdkMemoryTextureBuilderClass
{
  GObjectClass parent_class;
};

/**
 * GdkMemoryTextureBuilder:
 *
 * `GdkMemoryTextureBuilder` is a builder used to construct [class@Gdk.Texture] objects
 * from system memory provided via [struct@GLib.Bytes].
 *
 * The operation is quite simple: Create a texture builder, set all the necessary
 * properties - keep in mind that the properties [property@Gdk.MemoryTextureBuilder:bytes],
 * [property@Gdk.MemoryTextureBuilder:stride], [property@Gdk.MemoryTextureBuilder:width],
 * and [property@Gdk.MemoryTextureBuilder:height] are mandatory - and then call
 * [method@Gdk.MemoryTextureBuilder.build] to create the new texture.
 *
 * `GdkMemoryTextureBuilder` can be used for quick one-shot construction of
 * textures as well as kept around and reused to construct multiple textures.
 *
 * Since: 4.16
 */

enum
{
  PROP_0,
  PROP_BYTES,
  PROP_COLOR_STATE,
  PROP_FORMAT,
  PROP_HEIGHT,
  PROP_STRIDE,
  PROP_UPDATE_REGION,
  PROP_UPDATE_TEXTURE,
  PROP_WIDTH,

  N_PROPS
};

G_DEFINE_TYPE (GdkMemoryTextureBuilder, gdk_memory_texture_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gdk_memory_texture_builder_dispose (GObject *object)
{
  GdkMemoryTextureBuilder *self = GDK_MEMORY_TEXTURE_BUILDER (object);

  g_clear_pointer (&self->bytes, g_bytes_unref);
  g_clear_pointer (&self->color_state, gdk_color_state_unref);

  g_clear_object (&self->update_texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_memory_texture_builder_parent_class)->dispose (object);
}

static void
gdk_memory_texture_builder_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GdkMemoryTextureBuilder *self = GDK_MEMORY_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_BYTES:
      g_value_set_boxed (value, self->bytes);
      break;

    case PROP_COLOR_STATE:
      g_value_set_boxed (value, self->color_state);
      break;

    case PROP_FORMAT:
      g_value_set_enum (value, self->format);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    case PROP_STRIDE:
      g_value_set_uint64 (value, self->stride);
      break;

    case PROP_UPDATE_REGION:
      g_value_set_boxed (value, self->update_region);
      break;

    case PROP_UPDATE_TEXTURE:
      g_value_set_object (value, self->update_texture);
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
gdk_memory_texture_builder_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GdkMemoryTextureBuilder *self = GDK_MEMORY_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_BYTES:
      gdk_memory_texture_builder_set_bytes (self, g_value_get_boxed (value));
      break;

    case PROP_COLOR_STATE:
      gdk_memory_texture_builder_set_color_state (self, g_value_get_boxed (value));
      break;

    case PROP_FORMAT:
      gdk_memory_texture_builder_set_format (self, g_value_get_enum (value));
      break;

    case PROP_HEIGHT:
      gdk_memory_texture_builder_set_height (self, g_value_get_int (value));
      break;

    case PROP_STRIDE:
      gdk_memory_texture_builder_set_stride (self, g_value_get_uint64 (value));
      break;

    case PROP_UPDATE_REGION:
      gdk_memory_texture_builder_set_update_region (self, g_value_get_boxed (value));
      break;

    case PROP_UPDATE_TEXTURE:
      gdk_memory_texture_builder_set_update_texture (self, g_value_get_object (value));
      break;

    case PROP_WIDTH:
      gdk_memory_texture_builder_set_width (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_memory_texture_builder_class_init (GdkMemoryTextureBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_memory_texture_builder_dispose;
  gobject_class->get_property = gdk_memory_texture_builder_get_property;
  gobject_class->set_property = gdk_memory_texture_builder_set_property;

  /**
   * GdkMemoryTextureBuilder:bytes:
   *
   * The bytes holding the data.
   *
   * Since: 4.16
   */
  properties[PROP_BYTES] =
    g_param_spec_boxed ("bytes", NULL, NULL,
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:color-state:
   *
   * The colorstate describing the data.
   *
   * Since: 4.16
   */
  properties[PROP_COLOR_STATE] =
    g_param_spec_boxed ("color-state", NULL, NULL,
                        GDK_TYPE_COLOR_STATE,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:format:
   *
   * The format of the data.
   *
   * Since: 4.16
   */
  properties[PROP_FORMAT] =
    g_param_spec_enum ("format", NULL, NULL,
                       GDK_TYPE_MEMORY_FORMAT,
                       GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:height:
   *
   * The height of the texture.
   *
   * Since: 4.16
   */
  properties[PROP_HEIGHT] =
    g_param_spec_int ("height", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:stride:
   *
   * The rowstride of the texture.
   *
   * The rowstride is the number of bytes between the first pixel
   * in a row of image data, and the first pixel in the next row.
   *
   * Since: 4.16
   */
  properties[PROP_STRIDE] =
    g_param_spec_uint64 ("stride", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:update-region:
   *
   * The update region for [property@Gdk.MemoryTextureBuilder:update-texture].
   *
   * Since: 4.16
   */
  properties[PROP_UPDATE_REGION] =
    g_param_spec_boxed ("update-region", NULL, NULL,
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:update-texture:
   *
   * The texture [property@Gdk.MemoryTextureBuilder:update-region] is an update for.
   *
   * Since: 4.16
   */
  properties[PROP_UPDATE_TEXTURE] =
    g_param_spec_object ("update-texture", NULL, NULL,
                         GDK_TYPE_TEXTURE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkMemoryTextureBuilder:width:
   *
   * The width of the texture.
   *
   * Since: 4.16
   */
  properties[PROP_WIDTH] =
    g_param_spec_int ("width", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_memory_texture_builder_init (GdkMemoryTextureBuilder *self)
{
  self->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
  self->color_state = gdk_color_state_ref (gdk_color_state_get_srgb ());
}

/**
 * gdk_memory_texture_builder_new: (constructor):
 *
 * Creates a new texture builder.
 *
 * Returns: the new `GdkTextureBuilder`
 *
 * Since: 4.16
 **/
GdkMemoryTextureBuilder *
gdk_memory_texture_builder_new (void)
{
  return g_object_new (GDK_TYPE_MEMORY_TEXTURE_BUILDER, NULL);
}

/**
 * gdk_memory_texture_builder_get_bytes:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the bytes previously set via gdk_memory_texture_builder_set_bytes()
 * or %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The bytes
 *
 * Since: 4.16
 */
GBytes *
gdk_memory_texture_builder_get_bytes (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), NULL);

  return self->bytes;
}

/**
 * gdk_memory_texture_builder_set_bytes:
 * @self: a `GdkMemoryTextureBuilder`
 * @bytes: (nullable): The bytes the texture shows or %NULL to unset
 *
 * Sets the data to be shown but the texture.
 *
 * The bytes must be set before calling [method@Gdk.MemoryTextureBuilder.build].
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_bytes (GdkMemoryTextureBuilder *self,
                                      GBytes                  *bytes)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));
  g_return_if_fail (bytes != NULL);

  if (self->bytes == bytes)
    return;

  g_clear_pointer (&self->bytes, g_bytes_unref);
  self->bytes = bytes;
  if (bytes)
    g_bytes_ref (bytes);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BYTES]);
}

/**
 * gdk_memory_texture_builder_get_color_state:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the colorstate previously set via gdk_memory_texture_builder_set_color_state().
 *
 * Returns: (transfer none): The colorstate
 *
 * Since: 4.16
 */
GdkColorState *
gdk_memory_texture_builder_get_color_state (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), NULL);

  return self->color_state;
}

/**
 * gdk_memory_texture_builder_set_color_state:
 * @self: a `GdkMemoryTextureBuilder`
 * @color_state: (nullable): The colorstate describing the data
 *
 * Sets the colorstate describing the data.
 *
 * By default, the sRGB colorstate is used. If you don't know
 * what colorstates are, this is probably the right thing.
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_color_state (GdkMemoryTextureBuilder *self,
                                            GdkColorState           *color_state)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));
  g_return_if_fail (color_state != NULL);

  if (self->color_state == color_state)
    return;

  g_clear_pointer (&self->color_state, gdk_color_state_unref);
  self->color_state = color_state;
  if (color_state)
    gdk_color_state_ref (color_state);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR_STATE]);
}

/**
 * gdk_memory_texture_builder_get_height:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the height previously set via gdk_memory_texture_builder_set_height()
 * or 0 if the height wasn't set.
 *
 * Returns: The height
 *
 * Since: 4.16
 */
int
gdk_memory_texture_builder_get_height (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), 0);

  return self->height;
}

/**
 * gdk_memory_texture_builder_set_height:
 * @self: a `GdkMemoryTextureBuilder`
 * @height: The texture's height or 0 to unset
 *
 * Sets the height of the texture.
 *
 * The height must be set before calling [method@Gdk.MemoryTextureBuilder.build].
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_height (GdkMemoryTextureBuilder *self,
                                       int                      height)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));

  if (self->height == height)
    return;

  self->height = height;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEIGHT]);
}

/**
 * gdk_memory_texture_builder_get_width:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the width previously set via gdk_memory_texture_builder_set_width()
 * or 0 if the width wasn't set.
 *
 * Returns: The width
 *
 * Since: 4.16
 */
int
gdk_memory_texture_builder_get_width (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), 0);

  return self->width;
}

/**
 * gdk_memory_texture_builder_set_width:
 * @self: a `GdkMemoryTextureBuilder`
 * @width: The texture's width or 0 to unset
 *
 * Sets the width of the texture.
 *
 * The width must be set before calling [method@Gdk.MemoryTextureBuilder.build].
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_width (GdkMemoryTextureBuilder *self,
                                      int                      width)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));

  if (self->width == width)
    return;

  self->width = width;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDTH]);
}

/**
 * gdk_memory_texture_builder_get_stride:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the stride previously set via gdk_memory_texture_builder_set_stride().
 *
 * Returns: the stride
 *
 * Since: 4.16
 */
gsize
gdk_memory_texture_builder_get_stride (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), 0);

  return self->stride;
}

/**
 * gdk_memory_texture_builder_set_stride:
 * @self: a `GdkMemoryTextureBuilder`
 * @stride: the stride or 0 to unset
 *
 * Sets the rowstride of the bytes used.
 *
 * The rowstride must be set before calling [method@Gdk.MemoryTextureBuilder.build].
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_stride (GdkMemoryTextureBuilder *self,
                                       gsize                    stride)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));

  if (self->stride == stride)
    return;

  self->stride = stride;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STRIDE]);
}

/**
 * gdk_memory_texture_builder_get_format:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the format previously set via gdk_memory_texture_builder_set_format().
 *
 * Returns: The format
 *
 * Since: 4.16
 */
GdkMemoryFormat
gdk_memory_texture_builder_get_format (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);

  return self->format;
}

/**
 * gdk_memory_texture_builder_set_format:
 * @self: a `GdkMemoryTextureBuilder`
 * @format: The texture's format
 *
 * Sets the format of the bytes.
 *
 * The default is `GDK_MEMORY_R8G8B8A8_PREMULTIPLIED`.
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_format (GdkMemoryTextureBuilder *self,
                                       GdkMemoryFormat          format)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));

  if (self->format == format)
    return;

  self->format = format;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FORMAT]);
}

/**
 * gdk_memory_texture_builder_get_update_texture:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the texture previously set via gdk_memory_texture_builder_set_update_texture()
 * or %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The update texture
 *
 * Since: 4.16
 */
GdkTexture *
gdk_memory_texture_builder_get_update_texture (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), NULL);

  return self->update_texture;
}

/**
 * gdk_memory_texture_builder_set_update_texture:
 * @self: a `GdkMemoryTextureBuilder`
 * @texture: (nullable): the texture to update
 *
 * Sets the texture to be updated by this texture.
 *
 * See [method@Gdk.MemoryTextureBuilder.set_update_region] for an explanation.
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_update_texture (GdkMemoryTextureBuilder *self,
                                               GdkTexture              *texture)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));
  g_return_if_fail (texture == NULL || GDK_IS_TEXTURE (texture));

  if (!g_set_object (&self->update_texture, texture))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_TEXTURE]);
}

/**
 * gdk_memory_texture_builder_get_update_region:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Gets the region previously set via gdk_memory_texture_builder_set_update_region()
 * or %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The update region
 *
 * Since: 4.16
 */
cairo_region_t *
gdk_memory_texture_builder_get_update_region (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), NULL);

  return self->update_region;
}

/**
 * gdk_memory_texture_builder_set_update_region:
 * @self: a `GdkMemoryTextureBuilder`
 * @region: (nullable): the region to update
 *
 * Sets the region to be updated by this texture.
 *
 * Together with [property@Gdk.MemoryTextureBuilder:update-texture],
 * this describes an update of a previous texture.
 *
 * When rendering animations of large textures, it is possible that
 * consecutive textures are only updating contents in parts of the texture.
 * It is then possible to describe this update via these two properties,
 * so that GTK can avoid rerendering parts that did not change.
 *
 * An example would be a screen recording where only the mouse pointer moves.
 *
 * Since: 4.16
 */
void
gdk_memory_texture_builder_set_update_region (GdkMemoryTextureBuilder *self,
                                              cairo_region_t          *region)
{
  g_return_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self));

  if (self->update_region == region)
    return;

  g_clear_pointer (&self->update_region, cairo_region_destroy);

  if (region)
    self->update_region = cairo_region_reference (region);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_REGION]);
}

/**
 * gdk_memory_texture_builder_build:
 * @self: a `GdkMemoryTextureBuilder`
 *
 * Builds a new `GdkTexture` with the values set up in the builder.
 *
 * Note that it is a programming error to call this function if any mandatory
 * property has not been set.
 *
 * It is possible to call this function multiple times to create multiple textures,
 * possibly with changing properties in between.
 *
 * Returns: (transfer full): a newly built `GdkTexture`
 *
 * Since: 4.16
 */
GdkTexture *
gdk_memory_texture_builder_build (GdkMemoryTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE_BUILDER (self), NULL);
  g_return_val_if_fail (self->width > 0, NULL);
  g_return_val_if_fail (self->height > 0, NULL);
  g_return_val_if_fail (self->bytes != NULL, NULL);
  g_return_val_if_fail (self->stride >= self->width * gdk_memory_format_bytes_per_pixel (self->format), NULL);
  /* needs to be this complex to support subtexture of the bottom right part */
  g_return_val_if_fail (g_bytes_get_size (self->bytes) >= gdk_memory_format_min_buffer_size (self->format, self->stride, self->width, self->height), NULL);

  return gdk_memory_texture_new_from_builder (self);
}
