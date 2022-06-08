/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include <gtk/gtk.h>
#include <gdk/wayland/gdkwayland.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pixman.h>
#include <drm/drm_fourcc.h>

static struct wl_compositor *wl_compositor;
static struct wl_subcompositor *wl_subcompositor;
static struct wl_shm *wl_shm;

struct yuv_buffer {
        void *data;
        size_t bytes;
        struct wl_buffer *proxy;
        int width;
        int height;
};

static struct yuv_buffer *
yuv_buffer_create(size_t bytes,
                  int width,
                  int height,
                  int stride_bytes,
                  uint32_t drm_format)
{
        struct wl_shm_pool *pool;
        struct yuv_buffer *buf;
        int fd;
        const char *xdg_runtime_dir;

        buf = g_malloc(sizeof *buf);
        buf->bytes = bytes;
        buf->width = width;
        buf->height = height;

        xdg_runtime_dir = getenv ("XDG_RUNTIME_DIR");
        fd = open (xdg_runtime_dir, O_TMPFILE|O_RDWR|O_EXCL, 0600);
        ftruncate (fd, buf->bytes);
        g_assert(fd >= 0);

        buf->data = mmap(NULL, buf->bytes,
                         PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (buf->data == MAP_FAILED) {
                close(fd);
                g_assert(buf->data != MAP_FAILED);
        }

        pool = wl_shm_create_pool(wl_shm, fd, buf->bytes);
        buf->proxy = wl_shm_pool_create_buffer(pool, 0, buf->width, buf->height,
                                               stride_bytes, drm_format);
        wl_shm_pool_destroy(pool);
        close(fd);

        return buf;
}

/*
 * Based on Rec. ITU-R BT.601-7
 *
 * This is intended to be obvious and accurate, not fast.
 */
static void
x8r8g8b8_to_ycbcr8_bt601(uint32_t xrgb,
                         uint8_t *y_out, uint8_t *cb_out, uint8_t *cr_out)
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

static struct yuv_buffer *
yuv420_create_buffer(pixman_image_t *rgb_image)
{
        struct yuv_buffer *buf;
        size_t bytes;
        int width;
        int height;
        int x, y;
        void *rgb_pixels;
        int rgb_stride_bytes;
        uint32_t *rgb_row;
        uint8_t *y_base;
        uint8_t *u_base;
        uint8_t *v_base;
        uint8_t *y_row;
        uint8_t *u_row;
        uint8_t *v_row;
        uint32_t argb;
        uint32_t drm_format = DRM_FORMAT_YUV420;

        g_assert(drm_format == DRM_FORMAT_YUV420);

        width = pixman_image_get_width(rgb_image);
        height = pixman_image_get_height(rgb_image);
        rgb_pixels = pixman_image_get_data(rgb_image);
        rgb_stride_bytes = pixman_image_get_stride(rgb_image);

        /* Full size Y, quarter U and V */
        bytes = width * height + (width / 2) * (height / 2) * 2;
        buf = yuv_buffer_create(bytes, width, height, width, drm_format);

        y_base = buf->data;
        u_base = y_base + width * height;
        v_base = u_base + (width / 2) * (height / 2);

        for (y = 0; y < height; y++) {
                rgb_row = rgb_pixels + (y / 2 * 2) * rgb_stride_bytes;
                y_row = y_base + y * width;
                u_row = u_base + (y / 2) * (width / 2);
                v_row = v_base + (y / 2) * (width / 2);

                for (x = 0; x < width; x++) {
                        /*
                         * Sub-sample the source image instead, so that U and V
                         * sub-sampling does not require proper
                         * filtering/averaging/siting.
                         */
                        argb = *(rgb_row + x / 2 * 2);

                        /*
                         * A stupid way of "sub-sampling" chroma. This does not
                         * do the necessary filtering/averaging/siting or
                         * alternate Cb/Cr rows.
                         */
                        if ((y & 1) == 0 && (x & 1) == 0) {
                                x8r8g8b8_to_ycbcr8_bt601(argb, y_row + x,
                                                         u_row + x / 2,
                                                         v_row + x / 2);
                        } else {
                                x8r8g8b8_to_ycbcr8_bt601(argb, y_row + x,
                                                         NULL, NULL);
                        }
                }
        }

        return buf;
}

static void
destroy_cairo_surface(pixman_image_t *image, void *data)
{
        cairo_surface_t *surface = data;

        cairo_surface_destroy(surface);
}

struct format_map_entry {
        cairo_format_t cairo;
        pixman_format_code_t pixman;
};

static const struct format_map_entry format_map[] = {
        { CAIRO_FORMAT_ARGB32,    PIXMAN_a8r8g8b8 },
        { CAIRO_FORMAT_RGB24,     PIXMAN_x8r8g8b8 },
        { CAIRO_FORMAT_A8,        PIXMAN_a8 },
        { CAIRO_FORMAT_RGB16_565, PIXMAN_r5g6b5 },
};

static pixman_format_code_t
format_cairo2pixman(cairo_format_t fmt)
{
        unsigned i;

        for (i = 0; i < G_N_ELEMENTS(format_map); i++)
                if (format_map[i].cairo == fmt)
                        return format_map[i].pixman;

        g_assert_not_reached ();
}

static pixman_image_t *
image_convert_to_a8r8g8b8(pixman_image_t *image)
{
        pixman_image_t *ret;
        int width;
        int height;

        if (pixman_image_get_format(image) == PIXMAN_a8r8g8b8)
                return pixman_image_ref(image);

        width = pixman_image_get_width(image);
        height = pixman_image_get_height(image);

        ret = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, width, height,
                                                NULL, 0);
        g_assert(ret);

        pixman_image_composite32(PIXMAN_OP_SRC, image, NULL, ret,
                                 0, 0, 0, 0, 0, 0, width, height);

        return ret;
}

static pixman_image_t *
load_image_from_png(const char *fname)
{
        pixman_image_t *image;
        pixman_image_t *converted;
        cairo_format_t cairo_fmt;
        pixman_format_code_t pixman_fmt;
        cairo_surface_t *reference_cairo_surface;
        cairo_status_t status;
        int width;
        int height;
        int stride;
        void *data;

        reference_cairo_surface = cairo_image_surface_create_from_png(fname);
        cairo_surface_flush(reference_cairo_surface);
        status = cairo_surface_status(reference_cairo_surface);
        if (status != CAIRO_STATUS_SUCCESS) {
                g_error ("Could not open %s: %s\n", fname,
                        cairo_status_to_string(status));
                cairo_surface_destroy(reference_cairo_surface);
                return NULL;
        }

        cairo_fmt = cairo_image_surface_get_format(reference_cairo_surface);
        pixman_fmt = format_cairo2pixman(cairo_fmt);

        width = cairo_image_surface_get_width(reference_cairo_surface);
        height = cairo_image_surface_get_height(reference_cairo_surface);
        stride = cairo_image_surface_get_stride(reference_cairo_surface);
        data = cairo_image_surface_get_data(reference_cairo_surface);

        /* The Cairo surface will own the data, so we keep it around. */
        image = pixman_image_create_bits_no_clear(pixman_fmt,
                                                  width, height, data, stride);
        g_assert(image);

        pixman_image_set_destroy_function(image, destroy_cairo_surface,
                                          reference_cairo_surface);

        converted = image_convert_to_a8r8g8b8(image);
        pixman_image_unref(image);

        return converted;
}

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  //g_print ("global: %s %u, id %u\n", interface, version, id);
  if (strcmp (interface, "wl_compositor") == 0)
    wl_compositor = wl_registry_bind (registry, id, &wl_compositor_interface, version);

  if (strcmp (interface, "wl_subcompositor") == 0)
    wl_subcompositor = wl_registry_bind (registry, id, &wl_subcompositor_interface, version);

  if (strcmp (interface, "wl_shm") == 0)
    wl_shm = wl_registry_bind (registry, id, &wl_shm_interface, version);
}

static void
gdk_registry_handle_global_remove (void               *data,
                                   struct wl_registry *registry,
                                   uint32_t            id)
{
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

static void
set_up_registry (struct wl_display *wl_display)
{
  struct wl_registry *wl_registry;

  wl_registry = wl_display_get_registry (wl_display);
  wl_registry_add_listener (wl_registry, &registry_listener, NULL);
  if (wl_display_roundtrip (wl_display) < 0)
    exit (1);

  g_assert (wl_compositor != NULL);
  g_assert (wl_subcompositor != NULL);
  g_assert (wl_shm != NULL);
}

static void
create_subsurface (struct wl_surface     *parent,
                   struct wl_surface    **child,
                   struct wl_subsurface **subsurface)
{
  struct wl_region *region;

  *child = wl_compositor_create_surface (wl_compositor);
  *subsurface = wl_subcompositor_get_subsurface (wl_subcompositor, *child, parent);
  wl_subsurface_set_desync (*subsurface);

  region = wl_compositor_create_region (wl_compositor);
  wl_surface_set_input_region (*child, region);
  wl_region_destroy (region);
}

static void
surface_fill (struct wl_surface *surface,
              int                width,
              int                height,
              const GdkRGBA     *c)
{
  size_t size;
  const char *xdg_runtime_dir;
  int fd;
  guint32 *data;
  guint32 pixel;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;

#if 0
  size = width * height * 4;
  xdg_runtime_dir = getenv ("XDG_RUNTIME_DIR");
  fd = open (xdg_runtime_dir, O_TMPFILE|O_RDWR|O_EXCL, 0600);
  ftruncate (fd, size);
  data = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  pixel = 0xff000000 | ((int)(c->red * 255) << 16) | ((int)(c->green * 255) << 8) | ((int)c->blue * 255);

  for (int i = 0; i < height; i++)
    for (int j = 0; j < width; j++)
      data[i * width + j] = pixel;

  pool = wl_shm_create_pool (wl_shm, fd, size);
  buffer = wl_shm_pool_create_buffer (pool, 0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy (pool);

  close (fd);
#else
  pixman_image_t *img = load_image_from_png ("tests/chocolate-cake.png");
  struct yuv_buffer *buf = yuv420_create_buffer (img);
  buffer = buf->proxy;
#endif

  wl_surface_attach (surface, buffer, 0, 0);
  wl_surface_commit (surface);
}

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())
G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

GtkWidget * demo_widget_new       (void);

void        demo_widget_add_child (DemoWidget *self,
                                   GtkWidget  *child);

struct _DemoWidget
{
  GtkWidget parent_instance;

  struct wl_display *wl_display;
  struct wl_surface *parent;
  struct wl_surface *child;
  struct wl_subsurface *subsurface;

  GdkRGBA color1;
  GdkRGBA color2;
  guint64 time2;
  float t;

  guint tick_cb;

  GtkPopover *popover;
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

#define TIME_SPAN (3.0 * G_TIME_SPAN_SECOND)

static gboolean
change_color (GtkWidget     *widget,
              GdkFrameClock *frame_clock,
              gpointer       data)
{
  DemoWidget *demo = DEMO_WIDGET (widget);
  gint64 time;

  time = gdk_frame_clock_get_frame_time (frame_clock);

  if (time >= demo->time2)
    {
      demo->time2 = time + TIME_SPAN;

      demo->color1 = demo->color2;
      demo->color2.red = g_random_double_range (0, 1);
      demo->color2.green = g_random_double_range (0, 1);
      demo->color2.blue = g_random_double_range (0, 1);
      demo->color2.alpha = 1;
    }

  demo->t = 1 - (demo->time2 - time) / TIME_SPAN;

  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  DemoWidget *demo = data;

  gtk_popover_set_pointing_to (demo->popover, &(GdkRectangle){ x, y, 1, 1 });
  gtk_popover_popup (demo->popover);
}

static void
clicked_cb (GtkButton *button,
            gpointer   data)
{
  DemoWidget *demo = data;

  gtk_popover_popdown (demo->popover);
}

static void
demo_widget_init (DemoWidget *demo)
{
  GtkWidget *button;
  GtkGesture *controller;

  gtk_widget_add_tick_callback (GTK_WIDGET (demo), change_color, NULL, NULL);

  demo->popover = GTK_POPOVER (gtk_popover_new ());
  gtk_widget_set_parent (GTK_WIDGET (demo->popover), GTK_WIDGET (demo));
  button = gtk_button_new_with_label ("OK");
  g_signal_connect (button, "clicked", G_CALLBACK (clicked_cb), demo);
  gtk_popover_set_child (demo->popover, button);

  controller = gtk_gesture_click_new ();
  g_signal_connect (controller, "pressed", G_CALLBACK (pressed_cb), demo);
  gtk_widget_add_controller (GTK_WIDGET (demo), GTK_EVENT_CONTROLLER (controller));
}

static void
demo_widget_dispose (GObject *object)
{
  //DemoWidget *demo = DEMO_WIDGET (object);

  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_realize (GtkWidget *widget)
{
  DemoWidget *demo = DEMO_WIDGET (widget);
  GdkSurface *surface;

  GTK_WIDGET_CLASS (demo_widget_parent_class)->realize (widget);

  demo->wl_display = gdk_wayland_display_get_wl_display (gtk_widget_get_display (widget));
  surface = gtk_native_get_surface (gtk_widget_get_native (widget));
  demo->parent = gdk_wayland_surface_get_wl_surface (surface);

  set_up_registry (demo->wl_display);
  create_subsurface (demo->parent, &demo->child, &demo->subsurface);
}

static void
demo_widget_unrealize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (demo_widget_parent_class)->unrealize (widget);

  /* FIXME */
}

static void
demo_widget_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum_size,
                     int            *natural_size,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  *minimum_size = 100;
  *natural_size = 200;
}

static void
demo_widget_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  DemoWidget *demo = DEMO_WIDGET (widget);
  GtkNative *native;
  double x0, y0, x, y;

  native = gtk_widget_get_native (widget);
  gtk_native_get_surface_transform (native, &x0, &y0);
  gtk_widget_translate_coordinates (widget,
                                    GTK_WIDGET (native),
                                    0, 0,
                                    &x, &y);
  wl_subsurface_set_position (demo->subsurface, x0 + x + 40, y0 + y + 40);

  gtk_popover_present (demo->popover);
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  DemoWidget *demo = DEMO_WIDGET (widget);
  int width, height;
  GdkRGBA c;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  c.red = (1 - demo->t) * demo->color1.red + demo->t * demo->color2.red;
  c.green = (1 - demo->t) * demo->color1.green + demo->t * demo->color2.green;
  c.blue = (1 - demo->t) * demo->color1.blue + demo->t * demo->color2.blue;
  c.alpha = 1;

  surface_fill (demo->child, width - 80, height - 80, &c);
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;

  widget_class->realize = demo_widget_realize;
  widget_class->unrealize = demo_widget_unrealize;
  widget_class->measure = demo_widget_measure;
  widget_class->size_allocate = demo_widget_size_allocate;
  widget_class->snapshot = demo_widget_snapshot;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *child;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "hello subsurface");

  child = g_object_new (demo_widget_get_type (), NULL);
  gtk_window_set_child (GTK_WINDOW (window), child);

  gtk_widget_show (window);

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
