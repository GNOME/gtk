#include <gtk/gtk.h>
#include "gtk/gtkwidgetprivate.h"
#include "gdk/gdksurfaceprivate.h"

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
