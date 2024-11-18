#include "config.h"

#include "gdkdataformatprivate.h"

#include "gdk/gdkdmabuffourccprivate.h"

typedef struct _GdkDataFormatDescription GdkDataFormatDescription;

struct _GdkDataFormatDescription
{
  const char *name;
  GdkMemoryFormat conversion;
  gboolean is_yuv;
  gsize n_planes;
  gsize alignment;
  gsize block_size[2];
  guint32 dmabuf_fourcc;
#ifdef GDK_RENDERING_VULKAN
  struct {
    VkFormat format;
    VkComponentMapping swizzle;
  } vk;
#endif
  void                  (* read_line)                       (const GdkDataBuffer        *self,
                                                             gsize                       y,
                                                             guchar                     *dst_data);
  void                  (* convert)                         (const GdkDataBuffer        *self,
                                                             guchar                     *dst_data,
                                                             gsize                       dst_stride);
};

typedef struct _YUVCoefficients YUVCoefficients;

struct _YUVCoefficients
{
  int v_to_r;
  int u_to_g;
  int v_to_g;
  int u_to_b;
};

/* multiplied by 65536 */
static const YUVCoefficients itu601_narrow = { 104597, -25675, -53279, 132201 };
//static const YUVCoefficients itu601_wide = { 74711, -25864, -38050, 133176 };

static inline void
get_uv_values (const YUVCoefficients *coeffs,
               guint8                 u,
               guint8                 v,
               int                   *out_r,
               int                   *out_g,
               int                   *out_b)
{
  int u2 = (int) u - 127;
  int v2 = (int) v - 127;
  *out_r = coeffs->v_to_r * v2;
  *out_g = coeffs->u_to_g * u2 + coeffs->v_to_g * v2;
  *out_b = coeffs->u_to_b * u2;
}

static inline void
set_rgb_values (guint8 rgb[3],
                guint8 y,
                int    r,
                int    g,
                int    b)
{
  int y2 = y * 65536;

  rgb[0] = CLAMP ((y2 + r) >> 16, 0, 255);
  rgb[1] = CLAMP ((y2 + g) >> 16, 0, 255);
  rgb[2] = CLAMP ((y2 + b) >> 16, 0, 255);
}

static inline void
gdk_data_read_3_plane (guchar       *dst_data,
                       gsize         width,
                       const guchar *y_data,
                       const guchar *u_data,
                       const guchar *v_data,
                       gsize         x_subsample)
{
  gsize x;

  for (x = 0; x < width; x++)
    {
      dst_data[3 * x + 0] = y_data[x];
      dst_data[3 * x + 1] = u_data[x / x_subsample];
      dst_data[3 * x + 2] = v_data[x / x_subsample];
    }
}

#define READ_3_PLANE_FUNC(name, uv_swapped, x_subsample, y_subsample) \
static void \
gdk_data_read_ ## name (const GdkDataBuffer *self, \
                        gsize                y, \
                        guchar              *dst_data) \
{\
  gsize u = uv_swapped ? 2 : 1; \
  gsize v = uv_swapped ? 1 : 2; \
  gdk_data_read_3_plane (dst_data, \
                         self->width, \
                         self->planes[0].data + (y * self->planes[0].stride), \
                         self->planes[u].data + (y / y_subsample * self->planes[u].stride), \
                         self->planes[v].data + (y / y_subsample * self->planes[v].stride), \
                         x_subsample); \
}


READ_3_PLANE_FUNC (yuv410, FALSE, 4, 4)
READ_3_PLANE_FUNC (yvu410, TRUE, 4, 4)
READ_3_PLANE_FUNC (yuv411, FALSE, 4, 1)
READ_3_PLANE_FUNC (yvu411, TRUE, 4, 1)
READ_3_PLANE_FUNC (yuv420, FALSE, 2, 2)
READ_3_PLANE_FUNC (yvu420, TRUE, 2, 2)
READ_3_PLANE_FUNC (yuv422, FALSE, 2, 1)
READ_3_PLANE_FUNC (yvu422, TRUE, 2, 1)
READ_3_PLANE_FUNC (yuv444, FALSE, 1, 1)
READ_3_PLANE_FUNC (yvu444, TRUE, 1, 1)

static inline void
gdk_data_read_yuyv_like (guchar       *dst_data,
                         gsize         width,
                         const guchar *src_data,
                         gsize         y_index,
                         gsize         u_index,
                         gsize         v_index)
{
  gsize x;

  for (x = 0; x < width / 2; x++)
    {
      dst_data[6 * x + 0] = src_data[4 * x + y_index];
      dst_data[6 * x + 1] = src_data[4 * x + u_index];
      dst_data[6 * x + 2] = src_data[4 * x + v_index];
      dst_data[6 * x + 3] = src_data[4 * x + y_index + 2];
      dst_data[6 * x + 4] = src_data[4 * x + u_index];
      dst_data[6 * x + 5] = src_data[4 * x + v_index];
    }
}

#define READ_YUYV_FUNC(name, y_index, u_index, v_index) \
static void \
gdk_data_read_ ## name (const GdkDataBuffer *self, \
                        gsize                y, \
                        guchar              *dst_data) \
{ \
  gdk_data_read_yuyv_like (dst_data, \
                           self->width, \
                           self->planes[0].data + (y * self->planes[0].stride), \
                           y_index, u_index, v_index); \
}

READ_YUYV_FUNC (yuyv, 0, 1, 3)
READ_YUYV_FUNC (yvyu, 0, 3, 1)
READ_YUYV_FUNC (uyvy, 1, 0, 2)
READ_YUYV_FUNC (vyuy, 1, 2, 0)

static inline void
gdk_data_read_3_1 (guchar       *dst_data,
                   gsize         width,
                   const guchar *xrgb_data,
                   const guchar *a_data,
                   gsize         a_index)
{
  gsize x;

  for (x = 0; x < width; x++)
    {
      dst_data[4 * x + 0] = xrgb_data[4 * x + 0];
      dst_data[4 * x + 1] = xrgb_data[4 * x + 1];
      dst_data[4 * x + 2] = xrgb_data[4 * x + 2];
      dst_data[4 * x + 3] = xrgb_data[4 * x + 3];
      dst_data[4 * x + a_index] = a_data[x];
    }
}

#define READ_3_1_FUNC(name, a_index) \
static void \
gdk_data_read_ ## name (const GdkDataBuffer *self, \
                        gsize                y, \
                        guchar              *dst_data) \
{ \
  gdk_data_read_3_1 (dst_data, \
                     self->width, \
                     self->planes[0].data + (y * self->planes[0].stride), \
                     self->planes[1].data + (y * self->planes[1].stride), \
                     a_index); \
}

READ_3_1_FUNC (xrgb8_a8, 0)
READ_3_1_FUNC (xbgr8_a8, 0)
READ_3_1_FUNC (rgbx8_a8, 3)
READ_3_1_FUNC (bgrx8_a8, 3)

#define VULKAN_SWIZZLE(_R, _G, _B, _A) { VK_COMPONENT_SWIZZLE_ ## _R, VK_COMPONENT_SWIZZLE_ ## _G, VK_COMPONENT_SWIZZLE_ ## _B, VK_COMPONENT_SWIZZLE_ ## _A }
#define VULKAN_DEFAULT_SWIZZLE VULKAN_SWIZZLE (R, G, B, A)

static void             gdk_data_convert_generic_rgb8       (const GdkDataBuffer        *self,
                                                             guchar                     *dst_data,
                                                             gsize                       dst_stride);

GdkDataFormatDescription data_formats[] = {
  [GDK_DATA_YUV410] = {
    .name = "YUV410",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 4, 4 },
    .dmabuf_fourcc = DRM_FORMAT_YUV410,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yuv410,
  },
  [GDK_DATA_YVU410] = {
    .name = "YVU410",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 4, 4 },
    .dmabuf_fourcc = DRM_FORMAT_YVU410,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yvu410,
  },
  [GDK_DATA_YUV411] = {
    .name = "YUV411",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 4, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YUV411,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yuv411,
  },
  [GDK_DATA_YVU411] = {
    .name = "YVU411",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 4, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YVU411,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yvu411,
  },
  [GDK_DATA_YUV420] = {
    .name = "YUV420",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_YUV420,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yuv420,
  },
  [GDK_DATA_YVU420] = {
    .name = "YVU420",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_YVU420,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yvu420,
  },
  [GDK_DATA_YUV422] = {
    .name = "YUV422",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YUV422,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yuv422,
  },
  [GDK_DATA_YVU422] = {
    .name = "YVU422",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YVU422,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yvu422,
  },
  [GDK_DATA_YUV444] = {
    .name = "YUV444",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YUV444,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yuv444,
  },
  [GDK_DATA_YVU444] = {
    .name = "YVU444",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 3,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YVU444,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yvu444,
  },
  [GDK_DATA_YUYV] = {
    .name = "YUYV",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YUYV,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yuyv,
  },
  [GDK_DATA_YVYU] = {
    .name = "YVYU",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_YVYU,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_yvyu,
  },
  [GDK_DATA_UYVY] = {
    .name = "UYVY",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_UYVY,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_uyvy,
  },
  [GDK_DATA_VYUY] = {
    .name = "VYUY",
    .conversion = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_VYUY,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_vyuy,
  },
  [GDK_DATA_RGBX8_A8] = {
    .name = "GDK_DATA_RGBX8_A8",
    .conversion = GDK_MEMORY_R8G8B8A8,
    .is_yuv = FALSE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_RGBX8888_A8,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_rgbx8_a8,
  },
  [GDK_DATA_BGRX8_A8] = {
    .name = "GDK_DATA_BGRX8_A8",
    .conversion = GDK_MEMORY_B8G8R8A8,
    .is_yuv = FALSE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_BGRX8888_A8,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_bgrx8_a8,
  },
  [GDK_DATA_XRGB8_A8] = {
    .name = "GDK_DATA_XRGB8_A8",
    .conversion = GDK_MEMORY_A8R8G8B8,
    .is_yuv = FALSE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_XRGB8888_A8,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_xrgb8_a8,
  },
  [GDK_DATA_XBGR8_A8] = {
    .name = "GDK_DATA_XBGR8_A8",
    .conversion = GDK_MEMORY_A8B8G8R8,
    .is_yuv = FALSE,
    .n_planes = 1,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_XBGR8888_A8,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_data_convert_generic_rgb8,
    .read_line = gdk_data_read_xbgr8_a8,
  },
};

static void
yuv2rgb_line_rgb8 (guchar *data,
                   gsize   width)
{
  gsize i;
  int r, g, b;

  for (i = 0; i < width; i++)
    {
      get_uv_values (&itu601_narrow, data[1], data[2], &r, &g, &b);
      set_rgb_values (data, data[0], r, g, b);
      data += 3;
    }
}

static void
gdk_data_convert_generic_rgb8 (const GdkDataBuffer *self,
                               guchar              *dst_data,
                               gsize                dst_stride)
{
  gsize y;

  for (y = 0; y < self->height; y++)
    {
      guchar *dst = dst_data + (y * dst_stride);

      data_formats[self->format].read_line (self, y, dst);
      if (data_formats[self->format].is_yuv)
        yuv2rgb_line_rgb8 (dst, self->width);
    }
}

const char *
gdk_data_format_get_name (GdkDataFormat format)
{
  return data_formats[format].name;
}

GdkMemoryFormat
gdk_data_format_get_conversion (GdkDataFormat format)
{
  return data_formats[format].conversion;
}

gsize
gdk_data_format_n_planes (GdkDataFormat format)
{
  return data_formats[format].n_planes;
}

gsize
gdk_data_format_alignment (GdkDataFormat format)
{
  return data_formats[format].alignment;
}

gsize
gdk_data_format_block_width (GdkDataFormat format)
{
  return data_formats[format].block_size[0];
}

gsize
gdk_data_format_block_height (GdkDataFormat format)
{
  return data_formats[format].block_size[1];
}

gboolean
gdk_data_format_is_yuv (GdkDataFormat format)
{
  return data_formats[format].is_yuv;
}

guint32
gdk_data_format_get_dmabuf_fourcc (GdkDataFormat format)
{
  return data_formats[format].dmabuf_fourcc;
}

gboolean
gdk_data_format_find_by_dmabuf_fourcc (guint32        fourcc,
                                       GdkDataFormat *out_format)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (data_formats); i++)
    {
      if (data_formats[i].dmabuf_fourcc == fourcc)
        {
          *out_format = i;
          return TRUE;
        }
    }

  *out_format = GDK_DATA_N_FORMATS;
  return FALSE;
}

#ifdef GDK_RENDERING_VULKAN
VkFormat
gdk_data_format_vk_format (GdkDataFormat       format,
                           VkComponentMapping *out_swizzle)
{
  if (out_swizzle)
    *out_swizzle = data_formats[format].vk.swizzle;

  return data_formats[format].vk.format;
}
#endif

void
gdk_data_convert (const GdkDataBuffer *self,
                  guchar              *dest_data,
                  gsize                dest_stride)
{
  data_formats[self->format].convert (self,
                                      dest_data,
                                      dest_stride);
}

