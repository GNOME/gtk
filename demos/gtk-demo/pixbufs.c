/* Pixbufs
 *
 * A GdkPixbuf represents an image, normally in RGB or RGBA format.
 * Pixbufs are normally used to load files from disk and perform
 * image scaling.
 *
 * This demo is not all that educational, but looks cool. It was written
 * by Extreme Pixbuf Hacker Federico Mena Quintero. It also shows
 * off how to use GtkDrawingArea to do a simple animation.
 *
 * Look at the Image demo for additional pixbuf usage examples.
 *
 */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <math.h>

#include "demo-common.h"

#define FRAME_DELAY 50

#define BACKGROUND_NAME "background.jpg"

static const char *image_names[] = {
  "apple-red.png",
  "gnome-applets.png",
  "gnome-calendar.png",
  "gnome-foot.png",
  "gnome-gmush.png",
  "gnome-gimp.png",
  "gnome-gsame.png",
  "gnu-keys.png"
};

#define N_IMAGES G_N_ELEMENTS (image_names)

/* demo window */
static GtkWidget *window = NULL;

/* Current frame */
static GdkPixbuf *frame;

/* Background image */
static GdkPixbuf *background;
static gint back_width, back_height;

/* Images */
static GdkPixbuf *images[N_IMAGES];

/* Widgets */
static GtkWidget *da;

/* Loads the images for the demo and returns whether the operation succeeded */
static gboolean
load_pixbufs (GError **error)
{
  gint i;
  char *filename;

  if (background)
    return TRUE; /* already loaded earlier */

  /* demo_find_file() looks in the current directory first,
   * so you can run gtk-demo without installing GTK, then looks
   * in the location where the file is installed.
   */
  filename = demo_find_file (BACKGROUND_NAME, error);
  if (!filename)
    return FALSE; /* note that "error" was filled in and returned */

  background = gdk_pixbuf_new_from_file (filename, error);
  g_free (filename);

  if (!background)
    return FALSE; /* Note that "error" was filled with a GError */

  back_width = gdk_pixbuf_get_width (background);
  back_height = gdk_pixbuf_get_height (background);

  for (i = 0; i < N_IMAGES; i++)
    {
      filename = demo_find_file (image_names[i], error);
      if (!filename)
        return FALSE; /* Note that "error" was filled with a GError */

      images[i] = gdk_pixbuf_new_from_file (filename, error);
      g_free (filename);

      if (!images[i])
        return FALSE; /* Note that "error" was filled with a GError */
    }

  return TRUE;
}

/* Expose callback for the drawing area */
static gint
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
  gdk_cairo_set_source_pixbuf (cr, frame, 0, 0);
  cairo_paint (cr);

  return TRUE;
}

#define CYCLE_LEN 60

static int frame_num;

/* Timeout handler to regenerate the frame */
static gint
timeout (gpointer data)
{
  double f;
  int i;
  double xmid, ymid;
  double radius;

  gdk_pixbuf_copy_area (background, 0, 0, back_width, back_height,
                        frame, 0, 0);

  f = (double) (frame_num % CYCLE_LEN) / CYCLE_LEN;

  xmid = back_width / 2.0;
  ymid = back_height / 2.0;

  radius = MIN (xmid, ymid) / 2.0;

  for (i = 0; i < N_IMAGES; i++)
    {
      double ang;
      int xpos, ypos;
      int iw, ih;
      double r;
      GdkRectangle r1, r2, dest;
      double k;

      ang = 2.0 * G_PI * (double) i / N_IMAGES - f * 2.0 * G_PI;

      iw = gdk_pixbuf_get_width (images[i]);
      ih = gdk_pixbuf_get_height (images[i]);

      r = radius + (radius / 3.0) * sin (f * 2.0 * G_PI);

      xpos = floor (xmid + r * cos (ang) - iw / 2.0 + 0.5);
      ypos = floor (ymid + r * sin (ang) - ih / 2.0 + 0.5);

      k = (i & 1) ? sin (f * 2.0 * G_PI) : cos (f * 2.0 * G_PI);
      k = 2.0 * k * k;
      k = MAX (0.25, k);

      r1.x = xpos;
      r1.y = ypos;
      r1.width = iw * k;
      r1.height = ih * k;

      r2.x = 0;
      r2.y = 0;
      r2.width = back_width;
      r2.height = back_height;

      if (gdk_rectangle_intersect (&r1, &r2, &dest))
        gdk_pixbuf_composite (images[i],
                              frame,
                              dest.x, dest.y,
                              dest.width, dest.height,
                              xpos, ypos,
                              k, k,
                              GDK_INTERP_NEAREST,
                              ((i & 1)
                               ? MAX (127, fabs (255 * sin (f * 2.0 * G_PI)))
                               : MAX (127, fabs (255 * cos (f * 2.0 * G_PI)))));
    }

  gtk_widget_queue_draw (da);

  frame_num++;
  return G_SOURCE_CONTINUE;
}

static guint timeout_id;

static void
cleanup_callback (GObject   *object,
                  gpointer   data)
{
  g_source_remove (timeout_id);
  timeout_id = 0;
}

GtkWidget *
do_pixbufs (GtkWidget *do_widget)
{
  if (!window)
    {
      GError *error;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Pixbufs");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (cleanup_callback), NULL);


      error = NULL;
      if (!load_pixbufs (&error))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failed to load an image: %s",
                                           error->message);

          g_error_free (error);

          g_signal_connect (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), NULL);

          gtk_widget_show (dialog);
        }
      else
        {
          gtk_widget_set_size_request (window, back_width, back_height);

          frame = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, back_width, back_height);

          da = gtk_drawing_area_new ();

          g_signal_connect (da, "draw",
                            G_CALLBACK (draw_cb), NULL);

          gtk_container_add (GTK_CONTAINER (window), da);

          timeout_id = g_timeout_add (FRAME_DELAY, timeout, NULL);
        }
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
      g_object_unref (frame);
    }

  return window;
}
