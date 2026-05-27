/* Snapping
 *
 * Demonstrates how to snap content.
 *
 * This demo uses a grid of tiles and scales it without
 * seams when resizing the window.
 */

#include <gtk/gtk.h>
#include "demo4widget.h"

static const gboolean qr_code[] = {
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

enum {
  ON,
  OFF,
  BETWEEN
};

static const GdkRGBA colors[] = {
  [ON]      = { 0.2,  0.6,   1,    1 },
  [OFF]     = { 1,    0.95,  0.7,  1 },
  [BETWEEN] = { 1,    1,     1,    1 },
};

static void
snapshot_point (GtkSnapshot *snapshot,
                gboolean     on)
{
  gtk_snapshot_append_color (snapshot,
                             &colors[on ? ON : OFF],
                             &GRAPHENE_RECT_INIT (0, 0, 1, 1));
}

static void
snapshot_between_2_points (GtkSnapshot    *snapshot,
                           GtkOrientation  orientation,
                           gboolean        on_first,
                           gboolean        on_second)
{
  if (on_first == on_second)
    {
      snapshot_point (snapshot, on_first);
      return;
    }

  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, 0, 1, 1),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       orientation == GTK_ORIENTATION_HORIZONTAL
                                       ? &GRAPHENE_POINT_INIT (1, 0)
                                       : &GRAPHENE_POINT_INIT (0, 1),
                                       (GskColorStop[3]) {
                                           { 0.3, colors[on_first ? ON : OFF] },
                                           { 0.5, colors[BETWEEN] },
                                           { 0.7, colors[on_second ? ON : OFF] }
                                       },
                                       3);
}

static void
snapshot_between_4_points (GtkSnapshot    *snapshot,
                           gboolean        top_left,
                           gboolean        top_right,
                           gboolean        bottom_left,
                           gboolean        bottom_right)
{
  if (top_left == top_right && bottom_left == bottom_right)
    {
      snapshot_between_2_points (snapshot, GTK_ORIENTATION_VERTICAL, top_left, bottom_left);
      return;
    }
  else if (top_left == bottom_left && top_right == bottom_right)
    {
      snapshot_between_2_points (snapshot, GTK_ORIENTATION_HORIZONTAL, top_left, top_right);
      return;
    }
  else if (top_left == bottom_right && top_right == bottom_left)
    {
      /* across */
      gtk_snapshot_append_conic_gradient (snapshot,
                                          &GRAPHENE_RECT_INIT (0, 0, 1, 1),
                                          &GRAPHENE_POINT_INIT (0.5, 0.5),
                                          0,
                                          (GskColorStop[13]) {
                                              { 0,    colors[BETWEEN] },
                                              { 0.06, colors[top_right ? ON : OFF] },
                                              { 0.19, colors[top_right ? ON : OFF] },
                                              { 0.25, colors[BETWEEN] },
                                              { 0.31, colors[bottom_right ? ON : OFF] },
                                              { 0.44, colors[bottom_right ? ON : OFF] },
                                              { 0.5,  colors[BETWEEN] },
                                              { 0.56, colors[bottom_left ? ON : OFF] },
                                              { 0.69, colors[bottom_left ? ON : OFF] },
                                              { 0.75, colors[BETWEEN] },
                                              { 0.81, colors[top_left ? ON : OFF] },
                                              { 0.94, colors[top_left ? ON : OFF] },
                                              { 1.0,  colors[BETWEEN] }
                                          },
                                          13);
    }
  else
    {
      gboolean on[] = { top_left, top_right, bottom_left, bottom_right };
      gsize i;

      /* one of them is different */
      for (i = 0; i < 3; i++)
        {
          if (on[i] != on[3])
            break;
        }
      if (i == 0 && on[0] == on[1])
        i = 3;

      gtk_snapshot_append_radial_gradient (snapshot,
                                           &GRAPHENE_RECT_INIT (0, 0, 1, 1),
                                           &GRAPHENE_POINT_INIT (i & 1 ? 1 : 0, i & 2 ? 1 : 0),
                                           1,
                                           1,
                                           0,
                                           1,
                                           (GskColorStop[3]) {
                                               { 0.3, colors[on[i] ? ON : OFF] },
                                               { 0.5, colors[BETWEEN] },
                                               { 0.7, colors[on[i] ? OFF : ON] }
                                           },
                                           3);
    }
}

static void
snapshot_line (GtkSnapshot    *snapshot,
               const gboolean *qr)
{
  gsize x;

  gtk_snapshot_save (snapshot);

  snapshot_between_2_points (snapshot, GTK_ORIENTATION_HORIZONTAL, qr[0], qr[0]);
  for (x = 0; x < 21; x++)
    {
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (1, 0));
      snapshot_point (snapshot, qr[x]);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (1, 0));
      snapshot_between_2_points (snapshot, GTK_ORIENTATION_HORIZONTAL, qr[x], qr[MIN (x + 1, 20)]);
    }

  gtk_snapshot_restore (snapshot);
}

static void
snapshot_between_lines (GtkSnapshot    *snapshot,
                        const gboolean *above,
                        const gboolean *below)
{
  gsize x;

  gtk_snapshot_save (snapshot);

  snapshot_between_4_points (snapshot, above[0], above[0], below[0], below[0]);
  for (x = 0; x < 21; x++)
    {
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (1, 0));
      snapshot_between_2_points (snapshot, GTK_ORIENTATION_VERTICAL, above[x], below[x]);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (1, 0));
      snapshot_between_4_points (snapshot, above[x], above[MIN (x + 1, 20)], below[x], below[MIN (x + 1, 20)]);
    }

  gtk_snapshot_restore (snapshot);
}

static void
gtk_tiler_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  gsize y;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_set_snap (snapshot, GSK_RECT_SNAP_ROUND);
  gtk_snapshot_scale (snapshot, width / 43.0, height / 43.0);

  snapshot_between_lines (snapshot, qr_code, qr_code);
  for (y = 0; y < 21; y++)
    {
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, 1));
      snapshot_line (snapshot, &qr_code[21 * y]);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, 1));
      snapshot_between_lines (snapshot, &qr_code[21 * y], &qr_code[21 * MIN (y + 1, 20)]);
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
