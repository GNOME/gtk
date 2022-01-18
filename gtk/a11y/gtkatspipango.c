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
pango_style_to_string (Pango2Style style)
{
  switch (style)
    {
    case PANGO2_STYLE_NORMAL:
      return "normal";
    case PANGO2_STYLE_OBLIQUE:
      return "oblique";
    case PANGO2_STYLE_ITALIC:
      return "italic";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_variant_to_string (Pango2Variant variant)
{
  switch (variant)
    {
    case PANGO2_VARIANT_NORMAL:
      return "normal";
    case PANGO2_VARIANT_SMALL_CAPS:
      return "small_caps";
    case PANGO2_VARIANT_ALL_SMALL_CAPS:
      return "all_small_caps";
    case PANGO2_VARIANT_PETITE_CAPS:
      return "petite_caps";
    case PANGO2_VARIANT_ALL_PETITE_CAPS:
      return "all_petite_caps";
    case PANGO2_VARIANT_UNICASE:
      return "unicase";
    case PANGO2_VARIANT_TITLE_CAPS:
      return "title_caps";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_stretch_to_string (Pango2Stretch stretch)
{
  switch (stretch)
    {
    case PANGO2_STRETCH_ULTRA_CONDENSED:
      return "ultra_condensed";
    case PANGO2_STRETCH_EXTRA_CONDENSED:
      return "extra_condensed";
    case PANGO2_STRETCH_CONDENSED:
      return "condensed";
    case PANGO2_STRETCH_SEMI_CONDENSED:
      return "semi_condensed";
    case PANGO2_STRETCH_NORMAL:
      return "normal";
    case PANGO2_STRETCH_SEMI_EXPANDED:
      return "semi_expanded";
    case PANGO2_STRETCH_EXPANDED:
      return "expanded";
    case PANGO2_STRETCH_EXTRA_EXPANDED:
      return "extra_expanded";
    case PANGO2_STRETCH_ULTRA_EXPANDED:
      return "ultra_expanded";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_line_style_to_string (Pango2LineStyle value)
{
  switch (value)
    {
    case PANGO2_LINE_STYLE_NONE:
      return "none";
    case PANGO2_LINE_STYLE_SOLID:
      return "single";
    case PANGO2_LINE_STYLE_DOUBLE:
      return "double";
    case PANGO2_LINE_STYLE_DASHED:
      return "dashed";
    case PANGO2_LINE_STYLE_DOTTED:
      return "dotted";
    case PANGO2_LINE_STYLE_WAVY:
      return "wavy";
    default:
      g_assert_not_reached ();
    }
}

const char *
pango_wrap_mode_to_string (Pango2WrapMode mode)
{
  /* Keep these in sync with gtk_wrap_mode_to_string() */
  switch (mode)
    {
    case PANGO2_WRAP_WORD:
      return "word";
    case PANGO2_WRAP_CHAR:
      return "char";
    case PANGO2_WRAP_WORD_CHAR:
      return "word-char";
    default:
      g_assert_not_reached ();
    }
}

static const char *
pango_align_to_string (Pango2Alignment align)
{
  switch (align)
    {
    case PANGO2_ALIGN_LEFT:
      return "left";
    case PANGO2_ALIGN_CENTER:
      return "center";
    case PANGO2_ALIGN_RIGHT:
      return "right";
    case PANGO2_ALIGN_NATURAL:
      return "natural";
    case PANGO2_ALIGN_JUSTIFY:
      return "fill";
    default:
      g_assert_not_reached ();
    }
}

void
gtk_pango_get_font_attributes (Pango2FontDescription *font,
                               GVariantBuilder      *builder)
{
  char buf[60];

  g_variant_builder_add (builder, "{ss}", "style",
                         pango_style_to_string (pango2_font_description_get_style (font)));
  g_variant_builder_add (builder, "{ss}", "variant",
                         pango_variant_to_string (pango2_font_description_get_variant (font)));
  g_variant_builder_add (builder, "{ss}", "stretch",
                         pango_stretch_to_string (pango2_font_description_get_stretch (font)));
  g_variant_builder_add (builder, "{ss}", "family-name",
                         pango2_font_description_get_family (font));

  g_snprintf (buf, 60, "%d", pango2_font_description_get_weight (font));
  g_variant_builder_add (builder, "{ss}", "weight", buf);
  g_snprintf (buf, 60, "%i", pango2_font_description_get_size (font) / PANGO2_SCALE);
  g_variant_builder_add (builder, "{ss}", "size", buf);
}

/*
 * gtk_pango_get_default_attributes:
 * @layout: the `Pango2Layout` from which to get attributes
 * @builder: a `GVariantBuilder` to add to
 *
 * Adds the default text attributes from @layout to @builder,
 * after translating them from Pango2 attributes to atspi
 * attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the `AtkText` interface in widgets using Pango2
 * layouts.
 *
 * Returns: the modified @attributes
 */
void
gtk_pango_get_default_attributes (Pango2Layout     *layout,
                                  GVariantBuilder *builder)
{
  Pango2Context *context;

  context = pango2_layout_get_context (layout);
  if (context)
    {
      Pango2Language *language;
      Pango2FontDescription *font;

      language = pango2_context_get_language (context);
      if (language)
        g_variant_builder_add (builder, "{ss}", "language",
                               pango2_language_to_string (language));

      font = pango2_context_get_font_description (context);
      if (font)
        gtk_pango_get_font_attributes (font, builder);
    }

  g_variant_builder_add (builder, "{ss}", "justification",
                         pango_align_to_string (pango2_layout_get_alignment (layout)));

  g_variant_builder_add (builder, "{ss}", "wrap-mode",
                         pango_wrap_mode_to_string (pango2_layout_get_wrap (layout)));
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
 * @layout: the `Pango2Layout` to get the attributes from
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
 * translating them from Pango2 attributes to atspi attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the #AtkText interface in widgets using Pango2
 * layouts.
 */
void
gtk_pango_get_run_attributes (Pango2Layout     *layout,
                              GVariantBuilder *builder,
                              int              offset,
                              int             *start_offset,
                              int             *end_offset)
{
  Pango2AttrIterator *iter;
  Pango2AttrList *attr_list;
  Pango2Attribute *attr;
  int index, start_index, end_index;
  gboolean is_next;
  glong len;
  const char *text;
  char *value;
  const char *val;

  text = pango2_layout_get_text (layout);
  len = g_utf8_strlen (text, -1);

  /* Grab the attributes of the Pango2Layout, if any */
  attr_list = pango2_layout_get_attributes (layout);

  if (attr_list == NULL)
    {
      *start_offset = 0;
      *end_offset = len;
      return;
    }

  iter = pango2_attr_list_get_iterator (attr_list);
  /* Get invariant range offsets */
  /* If offset out of range, set offset in range */
  if (offset > len)
    offset = len;
  else if (offset < 0)
    offset = 0;

  index = g_utf8_offset_to_pointer (text, offset) - text;
  pango2_attr_iterator_range (iter, &start_index, &end_index);
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
      is_next = pango2_attr_iterator_next (iter);
      pango2_attr_iterator_range (iter, &start_index, &end_index);
    }

  /* Get attributes */
  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_FAMILY);
  if (attr != NULL)
    g_variant_builder_add (builder, "{ss}", "family-name", pango2_attribute_get_string (attr));

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_STYLE);
  if (attr != NULL)
    g_variant_builder_add (builder, "{ss}", "style", pango_style_to_string (pango2_attribute_get_int (attr)));

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_WEIGHT);
  if (attr != NULL)
    {
      value = g_strdup_printf ("%i", pango2_attribute_get_int (attr));
      g_variant_builder_add (builder, "{ss}", "weight", value);
      g_free (value);
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_VARIANT);
  if (attr != NULL)
    g_variant_builder_add (builder, "{ss}", "variant",
                           pango_variant_to_string (pango2_attribute_get_int (attr)));

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_STRETCH);
  if (attr != NULL)
    g_variant_builder_add (builder, "{ss}", "stretch",
                           pango_stretch_to_string (pango2_attribute_get_int (attr)));

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_SIZE);
  if (attr != NULL)
    {
      value = g_strdup_printf ("%i", pango2_attribute_get_int (attr) / PANGO2_SCALE);
      g_variant_builder_add (builder, "{ss}", "size", value);
      g_free (value);
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_UNDERLINE);
  if (attr != NULL)
    g_variant_builder_add (builder, "{ss}", "underline",
                           pango_line_style_to_string (pango2_attribute_get_int (attr)));

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_STRIKETHROUGH);
  if (attr != NULL)
    {
      if (pango2_attribute_get_int (attr))
        val = "true";
      else
        val = "false";
      g_variant_builder_add (builder, "{ss}", "strikethrough", val);
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_RISE);
  if (attr != NULL)
    {
      value = g_strdup_printf ("%i", pango2_attribute_get_int (attr));
      g_variant_builder_add (builder, "{ss}", "rise", value);
      g_free (value);
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_LANGUAGE);
  if (attr != NULL)
    {
      g_variant_builder_add (builder, "{ss}", "language",
                             pango2_language_to_string (pango2_attribute_get_language (attr)));
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_SCALE);
  if (attr != NULL)
    {
      value = g_strdup_printf ("%g", pango2_attribute_get_float (attr));
      g_variant_builder_add (builder, "{ss}", "scale", value);
      g_free (value);
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_FOREGROUND);
  if (attr != NULL)
    {
      Pango2Color *color = pango2_attribute_get_color (attr);
      value = g_strdup_printf ("%u,%u,%u,%u",
                               color->red,
                               color->green,
                               color->blue,
                               color->alpha);
      g_variant_builder_add (builder, "{ss}", "fg-color", value);
      g_free (value);
    }

  attr = pango2_attr_iterator_get (iter, PANGO2_ATTR_BACKGROUND);
  if (attr != NULL)
    {
      Pango2Color *color = pango2_attribute_get_color (attr);
      value = g_strdup_printf ("%u,%u,%u,%u",
                               color->red,
                               color->green,
                               color->blue,
                               color->alpha);
      g_variant_builder_add (builder, "{ss}", "bg-color", value);
      g_free (value);
    }
  pango2_attr_iterator_destroy (iter);
}

/*
 * gtk_pango_move_chars:
 * @layout: a `Pango2Layout`
 * @offset: a character offset in @layout
 * @count: the number of characters to move from @offset
 *
 * Returns the position that is @count characters from the
 * given @offset. @count may be positive or negative.
 *
 * For the purpose of this function, characters are defined
 * by what Pango2 considers cursor positions.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_chars (Pango2Layout *layout,
                      int          offset,
                      int          count)
{
  const Pango2LogAttr *attrs;
  int n_attrs;

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
 * @layout: a `Pango2Layout`
 * @offset: a character offset in @layout
 * @count: the number of words to move from @offset
 *
 * Returns the position that is @count words from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a word end, otherwise it will be a word start.
 * See the Pango2 documentation for details on how
 * word starts and ends are defined.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_words (Pango2Layout  *layout,
                      int           offset,
                      int           count)
{
  const Pango2LogAttr *attrs;
  int n_attrs;

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
 * @layout: a `Pango2Layout`
 * @offset: a character offset in @layout
 * @count: the number of sentences to move from @offset
 *
 * Returns the position that is @count sentences from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a sentence end, otherwise it will be a sentence start.
 * See the Pango2 documentation for details on how
 * sentence starts and ends are defined.
 *
 * Returns: the new position
 */
static int
gtk_pango_move_sentences (Pango2Layout  *layout,
                          int           offset,
                          int           count)
{
  const Pango2LogAttr *attrs;
  int n_attrs;

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
 * @layout: a `Pango2Layout`
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
gtk_pango_move_lines (Pango2Layout *layout,
                      int          offset,
                      int          count)
{
  GSList *lines, *l;
  Pango2Line *line;
  int num;
  const char *text;
  int pos, line_pos;
  int index;
  int len;

  text = pango2_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  lines = pango2_layout_get_lines (layout);
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
 * @layout: a `Pango2Layout`
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a word.
 *
 * Returns: %TRUE if @offset is inside a word
 */
static gboolean
gtk_pango_is_inside_word (Pango2Layout  *layout,
                          int           offset)
{
  const Pango2LogAttr *attrs;
  int n_attrs;

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_word_start || attrs[offset].is_word_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_word_start;

  return FALSE;
}

/*
 * gtk_pango_is_inside_sentence:
 * @layout: a `Pango2Layout`
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a sentence.
 *
 * Returns: %TRUE if @offset is inside a sentence
 */
static gboolean
gtk_pango_is_inside_sentence (Pango2Layout  *layout,
                              int           offset)
{
  const Pango2LogAttr *attrs;
  int n_attrs;

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_sentence_start || attrs[offset].is_sentence_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_sentence_start;

  return FALSE;
}

static void
pango2_layout_get_line_before (Pango2Layout           *layout,
                              int                    offset,
                              AtspiTextBoundaryType  boundary_type,
                              int                   *start_offset,
                              int                   *end_offset)
{
  Pango2LineIter *iter;
  Pango2Line *line, *prev_line = NULL, *prev_prev_line = NULL;
  int index, start_index, length, end_index;
  int prev_start_index, prev_length;
  int prev_prev_start_index, prev_prev_length;
  const char *text;
  gboolean found = FALSE;

  text = pango2_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango2_layout_get_iter (layout);
  do
    {
      line = pango2_line_iter_get_line (iter);
      start_index = pango2_line_get_start_index (line);
      length = pango2_line_get_length (line);
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
  while (pango2_line_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_start_index + prev_length;
      end_index = start_index;
    }
  pango2_line_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango2_layout_get_line_at (Pango2Layout           *layout,
                          int                    offset,
                          AtspiTextBoundaryType  boundary_type,
                          int                   *start_offset,
                          int                   *end_offset)
{
  Pango2LineIter *iter;
  Pango2Line *line, *prev_line = NULL;
  int index, start_index, length, end_index;
  const char *text;
  gboolean found = FALSE;

  text = pango2_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango2_layout_get_iter (layout);
  do
    {
      line = pango2_line_iter_get_line (iter);
      start_index = pango2_line_get_start_index (line);
      length = pango2_line_get_length (line);
      end_index = start_index + length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          switch (boundary_type)
            {
            case ATSPI_TEXT_BOUNDARY_LINE_START:
              if (pango2_line_iter_next_line (iter))
                end_index = pango2_line_get_start_index (pango2_line_iter_get_line (iter));
              break;
            case ATSPI_TEXT_BOUNDARY_LINE_END:
              if (prev_line)
                start_index = pango2_line_get_start_index (prev_line) + pango2_line_get_length (prev_line);
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
  while (pango2_line_iter_next_line (iter));

  if (!found)
    {
      start_index = pango2_line_get_start_index (prev_line) + pango2_line_get_length (prev_line);
      end_index = start_index;
    }
  pango2_line_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango2_layout_get_line_after (Pango2Layout           *layout,
                             int                    offset,
                             AtspiTextBoundaryType  boundary_type,
                             int                   *start_offset,
                             int                   *end_offset)
{
  Pango2LineIter *iter;
  Pango2Line *line, *prev_line = NULL;
  int index, start_index, length, end_index;
  const char *text;
  gboolean found = FALSE;

  text = pango2_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango2_layout_get_iter (layout);
  do
    {
      line = pango2_line_iter_get_line (iter);
      start_index = pango2_line_get_start_index (line);
      length = pango2_line_get_length (line);
      end_index = start_index + length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (pango2_line_iter_next_line (iter))
            {
              line = pango2_line_iter_get_line (iter);
              switch (boundary_type)
                {
                case ATSPI_TEXT_BOUNDARY_LINE_START:
                  start_index = pango2_line_get_start_index (line);
                  if (pango2_line_iter_next_line (iter))
                    end_index = pango2_line_get_start_index (pango2_line_iter_get_line (iter));
                  else
                    end_index = start_index + pango2_line_get_length (line);
                  break;
                case ATSPI_TEXT_BOUNDARY_LINE_END:
                  start_index = end_index;
                  end_index = pango2_line_get_start_index (line) + pango2_line_get_length (line);
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
  while (pango2_line_iter_next_line (iter));

  if (!found)
    {
      start_index = pango2_line_get_start_index (prev_line) + pango2_line_get_length (prev_line);
      end_index = start_index;
    }
  pango2_line_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

/*
 * gtk_pango_get_text_before:
 * @layout: a `Pango2Layout`
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
gtk_pango_get_text_before (Pango2Layout           *layout,
                           int                    offset,
                           AtspiTextBoundaryType  boundary_type,
                           int                   *start_offset,
                           int                   *end_offset)
{
  const char *text;
  int start, end;
  const Pango2LogAttr *attrs;
  int n_attrs;

  text = pango2_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
      pango2_layout_get_line_before (layout, offset, boundary_type, &start, &end);
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
 * @layout: a `Pango2Layout`
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
gtk_pango_get_text_after (Pango2Layout           *layout,
                          int                    offset,
                          AtspiTextBoundaryType  boundary_type,
                          int                   *start_offset,
                          int                   *end_offset)
{
  const char *text;
  int start, end;
  const Pango2LogAttr *attrs;
  int n_attrs;

  text = pango2_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
      pango2_layout_get_line_after (layout, offset, boundary_type, &start, &end);
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
 * @layout: a `Pango2Layout`
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
gtk_pango_get_text_at (Pango2Layout           *layout,
                       int                    offset,
                       AtspiTextBoundaryType  boundary_type,
                       int                   *start_offset,
                       int                   *end_offset)
{
  const char *text;
  int start, end;
  const Pango2LogAttr *attrs;
  int n_attrs;

  text = pango2_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
      pango2_layout_get_line_at (layout, offset, boundary_type, &start, &end);
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

char *gtk_pango_get_string_at (Pango2Layout           *layout,
                               int                    offset,
                               AtspiTextGranularity   granularity,
                               int                   *start_offset,
                               int                   *end_offset)
{
  const char *text;
  int start, end;
  const Pango2LogAttr *attrs;
  int n_attrs;

  text = pango2_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango2_layout_get_log_attrs (layout, &n_attrs);

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
      pango2_layout_get_line_at (layout, offset, ATSPI_TEXT_BOUNDARY_LINE_START, &start, &end);
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
