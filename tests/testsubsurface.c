#include <gtk/gtk.h>
#include "gtk/gtkwidgetprivate.h"
#include "gdk/gdksurfaceprivate.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/dma-heap.h>
#include <drm_fourcc.h>

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

static void
free_dmabuf (gpointer data)
{
  close (GPOINTER_TO_INT (data));
}

static GdkTexture *
make_dmabuf_color_texture (int width,
                           int height,
                           GdkRGBA *color)
{
  int fd;
  guchar *buf;
  GdkDmabufTextureBuilder *builder;
  GdkTexture *texture;
  gsize stride, size;
  GError *error = NULL;

  stride = width * 4;
  size = height * stride;
  fd = allocate_dma_buf (size);

  buf = mmap (NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);

  for (gsize i = 0; i < width * height * 4; i += 4)
    {
      buf[i]     = 255 * color->blue;
      buf[i + 1] = 255 * color->green;
      buf[i + 2] = 255 * color->red;
      buf[i + 3] = 255 * color->alpha;
    }

  munmap (buf, size);

  builder = gdk_dmabuf_texture_builder_new ();
  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_fourcc (builder, DRM_FORMAT_ARGB8888);
  gdk_dmabuf_texture_builder_set_modifier (builder, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_texture_builder_set_n_planes (builder, 1);
  gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
  gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);
  gdk_dmabuf_texture_builder_set_stride (builder, 0, stride);

  texture = gdk_dmabuf_texture_builder_build (builder, free_dmabuf, GINT_TO_POINTER (fd), &error);
  if (texture == NULL)
    g_error ("%s", error->message);

  g_object_unref (builder);

  return texture;
}

static GdkTexture *
make_shm_color_texture (int width,
                        int height,
                        GdkRGBA *color)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  guchar *data;
  gsize stride;
  GBytes *bytes;
  GdkTexture *texture;

  stride = 4 * width;
  data = g_new (guchar, stride * height);

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height,
                                                 stride);
  cr = cairo_create (surface);
  gdk_cairo_set_source_rgba (cr, color);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  bytes = g_bytes_new_take (data, stride * height);
  texture = gdk_memory_texture_new (width, height, GDK_MEMORY_DEFAULT, bytes, stride);
  g_bytes_unref (bytes);

  return texture;
}

static GdkTexture *
make_color_texture (int width,
                    int height,
                    GdkRGBA *color)
{
  if (dma_heap_fd != -1)
    return make_dmabuf_color_texture (width, height, color);
  else
    return make_shm_color_texture (width, height, color);
}

static GdkSubsurface *
add_subsurface (GtkWidget *window,
                GdkRGBA *color,
                const graphene_rect_t *rect)
{
  GdkSurface *surface;
  GdkSubsurface *subsurface;
  GdkTexture *texture;

  surface = gtk_widget_get_surface (GTK_WIDGET (window));

  subsurface = gdk_surface_create_subsurface (surface);

  texture = make_color_texture (20, 20, color);

  gdk_subsurface_attach (subsurface, texture, rect);

  g_object_unref (texture);

  return subsurface;
}

static GdkSubsurface *red, *blue;

static void red_above_blue (GtkButton *button) { gdk_subsurface_place_above (red, blue); }
static void red_below_blue (GtkButton *button) { gdk_subsurface_place_below (red, blue); }
static void blue_above_red (GtkButton *button) { gdk_subsurface_place_above (blue, red); }
static void blue_below_red (GtkButton *button) { gdk_subsurface_place_below (blue, red); }
static void red_above_main (GtkButton *button) { gdk_subsurface_place_above (red, NULL); }
static void red_below_main (GtkButton *button) { gdk_subsurface_place_below (red, NULL); }
static void blue_above_main (GtkButton *button) { gdk_subsurface_place_above (blue, NULL); }
static void blue_below_main (GtkButton *button) { gdk_subsurface_place_below (blue, NULL); }

static GtkWidget *
make_button (const char *name, gpointer cb)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (name);
  g_signal_connect (button, "clicked", G_CALLBACK (cb), NULL);

  return button;
}

static void
change_colors (GtkButton *button)
{
  GdkRGBA color;
  GdkTexture *texture;

  color.red = g_random_double_range (0.5, 1);
  color.green = g_random_double_range (0, 0.5);
  color.blue = g_random_double_range (0, 0.5);
  color.alpha = 1;

  texture = make_color_texture (20, 20, &color);
  gdk_subsurface_attach (red, texture, &GRAPHENE_RECT_INIT (200, 100, 50, 50));
  g_object_unref (texture);

  color.red = g_random_double_range (0, 0.5);
  color.green = g_random_double_range (0, 0.5);
  color.blue = g_random_double_range (0.5, 1);
  color.alpha = 1;

  texture = make_color_texture (20, 20, &color);
  gdk_subsurface_attach (blue, texture, &GRAPHENE_RECT_INIT (180, 120, 100, 20));
  g_object_unref (texture);
}

static GtkWidget *
make_buttons (void)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (box), make_button ("Red above blue", red_above_blue));
  gtk_box_append (GTK_BOX (box), make_button ("Red below blue", red_below_blue));
  gtk_box_append (GTK_BOX (box), make_button ("Red above main", red_above_main));
  gtk_box_append (GTK_BOX (box), make_button ("Red below main", red_below_main));
  gtk_box_append (GTK_BOX (box), make_button ("Blue above red", blue_above_red));
  gtk_box_append (GTK_BOX (box), make_button ("Blue below red", blue_below_red));
  gtk_box_append (GTK_BOX (box), make_button ("Blue above main", blue_above_main));
  gtk_box_append (GTK_BOX (box), make_button ("Blue below main", blue_below_main));
  gtk_box_append (GTK_BOX (box), make_button ("Change colors", change_colors));

  return box;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 210, -1);
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

  gtk_widget_realize (window);

  if (initialize_dma_heap ())
    g_print ("Using dambufs\n");
  else
    g_print ("Failed to initialize dma-heap, using shm\n");

  red = add_subsurface (window, &(GdkRGBA) { 1, 0, 0, 1 }, &GRAPHENE_RECT_INIT (200, 100, 50, 50));
  blue = add_subsurface (window, &(GdkRGBA) { 0, 0, 1, 1 }, &GRAPHENE_RECT_INIT (180, 120, 100, 20));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_box_append (GTK_BOX (box), make_buttons ());

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
