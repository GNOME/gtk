#include "svgsymbolicpaintable.h"

#include <gtk/gtk.h>

struct _SvgSymbolicPaintable
{
  GObject parent_instance;
  GFile *file;
  GskRenderNode *node;
  double width;
  double height;
};

struct _SvgSymbolicPaintableClass
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
set_attribute_error (GError     **error,
                     const char  *name,
                     const char  *value)
{
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
               "Could not handle %s attribute: %s", name, value);
}

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
          set_attribute_error (error, "width", width_attr);
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
          set_attribute_error (error, "height", height_attr);
          return;
        }

      gtk_snapshot_push_clip (data->snapshot, &GRAPHENE_RECT_INIT (0, 0, data->width, data->height));
      data->has_clip = TRUE;
    }
  else if (strcmp (element_name, "g") == 0)
    {
      /* Do nothing */
    }
  else if (strcmp (element_name, "path") == 0)
    {
      const char *path_attr = NULL;
      const char *fill_rule_attr = NULL;
      const char *fill_opacity_attr = NULL;
      const char *stroke_width_attr = NULL;
      const char *stroke_opacity_attr = NULL;
      const char *stroke_linecap_attr = NULL;
      const char *stroke_linejoin_attr = NULL;
      const char *stroke_miterlimit_attr = NULL;
      const char *stroke_dasharray_attr = NULL;
      const char *stroke_dashoffset_attr = NULL;
      const char *opacity_attr = NULL;
      const char *class_attr = NULL;
      GskPath *path = NULL;
      GskStroke *stroke = NULL;
      GskFillRule fill_rule;
      double opacity;
      double fill_opacity;
      double stroke_opacity;
      GdkRGBA fill_color;
      GdkRGBA stroke_color;
      char *end;
      gboolean do_fill = FALSE;
      gboolean do_stroke = FALSE;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "d", &path_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "class", &class_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "opacity", &opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-rule", &fill_rule_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-opacity", &fill_opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-width", &stroke_width_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-opacity", &stroke_opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-linecap", &stroke_linecap_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-linejoin", &stroke_linejoin_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-miterlimit", &stroke_miterlimit_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-dasharray", &stroke_dasharray_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-dashoffset", &stroke_dashoffset_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "style", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "id", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "color", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "overflow", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      path = gsk_path_parse (path_attr);
      if (!path)
        {
          set_attribute_error (error, "d", path_attr);
          return;
        }

      fill_opacity = 1;
      if (fill_opacity_attr)
        {
          fill_opacity = g_ascii_strtod (fill_opacity_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "fill-opacity", fill_opacity_attr);
              goto cleanup;
            }
          fill_opacity = CLAMP (fill_opacity, 0, 1);
        }

      stroke_opacity = 1;
      if (stroke_opacity_attr)
        {
          stroke_opacity = g_ascii_strtod (stroke_opacity_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "stroke-opacity", stroke_opacity_attr);
              goto cleanup;
            }
          stroke_opacity = CLAMP (stroke_opacity, 0, 1);
        }

      if (!class_attr)
        {
          fill_color = (GdkRGBA) { 0, 0, 0, fill_opacity };
          do_fill = TRUE;
          do_stroke = FALSE;
        }
      else
        {
          const char * const *classes;

          classes = (const char * const *) g_strsplit (class_attr, " ", 0);

          if (g_strv_contains (classes, "transparent-fill"))
            {
              do_fill = FALSE;
            }
          else if (g_strv_contains (classes, "foreground-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 0, 0, fill_opacity };
            }
          else if (g_strv_contains (classes, "success") ||
                   g_strv_contains (classes, "success-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 1, 0, 0, fill_opacity };
            }
          else if (g_strv_contains (classes, "warning") ||
                   g_strv_contains (classes, "warning-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 1, 0, 1 };
            }
          else if (g_strv_contains (classes, "error") ||
                   g_strv_contains (classes, "error-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 0, 1, fill_opacity };
            }
          else
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 0, 0, fill_opacity };
            }

          if (g_strv_contains (classes, "success-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 1, 0, 0, stroke_opacity };
            }
          else if (g_strv_contains (classes, "warning-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 0, 1, 0, stroke_opacity };
            }
          else if (g_strv_contains (classes, "error-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 0, 0, 1, stroke_opacity };
            }
          else if (g_strv_contains (classes, "foreground-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 0, 0, 0, stroke_opacity };
            }
          else
            {
              do_stroke = FALSE;
            }

          g_strfreev ((char **) classes);
        }

      opacity = 1;
      if (opacity_attr)
        {
          opacity = g_ascii_strtod (opacity_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "opacity", opacity_attr);
              goto cleanup;
            }
        }

      if (fill_rule_attr && strcmp (fill_rule_attr, "evenodd") == 0)
        fill_rule = GSK_FILL_RULE_EVEN_ODD;
      else
        fill_rule = GSK_FILL_RULE_WINDING;

      stroke = gsk_stroke_new (1);

      if (stroke_width_attr)
        {
          double w = g_ascii_strtod (stroke_width_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "stroke-width", stroke_width_attr);
              goto cleanup;
            }

          gsk_stroke_set_line_width (stroke, w);
        }

      if (stroke_linecap_attr)
        {
          if (strcmp (stroke_linecap_attr, "butt") == 0)
            gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_BUTT);
          else if (strcmp (stroke_linecap_attr, "round") == 0)
            gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
          else if (strcmp (stroke_linecap_attr, "square") == 0)
            gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_SQUARE);
          else
            {
              set_attribute_error (error, "stroke-linecap", stroke_linecap_attr);
              goto cleanup;
            }
        }

      if (stroke_linejoin_attr)
        {
          if (strcmp (stroke_linejoin_attr, "miter") == 0)
            gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_MITER);
          else if (strcmp (stroke_linejoin_attr, "round") == 0)
            gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);
          else if (strcmp (stroke_linejoin_attr, "bevel") == 0)
            gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_BEVEL);
          else
            {
              set_attribute_error (error, "stroke-linejoin", stroke_linejoin_attr);
              goto cleanup;
            }
        }

      if (stroke_miterlimit_attr)
        {
          double ml = g_ascii_strtod (stroke_miterlimit_attr, &end);
          if ((end && *end != '\0') || ml < 1)
            {
              set_attribute_error (error, "stroke-miterlimit", stroke_miterlimit_attr);
              goto cleanup;
            }

          gsk_stroke_set_miter_limit (stroke, ml);
        }

      if (stroke_dasharray_attr &&
          strcmp (stroke_dasharray_attr, "none") != 0)
        {
          char **str;
          gsize n_dash;

          str = g_strsplit_set (stroke_dasharray_attr, ", ", 0);

          n_dash = g_strv_length (str);
          if (n_dash > 0)
            {
              float *dash = g_newa (float, n_dash);

              for (int i = 0; i < n_dash; i++)
                {
                  dash[i] = g_ascii_strtod (str[i], &end);
                  if (end && *end != '\0')
                    {
                      set_attribute_error (error, "stroke-dasharray", stroke_dasharray_attr);
                      g_strfreev (str);
                      goto cleanup;
                    }
                }

              gsk_stroke_set_dash (stroke, dash, n_dash);
            }

          g_strfreev (str);
        }

      if (stroke_dashoffset_attr)
        {
          double offset = g_ascii_strtod (stroke_dashoffset_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "stroke-dashoffset", stroke_dashoffset_attr);
              goto cleanup;
            }

          gsk_stroke_set_dash_offset (stroke, offset);
        }

      if (opacity != 1)
        gtk_snapshot_push_opacity (data->snapshot, opacity);

      if (do_fill)
        gtk_snapshot_append_fill (data->snapshot, path, fill_rule, &fill_color);

      if (do_stroke)
        gtk_snapshot_append_stroke (data->snapshot, path, stroke, &stroke_color);

      if (opacity != 1)
        gtk_snapshot_pop (data->snapshot);

cleanup:
      g_clear_pointer (&path, gsk_path_unref);
      g_clear_pointer (&stroke, gsk_stroke_free);
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled element: %s", element_name);
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
parse_symbolic_svg (GBytes  *bytes,
                    double  *width,
                    double  *height,
                    GError **error)
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
  const char *text;
  gsize length;

  data.width = data.height = 0;
  data.snapshot = gtk_snapshot_new ();
  data.has_clip = FALSE;

  text = g_bytes_get_data (bytes, &length);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context, text, length, error))
    {
      GskRenderNode *node;

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

static char *
file_path (GFile *file)
{
  char *path = g_file_get_path (file);
  if (!path)
    path = g_file_get_uri (file);
  return path;
}

static GskRenderNode *
render_node_from_symbolic (GFile  *file,
                           double *width,
                           double *height)
{
  GBytes *bytes;
  GskRenderNode *node;
  GError *error = NULL;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (!bytes)
    {
      char *path = file_path (file);
      g_warning ("Failed to load %s: %s", path, error->message);
      g_free (path);
      g_error_free (error);
      return NULL;
    }

  node = parse_symbolic_svg (bytes, width, height, &error);
  if (!node)
    {
      char *path = file_path (file);
      g_warning ("Failed to parse %s: %s", path, error->message);
      g_free (path);
      g_error_free (error);
    }

  g_bytes_unref (bytes);

  return node;
}

/* }}} */
/* {{{ Render node recoloring */

/* This recolors nodes that are produced from symbolic
 * icons: container, clip, transform, fill, stroke, color
 *
 * It relies on the fact that the SVG parser uses
 * fixed RGBA values for the symbolic colors.
 */

static gboolean
recolor_node2 (GskRenderNode  *node,
               const GdkRGBA   colors[4],
               GtkSnapshot    *snapshot,
               GError        **error)
{
  switch ((int) gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
        if (!recolor_node2 (gsk_container_node_get_child (node, i), colors, snapshot, error))
          return FALSE;
      return TRUE;

    case GSK_TRANSFORM_NODE:
      {
        gboolean ret;

        gtk_snapshot_save (snapshot);
        gtk_snapshot_transform (snapshot, gsk_transform_node_get_transform (node));
        ret = recolor_node2 (gsk_transform_node_get_child (node), colors, snapshot, error);
        gtk_snapshot_restore (snapshot);

        return ret;
      }

    case GSK_CLIP_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_clip (snapshot, gsk_clip_node_get_clip (node));
        ret = recolor_node2 (gsk_clip_node_get_child (node), colors, snapshot, error);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

    case GSK_OPACITY_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_opacity (snapshot, gsk_opacity_node_get_opacity (node));
        ret = recolor_node2 (gsk_opacity_node_get_child (node), colors, snapshot, error);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

    case GSK_FILL_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_fill (snapshot,
                                gsk_fill_node_get_path (node),
                                gsk_fill_node_get_fill_rule (node));
        ret = recolor_node2 (gsk_fill_node_get_child (node), colors, snapshot, error);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

    case GSK_STROKE_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_stroke (snapshot,
                                  gsk_stroke_node_get_path (node),
                                  gsk_stroke_node_get_stroke (node));
        ret = recolor_node2 (gsk_stroke_node_get_child (node), colors, snapshot, error);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

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
      {
        char *name = g_enum_to_string (GSK_TYPE_RENDER_NODE_TYPE,
                                       gsk_render_node_get_node_type (node));
        g_set_error (error,
                     G_IO_ERROR, G_IO_ERROR_FAILED,
                     "Unsupported node type %s", name);
        g_free (name);
      }
      return FALSE;

    }
}

static gboolean
recolor_node (GskRenderNode  *node,
              const GdkRGBA  *colors,
              gsize           n_colors,
              GskRenderNode **recolored,
              GError        **error)
{
  GtkSnapshot *snapshot;
  gboolean ret;

  snapshot = gtk_snapshot_new ();
  ret = recolor_node2 (node, colors, snapshot, error);
  *recolored = gtk_snapshot_free_to_node (snapshot);

  if (!ret)
    g_clear_pointer (recolored, gsk_render_node_unref);

  return ret;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
svg_symbolic_paintable_snapshot (GdkPaintable *paintable,
                                 GdkSnapshot  *snapshot,
                                 double        width,
                                 double        height)
{
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, width, height, NULL, 0);
}

static int
svg_symbolic_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return ceil (SVG_SYMBOLIC_PAINTABLE (paintable)->width);
}

static int
svg_symbolic_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return ceil (SVG_SYMBOLIC_PAINTABLE (paintable)->height);
}

static void
svg_symbolic_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = svg_symbolic_paintable_snapshot;
  iface->get_intrinsic_width = svg_symbolic_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = svg_symbolic_paintable_get_intrinsic_height;
}

/* }}} */
 /* {{{ GtkSymbolicPaintable implementation */

static void
svg_symbolic_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                          GdkSnapshot          *snapshot,
                                          double                width,
                                          double                height,
                                          const GdkRGBA        *colors,
                                          gsize                 n_colors)
{
  SvgSymbolicPaintable *self = SVG_SYMBOLIC_PAINTABLE (paintable);
  double render_width;
  double render_height;
  graphene_rect_t icon_rect;
  graphene_rect_t render_rect;
  GskRenderNode *recolored;
  GError *error = NULL;

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
  else if (!recolor_node (self->node, colors, n_colors, &recolored, &error))
    {
      char *path = file_path (self->file);
      g_warning ("Failed to recolor %s: %s", path, error->message);
      g_free (path);
      g_error_free (error);
      gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 238/255.0, 106/255.0, 167/255.0,  1 }, &render_rect);
    }
  else
    {
      gtk_snapshot_append_node_scaled (snapshot, recolored, &icon_rect, &render_rect);
      gsk_render_node_unref (recolored);
    }
}

static void
svg_symbolic_symbolic_paintable_init_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = svg_symbolic_paintable_snapshot_symbolic;
}

/* }}} */
/* {{{ GObject boilerplate */

G_DEFINE_TYPE_WITH_CODE (SvgSymbolicPaintable, svg_symbolic_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                svg_symbolic_paintable_init_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                svg_symbolic_symbolic_paintable_init_interface))

static void
svg_symbolic_paintable_init (SvgSymbolicPaintable *self)
{
}

static void
svg_symbolic_paintable_dispose (GObject *object)
{
  SvgSymbolicPaintable *self = SVG_SYMBOLIC_PAINTABLE (object);

  g_clear_object (&self->file);
  g_clear_pointer (&self->node, gsk_render_node_unref);

  G_OBJECT_CLASS (svg_symbolic_paintable_parent_class)->dispose (object);
}

static void
svg_symbolic_paintable_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  SvgSymbolicPaintable *self = SVG_SYMBOLIC_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      g_clear_pointer (&self->node, gsk_render_node_unref);
      if (self->file)
        self->node = render_node_from_symbolic (self->file,
                                                &self->width,
                                                &self->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
svg_symbolic_paintable_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  SvgSymbolicPaintable *self = SVG_SYMBOLIC_PAINTABLE (object);

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
svg_symbolic_paintable_class_init (SvgSymbolicPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = svg_symbolic_paintable_dispose;
  object_class->set_property = svg_symbolic_paintable_set_property;
  object_class->get_property = svg_symbolic_paintable_get_property;

  g_object_class_install_property (object_class, PROP_FILE,
                                   g_param_spec_object ("file", NULL, NULL,
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}

/* }}} */
/* {{{ Public API */

SvgSymbolicPaintable *
svg_symbolic_paintable_new (GFile *file)
{
  return g_object_new (SVG_TYPE_SYMBOLIC_PAINTABLE,
                       "file", file,
                       NULL);
}

/* }}} */

/* vim:set foldmethod=marker: */
