#pragma once

#include "gdkenums.h"
#include "gdktypes.h"

#include "gdkmemoryformatprivate.h"

/* epoxy needs this, see https://github.com/anholt/libepoxy/issues/299 */
#ifdef GDK_WINDOWING_WIN32
#include <windows.h>
#endif

#include <epoxy/gl.h>

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

G_BEGIN_DECLS

typedef enum {
  GDK_DATA_NV12,
  GDK_DATA_NV21,
  GDK_DATA_NV16,
  GDK_DATA_NV61,
  GDK_DATA_NV24,
  GDK_DATA_NV42,
  GDK_DATA_P010,
  GDK_DATA_P012,
  GDK_DATA_P016,
  GDK_DATA_YUV410,
  GDK_DATA_YVU410,
  GDK_DATA_YUV411,
  GDK_DATA_YVU411,
  GDK_DATA_YUV420,
  GDK_DATA_YVU420,
  GDK_DATA_YUV422,
  GDK_DATA_YVU422,
  GDK_DATA_YUV444,
  GDK_DATA_YVU444,
  GDK_DATA_YUYV,
  GDK_DATA_YVYU,
  GDK_DATA_UYVY,
  GDK_DATA_VYUY,

  GDK_DATA_N_FORMATS
} GdkDataFormat;

#define GDK_DATA_FORMAT_MAX_PLANES 4

typedef struct _GdkDataBuffer GdkDataBuffer;

struct _GdkDataBuffer
{
  GdkDataFormat format;
  gsize width;
  gsize height;
  struct {
    const guchar *data;
    gsize stride;
  } planes[GDK_DATA_FORMAT_MAX_PLANES];
};

const char *            gdk_data_format_get_name                        (GdkDataFormat               format) G_GNUC_CONST;
GdkMemoryFormat         gdk_data_format_get_conversion                  (GdkDataFormat               format) G_GNUC_CONST;
gsize                   gdk_data_format_n_planes                        (GdkDataFormat               format) G_GNUC_CONST;
gsize                   gdk_data_format_alignment                       (GdkDataFormat               format) G_GNUC_CONST;

gsize                   gdk_data_format_block_width                     (GdkDataFormat               format) G_GNUC_CONST;
gsize                   gdk_data_format_block_height                    (GdkDataFormat               format) G_GNUC_CONST;

guint32                 gdk_data_format_get_dmabuf_fourcc               (GdkDataFormat               format) G_GNUC_CONST;

#ifdef GDK_RENDERING_VULKAN
VkFormat                gdk_data_format_vk_format                       (GdkDataFormat               format,
                                                                         VkComponentMapping         *out_swizzle);
#endif

void                    gdk_data_convert                                (const GdkDataBuffer        *self,
                                                                         guchar                     *dest_data,
                                                                         gsize                       dest_stride);

G_END_DECLS

