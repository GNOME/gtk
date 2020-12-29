/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "ottietextlayerprivate.h"

#include "ottietextvalueprivate.h"
#include "ottieparserprivate.h"
#include "ottiefontprivate.h"
#include "ottiecharprivate.h"
#include "ottiepathshapeprivate.h"

#include <glib/gi18n-lib.h>
#include <gsk/gsk.h>

struct _OttieTextLayer
{
  OttieLayer parent;

  OttieTextValue text;

  GHashTable *fonts;
  GHashTable *chars;
};

struct _OttieTextLayerClass
{
  OttieLayerClass parent_class;
};

G_DEFINE_TYPE (OttieTextLayer, ottie_text_layer, OTTIE_TYPE_LAYER)

static void
ottie_text_layer_update (OttieLayer *layer,
                         GHashTable *compositions,
                         GHashTable *fonts,
                         GHashTable *chars)
{
  OttieTextLayer *self = OTTIE_TEXT_LAYER (layer);

  g_clear_pointer (&self->fonts, g_hash_table_unref);
  g_clear_pointer (&self->chars, g_hash_table_unref);

  self->fonts = g_hash_table_ref (fonts);
  self->chars = g_hash_table_ref (chars);
}

static GskPath *
get_char_path (OttieChar   *ch,
               OttieRender *render,
               double       timestamp)
{
  OttieRender child_render;
  GskPath *path;

  ottie_render_init_child (&child_render, render);
  ottie_shape_render (ch->shapes, &child_render, timestamp);
  path = gsk_path_ref (ottie_render_get_path (&child_render));

  ottie_render_clear (&child_render);

  return path;
}

static OttieChar *
get_char (OttieTextLayer *self,
          OttieFont      *font,
          gunichar        ch)
{
  OttieCharKey key;
  char s[6] = { 0, };

  g_unichar_to_utf8 (ch, s);

  key.ch = s;
  key.family = font->family;
  key.style = font->style;

  return g_hash_table_lookup (self->chars, &key);
}

static void
render_text_item (OttieTextLayer *self,
                  OttieTextItem  *item,
                  OttieRender    *render,
                  double          timestamp)
{
  OttieFont *font;
  GskTransform *transform, *transform2;
  float font_scale, tx;
  char **lines;
  int n_lines;

  font = g_hash_table_lookup (self->fonts, item->font);
  if (font == NULL)
    {
      g_print ("Ottie is missing a font (%s). Sad!\n", item->font);
      return;
    }

  font_scale = item->size / 100.0;
  transform = gsk_transform_scale (NULL, font_scale, font_scale);

  lines = g_strsplit (item->text, "\r", -1);
  n_lines = g_strv_length (lines);

  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, - (n_lines - 1) * item->line_height / 2 - item->line_shift));

  for (int i = 0; i < n_lines; i++)
    {
      float line_width = 0;
      char *p;

      for (p = lines[i]; *p; p = g_utf8_next_char (p))
        {
          OttieChar *ch = get_char (self, font, g_utf8_get_char (p));
          if (ch == NULL)
            continue;
          line_width += ch->width * font_scale;
        }

      transform2 = gsk_transform_ref (transform);

      switch (item->justify)
        {
        case OTTIE_TEXT_JUSTIFY_LEFT:
          break;
        case OTTIE_TEXT_JUSTIFY_RIGHT:
          transform2 = gsk_transform_translate (transform2, &GRAPHENE_POINT_INIT (- line_width, 0));
          break;
        case OTTIE_TEXT_JUSTIFY_CENTER:
          transform2 = gsk_transform_translate (transform2, &GRAPHENE_POINT_INIT (- line_width/2, 0));
          break;
        default:
          g_assert_not_reached ();
        }

      for (p = lines[i]; *p; p = g_utf8_next_char (p))
        {
          OttieChar *ch = get_char (self, font, g_utf8_get_char (p));
          GskPath *path;

          if (ch == NULL)
            {
              g_print ("Ottie is missing a char. Sad!\n");
              continue;
            }

          path = get_char_path (ch, render, timestamp);

          ottie_render_add_transformed_path (render, path, gsk_transform_ref (transform2));

          tx = ch->width * font_scale + item->tracking / 10.0;
          transform2 = gsk_transform_translate (transform2, &GRAPHENE_POINT_INIT (tx, 0));
        }

      gsk_transform_unref (transform2);

      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, item->line_height));
    }

  gsk_transform_unref (transform);

  g_strfreev (lines);
}

static void
ottie_text_layer_render (OttieLayer  *layer,
                         OttieRender *render,
                         double       timestamp)
{
  OttieTextLayer *self = OTTIE_TEXT_LAYER (layer);
  OttieTextItem item;
  OttieRender child_render;
  GskPath *path;
  graphene_rect_t bounds;
  GskRenderNode *color_node;

  ottie_text_value_get (&self->text, timestamp, &item);

  ottie_render_init_child (&child_render, render);

  render_text_item (self, &item, &child_render, timestamp);

  path = ottie_render_get_path (&child_render);

  gsk_path_get_bounds (path, &bounds);

  color_node = gsk_color_node_new (&item.color, &bounds);

  ottie_render_add_node (&child_render, gsk_fill_node_new (color_node, path, GSK_FILL_RULE_WINDING));

  gsk_render_node_unref (color_node);

  ottie_render_merge (render, &child_render);

  ottie_render_clear (&child_render);
}

static void
ottie_text_layer_print (OttieObject  *obj,
                        OttiePrinter *printer)
{
  OttieTextLayer *self = OTTIE_TEXT_LAYER (obj);

  OTTIE_OBJECT_CLASS (ottie_text_layer_parent_class)->print (obj, printer);

  ottie_printer_add_int (printer, "ty", 5);
  ottie_printer_start_object (printer, "t");
  ottie_text_value_print (&self->text, "d", printer);
  ottie_printer_end_object (printer);
}

static void
ottie_text_layer_dispose (GObject *object)
{
  OttieTextLayer *self = OTTIE_TEXT_LAYER (object);

  ottie_text_value_clear (&self->text);

  g_clear_pointer (&self->fonts, g_hash_table_unref);
  g_clear_pointer (&self->chars, g_hash_table_unref);

  G_OBJECT_CLASS (ottie_text_layer_parent_class)->dispose (object);
}

static void
ottie_text_layer_class_init (OttieTextLayerClass *klass)
{
  OttieObjectClass *oobject_class = OTTIE_OBJECT_CLASS (klass);
  OttieLayerClass *layer_class = OTTIE_LAYER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  oobject_class->print = ottie_text_layer_print;

  layer_class->update = ottie_text_layer_update;
  layer_class->render = ottie_text_layer_render;

  gobject_class->dispose = ottie_text_layer_dispose;
}

static void
ottie_text_layer_init (OttieTextLayer *self)
{
}

static gboolean
ottie_text_layer_parse_text (JsonReader *reader,
                             gsize       offset,
                             gpointer    data)
{
  OttieTextLayer *self = data;
  OttieParserOption options[] = {
    { "d", ottie_text_value_parse, G_STRUCT_OFFSET (OttieTextLayer, text) },
  };

  return ottie_parser_parse_object (reader, "text data", options, G_N_ELEMENTS (options), self);
}

OttieLayer *
ottie_text_layer_parse (JsonReader *reader)
{
  OttieParserOption options[] = {
    OTTIE_PARSE_OPTIONS_LAYER,
    { "t", ottie_text_layer_parse_text, 0 },
  };
  OttieTextLayer *self;

  self = g_object_new (OTTIE_TYPE_TEXT_LAYER, NULL);

  if (!ottie_parser_parse_object (reader, "text layer", options, G_N_ELEMENTS (options), self))
    {
      g_object_unref (self);
      return NULL;
    }

  return OTTIE_LAYER (self);
}
