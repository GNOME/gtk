/* gtkpango.c - pango-related utilities
 *
 * Copyright (c) 2010 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkpango.h"
#include <pango2/pangocairo.h>
#include "gtkintl.h"
#include "gtkbuilderprivate.h"

static gboolean
attr_list_merge_filter (Pango2Attribute *attribute,
                        gpointer        list)
{
  pango2_attr_list_change (list, pango2_attribute_copy (attribute));
  return FALSE;
}

/*
 * _gtk_pango2_attr_list_merge:
 * @into: (nullable): a `Pango2AttrList` where attributes are merged
 * @from: (nullable): a `Pango2AttrList` with the attributes to merge
 *
 * Merges attributes from @from into @into.
 *
 * Returns: the merged list.
 */
Pango2AttrList *
_gtk_pango_attr_list_merge (Pango2AttrList *into,
                            Pango2AttrList *from)
{
  if (from)
    {
      if (into)
        pango2_attr_list_filter (from, attr_list_merge_filter, into);
      else
       return pango2_attr_list_ref (from);
    }

  return into;
}

static Pango2Attribute *
attribute_from_text (GtkBuilder  *builder,
                     const char  *name,
                     const char  *value,
                     GError     **error)
{
  Pango2Attribute *attribute = NULL;
  Pango2AttrType type;
  Pango2Language *language;
  Pango2FontDescription *font_desc;
  GdkRGBA *color;
  GValue val = G_VALUE_INIT;

  if (!gtk_builder_value_from_string_type (builder, PANGO2_TYPE_ATTR_TYPE, name, &val, error))
    return NULL;

  type = g_value_get_enum (&val);
  g_value_unset (&val);

  switch (type)
    {
    case PANGO2_ATTR_LANGUAGE:
      if ((language = pango2_language_from_string (value)))
        {
          attribute = pango2_attr_language_new (language);
          g_value_init (&val, G_TYPE_INT);
        }
      break;
    case PANGO2_ATTR_STYLE:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_STYLE, value, &val, error))
        attribute = pango2_attr_style_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_WEIGHT:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_WEIGHT, value, &val, error))
        attribute = pango2_attr_weight_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_VARIANT:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_VARIANT, value, &val, error))
        attribute = pango2_attr_variant_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_STRETCH:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_STRETCH, value, &val, error))
        attribute = pango2_attr_stretch_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_UNDERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_LINE_STYLE, value, &val, NULL))
        attribute = pango2_attr_underline_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_UNDERLINE_POSITION:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_UNDERLINE_POSITION, value, &val, NULL))
        attribute = pango2_attr_underline_position_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_STRIKETHROUGH:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_LINE_STYLE, value, &val, NULL))
        attribute = pango2_attr_strikethrough_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_GRAVITY:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_GRAVITY, value, &val, error))
        attribute = pango2_attr_gravity_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_GRAVITY_HINT:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_GRAVITY_HINT, value, &val, error))
        attribute = pango2_attr_gravity_hint_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_FAMILY:
      attribute = pango2_attr_family_new (value);
      g_value_init (&val, G_TYPE_INT);
      break;

    case PANGO2_ATTR_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango2_attr_size_new (g_value_get_int (&val));
      break;
    case PANGO2_ATTR_ABSOLUTE_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango2_attr_size_new_absolute (g_value_get_int (&val));
      break;

    case PANGO2_ATTR_FONT_DESC:
      if ((font_desc = pango2_font_description_from_string (value)))
        {
          attribute = pango2_attr_font_desc_new (font_desc);
          pango2_font_description_free (font_desc);
          g_value_init (&val, G_TYPE_INT);
        }
      break;
    case PANGO2_ATTR_FOREGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango2_attr_foreground_new (&(Pango2Color){color->red * 65535,
                                                               color->green * 65535,
                                                               color->blue * 65535,
                                                               color->alpha * 65535});
        }
      break;
    case PANGO2_ATTR_BACKGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango2_attr_background_new (&(Pango2Color){color->red * 65535,
                                                               color->green * 65535,
                                                               color->blue * 65535,
                                                               color->alpha * 65535});
        }
      break;
    case PANGO2_ATTR_UNDERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango2_attr_underline_color_new (&(Pango2Color){color->red * 65535,
                                                                    color->green * 65535,
                                                                    color->blue * 65535,
                                                                    color->alpha * 65535});
        }
      break;
    case PANGO2_ATTR_STRIKETHROUGH_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango2_attr_strikethrough_color_new (&(Pango2Color){color->red * 65535,
                                                                        color->green * 65535,
                                                                        color->blue * 65535,
                                                                        color->alpha * 65535});
        }
      break;
      /* Pango2AttrFloat */
    case PANGO2_ATTR_SCALE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, value, &val, error))
        attribute = pango2_attr_scale_new (g_value_get_double (&val));
      break;
    case PANGO2_ATTR_LETTER_SPACING:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango2_attr_letter_spacing_new (g_value_get_int (&val));
      break;
    case PANGO2_ATTR_RISE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango2_attr_rise_new (g_value_get_int (&val));
      break;
    case PANGO2_ATTR_FALLBACK:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango2_attr_fallback_new (g_value_get_boolean (&val));
      break;
    case PANGO2_ATTR_FONT_FEATURES:
      attribute = pango2_attr_font_features_new (value);
      break;
    case PANGO2_ATTR_PALETTE:
      attribute = pango2_attr_palette_new (value);
      break;
    case PANGO2_ATTR_ALLOW_BREAKS:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango2_attr_allow_breaks_new (g_value_get_boolean (&val));
      break;
    case PANGO2_ATTR_SHOW:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_SHOW_FLAGS, value, &val, error))
        attribute = pango2_attr_show_new (g_value_get_flags (&val));
      break;
    case PANGO2_ATTR_INSERT_HYPHENS:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango2_attr_insert_hyphens_new (g_value_get_boolean (&val));
      break;
    case PANGO2_ATTR_OVERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_LINE_STYLE, value, &val, NULL))
        attribute = pango2_attr_overline_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_OVERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango2_attr_overline_color_new (&(Pango2Color){color->red * 65535,
                                                                   color->green * 65535,
                                                                   color->blue * 65535,
                                                                   color->alpha * 65535});
        }
      break;
    case PANGO2_ATTR_LINE_HEIGHT:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, value, &val, error))
        attribute = pango2_attr_line_height_new (g_value_get_double (&val));
      break;
    case PANGO2_ATTR_ABSOLUTE_LINE_HEIGHT:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango2_attr_line_height_new_absolute (g_value_get_int (&val) * PANGO2_SCALE);
      break;
    case PANGO2_ATTR_LINE_SPACING:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango2_attr_line_spacing_new (g_value_get_int (&val) * PANGO2_SCALE);
      break;
    case PANGO2_ATTR_TEXT_TRANSFORM:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_TEXT_TRANSFORM, value, &val, error))
        attribute = pango2_attr_text_transform_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_WORD:
      attribute = pango2_attr_word_new ();
      break;
    case PANGO2_ATTR_SENTENCE:
      attribute = pango2_attr_sentence_new ();
      break;
    case PANGO2_ATTR_PARAGRAPH:
      attribute = pango2_attr_paragraph_new ();
      break;
    case PANGO2_ATTR_BASELINE_SHIFT:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_BASELINE_SHIFT, value, &val, NULL))
        attribute = pango2_attr_baseline_shift_new (g_value_get_enum (&val));
      else if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, NULL))
        attribute = pango2_attr_baseline_shift_new (g_value_get_enum (&val));
      else
        g_set_error (error,
                     GTK_BUILDER_ERROR,
                     GTK_BUILDER_ERROR_INVALID_VALUE,
                     "Could not parse '%s' as baseline shift value", value);
      break;
    case PANGO2_ATTR_FONT_SCALE:
      if (gtk_builder_value_from_string_type (builder, PANGO2_TYPE_FONT_SCALE, value, &val, error))
        attribute = pango2_attr_font_scale_new (g_value_get_enum (&val));
      break;
    case PANGO2_ATTR_INVALID:
    case PANGO2_ATTR_SHAPE:
    default:
      break;
    }

  g_value_unset (&val);

  return attribute;
}

void
gtk_pango_attribute_start_element (GtkBuildableParseContext  *context,
                                   const char                *element_name,
                                   const char               **names,
                                   const char               **values,
                                   gpointer                   user_data,
                                   GError                   **error)
{
  GtkPangoAttributeParserData *data = user_data;

  if (strcmp (element_name, "attribute") == 0)
    {
      Pango2Attribute *attr = NULL;
      const char *name = NULL;
      const char *value = NULL;
      const char *start = NULL;
      const char *end = NULL;
      guint start_val = 0;
      guint end_val = G_MAXUINT;
      GValue val = G_VALUE_INIT;

      if (!_gtk_builder_check_parent (data->builder, context, "attributes", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "value", &value,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "start", &start,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "end", &end,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (start)
        {
          if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, start, &val, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }
          start_val = g_value_get_uint (&val);
          g_value_unset (&val);
        }

      if (end)
        {
          if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, end, &val, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }
          end_val = g_value_get_uint (&val);
          g_value_unset (&val);
        }

      attr = attribute_from_text (data->builder, name, value, error);
      if (!attr)
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      pango2_attribute_set_range (attr, start_val, end_val);

      if (!data->attrs)
        data->attrs = pango2_attr_list_new ();

      pango2_attr_list_insert (data->attrs, attr);
    }
  else if (strcmp (element_name, "attributes") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkWidget", element_name,
                                        error);
    }
}
