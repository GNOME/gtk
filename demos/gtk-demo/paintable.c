/* Paintable/A simple paintable
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

static GtkWidget *window = NULL;

/* First, add the boilerplate for the object itself.
 * This part would normally go in the header.
 */
#define GTK_TYPE_NUCLEAR_ICON (gtk_nuclear_icon_get_type ())
G_DECLARE_FINAL_TYPE (GtkNuclearIcon, gtk_nuclear_icon, GTK, NUCLEAR_ICON, GObject)

GdkPaintable *gtk_nuclear_icon_new (void);

/* Declare the struct. We have no custom data, so it's very
 * bare.
 */
struct _GtkNuclearIcon
{
  GObject parent_instance;
};

struct _GtkNuclearIconClass
{
  GObjectClass parent_class;
};

/* Here, we implement the functionality required by the GdkPaintable interface */
static void
gtk_nuclear_icon_snapshot (GdkPaintable *paintable,
                           GdkSnapshot  *snapshot,
                           double        width,
                           double        height)
{
#define RADIUS 0.3
  cairo_t *cr;
  double size;

  /* The snapshot function is the only function we need to implement.
   * It does the actual drawing of the paintable.
   */

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 0.9, 0.75, 0.15, 1.0 },
                             &GRAPHENE_RECT_INIT (0, 0, width, height),
                             "Yellow background");

  size = MIN (width, height);
  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT ((width - size) / 2.0,
                                                       (height - size) / 2.0,
                                                       size, size),
                                  "Radioactive Icon");
  cairo_translate (cr, width / 2.0, height / 2.0);
  cairo_scale (cr, size, size);

  cairo_arc (cr, 0, 0, 0.1, - G_PI, G_PI);
  cairo_fill (cr);

  cairo_set_line_width (cr, RADIUS);
  cairo_set_dash (cr, (double[1]) { RADIUS * G_PI / 3 }, 1, 0.0);
  cairo_arc (cr, 0, 0, RADIUS, - G_PI, G_PI);
  cairo_stroke (cr);

  cairo_destroy (cr);
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

/* And finally, we add the simple constructor we declared in the header. */
GdkPaintable *
gtk_nuclear_icon_new (void)
{
  return g_object_new (GTK_TYPE_NUCLEAR_ICON, NULL);
}

GtkWidget *
do_paintable (GtkWidget *do_widget)
{
  GdkPaintable *nuclear;
  GtkWidget *image;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Nuclear Icon");
      gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);

      nuclear = gtk_nuclear_icon_new ();
      image = gtk_image_new_from_paintable (nuclear);
      gtk_container_add (GTK_CONTAINER (window), image);
      g_object_unref (nuclear);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
