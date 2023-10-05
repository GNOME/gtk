/*
 * Copyright © 2023 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gdkdmabuftexturebuilder.h"

#include "gdkdisplay.h"
#include "gdkenumtypes.h"
#include "gdkdmabuftextureprivate.h"

#include <cairo-gobject.h>


struct _GdkDmabufTextureBuilder
{
  GObject parent_instance;

  GdkDisplay *display;
  unsigned int width;
  unsigned int height;
  unsigned int fourcc;
  guint64 modifier;

  /* per-plane properties */
  int fd[4];
  unsigned int stride[4];
  unsigned int offset[4];

  GdkTexture *update_texture;
  cairo_region_t *update_region;
};

struct _GdkDmabufTextureBuilderClass
{
  GObjectClass parent_class;
};

/**
 * GdkDmabufTextureBuilder:
 *
 * `GdkDmabufTextureBuilder` is a buider used to construct [class@Gdk.Texture]
 * objects from dma-buf buffers.
 *
 * The operation is quite simple: Create a texture builder, set all the necessary
 * properties, and then call [method@Gdk.DmabufTextureBuilder.build] to create the
 * new texture.
 *
 * The required properties for a dma-buf texture are
 *  - The width and height in pixels
 *  - The `fourcc` code and `modifier` which together identify the
 *    format and memory layout of the dma-buf. See the `drm_fourcc.h`
 *    header for more information about these
 *  - The file descriptor referring to the buffer
 *
 * `GdkDmabufTextureBuilder` can be used for quick one-shot construction of
 * textures as well as kept around and reused to construct multiple textures.
 *
 * Since: 4.14
 */

enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FOURCC,
  PROP_MODIFIER,
  PROP_FD0,
  PROP_FD1,
  PROP_FD2,
  PROP_FD3,
  PROP_STRIDE0,
  PROP_STRIDE1,
  PROP_STRIDE2,
  PROP_STRIDE3,
  PROP_OFFSET0,
  PROP_OFFSET1,
  PROP_OFFSET2,
  PROP_OFFSET3,
  PROP_UPDATE_REGION,
  PROP_UPDATE_TEXTURE,

  N_PROPS
};

G_DEFINE_TYPE (GdkDmabufTextureBuilder, gdk_dmabuf_texture_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gdk_dmabuf_texture_builder_dispose (GObject *object)
{
  GdkDmabufTextureBuilder *self = GDK_DMABUF_TEXTURE_BUILDER (object);

  g_clear_object (&self->update_texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_dmabuf_texture_builder_parent_class)->dispose (object);
}

static void
gdk_dmabuf_texture_builder_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GdkDmabufTextureBuilder *self = GDK_DMABUF_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;

    case PROP_FOURCC:
      g_value_set_uint (value, self->fourcc);
      break;

    case PROP_MODIFIER:
      g_value_set_uint64 (value, self->modifier);
      break;

    case PROP_FD0:
      g_value_set_int (value, self->fd[0]);
      break;

    case PROP_FD1:
      g_value_set_int (value, self->fd[1]);
      break;

    case PROP_FD2:
      g_value_set_int (value, self->fd[2]);
      break;

    case PROP_FD3:
      g_value_set_int (value, self->fd[3]);
      break;

    case PROP_STRIDE0:
      g_value_set_uint (value, self->stride[0]);
      break;

    case PROP_STRIDE1:
      g_value_set_uint (value, self->stride[1]);
      break;

    case PROP_STRIDE2:
      g_value_set_uint (value, self->stride[2]);
      break;

    case PROP_STRIDE3:
      g_value_set_uint (value, self->stride[3]);
      break;

    case PROP_OFFSET0:
      g_value_set_uint (value, self->offset[0]);
      break;

    case PROP_OFFSET1:
      g_value_set_uint (value, self->offset[1]);
      break;

    case PROP_OFFSET2:
      g_value_set_uint (value, self->offset[2]);
      break;

    case PROP_OFFSET3:
      g_value_set_uint (value, self->offset[3]);
      break;

    case PROP_UPDATE_REGION:
      g_value_set_boxed (value, self->update_region);
      break;

    case PROP_UPDATE_TEXTURE:
      g_value_set_object (value, self->update_texture);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_dmabuf_texture_builder_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GdkDmabufTextureBuilder *self = GDK_DMABUF_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_DISPLAY:
      gdk_dmabuf_texture_builder_set_display (self, g_value_get_object (value));
      break;

    case PROP_WIDTH:
      gdk_dmabuf_texture_builder_set_width (self, g_value_get_uint (value));
      break;

    case PROP_HEIGHT:
      gdk_dmabuf_texture_builder_set_height (self, g_value_get_uint (value));
      break;

    case PROP_FOURCC:
      gdk_dmabuf_texture_builder_set_fourcc (self, g_value_get_uint (value));
      break;

    case PROP_MODIFIER:
      gdk_dmabuf_texture_builder_set_modifier (self, g_value_get_uint64 (value));
      break;

    case PROP_FD0:
      gdk_dmabuf_texture_builder_set_fd (self, 0, g_value_get_int (value));
      break;

    case PROP_FD1:
      gdk_dmabuf_texture_builder_set_fd (self, 1, g_value_get_int (value));
      break;

    case PROP_FD2:
      gdk_dmabuf_texture_builder_set_fd (self, 2, g_value_get_int (value));
      break;

    case PROP_FD3:
      gdk_dmabuf_texture_builder_set_fd (self, 3, g_value_get_int (value));
      break;

    case PROP_STRIDE0:
      gdk_dmabuf_texture_builder_set_stride (self, 0, g_value_get_uint (value));
      break;

    case PROP_STRIDE1:
      gdk_dmabuf_texture_builder_set_stride (self, 1, g_value_get_uint (value));
      break;

    case PROP_STRIDE2:
      gdk_dmabuf_texture_builder_set_stride (self, 2, g_value_get_uint (value));
      break;

    case PROP_STRIDE3:
      gdk_dmabuf_texture_builder_set_stride (self, 3, g_value_get_uint (value));
      break;

    case PROP_OFFSET0:
      gdk_dmabuf_texture_builder_set_offset (self, 0, g_value_get_uint (value));
      break;

    case PROP_OFFSET1:
      gdk_dmabuf_texture_builder_set_offset (self, 1, g_value_get_uint (value));
      break;

    case PROP_OFFSET2:
      gdk_dmabuf_texture_builder_set_offset (self, 2, g_value_get_uint (value));
      break;

    case PROP_OFFSET3:
      gdk_dmabuf_texture_builder_set_offset (self, 3, g_value_get_uint (value));
      break;

    case PROP_UPDATE_REGION:
      gdk_dmabuf_texture_builder_set_update_region (self, g_value_get_boxed (value));
      break;

    case PROP_UPDATE_TEXTURE:
      gdk_dmabuf_texture_builder_set_update_texture (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_dmabuf_texture_builder_class_init (GdkDmabufTextureBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_dmabuf_texture_builder_dispose;
  gobject_class->get_property = gdk_dmabuf_texture_builder_get_property;
  gobject_class->set_property = gdk_dmabuf_texture_builder_set_property;

  /**
   * GdkDmabufTextureBuilder:display: (attributes org.gdk.Property.get=gdk_dmabuf_texture_builder_get_display org.gdk.Property.set=gdk_dmabuf_texture_builder_set_display)
   *
   * The display that this texture will be used on.
   *
   * Since: 4.14
   */
  properties[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:width: (attributes org.gdk.Property.get=gdk_dmabuf_texture_builder_get_width org.gdk.Property.set=gdk_dmabuf_texture_builder_set_width)
   *
   * The width of the texture.
   *
   * Since: 4.14
   */
  properties[PROP_WIDTH] =
    g_param_spec_uint ("width", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:height: (attributes org.gdk.Property.get=gdk_dmabuf_texture_builder_get_height org.gdk.Property.set=gdk_dmabuf_texture_builder_set_height)
   *
   * The height of the texture.
   *
   * Since: 4.14
   */
  properties[PROP_HEIGHT] =
    g_param_spec_uint ("height", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:fourcc: (attributes org.gdk.Property.get=gdk_dmabuf_texture_builder_get_fourcc org.gdk.Property.set=gdk_dmabuf_texture_builder_set_fourcc)
   *
   * The format of the texture, as a fourcc value.
   *
   * Since: 4.14
   */
  properties[PROP_FOURCC] =
    g_param_spec_uint ("fourcc", NULL, NULL,
                       0, G_MAXINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:modifier:
   *
   * The modifier.
   *
   * Since: 4.14
   */
  properties[PROP_MODIFIER] =
    g_param_spec_uint64 ("modifier", NULL, NULL,
                         0, G_MAXUINT, 0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:fd0:
   *
   * The file descriptor for plane 0.
   *
   * Since: 4.14
   */
  properties[PROP_FD0] =
    g_param_spec_int ("fd0", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:fd1:
   *
   * The file descriptor for plane 1.
   *
   * Since: 4.14
   */
  properties[PROP_FD1] =
    g_param_spec_int ("fd1", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:fd2:
   *
   * The file descriptor for plane 2.
   *
   * Since: 4.14
   */
  properties[PROP_FD2] =
    g_param_spec_int ("fd2", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:fd3:
   *
   * The file descriptor for plane 3.
   *
   * Since: 4.14
   */
  properties[PROP_FD3] =
    g_param_spec_int ("fd3", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:stride0:
   *
   * The stride for plane 0.
   *
   * Since: 4.14
   */
  properties[PROP_STRIDE0] =
    g_param_spec_uint ("stride0", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:stride1:
   *
   * The stride for plane 1.
   *
   * Since: 4.14
   */
  properties[PROP_STRIDE1] =
    g_param_spec_uint ("stride1", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:stride2:
   *
   * The stride for plane 2.
   *
   * Since: 4.14
   */
  properties[PROP_STRIDE2] =
    g_param_spec_uint ("stride2", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:stride3:
   *
   * The stride for plane 3.
   *
   * Since: 4.14
   */
  properties[PROP_STRIDE3] =
    g_param_spec_uint ("stride3", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:offset0:
   *
   * The offset for plane 0.
   *
   * Since: 4.14
   */
  properties[PROP_OFFSET0] =
    g_param_spec_uint ("offset0", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:offset1:
   *
   * The offset for plane 1.
   *
   * Since: 4.14
   */
  properties[PROP_OFFSET1] =
    g_param_spec_uint ("offset1", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:offset2:
   *
   * The offset for plane 2.
   *
   * Since: 4.14
   */
  properties[PROP_OFFSET2] =
    g_param_spec_uint ("offset2", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:offset3:
   *
   * The offset for plane 3.
   *
   * Since: 4.14
   */
  properties[PROP_OFFSET3] =
    g_param_spec_uint ("offset3", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:update-region: (attributes org.gdk.Property.get=gdk_dmabuf_texture_builder_get_update_region org.gdk.Property.set=gdk_dmabuf_texture_builder_set_update_region)
   *
   * The update region for [property@Gdk.GLTextureBuilder:update-texture].
   *
   * Since: 4.14
   */
  properties[PROP_UPDATE_REGION] =
    g_param_spec_boxed ("update-region", NULL, NULL,
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:update-texture: (attributes org.gdk.Property.get=gdk_dmabuf_texture_builder_get_update_texture org.gdk.Property.set=gdk_dmabuf_texture_builder_set_update_texture)
   *
   * The texture [property@Gdk.GLTextureBuilder:update-region] is an update for.
   *
   * Since: 4.14
   */
  properties[PROP_UPDATE_TEXTURE] =
    g_param_spec_object ("update-texture", NULL, NULL,
                         GDK_TYPE_TEXTURE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_dmabuf_texture_builder_init (GdkDmabufTextureBuilder *self)
{
  self->fd[0] = self->fd[1] = self->fd[2] = self->fd[3] = -1;
}

/**
 * gdk_dmabuf_texture_builder_new: (constructor):
 *
 * Creates a new texture builder.
 *
 * Returns: the new `GdkTextureBuilder`
 *
 * Since: 4.14
 **/
GdkDmabufTextureBuilder *
gdk_dmabuf_texture_builder_new (void)
{
  return g_object_new (GDK_TYPE_DMABUF_TEXTURE_BUILDER, NULL);
}

/**
 * gdk_dmabuf_texture_builder_get_display:
 * @self: a `GdkDmabufTextureBuilder
 *
 * Returns the display that this texture builder is
 * associated with.
 *
 * Returns: (transfer none) (nullable): the display
 *
 * Since: 4.14
 */
GdkDisplay *
gdk_dmabuf_texture_builder_get_display (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);

  return self->display;
}

/**
 * gdk_dmabuf_texture_builder_set_display:
 * @self: a `GdkDmabufTextureBuilder
 * @display: the display
 *
 * Sets the display that this texture builder is
 * associated with.
 *
 * The display is used to determine the supported
 * dma-buf formats.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_display (GdkDmabufTextureBuilder *self,
                                        GdkDisplay              *display)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (g_set_object (&self->display, display))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISPLAY]);
}

/**
 * gdk_dmabuf_texture_builder_get_width: (attributes org.gdk.Method.get_property=width)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the width previously set via gdk_dmabuf_texture_builder_set_width() or
 * 0 if the width wasn't set.
 *
 * Returns: The width
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_width (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->width;
}

/**
 * gdk_dmabuf_texture_builder_set_width: (attributes org.gdk.Method.set_property=width)
 * @self: a `GdkDmabufTextureBuilder`
 * @width: The texture's width or 0 to unset
 *
 * Sets the width of the texture.
 *
 * The width must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_width (GdkDmabufTextureBuilder *self,
                                      unsigned int             width)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->width == width)
    return;

  self->width = width;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDTH]);
}

/**
 * gdk_dmabuf_texture_builder_get_height: (attributes org.gdk.Method.get_property=height)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the height previously set via gdk_dmabuf_texture_builder_set_height() or
 * 0 if the height wasn't set.
 *
 * Returns: The height
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_height (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->height;
}

/**
 * gdk_dmabuf_texture_builder_set_height: (attributes org.gdk.Method.set_property=height)
 * @self: a `GdkDmabufTextureBuilder`
 * @height: the texture's height or 0 to unset
 *
 * Sets the height of the texture.
 *
 * The height must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_height (GdkDmabufTextureBuilder *self,
                                       unsigned int             height)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->height == height)
    return;

  self->height = height;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEIGHT]);
}

/**
 * gdk_dmabuf_texture_builder_get_fourcc: (attributes org.gdk.Method.get_property=fourcc)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the format previously set via gdk_dmabuf_texture_builder_set_fourcc()
 * or 0 if the format wasn't set.
 *
 * The format is specified as a fourcc code.
 *
 * Returns: The format
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_fourcc (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->fourcc;
}

/**
 * gdk_dmabuf_texture_builder_set_fourcc: (attributes org.gdk.Method.set_property=fourcc)
 * @self: a `GdkDmabufTextureBuilder`
 * @fourcc: the texture's format or 0 to unset
 *
 * Sets the format of the texture.
 *
 * The format is specified as a fourcc code.
 *
 * The format must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_fourcc (GdkDmabufTextureBuilder *self,
                                       unsigned int             fourcc)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->fourcc == fourcc)
    return;

  self->fourcc = fourcc;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOURCC]);
}

/**
 * gdk_dmabuf_texture_builder_get_modifier:
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the modifier value.
 *
 * Returns: the modifier
 *
 * Since: 4.14
 */
guint64
gdk_dmabuf_texture_builder_get_modifier (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->modifier;
}

/**
 * gdk_dmabuf_texture_builder_set_modifier:
 * @self: a `GdkDmabufTextureBuilder`
 * @modifier: the modifier value
 *
 * Sets the modifier.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_modifier (GdkDmabufTextureBuilder *self,
                                         guint64                  modifier)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->modifier == modifier)
    return;

  self->modifier = modifier;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIER]);
}

/**
 * gdk_dmabuf_texture_builder_get_fd:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to get the fd for
 *
 * Gets the file descriptor for a plane.
 *
 * Returns: the file descriptor
 *
 * Since: 4.14
 */
int
gdk_dmabuf_texture_builder_get_fd (GdkDmabufTextureBuilder *self,
                                   int                      plane)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);
  g_return_val_if_fail (0 <= plane && plane < 4, 0);

  return self->fd[plane];
}

/**
 * gdk_dmabuf_texture_builder_set_fd:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to set the fd for
 * @fd: the file descriptor
 *
 * Sets the file descriptor for a plane.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_fd (GdkDmabufTextureBuilder *self,
                                   int                      plane,
                                   int                      fd)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (0 <= plane && plane < 4);

  if (self->fd[plane] == fd)
    return;

  self->fd[plane] = fd;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FD0 + plane]);
}

/**
 * gdk_dmabuf_texture_builder_get_stride:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to get the stride for
 *
 * Gets the stride value for a plane.
 *
 * Returns: the stride
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_stride (GdkDmabufTextureBuilder *self,
                                       int                      plane)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);
  g_return_val_if_fail (0 <= plane && plane < 4, 0);

  return self->stride[plane];
}

/**
 * gdk_dmabuf_texture_builder_set_stride:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to set the stride for
 * @stride: the stride value
 *
 * Sets the stride for a plane.
 *
 * The stride must be set for all planes before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_stride (GdkDmabufTextureBuilder *self,
                                       int                      plane,
                                       unsigned int             stride)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (0 <= plane && plane < 4);

  if (self->stride[plane] == stride)
    return;

  self->stride[plane] = stride;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STRIDE0 + plane]);
}

/**
 * gdk_dmabuf_texture_builder_get_offset:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to get the offset for
 *
 * Gets the offset value for a plane.
 *
 * Returns: the offset
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_offset (GdkDmabufTextureBuilder *self,
                                       int                      plane)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);
  g_return_val_if_fail (0 <= plane && plane < 4, 0);

  return self->offset[plane];
}

/**
 * gdk_dmabuf_texture_builder_set_offset:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to set the offset for
 * @offset: the offset value
 *
 * Sets the offset for a plane.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_offset (GdkDmabufTextureBuilder *self,
                                       int                      plane,
                                       unsigned int             offset)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (0 <= plane && plane < 4);

  if (self->offset[plane] == offset)
    return;

  self->offset[plane] = offset;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OFFSET0 + plane]);
}

/**
 * gdk_dmabuf_texture_builder_get_update_texture: (attributes org.gdk.Method.get_property=update_texture)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the texture previously set via gdk_dmabuf_texture_builder_set_update_texture() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The texture
 *
 * Since: 4.14
 */
GdkTexture *
gdk_dmabuf_texture_builder_get_update_texture (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);

  return self->update_texture;
}

/**
 * gdk_dmabuf_texture_builder_set_update_texture: (attributes org.gdk.Method.set_property=update_texture)
 * @self: a `GdkDmabufTextureBuilder`
 * @texture: (nullable): the texture to update
 *
 * Sets the texture to be updated by this texture. See
 * [method@Gdk.DmabufTextureBuilder.set_update_region] for an explanation.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_update_texture (GdkDmabufTextureBuilder *self,
                                               GdkTexture          *texture)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (texture == NULL || GDK_IS_TEXTURE (texture));

  if (!g_set_object (&self->update_texture, texture))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_TEXTURE]);
}

/**
 * gdk_dmabuf_texture_builder_get_update_region: (attributes org.gdk.Method.get_property=update_region)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the region previously set via gdk_dmabuf_texture_builder_set_update_region() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The region
 *
 * Since: 4.14
 */
cairo_region_t *
gdk_dmabuf_texture_builder_get_update_region (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);

  return self->update_region;
}

/**
 * gdk_dmabuf_texture_builder_set_update_region: (attributes org.gdk.Method.set_property=update_region)
 * @self: a `GdkDmabufTextureBuilder`
 * @region: (nullable): the region to update
 *
 * Sets the region to be updated by this texture. Together with
 * [property@Gdk.DmabufTextureBuilder:update-texture] this describes an
 * update of a previous texture.
 *
 * When rendering animations of large textures, it is possible that
 * consecutive textures are only updating contents in parts of the texture.
 * It is then possible to describe this update via these two properties,
 * so that GTK can avoid rerendering parts that did not change.
 *
 * An example would be a screen recording where only the mouse pointer moves.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_update_region (GdkDmabufTextureBuilder *self,
                                              cairo_region_t      *region)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->update_region == region)
    return;

  g_clear_pointer (&self->update_region, cairo_region_destroy);

  if (region)
    self->update_region = cairo_region_reference (region);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_REGION]);
}

/**
 * gdk_dmabuf_texture_builder_build:
 * @self: a `GdkDmabufTextureBuilder`
 * @destroy: (nullable): destroy function to be called when the texture is
 *   released
 * @data: user data to pass to the destroy function
 *
 * Builds a new `GdkTexture` with the values set up in the builder.
 *
 * The `destroy` function gets called when the returned texture gets released.
 *
 * Note that it is a programming error to call this function if any mandatory
 * property has not been set.
 *
 * It is possible to call this function multiple times to create multiple textures,
 * possibly with changing properties in between.
 *
 * Not all formats defined in the `drm_fourcc.h` header are supported.
 *
 * Returns: (transfer full) (nullable): a newly built `GdkTexture` or `NULL`
 *   if the format is not supported
 *
 * Since: 4.14
 */
GdkTexture *
gdk_dmabuf_texture_builder_build (GdkDmabufTextureBuilder *self,
                                  GDestroyNotify       destroy,
                                  gpointer             data)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);
  g_return_val_if_fail (destroy == NULL || data != NULL, NULL);
  g_return_val_if_fail (self->width > 0, NULL);
  g_return_val_if_fail (self->height > 0, NULL);
  g_return_val_if_fail (self->fourcc != 0, NULL);
  g_return_val_if_fail (self->fd[0] != -1, NULL);

#ifdef __linux__
  return gdk_dmabuf_texture_new_from_builder (self, destroy, data);
#else
  return NULL;
#endif
}
