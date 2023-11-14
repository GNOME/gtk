#include <gtk/gtk.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/dma-heap.h>
#include <drm_fourcc.h>

/* For this to work, you may need to give /dev/dma_heap/system
 * lax permissions.
 */

static int dma_heap_fd = -1;

static gboolean
initialize_dma_heap (void)
{
  dma_heap_fd = open ("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
  return dma_heap_fd != -1;
}

static int
allocate_dma_buf (gsize size)
{
  struct dma_heap_allocation_data heap_data;
  int ret;

  heap_data.len = size;
  heap_data.fd = 0;
  heap_data.fd_flags = O_RDWR | O_CLOEXEC;
  heap_data.heap_flags = 0;

  ret = ioctl (dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
  if (ret)
    g_error ("dma-buf allocation failed");

  return heap_data.fd;
}

static int
allocate_memfd (gsize size)
{
  int fd;

  fd = memfd_create ("buffer", MFD_CLOEXEC);
  if (fd == -1)
    g_error ("memfd allocation failed");

  ftruncate (fd, size);

  return fd;
}

static int
allocate_buffer (gsize size)
{
  if (dma_heap_fd != -1)
    return allocate_dma_buf (size);
  else
    return allocate_memfd (size);
}

static void
populate_buffer (int           fd,
                 const guchar *data,
                 gsize         size)
{
  guchar *buf;

  buf = mmap (NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
  memcpy (buf, data, size);
  munmap (buf, size);
}


/* The YUV conversion code is adapted from weston/tests/yuv-buffer-test.c */

/*
 * Based on Rec. ITU-R BT.601-7
 *
 * This is intended to be obvious and accurate, not fast.
 */
static void
x8r8g8b8_to_ycbcr8_bt601 (guint32 xrgb,
                          guchar *y_out,
                          guchar *cb_out,
                          guchar *cr_out)
{
  double y, cb, cr;
  double r = (xrgb >> 16) & 0xff;
  double g = (xrgb >> 8) & 0xff;
  double b = (xrgb >> 0) & 0xff;

  /* normalize to [0.0, 1.0] */
  r /= 255.0;
  g /= 255.0;
  b /= 255.0;

  /* Y normalized to [0.0, 1.0], Cb and Cr [-0.5, 0.5] */
  y = 0.299 * r + 0.587 * g + 0.114 * b;
  cr = (r - y) / 1.402;
  cb = (b - y) / 1.772;

  /* limited range quantization to 8 bit */
  *y_out = round(219.0 * y + 16.0);
  if (cr_out)
    *cr_out = round(224.0 * cr + 128.0);
  if (cb_out)
    *cb_out = round(224.0 * cb + 128.0);
}

/*
 * 3 plane YCbCr
 * plane 0: Y plane, [7:0] Y
 * plane 1: Cb plane, [7:0] Cb
 * plane 2: Cr plane, [7:0] Cr
 * YUV420: 2x2 subsampled Cb (1) and Cr (2) planes
 * YUV444: no subsampling
 */
static guchar *
y_u_v_create_buffer (uint32_t  drm_format,
                     guchar   *rgb_data,
                     int       rgb_width,
                     int       rgb_height,
                     int      *size,
                     int      *u_offset,
                     int      *v_offset)
{
  gsize bytes;
  int x, y;
  guint32 *rgb_row;
  guchar *y_base;
  guchar *u_base;
  guchar *v_base;
  guchar *y_row;
  guchar *u_row;
  guchar *v_row;
  guint32 argb;
  int sub = (drm_format == DRM_FORMAT_YUV420) ? 2 : 1;
  guchar *buf;

  g_assert (drm_format == DRM_FORMAT_YUV420 ||
            drm_format == DRM_FORMAT_YUV444);

  /* Full size Y plus quarter U and V */
  bytes = rgb_width * rgb_height +
          (rgb_width / sub) * (rgb_height / sub) * 2;

  buf = g_new (guchar, bytes);

  *size = bytes;
  *u_offset = rgb_width * rgb_height;
  *v_offset = *u_offset + (rgb_width / sub) * (rgb_height / sub);

  y_base = buf;
  u_base = y_base + rgb_width * rgb_height;
  v_base = u_base + (rgb_width / sub) * (rgb_height / sub);

  for (y = 0; y < rgb_height; y++)
    {
      rgb_row = (guint32 *) (rgb_data + y * 4 * rgb_width);
      y_row = y_base + y * rgb_width;
      u_row = u_base + (y / sub) * (rgb_width / sub);
      v_row = v_base + (y / sub) * (rgb_width / sub);

      for (x = 0; x < rgb_width; x++)
        {
          /*
           * Sub-sample the source image instead, so that U and V
           * sub-sampling does not require proper
           * filtering/averaging/siting.
           */
          argb = rgb_row[x];

          /*
           * A stupid way of "sub-sampling" chroma. This does not
           * do the necessary filtering/averaging/siting or
           * alternate Cb/Cr rows.
           */
          if ((y & (sub - 1)) == 0 && (x & (sub - 1)) == 0)
            x8r8g8b8_to_ycbcr8_bt601 (argb, y_row + x, u_row + x / sub, v_row + x / sub);
          else
            x8r8g8b8_to_ycbcr8_bt601 (argb, y_row + x, NULL, NULL);
        }
    }

  return buf;
}

/*
 * 2 plane YCbCr
 * plane 0 = Y plane, [7:0] Y
 * plane 1 = Cr:Cb plane, [15:0] Cr:Cb little endian
 * 2x2 subsampled Cr:Cb plane
 */
static guchar *
nv12_create_buffer (uint32_t drm_format,
                    guchar   *rgb_data,
                    int       rgb_width,
                    int       rgb_height,
                    int      *size,
                    int      *uv_offset)
{
  size_t bytes;
  int x, y;
  uint32_t *rgb_row;
  uint8_t *y_base;
  uint8_t *uv_base;
  uint8_t *y_row;
  uint16_t *uv_row;
  uint32_t argb;
  uint8_t cr;
  uint8_t cb;
  guchar *buf;

  g_assert (drm_format == DRM_FORMAT_NV12);

  /* Full size Y plane, half size UV plane */
  bytes = rgb_width * rgb_height +
          rgb_width * (rgb_height / 2);
  *size = bytes;

  buf = g_new0 (guchar, bytes);

  *uv_offset = rgb_width * rgb_height;
  y_base = buf;
  uv_base = y_base + rgb_width * rgb_height;

  for (y = 0; y < rgb_height; y++)
    {
      rgb_row = (uint32_t *) (rgb_data + y * 4 * rgb_width);
      y_row = y_base + y * rgb_width;
      uv_row = (uint16_t *) (uv_base + (y / 2) * rgb_width);

      for (x = 0; x < rgb_width; x++)
        {
          /*
           * Sub-sample the source image instead, so that U and V
           * sub-sampling does not require proper
           * filtering/averaging/siting.
           */
          argb = rgb_row[x];

          /*
           * A stupid way of "sub-sampling" chroma. This does not
           * do the necessary filtering/averaging/siting.
           */
          if ((y & 1) == 0 && (x & 1) == 0)
            {
              x8r8g8b8_to_ycbcr8_bt601(argb, y_row + x, &cb, &cr);
              *(uv_row + x / 2) = ((uint16_t)cr << 8) | cb;
            }
          else
            {
              x8r8g8b8_to_ycbcr8_bt601(argb, y_row + x, NULL, NULL);
            }
        }
    }

  return buf;
}

static void
texture_builder_set_planes (GdkDmabufTextureBuilder *builder,
                            gboolean                 disjoint,
                            const guchar            *buf,
                            unsigned                 size,
                            unsigned                 n_planes,
                            unsigned                 strides[4],
                            unsigned                 sizes[4])
{
  gdk_dmabuf_texture_builder_set_n_planes (builder, n_planes);

  if (disjoint)
    {
      unsigned offset = 0;
      unsigned i;

      for (i = 0; i < n_planes; i++)
        {
          int fd = allocate_buffer (sizes[i]);
          populate_buffer (fd, buf + offset, sizes[i]);

          gdk_dmabuf_texture_builder_set_fd (builder, i, fd);
          gdk_dmabuf_texture_builder_set_stride (builder, i, strides[i]);
          gdk_dmabuf_texture_builder_set_offset (builder, i, 0);
          offset += sizes[i];
        }
    }
  else
    {
      unsigned offset = 0;
      unsigned i;
      int fd = allocate_buffer (size);
      populate_buffer (fd, buf, size);

      for (i = 0; i < n_planes; i++)
        {
          gdk_dmabuf_texture_builder_set_fd (builder, i, fd);
          gdk_dmabuf_texture_builder_set_stride (builder, i, strides[i]);
          gdk_dmabuf_texture_builder_set_offset (builder, i, offset);
          offset += sizes[i];
        }
    }
}

static GdkTexture *
make_dmabuf_texture (const char *filename,
                     guint32     format,
                     gboolean    disjoint)
{
  GdkTexture *texture;
  int width, height;
  gsize rgb_stride, rgb_size;
  guchar *rgb_data;
  int fd;
  GdkDmabufTextureBuilder *builder;
  GError *error = NULL;

  if (initialize_dma_heap ())
    g_print ("Using dma_heap\n");
  else
    g_print ("Using memfd\n");

  texture = gdk_texture_new_from_filename (filename, NULL);

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  rgb_stride = 4 * width;
  rgb_size = rgb_stride * height;

  rgb_data = g_new0 (guchar, rgb_size);

  gdk_texture_download (texture, rgb_data, rgb_stride);

  g_object_unref (texture);

  builder = gdk_dmabuf_texture_builder_new ();

  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_fourcc (builder, format);
  gdk_dmabuf_texture_builder_set_modifier (builder, DRM_FORMAT_MOD_LINEAR);

  if (format == DRM_FORMAT_XRGB8888 ||
      format == DRM_FORMAT_ARGB8888)
    {
      gdk_dmabuf_texture_builder_set_n_planes (builder, 1);

      fd = allocate_buffer (rgb_size);
      populate_buffer (fd, rgb_data, rgb_size);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, rgb_stride);
    }
  else if (format == DRM_FORMAT_YUV420)
    {
      guchar *buf;
      int size, u_offset, v_offset;

      buf = y_u_v_create_buffer (format, rgb_data, width, height, &size, &u_offset, &v_offset);

      texture_builder_set_planes (builder,
                                  disjoint,
                                  buf,
                                  size,
                                  3,
                                  (unsigned[4]) { width, width / 2, width / 2 }, 
                                  (unsigned[4]) { width * height, width * height / 4, width * height / 4 });

      g_free (buf);
    }
  else if (format == DRM_FORMAT_NV12)
    {
      guchar *buf;
      int size, uv_offset;

      buf = nv12_create_buffer (format, rgb_data, width, height, &size, &uv_offset);

      texture_builder_set_planes (builder,
                                  disjoint,
                                  buf,
                                  size,
                                  2,
                                  (unsigned[4]) { width, width, }, 
                                  (unsigned[4]) { width * height, width * height / 2 });

      g_free (buf);
    }

  g_free (rgb_data);

  texture = gdk_dmabuf_texture_builder_build (builder, NULL, NULL, &error);
  if (!texture)
    g_error ("Failed to create dmabuf texture: %s", error->message);

  g_object_unref (builder);

  return texture;
}

static guint32 supported_formats[] = {
  DRM_FORMAT_ARGB8888,
  DRM_FORMAT_XRGB8888,
  DRM_FORMAT_YUV420,
  DRM_FORMAT_NV12,
};

static gboolean
format_is_supported (guint32 fmt)
{
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (supported_formats[i] == fmt)
        return TRUE;
    }

  return FALSE;
}

static char *
supported_formats_to_string (void)
{
  GString *s;

  s = g_string_new ("");
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (s->len)
        g_string_append (s, ", ");
      g_string_append_printf (s, "%.4s", (char *)&supported_formats[i]);
    }

  return g_string_free (s, FALSE);
}

G_GNUC_NORETURN
static void
usage (void)
{
  char *formats = supported_formats_to_string ();
  g_print ("Usage: testdmabuf [--undecorated] [--disjoint][--download-to FILE] FORMAT FILE\n"
           "Supported formats: %s\n", formats);
  g_free (formats);
  exit (1);
}

static guint32
parse_format (const char *a)
{
  if (strlen (a) == 4)
    {
      guint32 format = fourcc_code (a[0], a[1], a[2], a[3]);
      if (format_is_supported (format))
        return format;
    }

  usage ();
  return 0;
}

int
main (int argc, char *argv[])
{
  GdkTexture *texture;
  GtkWidget *window, *offload, *picture;
  char *filename;
  guint32 format;
  gboolean disjoint = FALSE;
  gboolean decorated = TRUE;
  unsigned int i;
  const char *save_filename = NULL;

  for (i = 1; i < argc; i++)
    {
      if (g_str_equal (argv[i], "--disjoint"))
        disjoint = TRUE;
      else if (g_str_equal (argv[i], "--undecorated"))
        decorated = FALSE;
      else if (g_str_equal (argv[i], "--download-to"))
        {
          i++;
          if (i == argc)
            usage ();

          save_filename = argv[i];
        }
      else
        break;
    }

  if (argc - i != 2)
    {
      usage ();
      return 1;
    }

  format = parse_format (argv[argc - 2]);
  filename = argv[argc - 1];

  gtk_init ();

  /* Get the list of supported formats with GDK_DEBUG=opengl */
  gdk_display_get_dmabuf_formats (gdk_display_get_default ());

  texture = make_dmabuf_texture (filename, format, disjoint);

  if (save_filename)
    gdk_texture_save_to_png (texture, save_filename);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), decorated);

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));

  offload = gtk_graphics_offload_new (picture);

  gtk_window_set_child (GTK_WINDOW (window), offload);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
