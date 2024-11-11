#include "config.h"

#include "gdkdataformatprivate.h"

#include "gdk/gdkdmabuffourccprivate.h"

typedef struct _GdkDataFormatDescription GdkDataFormatDescription;

struct _GdkDataFormatDescription
{
  const char *name;
  GdkMemoryFormat conversion;
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
gdk_convert_nv12_like (guchar       *dst_data,
                       gsize         dst_stride,
                       gsize         width,
                       gsize         height,
                       const guchar *y_data,
                       gsize         y_stride,
                       const guchar *uv_data,
                       gsize         uv_stride,
                       gboolean      uv_swapped,
                       gsize         x_subsample,
                       gsize         y_subsample)
{
  gsize x, y;

  for (y = 0; y < height; y += y_subsample)
    {
      for (x = 0; x < width; x += x_subsample)
        {
          int r, g, b;
          gsize xs, ys;

          get_uv_values (&itu601_narrow,
                         uv_data[x / x_subsample * 2 + (uv_swapped ? 1 : 0)], 
                         uv_data[x / x_subsample * 2 + (uv_swapped ? 0 : 1)],
                         &r, &g, &b);

          for (ys = 0; ys < y_subsample && y + ys < height; ys++)
            for (xs = 0; xs < x_subsample && x + xs < width; xs++)
              set_rgb_values (&dst_data[3 * (x + xs) + dst_stride * ys], y_data[x + xs + y_stride * ys], r, g, b);
        }
      dst_data += y_subsample * dst_stride;
      y_data += y_subsample * y_stride;
      uv_data += uv_stride;
    }
}

#define CONVERT_NV12_FUNC(name, uv_swapped, x_subsample, y_subsample) \
static void \
gdk_convert_ ## name (const GdkDataBuffer *self, \
                      guchar              *dst_data, \
                      gsize                dst_stride) \
{ \
  gdk_convert_nv12_like (dst_data, \
                         dst_stride, \
                         self->width, \
                         self->height, \
                         self->planes[0].data, \
                         self->planes[0].stride, \
                         self->planes[1].data, \
                         self->planes[1].stride, \
                         uv_swapped, x_subsample, y_subsample); \
}

CONVERT_NV12_FUNC (nv12, FALSE, 2, 2)
CONVERT_NV12_FUNC (nv21, TRUE, 2, 2)
CONVERT_NV12_FUNC (nv16, FALSE, 2, 1)
CONVERT_NV12_FUNC (nv61, TRUE, 2, 1)
CONVERT_NV12_FUNC (nv24, FALSE, 1, 1)
CONVERT_NV12_FUNC (nv42, TRUE, 1, 1)

static inline void
get_uv_values16 (const YUVCoefficients *coeffs,
                 guint16                u,
                 guint16                v,
                 gint64                *out_r,
                 gint64                *out_g,
                 gint64                *out_b)
{
  gint64 u2 = (gint64) u - 32767;
  gint64 v2 = (gint64) v - 32767;
  *out_r = coeffs->v_to_r * v2;
  *out_g = coeffs->u_to_g * u2 + coeffs->v_to_g * v2;
  *out_b = coeffs->u_to_b * u2;
}

static inline void
set_rgb_values16 (guint16 rgb[3],
                  guint16 y,
                  gint64  r,
                  gint64  g,
                  gint64  b)
{
  gint64 y2 = (gint64) y * 65536;

  rgb[0] = CLAMP ((y2 + r) >> 16, 0, 65535);
  rgb[1] = CLAMP ((y2 + g) >> 16, 0, 65535);
  rgb[2] = CLAMP ((y2 + b) >> 16, 0, 65535);
}

static inline void
gdk_convert_p010_like (guint16       *dst_data,
                       gsize          dst_stride,
                       gsize          width,
                       gsize          height,
                       const guint16 *y_data,
                       gsize          y_stride,
                       const guint16 *uv_data,
                       gsize          uv_stride,
                       gsize          bits_used,
                       gboolean       uv_swapped,
                       gsize          x_subsample,
                       gsize          y_subsample)
{
  gsize x, y;
  guint16 mask;

  mask = 0xFFFF << (16 - bits_used);

  for (y = 0; y < height; y += y_subsample)
    {
      for (x = 0; x < width; x += x_subsample)
        {
          gint64 r, g, b;
          gsize xs, ys;
          guint16 u, v;

          u = uv_data[x / x_subsample * 2 + (uv_swapped ? 1 : 0)];
          u = (u & mask) | (u >> bits_used);
          v = uv_data[x / x_subsample * 2 + (uv_swapped ? 0 : 1)];
          v = (v & mask) | (v >> bits_used);
          get_uv_values16 (&itu601_narrow, u, v, &r, &g, &b);

          for (ys = 0; ys < y_subsample && y + ys < height; ys++)
            for (xs = 0; xs < x_subsample && x + xs < width; xs++)
              {
                guint16 y_ = y_data[x + xs + y_stride * ys];
                y_ = (y_ & mask) | (y_ >> bits_used);
                set_rgb_values16 (&dst_data[3 * (x + xs) + dst_stride * ys], y_, r, g, b);
              }
        }
      dst_data += y_subsample * dst_stride;
      y_data += y_subsample * y_stride;
      uv_data += uv_stride;
    }
}

#define CONVERT_P010_FUNC(name, bits_used, uv_swapped, x_subsample, y_subsample) \
static void \
gdk_convert_ ## name (const GdkDataBuffer *self, \
                      guchar              *dst_data, \
                      gsize                dst_stride) \
{ \
  gdk_convert_p010_like ((guint16 *) dst_data, \
                         dst_stride / 2, \
                         self->width, \
                         self->height, \
                         (guint16 *) self->planes[0].data, \
                         self->planes[0].stride / 2, \
                         (guint16 *) self->planes[1].data, \
                         self->planes[1].stride / 2, \
                         bits_used, uv_swapped, x_subsample, y_subsample); \
}

CONVERT_P010_FUNC (p010, 10, FALSE, 2, 2)
CONVERT_P010_FUNC (p012, 12, FALSE, 2, 2)
CONVERT_P010_FUNC (p016, 16, FALSE, 2, 2)

static inline void
gdk_convert_3_plane (guchar       *dst_data,
                     gsize         dst_stride,
                     gsize         width,
                     gsize         height,
                     const guchar *y_data,
                     gsize         y_stride,
                     const guchar *u_data,
                     gsize         u_stride,
                     const guchar *v_data,
                     gsize         v_stride,
                     gsize         x_subsample,
                     gsize         y_subsample)
{
  gsize x, y;

  for (y = 0; y < height; y += y_subsample)
    {
      for (x = 0; x < width; x += x_subsample)
        {
          int r, g, b;
          gsize xs, ys;

          get_uv_values (&itu601_narrow, u_data[x / x_subsample], v_data[x / x_subsample], &r, &g, &b);

          for (ys = 0; ys < y_subsample && y + ys < height; ys++)
            for (xs = 0; xs < x_subsample && x + xs < width; xs++)
              set_rgb_values (&dst_data[3 * (x + xs) + dst_stride * ys], y_data[x + xs + y_stride * ys], r, g, b);
        }
      dst_data += y_subsample * dst_stride;
      y_data += y_subsample * y_stride;
      u_data += u_stride;
      v_data += v_stride;
    }
}

#define CONVERT_3_PLANE_FUNC(name, uv_swapped, x_subsample, y_subsample) \
static void \
gdk_convert_ ## name (const GdkDataBuffer *self, \
                      guchar              *dst_data, \
                      gsize                dst_stride) \
{ \
  gdk_convert_3_plane (dst_data, \
                       dst_stride, \
                       self->width, \
                       self->height, \
                       self->planes[0].data, \
                       self->planes[0].stride, \
                       self->planes[uv_swapped ? 2 : 1].data, \
                       self->planes[uv_swapped ? 2 : 1].stride, \
                       self->planes[uv_swapped ? 1 : 2].data, \
                       self->planes[uv_swapped ? 1 : 2].stride, \
                       x_subsample, y_subsample); \
}

CONVERT_3_PLANE_FUNC (yuv410, FALSE, 4, 4)
CONVERT_3_PLANE_FUNC (yvu410, TRUE, 4, 4)
CONVERT_3_PLANE_FUNC (yuv411, FALSE, 4, 1)
CONVERT_3_PLANE_FUNC (yvu411, TRUE, 4, 1)
CONVERT_3_PLANE_FUNC (yuv420, FALSE, 2, 2)
CONVERT_3_PLANE_FUNC (yvu420, TRUE, 2, 2)
CONVERT_3_PLANE_FUNC (yuv422, FALSE, 2, 1)
CONVERT_3_PLANE_FUNC (yvu422, TRUE, 2, 1)
CONVERT_3_PLANE_FUNC (yuv444, FALSE, 1, 1)
CONVERT_3_PLANE_FUNC (yvu444, TRUE, 1, 1)

static inline void
gdk_convert_yuyv_like (guchar       *dst_data,
                       gsize         dst_stride,
                       gsize         width,
                       gsize         height,
                       const guchar *src_data,
                       gsize         src_stride,
                       gsize         y_index,
                       gsize         u_index,
                       gsize         v_index)
{
  gsize x, y;

  for (y = 0; y < height; y ++)
    {
      for (x = 0; x < width; x += 2)
        {
          int r, g, b;

          get_uv_values (&itu601_narrow, src_data[2 * x + u_index], src_data[2 * x + v_index], &r, &g, &b);
          set_rgb_values (&dst_data[3 * x], src_data[2 * x + y_index], r, g, b);
          if (x + 1 < width)
            set_rgb_values (&dst_data[3 * (x + 1)], src_data[2 * x + y_index + 2], r, g, b);
        }
      dst_data += dst_stride;
      src_data += src_stride;
    }
}

#define CONVERT_YUYV_FUNC(name, y_index, u_index, v_index) \
static void \
gdk_convert_ ## name (const GdkDataBuffer *self, \
                      guchar              *dst_data, \
                      gsize                dst_stride) \
{ \
  gdk_convert_yuyv_like (dst_data, \
                         dst_stride, \
                         self->width, \
                         self->height, \
                         self->planes[0].data, \
                         self->planes[0].stride, \
                         y_index, u_index, v_index); \
}

CONVERT_YUYV_FUNC (yuyv, 0, 1, 3)
CONVERT_YUYV_FUNC (yvyu, 0, 3, 1)
CONVERT_YUYV_FUNC (uyvy, 1, 0, 2)
CONVERT_YUYV_FUNC (vyuy, 1, 2, 0)

#define VULKAN_SWIZZLE(_R, _G, _B, _A) { VK_COMPONENT_SWIZZLE_ ## _R, VK_COMPONENT_SWIZZLE_ ## _G, VK_COMPONENT_SWIZZLE_ ## _B, VK_COMPONENT_SWIZZLE_ ## _A }
#define VULKAN_DEFAULT_SWIZZLE VULKAN_SWIZZLE (R, G, B, A)

GdkDataFormatDescription data_formats[] = {
  [GDK_DATA_NV12] = {
    .name = "NV12",
    .conversion = GDK_MEMORY_R8G8B8,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_NV12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_convert_nv12,
  },
  [GDK_DATA_NV21] = {
    .name = "NV21",
    .conversion = GDK_MEMORY_R8G8B8,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_NV21,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_convert_nv21,
  },
  [GDK_DATA_NV16] = {
    .name = "NV16",
    .conversion = GDK_MEMORY_R8G8B8,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_NV16,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_convert_nv16,
  },
  [GDK_DATA_NV61] = {
    .name = "NV61",
    .conversion = GDK_MEMORY_R8G8B8,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 1 },
    .dmabuf_fourcc = DRM_FORMAT_NV61,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_convert_nv61,
  },
  [GDK_DATA_NV24] = {
    .name = "NV24",
    .conversion = GDK_MEMORY_R8G8B8,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_NV24,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_convert_nv24,
  },
  [GDK_DATA_NV42] = {
    .name = "NV42",
    .conversion = GDK_MEMORY_R8G8B8,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 1, 1 },
    .dmabuf_fourcc = DRM_FORMAT_NV42,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
    .convert = gdk_convert_nv42,
  },
  [GDK_DATA_P010] = {
    .name = "P010",
    .conversion = GDK_MEMORY_R16G16B16,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_P010,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_convert_p010,
  },
  [GDK_DATA_P012] = {
    .name = "P012",
    .conversion = GDK_MEMORY_R16G16B16,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_P012,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_convert_p012,
  },
  [GDK_DATA_P016] = {
    .name = "P016",
    .conversion = GDK_MEMORY_R16G16B16,
    .n_planes = 2,
    .alignment = 4,
    .block_size = { 2, 2 },
    .dmabuf_fourcc = DRM_FORMAT_P016,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
    .convert = gdk_convert_p016,
  },
  [GDK_DATA_YUV410] = {
    .name = "YUV410",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yuv410,
  },
  [GDK_DATA_YVU410] = {
    .name = "YVU410",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yvu410,
  },
  [GDK_DATA_YUV411] = {
    .name = "YUV411",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yuv411,
  },
  [GDK_DATA_YVU411] = {
    .name = "YVU411",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yvu411,
  },
  [GDK_DATA_YUV420] = {
    .name = "YUV420",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yuv420,
  },
  [GDK_DATA_YVU420] = {
    .name = "YVU420",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yvu420,
  },
  [GDK_DATA_YUV422] = {
    .name = "YUV422",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yuv422,
  },
  [GDK_DATA_YVU422] = {
    .name = "YVU422",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yvu422,
  },
  [GDK_DATA_YUV444] = {
    .name = "YUV444",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yuv444,
  },
  [GDK_DATA_YVU444] = {
    .name = "YVU444",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yvu444,
  },
  [GDK_DATA_YUYV] = {
    .name = "YUYV",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yuyv,
  },
  [GDK_DATA_YVYU] = {
    .name = "YVYU",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_yvyu,
  },
  [GDK_DATA_UYVY] = {
    .name = "UYVY",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_uyvy,
  },
  [GDK_DATA_VYUY] = {
    .name = "VYUY",
    .conversion = GDK_MEMORY_R8G8B8,
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
    .convert = gdk_convert_vyuy,
  },
};

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
  return TRUE;
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

