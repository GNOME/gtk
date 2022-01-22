/* gtkatspipango.c - pango-related utilities for AT-SPI
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

#include "config.h"
#include "gtkatspipangoprivate.h"

const char *
pango_style_to_string (PangoStyle style)
{
  switch (style)
    {
    case PANGO_STYLE_NORMAL:
      return "normal";
    case PANGO_STYLE_OBLIQUE:
      return "oblique";
    case PANGO_STYLE_ITALIC:
      return "italic";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_variant_to_string (PangoVariant variant)
{
  switch (variant)
    {
    case PANGO_VARIANT_NORMAL:
      return "normal";
    case PANGO_VARIANT_SMALL_CAPS:
      return "small_caps";
    case PANGO_VARIANT_ALL_SMALL_CAPS:
      return "all_small_caps";
    case PANGO_VARIANT_PETITE_CAPS:
      return "petite_caps";
    case PANGO_VARIANT_ALL_PETITE_CAPS:
      return "all_petite_caps";
    case PANGO_VARIANT_UNICASE:
      return "unicase";
    case PANGO_VARIANT_TITLE_CAPS:
      return "title_caps";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_stretch_to_string (PangoStretch stretch)
{
  switch (stretch)
    {
    case PANGO_STRETCH_ULTRA_CONDENSED:
      return "ultra_condensed";
    case PANGO_STRETCH_EXTRA_CONDENSED:
      return "extra_condensed";
    case PANGO_STRETCH_CONDENSED:
      return "condensed";
    case PANGO_STRETCH_SEMI_CONDENSED:
      return "semi_condensed";
    case PANGO_STRETCH_NORMAL:
      return "normal";
    case PANGO_STRETCH_SEMI_EXPANDED:
      return "semi_expanded";
    case PANGO_STRETCH_EXPANDED:
      return "expanded";
    case PANGO_STRETCH_EXTRA_EXPANDED:
      return "extra_expanded";
    case PANGO_STRETCH_ULTRA_EXPANDED:
      return "ultra_expanded";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_underline_to_string (PangoUnderline value)
{
  switch (value)
    {
    case PANGO_UNDERLINE_NONE:
      return "none";
    case PANGO_UNDERLINE_SINGLE:
    case PANGO_UNDERLINE_SINGLE_LINE:
      return "single";
    case PANGO_UNDERLINE_DOUBLE:
    case PANGO_UNDERLINE_DOUBLE_LINE:
      return "double";
    case PANGO_UNDERLINE_LOW:
      return "low";
    case PANGO_UNDERLINE_ERROR:
    case PANGO_UNDERLINE_ERROR_LINE:
      return "error";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_wrap_mode_to_string (PangoWrapMode mode)
{
  switch (mode)
    {
    case PANGO_WRAP_WORD:
      return "word";
    case PANGO_WRAP_CHAR:
      return "char";
    case PANGO_WRAP_WORD_CHAR:
      return "word-char";
    default:
      g_assert_not_reached ();
    }
}

static const char *
pango_align_to_string (PangoAlignment align)
{
  switch (align)
    {
    case PANGO_ALIGN_LEFT:
      return "left";
    case PANGO_ALIGN_CENTER:
      return "center";
    case PANGO_ALIGN_RIGHT:
      return "right";
    default:
      g_assert_not_reached ();
    }
}

void
gtk_pango_get_font_attributes (PangoFontDescription *font,
                               GVariantBuilder      *builder)
{
  char buf[60];

  g_variant_builder_add (builder, "{ss}", "style",
                         pango_style_to_string (pango_font_description_get_style (font)));
  g_variant_builder_add (builder, "{ss}", "variant",
                         pango_variant_to_string (pango_font_description_get_variant (font)));
  g_variant_builder_add (builder, "{ss}", "stretch",
                         pango_stretch_to_string (pango_font_description_get_stretch (font)));
  g_variant_builder_add (builder, "{ss}", "family-name",
                         pango_font_description_get_family (font));

  g_snprintf (buf, 60, "%d", pango_font_description_get_weight (font));
  g_variant_builder_add (builder, "{ss}", "weight", buf);
  g_snprintf (buf, 60, "%i", pango_font_description_get_size (font) / PANGO_SCALE);
  g_variant_builder_add (builder, "{ss}", "size", buf);
}

/*
 * gtk_pango_get_default_attributes:
 * @layout: the `PangoLayout` from which to get attributes
 * @builder: a `GVariantBuilder` to add to
 *
 * Adds the default text attributes from @layout to @builder,
 * after translating them from Pango attributes to atspi
 * attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the `AtkText` interface in widgets using Pango
 * layouts.
 *
 * Returns: the modified @attributes
 */
void
gtk_pango_get_default_attributes (PangoLayout     *layout,
                                  GVariantBuilder *builder)
{
  PangoContext *context;

  context = pango_layout_get_context (layout);
  if (context)
    {
      PangoLanguage *language;
      PangoFontDescription *font;

      language = pango_context_get_language (context);
      if (language)
        g_variant_builder_add (builder, "{ss}", "language",
                               pango_language_to_string (language));

      font = pango_context_get_font_description (context);
      if (font)
        gtk_pango_get_font_attributes (font, builder);
    }

  g_variant_builder_add (builder, "{ss}", "justification",
                         pango_align_to_string (pango_layout_get_alignment (layout)));

  g_variant_builder_add (builder, "{ss}", "wrap-mode",
                         pango_wrap_mode_to_string (pango_layout_get_wrap (layout)));
  g_variant_builder_add (builder, "{ss}", "strikethrough", "false");
  g_variant_builder_add (builder, "{ss}", "underline", "false");
  g_variant_builder_add (builder, "{ss}", "rise", "0");
  g_variant_builder_add (builder, "{ss}", "scale", "1");
  g_variant_builder_add (builder, "{ss}", "bg-full-height", "0");
  g_variant_builder_add (builder, "{ss}", "pixels-inside-wrap", "0");
  g_variant_builder_add (builder, "{ss}", "pixels-below-lines", "0");
  g_variant_builder_add (builder, "{ss}", "pixels-above-lines", "0");
  g_variant_builder_add (builder, "{ss}", "editable", "false");
  g_variant_builder_add (builder, "{ss}", "invisible", "false");
  g_variant_builder_add (builder, "{ss}", "indent", "0");
  g_variant_builder_add (builder, "{ss}", "right-margin", "0");
  g_variant_builder_add (builder, "{ss}", "left-margin", "0");
}

/*
 * gtk_pango_get_run_attributes:
 * @layout: the `PangoLayout` to get the attributes from
 * @builder: `GVariantBuilder` to add to
 * @offset: the offset at which the attributes are wanted
 * @start_offset: return location for the starting offset
 *    of the current run
 * @end_offset: return location for the ending offset of the
 *    current run
 *
 * Finds the “run” around index (i.e. the maximal range of characters
 * where the set of applicable attributes remains constant) and
 * returns the starting and ending offsets for it.
 *
 * The attributes for the run are added to @attributes, after
 * translating them from Pango attributes to atspi attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the #AtkText interface in widgets using Pango
 * layouts.
 */
void
gtk_pango_get_run_attributes (PangoLayout     *layout,
                              GVariantBuilder *builder,
                              int              offset,
                              int             *start_offset,
                              int             *end_offset)
{
  PangoAttrIterator *iter;
  PangoAttrList *attr;
  PangoAttrString *pango_string;
  PangoAttrInt *pango_int;
  PangoAttrColor *pango_color;
  PangoAttrLanguage *pango_lang;
  PangoAttrFloat *pango_float;
  int index, start_index, end_index;
  gboolean is_next;
  glong len;
  const char *text;
  char *value;
  const char *val;

  text = pango_layout_get_text (layout);
  len = g_utf8_strlen (text, -1);

  /* Grab the attributes of the PangoLayout, if any */
  attr = pango_layout_get_attributes (layout);

  if (attr == NULL)
    {
      *start_offset = 0;
      *end_offset = len;
      return;
    }

  iter = pango_attr_list_get_iterator (attr);
  /* Get invariant range offsets */
  /* If offset out of range, set offset in range */
  if (offset > len)
    offset = len;
  else if (offset < 0)
    offset = 0;

  index = g_utf8_offset_to_pointer (text, offset) - text;
  pango_attr_iterator_range (iter, &start_index, &end_index);
  is_next = TRUE;
  while (is_next)
    {
      if (index >= start_index && index < end_index)
        {
          *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
          if (end_index == G_MAXINT) /* Last iterator */
            end_index = len;

          *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
          break;
        }
      is_next = pango_attr_iterator_next (iter);
      pango_attr_iterator_range (iter, &start_index, &end_index);
    }

  /* Get attributes */
  pango_string = (PangoAttrString *) pango_attr_iterator_get (iter, PANGO_ATTR_FAMILY);
  if (pango_string != NULL)
    {
      value = g_strdup_printf ("%s", pango_string->value);
      g_variant_builder_add (builder, "{ss}", "family-name", value);
      g_free (value);
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_STYLE);
  if (pango_int != NULL)
    g_variant_builder_add (builder, "{ss}", "style", pango_style_to_string (pango_int->value));

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_WEIGHT);
  if (pango_int != NULL)
    {
      value = g_strdup_printf ("%i", pango_int->value);
      g_variant_builder_add (builder, "{ss}", "weight", value);
      g_free (value);
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_VARIANT);
  if (pango_int != NULL)
    g_variant_builder_add (builder, "{ss}", "variant",
                           pango_variant_to_string (pango_int->value));

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_STRETCH);
  if (pango_int != NULL)
    g_variant_builder_add (builder, "{ss}", "stretch",
                           pango_stretch_to_string (pango_int->value));

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_SIZE);
  if (pango_int != NULL)
    {
      value = g_strdup_printf ("%i", pango_int->value / PANGO_SCALE);
      g_variant_builder_add (builder, "{ss}", "size", value);
      g_free (value);
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE);
  if (pango_int != NULL)
    g_variant_builder_add (builder, "{ss}", "underline",
                           pango_underline_to_string (pango_int->value));

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_STRIKETHROUGH);
  if (pango_int != NULL)
    {
      if (pango_int->value)
        val = "true";
      else
        val = "false";
      g_variant_builder_add (builder, "{ss}", "strikethrough", val);
    }

  pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_RISE);
  if (pango_int != NULL)
    {
      value = g_strdup_printf ("%i", pango_int->value);
      g_variant_builder_add (builder, "{ss}", "rise", value);
      g_free (value);
    }

  pango_lang = (PangoAttrLanguage *) pango_attr_iterator_get (iter, PANGO_ATTR_LANGUAGE);
  if (pango_lang != NULL)
    {
      g_variant_builder_add (builder, "{ss}", "language",
                             pango_language_to_string (pango_lang->value));
    }

  pango_float = (PangoAttrFloat *) pango_attr_iterator_get (iter, PANGO_ATTR_SCALE);
  if (pango_float != NULL)
    {
      value = g_strdup_printf ("%g", pango_float->value);
      g_variant_builder_add (builder, "{ss}", "scale", value);
      g_free (value);
    }

  pango_color = (PangoAttrColor *) pango_attr_iterator_get (iter, PANGO_ATTR_FOREGROUND);
  if (pango_color != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u",
                               pango_color->color.red,
                               pango_color->color.green,
                               pango_color->color.blue);
      g_variant_builder_add (builder, "{ss}", "fg-color", value);
      g_free (value);
    }

  pango_color = (PangoAttrColor *) pango_attr_iterator_get (iter, PANGO_ATTR_BACKGROUND);
  if (pango_color != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u",
                               pango_color->color.red,
                               pango_color->color.green,
                               pango_color->color.blue);
      g_variant_builder_add (builder, "{ss}", "bg-color", value);
      g_free (value);
    }
  pango_attr_iterator_destroy (iter);
}

/*
 * gtk_pango_move_chars:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of characters to move from @offset
 *
 * Returns the position that is @count characters from the
 * given @offset. @count may be positive or negative.
 *
 * For the purpose of this function, characters are defined
 * by what Pango considers cursor positions.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_chars (PangoLayout *layout,
                      int          offset,
                      int          count)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_cursor_position);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_cursor_position);

      count++;
    }

  return offset;
}

/*
 * gtk_pango_move_words:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of words to move from @offset
 *
 * Returns the position that is @count words from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a word end, otherwise it will be a word start.
 * See the Pango documentation for details on how
 * word starts and ends are defined.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_words (PangoLayout  *layout,
                      int           offset,
                      int           count)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_word_end);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_word_start);

      count++;
    }

  return offset;
}

/*
 * gtk_pango_move_sentences:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of sentences to move from @offset
 *
 * Returns the position that is @count sentences from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a sentence end, otherwise it will be a sentence start.
 * See the Pango documentation for details on how
 * sentence starts and ends are defined.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_sentences (PangoLayout  *layout,
                          int           offset,
                          int           count)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_sentence_end);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_sentence_start);

      count++;
    }

  return offset;
}

#if 0
/*
 * gtk_pango_move_lines:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @count: the number of lines to move from @offset
 *
 * Returns the position that is @count lines from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is negative, the returned position will
 * be the start of a line, else it will be the end of
 * line.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_lines (PangoLayout *layout,
                      int          offset,
                      int          count)
{
  GSList *lines, *l;
  PangoLayoutLine *line;
  int num;
  const char *text;
  int pos, line_pos;
  int index;
  int len;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  lines = pango_layout_get_lines (layout);
  line = NULL;

  num = 0;
  for (l = lines; l; l = l->next)
    {
      line = l->data;
      if (index < line->start_index + line->length)
        break;
      num++;
    }

  if (count < 0)
    {
      num += count;
      if (num < 0)
        num = 0;

      line = g_slist_nth_data (lines, num);

      return g_utf8_pointer_to_offset (text, text + line->start_index);
    }
  else
    {
      line_pos = index - line->start_index;

      len = g_slist_length (lines);
      num += count;
      if (num >= len || (count == 0 && num == len - 1))
        return g_utf8_strlen (text, -1) - 1;

      line = l->data;
      pos = line->start_index + line_pos;
      if (pos >= line->start_index + line->length)
        pos = line->start_index + line->length - 1;

      return g_utf8_pointer_to_offset (text, text + pos);
    }
}
#endif

/*
 * gtk_pango_is_inside_word:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a word.
 *
 * Returns: %TRUE if @offset is inside a word
 */
static gboolean
gtk_pango_is_inside_word (PangoLayout  *layout,
                          int           offset)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_word_start || attrs[offset].is_word_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_word_start;

  return FALSE;
}

/*
 * gtk_pango_is_inside_sentence:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a sentence.
 *
 * Returns: %TRUE if @offset is inside a sentence
 */
static gboolean
gtk_pango_is_inside_sentence (PangoLayout  *layout,
                              int           offset)
{
  const PangoLogAttr *attrs;
  int n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_sentence_start || attrs[offset].is_sentence_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_sentence_start;

  return FALSE;
}

static void
pango_layout_get_line_before (PangoLayout           *layout,
                              int                    offset,
                              AtspiTextBoundaryType  boundary_type,
                              int                   *start_offset,
                              int                   *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL, *prev_prev_line = NULL;
  int index, start_index, length, end_index;
  int prev_start_index, prev_length;
  int prev_prev_start_index, prev_prev_length;
  const char *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = pango_layout_line_get_start_index (line);
      length = pango_layout_line_get_length (line);
      end_index = start_index + length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (prev_line)
            {
              switch (boundary_type)
                {
                case ATSPI_TEXT_BOUNDARY_LINE_START:
                  end_index = start_index;
                  start_index = prev_start_index;
                  break;
                case ATSPI_TEXT_BOUNDARY_LINE_END:
                  if (prev_prev_line)
                    start_index = prev_prev_start_index + prev_prev_length;
                  else
                    start_index = 0;
                  end_index = prev_start_index + prev_length;
                  break;
                case ATSPI_TEXT_BOUNDARY_CHAR:
                case ATSPI_TEXT_BOUNDARY_WORD_START:
                case ATSPI_TEXT_BOUNDARY_WORD_END:
                case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
                case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
                default:
                  g_assert_not_reached();
                }
            }
          else
            start_index = end_index = 0;

          found = TRUE;
          break;
        }

      prev_prev_line = prev_line;
      prev_prev_start_index = prev_start_index;
      prev_prev_length = prev_length;
      prev_line = line;
      prev_start_index = start_index;
      prev_length = length;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_start_index + prev_length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango_layout_get_line_at (PangoLayout           *layout,
                          int                    offset,
                          AtspiTextBoundaryType  boundary_type,
                          int                   *start_offset,
                          int                   *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  int index, start_index, length, end_index;
  const char *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = pango_layout_line_get_start_index (line);
      length = pango_layout_line_get_length (line);
      end_index = start_index + length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          switch (boundary_type)
            {
            case ATSPI_TEXT_BOUNDARY_LINE_START:
              if (pango_layout_iter_next_line (iter))
                end_index = pango_layout_line_get_start_index (pango_layout_iter_get_line (iter));
              break;
            case ATSPI_TEXT_BOUNDARY_LINE_END:
              if (prev_line)
                start_index = pango_layout_line_get_start_index (prev_line) + pango_layout_line_get_length (prev_line);
              break;
            case ATSPI_TEXT_BOUNDARY_CHAR:
            case ATSPI_TEXT_BOUNDARY_WORD_START:
            case ATSPI_TEXT_BOUNDARY_WORD_END:
            case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
            case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
            default:
              g_assert_not_reached();
            }

          found = TRUE;
          break;
        }

      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = pango_layout_line_get_start_index (prev_line) + pango_layout_line_get_length (prev_line);
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango_layout_get_line_after (PangoLayout           *layout,
                             int                    offset,
                             AtspiTextBoundaryType  boundary_type,
                             int                   *start_offset,
                             int                   *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  int index, start_index, length, end_index;
  const char *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = pango_layout_line_get_start_index (line);
      length = pango_layout_line_get_length (line);
      end_index = start_index + length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (pango_layout_iter_next_line (iter))
            {
              line = pango_layout_iter_get_line (iter);
              switch (boundary_type)
                {
                case ATSPI_TEXT_BOUNDARY_LINE_START:
                  start_index = pango_layout_line_get_start_index (line);
                  if (pango_layout_iter_next_line (iter))
                    end_index = pango_layout_line_get_start_index (pango_layout_iter_get_line (iter));
                  else
                    end_index = start_index + pango_layout_line_get_length (line);
                  break;
                case ATSPI_TEXT_BOUNDARY_LINE_END:
                  start_index = end_index;
                  end_index = pango_layout_line_get_start_index (line) + pango_layout_line_get_length (line);
                  break;
                case ATSPI_TEXT_BOUNDARY_CHAR:
                case ATSPI_TEXT_BOUNDARY_WORD_START:
                case ATSPI_TEXT_BOUNDARY_WORD_END:
                case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
                case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
                default:
                  g_assert_not_reached();
                }
            }
          else
            start_index = end_index;

          found = TRUE;
          break;
        }

      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = pango_layout_line_get_start_index (prev_line) + pango_layout_line_get_length (prev_line);
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

/*
 * gtk_pango_get_text_before:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @boundary_type: a #AtspiTextBoundaryType
 * @start_offset: return location for the start of the returned text
 * @end_offset: return location for the end of the return text
 *
 * Gets a slice of the text from @layout before @offset.
 *
 * The @boundary_type determines the size of the returned slice of
 * text. For the exact semantics of this function, see
 * atk_text_get_text_before_offset().
 *
 * Returns: a newly allocated string containing a slice of text
 *   from layout. Free with g_free().
 */
char *
gtk_pango_get_text_before (PangoLayout           *layout,
                           int                    offset,
                           AtspiTextBoundaryType  boundary_type,
                           int                   *start_offset,
                           int                   *end_offset)
{
  const char *text;
  int start, end;
  const PangoLogAttr *attrs;
  int n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      start = gtk_pango_move_chars (layout, start, -1);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (!attrs[start].is_word_start)
        start = gtk_pango_move_words (layout, start, -1);
      end = start;
      start = gtk_pango_move_words (layout, start, -1);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      if (gtk_pango_is_inside_word (layout, start) &&
          !attrs[start].is_word_start)
        start = gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = gtk_pango_move_chars (layout, start, -1);
      end = start;
      start = gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = gtk_pango_move_chars (layout, start, -1);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (!attrs[start].is_sentence_start)
        start = gtk_pango_move_sentences (layout, start, -1);
      end = start;
      start = gtk_pango_move_sentences (layout, start, -1);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_pango_is_inside_sentence (layout, start) &&
          !attrs[start].is_sentence_start)
        start = gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = gtk_pango_move_chars (layout, start, -1);
      end = start;
      start = gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = gtk_pango_move_chars (layout, start, -1);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
    case ATSPI_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_before (layout, offset, boundary_type, &start, &end);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}

/*
 * gtk_pango_get_text_after:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @boundary_type: a #AtspiTextBoundaryType
 * @start_offset: return location for the start of the returned text
 * @end_offset: return location for the end of the return text
 *
 * Gets a slice of the text from @layout after @offset.
 *
 * The @boundary_type determines the size of the returned slice of
 * text. For the exact semantics of this function, see
 * atk_text_get_text_after_offset().
 *
 * Returns: a newly allocated string containing a slice of text
 *   from layout. Free with g_free().
 */
char *
gtk_pango_get_text_after (PangoLayout           *layout,
                          int                    offset,
                          AtspiTextBoundaryType  boundary_type,
                          int                   *start_offset,
                          int                   *end_offset)
{
  const char *text;
  int start, end;
  const PangoLogAttr *attrs;
  int n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      start = gtk_pango_move_chars (layout, start, 1);
      end = start;
      end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (gtk_pango_is_inside_word (layout, end))
        end = gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        {
          end = gtk_pango_move_words (layout, end, 1);
          while (!attrs[end].is_word_start && end < n_attrs - 1)
            end = gtk_pango_move_chars (layout, end, 1);
        }
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      end = gtk_pango_move_words (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        end = gtk_pango_move_words (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (gtk_pango_is_inside_sentence (layout, end))
        end = gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        {
          end = gtk_pango_move_sentences (layout, end, 1);
          while (!attrs[end].is_sentence_start && end < n_attrs - 1)
            end = gtk_pango_move_chars (layout, end, 1);
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      end = gtk_pango_move_sentences (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        end = gtk_pango_move_sentences (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
    case ATSPI_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_after (layout, offset, boundary_type, &start, &end);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}

/*
 * gtk_pango_get_text_at:
 * @layout: a `PangoLayout`
 * @offset: a character offset in @layout
 * @boundary_type: a `AtspiTextBoundaryType`
 * @start_offset: return location for the start of the returned text
 * @end_offset: return location for the end of the return text
 *
 * Gets a slice of the text from @layout at @offset.
 *
 * The @boundary_type determines the size of the returned slice of
 * text. For the exact semantics of this function, see
 * atk_text_get_text_after_offset().
 *
 * Returns: a newly allocated string containing a slice of text
 *   from layout. Free with g_free().
 */
char *
gtk_pango_get_text_at (PangoLayout           *layout,
                       int                    offset,
                       AtspiTextBoundaryType  boundary_type,
                       int                   *start_offset,
                       int                   *end_offset)
{
  const char *text;
  int start, end;
  const PangoLogAttr *attrs;
  int n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (!attrs[start].is_word_start)
        start = gtk_pango_move_words (layout, start, -1);
      if (gtk_pango_is_inside_word (layout, end))
        end = gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      if (gtk_pango_is_inside_word (layout, start) &&
          !attrs[start].is_word_start)
        start = gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = gtk_pango_move_chars (layout, start, -1);
      end = gtk_pango_move_words (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (!attrs[start].is_sentence_start)
        start = gtk_pango_move_sentences (layout, start, -1);
      if (gtk_pango_is_inside_sentence (layout, end))
        end = gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_pango_is_inside_sentence (layout, start) &&
          !attrs[start].is_sentence_start)
        start = gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = gtk_pango_move_chars (layout, start, -1);
      end = gtk_pango_move_sentences (layout, end, 1);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
    case ATSPI_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_at (layout, offset, boundary_type, &start, &end);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}

char *gtk_pango_get_string_at (PangoLayout           *layout,
                               int                    offset,
                               AtspiTextGranularity   granularity,
                               int                   *start_offset,
                               int                   *end_offset)
{
  const char *text;
  int start, end;
  const PangoLogAttr *attrs;
  int n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (granularity)
    {
    case ATSPI_TEXT_GRANULARITY_CHAR:
      end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_GRANULARITY_WORD:
      if (!attrs[start].is_word_start)
        start = gtk_pango_move_words (layout, start, -1);
      if (gtk_pango_is_inside_word (layout, end))
        end = gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_GRANULARITY_SENTENCE:
      if (!attrs[start].is_sentence_start)
        start = gtk_pango_move_sentences (layout, start, -1);
      if (gtk_pango_is_inside_sentence (layout, end))
        end = gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = gtk_pango_move_chars (layout, end, 1);
      break;

    case ATSPI_TEXT_GRANULARITY_LINE:
      pango_layout_get_line_at (layout, offset, ATSPI_TEXT_BOUNDARY_LINE_START, &start, &end);
      break;

    case ATSPI_TEXT_GRANULARITY_PARAGRAPH:
      /* FIXME: In theory, a layout can hold more than one paragraph */
      start = 0;
      end = g_utf8_strlen (text, -1);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}
