#pragma once

#include <gdk/gdk.h>

#include "gdk/gdkmemorylayoutprivate.h"

typedef enum {
  CHANNEL_UINT_8,
  CHANNEL_UINT_16,
  CHANNEL_FLOAT_16,
  CHANNEL_FLOAT_32,
} ChannelType;

typedef struct _TextureBuilder TextureBuilder;
struct _TextureBuilder
{
  guchar *pixels;
  GdkMemoryLayout layout;
};

ChannelType     gdk_memory_format_get_channel_type              (GdkMemoryFormat                 format);
guint           gdk_memory_format_n_colors                      (GdkMemoryFormat                 format);

void            gdk_memory_layout_fudge                         (GdkMemoryLayout                *layout,
                                                                 gsize                           align);

void            gdk_memory_pixel_print                          (const guchar                   *data,
                                                                 const GdkMemoryLayout          *layout,
                                                                 gsize                           x,
                                                                 gsize                           y,
                                                                 GString                        *string);
gboolean        gdk_memory_pixel_equal                          (const guchar                   *data1,
                                                                 const GdkMemoryLayout          *layout1,
                                                                 const guchar                   *data2,
                                                                 const GdkMemoryLayout          *layout2,
                                                                 gsize                           x,
                                                                 gsize                           y,
                                                                 gboolean                        accurate);

void            texture_builder_init                            (TextureBuilder                 *builder,
                                                                 GdkMemoryFormat                 format,
                                                                 int                             width,
                                                                 int                             height);
GdkTexture *    texture_builder_finish                          (TextureBuilder                 *builder);
void            texture_builder_fill                            (TextureBuilder                 *builder,
                                                                 const GdkRGBA                  *color);
void            texture_builder_draw_color                      (TextureBuilder                 *builder,
                                                                 const cairo_rectangle_int_t    *area,
                                                                 const GdkRGBA                  *color);
void            texture_builder_draw_data                       (TextureBuilder                 *builder,
                                                                 gsize                           x,
                                                                 gsize                           y,
                                                                 const guchar                   *data,
                                                                 const GdkMemoryLayout          *layout);

void            compare_textures                                (GdkTexture                     *texture1,
                                                                 GdkTexture                     *texture2,
                                                                 gboolean                        accurate_compare);
