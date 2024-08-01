#include "config.h"

#include "gdkavifprivate.h"
#include "gdkmemorytexturebuilder.h"
#include "gdktexture.h"
#include "gdkdisplay.h"
#include "gdkcicpparamsprivate.h"
#include "gdkcolorstateprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkdmabuftexture.h"
#include "gdktexturedownloader.h"

#include <avif/avif.h>

#ifdef HAVE_DMABUF

#include "gdkdmabuftextureprivate.h"
#include "gdkdmabuftexturebuilder.h"
#include "gdkdmabuffourccprivate.h"

/* NOTE: We build this code outside of libgtk, in tests and the image tool, so this
 * code has to be a bit careful to avoid depending on private api, which can also
 * be dragged in indirectly, via inlines.
 */

#ifdef AVIF_DEBUG
#define DEBUG(fmt,...) g_log ("avif", G_LOG_LEVEL_DEBUG, fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define DEBUG(fmt,...)
#endif

/* {{{ udmabuf support */

#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/udmabuf.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <gio/gio.h>

static int udmabuf_fd;

static gboolean
udmabuf_initialize (GError **error)
{
  if (udmabuf_fd == 0)
    {
      udmabuf_fd = open ("/dev/udmabuf", O_RDWR);
      if (udmabuf_fd == -1)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Failed to open /dev/udmabuf: %s",
                       g_strerror (errno));
        }
    }

  return udmabuf_fd != -1;
}

typedef struct
{
  int mem_fd;
  int dmabuf_fd;
  size_t size;
  gpointer data;
} UDmabuf;

static void
udmabuf_free (gpointer data)
{
  UDmabuf *udmabuf = data;

  munmap (udmabuf->data, udmabuf->size);
  close (udmabuf->mem_fd);
  close (udmabuf->dmabuf_fd);

  g_free (udmabuf);
}

#define align(x,y) (((x) + (y) - 1) & ~((y) - 1))

static UDmabuf *
udmabuf_allocate (size_t   size,
                  GError **error)
{
  int mem_fd = -1;
  int dmabuf_fd = -1;
  uint64_t alignment;
  int res;
  gpointer data;
  UDmabuf *udmabuf;

  if (udmabuf_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "udmabuf not available");
      goto fail;
    }

  alignment = sysconf (_SC_PAGE_SIZE);

  size = align (size, alignment);

  mem_fd = memfd_create ("gtk", MFD_ALLOW_SEALING);
  if (mem_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to open /dev/udmabuf: %s",
                   g_strerror (errno));
      goto fail;
    }

  res = ftruncate (mem_fd, size);
  if (res == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "ftruncate failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  if (fcntl (mem_fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "ftruncate failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  dmabuf_fd = ioctl (udmabuf_fd,
                     UDMABUF_CREATE,
                     &(struct udmabuf_create) {
                       .memfd = mem_fd,
                       .flags = UDMABUF_FLAGS_CLOEXEC,
                       .offset = 0,
                       .size = size
                     });

  if (dmabuf_fd < 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "UDMABUF_CREATE ioctl failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  data = mmap (NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, 0);

  if (!data)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "mmap failed: %s",
                    g_strerror (errno));
      goto fail;
    }

  udmabuf = g_new0 (UDmabuf, 1);

  udmabuf->mem_fd = mem_fd;
  udmabuf->dmabuf_fd = dmabuf_fd;
  udmabuf->size = size;
  udmabuf->data = data;

  return udmabuf;

fail:
  if (mem_fd != -1)
    close (mem_fd);
  if (dmabuf_fd != -1)
    close (dmabuf_fd);

  return NULL;
}

/* }}} */
/* {{{ dmabuf texture support */

static GdkTexture *
gdk_avif_create_dmabuf_texture (avifDecoder    *decoder,
                                GdkColorState  *color_state,
                                GError        **error)
{
  guint fourcc = 0;
  guint32 width, height, depth;
  GdkDmabufTextureBuilder *builder;
  UDmabuf *udmabuf;
  gboolean combine_uv = FALSE;
  guchar *data;
  gsize size0, size1, size2;
  GdkTexture *texture;

  if (!udmabuf_initialize (error))
    return NULL;

  width = decoder->image->width;
  height = decoder->image->height;
  depth = decoder->image->depth;

  if (decoder->image->alphaPlane)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   "no yuv dmabuf with alpha");
      return NULL;
    }

  switch (decoder->image->yuvFormat)
    {
    case AVIF_PIXEL_FORMAT_YUV444:
      DEBUG ("load: format yuv444");
      if (depth == 8)
        fourcc = DRM_FORMAT_YUV444;
      break;
    case AVIF_PIXEL_FORMAT_YUV422:
      DEBUG ("load: format yuv422");
      if (depth == 8)
        fourcc = DRM_FORMAT_YUV422;
      break;
    case AVIF_PIXEL_FORMAT_YUV420:
      DEBUG ("load: format yuv420");
      combine_uv = TRUE;
      if (depth == 8)
        fourcc = DRM_FORMAT_NV12;
      else if (depth == 10)
        fourcc = DRM_FORMAT_P010;
      else if (depth == 12)
        fourcc = DRM_FORMAT_P012;
      else if (depth == 16)
        fourcc = DRM_FORMAT_P016;
      break;
    case AVIF_PIXEL_FORMAT_YUV400:
    case AVIF_PIXEL_FORMAT_NONE:
    case AVIF_PIXEL_FORMAT_COUNT:
    default:
      break;
    }

  if (fourcc == 0)
    {
      const char *names[] = { "none", "yuv444", "yuv422", "yuv420", "yuv400" };

      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   "unsupported pixel format %s, depth %u",
                   names[decoder->image->yuvFormat], decoder->image->depth);
      return NULL;
    }

  DEBUG ("load: use fourcc %.4s", (char *)&fourcc);

  builder = gdk_dmabuf_texture_builder_new ();

  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_color_state (builder, color_state);
  gdk_dmabuf_texture_builder_set_fourcc (builder, fourcc);
  gdk_dmabuf_texture_builder_set_modifier (builder, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, FALSE);

  if (combine_uv)
    {
      size0 = height * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y);
      size1 = height / 2 * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y);

      udmabuf = udmabuf_allocate (size0 + size1, NULL);

      data = (guchar *) udmabuf->data;

      if (depth == 8)
        {
          memcpy (data, avifImagePlane (decoder->image, AVIF_CHAN_Y), size0);
          for (int i = 0; i < height / 2; i++)
            {
              guchar *usrc = avifImagePlane (decoder->image, AVIF_CHAN_U) + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_U);
              guchar *vsrc = avifImagePlane (decoder->image, AVIF_CHAN_V) + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_V);
              guchar *dest = data + size0 + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y);
              for (int j = 0; j < width / 2; j++)
                {
                  dest[2*j] = usrc[j];
                  dest[2*j+1] = vsrc[j];
                }
            }
        }
      else
        {
          for (int i = 0; i < height; i++)
            {
              guint16 *src = (guint16 *) (avifImagePlane (decoder->image, AVIF_CHAN_Y) + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y));
              guint16 *dest = (guint16 *) (data + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y));
              for (int j = 0; j < width; j++)
                dest[j] = src[j] << (16 - depth);
            }
          for (int i = 0; i < height / 2; i++)
            {
              guint16 *usrc = (guint16 *)(avifImagePlane (decoder->image, AVIF_CHAN_U) + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_U));
              guint16 *vsrc = (guint16 *)(avifImagePlane (decoder->image, AVIF_CHAN_V) + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_V));
              guint16 *dest = (guint16 *)(data + size0 + i * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y));
              for (int j = 0; j < width / 2; j++)
                {
                  dest[2*j] = usrc[j] << (16 - depth);
                  dest[2*j+1] = vsrc[j] << (16 - depth);
                }
            }
        }

      gdk_dmabuf_texture_builder_set_n_planes (builder, 2);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, udmabuf->dmabuf_fd);
      gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y));

      gdk_dmabuf_texture_builder_set_fd (builder, 1, udmabuf->dmabuf_fd);
      gdk_dmabuf_texture_builder_set_offset (builder, 1, size0);
      gdk_dmabuf_texture_builder_set_stride (builder, 1, avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y));
    }
  else
    {
      size0 = height * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y);
      size1 = height * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_U);
      size2 = height * avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_V);

      udmabuf = udmabuf_allocate (size0 + size1 + size2, NULL);

      data = (guchar *) udmabuf->data;
      memcpy (data, avifImagePlane (decoder->image, AVIF_CHAN_Y), size0);
      memcpy (data + size0, avifImagePlane (decoder->image, AVIF_CHAN_U), size1);
      memcpy (data + size0 + size1, avifImagePlane (decoder->image, AVIF_CHAN_V), size2);

      gdk_dmabuf_texture_builder_set_n_planes (builder, 3);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, udmabuf->dmabuf_fd);
      gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y));

      gdk_dmabuf_texture_builder_set_fd (builder, 1, udmabuf->dmabuf_fd);
      gdk_dmabuf_texture_builder_set_offset (builder, 1, size0);
      gdk_dmabuf_texture_builder_set_stride (builder, 1, avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_U));

      gdk_dmabuf_texture_builder_set_fd (builder, 2, udmabuf->dmabuf_fd);
      gdk_dmabuf_texture_builder_set_offset (builder, 2, size0 + size1);
      gdk_dmabuf_texture_builder_set_stride (builder, 2, avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_V));
    }

  texture = gdk_dmabuf_texture_builder_build (builder, udmabuf_free, udmabuf, error);

  g_object_unref (builder);

  return texture;
}
#endif /* HAVE_DMABUF */

/*  }}} */
/* {{{ memory texture support */

static GdkTexture *
gdk_avif_create_memory_texture (avifDecoder    *decoder,
                                GdkColorState  *color_state,
                                GError        **error)
{
  guint32 width, height, depth, stride, bpp;
  GdkTexture *texture;
  GdkMemoryTextureBuilder *builder;
  guchar *data;
  GBytes *bytes;
  GdkMemoryFormat format = GDK_MEMORY_N_FORMATS;
  int X_SUB, Y_SUB;

  width = decoder->image->width;
  height = decoder->image->height;
  depth = decoder->image->depth;

  switch (decoder->image->yuvFormat)
    {
    case AVIF_PIXEL_FORMAT_YUV444:
      X_SUB = 1; Y_SUB = 1;
      if (depth == 8)
        {
          format = GDK_MEMORY_R8G8B8A8;
          bpp = 4;
        }
      else
        {
          format = GDK_MEMORY_R16G16B16A16;
          bpp = 8;
        }
      break;

    case AVIF_PIXEL_FORMAT_YUV422:
      X_SUB = 2; Y_SUB = 1;
      if (depth == 8)
        {
          format = GDK_MEMORY_R8G8B8A8;
          bpp = 8;
        }
      else
        {
          format = GDK_MEMORY_R16G16B16A16;
          bpp = 8;
        }
      break;

    case AVIF_PIXEL_FORMAT_YUV420:
      X_SUB = 2; Y_SUB = 2;
      break;

    case AVIF_PIXEL_FORMAT_YUV400:
    case AVIF_PIXEL_FORMAT_NONE:
    case AVIF_PIXEL_FORMAT_COUNT:
      if (depth == 8)
        {
          format = GDK_MEMORY_R8G8B8A8;
          bpp = 4;
        }
      else
        {
          format = GDK_MEMORY_R16G16B16A16;
          bpp = 8;
        }
    default:
      break;
    }

  if (format == GDK_MEMORY_N_FORMATS)
    {
      const char *names[] = { "none", "yuv444", "yuv422", "yuv420", "yuv400" };

      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   "unsupported pixel format for YUVA %s, depth %u",
                   names[decoder->image->yuvFormat], depth);
      return NULL;
    }

  stride = width * bpp;
  data = g_malloc (stride * height);

  if (depth == 8)
    {
      guchar *y_data = avifImagePlane (decoder->image, AVIF_CHAN_Y);
      guchar *u_data = avifImagePlane (decoder->image, AVIF_CHAN_U);
      guchar *v_data = avifImagePlane (decoder->image, AVIF_CHAN_V);
      guchar *a_data = avifImagePlane (decoder->image, AVIF_CHAN_A);
      guchar *dst_data = data;
      gsize y_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y);
      gsize u_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_U);
      gsize v_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_V);
      gsize a_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_A);
      gsize dst_stride = stride;

      for (int y = 0; y < height; y += Y_SUB)
        {
          for (int x = 0; x < width; x += X_SUB)
            {
              guint y_, u_, v_, a_;

              u_ = u_data[x / X_SUB];
              v_ = v_data[x / X_SUB];

              for (int ys = 0; ys < Y_SUB && y + ys < height; ys++)
                for (int xs = 0; xs < X_SUB && x + xs < width; xs++)
                  {
                    guchar *dest = &dst_data[4 * (x + xs) + dst_stride * ys];

                    y_ = y_data[x + xs + y_stride * ys];
                    a_ = a_data ? a_data[x + xs + a_stride * ys] : 0xff;

                    dest[0] = y_;
                    dest[1] = u_;
                    dest[2] = v_;
                    dest[3] = a_;
                  }
            }

          dst_data += Y_SUB * dst_stride;
          y_data += Y_SUB * y_stride;
          u_data += u_stride;
          v_data += v_stride;
          if (a_data)
            a_data += a_stride;
        }
    }
  else
    {
      guint16 *y_data = (guint16 *) avifImagePlane (decoder->image, AVIF_CHAN_Y);
      guint16 *u_data = (guint16 *) avifImagePlane (decoder->image, AVIF_CHAN_U);
      guint16 *v_data = (guint16 *) avifImagePlane (decoder->image, AVIF_CHAN_V);
      guint16 *a_data = (guint16 *) avifImagePlane (decoder->image, AVIF_CHAN_A);
      guint16 *dst_data = (guint16 *) data;
      gsize y_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_Y) / 2;
      gsize u_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_U) / 2;
      gsize v_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_V) / 2;
      gsize a_stride = avifImagePlaneRowBytes (decoder->image, AVIF_CHAN_A) / 2;
      gsize dst_stride = stride / 2;

      for (int y = 0; y < height; y += Y_SUB)
        {
          for (int x = 0; x < width; x += X_SUB)
            {
              guint y_, u_, v_, a_;

              u_ = u_data[x / X_SUB] << (16 - depth);
              v_ = v_data[x / X_SUB] << (16 - depth);

              for (int ys = 0; ys < Y_SUB && y + ys < height; ys++)
                for (int xs = 0; xs < X_SUB && x + xs < width; xs++)
                  {
                    guint16 *dest = &dst_data[4 * (x + xs) + dst_stride * ys];

                    y_ = y_data[x + xs + y_stride * ys] << (16 - depth);
                    a_ = (a_data ? a_data[x + xs + a_stride * ys] : 0xffff) << (16 - depth);

                    dest[0] = y_ | (y_ >> depth);
                    dest[1] = u_ | (u_ >> depth);
                    dest[2] = v_ | (v_ >> depth);
                    dest[3] = a_ | (a_ >> depth);
                  }
            }

          dst_data += Y_SUB * dst_stride;
          y_data += Y_SUB * y_stride;
          u_data += u_stride;
          v_data += v_stride;
        }
    }

  builder = gdk_memory_texture_builder_new ();

  bytes = g_bytes_new_take (data, stride * height);

  gdk_memory_texture_builder_set_width (builder, width);
  gdk_memory_texture_builder_set_height (builder, height);
  gdk_memory_texture_builder_set_bytes (builder, bytes);
  gdk_memory_texture_builder_set_stride (builder, stride);
  gdk_memory_texture_builder_set_format (builder, format);
  gdk_memory_texture_builder_set_color_state (builder, color_state);

  texture = gdk_memory_texture_builder_build (builder);

  g_bytes_unref (bytes);
  g_object_unref (builder);

  return texture;
}

/* }}} */
/* {{{ Public API */

GdkTexture *
gdk_load_avif (GBytes  *bytes,
               GError **error)
{
  avifDecoder *decoder;
  avifResult result;
  GdkCicpParams *params;
  GdkColorState *color_state = NULL;
  GdkTexture *texture = NULL;
  GError *local_error = NULL;

  decoder = avifDecoderCreate ();
  result = avifDecoderSetIOMemory (decoder,
                                   g_bytes_get_data (bytes, NULL),
                                   g_bytes_get_size (bytes));
  if (result != AVIF_RESULT_OK)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   "avifDecoderSetIOFile failed: %u", result);
      goto fail;
    }

  result = avifDecoderParse (decoder);
  if (result != AVIF_RESULT_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "avifDecoderParse failed: %u", result);
      goto fail;
    }

  result = avifDecoderNextImage (decoder);
  if (result != AVIF_RESULT_OK)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   "avifDecoderNextImage failed: %u", result);
      goto fail;
    }

  DEBUG ("load: depth %u", decoder->image->depth);

  DEBUG ("load: cicp %u/%u/%u/%u",
         decoder->image->colorPrimaries,
         decoder->image->transferCharacteristics,
         decoder->image->matrixCoefficients,
         decoder->image->yuvRange);

  params = gdk_cicp_params_new ();
  gdk_cicp_params_set_color_primaries (params, decoder->image->colorPrimaries);
  gdk_cicp_params_set_transfer_function (params, decoder->image->transferCharacteristics);
  gdk_cicp_params_set_matrix_coefficients (params, decoder->image->matrixCoefficients);
  gdk_cicp_params_set_range (params, decoder->image->yuvRange == AVIF_RANGE_LIMITED
                                     ? GDK_CICP_RANGE_NARROW
                                     : GDK_CICP_RANGE_FULL);

  color_state = gdk_cicp_params_build_color_state (params, error);
  g_object_unref (params);

  if (!color_state)
    goto fail;

#ifdef HAVE_DMABUF
  texture = gdk_avif_create_dmabuf_texture (decoder, color_state, &local_error);

  if (!texture)
    {
      DEBUG ("load: creating dmabuf failed: %s", local_error->message);
      g_clear_error (&local_error);
    }
#endif

  if (!texture)
    texture = gdk_avif_create_memory_texture (decoder, color_state, error);

  (gdk_color_state_unref) (color_state);

fail:
  avifDecoderDestroy (decoder);

  return texture;
}

static int
bytes_per_channel (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
      return 1;
    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
      return 2;
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return 4;
    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
    }
}

GBytes *
gdk_save_avif (GdkTexture *texture)
{
  uint32_t width, height, depth;
  GdkColorState *color_state;
  const GdkCicp *cicp;
  avifEncoder *encoder;
  avifImage *image = NULL;
  avifRWData output = AVIF_DATA_EMPTY;
  GdkTextureDownloader *downloader;
  GBytes *bytes = NULL;
  gsize stride;
  GBytes *out_bytes = NULL;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  if (bytes_per_channel (gdk_texture_get_format (texture)) == 1)
    depth = 8;
  else
    depth = 12;

  color_state = gdk_texture_get_color_state (texture);
  cicp = gdk_color_state_get_cicp (color_state);

  DEBUG ("save: depth %u", depth);

  DEBUG ("save: cicp %u/%u/%u/%u",
         cicp->color_primaries,
         cicp->transfer_function,
         cicp->matrix_coefficients,
         cicp->range);

  downloader = gdk_texture_downloader_new (texture);
  if (depth == 8)
    gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8);
  else
    gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R16G16B16A16);
  gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));

  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  gdk_texture_downloader_free (downloader);

  if (cicp->matrix_coefficients != 0)
    {
      /* some form of yuv */
      image = avifImageCreate (width, height, depth, AVIF_PIXEL_FORMAT_YUV444);
      image->colorPrimaries = cicp->color_primaries;
      image->transferCharacteristics = cicp->transfer_function;
      image->matrixCoefficients = cicp->matrix_coefficients;
      image->yuvRange = cicp->range == GDK_CICP_RANGE_NARROW
                        ? AVIF_RANGE_LIMITED
                        : AVIF_RANGE_FULL;

      avifImageAllocatePlanes (image, AVIF_PLANES_YUV);

      if (depth == 8)
        {
          for (gsize y = 0; y < height; y++)
            {
              const guchar *orig = (guchar *) g_bytes_get_data (bytes, NULL) + y * stride;
              guchar *y_ = avifImagePlane (image, AVIF_CHAN_Y) + y * avifImagePlaneRowBytes (image, AVIF_CHAN_Y);
              guchar *u_ = avifImagePlane (image, AVIF_CHAN_U) + y * avifImagePlaneRowBytes (image, AVIF_CHAN_U);
              guchar *v_ = avifImagePlane (image, AVIF_CHAN_V) + y * avifImagePlaneRowBytes (image, AVIF_CHAN_V);

              for (gsize x = 0; x < width; x++)
                {
                  y_[x] = orig[4*x + 0];
                  u_[x] = orig[4*x + 1];
                  v_[x] = orig[4*x + 2];
                }
            }
        }
      else
        {
          for (gsize y = 0; y < height; y++)
            {
              const guint16 *orig = (guint16 *) ((guchar *) g_bytes_get_data (bytes, NULL) + y * stride);
              guint16 *y_ = (guint16 *) (avifImagePlane (image, AVIF_CHAN_Y) + y * avifImagePlaneRowBytes (image, AVIF_CHAN_Y));
              guint16 *u_ = (guint16 *) (avifImagePlane (image, AVIF_CHAN_U) + y * avifImagePlaneRowBytes (image, AVIF_CHAN_U));
              guint16 *v_ = (guint16 *) (avifImagePlane (image, AVIF_CHAN_V) + y * avifImagePlaneRowBytes (image, AVIF_CHAN_V));

              for (gsize x = 0; x < width; x++)
                {
                  y_[x] = orig[4*x + 0] >> (16 - depth);
                  u_[x] = orig[4*x + 1] >> (16 - depth);
                  v_[x] = orig[4*x + 2] >> (16 - depth);
                }
            }
        }
    }
  else
    {
      avifRGBImage rgb;

      image = avifImageCreate (width, height, depth, AVIF_PIXEL_FORMAT_YUV444);
      image->colorPrimaries = cicp->color_primaries;
      image->transferCharacteristics = cicp->transfer_function;
      image->matrixCoefficients = cicp->matrix_coefficients;
      image->yuvRange = cicp->range == GDK_CICP_RANGE_NARROW
                        ? AVIF_RANGE_LIMITED
                        : AVIF_RANGE_FULL;

      avifRGBImageSetDefaults (&rgb, image);
      avifRGBImageAllocatePixels (&rgb);

      if (depth == 8)
        {
          for (gsize y = 0; y < height; y++)
            {
              const guchar *orig = (guchar *) g_bytes_get_data (bytes, NULL) + y * stride;
              guchar *pixels = rgb.pixels + y * rgb.rowBytes;
              for (gsize x = 0; x < width; x++)
                {
                  pixels[4*x + 0] = orig[4*x + 0];
                  pixels[4*x + 1] = orig[4*x + 1];
                  pixels[4*x + 2] = orig[4*x + 2];
                  pixels[4*x + 3] = orig[4*x + 3];
                }
            }
        }
      else
        {
          for (gsize y = 0; y < height; y++)
            {
              const guint16 *orig = (const guint16 *) ((guchar *) g_bytes_get_data (bytes, NULL) + y * stride);
              guint16 *pixels = (guint16 *) (rgb.pixels + y * rgb.rowBytes);
              for (gsize x = 0; x < width; x++)
                {
                  pixels[4*x + 0] = orig[4*x + 0] >> (16 - depth);
                  pixels[4*x + 1] = orig[4*x + 1] >> (16 - depth);
                  pixels[4*x + 2] = orig[4*x + 2] >> (16 - depth);
                  pixels[4*x + 3] = orig[4*x + 3] >> (16 - depth);
                }
            }
        }

      avifImageRGBToYUV(image, &rgb);
      avifRGBImageFreePixels (&rgb);
    }

  DEBUG ("save: cicp in image %u/%u/%u/%u",
         image->colorPrimaries,
         image->transferCharacteristics,
         image->matrixCoefficients,
         image->yuvRange);

  encoder = avifEncoderCreate ();

  if (avifEncoderWrite (encoder, image, &output) != AVIF_RESULT_OK)
    goto fail;

  out_bytes = g_bytes_new_take (output.data, output.size);

fail:
  g_clear_pointer (&bytes, g_bytes_unref);
  avifEncoderDestroy (encoder);
  if (image)
    avifImageDestroy (image);

  return out_bytes;
}

gboolean
gdk_is_avif (GBytes *bytes)
{
  avifROData input;

  input.data = g_bytes_get_data (bytes, &input.size);

  return avifPeekCompatibleFileType (&input);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
