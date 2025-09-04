#include "svgpaintable.h"

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

struct _SvgPaintable
{
  GObject parent_instance;
  GFile *file;
  GskRenderNode *node;
  double width;
  double height;
};

struct _SvgPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  NUM_PROPERTIES
};

/* {{{ Utilities */

/* Like gtk_snapshot_append_node, but transforms the node
 * to map the @from rect to the @to rect.
 */
static void
gtk_snapshot_append_node_scaled (GtkSnapshot     *snapshot,
                                 GskRenderNode   *node,
                                 graphene_rect_t *from,
                                 graphene_rect_t *to)
{
  if (graphene_rect_equal (from, to))
    {
      gtk_snapshot_append_node (snapshot, node);
    }
  else
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (to->origin.x,
                                                              to->origin.y));
      gtk_snapshot_scale (snapshot, to->size.width / from->size.width,
                                    to->size.height / from->size.height);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- from->origin.x,
                                                              - from->origin.y));
      gtk_snapshot_append_node (snapshot, node);
      gtk_snapshot_restore (snapshot);
    }
}

/* }}} */
/* {{{ SVG Handling */

static GskRenderNode *
render_node_from_svg (GFile  *file,
                      double *out_width,
                      double *out_height)
{
  RsvgHandle *handle;
  double width, height;
  GskRenderNode *node;
  cairo_t *cr;
  GError *error = NULL;

  handle = rsvg_handle_new_from_gfile_sync (file,
                                            RSVG_HANDLE_FLAGS_NONE,
                                            NULL,
                                            &error);
  if (!handle)
    {
      g_warning ("Failed to load SVG file: %s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  if (!rsvg_handle_get_intrinsic_size_in_pixels (handle, &width, &height))
    {
      gboolean has_viewbox;
      RsvgRectangle viewbox;

      rsvg_handle_get_intrinsic_dimensions (handle,
                                            NULL, NULL, NULL, NULL,
                                            &has_viewbox,
                                            &viewbox);

      if (has_viewbox)
        {
          width = viewbox.width;
          height = viewbox.height;
        }
      else
        {
          g_warning ("SVG without intrinsic size or viewbox");
          g_object_unref (handle);
          return NULL;
        }
    }

  node = gsk_cairo_node_new (&GRAPHENE_RECT_INIT (0, 0, width, height));
  cr = gsk_cairo_node_get_draw_context (node);

  if (!rsvg_handle_render_document (handle,
                                    cr,
                                    &(RsvgRectangle) { 0, 0, width, height },
                                    &error))
    {
      g_warning ("Failed to render SVG: %s", error->message);
      g_clear_error (&error);
      g_clear_pointer (&node, gsk_render_node_unref);
    }

  cairo_destroy (cr);
  g_object_unref (handle);

  *out_width = width;
  *out_height = height;

  return node;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
svg_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  double render_width;
  double render_height;
  graphene_rect_t icon_rect;
  graphene_rect_t render_rect;

  if (!self->file)
    return;

  if (self->width >= self->height)
    {
      render_width = width;
      render_height = height * (self->height / self->width);
    }
  else
    {
      render_width = width * (self->width / self->height);
      render_height = height;
    }

  graphene_rect_init (&icon_rect, 0, 0, self->width, self->height);
  graphene_rect_init (&render_rect,
                      (width - render_width) / 2,
                      (height - render_height) / 2,
                      render_width,
                      render_height);

  if (!self->node)
    {
      gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 238/255.0, 106/255.0, 167/255.0,  1 }, &GRAPHENE_RECT_INIT (0, 0, width, height));
    }
  else
    {
      gtk_snapshot_append_node_scaled (snapshot, self->node, &icon_rect, &render_rect);
    }
}

static int
svg_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return ceil (SVG_PAINTABLE (paintable)->width);
}

static int
svg_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return ceil (SVG_PAINTABLE (paintable)->height);
}

static void
svg_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = svg_paintable_snapshot;
  iface->get_intrinsic_width = svg_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = svg_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GObject boilerplate */

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
  g_clear_pointer (&self->node, gsk_render_node_unref);

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
      g_set_object (&self->file, g_value_get_object (value));
      g_clear_pointer (&self->node, gsk_render_node_unref);
      if (self->file)
        self->node = render_node_from_svg (self->file,
                                           &self->width,
                                           &self->height);
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
                                   g_param_spec_object ("file", NULL, NULL,
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}

/* }}} */
/* {{{ Public API */

SvgPaintable *
svg_paintable_new (GFile *file)
{
  return g_object_new (SVG_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}

/* }}} */

/* vim:set foldmethod=marker: */
