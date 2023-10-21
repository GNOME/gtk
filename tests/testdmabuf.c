#include <gtk/gtk.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-heap.h>
#include <drm/drm_fourcc.h>

/* For this to work, you may need to give /dev/dma_heap/system
 * lax permissions.
 */

static int
allocate_dma_buf (gsize size)
{
  static int fd = -1;
  struct dma_heap_allocation_data heap_data;
  int ret;

  if (fd == -1)
    {
      fd = open ("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
      if (fd == -1)
        g_error ("Failed to open /dev/dma_heap/system");
    }

  heap_data.len = size;
  heap_data.fd = 0;
  heap_data.fd_flags = O_RDWR | O_CLOEXEC;
  heap_data.heap_flags = 0;

  ret = ioctl (fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
  if (ret)
    g_error ("dma-buf allocation failed");

  return heap_data.fd;
}

static void
populate_dma_buf (int     fd,
                  guchar *data,
                  gsize   size)
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
  uint16_t *uv_base;
  uint8_t *y_row;
  uint16_t *uv_row;
  uint32_t argb;
  uint8_t cr;
  uint8_t cb;
  guchar *buf;

  g_assert (drm_format == DRM_FORMAT_NV12);

  /* Full size Y, quarter UV */
  bytes = rgb_width * rgb_height +
          (rgb_width / 2) * (rgb_height / 2) * sizeof (uint16_t);

  buf = g_new0 (guchar, bytes);

  *uv_offset = rgb_width * rgb_height;
  y_base = buf;
  uv_base = (uint16_t *)(y_base + rgb_width * rgb_height);

  for (y = 0; y < rgb_height; y++)
    {
      rgb_row = (uint32_t *) (rgb_data + y * 4 * rgb_width);
      y_row = y_base + y * rgb_width;
      uv_row = (uint16_t *) (uv_base + (y / 2) * (rgb_width / 2));

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

static GdkTexture *
make_dmabuf_texture (const char *filename,
                     guint32     format)
{
  GdkTexture *texture;
  int width, height;
  gsize rgb_stride, rgb_size;
  guchar *rgb_data;
  int fd;
  GdkDmabufTextureBuilder *builder;
  GError *error = NULL;

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

      fd = allocate_dma_buf (rgb_size);
      populate_dma_buf (fd, rgb_data, rgb_size);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, rgb_stride);
    }
  else if (format == DRM_FORMAT_YUV420)
    {
      guchar *buf;
      int size, u_offset, v_offset;

      gdk_dmabuf_texture_builder_set_n_planes (builder, 3);

      buf = y_u_v_create_buffer (format, rgb_data, width, height, &size, &u_offset, &v_offset);

      fd = allocate_dma_buf (width * height);
      populate_dma_buf (fd, buf, width * height);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, width);
      gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);

      fd = allocate_dma_buf ((width / 2) * (height / 2));
      populate_dma_buf (fd, buf + u_offset, (width / 2) * (height / 2));

      gdk_dmabuf_texture_builder_set_fd (builder, 1, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 1, width / 2);
      gdk_dmabuf_texture_builder_set_offset (builder, 1, 0);

      fd = allocate_dma_buf ((width / 2) * (height / 2));
      populate_dma_buf (fd, buf + v_offset, (width / 2) * (height / 2));

      gdk_dmabuf_texture_builder_set_fd (builder, 2, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 2, width / 2);
      gdk_dmabuf_texture_builder_set_offset (builder, 2, 0);

      g_free (buf);
    }
  else if (format == DRM_FORMAT_NV12)
    {
      guchar *buf;
      int size, uv_offset;

      gdk_dmabuf_texture_builder_set_n_planes (builder, 2);

      buf = nv12_create_buffer (format, rgb_data, width, height, &size, &uv_offset);

      fd = allocate_dma_buf (width * height);
      populate_dma_buf (fd, buf, width * height);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, width);
      gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);

      fd = allocate_dma_buf ((width / 2) * (height / 2) * sizeof (uint16_t));
      populate_dma_buf (fd, buf + uv_offset, (width / 2) * (height / 2) * sizeof (uint16_t));

      gdk_dmabuf_texture_builder_set_fd (builder, 1, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 1, width);
      gdk_dmabuf_texture_builder_set_offset (builder, 1, 0);

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
  g_print ("Usage: testdmabuf FORMAT FILE\n"
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
  GtkWidget *window, *picture;
  char *filename;
  guint32 format;

  if (argc != 3)
    usage ();

  format = parse_format (argv[1]);
  filename = argv[2];

  gtk_init ();

  /* Get the list of supported formats with GDK_DEBUG=opengl */
  gdk_display_get_dmabuf_formats (gdk_display_get_default ());

  texture = make_dmabuf_texture (filename, format);

  gdk_texture_save_to_png (texture, "testdmabuf.out.png");

  window = gtk_window_new ();

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));

  gtk_window_set_child (GTK_WINDOW (window), picture);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
