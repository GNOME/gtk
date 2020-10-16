/* gtkatspitextbuffer.c - GtkTextBuffer-related utilities for AT-SPI
 *
 * Copyright (c) 2020 Red Hat, Inc.
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

#include "config.h"
#include "gtkatspitextbufferprivate.h"
#include "gtkatspipangoprivate.h"
#include "gtktextviewprivate.h"

static const char *
gtk_justification_to_string (GtkJustification just)
{
  switch (just)
    {
    case GTK_JUSTIFY_LEFT:
      return "left";
    case GTK_JUSTIFY_RIGHT:
      return "right";
    case GTK_JUSTIFY_CENTER:
      return "center";
    case GTK_JUSTIFY_FILL:
      return "fill";
    default:
      g_assert_not_reached ();
    }
}

static const char *
gtk_text_direction_to_string (GtkTextDirection direction)
{
  switch (direction)
    {
    case GTK_TEXT_DIR_NONE:
      return "none";
    case GTK_TEXT_DIR_LTR:
      return "ltr";
    case GTK_TEXT_DIR_RTL:
      return "rtl";
    default:
      g_assert_not_reached ();
    }
}

void
gtk_text_view_add_default_attributes (GtkTextView     *view,
                                      GVariantBuilder *builder)
{
  GtkTextAttributes *text_attrs;
  PangoFontDescription *font;
  char *value;

  text_attrs = gtk_text_view_get_default_attributes (view);

  font = text_attrs->font;

  if (font)
    gtk_pango_get_font_attributes (font, builder);

  g_variant_builder_add (builder, "{ss}", "justification",
                         gtk_justification_to_string (text_attrs->justification));
  g_variant_builder_add (builder, "{ss}", "direction",
                         gtk_text_direction_to_string (text_attrs->direction));
  g_variant_builder_add (builder, "{ss}", "wrap-mode",
                         pango_wrap_mode_to_string ((PangoWrapMode)text_attrs->wrap_mode));
  g_variant_builder_add (builder, "{ss}", "editable",
                         text_attrs->editable ? "true" : "false");
  g_variant_builder_add (builder, "{ss}", "invisible",
                         text_attrs->invisible ? "true" : "false");
  g_variant_builder_add (builder, "{ss}", "bg-full-height",
                         text_attrs->bg_full_height ? "true" : "false");
  g_variant_builder_add (builder, "{ss}", "strikethrough",
                         text_attrs->appearance.strikethrough ? "true" : "false");
  g_variant_builder_add (builder, "{ss}", "underline",
                         pango_underline_to_string (text_attrs->appearance.underline));

  value = g_strdup_printf ("%u,%u,%u",
                           (guint)(text_attrs->appearance.bg_rgba->red * 65535),
                           (guint)(text_attrs->appearance.bg_rgba->green * 65535),
                           (guint)(text_attrs->appearance.bg_rgba->blue * 65535));
  g_variant_builder_add (builder, "{ss}", "bg-color", value);
  g_free (value);

  value = g_strdup_printf ("%u,%u,%u",
                           (guint)(text_attrs->appearance.fg_rgba->red * 65535),
                           (guint)(text_attrs->appearance.fg_rgba->green * 65535),
                           (guint)(text_attrs->appearance.fg_rgba->blue * 65535));
  g_variant_builder_add (builder, "{ss}", "bg-color", value);
  g_free (value);

  value = g_strdup_printf ("%g", text_attrs->font_scale);
  g_variant_builder_add (builder, "{ss}", "scale", value);
  g_free (value);

  value = g_strdup ((gchar *)(text_attrs->language));
  g_variant_builder_add (builder, "{ss}", "language", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->appearance.rise);
  g_variant_builder_add (builder, "{ss}", "rise", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->pixels_inside_wrap);
  g_variant_builder_add (builder, "{ss}", "pixels-inside-wrap", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->pixels_below_lines);
  g_variant_builder_add (builder, "{ss}", "pixels-below-lines", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->pixels_above_lines);
  g_variant_builder_add (builder, "{ss}", "pixels-above-lines", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->indent);
  g_variant_builder_add (builder, "{ss}", "indent", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->left_margin);
  g_variant_builder_add (builder, "{ss}", "left-margin", value);
  g_free (value);

  value = g_strdup_printf ("%i", text_attrs->right_margin);
  g_variant_builder_add (builder, "{ss}", "right-margin", value);
  g_free (value);

  gtk_text_attributes_unref (text_attrs);
}

void
gtk_text_buffer_get_run_attributes (GtkTextBuffer   *buffer,
                                    GVariantBuilder *builder,
                                    int              offset,
                                    int             *start_offset,
                                    int             *end_offset)
{
  GtkTextIter iter;
  GSList *tags, *temp_tags;
  gdouble scale = 1;
  gboolean val_set = FALSE;

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  gtk_text_iter_forward_to_tag_toggle (&iter, NULL);
  *end_offset = gtk_text_iter_get_offset (&iter);

  gtk_text_iter_backward_to_tag_toggle (&iter, NULL);
  *start_offset = gtk_text_iter_get_offset (&iter);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  tags = gtk_text_iter_get_tags (&iter);
  tags = g_slist_reverse (tags);

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoStyle style;

      g_object_get (tag,
                    "style-set", &val_set,
                    "style", &style,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "style", pango_style_to_string (style));
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoVariant variant;

      g_object_get (tag,
                    "variant-set", &val_set,
                    "variant", &variant,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "variant", pango_variant_to_string (variant));
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoStretch stretch;

      g_object_get (tag,
                    "stretch-set", &val_set,
                    "stretch", &stretch,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "stretch", pango_stretch_to_string (stretch));
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      GtkJustification justification;

      g_object_get (tag,
                    "justification-set", &val_set,
                    "justification", &justification,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "justification", gtk_justification_to_string (justification));
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      GtkTextDirection direction;

      g_object_get (tag, "direction", &direction, NULL);
      if (direction != GTK_TEXT_DIR_NONE)
        {
          val_set = TRUE;
          g_variant_builder_add (builder, "{ss}", "direction", gtk_text_direction_to_string (direction));
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      GtkWrapMode wrap_mode;

      g_object_get (tag,
                    "wrap-mode-set", &val_set,
                    "wrap-mode", &wrap_mode,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "wrap-mode", pango_wrap_mode_to_string ((PangoWrapMode)wrap_mode));
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "foreground-set", &val_set, NULL);
      if (val_set)
        {
          GdkRGBA *rgba;
          char *value;

          g_object_get (tag, "foreground", &rgba, NULL);
          value = g_strdup_printf ("%u,%u,%u",
                                   (guint) rgba->red * 65535,
                                   (guint) rgba->green * 65535,
                                   (guint) rgba->blue * 65535);
          gdk_rgba_free (rgba);
          g_variant_builder_add (builder, "{ss}", "fg-color", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "background-set", &val_set, NULL);
      if (val_set)
        {
          GdkRGBA *rgba;
          char *value;

          g_object_get (tag, "background-rgba", &rgba, NULL);
          value = g_strdup_printf ("%u,%u,%u",
                                   (guint) rgba->red * 65535,
                                   (guint) rgba->green * 65535,
                                   (guint) rgba->blue * 65535);
          gdk_rgba_free (rgba);
          g_variant_builder_add (builder, "{ss}", "bg-color", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "family-set", &val_set, NULL);

      if (val_set)
        {
          char *value;
          g_object_get (tag, "family", &value, NULL);
          g_variant_builder_add (builder, "{ss}", "family-name", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      g_object_get (tag, "language-set", &val_set, NULL);

      if (val_set)
        {
          char *value;
          g_object_get (tag, "language", &value, NULL);
          g_variant_builder_add (builder, "{ss}", "language", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int weight;

      g_object_get (tag,
                    "weight-set", &val_set,
                    "weight", &weight,
                    NULL);

      if (val_set)
        {
          char *value;
          value = g_strdup_printf ("%d", weight);
          g_variant_builder_add (builder, "{ss}", "weight", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  /* scale is special as the effective value is the product
   * of all specified values
   */
  temp_tags = tags;
  while (temp_tags)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      gboolean scale_set;

      g_object_get (tag, "scale-set", &scale_set, NULL);
      if (scale_set)
        {
          double font_scale;
          g_object_get (tag, "scale", &font_scale, NULL);
          val_set = TRUE;
          scale *= font_scale;
        }
      temp_tags = temp_tags->next;
    }
  if (val_set)
    {
      char *value = g_strdup_printf ("%g", scale);
      g_variant_builder_add (builder, "{ss}", "scale", value);
      g_free (value);
    }
  val_set = FALSE;

 temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int size;

      g_object_get (tag,
                    "size-set", &val_set,
                    "size", &size,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", size);
          g_variant_builder_add (builder, "{ss}", "size", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      gboolean strikethrough;

      g_object_get (tag,
                    "strikethrough-set", &val_set,
                    "strikethrough", &strikethrough,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "strikethrough", strikethrough ? "true" : "false");
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoUnderline underline;

      g_object_get (tag,
                    "underline-set", &val_set,
                    "underline", &underline,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "underline",
                               pango_underline_to_string (underline));
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int rise;

      g_object_get (tag,
                    "rise-set", &val_set,
                    "rise", &rise,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", rise);
          g_variant_builder_add (builder, "{ss}", "rise", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      gboolean bg_full_height;

      g_object_get (tag,
                    "background-full-height-set", &val_set,
                    "background-full-height", &bg_full_height,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "bg-full-height", bg_full_height ? "true" : "false");
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int pixels;

      g_object_get (tag,
                    "pixels-inside-wrap-set", &val_set,
                    "pixels-inside-wrap", &pixels,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", pixels);
          g_variant_builder_add (builder, "{ss}", "pixels-inside-wrap", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int pixels;

      g_object_get (tag,
                    "pixels-below-lines-set", &val_set,
                    "pixels-below-lines", &pixels,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", pixels);
          g_variant_builder_add (builder, "{ss}", "pixels-below-lines", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int pixels;

      g_object_get (tag,
                    "pixels-above-lines-set", &val_set,
                    "pixels-above-lines", &pixels,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", pixels);
          g_variant_builder_add (builder, "{ss}", "pixels-above-lines", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      gboolean editable;

      g_object_get (tag,
                    "editable-set", &val_set,
                    "editable", &editable,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "editable", editable ? "true" : "false");
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      gboolean invisible;

      g_object_get (tag,
                    "invisible-set", &val_set,
                    "invisible", &invisible,
                    NULL);
      if (val_set)
        g_variant_builder_add (builder, "{ss}", "invisible", invisible ? "true" : "false");
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

 temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int indent;

      g_object_get (tag,
                    "indent-set", &val_set,
                    "indent", &indent,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", indent);
          g_variant_builder_add (builder, "{ss}", "indent", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int margin;

      g_object_get (tag,
                    "right-margin-set", &val_set,
                    "right-margin", &margin,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", margin);
          g_variant_builder_add (builder, "{ss}", "right-margin", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      int margin;

      g_object_get (tag,
                    "left-margin-set", &val_set,
                    "left-margin", &margin,
                    NULL);
      if (val_set)
        {
          char *value = g_strdup_printf ("%i", margin);
          g_variant_builder_add (builder, "{ss}", "left-margin", value);
          g_free (value);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  g_slist_free (tags);
}

char *
gtk_text_view_get_text_before (GtkTextView           *view,
                               int                    offset,
                               AtspiTextBoundaryType  boundary_type,
                               int                   *start_offset,
                               int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_backward_char (&start);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      end = start;
      gtk_text_iter_backward_word_start (&start);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (&start) &&
          !gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      end = start;
      gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      end = start;
      gtk_text_iter_backward_sentence_start (&start);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (&start) &&
          !gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      end = start;
      gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
      gtk_text_view_backward_display_line_start (view, &start);
      end = start;
      gtk_text_view_backward_display_line (view, &start);
      gtk_text_view_backward_display_line_start (view, &start);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_END:
      gtk_text_view_backward_display_line_start (view, &start);
      if (!gtk_text_iter_is_start (&start))
        {
          gtk_text_view_backward_display_line (view, &start);
          end = start;
          gtk_text_view_forward_display_line_end (view, &end);
          if (!gtk_text_iter_is_start (&start))
            {
              if (gtk_text_view_backward_display_line (view, &start))
                gtk_text_view_forward_display_line_end (view, &start);
              else
                gtk_text_iter_set_offset (&start, 0);
            }
        }
      else
        end = start;
      break;

    default:
      g_assert_not_reached ();
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

char *
gtk_text_view_get_text_at (GtkTextView           *view,
                           int                    offset,
                           AtspiTextBoundaryType  boundary_type,
                           int                   *start_offset,
                           int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      if (gtk_text_iter_inside_word (&end))
        gtk_text_iter_forward_word_end (&end);
      while (!gtk_text_iter_starts_word (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (&start) &&
          !gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      gtk_text_iter_forward_word_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      if (gtk_text_iter_inside_sentence (&end))
        gtk_text_iter_forward_sentence_end (&end);
      while (!gtk_text_iter_starts_sentence (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (&start) &&
          !gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      gtk_text_iter_forward_sentence_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
      gtk_text_view_backward_display_line_start (view, &start);
      gtk_text_view_forward_display_line (view, &end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_END:
      gtk_text_view_backward_display_line_start (view, &start);
      if (!gtk_text_iter_is_start (&start))
        {
          gtk_text_view_backward_display_line (view, &start);
          gtk_text_view_forward_display_line_end (view, &start);
        }
      gtk_text_view_forward_display_line_end (view, &end);
      break;

    default:
      g_assert_not_reached ();
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

char *
gtk_text_view_get_text_after (GtkTextView           *view,
                              int                    offset,
                              AtspiTextBoundaryType  boundary_type,
                              int                   *start_offset,
                              int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (&start);
      gtk_text_iter_forward_chars (&end, 2);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (gtk_text_iter_inside_word (&end))
        gtk_text_iter_forward_word_end (&end);
      while (!gtk_text_iter_starts_word (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      start = end;
      if (!gtk_text_iter_is_end (&end))
        {
          gtk_text_iter_forward_word_end (&end);
          while (!gtk_text_iter_starts_word (&end))
            {
              if (!gtk_text_iter_forward_char (&end))
                break;
            }
        }
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      gtk_text_iter_forward_word_end (&end);
      start = end;
      if (!gtk_text_iter_is_end (&end))
        gtk_text_iter_forward_word_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (gtk_text_iter_inside_sentence (&end))
        gtk_text_iter_forward_sentence_end (&end);
      while (!gtk_text_iter_starts_sentence (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      start = end;
      if (!gtk_text_iter_is_end (&end))
        {
          gtk_text_iter_forward_sentence_end (&end);
          while (!gtk_text_iter_starts_sentence (&end))
            {
              if (!gtk_text_iter_forward_char (&end))
                break;
            }
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      gtk_text_iter_forward_sentence_end (&end);
      start = end;
      if (!gtk_text_iter_is_end (&end))
        gtk_text_iter_forward_sentence_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
      gtk_text_view_forward_display_line (view, &end);
      start = end;
      gtk_text_view_forward_display_line (view, &end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_END:
      gtk_text_view_forward_display_line_end (view, &end);
      start = end;
      gtk_text_view_forward_display_line (view, &end);
      gtk_text_view_forward_display_line_end (view, &end);
      break;

    default:
      g_assert_not_reached ();
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

char *
gtk_text_view_get_string_at (GtkTextView           *view,
                             int                    offset,
                             AtspiTextGranularity   granularity,
                             int                   *start_offset,
                             int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  if (granularity == ATSPI_TEXT_GRANULARITY_CHAR)
    {
      gtk_text_iter_forward_char (&end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_WORD)
    {
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      gtk_text_iter_forward_word_end (&end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_SENTENCE)
    {
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      gtk_text_iter_forward_sentence_end (&end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_LINE)
    {
      if (!gtk_text_view_starts_display_line (view, &start))
        gtk_text_view_backward_display_line (view, &start);
      gtk_text_view_forward_display_line_end (view, &end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_PARAGRAPH)
    {
      gtk_text_iter_set_line_offset (&start, 0);
      gtk_text_iter_forward_to_line_end (&end);
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}
