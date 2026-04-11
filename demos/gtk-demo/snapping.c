/* Snapping
 *
 * Demonstrates how to snap content.
 *
 * This demo uses a grid of tiles and scales it without
 * seams when resizing the window.
 */

#include <gtk/gtk.h>
#include "demo4widget.h"

static const gboolean qr[] = {
  0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0,
  0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0,
  0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0,
  0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0,
  0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0,
  0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0,
  1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1,
  1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1,
  1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0,
  1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
  0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
  0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0,
  0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1,
  0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0
};

#define GTK_TYPE_TILER (gtk_tiler_get_type ())
G_DECLARE_FINAL_TYPE (GtkTiler, gtk_tiler, GTK, TILER, GObject)

/* This is our object. It's just a timezone */
typedef struct _GtkTiler GtkTiler;
struct _GtkTiler
{
  GObject parent_instance;
};

static void
gtk_tiler_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  const GdkRGBA colors[2] = {
    { 0, 0, 1, 1 },
    { 1, 1, 0, 1 }
  };

  gsize x, y;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_set_snap (snapshot, GSK_RECT_SNAP_ROUND);
  gtk_snapshot_scale (snapshot, width / 21.0, height / 21.0);

  for (y = 0; y < 21; y++)
    {
      for (x = 0; x < 21; x++)
        {
          gtk_snapshot_append_color (snapshot,
                                     &colors[qr[21 * y + x]],
                                     &GRAPHENE_RECT_INIT (x, y, 1, 1));
        }
    }

  gtk_snapshot_restore (snapshot);
}

static double
gtk_tiler_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  return 1.0;
}

static void
gtk_tiler_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_tiler_snapshot;
  iface->get_intrinsic_aspect_ratio = gtk_tiler_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_WITH_CODE (GtkTiler, gtk_tiler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_tiler_paintable_init))

static void
gtk_tiler_class_init (GtkTilerClass *klass)
{
}

static void
gtk_tiler_init (GtkTiler *self)
{
}


GtkWidget *
do_snapping (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picture;
      GdkPaintable *tiler;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Snapping");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      picture = gtk_picture_new ();
      gtk_window_set_child (GTK_WINDOW (window), picture);

      tiler = g_object_new (GTK_TYPE_TILER, NULL);
      gtk_picture_set_paintable (GTK_PICTURE (picture), tiler);
      g_object_unref (tiler);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
