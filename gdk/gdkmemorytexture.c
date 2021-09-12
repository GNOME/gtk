/*
 * Copyright © 2018 Benjamin Otte
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

#include "gdkmemorytextureprivate.h"
#include "gsk/ngl/fp16private.h"

/**
 * GdkMemoryTexture:
 *
 * A `GdkTexture` representing image data in memory.
 */

struct _GdkMemoryTexture
{
  GdkTexture parent_instance;

  GdkMemoryFormat format;

  GBytes *bytes;
  gsize stride;
};

struct _GdkMemoryTextureClass
{
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkMemoryTexture, gdk_memory_texture, GDK_TYPE_TEXTURE)

gsize
gdk_memory_format_bytes_per_pixel (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
      return 3;

    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
      return 4;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
      return 6;

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      return 8;

    case GDK_MEMORY_R32G32B32_FLOAT:
      return 12;

    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      return 16;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return 4;
    }
}

static gsize
gdk_memory_format_alignment (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
      return G_ALIGNOF (guchar);

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
      return G_ALIGNOF (guint16);

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      return G_ALIGNOF (guint16);

    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      return G_ALIGNOF (float);

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return G_ALIGNOF (double);
    }
}

static void
gdk_memory_texture_dispose (GObject *object)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (object);

  g_clear_pointer (&self->bytes, g_bytes_unref);

  G_OBJECT_CLASS (gdk_memory_texture_parent_class)->dispose (object);
}

static GdkTexture *
gdk_memory_texture_download_texture (GdkTexture *texture)
{
  return g_object_ref (texture);
}

static void
gdk_memory_texture_download (GdkTexture *texture,
                             guchar     *data,
                             gsize       stride)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert (data, stride,
                      GDK_MEMORY_CONVERT_DOWNLOAD,
                      (guchar *) g_bytes_get_data (self->bytes, NULL),
                      self->stride,
                      self->format,
                      gdk_texture_get_width (texture),
                      gdk_texture_get_height (texture));
}

static void
gdk_memory_texture_download_float (GdkTexture *texture,
                                   float      *data,
                                   gsize       stride)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert_to_float (data, stride,
                               (guchar *) g_bytes_get_data (self->bytes, NULL),
                               self->stride,
                               self->format,
                               gdk_texture_get_width (texture),
                               gdk_texture_get_height (texture));
}

static void
gdk_memory_texture_class_init (GdkMemoryTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download_texture = gdk_memory_texture_download_texture;
  texture_class->download = gdk_memory_texture_download;
  texture_class->download_float = gdk_memory_texture_download_float;
  gobject_class->dispose = gdk_memory_texture_dispose;
}

static void
gdk_memory_texture_init (GdkMemoryTexture *self)
{
}

static GBytes *
gdk_memory_sanitize (GBytes          *bytes,
                     int              width,
                     int              height,
                     GdkMemoryFormat  format,
                     gsize            stride,
                     gsize           *out_stride)
{
  gsize align, size, copy_stride, bpp;
  const guchar *data;
  guchar *copy;
  int y;

  data = g_bytes_get_data (bytes, &size);
  align = gdk_memory_format_alignment (format);

  if (GPOINTER_TO_SIZE (data) % align == 0 &&
      stride % align == 0)
    {
      *out_stride = stride;
      return g_bytes_ref (bytes);
    }

  bpp = gdk_memory_format_bytes_per_pixel (format);
  copy_stride = bpp * width;
  /* align to multiples of 4, just to be sure */
  copy_stride = (copy_stride + 3) & ~3;
  copy = g_malloc (copy_stride * height);
  for (y = 0; y < height; y++)
    memcpy (copy + y * copy_stride, data + y * stride, bpp * width);

  *out_stride = copy_stride;
  return g_bytes_new_take (copy, copy_stride * height);
}

/**
 * gdk_memory_texture_new:
 * @width: the width of the texture
 * @height: the height of the texture
 * @format: the format of the data
 * @bytes: the `GBytes` containing the pixel data
 * @stride: rowstride for the data
 *
 * Creates a new texture for a blob of image data.
 *
 * The `GBytes` must contain @stride x @height pixels
 * in the given format.
 *
 * Returns: A newly-created `GdkTexture`
 */
GdkTexture *
gdk_memory_texture_new (int              width,
                        int              height,
                        GdkMemoryFormat  format,
                        GBytes          *bytes,
                        gsize            stride)
{
  GdkMemoryTexture *self;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (stride >= width * gdk_memory_format_bytes_per_pixel (format), NULL);

  bytes = gdk_memory_sanitize (bytes, width, height, format, stride, &stride);

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  self->format = format;
  self->bytes = bytes;
  self->stride = stride;

  return GDK_TEXTURE (self);
}

GdkMemoryFormat 
gdk_memory_texture_get_format (GdkMemoryTexture *self)
{
  return self->format;
}

const guchar *
gdk_memory_texture_get_data (GdkMemoryTexture *self)
{
  return g_bytes_get_data (self->bytes, NULL);
}

gsize
gdk_memory_texture_get_stride (GdkMemoryTexture *self)
{
  return self->stride;
}

static void
convert_memcpy (guchar       *dest_data,
                gsize         dest_stride,
                const guchar *src_data,
                gsize         src_stride,
                gsize         width,
                gsize         height)
{
  gsize y;

  for (y = 0; y < height; y++)
    memcpy (dest_data + y * dest_stride, src_data + y * src_stride, 4 * width);
}

#define SWIZZLE(A,R,G,B) \
static void \
convert_swizzle ## A ## R ## G ## B (guchar       *dest_data, \
                                     gsize         dest_stride, \
                                     const guchar *src_data, \
                                     gsize         src_stride, \
                                     gsize         width, \
                                     gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = src_data[4 * x + 0]; \
          dest_data[4 * x + R] = src_data[4 * x + 1]; \
          dest_data[4 * x + G] = src_data[4 * x + 2]; \
          dest_data[4 * x + B] = src_data[4 * x + 3]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE(3,2,1,0)
SWIZZLE(2,1,0,3)
SWIZZLE(3,0,1,2)
SWIZZLE(1,2,3,0)

#define SWIZZLE_OPAQUE(A,R,G,B) \
static void \
convert_swizzle_opaque_## A ## R ## G ## B (guchar       *dest_data, \
                                            gsize         dest_stride, \
                                            const guchar *src_data, \
                                            gsize         src_stride, \
                                            gsize         width, \
                                            gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = 0xFF; \
          dest_data[4 * x + R] = src_data[3 * x + 0]; \
          dest_data[4 * x + G] = src_data[3 * x + 1]; \
          dest_data[4 * x + B] = src_data[3 * x + 2]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE_OPAQUE(3,2,1,0)
SWIZZLE_OPAQUE(3,0,1,2)
SWIZZLE_OPAQUE(0,1,2,3)
SWIZZLE_OPAQUE(0,3,2,1)

#define PREMULTIPLY(d,c,a) G_STMT_START { guint t = c * a + 0x80; d = ((t >> 8) + t) >> 8; } G_STMT_END
#define SWIZZLE_PREMULTIPLY(A,R,G,B, A2,R2,G2,B2) \
static void \
convert_swizzle_premultiply_ ## A ## R ## G ## B ## _ ## A2 ## R2 ## G2 ## B2 \
                                    (guchar       *dest_data, \
                                     gsize         dest_stride, \
                                     const guchar *src_data, \
                                     gsize         src_stride, \
                                     gsize         width, \
                                     gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = src_data[4 * x + A2]; \
          PREMULTIPLY(dest_data[4 * x + R], src_data[4 * x + R2], src_data[4 * x + A2]); \
          PREMULTIPLY(dest_data[4 * x + G], src_data[4 * x + G2], src_data[4 * x + A2]); \
          PREMULTIPLY(dest_data[4 * x + B], src_data[4 * x + B2], src_data[4 * x + A2]); \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE_PREMULTIPLY (3,2,1,0, 3,2,1,0)
SWIZZLE_PREMULTIPLY (0,1,2,3, 3,2,1,0)
SWIZZLE_PREMULTIPLY (3,2,1,0, 0,1,2,3)
SWIZZLE_PREMULTIPLY (0,1,2,3, 0,1,2,3)
SWIZZLE_PREMULTIPLY (3,2,1,0, 3,0,1,2)
SWIZZLE_PREMULTIPLY (0,1,2,3, 3,0,1,2)
SWIZZLE_PREMULTIPLY (3,2,1,0, 0,3,2,1)
SWIZZLE_PREMULTIPLY (0,1,2,3, 0,3,2,1)
SWIZZLE_PREMULTIPLY (3,0,1,2, 3,2,1,0)
SWIZZLE_PREMULTIPLY (3,0,1,2, 0,1,2,3)
SWIZZLE_PREMULTIPLY (3,0,1,2, 3,0,1,2)
SWIZZLE_PREMULTIPLY (3,0,1,2, 0,3,2,1)

#define CONVERT_FUNC(name,suffix,R,G,B,A,step) \
static void \
convert_ ## name ## _to_ ## suffix (guchar       *dest_data, \
                                    gsize         dest_stride, \
                                    const guchar *src_data, \
                                    gsize         src_stride, \
                                    gsize         width, \
                                    gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          guchar conv[4]; \
          convert_pixel_ ## name (conv, src_data + step * x); \
          dest_data[4 * x + R] = conv[0]; \
          dest_data[4 * x + G] = conv[1]; \
          dest_data[4 * x + B] = conv[2]; \
          dest_data[4 * x + A] = conv[3]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

#define CONVERT_FUNCS(name,step) \
CONVERT_FUNC(name, download_le, 2, 1, 0, 3, step) \
CONVERT_FUNC(name, download_be, 1, 2, 3, 0, step) \
CONVERT_FUNC(name, gles_rgba, 0, 1, 2, 3, step) \

static inline void
convert_pixel_rgb16 (guchar *dest_data, const guchar *src_data)
{
  const guint16 *src = (const guint16 *) src_data;
  dest_data[0] = (guchar)(src[0] >> 8);
  dest_data[1] = (guchar)(src[1] >> 8);
  dest_data[2] = (guchar)(src[2] >> 8);
  dest_data[3] = 0xFF;
}
CONVERT_FUNCS(rgb16, 3 * sizeof (guint16))

static inline void
convert_pixel_rgba16 (guchar *dest_data, const guchar *src_data)
{
  const guint16 *src = (const guint16 *) src_data;
  dest_data[0] = (guchar)(src[0] >> 8);
  dest_data[1] = (guchar)(src[1] >> 8);
  dest_data[2] = (guchar)(src[2] >> 8);
  dest_data[3] = (guchar)(src[3] >> 8);
}
CONVERT_FUNCS(rgba16, 4 * sizeof (guint16))

static inline void
convert_pixel_rgb16f (guchar *dest_data, const guchar *src_data)
{
  float src[4];
  guint16 tmp[4];
  memcpy(tmp, src_data, sizeof(guint16) * 3);
  half_to_float4(tmp, src);
  dest_data[0] = CLAMP (src[0] * 256.f, 0.f, 255.f);
  dest_data[1] = CLAMP (src[1] * 256.f, 0.f, 255.f);
  dest_data[2] = CLAMP (src[2] * 256.f, 0.f, 255.f);
  dest_data[3] = 0xFF;
}
CONVERT_FUNCS(rgb16f, 3 * sizeof (guint16))

static inline void
convert_pixel_rgba16f (guchar *dest_data, const guchar *src_data)
{
  float src[4];
  half_to_float4((const guint16 *) src_data, src);
  dest_data[0] = CLAMP (src[0] * 256.f, 0.f, 255.f);
  dest_data[1] = CLAMP (src[1] * 256.f, 0.f, 255.f);
  dest_data[2] = CLAMP (src[2] * 256.f, 0.f, 255.f);
  dest_data[3] = CLAMP (src[3] * 256.f, 0.f, 255.f);
}
CONVERT_FUNCS(rgba16f, 4 * sizeof (guint16))

static inline void
convert_pixel_rgb32f (guchar *dest_data, const guchar *src_data)
{
  float *src = (float *) src_data;
  dest_data[0] = CLAMP (src[0] * 256.f, 0.f, 255.f);
  dest_data[1] = CLAMP (src[1] * 256.f, 0.f, 255.f);
  dest_data[2] = CLAMP (src[2] * 256.f, 0.f, 255.f);
  dest_data[3] = 0xFF;
}
CONVERT_FUNCS(rgb32f, 3 * sizeof (float))

static inline void
convert_pixel_rgba32f (guchar *dest_data, const guchar *src_data)
{
  float *src = (float *) src_data;
  dest_data[0] = CLAMP (src[0] * 256.f, 0.f, 255.f);
  dest_data[1] = CLAMP (src[1] * 256.f, 0.f, 255.f);
  dest_data[2] = CLAMP (src[2] * 256.f, 0.f, 255.f);
  dest_data[3] = CLAMP (src[3] * 256.f, 0.f, 255.f);
}
CONVERT_FUNCS(rgba32f, 4 * sizeof (float))

typedef void (* ConversionFunc) (guchar       *dest_data,
                                 gsize         dest_stride,
                                 const guchar *src_data,
                                 gsize         src_stride,
                                 gsize         width,
                                 gsize         height);

static ConversionFunc converters[GDK_MEMORY_N_FORMATS][GDK_MEMORY_N_CONVERSIONS] =
{
  { convert_memcpy, convert_swizzle3210, convert_swizzle2103 },
  { convert_swizzle3210, convert_memcpy, convert_swizzle3012 },
  { convert_swizzle2103, convert_swizzle1230, convert_memcpy },
  { convert_swizzle_premultiply_3210_3210, convert_swizzle_premultiply_0123_3210, convert_swizzle_premultiply_3012_3210,  },
  { convert_swizzle_premultiply_3210_0123, convert_swizzle_premultiply_0123_0123, convert_swizzle_premultiply_3012_0123 },
  { convert_swizzle_premultiply_3210_3012, convert_swizzle_premultiply_0123_3012, convert_swizzle_premultiply_3012_3012 },
  { convert_swizzle_premultiply_3210_0321, convert_swizzle_premultiply_0123_0321, convert_swizzle_premultiply_3012_0321 },
  { convert_swizzle_opaque_3210, convert_swizzle_opaque_0123, convert_swizzle_opaque_3012 },
  { convert_swizzle_opaque_3012, convert_swizzle_opaque_0321, convert_swizzle_opaque_3210 },
  { convert_rgb16_to_download_le, convert_rgb16_to_download_be, convert_rgb16_to_gles_rgba },
  { convert_rgba16_to_download_le, convert_rgba16_to_download_be, convert_rgba16_to_gles_rgba },
  { convert_rgb16f_to_download_le, convert_rgb16f_to_download_be, convert_rgb16f_to_gles_rgba },
  { convert_rgba16f_to_download_le, convert_rgba16f_to_download_be, convert_rgba16f_to_gles_rgba },
  { convert_rgb32f_to_download_le, convert_rgb32f_to_download_be, convert_rgb32f_to_gles_rgba },
  { convert_rgba32f_to_download_le, convert_rgba32f_to_download_be, convert_rgba32f_to_gles_rgba }
};

void
gdk_memory_convert (guchar              *dest_data,
                    gsize                dest_stride,
                    GdkMemoryConversion  dest_format,
                    const guchar        *src_data,
                    gsize                src_stride,
                    GdkMemoryFormat      src_format,
                    gsize                width,
                    gsize                height)
{
  g_assert (dest_format < 3);
  g_assert (src_format < GDK_MEMORY_N_FORMATS);

  converters[src_format][dest_format] (dest_data, dest_stride, src_data, src_stride, width, height);
}

#define CONVERT_FLOAT(R,G,B,A,premultiply) G_STMT_START {\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          if (A >= 0) \
            { \
              dest_data[4 * x + 0] = src_data[4 * x + R] / 255.0f; \
              dest_data[4 * x + 1] = src_data[4 * x + G] / 255.0f; \
              dest_data[4 * x + 2] = src_data[4 * x + B] / 255.0f; \
              dest_data[4 * x + 3] = src_data[4 * x + A] / 255.0f; \
              if (premultiply) \
                { \
                  dest_data[4 * x + 0] *= dest_data[4 * x + 3]; \
                  dest_data[4 * x + 1] *= dest_data[4 * x + 3]; \
                  dest_data[4 * x + 2] *= dest_data[4 * x + 3]; \
                } \
            } \
          else \
            { \
              dest_data[4 * x + 0] = src_data[3 * x + R] / 255.0f; \
              dest_data[4 * x + 1] = src_data[3 * x + G] / 255.0f; \
              dest_data[4 * x + 2] = src_data[3 * x + B] / 255.0f; \
              dest_data[4 * x + 3] = 1.0; \
            } \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}G_STMT_END

#define CONVERT_FLOAT_PIXEL(func,step) G_STMT_START{\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          func (dest_data + 4 * x, src_data + step * x); \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}G_STMT_END

static inline void
convert_rgb16_to_float (float *dest, const guchar *src_data)
{
  const guint16 *src = (const guint16 *) src_data;
  dest[0] = src[0] / 65535.f;
  dest[1] = src[1] / 65535.f;
  dest[2] = src[2] / 65535.f;
  dest[3] = 1.0;
}

static inline void
convert_rgba16_to_float (float *dest, const guchar *src_data)
{
  const guint16 *src = (const guint16 *) src_data;
  dest[0] = src[0] / 65535.f;
  dest[1] = src[1] / 65535.f;
  dest[2] = src[2] / 65535.f;
  dest[3] = 1.0;
}

static inline void
convert_rgb16f_to_float (float *dest, const guchar *src_data)
{
  guint16 tmp[4];
  memcpy(tmp, src_data, sizeof(guint16) * 3);
  tmp[3] = FP16_ONE;
  half_to_float4 (tmp, dest);
}

static inline void
convert_rgba16f_to_float (float *dest, const guchar *src_data)
{
  half_to_float4 ((const guint16 *) src_data, dest);
}

static inline void
convert_rgb32f_to_float (float *dest, const guchar *src_data)
{
  const float *src = (const float *) src_data;
  dest[0] = src[0];
  dest[1] = src[1];
  dest[2] = src[2];
  dest[3] = 1.0;
}

static inline void
convert_rgba32f_to_float (float *dest, const guchar *src_data)
{
  const float *src = (const float *) src_data;
  dest[0] = src[0];
  dest[1] = src[1];
  dest[2] = src[2];
  dest[3] = src[3];
}

void
gdk_memory_convert_to_float (float           *dest_data,
                             gsize            dest_stride,
                             const guchar    *src_data,
                             gsize            src_stride,
                             GdkMemoryFormat  src_format,
                             gsize            width,
                             gsize            height)
{
  gsize x, y;

  switch (src_format)
  {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      CONVERT_FLOAT (2, 1, 0, 3, FALSE);
      break;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      CONVERT_FLOAT (1, 2, 3, 0, FALSE);
      break;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      CONVERT_FLOAT (0, 1, 2, 3, FALSE);
      break;
    case GDK_MEMORY_B8G8R8A8:
      CONVERT_FLOAT (2, 1, 0, 3, TRUE);
      break;
    case GDK_MEMORY_A8R8G8B8:
      CONVERT_FLOAT (1, 2, 3, 0, TRUE);
      break;
    case GDK_MEMORY_R8G8B8A8:
      CONVERT_FLOAT (0, 1, 2, 3, TRUE);
      break;
    case GDK_MEMORY_A8B8G8R8:
      CONVERT_FLOAT (3, 2, 1, 0, TRUE);
      break;
    case GDK_MEMORY_R8G8B8:
      CONVERT_FLOAT (0, 1, 2, -1, FALSE);
      break;
    case GDK_MEMORY_B8G8R8:
      CONVERT_FLOAT (2, 1, 0, -1, FALSE);
      break;
    case GDK_MEMORY_R16G16B16:
      CONVERT_FLOAT_PIXEL (convert_rgb16_to_float, 3 * sizeof (guint16));
      break;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      CONVERT_FLOAT_PIXEL (convert_rgba16_to_float, 4 * sizeof (guint16));
      break;
    case GDK_MEMORY_R16G16B16_FLOAT:
      CONVERT_FLOAT_PIXEL (convert_rgb16f_to_float, 3 * sizeof (guint16));
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      CONVERT_FLOAT_PIXEL (convert_rgba16f_to_float, 4 * sizeof (guint16));
      break;
    case GDK_MEMORY_R32G32B32_FLOAT:
      CONVERT_FLOAT_PIXEL (convert_rgb32f_to_float, 3 * sizeof (float));
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      CONVERT_FLOAT_PIXEL (convert_rgba32f_to_float, 4 * sizeof (float));
      break;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached();
  }
}
