/* Paintable/SVG
 *
 * This demo shows wrapping a librsvg RsvgHandle in a GdkPaintable
 * to display an SVG image that can be scaled by resizing the window.
 *
 * This demo relies on librsvg, which GTK itself does not link against.
 */

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

#define SVG_TYPE_PAINTABLE (svg_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SvgPaintable, svg_paintable, SVG, PAINTABLE, GObject)

struct _SvgPaintable
{
  GObject parent_instance;
  GFile *file;
  RsvgHandle *handle;
};

struct _SvgPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  NUM_PROPERTIES
};

static void
svg_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  cairo_t *cr;
  GError *error = NULL;

  cr = gtk_snapshot_append_cairo (GTK_SNAPSHOT (snapshot),
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (!rsvg_handle_render_document (self->handle, cr,
                                    &(RsvgRectangle) {0, 0, width, height},
                                    &error))
    {
      g_error ("%s", error->message);
    }

  cairo_destroy (cr);
}

static int
svg_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  RsvgDimensionData data;

  rsvg_handle_get_dimensions (self->handle, &data);

  return data.width;
}

static int
svg_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  RsvgDimensionData data;

  rsvg_handle_get_dimensions (self->handle, &data);

  return data.height;
}

static void
svg_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = svg_paintable_snapshot;
  iface->get_intrinsic_width = svg_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = svg_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (SvgPaintable, svg_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                svg_paintable_init_interface))

static void
svg_paintable_init (SvgPaintable *self)
{
}

static void
svg_paintable_dispose (GObject *object)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  g_clear_object (&self->file);
  g_clear_object (&self->handle);

  G_OBJECT_CLASS (svg_paintable_parent_class)->dispose (object);
}

static void
svg_paintable_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      {
        GFile *file = g_value_get_object (value);
        RsvgHandle *handle = rsvg_handle_new_from_gfile_sync (file,
                                                              RSVG_HANDLE_FLAGS_NONE,
                                                              NULL,
                                                              NULL);
        rsvg_handle_set_dpi (handle, 90);

        g_set_object (&self->file, file);
        g_set_object (&self->handle, handle);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
svg_paintable_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
svg_paintable_class_init (SvgPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = svg_paintable_dispose;
  object_class->set_property = svg_paintable_set_property;
  object_class->get_property = svg_paintable_get_property;

  g_object_class_install_property (object_class, PROP_FILE,
                                   g_param_spec_object ("file", "File", "File",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}

static SvgPaintable *
svg_paintable_new (GFile *file)
{
  return g_object_new (SVG_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}

static void
file_set (GtkFileChooserButton *button,
          GtkWidget            *picture)
{
  GFile *file;
  SvgPaintable *paintable;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button));

  paintable = svg_paintable_new (file);
  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (paintable));

  g_object_unref (paintable);
  g_object_unref (file);
}

static GtkWidget *window;

GtkWidget *
do_paintable_svg (GtkWidget *do_widget)
{
  GtkWidget *header;
  GtkWidget *picture;
  GtkFileFilter *filter;
  GtkWidget *button;
  GFile *file;
  SvgPaintable *paintable;

  if (!window)
    {
      window = gtk_window_new ();
      header = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 330);
      gtk_window_set_title (GTK_WINDOW (window), "Paintable — SVG");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      button = gtk_file_chooser_button_new ("Select an SVG file", GTK_FILE_CHOOSER_ACTION_OPEN);
      filter = gtk_file_filter_new ();
      gtk_file_filter_add_mime_type (filter, "image/svg+xml");
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (button), filter);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      picture = gtk_picture_new ();
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
      gtk_widget_set_size_request (picture, 16, 16);

      g_signal_connect (button, "file-set", G_CALLBACK (file_set), picture);

      gtk_window_set_child (GTK_WINDOW (window), picture);

      file = g_file_new_for_uri ("resource:///paintable_svg/org.gtk.gtk4.NodeEditor.Devel.svg");
      paintable = svg_paintable_new (file);
      gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (paintable));
      g_object_unref (paintable);
      g_object_unref (file);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
