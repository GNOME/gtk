/* GtkIconTheme - a loader for icon themes
 * gtk-icon-theme.c Copyright (C) 2002, 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>

#include "gtkiconpaintableprivate.h"

#include "gtkiconthemeprivate.h"
#include "gtksnapshotprivate.h"
#include "gtksymbolicpaintable.h"
#include "gdktextureutilsprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkprofilerprivate.h"


/**
 * GtkIconPaintable:
 *
 * Contains information found when looking up an icon in `GtkIconTheme`
 * or loading it from a file.
 *
 * `GtkIconPaintable` implements `GdkPaintable` and `GtkSymbolicPaintable`.
 */

/* {{{ Utilities */

static inline gboolean
icon_uri_is_symbolic (const char *icon_name,
                      int         icon_name_len)
{
  if (g_str_has_suffix (icon_name, "-symbolic.svg") ||
      g_str_has_suffix (icon_name, ".symbolic.png") ||
      g_str_has_suffix (icon_name, "-symbolic-ltr.svg") ||
      g_str_has_suffix (icon_name, "-symbolic-rtl.svg"))
    return TRUE;

  return FALSE;
}

static void
gtk_icon_paintable_set_file (GtkIconPaintable *icon,
                             GFile            *file)
{
  if (!file)
    return;

  icon->loadable = G_LOADABLE_ICON (g_file_icon_new (file));
  icon->is_resource = g_file_has_uri_scheme (file, "resource");

  if (icon->is_resource)
    {
      char *uri = g_file_get_uri (file);
      icon->filename = g_strdup (uri + strlen ("resource://"));
      g_free (uri);
    }
  else
    {
      icon->filename = g_file_get_path (file);
    }

  icon->is_svg = g_str_has_suffix (icon->filename, ".svg");
  icon->is_symbolic = icon_uri_is_symbolic (icon->filename, -1);
}

static GFile *
new_resource_file (const char *filename)
{
  char *escaped = g_uri_escape_string (filename,
                                       G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  char *uri = g_strconcat ("resource://", escaped, NULL);
  GFile *file = g_file_new_for_uri (uri);

  g_free (escaped);
  g_free (uri);

  return file;
}

static GskRenderNode *
enforce_logical_size (GskRenderNode *node,
                      double         width,
                      double         height)
{
  GskRenderNode *nodes[2];

  nodes[0] = gsk_color_node_new (&(GdkRGBA) { 0, 0, 0, 0 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height));
  nodes[1] = node;

  node = gsk_container_node_new (nodes, 2);

  gsk_render_node_unref (nodes[0]);
  gsk_render_node_unref (nodes[1]);

  return node;
}

/* }}} */
/* {{{ Icon loading */

/* This function contains the complicated logic for deciding
 * on the size at which to load the icon and loading it at
 * that size.
 */
static void
icon_ensure_node__locked (GtkIconPaintable *icon,
                          gboolean          in_thread)
{
  gint64 before;
  int pixel_size;
  GError *load_error = NULL;
  GdkTexture *texture = NULL;
  gboolean only_fg = FALSE;
  gboolean single_path = FALSE;

  icon_cache_mark_used_if_cached (icon);

  if (icon->node)
    return;

  before = GDK_PROFILER_CURRENT_TIME;

  /* This is the natural pixel size for the requested icon size + scale in this directory.
   * We precalculate this so we can use it as a rasterization size for svgs.
   */
  pixel_size = icon->desired_size * icon->desired_scale;

  /* At this point, we need to actually get the icon; either from the
   * builtin image or by loading the file
   */
  if (icon->is_resource)
    {
      if (icon->is_svg)
        {
          if (icon->is_symbolic)
            {
              if (icon->allow_node)
                icon->node = gsk_render_node_new_from_resource_symbolic (icon->filename,
                                                                         &only_fg,
                                                                         &single_path,
                                                                         &icon->width,
                                                                         &icon->height);
              if (!icon->node)
                texture = gdk_texture_new_from_resource_symbolic (icon->filename,
                                                                  pixel_size, pixel_size,
                                                                  &only_fg,
                                                                  &load_error);
            }
          else
            texture = gdk_texture_new_from_resource_at_scale (icon->filename,
                                                              pixel_size, pixel_size,
                                                              &only_fg,
                                                              &load_error);
        }
      else
        texture = gdk_texture_new_from_resource_with_fg (icon->filename, &only_fg);
    }
  else if (icon->filename)
    {
      if (icon->is_svg)
        {
          if (icon->is_symbolic)
            {
              if (icon->allow_node)
                icon->node = gsk_render_node_new_from_filename_symbolic (icon->filename,
                                                                         &only_fg,
                                                                         &single_path,
                                                                         &icon->width,
                                                                         &icon->height);
              if (!icon->node)
                texture = gdk_texture_new_from_filename_symbolic (icon->filename,
                                                                  pixel_size, pixel_size,
                                                                  &only_fg,
                                                                  &load_error);
            }
          else
            {
              GFile *file = g_file_new_for_path (icon->filename);
              GInputStream *stream = G_INPUT_STREAM (g_file_read (file, NULL, &load_error));

              if (stream)
                {
                  texture = gdk_texture_new_from_stream_at_scale (stream,
                                                                  pixel_size, pixel_size,
                                                                  &only_fg,
                                                                  NULL,
                                                                  &load_error);
                  g_object_unref (stream);
                }

              g_object_unref (file);
            }
        }
      else
        {
          texture = gdk_texture_new_from_filename_with_fg (icon->filename, &only_fg, &load_error);
        }
    }
  else
    {
      GInputStream *stream;

      g_assert (icon->loadable);

      stream = g_loadable_icon_load (icon->loadable, pixel_size, NULL, NULL, &load_error);
      if (stream)
        {
          /* SVG icons are a special case - we just immediately scale them
           * to the desired size
           */
          if (icon->is_svg)
            texture = gdk_texture_new_from_stream_at_scale (stream,
                                                            pixel_size, pixel_size,
                                                            &only_fg,
                                                            NULL,
                                                            &load_error);
          else
            texture = gdk_texture_new_from_stream_with_fg (stream, &only_fg, NULL, &load_error);

          g_object_unref (stream);
        }
    }

  icon->only_fg = only_fg;
  icon->single_path = single_path;

  if (icon->node)
    {
      g_assert (texture == NULL);
    }
  else
    {
      if (!texture)
        {
          g_warning ("Failed to load icon %s: %s", icon->filename, load_error ? load_error->message : "");
          g_clear_error (&load_error);
          texture = gdk_texture_new_from_resource (IMAGE_MISSING_RESOURCE_PATH);
          g_set_str (&icon->icon_name, "image-missing");
          icon->is_symbolic = FALSE;
          icon->only_fg = FALSE;
        }

      icon->width = gdk_texture_get_width (texture);
      icon->height = gdk_texture_get_height (texture);
      icon->node = gsk_texture_node_new (texture, &GRAPHENE_RECT_INIT (0, 0,
                                                                       icon->width,
                                                                       icon->height));
      g_object_unref (texture);
    }

  if (GDK_PROFILER_IS_RUNNING)
    {
      gint64 end = GDK_PROFILER_CURRENT_TIME;
      /* Don't report quick (< 0.5 msec) parses */
      if (end - before > 500000 || !in_thread)
        {
          gdk_profiler_add_markf (before, (end - before), in_thread ?  "Icon load (thread)" : "Icon load" ,
                                  "%s size %d@%d", icon->filename, icon->desired_size, icon->desired_scale);
        }
    }
}

static GskRenderNode *
gtk_icon_paintable_ensure_node (GtkIconPaintable *self)
{
  GskRenderNode *node = NULL;

  g_mutex_lock (&self->texture_lock);

  icon_ensure_node__locked (self, FALSE);

  node = self->node;

  g_mutex_unlock (&self->texture_lock);

  g_assert (node != NULL);

  return node;
}

/* }}} */
/* {{{ Recoloring by color matrix */

static void
init_color_matrix (graphene_matrix_t *color_matrix,
                   graphene_vec4_t   *color_offset,
                   const GdkRGBA     *foreground_color,
                   const GdkRGBA     *success_color,
                   const GdkRGBA     *warning_color,
                   const GdkRGBA     *error_color)
{
  const GdkRGBA fg_default = { 0.7450980392156863, 0.7450980392156863, 0.7450980392156863, 1.0};
  const GdkRGBA success_default = { 0.3046921492332342,0.6015716792553597, 0.023437857633325704, 1.0};
  const GdkRGBA warning_default = {0.9570458533607996, 0.47266346227206835, 0.2421911955443656, 1.0 };
  const GdkRGBA error_default = { 0.796887159533074, 0 ,0, 1.0 };
  const GdkRGBA *fg = foreground_color ? foreground_color : &fg_default;
  const GdkRGBA *sc = success_color ? success_color : &success_default;
  const GdkRGBA *wc = warning_color ? warning_color : &warning_default;
  const GdkRGBA *ec = error_color ? error_color : &error_default;

  graphene_matrix_init_from_float (color_matrix,
                                   (float[16]) {
                                     sc->red - fg->red, sc->green - fg->green, sc->blue - fg->blue, 0,
                                     wc->red - fg->red, wc->green - fg->green, wc->blue - fg->blue, 0,
                                     ec->red - fg->red, ec->green - fg->green, ec->blue - fg->blue, 0,
                                     0, 0, 0, fg->alpha
                                   });
  graphene_vec4_init (color_offset, fg->red, fg->green, fg->blue, 0);
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static void
gtk_icon_paintable_snapshot_with_weight (GtkSymbolicPaintable *paintable,
                                         GtkSnapshot          *snapshot,
                                         double                width,
                                         double                height,
                                         const GdkRGBA        *colors,
                                         gsize                 n_colors,
                                         double                weight)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (paintable);
  GskRenderNode *node, *recolored;
  double render_width;
  double render_height;
  graphene_rect_t icon_rect;
  graphene_rect_t render_rect;
  gboolean colors_opaque;

  node = gtk_icon_paintable_ensure_node (icon);
  gsk_render_node_ref (node);

  if (icon->width >= icon->height)
    {
      render_width = width;
      render_height = height * (icon->height / icon->width);
    }
  else
    {
      render_width = width * (icon->width / icon->height);
      render_height = height;
    }

  graphene_rect_init (&icon_rect, 0, 0, icon->width, icon->height);
  graphene_rect_init (&render_rect,
                      (width - render_width) / 2,
                      (height - render_height) / 2,
                      render_width,
                      render_height);

  if (icon->only_fg)
    colors_opaque = gdk_rgba_is_opaque (&colors[GTK_SYMBOLIC_COLOR_FOREGROUND]);
  else
    colors_opaque = gdk_rgba_is_opaque (&colors[GTK_SYMBOLIC_COLOR_FOREGROUND]) &&
                    gdk_rgba_is_opaque (&colors[GTK_SYMBOLIC_COLOR_SUCCESS]) &&
                    gdk_rgba_is_opaque (&colors[GTK_SYMBOLIC_COLOR_WARNING]) &&
                    gdk_rgba_is_opaque (&colors[GTK_SYMBOLIC_COLOR_ERROR]);

  if (icon->is_symbolic && icon->allow_recolor &&
      (icon->single_path || colors_opaque) &&
      gsk_render_node_recolor (node, colors, n_colors, weight, &recolored))
    {
      g_debug ("snapshot symbolic icon as recolored node");
      recolored = enforce_logical_size (recolored, icon->width, icon->height);

      gtk_snapshot_append_node_scaled (snapshot, recolored, &icon_rect, &render_rect);
      gsk_render_node_unref (recolored);
    }
  else if (icon->is_symbolic && icon->only_fg && icon->allow_mask)
    {
      g_debug ("snapshot symbolic icon %s using mask",
               gsk_render_node_get_node_type (node) == GSK_TEXTURE_NODE
               ? "as texture" : "as node");
      if (gsk_render_node_get_node_type (node) != GSK_TEXTURE_NODE)
        node = enforce_logical_size (node, icon->width, icon->height);

      gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
      gtk_snapshot_append_node_scaled (snapshot, node, &icon_rect, &render_rect);
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_append_color (snapshot, &colors[0], &render_rect);
      gtk_snapshot_pop (snapshot);
    }
  else if (icon->is_symbolic)
    {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;

      g_debug ("snapshot symbolic icon %s using color-matrix",
               gsk_render_node_get_node_type (node) == GSK_TEXTURE_NODE
               ? "as texture" : "as node");
      if (gsk_render_node_get_node_type (node) != GSK_TEXTURE_NODE)
        node = enforce_logical_size (node, icon->width, icon->height);

      init_color_matrix (&matrix, &offset,
                         &colors[GTK_SYMBOLIC_COLOR_FOREGROUND],
                         &colors[GTK_SYMBOLIC_COLOR_SUCCESS],
                         &colors[GTK_SYMBOLIC_COLOR_WARNING],
                         &colors[GTK_SYMBOLIC_COLOR_ERROR]);

      gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
      gtk_snapshot_append_node_scaled (snapshot, node, &icon_rect, &render_rect);
      gtk_snapshot_pop (snapshot);
    }
  else
    {
      gtk_snapshot_append_node_scaled (snapshot, node, &icon_rect, &render_rect);
    }

  gsk_render_node_unref (node);
}

static void
gtk_icon_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                      GtkSnapshot          *snapshot,
                                      double                width,
                                      double                height,
                                      const GdkRGBA        *colors,
                                      gsize                 n_colors)
{
  gtk_icon_paintable_snapshot_with_weight (paintable, snapshot,
                                           width, height,
                                           colors, n_colors,
                                           400);
}

static void
icon_symbolic_paintable_init (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_with_weight = gtk_icon_paintable_snapshot_with_weight;
  iface->snapshot_symbolic = gtk_icon_paintable_snapshot_symbolic;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
icon_paintable_snapshot (GdkPaintable *paintable,
                         GtkSnapshot  *snapshot,
                         double        width,
                         double        height)
{
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, width, height, NULL, 0);
}

static GdkPaintableFlags
icon_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS;
}

static int
icon_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (paintable);

  return icon->desired_size;
}

static int
icon_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (paintable);

  return icon->desired_size;
}

static void
icon_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = icon_paintable_snapshot;
  iface->get_flags = icon_paintable_get_flags;
  iface->get_intrinsic_width = icon_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = icon_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GObject boilerplate */

struct _GtkIconPaintableClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_FILE,
  PROP_ICON_NAME,
  PROP_IS_SYMBOLIC,
  PROP_SIZE,
  PROP_SCALE,
};

G_DEFINE_TYPE_WITH_CODE (GtkIconPaintable, gtk_icon_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                icon_paintable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                icon_symbolic_paintable_init))

static void
gtk_icon_paintable_init (GtkIconPaintable *icon)
{
  g_mutex_init (&icon->texture_lock);

  icon->allow_node = 1;
  icon->allow_recolor = 1;
  icon->allow_mask = 1;

  icon->desired_size = 16;
  icon->desired_scale = 1;
}

static void
gtk_icon_paintable_finalize (GObject *object)
{
  GtkIconPaintable *icon = (GtkIconPaintable *) object;

  icon_cache_remove (icon);

  g_strfreev (icon->key.icon_names);

  g_free (icon->filename);
  g_free (icon->icon_name);

  g_clear_object (&icon->loadable);
  g_clear_pointer (&icon->node, gsk_render_node_unref);

  g_mutex_clear (&icon->texture_lock);

  G_OBJECT_CLASS (gtk_icon_paintable_parent_class)->finalize (object);
}

static void
gtk_icon_paintable_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, gtk_icon_paintable_get_file (icon));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, icon->icon_name);
      break;

    case PROP_IS_SYMBOLIC:
      g_value_set_boolean (value, icon->is_symbolic);
      break;

    case PROP_SIZE:
      g_value_set_int (value, icon->desired_size);
      break;

    case PROP_SCALE:
      g_value_set_int (value, icon->desired_scale);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_icon_paintable_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gtk_icon_paintable_set_file (icon, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      icon->icon_name = g_value_dup_string (value);
      break;

    case PROP_IS_SYMBOLIC:
      if (icon->is_symbolic != g_value_get_boolean (value))
        {
          icon->is_symbolic = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SIZE:
      if (icon->desired_size != g_value_get_int (value))
        {
          icon->desired_size = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SCALE:
      if (icon->desired_scale != g_value_get_int (value))
        {
          icon->desired_scale = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
gtk_icon_paintable_class_init (GtkIconPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_icon_paintable_get_property;
  gobject_class->set_property = gtk_icon_paintable_set_property;
  gobject_class->finalize = gtk_icon_paintable_finalize;

  /**
   * GtkIconPaintable:file:
   *
   * The file representing the icon, if any.
   */
  g_object_class_install_property (gobject_class, PROP_FILE,
                                   g_param_spec_object ("file", NULL, NULL,
                                                        G_TYPE_FILE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));

  /**
   * GtkIconPaintable:icon-name:
   *
   * The icon name that was chosen during lookup.
   *
   * Deprecated: 4.20
   */
  g_object_class_install_property (gobject_class, PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_DEPRECATED));

  /**
   * GtkIconPaintable:is-symbolic: (getter is_symbolic)
   *
   * Whether the icon is symbolic or not.
   *
   * Deprecated: 4.20
   */
  g_object_class_install_property (gobject_class, PROP_IS_SYMBOLIC,
                                   g_param_spec_boolean ("is-symbolic", NULL, NULL,
                                                        FALSE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_DEPRECATED));

  g_object_class_install_property (gobject_class, PROP_SIZE,
                                   g_param_spec_int ("size", NULL, NULL,
                                                     0, G_MAXINT, 16,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class, PROP_SCALE,
                                   g_param_spec_int ("scale", NULL, NULL,
                                                     0, G_MAXINT, 1,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY));
}

/* }}} */
/* {{{ Private API */

void
gtk_icon_paintable_load_in_thread (GtkIconPaintable *self)
{
  g_mutex_lock (&self->texture_lock);
  icon_ensure_node__locked (self, TRUE);
  g_mutex_unlock (&self->texture_lock);
}

void
gtk_icon_paintable_set_debug (GtkIconPaintable *icon,
                              gboolean          allow_node,
                              gboolean          allow_recolor,
                              gboolean          allow_mask)
{
  icon->allow_node = allow_node;
  icon->allow_recolor = allow_recolor;
  icon->allow_mask = allow_mask;
}

void
gtk_icon_paintable_set_icon_name (GtkIconPaintable *icon,
                                  const char       *name)
{
  g_set_str (&icon->icon_name, name);
}

GtkIconPaintable *
gtk_icon_paintable_new_for_texture (GdkTexture *texture,
                                    int         desired_size,
                                    int         desired_scale)
{
  GtkIconPaintable *icon;

  icon = g_object_new (GTK_TYPE_ICON_PAINTABLE, NULL);
  icon->desired_size = desired_size;
  icon->desired_scale = desired_scale;
  icon->width = gdk_texture_get_width (texture);
  icon->height = gdk_texture_get_height (texture);
  icon->node = gsk_texture_node_new (texture,
                                     &GRAPHENE_RECT_INIT (0, 0,
                                                          icon->width,
                                                          icon->height));

  return icon;
}

GtkIconPaintable *
gtk_icon_paintable_new_for_path (const char *path,
                                 gboolean    is_resource,
                                 int         desired_size,
                                 int         desired_scale)
{
  GtkIconPaintable *icon;

  icon = g_object_new (GTK_TYPE_ICON_PAINTABLE, NULL);
  icon->desired_size = desired_size;
  icon->desired_scale = desired_scale;
  icon->filename = g_strdup (path);
  icon->is_resource = is_resource;
  icon->is_svg = g_str_has_suffix (icon->filename, ".svg");
  icon->is_symbolic = icon_uri_is_symbolic (icon->filename, -1);

  return icon;
}

GtkIconPaintable *
gtk_icon_paintable_new_for_loadable (GLoadableIcon *loadable,
                                     int            desired_size,
                                     int            desired_scale)
{
  GtkIconPaintable *icon;

  icon = g_object_new (GTK_TYPE_ICON_PAINTABLE, NULL);
  icon->desired_size = desired_size;
  icon->desired_scale = desired_scale;
  icon->loadable = g_object_ref (loadable);

  return icon;
}

/* }}} */
/* {{{ Public API */

/**
 * gtk_icon_paintable_new_for_file:
 * @file: a `GFile`
 * @size: desired icon size, in application pixels
 * @scale: the desired scale
 *
 * Creates a `GtkIconPaintable` for a file with a given size and scale.
 *
 * The icon can then be rendered by using it as a `GdkPaintable`.
 *
 * Returns: (transfer full): a `GtkIconPaintable` containing
 *   for the icon. Unref with g_object_unref()
 */
GtkIconPaintable *
gtk_icon_paintable_new_for_file (GFile *file,
                                 int    size,
                                 int    scale)
{
  return g_object_new (GTK_TYPE_ICON_PAINTABLE,
                       "file", file,
                       "size", size,
                       "scale", scale,
                       NULL);
}

/**
 * gtk_icon_paintable_is_symbolic: (get-property is-symbolic)
 * @self: an icon paintable
 *
 * Checks if the icon is symbolic or not.
 *
 * This currently uses only the file name and not the file contents
 * for determining this. This behaviour may change in the future.
 *
 * Returns: true if the icon is symbolic, false otherwise
 *
 * Deprecated: 4.20
 */
gboolean
gtk_icon_paintable_is_symbolic (GtkIconPaintable *icon)
{
  g_return_val_if_fail (GTK_IS_ICON_PAINTABLE (icon), FALSE);

  return icon->is_symbolic;
}

/**
 * gtk_icon_paintable_get_icon_name:
 * @self: a `GtkIconPaintable`
 *
 * Get the icon name being used for this icon.
 *
 * When an icon looked up in the icon theme was not available, the
 * icon theme may use fallback icons - either those specified to
 * gtk_icon_theme_lookup_icon() or the always-available
 * "image-missing". The icon chosen is returned by this function.
 *
 * If the icon was created without an icon theme, this function
 * returns %NULL.
 *
 * Returns: (nullable) (type filename): the themed icon-name for the
 *   icon, or %NULL if its not a themed icon.
 *
 * Deprecated: 4.20
 */
const char *
gtk_icon_paintable_get_icon_name (GtkIconPaintable *icon)
{
  g_return_val_if_fail (icon != NULL, NULL);

  return icon->icon_name;
}

/**
 * gtk_icon_paintable_get_file:
 * @self: a `GtkIconPaintable`
 *
 * Gets the `GFile` that was used to load the icon.
 *
 * Returns %NULL if the icon was not loaded from a file.
 *
 * Returns: (nullable) (transfer full): the `GFile` for the icon
 */
GFile *
gtk_icon_paintable_get_file (GtkIconPaintable *icon)
{
  if (G_IS_FILE_ICON (icon->loadable))
    {
      return g_object_ref (g_file_icon_get_file (G_FILE_ICON (icon->loadable)));
    }
  else if (icon->filename)
    {
      if (icon->is_resource)
        return new_resource_file (icon->filename);
      else
        return g_file_new_for_path (icon->filename);
    }

  return NULL;
}

/* }}} */

/* vim:set foldmethod=marker: */

