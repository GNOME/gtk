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

static struct wl_compositor *wl_compositor;
static struct wl_subcompositor *wl_subcompositor;
static struct wl_shm *wl_shm;

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
