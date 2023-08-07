/* Path/Fill and Stroke
 *
 * This demo shows how to use GskPath to draw shapes that are (a bit)
 * more complex than a rounded rectangle.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "paintable.h"

#define GTK_TYPE_LOGO_PAINTABLE (gtk_logo_paintable_get_type ())
G_DECLARE_FINAL_TYPE (GtkLogoPaintable, gtk_logo_paintable, GTK, LOGO_PAINTABLE, GObject)

struct _GtkLogoPaintable
{
  GObject parent_instance;

  int width;
  int height;
  GskPath *path[3];
  GdkRGBA color[3];

  GskPath *stroke_path;
  GskStroke *stroke1;
  GskStroke *stroke2;
  GdkRGBA stroke_color;
};

struct _GtkLogoPaintableClass
{
  GObjectClass parent_class;
};

static int
gtk_logo_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkLogoPaintable *self = GTK_LOGO_PAINTABLE (paintable);

  return self->width;
}

static int
gtk_logo_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkLogoPaintable *self = GTK_LOGO_PAINTABLE (paintable);

  return self->height;
}

static void
gtk_logo_paintable_snapshot (GdkPaintable *paintable,
                             GdkSnapshot  *snapshot,
                             double        width,
                             double        height)
{
  GtkLogoPaintable *self = GTK_LOGO_PAINTABLE (paintable);

  for (unsigned int i = 0; i < 3; i++)
    {
      gtk_snapshot_push_fill (snapshot, self->path[i], GSK_FILL_RULE_WINDING);
      gtk_snapshot_append_color (snapshot,
                                 &self->color[i],
                                 &GRAPHENE_RECT_INIT (0, 0, width, height));
      gtk_snapshot_pop (snapshot);
    }
  for (unsigned int i = 0; i < 3; i++)
    {
      gtk_snapshot_push_stroke (snapshot, self->stroke_path, self->stroke1);
      gtk_snapshot_append_color (snapshot,
                                 &self->stroke_color,
                                 &GRAPHENE_RECT_INIT (0, 0, width, height));
      gtk_snapshot_pop (snapshot);
    }

  gtk_snapshot_push_stroke (snapshot, self->stroke_path, self->stroke2);
  gtk_snapshot_append_color (snapshot,
                             &self->stroke_color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (snapshot);
}

static GdkPaintableFlags
gtk_logo_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_CONTENTS | GDK_PAINTABLE_STATIC_SIZE;
}

static void
gtk_logo_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = gtk_logo_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_logo_paintable_get_intrinsic_height;
  iface->snapshot = gtk_logo_paintable_snapshot;
  iface->get_flags = gtk_logo_paintable_get_flags;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkLogoPaintable, gtk_logo_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_logo_paintable_paintable_init))

static void
gtk_logo_paintable_dispose (GObject *object)
{
  GtkLogoPaintable *self = GTK_LOGO_PAINTABLE (object);

  for (unsigned int i = 0; i < 3; i++)
    gsk_path_unref (self->path[i]);

  gsk_path_unref (self->stroke_path);

  gsk_stroke_free (self->stroke1);
  gsk_stroke_free (self->stroke2);

  G_OBJECT_CLASS (gtk_logo_paintable_parent_class)->dispose (object);
}

static void
gtk_logo_paintable_class_init (GtkLogoPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_logo_paintable_dispose;
}

static void
gtk_logo_paintable_init (GtkLogoPaintable *self)
{
}

static GdkPaintable *
gtk_logo_paintable_new (void)
{
  GtkLogoPaintable *self;
  graphene_rect_t bounds, bounds2;

  self = g_object_new (GTK_TYPE_LOGO_PAINTABLE, NULL);

  /* Paths and colors extracted from gtk-logo.svg */
  self->path[0] = gsk_path_parse ("m3.12,66.17 -2.06,-51.46 32.93,24.7 v55.58 l-30.87,-28.82 z");
  self->path[1] = gsk_path_parse ("m34,95 49.4,-20.58 4.12,-51.46 -53.52,16.47 v55.58 z");
  self->path[2] = gsk_path_parse ("m1.06,14.71 32.93,24.7 53.52,-16.47 -36.75,-21.88 -49.7,13.65 z");

  gdk_rgba_parse (&self->color[0], "#e40000");
  gdk_rgba_parse (&self->color[1], "#7fe719");
  gdk_rgba_parse (&self->color[2], "#729fcf");

  self->stroke_path = gsk_path_parse ("m50.6,51.3 -47.3,14 z l33,23 z v-50");
  self->stroke1 = gsk_stroke_new (2.12);
  self->stroke2 = gsk_stroke_new (1.25);
  gdk_rgba_parse (&self->stroke_color, "#ffffff");

  gsk_path_get_stroke_bounds (self->path[0], self->stroke1, &bounds);
  gsk_path_get_stroke_bounds (self->path[1], self->stroke1, &bounds2);
  graphene_rect_union (&bounds, &bounds2, &bounds);
  gsk_path_get_stroke_bounds (self->path[2], self->stroke1, &bounds2);
  graphene_rect_union (&bounds, &bounds2, &bounds);
  gsk_path_get_stroke_bounds (self->stroke_path, self->stroke2, &bounds2);
  graphene_rect_union (&bounds, &bounds2, &bounds);

  self->width = bounds.origin.x + bounds.size.width;
  self->height = bounds.origin.y + bounds.size.height;

  return GDK_PAINTABLE (self);
}

GtkWidget *
do_path_fill (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picture;
      GdkPaintable *paintable;

      window = gtk_window_new ();
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_window_set_title (GTK_WINDOW (window), "Path Fill");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      paintable = gtk_logo_paintable_new ();
      picture = gtk_picture_new_for_paintable (paintable);
      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
      g_object_unref (paintable);

      gtk_window_set_child (GTK_WINDOW (window), picture);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
