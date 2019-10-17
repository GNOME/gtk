/*
 * Copyright (c) 2016 Red Hat, Inc.
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

#include "recorder.h"

#include <gtk/gtkbox.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtklistbox.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkpicture.h>
#include <gtk/gtkpopover.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktreelistmodel.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gsk/gskrendererprivate.h>
#include <gsk/gskrendernodeprivate.h>
#include <gsk/gskroundedrectprivate.h>
#include <gsk/gsktransformprivate.h>

#include <glib/gi18n-lib.h>
#include <gdk/gdktextureprivate.h>
#include "gtk/gtkdebug.h"
#include "gtk/gtkiconprivate.h"
#include "gtk/gtkrendernodepaintableprivate.h"

#include "recording.h"
#include "renderrecording.h"
#include "startrecording.h"

struct _GtkInspectorRecorderPrivate
{
  GListModel *recordings;
  GtkTreeListModel *render_node_model;

  GtkWidget *recordings_list;
  GtkWidget *render_node_view;
  GtkWidget *render_node_list;
  GtkWidget *render_node_save_button;
  GtkWidget *node_property_tree;
  GtkTreeModel *render_node_properties;

  GtkInspectorRecording *recording; /* start recording if recording or NULL if not */

  gboolean debug_nodes;
};

enum
{
  PROP_0,
  PROP_RECORDING,
  PROP_DEBUG_NODES,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorRecorder, gtk_inspector_recorder, GTK_TYPE_BIN)

static GListModel *
create_render_node_list_model (GskRenderNode **nodes,
                               guint           n_nodes)
{
  GListStore *store;
  guint i;

  /* can't put render nodes into list models - they're not GObjects */
  store = g_list_store_new (GDK_TYPE_PAINTABLE);

  for (i = 0; i < n_nodes; i++)
    {
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (nodes[i], &bounds);
      GdkPaintable *paintable = gtk_render_node_paintable_new (nodes[i], &bounds);
      g_list_store_append (store, paintable);
      g_object_unref (paintable);
    }

  return G_LIST_MODEL (store);
}

static GListModel *
create_list_model_for_render_node (GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node))
    {
    default:
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return NULL;

    case GSK_CAIRO_NODE:
    case GSK_TEXT_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
      /* no children */
      return NULL;

    case GSK_TRANSFORM_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_transform_node_get_child (node) }, 1);

    case GSK_OPACITY_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_opacity_node_get_child (node) }, 1);

    case GSK_COLOR_MATRIX_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_color_matrix_node_get_child (node) }, 1);

    case GSK_BLUR_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_blur_node_get_child (node) }, 1);

    case GSK_REPEAT_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_repeat_node_get_child (node) }, 1);

    case GSK_CLIP_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_clip_node_get_child (node) }, 1);

    case GSK_ROUNDED_CLIP_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_rounded_clip_node_get_child (node) }, 1);

    case GSK_SHADOW_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_shadow_node_get_child (node) }, 1);

    case GSK_BLEND_NODE:
      return create_render_node_list_model ((GskRenderNode *[2]) { gsk_blend_node_get_bottom_child (node), 
                                                                   gsk_blend_node_get_top_child (node) }, 2);

    case GSK_CROSS_FADE_NODE:
      return create_render_node_list_model ((GskRenderNode *[2]) { gsk_cross_fade_node_get_start_child (node),
                                                                   gsk_cross_fade_node_get_end_child (node) }, 2);

    case GSK_CONTAINER_NODE:
      {
        GListStore *store;
        guint i;

        /* can't put render nodes into list models - they're not GObjects */
        store = g_list_store_new (GDK_TYPE_PAINTABLE);

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            GskRenderNode *child = gsk_container_node_get_child (node, i);
            graphene_rect_t bounds;
            GdkPaintable *paintable;

            gsk_render_node_get_bounds (child, &bounds);
            paintable = gtk_render_node_paintable_new (child, &bounds);
            g_list_store_append (store, paintable);
            g_object_unref (paintable);
          }

        return G_LIST_MODEL (store);
      }

    case GSK_DEBUG_NODE:
      return create_render_node_list_model ((GskRenderNode *[1]) { gsk_debug_node_get_child (node) }, 1);
    }
}

static GListModel *
create_list_model_for_render_node_paintable (gpointer paintable,
                                             gpointer unused)
{
  GskRenderNode *node = gtk_render_node_paintable_get_render_node (paintable);

  return create_list_model_for_render_node (node);
}

static void
recordings_clear_all (GtkButton            *button,
                      GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  g_list_store_remove_all (G_LIST_STORE (priv->recordings));
}

static const char *
node_type_name (GskRenderNodeType type)
{
  switch (type)
    {
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return "Unknown";
    case GSK_CONTAINER_NODE:
      return "Container";
    case GSK_DEBUG_NODE:
      return "Debug";
    case GSK_CAIRO_NODE:
      return "Cairo";
    case GSK_COLOR_NODE:
      return "Color";
    case GSK_LINEAR_GRADIENT_NODE:
      return "Linear Gradient";
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      return "Repeating Linear Gradient";
    case GSK_BORDER_NODE:
      return "Border";
    case GSK_TEXTURE_NODE:
      return "Texture";
    case GSK_INSET_SHADOW_NODE:
      return "Inset Shadow";
    case GSK_OUTSET_SHADOW_NODE:
      return "Outset Shadow";
    case GSK_TRANSFORM_NODE:
      return "Transform";
    case GSK_OPACITY_NODE:
      return "Opacity";
    case GSK_COLOR_MATRIX_NODE:
      return "Color Matrix";
    case GSK_REPEAT_NODE:
      return "Repeat";
    case GSK_CLIP_NODE:
      return "Clip";
    case GSK_ROUNDED_CLIP_NODE:
      return "Rounded Clip";
    case GSK_SHADOW_NODE:
      return "Shadow";
    case GSK_BLEND_NODE:
      return "Blend";
    case GSK_CROSS_FADE_NODE:
      return "CrossFade";
    case GSK_TEXT_NODE:
      return "Text";
    case GSK_BLUR_NODE:
      return "Blur";
    }
}

static char *
node_name (GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
      return g_strdup (node_type_name (gsk_render_node_get_node_type (node)));

    case GSK_DEBUG_NODE:
      return g_strdup (gsk_debug_node_get_message (node));

    case GSK_COLOR_NODE:
      return gdk_rgba_to_string (gsk_color_node_peek_color (node));

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        return g_strdup_printf ("%dx%d Texture", gdk_texture_get_width (texture), gdk_texture_get_height (texture));
      }
    }
}

static GtkWidget *
create_widget_for_render_node (gpointer row_item,
                               gpointer unused)
{
  GdkPaintable *paintable;
  GskRenderNode *node;
  GtkWidget *row, *box, *child;
  char *name;
  guint depth;

  paintable = gtk_tree_list_row_get_item (row_item);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));
  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_container_add (GTK_CONTAINER (row), box);

  /* expander */
  depth = gtk_tree_list_row_get_depth (row_item);
  if (depth > 0)
    {
      child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_size_request (child, 16 * depth, 0);
      gtk_container_add (GTK_CONTAINER (box), child);
    }
  if (gtk_tree_list_row_is_expandable (row_item))
    {
      GtkWidget *title, *arrow;

      child = g_object_new (GTK_TYPE_BOX, "css-name", "expander", NULL);
      
      title = g_object_new (GTK_TYPE_TOGGLE_BUTTON, "css-name", "title", NULL);
      gtk_button_set_relief (GTK_BUTTON (title), GTK_RELIEF_NONE);
      g_object_bind_property (row_item, "expanded", title, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_container_add (GTK_CONTAINER (child), title);
      g_object_set_data_full (G_OBJECT (row), "make-sure-its-not-unreffed", g_object_ref (row_item), g_object_unref);

      arrow = gtk_icon_new ("arrow");
      gtk_container_add (GTK_CONTAINER (title), arrow);
    }
  else
    {
      child = gtk_image_new (); /* empty whatever */
    }
  gtk_container_add (GTK_CONTAINER (box), child);

  /* icon */
  child = gtk_image_new_from_paintable (paintable);
  gtk_container_add (GTK_CONTAINER (box), child);

  /* name */
  name = node_name (node);
  child = gtk_label_new (name);
  g_free (name);
  gtk_container_add (GTK_CONTAINER (box), child);

  g_object_unref (paintable);

  return row;
}

static void
recordings_list_row_selected (GtkListBox           *box,
                              GtkListBoxRow        *row,
                              GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkInspectorRecording *recording;

  if (row)
    recording = g_list_model_get_item (priv->recordings, gtk_list_box_row_get_index (row));
  else
    recording = NULL;

  g_clear_object (&priv->render_node_model);

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      GListStore *root_model;
      graphene_rect_t bounds;
      GskRenderNode *node;
      GdkPaintable *paintable;

      node = gtk_inspector_render_recording_get_node (GTK_INSPECTOR_RENDER_RECORDING (recording));
      gsk_render_node_get_bounds (node, &bounds);
      paintable = gtk_render_node_paintable_new (node, &bounds);
      gtk_picture_set_paintable (GTK_PICTURE (priv->render_node_view), paintable);

      root_model = g_list_store_new (GDK_TYPE_PAINTABLE);
      g_list_store_append (root_model, paintable);
      priv->render_node_model = gtk_tree_list_model_new (FALSE,
                                                         G_LIST_MODEL (root_model),
                                                         TRUE,
                                                         create_list_model_for_render_node_paintable,
                                                         NULL, NULL);
      g_object_unref (root_model);
      g_object_unref (paintable);

      g_print ("%u render nodes\n", g_list_model_get_n_items (G_LIST_MODEL (priv->render_node_model)));
    }
  else
    {
      gtk_picture_set_paintable (GTK_PICTURE (priv->render_node_view), NULL);
    }


  gtk_list_box_bind_model (GTK_LIST_BOX (priv->render_node_list),
                           G_LIST_MODEL (priv->render_node_model),
                           create_widget_for_render_node,
                           NULL, NULL);

  if (recording)
    g_object_unref (recording);
}

static GdkTexture *
get_color_texture (const GdkRGBA *color)
{
  GdkTexture *texture;
  guchar pixel[4];
  guchar *data;
  GBytes *bytes;
  gint width = 30;
  gint height = 30;
  gint i;

  pixel[0] = round (color->red * 255);
  pixel[1] = round (color->green * 255);
  pixel[2] = round (color->blue * 255);
  pixel[3] = round (color->alpha * 255);

  data = g_malloc (4 * width * height);
  for (i = 0; i < width * height; i++)
    {
      memcpy (data + 4 * i, pixel, 4);
    }

  bytes = g_bytes_new_take (data, 4 * width * height);
  texture = gdk_memory_texture_new (width,
                                    height,
                                    GDK_MEMORY_R8G8B8A8,
                                    bytes,
                                    width * 4);
  g_bytes_unref (bytes);

  return texture;
}

static GdkTexture *
get_linear_gradient_texture (gsize n_stops, const GskColorStop *stops)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  GdkTexture *texture;
  int i;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 90, 30);
  cr = cairo_create (surface);

  pattern = cairo_pattern_create_linear (0, 0, 90, 0);
  for (i = 0; i < n_stops; i++)
    {
      cairo_pattern_add_color_stop_rgba (pattern,
                                         stops[i].offset,
                                         stops[i].color.red,
                                         stops[i].color.green,
                                         stops[i].color.blue,
                                         stops[i].color.alpha);
    }

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_rectangle (cr, 0, 0, 90, 30);
  cairo_fill (cr);
  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
add_text_row (GtkListStore *store,
              const char   *name,
              const char   *text)
{
  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, name,
                                     1, text,
                                     2, FALSE,
                                     3, NULL,
                                     -1);
}

static void
add_color_row (GtkListStore  *store,
               const char    *name,
               const GdkRGBA *color)
{
  char *text;
  GdkTexture *texture;

  text = gdk_rgba_to_string (color);
  texture = get_color_texture (color);
  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, name,
                                     1, text,
                                     2, TRUE,
                                     3, texture,
                                     -1);
  g_free (text);
  g_object_unref (texture);
}

static void
add_float_row (GtkListStore  *store,
               const char    *name,
               float          value)
{
  char *text = g_strdup_printf ("%.2f", value);
  add_text_row (store, name, text);
  g_free (text);
}

static void
populate_render_node_properties (GtkListStore  *store,
                                 GskRenderNode *node)
{
  graphene_rect_t bounds;
  char *tmp;

  gtk_list_store_clear (store);

  gsk_render_node_get_bounds (node, &bounds);

  add_text_row (store, "Type", node_type_name (gsk_render_node_get_node_type (node)));

  tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                         bounds.size.width,
                         bounds.size.height,
                         bounds.origin.x,
                         bounds.origin.y);
  add_text_row (store, "Bounds", tmp);
  g_free (tmp);

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CAIRO_NODE:
      {
        GdkTexture *texture;
        cairo_surface_t *drawn_surface;
        cairo_t *cr;
        gboolean show_inline;

        drawn_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                    ceilf (node->bounds.size.width),
                                                    ceilf (node->bounds.size.height));
        cr = cairo_create (drawn_surface);
        cairo_save (cr);
        cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);
        gsk_render_node_draw (node, cr);
        cairo_restore (cr);

        cairo_destroy (cr);

        texture = gdk_texture_new_for_surface (drawn_surface);
        cairo_surface_destroy (drawn_surface);

        show_inline = gdk_texture_get_height (texture) <= 40 &&
                      gdk_texture_get_width (texture) <= 100;

        gtk_list_store_insert_with_values (store, NULL, -1,
                                           0, "Surface",
                                           1, show_inline ? "" : "Yes (click to show)",
                                           2, show_inline,
                                           3, texture,
                                           -1);
      }
      break;

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = g_object_ref (gsk_texture_node_get_texture (node));
        gboolean show_inline;

        show_inline = gdk_texture_get_height (texture) <= 40 &&
                      gdk_texture_get_width (texture) <= 100;

        gtk_list_store_insert_with_values (store, NULL, -1,
                                           0, "Texture",
                                           1, show_inline ? "" : "Yes (click to show)",
                                           2, show_inline,
                                           3, texture,
                                           -1);
      }
      break;

    case GSK_COLOR_NODE:
      add_color_row (store, "Color", gsk_color_node_peek_color (node));
      break;

    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      {
        const graphene_point_t *start = gsk_linear_gradient_node_peek_start (node);
        const graphene_point_t *end = gsk_linear_gradient_node_peek_end (node);
        const gsize n_stops = gsk_linear_gradient_node_get_n_color_stops (node);
        const GskColorStop *stops = gsk_linear_gradient_node_peek_color_stops (node);
        int i;
        GString *s;
        GdkTexture *texture;

        tmp = g_strdup_printf ("%.2f %.2f ⟶ %.2f %.2f", start->x, start->y, end->x, end->y);
        add_text_row (store, "Direction", tmp);
        g_free (tmp);

        s = g_string_new ("");
        for (i = 0; i < n_stops; i++)
          {
            tmp = gdk_rgba_to_string (&stops[i].color);
            g_string_append_printf (s, "%.2f, %s\n", stops[i].offset, tmp);
            g_free (tmp);
          }

        texture = get_linear_gradient_texture (n_stops, stops);
        gtk_list_store_insert_with_values (store, NULL, -1,
                                           0, "Color Stops",
                                           1, s->str,
                                           2, TRUE,
                                           3, texture,
                                           -1);
        g_string_free (s, TRUE);
        g_object_unref (texture);
      }
      break;

    case GSK_TEXT_NODE:
      {
        const PangoFont *font = gsk_text_node_peek_font (node);
        const PangoGlyphInfo *glyphs = gsk_text_node_peek_glyphs (node);
        const GdkRGBA *color = gsk_text_node_peek_color (node);
        guint num_glyphs = gsk_text_node_get_num_glyphs (node);
        const graphene_point_t *offset = gsk_text_node_get_offset (node);
        PangoFontDescription *desc;
        GString *s;
        int i;

        desc = pango_font_describe ((PangoFont *)font);
        tmp = pango_font_description_to_string (desc);
        add_text_row (store, "Font", tmp);
        g_free (tmp);
        pango_font_description_free (desc);

        s = g_string_sized_new (6 * num_glyphs);
        for (i = 0; i < num_glyphs; i++)
          g_string_append_printf (s, "%x ", glyphs[i].glyph);
        add_text_row (store, "Glyphs", s->str);
        g_string_free (s, TRUE);

        tmp = g_strdup_printf ("%.2f %.2f", offset->x, offset->y);
        add_text_row (store, "Position", tmp);
        g_free (tmp);

        add_color_row (store, "Color", color);
      }
      break;

    case GSK_BORDER_NODE:
      {
        const char *name[4] = { "Top", "Right", "Bottom", "Left" };
        const float *widths = gsk_border_node_peek_widths (node);
        const GdkRGBA *colors = gsk_border_node_peek_colors (node);
        int i;

        for (i = 0; i < 4; i++)
          {
            GdkTexture *texture;
            char *text;

            texture = get_color_texture (&colors[i]);
            text = gdk_rgba_to_string (&colors[i]);
            tmp = g_strdup_printf ("%.2f, %s", widths[i], text);
            gtk_list_store_insert_with_values (store, NULL, -1,
                                               0, name[i],
                                               1, tmp,
                                               2, TRUE,
                                               3, texture,
                                               -1);
            g_free (text);
            g_free (tmp);
            g_object_unref (texture);
          }
      }
      break;

    case GSK_OPACITY_NODE:
      add_float_row (store, "Opacity", gsk_opacity_node_get_opacity (node));
      break;

    case GSK_CROSS_FADE_NODE:
      add_float_row (store, "Progress", gsk_cross_fade_node_get_progress (node));
      break;

    case GSK_BLEND_NODE:
      {
        GskBlendMode mode = gsk_blend_node_get_blend_mode (node);
        tmp = g_enum_to_string (GSK_TYPE_BLEND_MODE, mode);
        add_text_row (store, "Blendmode", tmp);
        g_free (tmp);
      }
      break;

    case GSK_BLUR_NODE:
      add_float_row (store, "Radius", gsk_blur_node_get_radius (node));
      break;

    case GSK_INSET_SHADOW_NODE:
      {
        const GdkRGBA *color = gsk_inset_shadow_node_peek_color (node);
        float dx = gsk_inset_shadow_node_get_dx (node);
        float dy = gsk_inset_shadow_node_get_dy (node);
        float spread = gsk_inset_shadow_node_get_spread (node);
        float radius = gsk_inset_shadow_node_get_blur_radius (node);

        add_color_row (store, "Color", color);

        tmp = g_strdup_printf ("%.2f %.2f", dx, dy);
        add_text_row (store, "Offset", tmp);
        g_free (tmp);

        add_float_row (store, "Spread", spread);
        add_float_row (store, "Radius", radius);
      }
      break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        const GskRoundedRect *outline = gsk_outset_shadow_node_peek_outline (node);
        const GdkRGBA *color = gsk_outset_shadow_node_peek_color (node);
        float dx = gsk_outset_shadow_node_get_dx (node);
        float dy = gsk_outset_shadow_node_get_dy (node);
        float spread = gsk_outset_shadow_node_get_spread (node);
        float radius = gsk_outset_shadow_node_get_blur_radius (node);
        float rect[12];

        gsk_rounded_rect_to_float (outline, rect);
        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               rect[2], rect[3], rect[0], rect[1]);
        add_text_row (store, "Outline", tmp);
        g_free (tmp);

        add_color_row (store, "Color", color);

        tmp = g_strdup_printf ("%.2f %.2f", dx, dy);
        add_text_row (store, "Offset", tmp);
        g_free (tmp);

        add_float_row (store, "Spread", spread);
        add_float_row (store, "Radius", radius);
      }
      break;

    case GSK_REPEAT_NODE:
      {
        const graphene_rect_t *child_bounds = gsk_repeat_node_peek_child_bounds (node);

        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               child_bounds->size.width,
                               child_bounds->size.height,
                               child_bounds->origin.x,
                               child_bounds->origin.y);
        add_text_row (store, "Child Bounds", tmp);
        g_free (tmp);
      }
      break;

    case GSK_COLOR_MATRIX_NODE:
      {
        const graphene_matrix_t *matrix = gsk_color_matrix_node_peek_color_matrix (node);
        const graphene_vec4_t *offset = gsk_color_matrix_node_peek_color_offset (node);

        tmp = g_strdup_printf ("% .2f % .2f % .2f % .2f\n"
                               "% .2f % .2f % .2f % .2f\n"
                               "% .2f % .2f % .2f % .2f\n"
                               "% .2f % .2f % .2f % .2f",
                               graphene_matrix_get_value (matrix, 0, 0),
                               graphene_matrix_get_value (matrix, 0, 1),
                               graphene_matrix_get_value (matrix, 0, 2),
                               graphene_matrix_get_value (matrix, 0, 3),
                               graphene_matrix_get_value (matrix, 1, 0),
                               graphene_matrix_get_value (matrix, 1, 1),
                               graphene_matrix_get_value (matrix, 1, 2),
                               graphene_matrix_get_value (matrix, 1, 3),
                               graphene_matrix_get_value (matrix, 2, 0),
                               graphene_matrix_get_value (matrix, 2, 1),
                               graphene_matrix_get_value (matrix, 2, 2),
                               graphene_matrix_get_value (matrix, 2, 3),
                               graphene_matrix_get_value (matrix, 3, 0),
                               graphene_matrix_get_value (matrix, 3, 1),
                               graphene_matrix_get_value (matrix, 3, 2),
                               graphene_matrix_get_value (matrix, 3, 3));
        add_text_row (store, "Matrix", tmp);
        g_free (tmp);
        tmp = g_strdup_printf ("%.2f %.2f %.2f %.2f",
                               graphene_vec4_get_x (offset),
                               graphene_vec4_get_y (offset),
                               graphene_vec4_get_z (offset),
                               graphene_vec4_get_w (offset));
        add_text_row (store, "Offset", tmp);
        g_free (tmp);
      }
      break;

    case GSK_CLIP_NODE:
      {
        const graphene_rect_t *clip = gsk_clip_node_peek_clip (node);
        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               clip->size.width,
                               clip->size.height,
                               clip->origin.x,
                               clip->origin.y);
        add_text_row (store, "Clip", tmp);
        g_free (tmp);
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        const GskRoundedRect *clip = gsk_rounded_clip_node_peek_clip (node);
        tmp = g_strdup_printf ("%.2f x %.2f + %.2f + %.2f",
                               clip->bounds.size.width,
                               clip->bounds.size.height,
                               clip->bounds.origin.x,
                               clip->bounds.origin.y);
        add_text_row (store, "Clip", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[0].width, clip->corner[0].height);
        add_text_row (store, "Top Left Corner Size", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[1].width, clip->corner[1].height);
        add_text_row (store, "Top Right Corner Size", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[2].width, clip->corner[2].height);
        add_text_row (store, "Bottom Right Corner Size", tmp);
        g_free (tmp);

        tmp = g_strdup_printf ("%.2f x %.2f", clip->corner[3].width, clip->corner[3].height);
        add_text_row (store, "Bottom Left Corner Size", tmp);
        g_free (tmp);
      }
      break;

    case GSK_CONTAINER_NODE:
      tmp = g_strdup_printf ("%d", gsk_container_node_get_n_children (node));
      add_text_row (store, "Children", tmp);
      g_free (tmp);
      break;

    case GSK_DEBUG_NODE:
      add_text_row (store, "Message", gsk_debug_node_get_message (node));
      break;

    case GSK_SHADOW_NODE:
      {
        int i;

        for (i = 0; i < gsk_shadow_node_get_n_shadows (node); i++)
          {
            char *label;
            char *value;
            const GskShadow *shadow = gsk_shadow_node_peek_shadow (node, i);

            label = g_strdup_printf ("Color %d", i);
            add_color_row (store, label, &shadow->color);
            g_free (label);

            label = g_strdup_printf ("Offset %d", i);
            value = g_strdup_printf ("%.2f %.2f", shadow->dx, shadow->dy);
            add_text_row (store, label, value);
            g_free (value);
            g_free (label);

            label = g_strdup_printf ("Radius %d", i);
            add_float_row (store, label, shadow->radius);
            g_free (label);
          }
      }
      break;

    case GSK_TRANSFORM_NODE:
      {
        static const char * category_names[] = {
          [GSK_TRANSFORM_CATEGORY_UNKNOWN] = "unknown",
          [GSK_TRANSFORM_CATEGORY_ANY] = "any",
          [GSK_TRANSFORM_CATEGORY_3D] = "3D",
          [GSK_TRANSFORM_CATEGORY_2D] = "2D",
          [GSK_TRANSFORM_CATEGORY_2D_AFFINE] = "2D affine",
          [GSK_TRANSFORM_CATEGORY_2D_TRANSLATE] = "2D translate",
          [GSK_TRANSFORM_CATEGORY_IDENTITY] = "identity"
        };
        GskTransform *transform;
        char *s;

        transform = gsk_transform_node_get_transform (node);
        s = gsk_transform_to_string (transform);
        add_text_row (store, "Matrix", s);
        g_free (s);
        add_text_row (store, "Category", category_names[gsk_transform_get_category (transform)]);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      break;
    }
}

static GskRenderNode *
get_selected_node (GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkTreeListRow *row_item;
  GtkListBoxRow *row;
  GdkPaintable *paintable;
  GskRenderNode *node;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->render_node_list));
  if (row == NULL)
    return NULL;

  row_item = g_list_model_get_item (G_LIST_MODEL (priv->render_node_model),
                                    gtk_list_box_row_get_index (row));
  paintable = gtk_tree_list_row_get_item (row_item);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));
  g_object_unref (paintable);
  g_object_unref (row_item);

  return node;
}

static void
render_node_list_selection_changed (GtkListBox           *list,
                                    GtkListBoxRow        *row,
                                    GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GskRenderNode *node;
  GdkPaintable *paintable;
  GtkTreeListRow *row_item;

  if (row == NULL)
    {
      gtk_widget_set_sensitive (priv->render_node_save_button, FALSE);
      return;
    }

  row_item = g_list_model_get_item (G_LIST_MODEL (priv->render_node_model),
                                    gtk_list_box_row_get_index (row));
  paintable = gtk_tree_list_row_get_item (row_item);

  gtk_widget_set_sensitive (priv->render_node_save_button, TRUE);
  gtk_picture_set_paintable (GTK_PICTURE (priv->render_node_view), paintable);
  node = gtk_render_node_paintable_get_render_node (GTK_RENDER_NODE_PAINTABLE (paintable));
  populate_render_node_properties (GTK_LIST_STORE (priv->render_node_properties), node);

  g_object_unref (paintable);
  g_object_unref (row_item);
}

static void
render_node_save_response (GtkWidget     *dialog,
                           gint           response,
                           GskRenderNode *node)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GBytes *bytes = gsk_render_node_serialize (node);
      GError *error = NULL;

      if (!g_file_replace_contents (gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog)),
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes),
                                    NULL,
                                    FALSE,
                                    0,
                                    NULL,
                                    NULL,
                                    &error))
        {
          GtkWidget *message_dialog;

          message_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog))),
                                                   GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   _("Saving RenderNode failed"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message_dialog),
                                                    "%s", error->message);
          g_signal_connect (message_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_widget_show (message_dialog);
          g_error_free (error);
        }

      g_bytes_unref (bytes);
    }

  gtk_widget_destroy (dialog);
}

static void
render_node_save (GtkButton            *button,
                  GtkInspectorRecorder *recorder)
{
  GskRenderNode *node;
  GtkWidget *dialog;
  char *filename, *nodename;

  node = get_selected_node (recorder);
  if (node == NULL)
    return;

  dialog = gtk_file_chooser_dialog_new ("",
                                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (recorder))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  nodename = node_name (node);
  filename = g_strdup_printf ("%s.node", nodename);
  g_free (nodename);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
  g_free (filename);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (render_node_save_response), node);
  gtk_widget_show (dialog);
}

static char *
format_timespan (gint64 timespan)
{
  if (ABS (timespan) < G_TIME_SPAN_MILLISECOND)
    return g_strdup_printf ("%fus", (double) timespan);
  else if (ABS (timespan) < 10 * G_TIME_SPAN_MILLISECOND)
    return g_strdup_printf ("%.1fs", (double) timespan / G_TIME_SPAN_MILLISECOND);
  else if (ABS (timespan) < G_TIME_SPAN_SECOND)
    return g_strdup_printf ("%.0fms", (double) timespan / G_TIME_SPAN_MILLISECOND);
  else if (ABS (timespan) < 10 * G_TIME_SPAN_SECOND)
    return g_strdup_printf ("%.1fs", (double) timespan / G_TIME_SPAN_SECOND);
  else
    return g_strdup_printf ("%.0fs", (double) timespan / G_TIME_SPAN_SECOND);
}

static GtkWidget *
gtk_inspector_recorder_recordings_list_create_widget (gpointer item,
                                                      gpointer user_data)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (user_data);
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkInspectorRecording *recording = GTK_INSPECTOR_RECORDING (item);
  GtkWidget *widget;

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      GtkInspectorRecording *previous = NULL;
      char *time_str, *str;
      const char *render_str;
      cairo_region_t *region;
      GtkWidget *hbox, *label, *button;
      guint i;

      widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_container_add (GTK_CONTAINER (widget), hbox);

      for (i = 0; i < g_list_model_get_n_items (priv->recordings); i++)
        {
          GtkInspectorRecording *r = g_list_model_get_item (priv->recordings, i);

          g_object_unref (r);

          if (r == recording)
            break;

          if (GTK_INSPECTOR_IS_RENDER_RECORDING (r))
            previous = r;
          else if (GTK_INSPECTOR_IS_START_RECORDING (r))
            previous = NULL;
        }

      region = cairo_region_create_rectangle (
                   gtk_inspector_render_recording_get_area (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      cairo_region_subtract (region,
                             gtk_inspector_render_recording_get_clip_region (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      if (cairo_region_is_empty (region))
        render_str = "Full Render";
      else
        render_str = "Partial Render";
      cairo_region_destroy (region);

      if (previous)
        {
          time_str = format_timespan (gtk_inspector_recording_get_timestamp (recording) -
                                      gtk_inspector_recording_get_timestamp (previous));
          str = g_strdup_printf ("<b>%s</b>\n+%s", render_str, time_str);
          g_free (time_str);
        }
      else
        {
          str = g_strdup_printf ("<b>%s</b>\n", render_str);
        }
      label = gtk_label_new (str);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      g_free (str);
      gtk_container_add (GTK_CONTAINER (hbox), label);

      button = gtk_toggle_button_new ();
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_button_set_icon_name (GTK_BUTTON (button), "view-more-symbolic");

      gtk_container_add (GTK_CONTAINER (hbox), button);

      label = gtk_label_new (gtk_inspector_render_recording_get_profiler_info (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      gtk_widget_hide (label);
      gtk_container_add (GTK_CONTAINER (widget), label);
      g_object_bind_property (button, "active", label, "visible", 0);
    }
  else
    {
      widget = gtk_label_new ("<b>Start of Recording</b>");
      gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
    }

  g_object_set (widget, "margin", 6, NULL); /* Seriously? g_object_set() needed for that? */

  return widget;
}

static void
node_property_activated (GtkTreeView *tv,
                         GtkTreePath *path,
                         GtkTreeViewColumn *col,
                         GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkTreeIter iter;
  GdkRectangle rect;
  GdkTexture *texture;
  gboolean visible;
  GtkWidget *popover;
  GtkWidget *image;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->render_node_properties), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (priv->render_node_properties), &iter,
                      2, &visible,
                      3, &texture,
                      -1);
  gtk_tree_view_get_cell_area (tv, path, col, &rect);
  gtk_tree_view_convert_bin_window_to_widget_coords (tv, rect.x, rect.y, &rect.x, &rect.y);

  if (texture == NULL || visible)
    return;

  popover = gtk_popover_new (GTK_WIDGET (tv));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  image = gtk_image_new_from_paintable (GDK_PAINTABLE (texture));
  g_object_set (image, "margin", 20, NULL);
  gtk_container_add (GTK_CONTAINER (popover), image);
  gtk_popover_popup (GTK_POPOVER (popover));

  g_signal_connect (popover, "unmap", G_CALLBACK (gtk_widget_destroy), NULL);

  g_object_unref (texture);
}

static void
gtk_inspector_recorder_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  switch (param_id)
    {
    case PROP_RECORDING:
      g_value_set_boolean (value, priv->recording != NULL);
      break;

    case PROP_DEBUG_NODES:
      g_value_set_boolean (value, priv->debug_nodes);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recorder_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);

  switch (param_id)
    {
    case PROP_RECORDING:
      gtk_inspector_recorder_set_recording (recorder, g_value_get_boolean (value));
      break;

    case PROP_DEBUG_NODES:
      gtk_inspector_recorder_set_debug_nodes (recorder, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recorder_dispose (GObject *object)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  g_clear_object (&priv->render_node_model);

  G_OBJECT_CLASS (gtk_inspector_recorder_parent_class)->dispose (object);
}

static void
gtk_inspector_recorder_class_init (GtkInspectorRecorderClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_inspector_recorder_get_property;
  object_class->set_property = gtk_inspector_recorder_set_property;
  object_class->dispose = gtk_inspector_recorder_dispose;

  props[PROP_RECORDING] =
    g_param_spec_boolean ("recording",
                          "Recording",
                          "Whether the recorder is currently recording",
                          FALSE,
                          G_PARAM_READWRITE);
  props[PROP_DEBUG_NODES] =
    g_param_spec_boolean ("debug-nodes",
                          "Debug nodes",
                          "Whether to insert extra debug nodes in the tree",
                          FALSE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/recorder.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, recordings);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, recordings_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, render_node_view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, render_node_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, render_node_save_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, node_property_tree);

  gtk_widget_class_bind_template_callback (widget_class, recordings_clear_all);
  gtk_widget_class_bind_template_callback (widget_class, recordings_list_row_selected);
  gtk_widget_class_bind_template_callback (widget_class, render_node_list_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, render_node_save);
  gtk_widget_class_bind_template_callback (widget_class, node_property_activated);
}

static void
gtk_inspector_recorder_init (GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  gtk_widget_init_template (GTK_WIDGET (recorder));

  gtk_list_box_bind_model (GTK_LIST_BOX (priv->recordings_list),
                           priv->recordings,
                           gtk_inspector_recorder_recordings_list_create_widget,
                           recorder,
                           NULL);

  priv->render_node_properties = GTK_TREE_MODEL (gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, GDK_TYPE_TEXTURE));
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->node_property_tree), priv->render_node_properties);
  g_object_unref (priv->render_node_properties);
}

static void
gtk_inspector_recorder_add_recording (GtkInspectorRecorder  *recorder,
                                      GtkInspectorRecording *recording)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  guint count;
  GtkListBoxRow *selected_row;
  gboolean should_select_new_row;

  count = g_list_model_get_n_items (priv->recordings);
  selected_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->recordings_list));
  if (count == 0 || selected_row == NULL)
    should_select_new_row = TRUE;
  else
    should_select_new_row = (gtk_list_box_row_get_index (selected_row) == count - 1);

  g_list_store_append (G_LIST_STORE (priv->recordings), recording);

  if (should_select_new_row)
    {
      gtk_list_box_select_row (GTK_LIST_BOX (priv->recordings_list),
                               gtk_list_box_get_row_at_index (GTK_LIST_BOX (priv->recordings_list), count));
    }
}

void
gtk_inspector_recorder_set_recording (GtkInspectorRecorder *recorder,
                                      gboolean              recording)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  if (gtk_inspector_recorder_is_recording (recorder) == recording)
    return;

  if (recording)
    {
      priv->recording = gtk_inspector_start_recording_new ();
      gtk_inspector_recorder_add_recording (recorder, priv->recording);
    }
  else
    {
      g_clear_object (&priv->recording);
    }

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_RECORDING]);
}

gboolean
gtk_inspector_recorder_is_recording (GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  return priv->recording != NULL;
}

void
gtk_inspector_recorder_record_render (GtkInspectorRecorder *recorder,
                                      GtkWidget            *widget,
                                      GskRenderer          *renderer,
                                      GdkSurface           *surface,
                                      const cairo_region_t *region,
                                      GskRenderNode        *node)
{
  GtkInspectorRecording *recording;
  GdkFrameClock *frame_clock;

  if (!gtk_inspector_recorder_is_recording (recorder))
    return;

  frame_clock = gtk_widget_get_frame_clock (widget);

  recording = gtk_inspector_render_recording_new (gdk_frame_clock_get_frame_time (frame_clock),
                                                  gsk_renderer_get_profiler (renderer),
                                                  &(GdkRectangle) { 0, 0,
                                                    gdk_surface_get_width (surface),
                                                    gdk_surface_get_height (surface) },
                                                  region,
                                                  node);
  gtk_inspector_recorder_add_recording (recorder, recording);
  g_object_unref (recording);
}

void
gtk_inspector_recorder_set_debug_nodes (GtkInspectorRecorder *recorder,
                                        gboolean              debug_nodes)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  guint flags;

  if (priv->debug_nodes == debug_nodes)
    return;

  priv->debug_nodes = debug_nodes;

  flags = gtk_get_debug_flags ();

  if (debug_nodes)
    flags |= GTK_DEBUG_SNAPSHOT;
  else
    flags &= ~GTK_DEBUG_SNAPSHOT;

  gtk_set_debug_flags (flags);

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_DEBUG_NODES]);
}

// vim: set et sw=2 ts=2:
