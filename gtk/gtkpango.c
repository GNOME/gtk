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
#include <pango/pangocairo.h>
#include "gtkintl.h"

static AtkAttributeSet *
add_attribute (AtkAttributeSet  *attributes,
               AtkTextAttribute  attr,
               const gchar      *value)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (value);

  return g_slist_prepend (attributes, at);
}

/*
 * _gtk_pango_get_default_attributes:
 * @attributes: a #AtkAttributeSet to add the attributes to
 * @layout: the #PangoLayout from which to get attributes
 *
 * Adds the default text attributes from @layout to @attributes,
 * after translating them from Pango attributes to ATK attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the #AtkText interface in widgets using Pango
 * layouts.
 *
 * Returns: the modified @attributes
 */
AtkAttributeSet*
_gtk_pango_get_default_attributes (AtkAttributeSet *attributes,
                                   PangoLayout     *layout)
{
  PangoContext *context;
  gint i;
  PangoWrapMode mode;

  context = pango_layout_get_context (layout);
  if (context)
    {
      PangoLanguage *language;
      PangoFontDescription *font;

      language = pango_context_get_language (context);
      if (language)
        attributes = add_attribute (attributes, ATK_TEXT_ATTR_LANGUAGE,
                         pango_language_to_string (language));

      font = pango_context_get_font_description (context);
      if (font)
        {
          gchar buf[60];
          attributes = add_attribute (attributes, ATK_TEXT_ATTR_STYLE,
                           atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE,
                                 pango_font_description_get_style (font)));
          attributes = add_attribute (attributes, ATK_TEXT_ATTR_VARIANT,
                           atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT,
                                 pango_font_description_get_variant (font)));
          attributes = add_attribute (attributes, ATK_TEXT_ATTR_STRETCH,
                           atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH,
                                 pango_font_description_get_stretch (font)));
          attributes = add_attribute (attributes, ATK_TEXT_ATTR_FAMILY_NAME,
                           pango_font_description_get_family (font));
          g_snprintf (buf, 60, "%d", pango_font_description_get_weight (font));
          attributes = add_attribute (attributes, ATK_TEXT_ATTR_WEIGHT, buf);
          g_snprintf (buf, 60, "%i", pango_font_description_get_size (font) / PANGO_SCALE);
          attributes = add_attribute (attributes, ATK_TEXT_ATTR_SIZE, buf);
        }
    }
  if (pango_layout_get_justify (layout))
    {
      i = 3;
    }
  else
    {
      PangoAlignment align;

      align = pango_layout_get_alignment (layout);
      if (align == PANGO_ALIGN_LEFT)
        i = 0;
      else if (align == PANGO_ALIGN_CENTER)
        i = 2;
      else   /* PANGO_ALIGN_RIGHT */
        i = 1;
    }
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_JUSTIFICATION,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, i));
  mode = pango_layout_get_wrap (layout);
  if (mode == PANGO_WRAP_WORD)
    i = 2;
  else   /* PANGO_WRAP_CHAR */
    i = 1;
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_WRAP_MODE,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_WRAP_MODE, i));

  attributes = add_attribute (attributes, ATK_TEXT_ATTR_STRIKETHROUGH,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH, 0));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_UNDERLINE,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE, 0));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_RISE, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_SCALE, "1");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_BG_FULL_HEIGHT, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_PIXELS_BELOW_LINES, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_PIXELS_ABOVE_LINES, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_EDITABLE,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_EDITABLE, 0));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_INVISIBLE,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_INVISIBLE, 0));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_INDENT, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_RIGHT_MARGIN, "0");
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_LEFT_MARGIN, "0");

  return attributes;
}

/*
 * _gtk_pango_get_run_attributes:
 * @attributes: a #AtkAttributeSet to add attributes to
 * @layout: the #PangoLayout to get the attributes from
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
 * translating them from Pango attributes to ATK attributes.
 *
 * This is a convenience function that can be used to implement
 * support for the #AtkText interface in widgets using Pango
 * layouts.
 *
 * Returns: the modified #AtkAttributeSet
 */
AtkAttributeSet *
_gtk_pango_get_run_attributes (AtkAttributeSet *attributes,
                               PangoLayout     *layout,
                               gint             offset,
                               gint            *start_offset,
                               gint            *end_offset)
{
  PangoAttrIterator *iter;
  PangoAttrList *attr;
  PangoAttrString *pango_string;
  PangoAttrInt *pango_int;
  PangoAttrColor *pango_color;
  PangoAttrLanguage *pango_lang;
  PangoAttrFloat *pango_float;
  gint index, start_index, end_index;
  gboolean is_next;
  glong len;
  const gchar *text;
  gchar *value;

  text = pango_layout_get_text (layout);
  len = g_utf8_strlen (text, -1);

  /* Grab the attributes of the PangoLayout, if any */
  attr = pango_layout_get_attributes (layout);

  if (attr == NULL)
    {
      *start_offset = 0;
      *end_offset = len;
      return attributes;
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
  pango_string = (PangoAttrString*) pango_attr_iterator_get (iter, PANGO_ATTR_FAMILY);
  if (pango_string != NULL)
    {
      value = g_strdup_printf ("%s", pango_string->value);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_FAMILY_NAME, value);
      g_free (value);
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_STYLE);
  if (pango_int != NULL)
    {
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_STYLE,
                       atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE, pango_int->value));
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_WEIGHT);
  if (pango_int != NULL)
    {
      value = g_strdup_printf ("%i", pango_int->value);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_WEIGHT, value);
      g_free (value);
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_VARIANT);
  if (pango_int != NULL)
    {
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_VARIANT,
                       atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT, pango_int->value));
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_STRETCH);
  if (pango_int != NULL)
    {
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_STRETCH,
                       atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH, pango_int->value));
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_SIZE);
  if (pango_int != NULL)
    {
      value = g_strdup_printf ("%i", pango_int->value / PANGO_SCALE);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_SIZE, value);
      g_free (value);
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE);
  if (pango_int != NULL)
    {
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_UNDERLINE,
                       atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE, pango_int->value));
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_STRIKETHROUGH);
  if (pango_int != NULL)
    {
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_STRIKETHROUGH,
                       atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH, pango_int->value));
    }
  pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, PANGO_ATTR_RISE);
  if (pango_int != NULL)
    {
      value = g_strdup_printf ("%i", pango_int->value);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_RISE, value);
      g_free (value);
    }
  pango_lang = (PangoAttrLanguage*) pango_attr_iterator_get (iter, PANGO_ATTR_LANGUAGE);
  if (pango_lang != NULL)
    {
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_LANGUAGE,
                                  pango_language_to_string (pango_lang->value));
    }
  pango_float = (PangoAttrFloat*) pango_attr_iterator_get (iter, PANGO_ATTR_SCALE);
  if (pango_float != NULL)
    {
      value = g_strdup_printf ("%g", pango_float->value);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_SCALE, value);
      g_free (value);
    }
  pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter, PANGO_ATTR_FOREGROUND);
  if (pango_color != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u",
                               pango_color->color.red,
                               pango_color->color.green,
                               pango_color->color.blue);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_FG_COLOR, value);
      g_free (value);
    }
  pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter, PANGO_ATTR_BACKGROUND);
  if (pango_color != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u",
                               pango_color->color.red,
                               pango_color->color.green,
                               pango_color->color.blue);
      attributes = add_attribute (attributes, ATK_TEXT_ATTR_BG_COLOR, value);
      g_free (value);
    }
  pango_attr_iterator_destroy (iter);

  return attributes;
}

/*
 * _gtk_pango_move_chars:
 * @layout: a #PangoLayout
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
static gint
_gtk_pango_move_chars (PangoLayout *layout,
                       gint         offset,
                       gint         count)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

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
 * _gtk_pango_move_words:
 * @layout: a #PangoLayout
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
static gint
_gtk_pango_move_words (PangoLayout  *layout,
                       gint          offset,
                       gint          count)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

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
 * _gtk_pango_move_sentences:
 * @layout: a #PangoLayout
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
static gint
_gtk_pango_move_sentences (PangoLayout  *layout,
                           gint          offset,
                           gint          count)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

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

/*
 * _gtk_pango_is_inside_word:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a word.
 *
 * Returns: %TRUE if @offset is inside a word
 */
static gboolean
_gtk_pango_is_inside_word (PangoLayout  *layout,
                           gint          offset)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_word_start || attrs[offset].is_word_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_word_start;

  return FALSE;
}

/*
 * _gtk_pango_is_inside_sentence:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a sentence.
 *
 * Returns: %TRUE if @offset is inside a sentence
 */
static gboolean
_gtk_pango_is_inside_sentence (PangoLayout  *layout,
                               gint          offset)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_sentence_start || attrs[offset].is_sentence_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_sentence_start;

  return FALSE;
}

static void
pango_layout_get_line_before (PangoLayout     *layout,
                              AtkTextBoundary  boundary_type,
                              gint             offset,
                              gint            *start_offset,
                              gint            *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL, *prev_prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (prev_line)
            {
              switch (boundary_type)
                {
                case ATK_TEXT_BOUNDARY_LINE_START:
                  end_index = start_index;
                  start_index = prev_line->start_index;
                  break;
                case ATK_TEXT_BOUNDARY_LINE_END:
                  if (prev_prev_line)
                    start_index = prev_prev_line->start_index + prev_prev_line->length;
                  else
                    start_index = 0;
                  end_index = prev_line->start_index + prev_line->length;
                  break;
                case ATK_TEXT_BOUNDARY_CHAR:
                case ATK_TEXT_BOUNDARY_WORD_START:
                case ATK_TEXT_BOUNDARY_WORD_END:
                case ATK_TEXT_BOUNDARY_SENTENCE_START:
                case ATK_TEXT_BOUNDARY_SENTENCE_END:
                default:
                  g_assert_not_reached();
                  break;
                }
            }
          else
            start_index = end_index = 0;

          found = TRUE;
          break;
        }

      prev_prev_line = prev_line;
      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango_layout_get_line_at (PangoLayout     *layout,
                          AtkTextBoundary  boundary_type,
                          gint             offset,
                          gint            *start_offset,
                          gint            *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          switch (boundary_type)
            {
            case ATK_TEXT_BOUNDARY_LINE_START:
              if (pango_layout_iter_next_line (iter))
                end_index = pango_layout_iter_get_line (iter)->start_index;
              break;
            case ATK_TEXT_BOUNDARY_LINE_END:
              if (prev_line)
                start_index = prev_line->start_index + prev_line->length;
              break;
            case ATK_TEXT_BOUNDARY_CHAR:
            case ATK_TEXT_BOUNDARY_WORD_START:
            case ATK_TEXT_BOUNDARY_WORD_END:
            case ATK_TEXT_BOUNDARY_SENTENCE_START:
            case ATK_TEXT_BOUNDARY_SENTENCE_END:
            default:
              g_assert_not_reached();
              break;
            }

          found = TRUE;
          break;
        }

      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango_layout_get_line_after (PangoLayout     *layout,
                             AtkTextBoundary  boundary_type,
                             gint             offset,
                             gint            *start_offset,
                             gint            *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (pango_layout_iter_next_line (iter))
            {
              line = pango_layout_iter_get_line (iter);
              switch (boundary_type)
                {
                case ATK_TEXT_BOUNDARY_LINE_START:
                  start_index = line->start_index;
                  if (pango_layout_iter_next_line (iter))
                    end_index = pango_layout_iter_get_line (iter)->start_index;
                  else
                    end_index = start_index + line->length;
                  break;
                case ATK_TEXT_BOUNDARY_LINE_END:
                  start_index = end_index;
                  end_index = line->start_index + line->length;
                  break;
                case ATK_TEXT_BOUNDARY_CHAR:
                case ATK_TEXT_BOUNDARY_WORD_START:
                case ATK_TEXT_BOUNDARY_WORD_END:
                case ATK_TEXT_BOUNDARY_SENTENCE_START:
                case ATK_TEXT_BOUNDARY_SENTENCE_END:
                default:
                  g_assert_not_reached();
                  break;
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
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

/*
 * _gtk_pango_get_text_before:
 * @layout: a #PangoLayout
 * @boundary_type: a #AtkTextBoundary
 * @offset: a character offset in @layout
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
 *     from layout. Free with g_free().
 */
gchar *
_gtk_pango_get_text_before (PangoLayout     *layout,
                            AtkTextBoundary  boundary_type,
                            gint             offset,
                            gint            *start_offset,
                            gint            *end_offset)
{
  const gchar *text;
  gint start, end;
  const PangoLogAttr *attrs;
  gint n_attrs;

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
    case ATK_TEXT_BOUNDARY_CHAR:
      start = _gtk_pango_move_chars (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      end = start;
      start = _gtk_pango_move_words (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      if (_gtk_pango_is_inside_word (layout, start) &&
          !attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = start;
      start = _gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      end = start;
      start = _gtk_pango_move_sentences (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (_gtk_pango_is_inside_sentence (layout, start) &&
          !attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = start;
      start = _gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_before (layout, boundary_type, offset, &start, &end);
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
 * _gtk_pango_get_text_after:
 * @layout: a #PangoLayout
 * @boundary_type: a #AtkTextBoundary
 * @offset: a character offset in @layout
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
 *     from layout. Free with g_free().
 */
gchar *
_gtk_pango_get_text_after (PangoLayout     *layout,
                           AtkTextBoundary  boundary_type,
                           gint             offset,
                           gint            *start_offset,
                           gint            *end_offset)
{
  const gchar *text;
  gint start, end;
  const PangoLogAttr *attrs;
  gint n_attrs;

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
    case ATK_TEXT_BOUNDARY_CHAR:
      start = _gtk_pango_move_chars (layout, start, 1);
      end = start;
      end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (_gtk_pango_is_inside_word (layout, end))
        end = _gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        {
          end = _gtk_pango_move_words (layout, end, 1);
          while (!attrs[end].is_word_start && end < n_attrs - 1)
            end = _gtk_pango_move_chars (layout, end, 1);
        }
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      end = _gtk_pango_move_words (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        end = _gtk_pango_move_words (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (_gtk_pango_is_inside_sentence (layout, end))
        end = _gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        {
          end = _gtk_pango_move_sentences (layout, end, 1);
          while (!attrs[end].is_sentence_start && end < n_attrs - 1)
            end = _gtk_pango_move_chars (layout, end, 1);
        }
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      end = _gtk_pango_move_sentences (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        end = _gtk_pango_move_sentences (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_after (layout, boundary_type, offset, &start, &end);
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
 * _gtk_pango_get_text_at:
 * @layout: a #PangoLayout
 * @boundary_type: a #AtkTextBoundary
 * @offset: a character offset in @layout
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
 *     from layout. Free with g_free().
 */
gchar *
_gtk_pango_get_text_at (PangoLayout     *layout,
                        AtkTextBoundary  boundary_type,
                        gint             offset,
                        gint            *start_offset,
                        gint            *end_offset)
{
  const gchar *text;
  gint start, end;
  const PangoLogAttr *attrs;
  gint n_attrs;

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
    case ATK_TEXT_BOUNDARY_CHAR:
      end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      if (_gtk_pango_is_inside_word (layout, end))
        end = _gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      if (_gtk_pango_is_inside_word (layout, start) &&
          !attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = _gtk_pango_move_words (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      if (_gtk_pango_is_inside_sentence (layout, end))
        end = _gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (_gtk_pango_is_inside_sentence (layout, start) &&
          !attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = _gtk_pango_move_sentences (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_at (layout, boundary_type, offset, &start, &end);
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

static gboolean
attr_list_merge_filter (PangoAttribute *attribute,
                        gpointer        list)
{
  pango_attr_list_change (list, pango_attribute_copy (attribute));
  return FALSE;
}

/*
 * _gtk_pango_attr_list_merge:
 * @into: a #PangoAttrList where attributes are merged or %NULL
 * @from: a #PangoAttrList with the attributes to merge or %NULL
 *
 * Merges attributes from @from into @into.
 *
 * Returns: the merged list.
 */
PangoAttrList *
_gtk_pango_attr_list_merge (PangoAttrList *into,
                            PangoAttrList *from)
{
  if (from)
    {
      if (into)
        pango_attr_list_filter (from, attr_list_merge_filter, into);
      else
       return pango_attr_list_ref (from);
    }

  return into;
}

static PangoAttribute *
attribute_from_text (GtkBuilder  *builder,
                     const char  *name,
                     const char  *value,
                     GError     **error)
{
  PangoAttribute *attribute = NULL;
  PangoAttrType type;
  PangoLanguage *language;
  PangoFontDescription *font_desc;
  GdkRGBA *color;
  GValue val = G_VALUE_INIT;

  if (!gtk_builder_value_from_string_type (builder, PANGO_TYPE_ATTR_TYPE, name, &val, error))
    return NULL;

  type = g_value_get_enum (&val);
  g_value_unset (&val);

  switch (type)
    {
      /* PangoAttrLanguage */
    case PANGO_ATTR_LANGUAGE:
      if ((language = pango_language_from_string (value)))
        {
          attribute = pango_attr_language_new (language);
          g_value_init (&val, G_TYPE_INT);
        }
      break;
      /* PangoAttrInt */
    case PANGO_ATTR_STYLE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STYLE, value, &val, error))
        attribute = pango_attr_style_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_WEIGHT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_WEIGHT, value, &val, error))
        attribute = pango_attr_weight_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_VARIANT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_VARIANT, value, &val, error))
        attribute = pango_attr_variant_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_STRETCH:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STRETCH, value, &val, error))
        attribute = pango_attr_stretch_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_UNDERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_UNDERLINE, value, &val, NULL))
        attribute = pango_attr_underline_new (g_value_get_enum (&val));
      else
        {
          /* XXX: allow boolean for backwards compat, so ignore error */
          /* Deprecate this somehow */
          g_value_unset (&val);
          if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
            attribute = pango_attr_underline_new (g_value_get_boolean (&val));
        }
      break;
    case PANGO_ATTR_STRIKETHROUGH:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_strikethrough_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_GRAVITY:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY, value, &val, error))
        attribute = pango_attr_gravity_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_GRAVITY_HINT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY_HINT, value, &val, error))
        attribute = pango_attr_gravity_hint_new (g_value_get_enum (&val));
      break;
      /* PangoAttrString */
    case PANGO_ATTR_FAMILY:
      attribute = pango_attr_family_new (value);
      g_value_init (&val, G_TYPE_INT);
      break;

      /* PangoAttrSize */
    case PANGO_ATTR_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_size_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_ABSOLUTE_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_size_new_absolute (g_value_get_int (&val));
      break;

      /* PangoAttrFontDesc */
    case PANGO_ATTR_FONT_DESC:
      if ((font_desc = pango_font_description_from_string (value)))
        {
          attribute = pango_attr_font_desc_new (font_desc);
          pango_font_description_free (font_desc);
          g_value_init (&val, G_TYPE_INT);
        }
      break;
      /* PangoAttrColor */
    case PANGO_ATTR_FOREGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_foreground_new (color->red * 65535,
                                                 color->green * 65535,
                                                 color->blue * 65535);
        }
      break;
    case PANGO_ATTR_BACKGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_background_new (color->red * 65535,
                                                 color->green * 65535,
                                                 color->blue * 65535);
        }
      break;
    case PANGO_ATTR_UNDERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_underline_color_new (color->red * 65535,
                                                      color->green * 65535,
                                                      color->blue * 65535);
        }
      break;
    case PANGO_ATTR_STRIKETHROUGH_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_strikethrough_color_new (color->red * 65535,
                                                          color->green * 65535,
                                                          color->blue * 65535);
        }
      break;
      /* PangoAttrShape */
    case PANGO_ATTR_SHAPE:
      /* Unsupported for now */
      break;
      /* PangoAttrFloat */
    case PANGO_ATTR_SCALE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, value, &val, error))
        attribute = pango_attr_scale_new (g_value_get_double (&val));
      break;
    case PANGO_ATTR_LETTER_SPACING:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_letter_spacing_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_RISE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_rise_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_FALLBACK:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_fallback_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_FONT_FEATURES:
      attribute = pango_attr_font_features_new (value);
      break;
    case PANGO_ATTR_FOREGROUND_ALPHA:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_foreground_alpha_new ((guint16)g_value_get_int (&val));
      break;
    case PANGO_ATTR_BACKGROUND_ALPHA:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, value, &val, error))
        attribute = pango_attr_background_alpha_new ((guint16)g_value_get_int (&val));
      break;
#if PANGO_VERSION_CHECK(1,44,0)
    case PANGO_ATTR_ALLOW_BREAKS:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_allow_breaks_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_SHOW:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_SHOW_FLAGS, value, &val, error))
        attribute = pango_attr_show_new (g_value_get_flags (&val));
      break;
    case PANGO_ATTR_INSERT_HYPHENS:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
        attribute = pango_attr_insert_hyphens_new (g_value_get_boolean (&val));
      break;
#endif
#if PANGO_VERSION_CHECK(1,45,0)
    case PANGO_ATTR_OVERLINE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_OVERLINE, value, &val, NULL))
        attribute = pango_attr_overline_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_OVERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_RGBA, value, &val, error))
        {
          color = g_value_get_boxed (&val);
          attribute = pango_attr_overline_color_new (color->red * 65535,
                                                     color->green * 65535,
                                                     color->blue * 65535);
        }
      break;
#endif
    case PANGO_ATTR_INVALID:
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
      PangoAttribute *attr = NULL;
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

      attr->start_index = start_val;
      attr->end_index = end_val;

      if (!data->attrs)
        data->attrs = pango_attr_list_new ();

      pango_attr_list_insert (data->attrs, attr);
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
                                        "GtkContainer", element_name,
                                        error);
    }
}
