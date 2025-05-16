#include "symbolicpaintable.h"

#include <gtk/gtk.h>

struct _SymbolicPaintable
{
  GObject parent_instance;
  GFile *file;
  GskRenderNode *node;
  double width;
  double height;
};

struct _SymbolicPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  NUM_PROPERTIES
};

/* {{{ SVG Parser */

/* Not a complete SVG parser by any means.
 * We just handle what can be found in symbolic icons.
 */

typedef struct
{
  double width, height;
  GtkSnapshot *snapshot;
  gboolean has_clip;
} ParserData;

static void
start_element_cb (GMarkupParseContext  *context,
                  const gchar          *element_name,
                  const gchar         **attribute_names,
                  const gchar         **attribute_values,
                  gpointer              user_data,
                  GError              **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      char *end;

      for (int i = 0; attribute_names[i]; i++)
        {
          if (strcmp (attribute_names[i], "width") == 0)
            width_attr = attribute_values[i];
          else if (strcmp (attribute_names[i], "height") == 0)
            height_attr = attribute_values[i];
        }

      if (!width_attr)
        {
          g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                               "Missing attribute: width");
          return;
        }
      data->width = g_ascii_strtod (width_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid width attribute: %s", width_attr);
          return;
        }

      if (!height_attr)
        {
          g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                               "Missing attribute: height");
          return;
        }

      data->height = g_ascii_strtod (height_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid height attribute: %s", height_attr);
          return;
        }

      gtk_snapshot_push_clip (data->snapshot, &GRAPHENE_RECT_INIT (0, 0, data->width, data->height));
      data->has_clip = TRUE;
    }
  else if (strcmp (element_name, "g") == 0)
    {
      /* Do nothing */
    }
  else if (strcmp (element_name, "p") == 0 ||
           strcmp (element_name, "path") == 0)
    {
      const char *path_attr = NULL;
      const char *fill_rule_attr = NULL;
      const char *fill_opacity_attr = NULL;
      const char *opacity_attr = NULL;
      const char *class_attr = NULL;
      const char *style_attr = NULL;
      GskPath *path;
      GskFillRule fill_rule;
      double opacity;
      GdkRGBA color;
      char *end;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "d", &path_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-rule", &fill_rule_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-opacity", &fill_opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "opacity", &opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "class", &class_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "style", &style_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "id", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      if (fill_rule_attr && strcmp (fill_rule_attr, "evenodd") == 0)
        fill_rule = GSK_FILL_RULE_EVEN_ODD;
      else
        fill_rule = GSK_FILL_RULE_WINDING;

      opacity = 1;

      if (fill_opacity_attr)
        {
          opacity = g_ascii_strtod (fill_opacity_attr, &end);
          if (end && *end != '\0')
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid fill-opacity attribute: %s", fill_opacity_attr);
              return;
            }
        }
      else if (opacity_attr)
        {
          opacity = g_ascii_strtod (opacity_attr, &end);
          if (end && *end != '\0')
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid opacity attribute: %s", fill_opacity_attr);
              return;
            }
        }
      else if (style_attr)
        {
          char *p;

          p = strstr (style_attr, "opacity:");
          if (p)
            {
              opacity = g_ascii_strtod (p, &end);
              if (end && *end != '\0' && *end != ';')
                {
                  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                               "Failed to parse opacity in style attribute: %s", style_attr);
                  return;
                }
            }
        }

      if (!class_attr)
        class_attr = "foreground";

      if (strcmp (class_attr, "foreground") == 0)
        color = (GdkRGBA) { 0, 0, 0, opacity };
      else if (strcmp (class_attr, "success") == 0)
        color = (GdkRGBA) { 1, 0, 0, opacity };
      else if (strcmp (class_attr, "warning") == 0)
        color = (GdkRGBA) { 0, 1, 0, opacity };
      else if (strcmp (class_attr, "error") == 0)
        color = (GdkRGBA) { 0, 0, 1, opacity };
      else
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unsupported class: %s", class_attr);
          return;
        }

      path = gsk_path_parse (path_attr);
      if (!path)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Failed to parse path: %s", path_attr);
          return;
        }

      gtk_snapshot_append_fill (data->snapshot, path, fill_rule, &color);

      gsk_path_unref (path);
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled element: %s", element_name);
      return;
    }
}

static void
end_element_cb (GMarkupParseContext  *context,
                const gchar          *element_name,
                gpointer              user_data,
                GError              **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "svg") == 0)
    {
      if (data->has_clip)
        {
          gtk_snapshot_pop (data->snapshot);
          data->has_clip = FALSE;
        }
    }
}

static GskRenderNode *
parse_symbolic_svg (GBytes *bytes,
                    double *width,
                    double *height)
{
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    end_element_cb,
    NULL,
    NULL,
    NULL,
  };
  ParserData data;
  GError *error = NULL;
  const char *text;
  gsize len;

  data.width = data.height = 0;
  data.snapshot = gtk_snapshot_new ();
  data.has_clip = FALSE;

  text = g_bytes_get_data (bytes, &len);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context, text, len, &error))
    {
      GskRenderNode *node;

      g_error_free (error);

      g_markup_parse_context_free (context);
      if (data.has_clip)
        gtk_snapshot_pop (data.snapshot);
      node = gtk_snapshot_free_to_node (data.snapshot);
      g_clear_pointer (&node, gsk_render_node_unref);

      return NULL;
    }

  g_markup_parse_context_free (context);

  *width = data.width;
  *height = data.height;

  return gtk_snapshot_free_to_node (data.snapshot);
}

GskRenderNode *
render_node_from_symbolic (GFile  *file,
                           double *width,
                           double *height)
{
  GBytes *bytes;
  GskRenderNode *node;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (!bytes)
    return NULL;

  node = parse_symbolic_svg (bytes, width, height);
  g_bytes_unref (bytes);

  return node;
}

/* }}} */
/* {{{ Render node recoloring */

/* This recolors nodes that are produced from
 * symbolic icons: container, transform, fill, color
 *
 * It relies on the fact that the SVG parser uses
 * fixed RGBA values for the symbolic colors.
 */

static gboolean
recolor_node2 (GskRenderNode *node,
               const GdkRGBA  colors[4],
               GtkSnapshot   *snapshot)
{
  switch ((int) gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
        if (!recolor_node2 (gsk_container_node_get_child (node, i), colors, snapshot))
          return FALSE;
      return TRUE;
    case GSK_TRANSFORM_NODE:
      {
        gboolean ret;

        gtk_snapshot_save (snapshot);
        gtk_snapshot_transform (snapshot, gsk_transform_node_get_transform (node));
        ret = recolor_node2 (gsk_transform_node_get_child (node), colors, snapshot);
        gtk_snapshot_restore (snapshot);

        return ret;
      }
    case GSK_FILL_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_fill (snapshot,
                                gsk_fill_node_get_path (node),
                                gsk_fill_node_get_fill_rule (node));
        ret = recolor_node2 (gsk_fill_node_get_child (node), colors, snapshot);
        gtk_snapshot_pop (snapshot);

        return ret;
      }
      break;
    case GSK_COLOR_NODE:
      {
        graphene_rect_t bounds;
        GdkRGBA color;
        float alpha;

        gsk_render_node_get_bounds (node, &bounds);
        color = *gsk_color_node_get_color (node);

        /* Preserve the alpha that was set from fill-opacity */
        alpha = color.alpha;
        color.alpha = 1;

        if (gdk_rgba_equal (&color, &(GdkRGBA) { 0, 0, 0, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_FOREGROUND];
        else if (gdk_rgba_equal (&color, &(GdkRGBA) { 0, 0, 1, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_ERROR];
        else if (gdk_rgba_equal (&color, &(GdkRGBA) { 0, 1, 0, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_WARNING];
        else if (gdk_rgba_equal (&color, &(GdkRGBA) { 1, 0, 0, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_SUCCESS];

        color.alpha *= alpha;

        gtk_snapshot_append_color (snapshot, &color, &bounds);
      }
      return TRUE;
    default:
      return FALSE;
    }
}

static gboolean
recolor_node (GskRenderNode  *node,
              const GdkRGBA  *colors,
              gsize           n_colors,
              GskRenderNode **recolored)
{
  GtkSnapshot *snapshot;
  gboolean ret;

  snapshot = gtk_snapshot_new ();
  ret = recolor_node2 (node, colors, snapshot);
  *recolored = gtk_snapshot_free_to_node (snapshot);

  if (!ret)
    g_clear_pointer (recolored, gsk_render_node_unref);

  return ret;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
symbolic_paintable_snapshot (GdkPaintable *paintable,
                             GdkSnapshot  *snapshot,
                             double        width,
                             double        height)
{
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, width, height, NULL, 0);
}

static int
symbolic_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);

  return ceil (self->width);
}

static int
symbolic_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);

  return ceil (self->height);
}

static void
symbolic_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = symbolic_paintable_snapshot;
  iface->get_intrinsic_width = symbolic_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = symbolic_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

/* Like gtk_snapshot_append_node, but transforms the node
 * to make its bounds match the given rect
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


static void
symbolic_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                      GdkSnapshot          *snapshot,
                                      double                width,
                                      double                height,
                                      const GdkRGBA        *colors,
                                      gsize                 n_colors)
{
  SymbolicPaintable *self = SYMBOLIC_PAINTABLE (paintable);
  double render_width;
  double render_height;
  graphene_rect_t icon_rect;
  graphene_rect_t render_rect;
  GskRenderNode *recolored;

  if (!self->node)
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

  if (recolor_node (self->node, colors, n_colors, &recolored))
    {
      gtk_snapshot_append_node_scaled (snapshot, recolored, &icon_rect, &render_rect);
      gsk_render_node_unref (recolored);
    }
  else
    {
      gtk_snapshot_append_node_scaled (snapshot, self->node, &icon_rect, &render_rect);
    }
}

static void
symbolic_symbolic_paintable_init_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = symbolic_paintable_snapshot_symbolic;
}

/* }}} */
/* {{{ GObject boilerplate */

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
  g_clear_pointer (&self->node, gsk_render_node_unref);

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
      g_set_object (&self->file, g_value_get_object (value));
      g_clear_pointer (&self->node, gsk_render_node_unref);
      self->node = render_node_from_symbolic (self->file,
                                              &self->width,
                                              &self->height);
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

/* }}} */
/* {{{ Public API */

GdkPaintable *
symbolic_paintable_new (GFile *file)
{
  return g_object_new (SYMBOLIC_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}

/* }}} */

/* vim:set foldmethod=marker: */
