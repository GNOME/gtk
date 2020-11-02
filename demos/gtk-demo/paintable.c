/* Paintable/Simple Paintable
 *
 * GdkPaintable is an interface used by GTK for drawings of any sort
 * that do not require layouting or positioning.
 *
 * This demo code gives a simple example on how a paintable can
 * be created.
 *
 * Paintables can be used in many places inside GTK widgets, but the
 * most common usage is inside GtkImage and that's what we're going
 * to do here.
 */

#include <gtk/gtk.h>

#include "paintable.h"

static GtkWidget *window = NULL;

/* First, add the boilerplate for the object itself.
 * This part would normally go in the header.
 */
#define GTK_TYPE_NUCLEAR_ICON (gtk_nuclear_icon_get_type ())
G_DECLARE_FINAL_TYPE (GtkNuclearIcon, gtk_nuclear_icon, GTK, NUCLEAR_ICON, GObject)

/* Declare the struct. */
struct _GtkNuclearIcon
{
  GObject parent_instance;

  /* We store this rotation value here.
   * We are not doing with it here, but it will come in
   * very useful in the followup demos.
   */
  double rotation;
};

struct _GtkNuclearIconClass
{
  GObjectClass parent_class;
};

/* This is the function that draws the actual icon.
 * We make it a custom function and define it in the paintable.h header
 * so that it can be called from all the other demos, too.
 */
void
gtk_nuclear_snapshot (GtkSnapshot *snapshot,
                      double       width,
                      double       height,
                      double       rotation,
                      gboolean     draw_background)
{
#define RADIUS 0.3
  cairo_t *cr;
  double size;

  if (draw_background)
    gtk_snapshot_append_color (snapshot,
                               &(GdkRGBA) { 0.9, 0.75, 0.15, 1.0 },
                               &GRAPHENE_RECT_INIT (0, 0, width, height));

  size = MIN (width, height);
  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT ((width - size) / 2.0,
                                                       (height - size) / 2.0,
                                                       size, size));
  cairo_translate (cr, width / 2.0, height / 2.0);
  cairo_scale (cr, size, size);
  cairo_rotate (cr, rotation);

  cairo_arc (cr, 0, 0, 0.1, - G_PI, G_PI);
  cairo_fill (cr);

  cairo_set_line_width (cr, RADIUS);
  cairo_set_dash (cr, (double[1]) { RADIUS * G_PI / 3 }, 1, 0.0);
  cairo_arc (cr, 0, 0, RADIUS, - G_PI, G_PI);
  cairo_stroke (cr);

  cairo_destroy (cr);
}

/* Here, we implement the functionality required by the GdkPaintable interface */
static void
gtk_nuclear_icon_snapshot (GdkPaintable *paintable,
                           GdkSnapshot  *snapshot,
                           double        width,
                           double        height)
{
  GtkNuclearIcon *nuclear = GTK_NUCLEAR_ICON (paintable);

  /* The snapshot function is the only function we need to implement.
   * It does the actual drawing of the paintable.
   */

  gtk_nuclear_snapshot (snapshot,
                        width, height,
                        nuclear->rotation,
                        TRUE);
}

static GdkPaintableFlags
gtk_nuclear_icon_get_flags (GdkPaintable *paintable)
{
  /* The flags are very useful to let GTK know that this image
   * is never going to change.
   * This allows many optimizations and should therefore always
   * be set.
   */
  return GDK_PAINTABLE_STATIC_CONTENTS | GDK_PAINTABLE_STATIC_SIZE;
}

static void
gtk_nuclear_icon_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_nuclear_icon_snapshot;
  iface->get_flags = gtk_nuclear_icon_get_flags;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkNuclearIcon, gtk_nuclear_icon, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_nuclear_icon_paintable_init))

/* Here's the boilerplate for the GObject declaration.
 * We don't need to do anything special here, because we keep no
 * data of our own.
 */
static void
gtk_nuclear_icon_class_init (GtkNuclearIconClass *klass)
{
}

static void
gtk_nuclear_icon_init (GtkNuclearIcon *nuclear)
{
}

/* And finally, we add a simple constructor.
 * It is declared in the header so that the other examples
 * can use it.
 */
GdkPaintable *
gtk_nuclear_icon_new (double rotation)
{
  GtkNuclearIcon *nuclear;

  nuclear = g_object_new (GTK_TYPE_NUCLEAR_ICON, NULL);
  nuclear->rotation = rotation;

  return GDK_PAINTABLE (nuclear);
}

GtkWidget *
do_paintable (GtkWidget *do_widget)
{
  GdkPaintable *nuclear;
  GtkWidget *image;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Nuclear Icon");
      gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      nuclear = gtk_nuclear_icon_new (0.0);
      image = gtk_image_new_from_paintable (nuclear);
      gtk_window_set_child (GTK_WINDOW (window), image);
      g_object_unref (nuclear);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
