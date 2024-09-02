#pragma once

#include <gdk/gdk.h>

/* This shadows a function of the same name in a GDK private header,
 * because tests can't use hidden functions from the shared library */
#define gdk_memory_format_bytes_per_pixel test_memory_format_bytes_per_pixel

typedef enum {
  CHANNEL_UINT_8,
  CHANNEL_UINT_16,
  CHANNEL_FLOAT_16,
  CHANNEL_FLOAT_32,
} ChannelType;

typedef struct _TextureBuilder TextureBuilder;
struct _TextureBuilder
{
  GdkMemoryFormat format;
  int width;
  int height;

  guchar *pixels;
  gsize stride;
  gsize offset;
};

gsize       gdk_memory_format_bytes_per_pixel  (GdkMemoryFormat  format);
ChannelType gdk_memory_format_get_channel_type (GdkMemoryFormat  format);
guint       gdk_memory_format_n_colors         (GdkMemoryFormat  format);
gboolean    gdk_memory_format_has_alpha        (GdkMemoryFormat  format);
gboolean    gdk_memory_format_is_premultiplied (GdkMemoryFormat  format);
gboolean    gdk_memory_format_is_deep          (GdkMemoryFormat  format);
void        gdk_memory_format_pixel_print      (GdkMemoryFormat  format,
                                                const guchar    *data,
                                                GString         *string);
gboolean    gdk_memory_format_pixel_equal      (GdkMemoryFormat  format,
                                                gboolean         accurate,
                                                const guchar    *pixel1,
                                                const guchar    *pixel2);

void        texture_builder_init      (TextureBuilder  *builder,
                                       GdkMemoryFormat  format,
                                       int              width,
                                       int              height);
GdkTexture *texture_builder_finish    (TextureBuilder  *builder);
void        texture_builder_fill      (TextureBuilder  *builder,
                                       const GdkRGBA   *color);
void        texture_builder_set_pixel (TextureBuilder  *builder,
                                       int              x,
                                       int              y,
                                       const GdkRGBA   *color);

void        compare_textures (GdkTexture *texture1,
                              GdkTexture *texture2,
                              gboolean    accurate_compare);
