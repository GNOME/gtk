/* GTK - The GIMP Toolkit
 * gtktextbuffer.c Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */


#include <string.h>

#include "gtkclipboard.h"
#include "gtkinvisible.h"
#include "gtksignal.h"
#include "gtktextbuffer.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"
#include <string.h>

typedef struct _ClipboardRequest ClipboardRequest;

struct _ClipboardRequest
{
  GtkTextBuffer *buffer;
  gboolean interactive;
  gboolean default_editable;
  gboolean is_clipboard;
  gboolean replace_selection;
};

enum {
  INSERT_TEXT,
  DELETE_TEXT,
  CHANGED,
  MODIFIED_CHANGED,
  MARK_SET,
  MARK_DELETED,
  APPLY_TAG,
  REMOVE_TAG,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

enum {
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_UTF8_STRING,
  TARGET_TEXT_BUFFER_CONTENTS
};

static void gtk_text_buffer_init       (GtkTextBuffer      *tkxt_buffer);
static void gtk_text_buffer_class_init (GtkTextBufferClass *klass);
static void gtk_text_buffer_finalize   (GObject            *object);


static void gtk_text_buffer_update_primary_selection   (GtkTextBuffer     *buffer);
static void gtk_text_buffer_real_insert_text           (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *iter,
                                                        const gchar       *text,
                                                        gint               len,
                                                        gboolean           interactive);
static void gtk_text_buffer_real_delete_text           (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *start,
                                                        GtkTextIter       *end,
                                                        gboolean           interactive);
static void gtk_text_buffer_real_apply_tag             (GtkTextBuffer     *buffer,
                                                        GtkTextTag        *tag,
                                                        const GtkTextIter *start_char,
                                                        const GtkTextIter *end_char);
static void gtk_text_buffer_real_remove_tag            (GtkTextBuffer     *buffer,
                                                        GtkTextTag        *tag,
                                                        const GtkTextIter *start_char,
                                                        const GtkTextIter *end_char);
static void gtk_text_buffer_real_changed               (GtkTextBuffer     *buffer);

static GtkTextBTree* get_btree (GtkTextBuffer *buffer);

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_buffer_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextBuffer",
        sizeof (GtkTextBuffer),
        sizeof (GtkTextBufferClass),
        (GtkClassInitFunc) gtk_text_buffer_class_init,
        (GtkObjectInitFunc) gtk_text_buffer_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      our_type = gtk_type_unique (GTK_TYPE_OBJECT, &our_info);
    }

  return our_type;
}

static void
gtk_text_buffer_class_init (GtkTextBufferClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  signals[INSERT_TEXT] =
    gtk_signal_new ("insert_text",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, insert_text),
                    gtk_marshal_VOID__BOXED_BOXED_INT_BOOLEAN,
                    GTK_TYPE_NONE,
                    4,
                    GTK_TYPE_TEXT_ITER,
                    GTK_TYPE_TEXT_ITER,
                    GTK_TYPE_INT,
                    GTK_TYPE_BOOL);

  signals[DELETE_TEXT] =
    gtk_signal_new ("delete_text",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, delete_text),
                    gtk_marshal_VOID__BOXED_BOXED_BOOLEAN,
                    GTK_TYPE_NONE,
                    3,
                    GTK_TYPE_TEXT_ITER,
                    GTK_TYPE_TEXT_ITER,
                    GTK_TYPE_BOOL);

  signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, changed),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE,
                    0);

  signals[MODIFIED_CHANGED] =
    gtk_signal_new ("modified_changed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, modified_changed),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE,
                    0);

  signals[MARK_SET] =
    gtk_signal_new ("mark_set",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, mark_set),
                    gtk_marshal_VOID__BOXED_OBJECT,
                    GTK_TYPE_NONE,
                    2,
                    GTK_TYPE_TEXT_ITER,
                    G_TYPE_OBJECT);

  signals[MARK_DELETED] =
    gtk_signal_new ("mark_deleted",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, mark_deleted),
                    gtk_marshal_VOID__OBJECT,
                    GTK_TYPE_NONE,
                    1,
                    G_TYPE_OBJECT);

  signals[APPLY_TAG] =
    gtk_signal_new ("apply_tag",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, apply_tag),
                    gtk_marshal_VOID__OBJECT_BOXED_BOXED,
                    GTK_TYPE_NONE,
                    3,
                    G_TYPE_OBJECT,
                    GTK_TYPE_TEXT_ITER,
                    GTK_TYPE_TEXT_ITER);

  signals[REMOVE_TAG] =
    gtk_signal_new ("remove_tag",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, remove_tag),
                    gtk_marshal_VOID__OBJECT_BOXED_BOXED,
                    GTK_TYPE_NONE,
                    3,
                    G_TYPE_OBJECT,
                    GTK_TYPE_TEXT_ITER,
                    GTK_TYPE_TEXT_ITER);

  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

  gobject_class->finalize = gtk_text_buffer_finalize;

  klass->insert_text = gtk_text_buffer_real_insert_text;
  klass->delete_text = gtk_text_buffer_real_delete_text;
  klass->apply_tag = gtk_text_buffer_real_apply_tag;
  klass->remove_tag = gtk_text_buffer_real_remove_tag;
  klass->changed = gtk_text_buffer_real_changed;
}

void
gtk_text_buffer_init (GtkTextBuffer *buffer)
{
  buffer->clipboard_contents = NULL;
}

/**
 * gtk_text_buffer_new:
 * @table: a tag table, or NULL to create a new one
 *
 * Creates a new text buffer.
 *
 * Return value: a new text buffer
 **/
GtkTextBuffer*
gtk_text_buffer_new (GtkTextTagTable *table)
{
  GtkTextBuffer *text_buffer;

  text_buffer = GTK_TEXT_BUFFER (gtk_type_new (gtk_text_buffer_get_type ()));

  if (table)
    {
      text_buffer->tag_table = table;

      gtk_object_ref (GTK_OBJECT (text_buffer->tag_table));
      gtk_object_sink (GTK_OBJECT (text_buffer->tag_table));
    }

  gtk_object_ref (GTK_OBJECT (text_buffer));
  gtk_object_sink (GTK_OBJECT (text_buffer));
  
  return text_buffer;
}

static void
gtk_text_buffer_finalize (GObject *object)
{
  GtkTextBuffer *buffer;

  buffer = GTK_TEXT_BUFFER (object);

  if (buffer->clipboard_contents)
    {
      g_object_unref (G_OBJECT (buffer->clipboard_contents));
      buffer->clipboard_contents = NULL;
    }
  
  if (buffer->tag_table)
    {
      gtk_object_unref (GTK_OBJECT (buffer->tag_table));
      buffer->tag_table = NULL;
    }

  if (buffer->btree)
    {
      gtk_text_btree_unref (buffer->btree);
      buffer->btree = NULL;
    }
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtkTextTagTable*
get_table (GtkTextBuffer *buffer)
{
  if (buffer->tag_table == NULL)
    {
      buffer->tag_table = gtk_text_tag_table_new ();

      gtk_object_ref (GTK_OBJECT (buffer->tag_table));
      gtk_object_sink (GTK_OBJECT (buffer->tag_table));
    }

  return buffer->tag_table;
}

static GtkTextBTree*
get_btree (GtkTextBuffer *buffer)
{
  if (buffer->btree == NULL)
    buffer->btree = gtk_text_btree_new (gtk_text_buffer_get_tag_table (buffer),
                                        buffer);

  return buffer->btree;
}

GtkTextBTree*
_gtk_text_buffer_get_btree (GtkTextBuffer *buffer)
{
  return get_btree (buffer);
}

/**
 * gtk_text_buffer_get_tag_table:
 * @buffer: a #GtkTextBuffer
 *
 * Get the #GtkTextTagTable associated with this buffer.
 *
 * Return value: the buffer's tag table
 **/
GtkTextTagTable*
gtk_text_buffer_get_tag_table (GtkTextBuffer  *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return get_table (buffer);
}

/**
 * gtk_text_buffer_set_text:
 * @buffer: a #GtkTextBuffer
 * @text: UTF-8 text to insert
 * @len: length of @text in bytes
 *
 * Deletes current contents of @buffer, and inserts @text instead.  If
 * @text doesn't end with a newline, a newline is added;
 * #GtkTextBuffer contents must always end with a newline. If @text
 * ends with a newline, the new buffer contents will be exactly
 * @text. If @len is -1, @text must be nul-terminated.
 **/
void
gtk_text_buffer_set_text (GtkTextBuffer *buffer,
                          const gchar   *text,
                          gint           len)
{
  GtkTextIter start, end;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  /* Chop newline, since the buffer will already have one
   * in it.
   */
  if (len > 0 && text[len-1] == '\n')
    len -= 1;

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  gtk_text_buffer_delete (buffer, &start, &end);

  if (len > 0)
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
      gtk_text_buffer_insert (buffer, &start, text, len);
    }
}

/*
 * Insertion
 */

static void

gtk_text_buffer_real_insert_text (GtkTextBuffer *buffer,
                                  GtkTextIter *iter,
                                  const gchar *text,
                                  gint len,
                                  gboolean interactive)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);

  gtk_text_btree_insert (iter, text, len);

  gtk_signal_emit (GTK_OBJECT (buffer), signals[CHANGED]);
}

static void
gtk_text_buffer_emit_insert (GtkTextBuffer *buffer,
                             GtkTextIter *iter,
                             const gchar *text,
                             gint len,
                             gboolean interactive)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  if (len > 0)
    {
      gtk_signal_emit (GTK_OBJECT (buffer), signals[INSERT_TEXT],
                       iter, text, len, interactive);
    }
}

/**
 * gtk_text_buffer_insert:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in the buffer
 * @text: UTF-8 format text to insert
 * @len: length of text in bytes, or -1
 *
 * Inserts @len bytes of @text at position @iter.  If @len is -1,
 * @text must be nul-terminated and will be inserted in its
 * entirety. Emits the "insert_text" signal; insertion actually occurs
 * in the default handler for the signal. @iter is invalidated when
 * insertion occurs (because the buffer contents change), but the
 * default signal handler revalidates it to point to the end of the
 * inserted text.
 *
 **/
void
gtk_text_buffer_insert (GtkTextBuffer *buffer,
                        GtkTextIter *iter,
                        const gchar *text,
                        gint len)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);

  gtk_text_buffer_emit_insert (buffer, iter, text, len, FALSE);
}

/**
 * gtk_text_buffer_insert_at_cursor:
 * @buffer: a #GtkTextBuffer
 * @text: some text in UTF-8 format
 * @len: length of text, in bytes
 *
 * Simply calls gtk_text_buffer_insert (), using the current
 * cursor position as the insertion point.
 **/
void
gtk_text_buffer_insert_at_cursor (GtkTextBuffer *buffer,
                                  const gchar *text,
                                  gint len)
{
  GtkTextIter iter;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (text != NULL);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                    gtk_text_buffer_get_mark (buffer,
                                                              "insert"));

  gtk_text_buffer_insert (buffer, &iter, text, len);
}

/**
 * gtk_text_buffer_insert_interactive:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @text: some UTF-8 text
 * @len: length of text in bytes, or -1
 * @default_editable: default editability of buffer
 *
 * Like gtk_text_buffer_insert (), but the insertion will not occur if
 * @iter is at a non-editable location in the buffer.  Usually you
 * want to prevent insertions at ineditable locations if the insertion
 * results from a user action (is interactive).
 *
 * @default_editable indicates the editability of text that doesn't
 * have a tag affecting editability applied to it. Typically the
 * result of gtk_text_view_get_editable() is appropriate here.
 *
 * Return value: whether text was actually inserted
 **/
gboolean
gtk_text_buffer_insert_interactive (GtkTextBuffer *buffer,
                                    GtkTextIter   *iter,
                                    const gchar   *text,
                                    gint           len,
                                    gboolean       default_editable)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (text != NULL, FALSE);

  if (gtk_text_iter_editable (iter, default_editable))
    {
      gtk_text_buffer_emit_insert (buffer, iter, text, len, TRUE);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_buffer_insert_interactive_at_cursor:
 * @buffer: a #GtkTextBuffer
 * @text: text in UTF-8 format
 * @len: length of text in bytes, or -1
 * @default_editable: default editability of buffer
 *
 * Calls gtk_text_buffer_insert_interactive () at the cursor
 * position.
 *
 * @default_editable indicates the editability of text that doesn't
 * have a tag affecting editability applied to it. Typically the
 * result of gtk_text_view_get_editable() is appropriate here.
 * 
 * Return value: whether text was actually inserted
 **/
gboolean
gtk_text_buffer_insert_interactive_at_cursor (GtkTextBuffer *buffer,
                                              const gchar   *text,
                                              gint           len,
                                              gboolean       default_editable)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (text != NULL, FALSE);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                    gtk_text_buffer_get_mark (buffer,
                                                              "insert"));

  return gtk_text_buffer_insert_interactive (buffer, &iter, text, len,
                                             default_editable);
}

static gboolean
possibly_not_text (gunichar ch,
                   gpointer user_data)
{
  return ch == 0xFFFD;
}

static void
insert_text_range (GtkTextBuffer     *buffer,
                   GtkTextIter       *iter,
                   const GtkTextIter *orig_start,
                   const GtkTextIter *orig_end,
                   gboolean           interactive)
{
  gchar *text;

  text = gtk_text_iter_get_text (orig_start, orig_end);

  gtk_text_buffer_emit_insert (buffer, iter, text, -1, interactive);

  g_free (text);
}

typedef struct _Range Range;
struct _Range
{
  GtkTextBuffer *buffer;
  GtkTextMark *start_mark;
  GtkTextMark *end_mark;
  GtkTextMark *whole_end_mark;
  GtkTextIter *range_start;
  GtkTextIter *range_end;
  GtkTextIter *whole_end;
};

static Range*
save_range (GtkTextIter *range_start,
            GtkTextIter *range_end,
            GtkTextIter *whole_end)
{
  Range *r;

  r = g_new (Range, 1);

  r->buffer = gtk_text_iter_get_buffer (range_start);
  g_object_ref (G_OBJECT (r->buffer));
  
  r->start_mark = 
    gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                 NULL,
                                 range_start,
                                 TRUE);
  r->end_mark = 
    gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                 NULL,
                                 range_end,
                                 FALSE);

  r->whole_end_mark = 
    gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                 NULL,
                                 whole_end,
                                 FALSE);

  r->range_start = range_start;
  r->range_end = range_end;
  r->whole_end = whole_end;

  return r;
}

static void
restore_range (Range *r)
{
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->range_start,
                                    r->start_mark);
      
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->range_end,
                                    r->end_mark);
      
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->whole_end,
                                    r->whole_end_mark);
      
  gtk_text_buffer_delete_mark (r->buffer, r->start_mark);
  gtk_text_buffer_delete_mark (r->buffer, r->end_mark);
  gtk_text_buffer_delete_mark (r->buffer, r->whole_end_mark);

  g_object_unref (G_OBJECT (r->buffer));
  g_free (r); 
}

static void
insert_range_untagged (GtkTextBuffer     *buffer,
                       GtkTextIter       *iter,
                       const GtkTextIter *orig_start,
                       const GtkTextIter *orig_end,
                       gboolean           interactive)
{
  GtkTextIter range_start;
  GtkTextIter range_end;
  GtkTextIter start, end;
  GtkTextBuffer *src_buffer;
  Range *r;
  
  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  start = *orig_start;
  end = *orig_end;
  
  src_buffer = gtk_text_iter_get_buffer (&start);
  
  range_start = start;
  range_end = start;
  
  while (TRUE)
    {
      if (gtk_text_iter_equal (&range_start, &range_end))
        {
          /* Figure out how to move forward */

          g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
          
          if (gtk_text_iter_equal (&range_end, &end))
            {
              /* nothing left to do */
              break;
            }
          else if (gtk_text_iter_get_char (&range_end) == 0xFFFD)
            {
              GdkPixbuf *pixbuf = NULL;
              GtkTextChildAnchor *anchor = NULL;
              pixbuf = gtk_text_iter_get_pixbuf (&range_end);
              anchor = gtk_text_iter_get_child_anchor (&range_end);

              if (pixbuf)
                {
                  r = save_range (&range_start,
                                  &range_end,
                                  &end);

                  gtk_text_buffer_insert_pixbuf (buffer,
                                                 iter,
                                                 pixbuf);

                  restore_range (r);
                  r = NULL;
                  
                  gtk_text_iter_next_char (&range_end);
                  
                  range_start = range_end;
                }
              else if (anchor)
                {
                  /* Just skip anchors */

                  gtk_text_iter_next_char (&range_end);
                  range_start = range_end;
                }
              else
                {
                  /* The 0xFFFD was in a text segment, so
                   * keep going. 
                   */
                  gtk_text_iter_forward_find_char (&range_end,
                                                   possibly_not_text, NULL,
                                                   &end);
                  
                  g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
                }
            }
          else
            {
              /* Text segment starts here, so forward search to
               * find its possible endpoint
               */
              gtk_text_iter_forward_find_char (&range_end,
                                               possibly_not_text, NULL,
                                               &end);
              
              g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
            }
        }
      else
        {
          r = save_range (&range_start,
                          &range_end,
                          &end);
          
          insert_text_range (buffer,
                             iter,
                             &range_start,
                             &range_end,
                             interactive);

          restore_range (r);
          r = NULL;
          
          range_start = range_end;
        }
    }
}

static void
gtk_text_buffer_real_insert_range (GtkTextBuffer     *buffer,
                                   GtkTextIter       *iter,
                                   const GtkTextIter *orig_start,
                                   const GtkTextIter *orig_end,
                                   gboolean           interactive)
{
  /* Find each range of uniformly-tagged text, insert it,
   * then apply the tags.
   */
  GtkTextIter start = *orig_start;
  GtkTextIter end = *orig_end;
  GtkTextIter range_start;
  GtkTextIter range_end;
  GtkTextBuffer *src_buffer;
  Range *r;
  
  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  src_buffer = gtk_text_iter_get_buffer (orig_start);
  
  gtk_text_iter_reorder (&start, &end);

  range_start = start;
  range_end = start;

  while (TRUE)
    {
      gint start_offset;
      GtkTextIter start_iter;
      GSList *tags;
      GSList *tmp_list;
      
      if (gtk_text_iter_equal (&range_start, &end))
        break; /* All done */

      g_assert (gtk_text_iter_compare (&range_start, &end) < 0);
      
      gtk_text_iter_forward_to_tag_toggle (&range_end, NULL);

      g_assert (!gtk_text_iter_equal (&range_start, &range_end));

      /* Clamp to the end iterator */
      if (gtk_text_iter_compare (&range_end, &end) > 0)
        range_end = end;
      
      /* We have a range with unique tags; insert it, and
       * apply all tags.
       */
      start_offset = gtk_text_iter_get_offset (iter);

      r = save_range (&range_start, &range_end, &end);
      
      insert_range_untagged (buffer, iter, &range_start, &range_end, interactive);

      restore_range (r);
      r = NULL;
      
      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start_offset);
      
      tags = gtk_text_iter_get_tags (&range_start);
      tmp_list = tags;
      while (tmp_list != NULL)
        {
          gtk_text_buffer_apply_tag (buffer,
                                     tmp_list->data,
                                     &start_iter,
                                     iter);
          
          tmp_list = g_slist_next (tmp_list);
        }
      g_slist_free (tags);

      range_start = range_end;
    }
}

/**
 * gtk_text_buffer_insert_range:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @start: a position in a #GtkTextBuffer
 * @end: another position in the same buffer as @start
 *
 * Copies text, tags, and pixbufs between @start and @end (the order
 * of @start and @end doesn't matter) and inserts the copy at @iter.
 * Used instead of simply getting/inserting text because it preserves
 * images and tags. If @start and @end are in a different buffer from
 * @buffer, the two buffers must share the same tag table.
 *
 * Implemented via emissions of the insert_text and apply_tag signals,
 * so expect those.
 **/
void
gtk_text_buffer_insert_range (GtkTextBuffer     *buffer,
                              GtkTextIter       *iter,
                              const GtkTextIter *start,
                              const GtkTextIter *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) ==
                    gtk_text_iter_get_buffer (end));
  g_return_if_fail (gtk_text_iter_get_buffer (start)->tag_table ==
                    buffer->tag_table);  

  gtk_text_buffer_real_insert_range (buffer, iter, start, end, FALSE);
}

/**
 * gtk_text_buffer_insert_range_interactive:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @start: a position in a #GtkTextBuffer
 * @end: another position in the same buffer as @start
 * @default_editable: default editability of the buffer
 *
 * Same as gtk_text_buffer_insert_range(), but does nothing if the
 * insertion point isn't editable. The @default_editable parameter
 * indicates whether the text is editable at @iter if no tags
 * enclosing @iter affect editability. Typically the result of
 * gtk_text_view_get_editable() is appropriate here.
 *
 * Returns: whether an insertion was possible at @iter
 **/
gboolean
gtk_text_buffer_insert_range_interactive (GtkTextBuffer     *buffer,
                                          GtkTextIter       *iter,
                                          const GtkTextIter *start,
                                          const GtkTextIter *end,
                                          gboolean           default_editable)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start) ==
                        gtk_text_iter_get_buffer (end), FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start)->tag_table ==
                        buffer->tag_table, FALSE);


  if (gtk_text_iter_editable (iter, default_editable))
    {
      gtk_text_buffer_real_insert_range (buffer, iter, start, end, TRUE);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_buffer_insert_with_tags:
 * @buffer: a #GtkTextBuffer
 * @iter: an iterator in @buffer
 * @text: UTF-8 text
 * @len: length of @text, or -1
 * @first_tag: first tag to apply to @text
 * @Varargs: NULL-terminated list of tags to apply
 *
 * Inserts @text into @buffer at @iter, applying the list of tags to
 * the newly-inserted text. The last tag specified must be NULL to
 * terminate the list. Equivalent to calling gtk_text_buffer_insert (),
 * then gtk_text_buffer_apply_tag () on the inserted text;
 * gtk_text_buffer_insert_with_tags () is just a convenience function.
 **/
void
gtk_text_buffer_insert_with_tags (GtkTextBuffer *buffer,
                                  GtkTextIter   *iter,
                                  const gchar   *text,
                                  gint           len,
                                  GtkTextTag    *first_tag,
                                  ...)
{
  gint start_offset;
  GtkTextIter start;
  va_list args;
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);

  start_offset = gtk_text_iter_get_offset (iter);

  gtk_text_buffer_insert (buffer, iter, text, len);

  if (first_tag == NULL)
    return;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);

  va_start (args, first_tag);
  tag = first_tag;
  while (tag)
    {
      gtk_text_buffer_apply_tag (buffer, tag, &start, iter);

      tag = va_arg (args, GtkTextTag*);
    }

  va_end (args);
}

/**
 * gtk_text_buffer_insert_with_tags_by_name:
 * @buffer: a #GtkTextBuffer
 * @iter: position in @buffer
 * @text: UTF-8 text
 * @len: length of @text, or -1
 * @first_tag_name: name of a tag to apply to @text
 * @Varargs: more tag names
 *
 * Same as gtk_text_buffer_insert_with_tags (), but allows you
 * to pass in tag names instead of tag objects.
 **/
void
gtk_text_buffer_insert_with_tags_by_name  (GtkTextBuffer *buffer,
                                           GtkTextIter   *iter,
                                           const gchar   *text,
                                           gint           len,
                                           const gchar   *first_tag_name,
                                           ...)
{
  gint start_offset;
  GtkTextIter start;
  va_list args;
  const gchar *tag_name;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);

  start_offset = gtk_text_iter_get_offset (iter);

  gtk_text_buffer_insert (buffer, iter, text, len);

  if (first_tag_name == NULL)
    return;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);

  va_start (args, first_tag_name);
  tag_name = first_tag_name;
  while (tag_name)
    {
      GtkTextTag *tag;

      tag = gtk_text_tag_table_lookup (buffer->tag_table,
                                       tag_name);

      if (tag == NULL)
        {
          g_warning ("%s: no tag with name '%s'!", G_STRLOC, tag_name);
          return;
        }

      gtk_text_buffer_apply_tag (buffer, tag, &start, iter);

      tag_name = va_arg (args, const gchar*);
    }

  va_end (args);
}


/*
 * Deletion
 */

static void
gtk_text_buffer_real_delete_text (GtkTextBuffer *buffer,
                                  GtkTextIter *start,
                                  GtkTextIter *end,
                                  gboolean interactive)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  gtk_text_btree_delete (start, end);

  /* may have deleted the selection... */
  gtk_text_buffer_update_primary_selection (buffer);

  gtk_signal_emit (GTK_OBJECT (buffer), signals[CHANGED]);
}

static void
gtk_text_buffer_emit_delete (GtkTextBuffer *buffer,
                             GtkTextIter *start,
                             GtkTextIter *end,
                             gboolean interactive)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  if (gtk_text_iter_equal (start, end))
    return;

  gtk_text_iter_reorder (start, end);

  /* Somewhat annoyingly, if you try to delete the final newline
   * the BTree will put it back; which means you can't deduce the
   * final contents of the buffer purely by monitoring insert/delete
   * signals on the buffer. But if you delete the final newline, any
   * tags on the newline will go away, oddly. See comment in
   * gtktextbtree.c. This is all sort of annoying, but really hard
   * to fix.
   */
  gtk_signal_emit (GTK_OBJECT (buffer),
                   signals[DELETE_TEXT],
                   start, end,
                   interactive);
}

/**
 * gtk_text_buffer_delete:
 * @buffer: a #GtkTextBuffer
 * @start: a position in @buffer
 * @end: another position in @buffer
 *
 * Deletes text between @start and @end. The order of @start and @end
 * is not actually relevant; gtk_text_buffer_delete () will reorder
 * them. This function actually emits the "delete_text" signal, and
 * the default handler of that signal deletes the text. Because the
 * buffer is modified, all outstanding iterators become invalid after
 * calling this function; however, the @start and @end will be
 * re-initialized to point to the location where text was deleted.
 *
 * Note that the final newline in the buffer may not be deleted; a
 * #GtkTextBuffer always contains at least one newline.  You can
 * safely include the final newline in the range [@start,@end) but it
 * won't be affected by the deletion.
 *
 **/
void
gtk_text_buffer_delete (GtkTextBuffer *buffer,
                        GtkTextIter   *start,
                        GtkTextIter   *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  gtk_text_buffer_emit_delete (buffer, start, end, FALSE);
}

/**
 * gtk_text_buffer_delete_interactive:
 * @buffer: a #GtkTextBuffer
 * @start_iter: start of range to delete
 * @end_iter: end of range
 * @default_editable: whether the buffer is editable by default
 *
 * Deletes all <emphasis>editable</emphasis> text in the given range.
 * Calls gtk_text_buffer_delete () for each editable sub-range of
 * [@start,@end). @start and @end are revalidated to point to
 * the location of the last deleted range, or left untouched if
 * no text was deleted.
 *
 * Return value: whether some text was actually deleted
 **/
gboolean
gtk_text_buffer_delete_interactive (GtkTextBuffer *buffer,
                                    GtkTextIter   *start_iter,
                                    GtkTextIter   *end_iter,
                                    gboolean       default_editable)
{
  GtkTextMark *end_mark;
  GtkTextMark *start_mark;
  GtkTextIter iter;
  gboolean current_state;
  gboolean deleted_stuff = FALSE;

  /* Delete all editable text in the range start_iter, end_iter */

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (start_iter != NULL, FALSE);
  g_return_val_if_fail (end_iter != NULL, FALSE);

  gtk_text_iter_reorder (start_iter, end_iter);

  start_mark = gtk_text_buffer_create_mark (buffer, NULL,
                                            start_iter, TRUE);
  end_mark = gtk_text_buffer_create_mark (buffer, NULL,
                                          end_iter, FALSE);
  iter = *start_iter;

  current_state = gtk_text_iter_editable (&iter, default_editable);

  while (TRUE)
    {
      gboolean new_state;
      gboolean done = FALSE;
      GtkTextIter end;

      gtk_text_iter_forward_to_tag_toggle (&iter, NULL);

      gtk_text_buffer_get_iter_at_mark (buffer, &end, end_mark);

      if (gtk_text_iter_compare (&iter, &end) >= 0)
        {
          done = TRUE;
          iter = end; /* clamp to the last boundary */
        }

      new_state = gtk_text_iter_editable (&iter, default_editable);

      if (current_state == new_state)
        {
          if (done)
            {
              if (current_state)
                {
                  /* We're ending an editable region. Delete said region. */
                  GtkTextIter start;

                  gtk_text_buffer_get_iter_at_mark (buffer, &start, start_mark);

                  gtk_text_buffer_emit_delete (buffer, &start, &iter, TRUE);

                  deleted_stuff = TRUE;

                  /* revalidate user's iterators. */
                  *start_iter = start;
                  *end_iter = iter;
                }

              break;
            }
          else
            continue;
        }

      if (current_state && !new_state)
        {
          /* End of an editable region. Delete it. */
          GtkTextIter start;

          gtk_text_buffer_get_iter_at_mark (buffer, &start, start_mark);

          gtk_text_buffer_emit_delete (buffer, &start, &iter, TRUE);

          current_state = FALSE;
          deleted_stuff = TRUE;

          /* revalidate user's iterators. */
          *start_iter = start;
          *end_iter = iter;
        }
      else
        {
          /* We are at the start of an editable region. We won't be deleting
           * the previous region. Move start mark to start of this region.
           */

          g_assert (!current_state && new_state);

          gtk_text_buffer_move_mark (buffer, start_mark,
                                     &iter);


          current_state = TRUE;
        }

      if (done)
        break;
    }


  gtk_text_buffer_delete_mark (buffer, start_mark);
  gtk_text_buffer_delete_mark (buffer, end_mark);

  return deleted_stuff;
}

/*
 * Extracting textual buffer contents
 */

/**
 * gtk_text_buffer_get_text:
 * @buffer: a #GtkTextBuffer
 * @start: start of a range
 * @end: end of a range
 * @include_hidden_chars: whether to include invisible text
 *
 * Returns the text in the range [@start,@end). Excludes undisplayed
 * text (text marked with tags that set the invisibility attribute) if
 * @include_hidden_chars is FALSE. Does not include characters
 * representing embedded images, so byte and character indexes into
 * the returned string do <emphasis>not</emphasis> correspond to byte
 * and character indexes into the buffer. Contrast with
 * gtk_text_buffer_get_slice ().
 *
 * Return value: an allocated UTF-8 string
 **/
gchar*
gtk_text_buffer_get_text (GtkTextBuffer      *buffer,
                          const GtkTextIter *start,
                          const GtkTextIter *end,
                          gboolean             include_hidden_chars)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  if (include_hidden_chars)
    return gtk_text_iter_get_text (start, end);
  else
    return gtk_text_iter_get_visible_text (start, end);
}

/**
 * gtk_text_buffer_get_slice:
 * @buffer: a #GtkTextBuffer
 * @start: start of a range
 * @end: end of a range
 * @include_hidden_chars: whether to include invisible text
 *
 * Returns the text in the range [@start,@end). Excludes undisplayed
 * text (text marked with tags that set the invisibility attribute) if
 * @include_hidden_chars is FALSE. The returned string includes a
 * 0xFFFD character whenever the buffer contains
 * embedded images, so byte and character indexes into
 * the returned string <emphasis>do</emphasis> correspond to byte
 * and character indexes into the buffer. Contrast with
 * gtk_text_buffer_get_text (). Note that 0xFFFD can occur in normal
 * text as well, so it is not a reliable indicator that a pixbuf or
 * widget is in the buffer.
 *
 * Return value: an allocated UTF-8 string
 **/
gchar*
gtk_text_buffer_get_slice (GtkTextBuffer      *buffer,
                           const GtkTextIter *start,
                           const GtkTextIter *end,
                           gboolean             include_hidden_chars)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  if (include_hidden_chars)
    return gtk_text_iter_get_slice (start, end);
  else
    return gtk_text_iter_get_visible_slice (start, end);
}

/*
 * Pixmaps
 */

void
gtk_text_buffer_insert_pixbuf         (GtkTextBuffer      *buffer,
                                       GtkTextIter        *iter,
                                       GdkPixbuf          *pixbuf)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  gtk_text_btree_insert_pixbuf (iter, pixbuf);

  /* FIXME pixbuf-specific signal like insert_text */

  gtk_signal_emit (GTK_OBJECT (buffer), signals[CHANGED]);
}

/*
 * Child anchor
 */

GtkTextChildAnchor*
gtk_text_buffer_create_child_anchor (GtkTextBuffer *buffer,
                                     GtkTextIter   *iter)
{
  GtkTextChildAnchor *anchor;
  
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  anchor = gtk_text_btree_create_child_anchor (iter);

  /* FIXME child-anchor-specific signal */
  
  gtk_signal_emit (GTK_OBJECT (buffer), signals[CHANGED]);

  return anchor;
}


/*
 * Mark manipulation
 */

static void
gtk_text_buffer_mark_set (GtkTextBuffer     *buffer,
                          const GtkTextIter *location,
                          GtkTextMark       *mark)
{
  /* IMO this should NOT work like insert_text and delete_text,
     where the real action happens in the default handler.

     The reason is that the default handler would be _required_,
     i.e. the whole widget would start breaking and segfaulting
     if the default handler didn't get run. So you can't really
     override the default handler or stop the emission; that is,
     this signal is purely for notification, and not to allow users
     to modify the default behavior. */

  g_object_ref (G_OBJECT (mark));

  gtk_signal_emit (GTK_OBJECT (buffer),
                   signals[MARK_SET],
                   location,
                   mark);

  g_object_unref (G_OBJECT (mark));
}

/**
 * gtk_text_buffer_set_mark:
 * @buffer:       a #GtkTextBuffer
 * @mark_name:    name of the mark
 * @iter:         location for the mark.
 * @left_gravity: if the mark is created by this function, gravity for the new
 *                mark.
 * @should_exist: if %TRUE, warn if the mark does not exist, and return
 *                immediately.
 *
 * Move the mark to the given position, if not @should_exist, create the mark.
 *
 * Return value: mark
 **/
static GtkTextMark*
gtk_text_buffer_set_mark (GtkTextBuffer *buffer,
                          GtkTextMark *existing_mark,
                          const gchar *mark_name,
                          const GtkTextIter *iter,
                          gboolean left_gravity,
                          gboolean should_exist)
{
  GtkTextIter location;
  GtkTextMark *mark;

  mark = gtk_text_btree_set_mark (get_btree (buffer),
                                  existing_mark,
                                  mark_name,
                                  left_gravity,
                                  iter,
                                  should_exist);

  if (gtk_text_btree_mark_is_insert (get_btree (buffer), mark) ||
      gtk_text_btree_mark_is_selection_bound (get_btree (buffer), mark))
    {
      gtk_text_buffer_update_primary_selection (buffer);
    }

  gtk_text_btree_get_iter_at_mark (get_btree (buffer),
                                   &location,
                                   mark);

  gtk_text_buffer_mark_set (buffer, &location, mark);

  return mark;
}

/**
 * gtk_text_buffer_create_mark:
 * @buffer: a #GtkTextBuffer
 * @mark_name: name for mark, or %NULL
 * @where: location to place mark
 * @left_gravity: whether the mark has left gravity
 *
 * Creates a mark at position @where. If @mark_name is %NULL, the mark
 * is anonymous; otherwise, the mark can be retrieved by name using
 * gtk_text_buffer_get_mark (). If a mark has left gravity, and text is
 * inserted at the mark's current location, the mark will be moved to
 * the left of the newly-inserted text. If the mark has right gravity
 * (@left_gravity = %FALSE), the mark will end up on the right of
 * newly-inserted text. The standard left-to-right cursor is a mark
 * with right gravity (when you type, the cursor stays on the right
 * side of the text you're typing).
 *
 * The caller of this function does <emphasis>not</emphasis> own a reference
 * to the returned #GtkTextMark, so you can ignore the return value
 * if you like. Marks are owned by the buffer and go away when the
 * buffer does.
 *
 * Emits the "mark_set" signal as notification of the mark's initial
 * placement.
 *
 * Return value: the new #GtkTextMark object
 **/
GtkTextMark*
gtk_text_buffer_create_mark (GtkTextBuffer *buffer,
                             const gchar *mark_name,
                             const GtkTextIter *where,
                             gboolean left_gravity)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return gtk_text_buffer_set_mark (buffer, NULL, mark_name, where,
                                   left_gravity, FALSE);
}

/**
 * gtk_text_buffer_move_mark:
 * @buffer: a #GtkTextBuffer
 * @mark: a #GtkTextMark
 * @where: new location for @mark in @buffer
 *
 * Moves @mark to the new location @where. Emits the "mark_set" signal
 * as notification of the move.
 **/
void
gtk_text_buffer_move_mark (GtkTextBuffer *buffer,
                           GtkTextMark *mark,
                           const GtkTextIter *where)
{
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (!gtk_text_mark_get_deleted (mark));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_set_mark (buffer, mark, NULL, where, FALSE, TRUE);
}

/**
 * gtk_text_buffer_get_iter_at_mark:
 * @buffer: a #GtkTextBuffer
 * @iter: iterator to initialize
 * @mark: a #GtkTextMark in @buffer
 *
 * Initializes @iter with the current position of @mark.
 **/
void
gtk_text_buffer_get_iter_at_mark (GtkTextBuffer *buffer,
                                  GtkTextIter *iter,
                                  GtkTextMark *mark)
{
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (!gtk_text_mark_get_deleted (mark));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_btree_get_iter_at_mark (get_btree (buffer),
                                   iter,
                                   mark);
}

/**
 * gtk_text_buffer_delete_mark:
 * @buffer: a #GtkTextBuffer
 * @mark: a #GtkTextMark in @buffer
 *
 * Deletes @mark, so that it's no longer located anywhere in the
 * buffer. Removes the reference the buffer holds to the mark, so if
 * you haven't called g_object_ref () on the mark, it will be freed. Even
 * if the mark isn't freed, most operations on @mark become
 * invalid. There is no way to undelete a
 * mark. gtk_text_mark_get_deleted () will return TRUE after this
 * function has been called on a mark; gtk_text_mark_get_deleted ()
 * indicates that a mark no longer belongs to a buffer. The "mark_deleted"
 * signal will be emitted as notification after the mark is deleted.
 **/
void
gtk_text_buffer_delete_mark (GtkTextBuffer *buffer,
                             GtkTextMark   *mark)
{
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (!gtk_text_mark_get_deleted (mark));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  g_object_ref (G_OBJECT (mark));

  gtk_text_btree_remove_mark (get_btree (buffer), mark);

  /* See rationale above for MARK_SET on why we emit this after
   * removing the mark, rather than removing the mark in a default
   * handler.
   */
  gtk_signal_emit (GTK_OBJECT (buffer), signals[MARK_DELETED],
                   mark);

  g_object_unref (G_OBJECT (mark));
}

/**
 * gtk_text_buffer_get_mark:
 * @buffer: a #GtkTextBuffer
 * @name: a mark name
 *
 * Returns the mark named @name in buffer @buffer, or NULL if no such
 * mark exists in the buffer.
 *
 * Return value: a #GtkTextMark, or NULL
 **/
GtkTextMark*
gtk_text_buffer_get_mark (GtkTextBuffer      *buffer,
                          const gchar         *name)
{
  GtkTextMark *mark;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  mark = gtk_text_btree_get_mark_by_name (get_btree (buffer), name);

  return mark;
}


/**
 * gtk_text_buffer_move_mark_by_name:
 * @buffer: a #GtkTextBuffer
 * @name: name of a mark
 * @where: new location for mark
 *
 * Moves the mark named @name (which must exist) to location @where.
 * See gtk_text_buffer_move_mark () for details.
 **/
void
gtk_text_buffer_move_mark_by_name (GtkTextBuffer     *buffer,
                                   const gchar       *name,
                                   const GtkTextIter *where)
{
  GtkTextMark *mark;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);

  mark = gtk_text_btree_get_mark_by_name (get_btree (buffer), name);

  if (mark == NULL)
    {
      g_warning ("%s: no mark named '%s'", G_STRLOC, name);
      return;
    }

  gtk_text_buffer_move_mark (buffer, mark, where);
}

/**
 * gtk_text_buffer_delete_mark_by_name:
 * @buffer: a #GtkTextBuffer
 * @name: name of a mark in @buffer
 *
 * Deletes the mark named @name; the mark must exist. See
 * gtk_text_buffer_delete_mark () for details.
 **/
void
gtk_text_buffer_delete_mark_by_name (GtkTextBuffer     *buffer,
                                     const gchar       *name)
{
  GtkTextMark *mark;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);

  mark = gtk_text_btree_get_mark_by_name (get_btree (buffer), name);

  if (mark == NULL)
    {
      g_warning ("%s: no mark named '%s'", G_STRLOC, name);
      return;
    }

  gtk_text_buffer_delete_mark (buffer, mark);
}

/**
 * gtk_text_buffer_get_insert:
 * @buffer: a #GtkTextBuffer
 *
 * Returns the mark that represents the cursor (insertion point).
 * Equivalent to calling gtk_text_buffer_get_mark () to get the mark
 * name "insert," but very slightly more efficient, and involves less
 * typing.
 *
 * Return value: insertion point mark
 **/
GtkTextMark*
gtk_text_buffer_get_insert (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  /* FIXME use struct member in btree */
  return gtk_text_buffer_get_mark (buffer, "insert");
}

/**
 * gtk_text_buffer_get_selection_bound:
 * @buffer: a #GtkTextBuffer
 *
 * Returns the mark that represents the selection bound.  Equivalent
 * to calling gtk_text_buffer_get_mark () to get the mark name
 * "selection_bound," but very slightly more efficient, and involves
 * less typing.
 *
 * The currently-selected text in @buffer is the region between the
 * "selection_bound" and "insert" marks. If "selection_bound" and
 * "insert" are in the same place, then there is no current selection.
 * gtk_text_buffer_get_selection_bounds () is another convenient function
 * for handling the selection, if you just want to know whether there's a
 * selection and what its bounds are.
 *
 * Return value: selection bound mark
 **/
GtkTextMark*
gtk_text_buffer_get_selection_bound (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  /* FIXME use struct member in btree */
  return gtk_text_buffer_get_mark (buffer, "selection_bound");
}

void
gtk_text_buffer_get_iter_at_child_anchor (GtkTextBuffer      *buffer,
                                          GtkTextIter        *iter,
                                          GtkTextChildAnchor *anchor)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (!gtk_text_child_anchor_get_deleted (anchor));
  
  gtk_text_btree_get_iter_at_child_anchor (get_btree (buffer),
                                           iter,
                                           anchor);
}

/**
 * gtk_text_buffer_place_cursor:
 * @buffer: a #GtkTextBuffer
 * @where: where to put the cursor
 *
 * This function moves the "insert" and "selection_bound" marks
 * simultaneously.  If you move them to the same place in two steps
 * with gtk_text_buffer_move_mark (), you will temporarily select a
 * region in between their old and new locations, which can be pretty
 * inefficient since the temporarily-selected region will force stuff
 * to be recalculated. This function moves them as a unit, which can
 * be optimized.
 **/
void
gtk_text_buffer_place_cursor (GtkTextBuffer     *buffer,
                              const GtkTextIter *where)
{
  GtkTextIter real;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  real = *where;

  if (gtk_text_iter_is_last (&real))
    gtk_text_iter_prev_char (&real);

  gtk_text_btree_place_cursor (get_btree (buffer), &real);
  gtk_text_buffer_mark_set (buffer, &real,
                            gtk_text_buffer_get_mark (buffer,
                                                      "insert"));
  gtk_text_buffer_mark_set (buffer, &real,
                            gtk_text_buffer_get_mark (buffer,
                                                      "selection_bound"));
}

/*
 * Tags
 */

/**
 * gtk_text_buffer_create_tag:
 * @buffer: a #GtkTextBuffer
 * @tag_name: name of the new tag, or %NULL
 *
 * Creates a tag and adds it to the tag table for @buffer.
 * Equivalent to calling gtk_text_tag_new () and then adding the
 * tag to the buffer's tag table. The returned tag has its refcount
 * incremented, as if you'd called gtk_text_tag_new ().
 *
 * If @tag_name is %NULL, the tag is anonymous.
 *
 * Return value: a new tag
 **/
GtkTextTag*
gtk_text_buffer_create_tag (GtkTextBuffer *buffer,
                            const gchar *tag_name)
{
  GtkTextTag *tag;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  tag = gtk_text_tag_new (tag_name);

  gtk_text_tag_table_add (get_table (buffer), tag);

  return tag;
}

static void
gtk_text_buffer_real_apply_tag (GtkTextBuffer *buffer,
                                GtkTextTag *tag,
                                const GtkTextIter *start,
                                const GtkTextIter *end)
{
  gtk_text_btree_tag (start, end, tag, TRUE);
}

static void
gtk_text_buffer_real_remove_tag (GtkTextBuffer *buffer,
                                 GtkTextTag *tag,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  gtk_text_btree_tag (start, end, tag, FALSE);
}

static void
gtk_text_buffer_real_changed (GtkTextBuffer *buffer)
{
  gtk_text_buffer_set_modified (buffer, TRUE);
}

static void
gtk_text_buffer_emit_tag (GtkTextBuffer *buffer,
                          GtkTextTag *tag,
                          gboolean apply,
                          const GtkTextIter *start,
                          const GtkTextIter *end)
{
  GtkTextIter start_tmp = *start;
  GtkTextIter end_tmp = *end;

  g_return_if_fail (tag != NULL);

  gtk_text_iter_reorder (&start_tmp, &end_tmp);

  if (apply)
    gtk_signal_emit (GTK_OBJECT (buffer), signals[APPLY_TAG],
                     tag, &start_tmp, &end_tmp);
  else
    gtk_signal_emit (GTK_OBJECT (buffer), signals[REMOVE_TAG],
                     tag, &start_tmp, &end_tmp);
}


void
gtk_text_buffer_apply_tag (GtkTextBuffer *buffer,
                           GtkTextTag    *tag,
                           const GtkTextIter *start,
                           const GtkTextIter *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  gtk_text_buffer_emit_tag (buffer, tag, TRUE, start, end);
}

void
gtk_text_buffer_remove_tag (GtkTextBuffer *buffer,
                            GtkTextTag    *tag,
                            const GtkTextIter *start,
                            const GtkTextIter *end)

{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  gtk_text_buffer_emit_tag (buffer, tag, FALSE, start, end);
}


void
gtk_text_buffer_apply_tag_by_name (GtkTextBuffer *buffer,
                                   const gchar *name,
                                   const GtkTextIter *start,
                                   const GtkTextIter *end)
{
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  tag = gtk_text_tag_table_lookup (get_table (buffer),
                                   name);

  if (tag == NULL)
    {
      g_warning ("Unknown tag `%s'", name);
      return;
    }

  gtk_text_buffer_emit_tag (buffer, tag, TRUE, start, end);
}

void
gtk_text_buffer_remove_tag_by_name (GtkTextBuffer *buffer,
                                    const gchar *name,
                                    const GtkTextIter *start,
                                    const GtkTextIter *end)
{
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  tag = gtk_text_tag_table_lookup (get_table (buffer),
                                   name);

  if (tag == NULL)
    {
      g_warning ("Unknown tag `%s'", name);
      return;
    }

  gtk_text_buffer_emit_tag (buffer, tag, FALSE, start, end);
}


/*
 * Obtain various iterators
 */

void
gtk_text_buffer_get_iter_at_line_offset (GtkTextBuffer      *buffer,
                                         GtkTextIter        *iter,
                                         gint                line_number,
                                         gint                char_offset)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_btree_get_iter_at_line_char (get_btree (buffer),
                                        iter, line_number, char_offset);
}

void
gtk_text_buffer_get_iter_at_line_index  (GtkTextBuffer *buffer,
                                         GtkTextIter   *iter,
                                         gint           line_number,
                                         gint           byte_index)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_btree_get_iter_at_line_byte (get_btree (buffer),
                                        iter, line_number, byte_index);
}

void
gtk_text_buffer_get_iter_at_line    (GtkTextBuffer      *buffer,
                                     GtkTextIter        *iter,
                                     gint                line_number)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_line_offset (buffer, iter, line_number, 0);
}

void
gtk_text_buffer_get_iter_at_offset         (GtkTextBuffer      *buffer,
                                            GtkTextIter        *iter,
                                            gint                char_offset)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_btree_get_iter_at_char (get_btree (buffer), iter, char_offset);
}

void
gtk_text_buffer_get_last_iter         (GtkTextBuffer      *buffer,
                                       GtkTextIter        *iter)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_btree_get_last_iter (get_btree (buffer), iter);
}

void
gtk_text_buffer_get_bounds (GtkTextBuffer *buffer,
                            GtkTextIter   *start,
                            GtkTextIter   *end)
{
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_btree_get_iter_at_char (get_btree (buffer), start, 0);
  gtk_text_btree_get_last_iter (get_btree (buffer), end);
}

/*
 * Modified flag
 */

gboolean
gtk_text_buffer_modified (GtkTextBuffer      *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  return buffer->modified;
}

void
gtk_text_buffer_set_modified (GtkTextBuffer      *buffer,
                              gboolean             setting)
{
  gboolean fixed_setting;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  fixed_setting = setting != FALSE;

  if (buffer->modified == fixed_setting)
    return;
  else
    {
      buffer->modified = fixed_setting;
      gtk_signal_emit (GTK_OBJECT (buffer), signals[MODIFIED_CHANGED]);
    }
}


/*
 * Assorted other stuff
 */

gint
gtk_text_buffer_get_line_count (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), 0);

  return gtk_text_btree_line_count (get_btree (buffer));
}

gint
gtk_text_buffer_get_char_count (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), 0);

  return gtk_text_btree_char_count (get_btree (buffer));
}

/* Called when we lose the primary selection.
 */
static void
clipboard_clear_selection_cb (GtkClipboard *clipboard,
                              gpointer      data)
{
  /* Move selection_bound to the insertion point */
  GtkTextIter insert;
  GtkTextIter selection_bound;
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (data);

  gtk_text_buffer_get_iter_at_mark (buffer, &insert,
                                    gtk_text_buffer_get_mark (buffer, "insert"));
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound,
                                    gtk_text_buffer_get_mark (buffer, "selection_bound"));

  if (!gtk_text_iter_equal (&insert, &selection_bound))
    gtk_text_buffer_move_mark (buffer,
                               gtk_text_buffer_get_mark (buffer, "selection_bound"),
                               &insert);
}

/* Called when we have the primary selection and someone else wants our
 * data in order to paste it.
 */
static void
clipboard_get_selection_cb (GtkClipboard     *clipboard,
                            GtkSelectionData *selection_data,
                            guint             info,
                            gpointer          data)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (data);
  GtkTextIter start, end;

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      if (selection_data->target ==
          gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE))
        {
          /* Provide the address of the buffer; this will only be
           * used within-process
           */
          gtk_selection_data_set (selection_data,
                                  gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE),
                                  8, /* bytes */
                                  (void*)&buffer,
                                  sizeof (buffer));
        }
      else
        {
          gchar *str;
          
          str = gtk_text_iter_get_visible_text (&start, &end);
          gtk_selection_data_set_text (selection_data, str);
          g_free (str);
        }
    }
}

/* Provide cut/copied data */
static void
clipboard_get_contents_cb (GtkClipboard     *clipboard,
                           GtkSelectionData *selection_data,
                           guint             info,
                           gpointer          data)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (data);
  GtkTextBuffer *contents = buffer->clipboard_contents;

  if (selection_data->target ==
      gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE))
    {
      /* Provide the address of the clipboard buffer; this will only
       * be used within-process. OK to supply a NULL value for contents.
       */
      gtk_selection_data_set (selection_data,
                              gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE),
                              8, /* bytes */
                              (void*)&contents,
                              sizeof (contents));
    }
  else
    {
      /* Just provide text from the clipboard buffer */
      if (buffer->clipboard_contents)
        {
          gchar *str;
          GtkTextIter start, end;
          
          gtk_text_buffer_get_bounds (contents, &start, &end);
          /* strip off the trailing newline, it isn't part of the text that was cut */
          gtk_text_iter_prev_char (&end);
          
          str = gtk_text_iter_get_visible_text (&start, &end);
          gtk_selection_data_set_text (selection_data, str);
          g_free (str);
        }
      else
        {
          gtk_selection_data_set_text (selection_data, "");
          return;
        }
    }
}

static void
clipboard_clear_contents_cb (GtkClipboard *clipboard,
                             gpointer      data)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (data);

  if (buffer->clipboard_contents)
    {
      g_object_unref (G_OBJECT (buffer->clipboard_contents));
      buffer->clipboard_contents = NULL;
    }  
}

static void
get_paste_point (GtkTextBuffer *buffer,
                 GtkTextIter   *iter,
                 gboolean       clear_afterward)
{
  GtkTextIter insert_point;
  GtkTextMark *paste_point_override;

  paste_point_override = gtk_text_buffer_get_mark (buffer,
                                                   "gtk_paste_point_override");

  if (paste_point_override != NULL)
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert_point,
                                        paste_point_override);
      if (clear_afterward)
        gtk_text_buffer_delete_mark (buffer,
                                     gtk_text_buffer_get_mark (buffer,
                                                               "gtk_paste_point_override"));
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert_point,
                                        gtk_text_buffer_get_mark (buffer,
                                                                  "insert"));
    }

  *iter = insert_point;
}

static void
pre_paste_prep (ClipboardRequest *request_data,
                GtkTextIter      *insert_point)
{
  GtkTextBuffer *buffer = request_data->buffer;

  if (request_data->replace_selection)
    {
      GtkTextIter start, end;
      
      if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        {
          if (request_data->interactive)
            gtk_text_buffer_delete_interactive (buffer,
                                                &start,
                                                &end,
                                                request_data->default_editable);
          else
            gtk_text_buffer_delete (buffer, &start, &end);
        }
    }
      
  get_paste_point (buffer, insert_point, TRUE);
}

/* Called when we request a paste and receive the text data
 */
static void
clipboard_text_received (GtkClipboard *clipboard,
                         const gchar  *str,
                         gpointer      data)
{
  ClipboardRequest *request_data = data;
  GtkTextBuffer *buffer = request_data->buffer;

  if (str)
    {
      GtkTextIter insert_point;

      pre_paste_prep (request_data, &insert_point);
      
      if (request_data->interactive)
        gtk_text_buffer_insert_interactive (buffer, &insert_point,
                                            str, -1, request_data->default_editable);
      else
        gtk_text_buffer_insert (buffer, &insert_point,
                                str, -1);
    }

  g_object_unref (G_OBJECT (buffer));
  g_free (request_data);
}

static GtkTextBuffer*
selection_data_get_buffer (GtkSelectionData *selection_data,
                           ClipboardRequest *request_data)
{
  GdkWindow *owner;
  GtkTextBuffer *src_buffer = NULL;

  /* If we can get the owner, the selection is in-process */
  owner = gdk_selection_owner_get (selection_data->selection);

  if (owner == NULL)
    return NULL;
  
  if (selection_data->type != gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE))
    return NULL;

  if (selection_data->length != sizeof (src_buffer))
    return NULL;
          
  memcpy (&src_buffer, selection_data->data, sizeof (src_buffer));

  if (src_buffer == NULL)
    return NULL;
  
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (src_buffer), NULL);

  if (gtk_text_buffer_get_tag_table (src_buffer) !=
      gtk_text_buffer_get_tag_table (request_data->buffer))
    return NULL;
  
  return src_buffer;
}

static void
paste_from_buffer (ClipboardRequest    *request_data,
                   GtkTextBuffer       *src_buffer,
                   const GtkTextIter   *start,
                   const GtkTextIter   *end)
{
  GtkTextIter insert_point;
  
  /* We're about to emit a bunch of signals, so be safe */
  g_object_ref (G_OBJECT (src_buffer));
  
  pre_paste_prep (request_data, &insert_point);
  
  if (!gtk_text_iter_equal (start, end))
    {
      gtk_text_buffer_real_insert_range (request_data->buffer,
                                         &insert_point,
                                         start,
                                         end,
                                         request_data->interactive);
    }
      
  g_object_unref (G_OBJECT (src_buffer));
}

static void
clipboard_clipboard_buffer_received (GtkClipboard     *clipboard,
                                     GtkSelectionData *selection_data,
                                     gpointer          data)
{
  ClipboardRequest *request_data = data;
  GtkTextBuffer *src_buffer;
  
  src_buffer = selection_data_get_buffer (selection_data, request_data); 

  if (src_buffer)
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_bounds (src_buffer, &start, &end);
      /* There's an extra newline on clipboard_contents */
      gtk_text_iter_prev_char (&end);
      
      paste_from_buffer (request_data, src_buffer,
                         &start, &end);
    }
  else
    {
      /* Request the text selection instead */
      gtk_clipboard_request_text (clipboard,
                                  clipboard_text_received,
                                  data);
    }
}

static void
clipboard_selection_buffer_received (GtkClipboard     *clipboard,
                                     GtkSelectionData *selection_data,
                                     gpointer          data)
{
  ClipboardRequest *request_data = data;
  GtkTextBuffer *src_buffer;
  
  src_buffer = selection_data_get_buffer (selection_data, request_data); 

  if (src_buffer)
    {
      GtkTextIter start, end;

      if (gtk_text_buffer_get_selection_bounds (src_buffer, &start, &end))
        paste_from_buffer (request_data, src_buffer,
                           &start, &end);
    }
  else
    {
      /* Request the text selection instead */
      gtk_clipboard_request_text (clipboard,
                                  clipboard_text_received,
                                  data);
    }
}

static const GtkTargetEntry targets[] = {
  { "STRING", 0, TARGET_STRING },
  { "TEXT",   0, TARGET_TEXT },
  { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
  { "UTF8_STRING", 0, TARGET_UTF8_STRING },
  { "GTK_TEXT_BUFFER_CONTENTS", 0, TARGET_TEXT_BUFFER_CONTENTS }
};

static void
gtk_text_buffer_update_primary_selection (GtkTextBuffer *buffer)
{
  GtkTextIter start;
  GtkTextIter end;

  GtkClipboard *clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

  /* Determine whether we have a selection and adjust X selection
   * accordingly.
   */

  if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (buffer))
        gtk_clipboard_clear (clipboard);
    }
  else
    {
      /* Even if we already have the selection, we need to update our
       * timestamp.
       */
      if (!gtk_clipboard_set_with_owner (clipboard, targets, G_N_ELEMENTS (targets),
                                         clipboard_get_selection_cb,
                                         clipboard_clear_selection_cb,
                                         G_OBJECT (buffer)))
        clipboard_clear_selection_cb (clipboard, buffer);
    }
}

static void
paste (GtkTextBuffer *buffer,
       gboolean       is_clipboard,
       gboolean       interactive,
       gboolean       default_editable)
{
  ClipboardRequest *data = g_new (ClipboardRequest, 1);
  GtkTextIter paste_point;
  GtkTextIter start, end;
  
  data->buffer = buffer;
  g_object_ref (G_OBJECT (buffer));
  data->interactive = interactive;
  data->default_editable = default_editable;

  /* When pasting with the cursor inside the selection area, you
   * replace the selection with the new text, otherwise, you
   * simply insert the new text at the point where the click
   * occured, unselecting any selected text. The replace_selection
   * flag toggles this behavior. FIXME set the flag based on something.
   */
  data->replace_selection = FALSE;
  
  get_paste_point (buffer, &paste_point, FALSE);
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end) &&
      gtk_text_iter_in_range (&paste_point, &start, &end))
    data->replace_selection = TRUE;

  if (is_clipboard)
    gtk_clipboard_request_contents (gtk_clipboard_get (GDK_NONE),
                                    
                                    gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE),
                                    clipboard_clipboard_buffer_received, data);
  else
    gtk_clipboard_request_contents (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
                                    
                                    gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE),
                                    clipboard_selection_buffer_received, data);    
}

void
gtk_text_buffer_paste_primary (GtkTextBuffer       *buffer,
                               const GtkTextIter   *override_location,
                               gboolean             default_editable)
{
  if (override_location != NULL)
    gtk_text_buffer_create_mark (buffer,
                                 "gtk_paste_point_override",
                                 override_location, FALSE);

  paste (buffer, FALSE, TRUE, default_editable);
}

void
gtk_text_buffer_paste_clipboard (GtkTextBuffer *buffer,
                                 gboolean       default_editable)
{
  paste (buffer, TRUE, TRUE, default_editable);
}

gboolean
gtk_text_buffer_delete_selection (GtkTextBuffer *buffer,
                                  gboolean interactive,
                                  gboolean default_editable)
{
  GtkTextIter start;
  GtkTextIter end;

  if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      return FALSE; /* No selection */
    }
  else
    {
      if (interactive)
        gtk_text_buffer_delete_interactive (buffer, &start, &end, default_editable);
      else
        gtk_text_buffer_delete (buffer, &start, &end);

      return TRUE; /* We deleted stuff */
    }
}

static void
cut_or_copy (GtkTextBuffer *buffer,
             gboolean delete_region_after,
             gboolean interactive,
             gboolean default_editable)
{
  /* We prefer to cut the selected region between selection_bound and
   * insertion point. If that region is empty, then we cut the region
   * between the "anchor" and the insertion point (this is for
   * C-space and M-w and other Emacs-style copy/yank keys). Note that
   * insert and selection_bound are guaranteed to exist, but the
   * anchor only exists sometimes.
   */
  GtkTextIter start;
  GtkTextIter end;

  if (buffer->clipboard_contents)
    {
      g_object_unref (G_OBJECT (buffer->clipboard_contents));
      buffer->clipboard_contents = NULL;
    }
  
  if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      /* Let's try the anchor thing */
      GtkTextMark * anchor = gtk_text_buffer_get_mark (buffer, "anchor");

      if (anchor == NULL)
        return;
      else
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &end, anchor);
          gtk_text_iter_reorder (&start, &end);
        }
    }

  if (!gtk_text_iter_equal (&start, &end))
    {
      GtkClipboard *clipboard = gtk_clipboard_get (GDK_NONE);
      GtkTextIter ins;
      
      buffer->clipboard_contents =
        gtk_text_buffer_new (gtk_text_buffer_get_tag_table (buffer));

      gtk_text_buffer_get_iter_at_offset (buffer->clipboard_contents,
                                          &ins, 0);
      
      gtk_text_buffer_insert_range (buffer->clipboard_contents,
                                    &ins,
                                    &start,
                                    &end);
                                    
      if (!gtk_clipboard_set_with_owner (clipboard, targets, G_N_ELEMENTS (targets),
                                         clipboard_get_contents_cb,
                                         clipboard_clear_contents_cb,
                                         G_OBJECT (buffer)))
        clipboard_clear_contents_cb (clipboard, buffer);      

      if (delete_region_after)
        {
          if (interactive)
            gtk_text_buffer_delete_interactive (buffer, &start, &end,
                                                default_editable);
          else
            gtk_text_buffer_delete (buffer, &start, &end);
        }
    }
}

void
gtk_text_buffer_cut_clipboard (GtkTextBuffer *buffer,
                               gboolean       default_editable)
{
  cut_or_copy (buffer, TRUE, TRUE, default_editable);
}

void
gtk_text_buffer_copy_clipboard (GtkTextBuffer *buffer)
{
  cut_or_copy (buffer, FALSE, TRUE, TRUE);
}


/**
 * gtk_text_buffer_get_selection_bounds:
 * @buffer: a #GtkTextBuffer
 * @start: iterator to initialize with selection start
 * @end: iterator to initialize with selection end
 *
 * Returns %TRUE if some text is selected; places the bounds
 * of the selection in @start and @end (if the selection has length 0,
 * then @start and @end are filled in with the same value).
 * @start and @end will be in ascending order. If @start and @end are
 * NULL, then they are not filled in, but the return value still indicates
 * whether text is selected.
 *
 * Return value: whether the selection has nonzero length
 **/
gboolean
gtk_text_buffer_get_selection_bounds   (GtkTextBuffer      *buffer,
                                        GtkTextIter        *start,
                                        GtkTextIter        *end)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  return gtk_text_btree_get_selection_bounds (get_btree (buffer), start, end);
}


/*
 * Debug spew
 */

void
_gtk_text_buffer_spew (GtkTextBuffer *buffer)
{
  gtk_text_btree_spew (get_btree (buffer));
}





