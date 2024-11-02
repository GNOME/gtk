#include "config.h"

#include "gdkdxgiformatprivate.h"

#include "gdkmemoryformatprivate.h"


typedef struct _GdkDXGIFormatTable GdkDXGIFormatTable;

typedef void            (* GdkDXGIConvertFunc)              (DXGI_FORMAT                 src_format,
                                                             gboolean                    src_premultiplied,
                                                             const guchar               *src_data,
                                                             gsize                       src_stride,
                                                             GdkColorState              *src_color_state,
                                                             GdkMemoryFormat             dest_format,
                                                             guchar                     *dest_data,
                                                             gsize                       dest_stride,
                                                             GdkColorState              *dest_color_state,
                                                             gsize                       width,
                                                             gsize                       height);

struct _GdkDXGIFormatTable
{
  GdkMemoryFormat memory_format_straight;
  GdkMemoryFormat memory_format_premultiplied;
  guint is_yuv :1;
  GLenum internal_format;
  GdkDXGIConvertFunc convert_func;
};

static void
default_convert_func (DXGI_FORMAT      src_format,
                      gboolean         src_premultiplied,
                      const guchar    *src_data,
                      gsize            src_stride,
                      GdkColorState   *src_color_state,
                      GdkMemoryFormat  dest_format,
                      guchar          *dest_data,
                      gsize            dest_stride,
                      GdkColorState   *dest_color_state,
                      gsize            width,
                      gsize            height)
{
  GdkMemoryFormat src_memory_format = gdk_dxgi_format_get_memory_format (src_format, src_premultiplied);

  gdk_memory_convert (dest_data,
                      dest_stride,
                      dest_format,
                      dest_color_state,
                      src_data,
                      src_stride,
                      src_memory_format,
                      src_color_state,
                      width,
                      height);
}

static const GdkDXGIFormatTable format_table[] = {
  [DXGI_FORMAT_UNKNOWN] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32A32_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32A32_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_R32G32B32A32_FLOAT,
    .memory_format_premultiplied = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
    .is_yuv = FALSE,
    .internal_format = GL_RGBA32F,
    .convert_func = default_convert_func
  },
  [DXGI_FORMAT_R32G32B32A32_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32A32_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32B32_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16B16A16_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16B16A16_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_R16G16B16A16_FLOAT,
    .memory_format_premultiplied = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .is_yuv = FALSE,
    .internal_format = GL_RGBA16F,
    .convert_func = default_convert_func
  },
  [DXGI_FORMAT_R16G16B16A16_UNORM] = {
    .memory_format_straight = GDK_MEMORY_R16G16B16A16,
    .memory_format_premultiplied = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .internal_format = GL_RGBA16,
    .convert_func = default_convert_func,
  },
  [DXGI_FORMAT_R16G16B16A16_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16B16A16_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16B16A16_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G32_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32G8X24_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_D32_FLOAT_S8X24_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_X32_TYPELESS_G8X24_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R10G10B10A2_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R10G10B10A2_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R10G10B10A2_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R11G11B10_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8B8A8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8B8A8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_R8G8B8A8,
    .memory_format_premultiplied = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .internal_format = GL_RGBA8,
    .convert_func = default_convert_func
  },
  [DXGI_FORMAT_R8G8B8A8_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8B8A8_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8B8A8_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8B8A8_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16G16_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_D32_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R32_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R24G8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_D24_UNORM_S8_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R24_UNORM_X8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_X24_TYPELESS_G8_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16_FLOAT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_D16_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R16_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8_UINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8_SINT] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_A8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R1_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R9G9B9E5_SHAREDEXP] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_R8G8_B8G8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_G8R8_G8B8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC1_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC1_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC1_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC2_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC2_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC2_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC3_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC3_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC3_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC4_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC4_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC4_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC5_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC5_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC5_SNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B5G6R5_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B5G5R5A1_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B8G8R8A8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_B8G8R8A8,
    .memory_format_premultiplied = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .internal_format = GL_BGRA8_EXT,
    .convert_func = default_convert_func
  },
  [DXGI_FORMAT_B8G8R8X8_UNORM] = {
    .memory_format_straight = GDK_MEMORY_B8G8R8X8,
    .memory_format_premultiplied = GDK_MEMORY_B8G8R8X8,
    .is_yuv = FALSE,
    .internal_format = GL_BGRA8_EXT,
    .convert_func = default_convert_func
  },
  [DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B8G8R8A8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B8G8R8A8_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B8G8R8X8_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B8G8R8X8_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC6H_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC6H_UF16] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC6H_SF16] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC7_TYPELESS] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC7_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_BC7_UNORM_SRGB] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_AYUV] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_Y410] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_Y416] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_NV12] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_P010] = {
    .memory_format_straight = GDK_MEMORY_R16G16B16,
    .memory_format_premultiplied = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_P016] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_420_OPAQUE] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_YUY2] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_Y210] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_Y216] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_NV11] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_AI44] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_IA44] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_P8] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_A8P8] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_B4G4R4A4_UNORM] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_P208] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_V208] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
  [DXGI_FORMAT_V408] = {
    .memory_format_straight = GDK_MEMORY_N_FORMATS,
    .memory_format_premultiplied = GDK_MEMORY_N_FORMATS,
    .is_yuv = FALSE,
    .internal_format = 0,
    .convert_func = NULL
  },
};

gboolean
gdk_dxgi_format_is_supported (DXGI_FORMAT dxgi_format)
{
  if (dxgi_format >= G_N_ELEMENTS (format_table))
    return FALSE;

  return format_table[dxgi_format].convert_func != NULL;
}

GLenum
gdk_dxgi_format_get_gl_format (DXGI_FORMAT dxgi_format)
{
  if (dxgi_format >= G_N_ELEMENTS (format_table))
    return 0;

  return format_table[dxgi_format].internal_format;
}

/*<private>:
 * Gets the memory format that best represents the given
 * dxgi_format.
 * 
 * Note that this does *not* provide an exact match as there
 * are more DXGI formats supported than exist memory formats.
 * If you want an exact match, check gdk_memory_format_dxgi_format()
 * for the returned format.
 */
GdkMemoryFormat
gdk_dxgi_format_get_memory_format (DXGI_FORMAT dxgi_format,
                                   gboolean    premultiplied)
{
  if (dxgi_format >= G_N_ELEMENTS (format_table))
    return GDK_MEMORY_N_FORMATS;

  if (premultiplied)
    return format_table[dxgi_format].memory_format_premultiplied;
  else
    return format_table[dxgi_format].memory_format_straight;
}

gboolean
gdk_dxgi_format_is_yuv (DXGI_FORMAT dxgi_format)
{
  return format_table[dxgi_format].is_yuv;
}

void
gdk_dxgi_format_convert (DXGI_FORMAT      src_format,
                         gboolean         src_premultiplied,
                         const guchar    *src_data,
                         gsize            src_stride,
                         GdkColorState   *src_color_state,
                         GdkMemoryFormat  dest_format,
                         guchar          *dest_data,
                         gsize            dest_stride,
                         GdkColorState   *dest_color_state,
                         gsize            width,
                         gsize            height)
{
  g_assert (format_table[src_format].convert_func);

  format_table[src_format].convert_func (src_format, src_premultiplied,
                                         src_data, src_stride, src_color_state,
                                         dest_format, 
                                         dest_data, dest_stride, dest_color_state,
                                         width, height);
}
  