/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include "gailtextutil.h"

/**
 * SECTION:gailtextutil
 * @Short_description: GailTextUtil is a utility class which can be used to
 *   implement some of the #AtkText functions for accessible objects
 *   which implement #AtkText.
 * @Title: GailTextUtil
 *
 * GailTextUtil is a utility class which can be used to implement the
 * #AtkText functions which get text for accessible objects which implement
 * #AtkText.
 *
 * In GAIL it is used by the accsesible objects for #GnomeCanvasText, #GtkEntry,
 * #GtkLabel, #GtkCellRendererText and #GtkTextView.
 */

static void gail_text_util_class_init      (GailTextUtilClass *klass);

static void gail_text_util_init            (GailTextUtil      *textutil);
static void gail_text_util_finalize        (GObject           *object);


static void get_pango_text_offsets         (PangoLayout         *layout,
                                            GtkTextBuffer       *buffer,
                                            GailOffsetType      function,
                                            AtkTextBoundary     boundary_type,
                                            gint                offset,
                                            gint                *start_offset,
                                            gint                *end_offset,
                                            GtkTextIter         *start_iter,
                                            GtkTextIter         *end_iter);
static GObjectClass *parent_class = NULL;

GType
gail_text_util_get_type(void)
{
  static GType type = 0;

  if (!type)
    {
      const GTypeInfo tinfo =
      {
        sizeof (GailTextUtilClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gail_text_util_class_init,
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof(GailTextUtil),
        0, /* nb preallocs */
        (GInstanceInitFunc) gail_text_util_init,
        NULL, /* value table */
      };

      type = g_type_register_static (G_TYPE_OBJECT, "GailTextUtil", &tinfo, 0);
    }
  return type;
}

/**
 * gail_text_util_new:
 *
 * This function creates a new GailTextUtil object.
 *
 * Returns: the GailTextUtil object
 **/
GailTextUtil*
gail_text_util_new (void)
{
  return GAIL_TEXT_UTIL (g_object_new (GAIL_TYPE_TEXT_UTIL, NULL));
}

static void
gail_text_util_init (GailTextUtil *textutil)
{
  textutil->buffer = NULL;
}

static void
gail_text_util_class_init (GailTextUtilClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gail_text_util_finalize;
}

static void
gail_text_util_finalize (GObject *object)
{
  GailTextUtil *textutil = GAIL_TEXT_UTIL (object);

  if (textutil->buffer)
    g_object_unref (textutil->buffer);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gail_text_util_text_setup:
 * @textutil: The #GailTextUtil to be initialized.
 * @text: A gchar* which points to the text to be stored in the GailTextUtil
 *
 * This function initializes the GailTextUtil with the specified character string,
 **/
void
gail_text_util_text_setup (GailTextUtil *textutil,
                           const gchar  *text)
{
  g_return_if_fail (GAIL_IS_TEXT_UTIL (textutil));

  if (textutil->buffer)
    {
      if (!text)
        {
          g_object_unref (textutil->buffer);
          textutil->buffer = NULL;
          return;
        }
    }
  else
    {
      textutil->buffer = gtk_text_buffer_new (NULL);
    }

  gtk_text_buffer_set_text (textutil->buffer, text, -1);
}

/**
 * gail_text_util_buffer_setup:
 * @textutil: A #GailTextUtil to be initialized
 * @buffer: The #GtkTextBuffer which identifies the text to be stored in the GailUtil.
 *
 * This function initializes the GailTextUtil with the specified GtkTextBuffer
 **/
void
gail_text_util_buffer_setup  (GailTextUtil  *textutil,
                              GtkTextBuffer   *buffer)
{
  g_return_if_fail (GAIL_IS_TEXT_UTIL (textutil));

  textutil->buffer = g_object_ref (buffer);
}

/**
 * gail_text_util_get_text:
 * @textutil: A #GailTextUtil
 * @layout: A gpointer which is a PangoLayout, a GtkTreeView of NULL
 * @function: An enumeration specifying whether to return the text before, at, or
 *   after the offset.
 * @boundary_type: The boundary type.
 * @offset: The offset of the text in the GailTextUtil 
 * @start_offset: Address of location in which the start offset is returned
 * @end_offset: Address of location in which the end offset is returned
 *
 * This function gets the requested substring from the text in the GtkTextUtil.
 * The layout is used only for getting the text on a line. The value is NULL 
 * for a GtkTextView which is not wrapped, is a GtkTextView for a GtkTextView 
 * which is wrapped and is a PangoLayout otherwise.
 *
 * Returns: the substring requested
 **/
gchar*
gail_text_util_get_text (GailTextUtil    *textutil,
                         gpointer        layout,
                         GailOffsetType  function,
                         AtkTextBoundary boundary_type,
                         gint            offset,
                         gint            *start_offset,
                         gint            *end_offset)
{
  GtkTextIter start, end;
  gint line_number;
  GtkTextBuffer *buffer;

  g_return_val_if_fail (GAIL_IS_TEXT_UTIL (textutil), NULL);

  buffer = textutil->buffer;
  if (buffer == NULL)
    {
      *start_offset = 0;
      *end_offset = 0;
      return NULL;
    }

  if (!gtk_text_buffer_get_char_count (buffer))
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }
  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);

    
  end = start;

  switch (function)
    {
    case GAIL_BEFORE_OFFSET:
      switch (boundary_type)
        {
        case ATK_TEXT_BOUNDARY_CHAR:
          gtk_text_iter_backward_char(&start);
          break;
        case ATK_TEXT_BOUNDARY_WORD_START:
          if (!gtk_text_iter_starts_word (&start))
            gtk_text_iter_backward_word_start (&start);
          end = start;
          gtk_text_iter_backward_word_start(&start);
          break;
        case ATK_TEXT_BOUNDARY_WORD_END:
          if (gtk_text_iter_inside_word (&start) &&
              !gtk_text_iter_starts_word (&start))
            gtk_text_iter_backward_word_start (&start);
          while (!gtk_text_iter_ends_word (&start))
            {
              if (!gtk_text_iter_backward_char (&start))
                break;
            }
          end = start;
          gtk_text_iter_backward_word_start(&start);
          while (!gtk_text_iter_ends_word (&start))
            {
              if (!gtk_text_iter_backward_char (&start))
                break;
            }
          break;
        case ATK_TEXT_BOUNDARY_SENTENCE_START:
          if (!gtk_text_iter_starts_sentence (&start))
            gtk_text_iter_backward_sentence_start (&start);
          end = start;
          gtk_text_iter_backward_sentence_start (&start);
          break;
        case ATK_TEXT_BOUNDARY_SENTENCE_END:
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
        case ATK_TEXT_BOUNDARY_LINE_START:
          if (layout == NULL)
            {
              line_number = gtk_text_iter_get_line (&start);
              if (line_number == 0)
                {
                  gtk_text_buffer_get_iter_at_offset (buffer,
                    &start, 0);
                }
              else
                {
                  gtk_text_iter_backward_line (&start);
                  gtk_text_iter_forward_line (&start);
                }
              end = start;
              gtk_text_iter_backward_line (&start);
            }
          else if GTK_IS_TEXT_VIEW (layout)
            {
              GtkTextView *view = GTK_TEXT_VIEW (layout);

              gtk_text_view_backward_display_line_start (view, &start);
              end = start;
              gtk_text_view_backward_display_line (view, &start);
            }
          else if (PANGO_IS_LAYOUT (layout))
            get_pango_text_offsets (PANGO_LAYOUT (layout),
                                    buffer,
                                    function,
                                    boundary_type,
                                    offset,
                                    start_offset,
                                    end_offset,
                                    &start,
                                    &end);
          break;
        case ATK_TEXT_BOUNDARY_LINE_END:
          if (layout == NULL)
            {
              line_number = gtk_text_iter_get_line (&start);
              if (line_number == 0)
                {
                  gtk_text_buffer_get_iter_at_offset (buffer,
                    &start, 0);
                  end = start;
                }
              else
                {
                  gtk_text_iter_backward_line (&start);
                  end = start;
                  while (!gtk_text_iter_ends_line (&start))
                    {
                      if (!gtk_text_iter_backward_char (&start))
                        break;
                    }
                  gtk_text_iter_forward_to_line_end (&end);
                }
            }
          else if GTK_IS_TEXT_VIEW (layout)
            {
              GtkTextView *view = GTK_TEXT_VIEW (layout);

              gtk_text_view_backward_display_line_start (view, &start);
              if (!gtk_text_iter_is_start (&start))
                {
                  gtk_text_view_backward_display_line (view, &start);
                  end = start;
                  if (!gtk_text_iter_is_start (&start))
                    {
                      gtk_text_view_backward_display_line (view, &start);
                      gtk_text_view_forward_display_line_end (view, &start);
                    }
                  gtk_text_view_forward_display_line_end (view, &end);
                } 
              else
                {
                  end = start;
                }
            }
          else if (PANGO_IS_LAYOUT (layout))
            get_pango_text_offsets (PANGO_LAYOUT (layout),
                                    buffer,
                                    function,
                                    boundary_type,
                                    offset,
                                    start_offset,
                                    end_offset,
                                    &start,
                                    &end);
          break;
        }
      break;
 
    case GAIL_AT_OFFSET:
      switch (boundary_type)
        {
        case ATK_TEXT_BOUNDARY_CHAR:
          gtk_text_iter_forward_char (&end);
          break;
        case ATK_TEXT_BOUNDARY_WORD_START:
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
        case ATK_TEXT_BOUNDARY_WORD_END:
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
        case ATK_TEXT_BOUNDARY_SENTENCE_START:
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
        case ATK_TEXT_BOUNDARY_SENTENCE_END:
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
        case ATK_TEXT_BOUNDARY_LINE_START:
          if (layout == NULL)
            {
              line_number = gtk_text_iter_get_line (&start);
              if (line_number == 0)
                {
                  gtk_text_buffer_get_iter_at_offset (buffer,
                    &start, 0);
                }
              else
                {
                  gtk_text_iter_backward_line (&start);
                  gtk_text_iter_forward_line (&start);
                }
              gtk_text_iter_forward_line (&end);
            }
          else if GTK_IS_TEXT_VIEW (layout)
            {
              GtkTextView *view = GTK_TEXT_VIEW (layout);

              gtk_text_view_backward_display_line_start (view, &start);
              /*
               * The call to gtk_text_iter_forward_to_end() is needed
               * because of bug 81960
               */
              if (!gtk_text_view_forward_display_line (view, &end))
                gtk_text_iter_forward_to_end (&end);
            }
          else if PANGO_IS_LAYOUT (layout)
            get_pango_text_offsets (PANGO_LAYOUT (layout),
                                    buffer,
                                    function,
                                    boundary_type,
                                    offset,
                                    start_offset,
                                    end_offset,
                                    &start,
                                    &end);

          break;
        case ATK_TEXT_BOUNDARY_LINE_END:
          if (layout == NULL)
            {
              line_number = gtk_text_iter_get_line (&start);
              if (line_number == 0)
                {
                  gtk_text_buffer_get_iter_at_offset (buffer,
                    &start, 0);
                }
              else
                {
                  gtk_text_iter_backward_line (&start);
                  gtk_text_iter_forward_line (&start);
                }
              while (!gtk_text_iter_ends_line (&start))
                {
                  if (!gtk_text_iter_backward_char (&start))
                    break;
                }
              gtk_text_iter_forward_to_line_end (&end);
            }
          else if GTK_IS_TEXT_VIEW (layout)
            {
              GtkTextView *view = GTK_TEXT_VIEW (layout);

              gtk_text_view_backward_display_line_start (view, &start);
              if (!gtk_text_iter_is_start (&start))
                {
                  gtk_text_view_backward_display_line (view, &start);
                  gtk_text_view_forward_display_line_end (view, &start);
                } 
              gtk_text_view_forward_display_line_end (view, &end);
            }
          else if PANGO_IS_LAYOUT (layout)
            get_pango_text_offsets (PANGO_LAYOUT (layout),
                                    buffer,
                                    function,
                                    boundary_type,
                                    offset,
                                    start_offset,
                                    end_offset,
                                    &start,
                                    &end);
          break;
        }
      break;
  
    case GAIL_AFTER_OFFSET:
      switch (boundary_type)
        {
        case ATK_TEXT_BOUNDARY_CHAR:
          gtk_text_iter_forward_char(&start);
          gtk_text_iter_forward_chars(&end, 2);
          break;
        case ATK_TEXT_BOUNDARY_WORD_START:
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
        case ATK_TEXT_BOUNDARY_WORD_END:
          gtk_text_iter_forward_word_end (&end);
          start = end;
          if (!gtk_text_iter_is_end (&end))
            gtk_text_iter_forward_word_end (&end);
          break;
        case ATK_TEXT_BOUNDARY_SENTENCE_START:
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
        case ATK_TEXT_BOUNDARY_SENTENCE_END:
          gtk_text_iter_forward_sentence_end (&end);
          start = end;
          if (!gtk_text_iter_is_end (&end))
            gtk_text_iter_forward_sentence_end (&end);
          break;
        case ATK_TEXT_BOUNDARY_LINE_START:
          if (layout == NULL)
            {
              gtk_text_iter_forward_line (&end);
              start = end;
              gtk_text_iter_forward_line (&end);
            }
          else if GTK_IS_TEXT_VIEW (layout)
            {
              GtkTextView *view = GTK_TEXT_VIEW (layout);

              gtk_text_view_forward_display_line (view, &end);
              start = end; 
              gtk_text_view_forward_display_line (view, &end);
            }
          else if (PANGO_IS_LAYOUT (layout))
            get_pango_text_offsets (PANGO_LAYOUT (layout),
                                    buffer,
                                    function,
                                    boundary_type,
                                    offset,
                                    start_offset,
                                    end_offset,
                                    &start,
                                    &end);
          break;
        case ATK_TEXT_BOUNDARY_LINE_END:
          if (layout == NULL)
            {
              gtk_text_iter_forward_line (&start);
              end = start;
              if (!gtk_text_iter_is_end (&start))
                { 
                  while (!gtk_text_iter_ends_line (&start))
                  {
                    if (!gtk_text_iter_backward_char (&start))
                      break;
                  }
                  gtk_text_iter_forward_to_line_end (&end);
                }
            }
          else if GTK_IS_TEXT_VIEW (layout)
            {
              GtkTextView *view = GTK_TEXT_VIEW (layout);

              gtk_text_view_forward_display_line_end (view, &end);
              start = end; 
              gtk_text_view_forward_display_line (view, &end);
              gtk_text_view_forward_display_line_end (view, &end);
            }
          else if (PANGO_IS_LAYOUT (layout))
            get_pango_text_offsets (PANGO_LAYOUT (layout),
                                    buffer,
                                    function,
                                    boundary_type,
                                    offset,
                                    start_offset,
                                    end_offset,
                                    &start,
                                    &end);
          break;
        }
      break;
    }
  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

/**
 * gail_text_util_get_substring:
 * @textutil: A #GailTextUtil
 * @start_pos: The start position of the substring
 * @end_pos: The end position of the substring.
 *
 * Gets the substring indicated by @start_pos and @end_pos
 *
 * Returns: the substring indicated by @start_pos and @end_pos
 **/
gchar*
gail_text_util_get_substring (GailTextUtil *textutil,
                              gint         start_pos, 
                              gint         end_pos)
{
  GtkTextIter start, end;
  GtkTextBuffer *buffer;

  g_return_val_if_fail(GAIL_IS_TEXT_UTIL (textutil), NULL);

  buffer = textutil->buffer;
  if (buffer == NULL)
     return NULL;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
  if (end_pos < 0)
    gtk_text_buffer_get_end_iter (buffer, &end);
  else
    gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
get_pango_text_offsets (PangoLayout         *layout,
                        GtkTextBuffer       *buffer,
                        GailOffsetType      function,
                        AtkTextBoundary     boundary_type,
                        gint                offset,
                        gint                *start_offset,
                        gint                *end_offset,
                        GtkTextIter         *start_iter,
                        GtkTextIter         *end_iter)
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
          /*
           * Found line for offset
           */
          switch (function)
            {
            case GAIL_BEFORE_OFFSET:
                  /*
                   * We want the previous line
                   */
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
                        start_index = prev_prev_line->start_index + 
                                  prev_prev_line->length;
                      end_index = prev_line->start_index + prev_line->length;
                      break;
                    default:
                      g_assert_not_reached();
                    }
                }
              else
                start_index = end_index = 0;
              break;
            case GAIL_AT_OFFSET:
              switch (boundary_type)
                {
                case ATK_TEXT_BOUNDARY_LINE_START:
                  if (pango_layout_iter_next_line (iter))
                    end_index = pango_layout_iter_get_line (iter)->start_index;
                  break;
                case ATK_TEXT_BOUNDARY_LINE_END:
                  if (prev_line)
                    start_index = prev_line->start_index + 
                                  prev_line->length;
                  break;
                default:
                  g_assert_not_reached();
                }
              break;
            case GAIL_AFTER_OFFSET:
               /*
                * We want the next line
                */
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
                    default:
                      g_assert_not_reached();
                    }
                }
              else
                start_index = end_index;
              break;
            }
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
 
  gtk_text_buffer_get_iter_at_offset (buffer, start_iter, *start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, end_iter, *end_offset);
}
