#include "symbolicpaintable.h"

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

struct _SymbolicPaintable
{
  GObject parent_instance;
  GFile *file;
  RsvgHandle *handle;
};

struct _SymbolicPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  NUM_PROPERTIES
};

static void
snapshot_with_style (SymbolicPaintable *self,
                     GdkSnapshot       *snapshot,
                     double             width,
                     double             height,
                     const char        *css,
                     gsize              css_len)
{
  cairo_t *cr;
  GError *error = NULL;

  if (!rsvg_handle_set_stylesheet (self->handle, (const guint8 *) css, css_len, &error))
    {
      g_error ("%s", error->message);
    }

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
static void
symbolic_paintable_snapshot (GdkPaintable *paintable,
                             GdkSnapshot  *snapshot,
                             double        width,
                             double        height)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);

  snapshot_with_style (self, snapshot, width, height, "", 0);
}

static int
symbolic_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);
  double width;

  if (!rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &width, NULL))
    return 0;

  return ceil (width);
}

static int
symbolic_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);
  double height;

  if (!rsvg_handle_get_intrinsic_size_in_pixels (self->handle, NULL, &height))
    return 0;

  return ceil (height);
}

static void
symbolic_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = symbolic_paintable_snapshot;
  iface->get_intrinsic_width = symbolic_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = symbolic_paintable_get_intrinsic_height;
}

static void
make_stylesheet (char          *buffer,
                 gsize          size,
                 const GdkRGBA *fg_color,
                 const GdkRGBA *success_color,
                 const GdkRGBA *warning_color,
                 const GdkRGBA *error_color)
{
  g_snprintf (buffer, size,
              "rect,circle,path { fill: rgba(%d,%d,%d,%.2f) !important; }\n"
              ".success { fill: rgba(%d,%d,%d,%.2f) !important; }\n"
              ".warning { fill: rgba(%d,%d,%d,%.2f) !important; }\n"
              ".error { fill: rgba(%d,%d,%d,%.2f) !important; }\n",
              (int)(0.5 + CLAMP (fg_color->red, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (fg_color->green, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (fg_color->blue, 0., 1.) * 255.),
              fg_color->alpha,
              (int)(0.5 + CLAMP (success_color->red, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (success_color->green, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (success_color->blue, 0., 1.) * 255.),
              success_color->alpha,
              (int)(0.5 + CLAMP (warning_color->red, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (warning_color->green, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (warning_color->blue, 0., 1.) * 255.),
              warning_color->alpha,
              (int)(0.5 + CLAMP (error_color->red, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (error_color->green, 0., 1.) * 255.),
              (int)(0.5 + CLAMP (error_color->blue, 0., 1.) * 255.),
              error_color->alpha);
}

static void
symbolic_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                      GdkSnapshot          *snapshot,
                                      double                width,
                                      double                height,
                                      const GdkRGBA        *colors,
                                      gsize                 n_colors)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);
  char css[512];

  make_stylesheet (css, sizeof (css),
                   &colors[GTK_SYMBOLIC_COLOR_FOREGROUND],
                   &colors[GTK_SYMBOLIC_COLOR_SUCCESS],
                   &colors[GTK_SYMBOLIC_COLOR_WARNING],
                   &colors[GTK_SYMBOLIC_COLOR_ERROR]);

  snapshot_with_style (self, snapshot, width, height, css, strlen (css));
}

static void
symbolic_symbolic_paintable_init_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = symbolic_paintable_snapshot_symbolic;
}

G_DEFINE_TYPE_WITH_CODE (SymbolicPaintable, symbolic_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                symbolic_paintable_init_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                symbolic_symbolic_paintable_init_interface))

static void
symbolic_paintable_init (SymbolicPaintable *self)
{
}

static void
symbolic_paintable_dispose (GObject *object)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (object);

  g_clear_object (&self->file);
  g_clear_object (&self->handle);

  G_OBJECT_CLASS (symbolic_paintable_parent_class)->dispose (object);
}

static void
symbolic_paintable_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (object);

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
symbolic_paintable_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (object);

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
symbolic_paintable_class_init (SymbolicPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = symbolic_paintable_dispose;
  object_class->set_property = symbolic_paintable_set_property;
  object_class->get_property = symbolic_paintable_get_property;

  g_object_class_install_property (object_class, PROP_FILE,
                                   g_param_spec_object ("file", NULL, NULL,
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}

GdkPaintable *
symbolic_paintable_new (GFile *file)
{
  return g_object_new (SYMBOLIC_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}
