/* gtktextlayout.c - calculate the layout of the text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
 *
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 * 
 */

#include "gtktextlayout.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"

#include <stdlib.h>

/* DisplayLineList is our per-btree-line view-specific data.
   The DisplayLine object for the start of each btree line
   (counter.byte_index == 0) owns the DisplayLineList for
   that btree line. */
typedef struct _DisplayLineList DisplayLineList;

struct _DisplayLineList {
  /* These fields must be synced with GtkTextLineData in
     gtktextbtree.h, and are managed by the btree. */
  GtkTextLayout *layout; /* gpointer view_id */
  GtkTextLineData *next;
  /* these are synced, but managed by us */
  gint width;
  gint height;
  /* end of sync-requiring fields */
  
  GtkTextDisplayLine *lines;
};

static GtkTextDisplayChunk *gtk_text_view_display_chunk_new        (GtkTextDisplayChunkType  type);
static void                 gtk_text_view_display_chunk_destroy    (GtkTextLayout           *layout,
								    GtkTextDisplayChunk     *chunk);
static GtkTextDisplayLine * gtk_text_layout_find_display_line      (GtkTextLayout           *layout,
								    const GtkTextIter       *index);
static GtkTextDisplayLine * gtk_text_layout_find_display_line_at_y (GtkTextLayout           *layout,
								    gint                     y,
								    gint                    *first_line_y);
static DisplayLineList    * display_line_list_new                  (GtkTextLayout           *layout,
								    GtkTextLine             *line);
static void                 display_line_list_destroy              (DisplayLineList         *list);
static void                 display_line_list_create_lines         (DisplayLineList         *list,
								    GtkTextLine             *line,
								    GtkTextLayout           *layout);
static void                 display_line_list_delete_lines         (DisplayLineList         *list);


static guint count_bytes_that_fit (GdkFont *font,
                                  const gchar *str,
                                  gint len,
                                  int start_x, /* first pixel we can use */
                                  int end_x, /* can't use this pixel */
                                  int *end_pos); /* last pixel we did use */


static GtkTextLineData *gtk_text_layout_real_wrap (GtkTextLayout *layout,
                                                     GtkTextLine *line,
                                                     /* may be NULL */
                                                     GtkTextLineData *line_data);

static void gtk_text_layout_real_invalidate (GtkTextLayout *layout,
                                             const GtkTextIter *start,
                                             const GtkTextIter *end);

static void line_data_destructor (gpointer data);

static void gtk_text_layout_invalidate_all (GtkTextLayout *layout);

static gint utf8_text_width (GdkFont *font, const gchar *text, gint len);

enum {
  NEED_REPAINT,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

static void gtk_text_layout_init (GtkTextLayout *text_layout);
static void gtk_text_layout_class_init (GtkTextLayoutClass *klass);
static void gtk_text_layout_destroy (GtkObject *object);
static void gtk_text_layout_finalize (GtkObject *object);

void gtk_marshal_NONE__INT_INT_INT_INT (GtkObject  *object,
                                        GtkSignalFunc func,
                                        gpointer func_data,
                                        GtkArg  *args);

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_layout_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextLayout",
        sizeof (GtkTextLayout),
        sizeof (GtkTextLayoutClass),
        (GtkClassInitFunc) gtk_text_layout_class_init,
        (GtkObjectInitFunc) gtk_text_layout_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

    our_type = gtk_type_unique (GTK_TYPE_OBJECT, &our_info);
  }

  return our_type;
}

static void
gtk_text_layout_class_init (GtkTextLayoutClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  signals[NEED_REPAINT] =
    gtk_signal_new ("need_repaint",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkTextLayoutClass, need_repaint),
                    gtk_marshal_NONE__INT_INT_INT_INT,
                    GTK_TYPE_NONE,
                    4,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_text_layout_destroy;
  object_class->finalize = gtk_text_layout_finalize;

  klass->wrap = gtk_text_layout_real_wrap;
  klass->invalidate = gtk_text_layout_real_invalidate;
}

void
gtk_text_layout_init (GtkTextLayout *text_layout)
{

}

GtkTextLayout*
gtk_text_layout_new (void)
{
  return GTK_TEXT_LAYOUT (gtk_type_new (gtk_text_layout_get_type ()));
}

static void
free_style_cache (GtkTextLayout *text_layout)
{
  if (text_layout->one_style_cache)
    {
      gtk_text_view_style_values_unref (text_layout->one_style_cache);
      text_layout->one_style_cache = NULL;
    }
}

static void
gtk_text_layout_destroy (GtkObject *object)
{
  GtkTextLayout *layout;

  layout = GTK_TEXT_LAYOUT (object);

  gtk_text_layout_set_buffer (layout, NULL);  

  if (layout->default_style)
    gtk_text_view_style_values_unref (layout->default_style);
  layout->default_style = NULL;
  
  (* parent_class->destroy) (object);
}

static void
gtk_text_layout_finalize (GtkObject *object)
{
  GtkTextLayout *text_layout;

  text_layout = GTK_TEXT_LAYOUT (object);

  (* parent_class->finalize) (object);
}

void
gtk_text_layout_set_buffer (GtkTextLayout *layout,
                            GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_VIEW_BUFFER (buffer));
  
  if (layout->buffer == buffer)
    return;

  free_style_cache (layout);
  
  if (layout->buffer)
    {
      gtk_text_btree_remove_view (buffer->tree, layout);
      
      gtk_object_unref (GTK_OBJECT (layout->buffer));
      layout->buffer = NULL;
    }

  if (buffer)
    {
      layout->buffer = buffer;

      gtk_object_sink (GTK_OBJECT (buffer));
      gtk_object_ref (GTK_OBJECT (buffer));

      gtk_text_btree_add_view (buffer->tree, layout,
                               line_data_destructor);
    }
}

void
gtk_text_layout_default_style_changed (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  
  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_set_default_style (GtkTextLayout *layout,
                                   GtkTextStyleValues *values)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (values != NULL);

  if (values == layout->default_style)
    return;

  gtk_text_view_style_values_ref (values);
  
  if (layout->default_style)
    gtk_text_view_style_values_unref (layout->default_style);

  layout->default_style = values;

  gtk_text_layout_default_style_changed (layout);
}

void
gtk_text_layout_set_screen_width (GtkTextLayout *layout, gint width)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (width >= 0);
  g_return_if_fail (layout->wrap_loop_count == 0);
  
  if (layout->screen_width == width)
    return;
  
  layout->screen_width = width;
  
  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_get_size (GtkTextLayout *layout,
                          gint *width,
                          gint *height)
{
  gint w, h;
  
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  
  gtk_text_btree_get_view_size (layout->buffer->tree, layout,
                                &w, &h);

  layout->width = w;
  layout->height = h;
  
  if (width)
    *width = layout->width;

  if (height)
    *height = layout->height;
}

void
gtk_text_layout_need_repaint (GtkTextLayout *layout,
                              gint x, gint y,
                              gint width, gint height)
{
  gtk_signal_emit (GTK_OBJECT (layout), signals[NEED_REPAINT],
                  x, y, width, height);
}

void
gtk_text_layout_invalidate (GtkTextLayout *layout,
                            const GtkTextIter *start_index,
                            const GtkTextIter *end_index)
{
  (* GTK_TEXT_LAYOUT_CLASS (GTK_OBJECT (layout)->klass)->invalidate)
    (layout, start_index, end_index);
}

GtkTextLineData*
gtk_text_layout_wrap (GtkTextLayout *layout,
                       GtkTextLine *line,
                       /* may be NULL */
                       GtkTextLineData *line_data)
{
  return (* GTK_TEXT_LAYOUT_CLASS (GTK_OBJECT (layout)->klass)->wrap)
    (layout, line, line_data);
}

static GtkTextDisplayLine*
gtk_text_layout_find_display_line_at_y (GtkTextLayout *layout,
                                         gint y, gint *first_line_y)
{
  DisplayLineList *dline_list;
  GtkTextLine *line;
  GtkTextDisplayLine *iter;
  gint this_y = 0;
  
  dline_list = gtk_text_btree_find_line_data_by_y (layout->buffer->tree,
                                                   layout, y,
                                                   &this_y);

  if (dline_list == NULL)
    return NULL;
  
  if (first_line_y)
    *first_line_y = this_y;
  
  iter = dline_list->lines;
  line = iter->line;
  while (iter &&
         iter->line == line)
    {
      if (y < (this_y + iter->height))
        return iter;
      
      this_y += iter->height;
      iter = iter->next;
    }
  
  return NULL;
}

static GtkTextDisplayLine*
gtk_text_layout_find_display_line (GtkTextLayout     *layout,
				   const GtkTextIter *location)
{
  DisplayLineList *dline_list;
  GtkTextDisplayLine *iter;
  GtkTextLine *line;
  gint byte_index;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout), NULL);
  g_return_val_if_fail (location != NULL, NULL);

  line = gtk_text_iter_get_line (location);
  byte_index = gtk_text_iter_get_line_byte (location);
  
  dline_list = gtk_text_line_get_data (line, layout);

  g_assert (dline_list != NULL);

  /* Make sure we have the display lines computed */
  display_line_list_create_lines (dline_list, line, layout);
  
  iter = dline_list->lines;
  while (iter != NULL)
    {
      g_assert (iter->line == line); /* if fails, probably
                                                      an invalid byte_index
                                                      in the index */

      if (byte_index >= iter->byte_offset &&
          (iter->next == NULL ||
           byte_index < iter->next->byte_offset))
        return iter;
      else
        iter = iter->next;
    }

  g_assert_not_reached ();
  return NULL;
}

GSList*
gtk_text_layout_get_lines (GtkTextLayout *layout,
                           /* [top_y, bottom_y) */
                           gint top_y, 
                           gint bottom_y,
                           gint *first_line_y)
{
  GtkTextDisplayLine *first_line;
  GtkTextDisplayLine *last_line;
  GtkTextLine *first_btree_line;
  GtkTextLine *last_btree_line;
  GtkTextLine *line;
  GSList *retval;
  gint ignore;
  
  g_return_val_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout), NULL);
  g_return_val_if_fail (bottom_y > top_y, NULL);

  retval = NULL;
  
  first_line = gtk_text_layout_find_display_line_at_y (layout, top_y, first_line_y);
  if (first_line == NULL)
    {
      g_assert (top_y > 0);
      /* off the bottom */
      return NULL;
    }
  
  /* -1 since bottom_y is one past */
  last_line = gtk_text_layout_find_display_line_at_y (layout, bottom_y-1, NULL);
  
  first_btree_line = first_line->line;
  if (last_line)
    last_btree_line = last_line->line;
  else
    last_btree_line = gtk_text_btree_get_line (layout->buffer->tree,
                                               gtk_text_btree_line_count (layout->buffer->tree) - 1, &ignore);

  g_assert (last_btree_line != NULL);

  line = first_btree_line;
  while (TRUE)
    {
      DisplayLineList *list;
      GtkTextDisplayLine *iter;

      list = gtk_text_line_get_data (line, layout);

      g_assert (list != NULL);

      display_line_list_create_lines (list, line, layout);
      
      iter = list->lines;
      while (iter != NULL)
        {
          retval = g_slist_prepend (retval, iter);
          
          iter = iter->next;
        }

      if (line == last_btree_line)
        break;
      
      line = gtk_text_line_next (line);
    }
  
  retval = g_slist_reverse (retval);

  return retval;
}

static void
invalidate_cached_style (GtkTextLayout *layout)
{
  free_style_cache (layout);
}

/* These should be called around a loop which wraps a CONTIGUOUS bunch
   of display lines. If the lines aren't contiguous you can't call
   these. */
void
gtk_text_layout_wrap_loop_start (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (layout->one_style_cache == NULL);
  
  layout->wrap_loop_count += 1;
}

void
gtk_text_layout_wrap_loop_end (GtkTextLayout *layout)
{
  g_return_if_fail (layout->wrap_loop_count > 0);
  
  layout->wrap_loop_count -= 1;

  if (layout->wrap_loop_count == 0)
    {
      /* We cache a some stuff if we're iterating over some lines wrapping
         them. This cleans it up. */
      /* Nuke our cached style */
      invalidate_cached_style (layout);
      g_assert (layout->one_style_cache == NULL);
    }
}

static void
gtk_text_layout_invalidate_all (GtkTextLayout *layout)
{
  GtkTextIter start;
  GtkTextIter end;
  
  if (layout->buffer == NULL)
    return;

  gtk_text_buffer_get_bounds (layout->buffer, &start, &end);

  gtk_text_layout_invalidate (layout, &start, &end);
}

static void
gtk_text_layout_real_invalidate (GtkTextLayout *layout,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  DisplayLineList *dline_list;
  GtkTextLine *line;
  GtkTextLine *last_line;
  
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (layout->wrap_loop_count == 0);

#if 0
  gtk_text_view_index_spew (start_index, "invalidate start");
  gtk_text_view_index_spew (end_index, "invalidate end");
#endif
  
  /* If the end is at byte 0 of a line we don't actually need to nuke
     last_line, but for now we just nuke it anyway. */
  last_line = gtk_text_iter_get_line (end);
  
  line = gtk_text_iter_get_line (start);

  while (TRUE)
    {
      dline_list = gtk_text_line_get_data (line, layout);

      if (dline_list)
        {
          display_line_list_delete_lines (dline_list);
          g_assert (dline_list->lines == NULL);

          gtk_text_line_invalidate_wrap (line,
                                         (GtkTextLineData*)dline_list);
        }
      
      if (line == last_line)
        break;
      
      line = gtk_text_line_next (line);
    }
  
  /* FIXME yeah we could probably do a bit better here */
  gtk_text_layout_need_repaint (layout, 0, 0,
                                layout->width, layout->height);
}

static GtkTextLineData*
gtk_text_layout_real_wrap (GtkTextLayout *layout,
			   GtkTextLine *line,
                            /* may be NULL */
			   GtkTextLineData *line_data)
{
  DisplayLineList *list;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout), NULL);
  
  list = (DisplayLineList*)line_data;
  
  if (list == NULL)
    {
      list = display_line_list_new (layout, line);

      gtk_text_line_add_data (line,
                              (GtkTextLineData*)list);
    }

  display_line_list_create_lines (list, line, layout);

  /* FIXME can probably re-delete the lines immediately. */
  
  return (GtkTextLineData*)list;
}

/*
 * Layout utility functions
 */

/* If you get the style with get_style () you need to call
   release_style () to free it. */
static GtkTextStyleValues*
get_style (GtkTextLayout *layout,
          const GtkTextIter *iter)
{
  GtkTextTag** tags;
  gint tag_count = 0;
  GtkTextStyleValues *style;
  
  /* If we have the one-style cache, then it means
     that we haven't seen a toggle since we filled in the
     one-style cache.
  */
  if (layout->one_style_cache != NULL)
    {
      gtk_text_view_style_values_ref (layout->one_style_cache);
      return layout->one_style_cache;
    }

  g_assert (layout->one_style_cache == NULL);
  
  /* Get the tags at this spot */
  tags = gtk_text_btree_get_tags (iter,
                                  &tag_count);

  /* No tags, use default style */
  if (tags == NULL || tag_count == 0)
    {
      /* One ref for the return value, one ref for the
         layout->one_style_cache reference */
      gtk_text_view_style_values_ref (layout->default_style);
      gtk_text_view_style_values_ref (layout->default_style);
      layout->one_style_cache = layout->default_style;

      if (tags)
        g_free (tags);

      return layout->default_style;
    }
  
  /* Sort tags in ascending order of priority */
  gtk_text_tag_array_sort (tags, tag_count);
  
  style = gtk_text_view_style_values_new ();
  
  gtk_text_view_style_values_copy (layout->default_style,
                              style);

  gtk_text_view_style_values_fill_from_tags (style,
                                        tags,
                                        tag_count);

  g_free (tags);

  g_assert (style->refcount == 1);

  /* Leave this style as the last one seen */
  g_assert (layout->one_style_cache == NULL);
  gtk_text_view_style_values_ref (style); /* ref held by layout->one_style_cache */
  layout->one_style_cache = style;
  
  /* Returning yet another refcount */
  return style;
}

static void
release_style (GtkTextLayout *layout,
              GtkTextStyleValues *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);

  gtk_text_view_style_values_unref (style);
}

/*
 * Chunks
 */

static GtkTextDisplayChunk*
gtk_text_view_display_chunk_new (GtkTextDisplayChunkType type)
{
  GtkTextDisplayChunk *chunk;

  chunk = g_new0(GtkTextDisplayChunk, 1);

  chunk->type = type;

  return chunk;
}

static void
gtk_text_view_display_chunk_destroy (GtkTextLayout *layout,
                                GtkTextDisplayChunk *chunk)
{
#if 0
  /* FIXME we have to see what all these undisplay funcs did and
     emulate them here */
  if (chunk->undisplayFunc != NULL) {
    (*chunk->undisplayFunc)(layout, chunk);
  }
#endif

  release_style (layout, chunk->style);

  if (chunk->type == GTK_TEXT_DISPLAY_CHUNK_PIXMAP)
    {
      if (chunk->d.pixmap.pixmap)
        gdk_pixmap_unref (chunk->d.pixmap.pixmap);

      if (chunk->d.pixmap.mask)
        gdk_bitmap_unref (chunk->d.pixmap.mask);
    }
  
  g_free (chunk);
}

/*
 * Lines
 */

static GtkTextDisplayLine   *gtk_text_view_display_line_new     (GtkTextLine *line,
								 gint byte_offset);

static void                  gtk_text_view_display_line_destroy (GtkTextDisplayLine *line);


/* This function tries to optimize the case where a line
   is completely invisible */
static gboolean
totally_invisible_line (GtkTextLayout *layout,
                       GtkTextDisplayLine *line,
                       const GtkTextIter *iter)
{
  GtkTextLineSegment *seg;
  int bytes = 0;
  
  /* If we aren't at the start of the line, we aren't
     a totally invisible line */
  if (!gtk_text_iter_starts_line (iter))
    return FALSE;

  /* If we have a cached style, then we know it does actually apply
     and we can just see if it is elided. */
  if (layout->one_style_cache &&
      !layout->one_style_cache->elide)
    return FALSE;
  /* Without the cache, we check if the first char is visible, if so
     we are partially visible.  Note that we have to check this since
     we don't know the current elided/nonelided toggle state; this
     function can use the whole btree to get it right. */
  else if (!gtk_text_btree_char_is_invisible (iter))
    return FALSE;

  bytes = 0;
  seg = gtk_text_iter_get_line (iter)->segments;

  while (seg != NULL)
    {
      if (seg->byte_count > 0)
        bytes += seg->byte_count;

      /* Note that these two tests can cause us to bail out
         when we shouldn't, because a higher-priority tag
         may override these settings. However the important
         thing is to only elide really-elided lines, rather
         than to elide all really-elided lines. */
      
      else if (seg->type == &gtk_text_view_toggle_on_type)
        {
          invalidate_cached_style (layout);
          
          /* Bail out if an elision-unsetting tag begins */
          if (seg->body.toggle.info->tag->elide_set &&
              !seg->body.toggle.info->tag->values->elide)
            break;
        }
      else if (seg->type == &gtk_text_view_toggle_off_type)
        {
          invalidate_cached_style (layout);
          
          /* Bail out if an elision-setting tag ends */
          if (seg->body.toggle.info->tag->elide_set &&
              seg->body.toggle.info->tag->values->elide)
            break;
        }

      seg = seg->next;
    }

  if (seg != NULL)       /* didn't reach line end */
    return FALSE;
  else
    {
      line->byte_count = bytes;

      /* Leave height/length set to 0 */
      
      return TRUE;
    }
}

static void
chunk_self_check (GtkTextDisplayChunk *chunk)
{
  if (chunk->type == GTK_TEXT_DISPLAY_CHUNK_TEXT)
    {
      gint char_count = 0;
      gint byte_count = 0;

      while (byte_count < chunk->d.charinfo.byte_count)
        {
          GtkTextUniChar ch;
          
          byte_count += gtk_text_utf_to_unichar (chunk->d.charinfo.text + byte_count,
                                                 &ch);

          char_count += 1;
        }

      if (byte_count != chunk->d.charinfo.byte_count)
        {
          g_error ("Byte count for text display chunk incorrect");
        }
    }

  if (chunk->type == GTK_TEXT_DISPLAY_CHUNK_PIXMAP)
    {
      g_assert (chunk->byte_count == 2);
    }
}

typedef enum {
  GTK_TEXT_LAYOUT_OK,
  GTK_TEXT_LAYOUT_NOTHING_FITS
} GtkTextLayoutResult;

static GtkTextLayoutResult
layout_pixmap_segment (GtkTextLayout *layout,
                      GtkTextLineSegment *seg,
                      GtkTextDisplayLine *line,
                      GtkTextDisplayChunk *chunk,
                      gboolean seen_chars, /* seen some chars already */
                      gint offset, /* offset into the segment */
                      gint x,      /* first X position we can use */
                      gint max_x,  /* first X position that's off-limits,
                                      or -1 if none */
                      gint max_bytes) /* max chars we can put in the chunk */
{
  gint width, height;
  GdkPixmap *pixmap;
  
  g_return_val_if_fail (max_x < 0 || x < max_x,
                       GTK_TEXT_LAYOUT_NOTHING_FITS);
  g_assert (chunk->type == GTK_TEXT_DISPLAY_CHUNK_PIXMAP);  

  pixmap = seg->body.pixmap.pixmap;


  width = 0;
  height = 0;
  if (pixmap)
    {
      gdk_window_get_size (pixmap, &width, &height);
    }

  if ((seen_chars &&
       ((x + width) >= max_x)) ||
      max_bytes == 0)
    {
      return GTK_TEXT_LAYOUT_NOTHING_FITS;
    }
  
  /* Fill in the display chunk */

  chunk->byte_count = seg->byte_count;
  chunk->x = x;
  chunk->width = width;
  chunk->height = height;

  /* Shift the baseline if we have super/subscript.
     Note that this means ascent or descent can be negative,
     but their sum remains positive and constant.
     ( ascent + descent == height == ascent + offset + descent - offset )
  */
  chunk->ascent = height + chunk->style->offset;
  chunk->descent = - chunk->style->offset;

  chunk->d.pixmap.pixmap = seg->body.pixmap.pixmap;
  chunk->d.pixmap.mask = seg->body.pixmap.mask;

  if (chunk->d.pixmap.pixmap)
    gdk_pixmap_ref (chunk->d.pixmap.pixmap);

  if (chunk->d.pixmap.mask)
    gdk_bitmap_ref (chunk->d.pixmap.mask);
  
  return GTK_TEXT_LAYOUT_OK;
}

static GtkTextLayoutResult
layout_char_segment (GtkTextLayout *layout,
                    GtkTextLineSegment *seg,
                    GtkTextDisplayLine *line,
                    GtkTextDisplayChunk *chunk,
                    gboolean seen_chars, /* seen some chars already */
                    gint offset, /* offset into the segment */
                    gint x,      /* first X position we can use */
                    gint max_x,  /* first X position that's off-limits,
                                    or -1 if none */
                    gint max_bytes) /* max chars we can put in the chunk */
{
  const gchar *p;
  GdkFont *font;
  guint bytes_that_fit;
  gint end_x = 0;

  g_return_val_if_fail (max_x < 0 || x < max_x,
                       GTK_TEXT_LAYOUT_NOTHING_FITS);
  g_assert (chunk->type == GTK_TEXT_DISPLAY_CHUNK_TEXT);
  
  /* Figure out how many characters we can fit.

     We include partial characters in some cases:
      a) If the line has no chars yet, i.e. all lines must have at
         least one char
      b) trailing whitespace at the end of the line can be chopped
         off if at least one pixel is left to put it in

     Also, newlines take up no space so always get included
     at the end of lines.
  */

  font = chunk->style->font;
  p = seg->body.chars + offset;
  
  bytes_that_fit = count_bytes_that_fit (font, p, max_bytes,
                                        x, max_x, &end_x);


  g_assert (bytes_that_fit <= max_bytes);
  
  if (bytes_that_fit < max_bytes)
    {
      g_assert (max_x >= 0); /* we had a limit on space */
      
      if (bytes_that_fit == 0 &&
          !seen_chars)
        {
          /*  Partial character case a) */
          GtkTextUniChar ch;

          bytes_that_fit = gtk_text_utf_to_unichar (p, &ch);
          
          end_x = x + utf8_text_width (font, p, bytes_that_fit);
        }
      else if ( (end_x < (max_x - 1) ) &&
                (p[bytes_that_fit] == ' ' ||
                 p[bytes_that_fit] == '\t') )
        {
          /*  Partial character case b) */
          bytes_that_fit += 1;
          end_x = max_x - 1; /* take up remaining space */
        }

      /* grab one trailing newline */
      if (p[bytes_that_fit] == '\n')
        bytes_that_fit += 1;

      if (bytes_that_fit == 0)
        return GTK_TEXT_LAYOUT_NOTHING_FITS;
    }

  g_assert (end_x >= x);
  
  /* Fill in the display chunk */

  chunk->byte_count = bytes_that_fit;
  chunk->x = x;
  chunk->width = end_x - x;
  /* Shift the baseline if we have super/subscript.
     Note that this means ascent or descent can be negative,
     but their sum remains positive and constant.
     ( ascent + descent == height == ascent + offset + descent - offset )
  */
  chunk->ascent = font->ascent + chunk->style->offset;
  chunk->descent = font->descent - chunk->style->offset;

  /*
    The Tk widget copies the character data out of the btree
    here. I'm going to try not copying it and see if I get bitten,
    just to chill out the RAM a little bit.
  */
  
  /*
    FIXME if we do end up copying this, note that we should be
    able to demand-copy it only for the stuff that's actually on-screen,
    since we already have the layout information.
  */

  chunk->d.charinfo.byte_count = bytes_that_fit;
  chunk->d.charinfo.text = p;

  g_assert (gtk_text_byte_begins_utf8_char (p));
  
  /* we don't want to draw the newline, remember this is
     text for display */
  if (p[bytes_that_fit] == '\n')
    chunk->d.charinfo.byte_count -= 1;

  /* FIXME do the line break stuff */

#if 0
  chunk_self_check (chunk); /* DEBUG only - FIXME */
#endif
  
  return GTK_TEXT_LAYOUT_OK;
}

static void
merge_adjacent_elided_chunks (GtkTextLayout *layout,
			      GtkTextDisplayLineWrapInfo *wrapinfo)
{
  GtkTextDisplayChunk *iter;
  GtkTextDisplayChunk *prev;

  prev = wrapinfo->chunks;
  iter = wrapinfo->chunks->next;

  g_assert (prev != NULL);

  while (iter != NULL)
    {
      if (prev->type == GTK_TEXT_DISPLAY_CHUNK_TEXT &&
          iter->type == GTK_TEXT_DISPLAY_CHUNK_TEXT && 
          prev->style->elide &&
          iter->style->elide)
        {
          prev->byte_count += iter->byte_count;
          prev->next = iter->next;
          
          gtk_text_view_display_chunk_destroy (layout, iter);

          iter = prev->next;
        }
      else
        {
          prev = iter;
          iter = iter->next;
        }
    }
}

static void
get_margins (GtkTextLayout      *layout,
	     GtkTextStyleValues *style,
	     const GtkTextIter  *iter,
	     gint               *left,
	     gint               *right)
{

  if (left)
    {
      if (gtk_text_iter_starts_line (iter)) /* start of line */
        *left = style->left_margin;
      else
        *left = style->left_wrapped_line_margin;
    }

  if (right)
    {
      g_assert (left);
      
      if (style->wrap_mode == GTK_WRAPMODE_NONE)
        *right = -1; /* no max X pixel */
      else
        {
          /* Remember that the right margin pixel should be the first
             off-limits one, so add 1 to these */
          
          *right = layout->screen_width - style->right_margin + 1;
          
          /* Ensure the display width isn't negative */
          if (*right <= *left)
            *right = *left + 1;
        }
    }
}

/* FIXME the loop in here can likely get a little simpler by taking
   advantage of the new iterator instead of looping over segments
   manually */
GtkTextDisplayLineWrapInfo*
gtk_text_view_display_line_wrap (GtkTextLayout      *layout,
				 GtkTextDisplayLine *line)
{
  GtkTextDisplayLineWrapInfo *wrapinfo;
  GtkTextLineSegment *seg;
  GtkTextDisplayChunk *last_chunk;
  gint x = 0;
  gint max_x = 1; /* this is an off-limits pixel, so 1 is the minimum */
  gboolean seen_chars = FALSE;
  gboolean got_margins = FALSE;
  gint max_bytes;
  gboolean fit_whole_line;
  gint segment_offset = 0;
  GtkTextIter iter;
  
  g_return_val_if_fail (line != NULL, NULL);
  
  wrapinfo = g_new0(GtkTextDisplayLineWrapInfo, 1);
  
  /* Reset the line variables that are computed
     from the wrap */
  line->byte_count = 0;
  line->height = 0;
  line->length = 0;

  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                   &iter, line->line, line->byte_offset);
  
  /*
   * Special-case optimization for completely
   * invisible lines; makes it faster to deal
   * with sequences of invisible lines.
   */
  if (totally_invisible_line (layout, line, &iter))
    return wrapinfo;

  /* Iterate over segments, creating display chunks for them. */
  seg = gtk_text_iter_get_any_segment (&iter);
  segment_offset = gtk_text_iter_get_segment_byte (&iter);
  last_chunk = NULL;
  
  while (seg != NULL)
    {
      GtkTextDisplayChunk *chunk = NULL;

      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                       &iter, line->line,
                                       line->byte_offset + line->byte_count);
      
      /* Character segments */
      if (seg->type == &gtk_text_view_char_type)
        {
          if (seen_chars && (max_x >= 0 && x >= max_x))
            {
              /* There is no way we can fit on this line,
                 there's no space left. */
              
              goto done_with_line;
            }
          
          chunk =
            gtk_text_view_display_chunk_new (GTK_TEXT_DISPLAY_CHUNK_TEXT);

          if (wrapinfo->chunks == NULL) /* We are the first chunk */
            wrapinfo->chunks = chunk;

          if (last_chunk)          /* Link to the previous chunk */
            last_chunk->next = chunk;

          chunk->style = get_style (layout, &iter);
          
          /* First see if the chunk is elided, and ignore it if so. Tk
             looked at tabs, wrap mode, etc. before doing this, but
             that made no sense to me, so I am just skipping the
             elided chunks */
          if (chunk->style->elide)
            {
              line->byte_count += seg->byte_count - segment_offset;
              
              goto finished_with_segment;
            }
          else
            {
              GtkTextLayoutResult result;

              if (!got_margins)
                {
                  get_margins (layout, chunk->style, &iter,
                              &x, &max_x);
                  got_margins = TRUE;
                }

              g_assert (max_x < 0 || max_x <= layout->screen_width+1);
              
              max_bytes = seg->byte_count - segment_offset;
              
              result = layout_char_segment (layout, seg, line, chunk,
                                           seen_chars,
                                           segment_offset, x, max_x, max_bytes);
              
              if (result == GTK_TEXT_LAYOUT_OK)
                {
                  line->byte_count += chunk->byte_count;
                  seen_chars = TRUE;                  
                  goto finished_with_segment;
                }
              else if (result == GTK_TEXT_LAYOUT_NOTHING_FITS)
                {
                  /* The char layout function is guaranteed to put
                     at least one char on the line if the line has no
                     chars yet */
                  g_assert (wrapinfo->chunks != NULL &&
                           wrapinfo->chunks != chunk);
                  g_assert (seen_chars);
                  
                  /* Nothing more on this display line,
                     nuke our tentative chunk */
                  if (last_chunk)
                    last_chunk->next = NULL;
                  if (wrapinfo->chunks == chunk) /* This should never happen though */
                    wrapinfo->chunks = NULL;
                  gtk_text_view_display_chunk_destroy (layout, chunk);
                  chunk = NULL;

                  goto done_with_line;
                }
              else
                g_assert_not_reached ();
            } /* if (non-elided character segment) */
          
        } /* if (character segment) */

      /* Pixmaps */


      /* Some cut-and-paste between here and char segment, but
         very inconvenient to put in a function; need to think
         of a plan. */
      else if (seg->type == &gtk_text_pixmap_type)
        {
          if (seen_chars && (max_x >= 0 && x >= max_x))
            {
              /* There is no way we can fit on this line,
                 there's no space left. */
              
              goto done_with_line;
            }
          
          chunk =
            gtk_text_view_display_chunk_new (GTK_TEXT_DISPLAY_CHUNK_PIXMAP);

          if (wrapinfo->chunks == NULL) /* We are the first chunk */
            wrapinfo->chunks = chunk;

          if (last_chunk)          /* Link to the previous chunk */
            last_chunk->next = chunk;

          chunk->style = get_style (layout, &iter);
          
          /* First see if the chunk is elided, and ignore it if so. Tk
             looked at tabs, wrap mode, etc. before doing this, but
             that made no sense to me, so I am just skipping the
             elided chunks */
          if (chunk->style->elide)
            {
              line->byte_count += seg->byte_count - segment_offset;
              
              goto finished_with_segment;
            }
          else
            {
              GtkTextLayoutResult result;

              if (!got_margins)
                {
                  get_margins (layout, chunk->style, &iter,
                              &x, &max_x);
                  got_margins = TRUE;
                }
              
              g_assert (max_x < 0 || max_x <= layout->screen_width+1);
              
              max_bytes = seg->byte_count - segment_offset;
              
              result = layout_pixmap_segment (layout, seg, line, chunk,
                                             seen_chars,
                                             segment_offset, x, max_x, max_bytes);
              
              if (result == GTK_TEXT_LAYOUT_OK)
                {
                  line->byte_count += chunk->byte_count;
                  goto finished_with_segment;
                }
              else if (result == GTK_TEXT_LAYOUT_NOTHING_FITS)
                {                  
                  /* Nothing more on this display line,
                     nuke our tentative chunk */
                  if (last_chunk)
                    last_chunk->next = NULL;
                  if (wrapinfo->chunks == chunk) /* This should never happen though */
                    wrapinfo->chunks = NULL;
                  gtk_text_view_display_chunk_destroy (layout, chunk);
                  chunk = NULL;

                  goto done_with_line;
                }
              else
                g_assert_not_reached ();
            } /* if pixmap not elided */
        } /* end of if (pixmap segment) */
      
      
      /* Toggles */


      else if (seg->type == &gtk_text_view_toggle_on_type ||
               seg->type == &gtk_text_view_toggle_off_type)
        {
          /* Style may have changed, drop our
             current cached style */
          invalidate_cached_style (layout);
          
          /* semi-bogus temporary hack */
          line->byte_count += seg->byte_count - segment_offset;
          segment_offset += seg->byte_count - segment_offset;
          /* end semi-bogus temporary hack */
          
          goto finished_with_segment;
        } /* if (toggle segment) */
      
      
      /* Marks */

      
      else if (seg->type == &gtk_text_view_right_mark_type ||
               seg->type == &gtk_text_view_left_mark_type)
        {
          /* Display visible marks */

          if (seg->body.mark.visible)
            {
              chunk =
                gtk_text_view_display_chunk_new (GTK_TEXT_DISPLAY_CHUNK_CURSOR);
              
              if (wrapinfo->chunks == NULL) /* We are the first chunk */
                wrapinfo->chunks = chunk;

              if (last_chunk)          /* Link to the previous chunk */
                last_chunk->next = chunk;
              
              chunk->style = get_style (layout, &iter);

              line->byte_count += chunk->byte_count;

              if (got_margins)
                {
                  chunk->x = x;
                }
              else
                {
                  /* get x from our left margin, since there are
                     no char segments yet to use the margin from.
                     This is probably wrong; probably we want
                     to just fill in our X after we see a char
                     segment so we snap to the front of the
                     first char segment. */
                  get_margins (layout, chunk->style, &iter,
                              &chunk->x, NULL);
                }
              
              chunk->width = 0;
              chunk->ascent = chunk->style->font->ascent;
              chunk->descent = chunk->style->font->descent;
            }
          else
            {
              /* semi-bogus temporary hack */
              line->byte_count += seg->byte_count - segment_offset;
              segment_offset += seg->byte_count - segment_offset;
              /* end semi-bogus temporary hack */
            }
          
          goto finished_with_segment;
        } /* if (mark segment) */

      else
        g_error ("Unknown segment type: %s", seg->type->name);

    finished_with_segment:
      if (chunk)
        {
          /* We added a chunk based on this segment */
          last_chunk = chunk;
          segment_offset += chunk->byte_count;
          x += chunk->width;
        }

      /* Move to the next segment if we finished this segment */
      if (segment_offset >= seg->byte_count)
        {
          seg = seg->next;
          segment_offset = 0;
        }
    }

 done_with_line:

  g_assert (seen_chars); /* Each line should at least have a single newline in it */
  g_assert (last_chunk != NULL); /* have at least one chunk */

  fit_whole_line = (seg == NULL);
  
  merge_adjacent_elided_chunks (layout, wrapinfo);

  /* Now we need to calculate the attributes of the
     line as a whole. This could probably stand
     to be a separate function. */
  {
    gint justify_indent = 0;
    gint max_ascent = 0;
    gint max_descent = 0;
    gint max_height = 0;
    GtkTextDisplayChunk *iter;
    
    line->length = last_chunk->x + last_chunk->width;
    
    switch (wrapinfo->chunks->style->justify)
      {
      case GTK_JUSTIFY_LEFT:
        justify_indent = 0;
        break;
      case GTK_JUSTIFY_RIGHT:
        /* -1 since max_x isn't a valid pixel to use */
        justify_indent = max_x - line->length - 1;
        break;
      case GTK_JUSTIFY_CENTER:
        justify_indent = (max_x - line->length - 1) / 2;
        break;
      case GTK_JUSTIFY_FILL:
        g_warning ("FIXME we don't support GTK_JUSTIFY_FILL yet");
        break;
      default:
        g_assert_not_reached ();
        break;
      }

    iter = wrapinfo->chunks;
    while (iter != NULL)
      {
        /* move all the chunks over */
        iter->x += justify_indent;

        /* Compute some maximums */
        max_ascent = MAX (max_ascent, iter->ascent);
        max_descent = MAX (max_descent, iter->descent);
        max_height = MAX (max_height, iter->height);

        iter = iter->next;
      }

    /* Justification may have changed this value */
    line->length = last_chunk->x + last_chunk->width;

    if (max_height < (max_ascent + max_descent))
      {
        /* All the non-text segments were shorter than the text;
           increase the total height to fit the text */
        max_height = max_ascent + max_descent;
        wrapinfo->baseline = max_ascent;
      }
    else
      {
        /* Some non-text segments were taller; center the text
           baseline in the total height */
        wrapinfo->baseline = max_ascent + (max_height - (max_ascent + max_descent))/2;
      }

    line->height = max_height;

    /* Spacing above/below the line */
    
    if (line->byte_offset == 0)
      wrapinfo->space_above = wrapinfo->chunks->style->pixels_above_lines;
    else
      wrapinfo->space_above =
        wrapinfo->chunks->style->pixels_inside_wrap / 2 +
        wrapinfo->chunks->style->pixels_inside_wrap % 2; /* put remainder here */

    if (fit_whole_line)
      wrapinfo->space_below = wrapinfo->chunks->style->pixels_below_lines;
    else
      wrapinfo->space_below =
        wrapinfo->chunks->style->pixels_inside_wrap/2; /* could have put
                                                      remainder here */


    /* Consider spacing in the total line height, and the offset of
       the baseline from the top of the line. */
    line->height += wrapinfo->space_below + wrapinfo->space_above;
    wrapinfo->baseline += wrapinfo->space_above;
  }

  /* Free this if we aren't in a loop */
  if (layout->wrap_loop_count == 0)
    {
      invalidate_cached_style (layout);
    }
  
  return wrapinfo;
}

void
gtk_text_view_display_line_unwrap (GtkTextLayout              *layout,
				   GtkTextDisplayLine         *line,
				   GtkTextDisplayLineWrapInfo *wrapinfo)
{
  GtkTextDisplayChunk *chunk;
  GtkTextDisplayChunk *next;

  g_return_if_fail (line != NULL);
  g_return_if_fail (line->height >= 0);
  g_return_if_fail (line->length >= 0);
  g_return_if_fail (wrapinfo != NULL);
  
  for (chunk = wrapinfo->chunks; chunk != NULL; chunk = next)
    {
      next = chunk->next;
      gtk_text_view_display_chunk_destroy (layout, chunk);
    }

  g_free (wrapinfo);
}

static DisplayLineList*
display_line_list_new (GtkTextLayout *layout,
                      GtkTextLine *line)
{
  DisplayLineList *list;

  list = g_new (DisplayLineList, 1);

  list->layout = layout;
  list->next = NULL;
  list->width = -1;
  list->height = -1;

  list->lines = NULL;
  
  return list;
}

static void
line_data_destructor (gpointer data)
{
  display_line_list_destroy (data);
}

static void
display_line_list_destroy (DisplayLineList *list)
{
  g_return_if_fail (list != NULL);
  
  if (list->lines)
    display_line_list_delete_lines (list);

  g_assert (list->lines == NULL);
  
  g_free (list);
}

static void
display_line_list_create_lines (DisplayLineList *list,
                               GtkTextLine *line,
                               GtkTextLayout *layout)
{
  GtkTextDisplayLine *last;
  GtkTextDisplayLine *new_line;
  GtkTextDisplayLineWrapInfo *wrapinfo;
  GtkTextLineSegment *seg;
  gint byte;
  gint max_bytes;
  
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  
  if (list->lines != NULL)
    {
      g_return_if_fail (list->height >= 0);
      return;
    }

  list->width = 0;
  list->height = 0;
  
  max_bytes = 0;
  seg = line->segments;
  while (seg != NULL)
    {
      max_bytes += seg->byte_count;
      seg = seg->next;
    }

  gtk_text_layout_wrap_loop_start (layout);

  byte = 0;
  
  last = NULL;
  while (byte < max_bytes)
    {
      new_line = gtk_text_view_display_line_new (line, byte);
      
      g_assert (new_line != NULL);

      /* We need to wrap the line to
         fill in the height/length/byte_count fields */
      wrapinfo = gtk_text_view_display_line_wrap (layout, new_line);
      /* But we don't actually care about the display info,
         so just free it immediately */
      gtk_text_view_display_line_unwrap (layout, new_line, wrapinfo);

      list->height += new_line->height;
      list->width = MAX ( list->width, new_line->length);
      
      if (last)
        last->next = new_line;
      else
        list->lines = new_line;
      
      last = new_line;

      byte += new_line->byte_count;
    }

  gtk_text_layout_wrap_loop_end (layout);
}

static void
display_line_list_delete_lines (DisplayLineList *list)
{
  GtkTextDisplayLine *iter;
  GtkTextDisplayLine *next;

  iter = list->lines;
  while (iter != NULL)
    {
      next = iter->next;

      gtk_text_view_display_line_destroy (iter);
      
      iter = next;
    }
  
  list->lines = NULL;
}

/* Create a new layout line for the line starting at the given index. */   
static GtkTextDisplayLine*
gtk_text_view_display_line_new (GtkTextLine *btree_line, gint byte_offset)
{
  GtkTextDisplayLine *line;
  
  /* Init struct values to 0/NULL */
  line = g_new0(GtkTextDisplayLine, 1);

  line->line = btree_line;
  line->byte_offset = byte_offset;
  
  return line;
}

static void
gtk_text_view_display_line_destroy (GtkTextDisplayLine *line)
{
  g_free (line);
}

static gint
get_byte_at_x (GtkTextDisplayChunk *chunk, gint x)
{
  g_return_val_if_fail (x >= chunk->x, 0);
  g_return_val_if_fail (chunk->type == GTK_TEXT_DISPLAY_CHUNK_TEXT ||
                       chunk->type == GTK_TEXT_DISPLAY_CHUNK_PIXMAP, 0);
  
  switch (chunk->type)
    {
    case GTK_TEXT_DISPLAY_CHUNK_TEXT:
      {
        /* We want to "round down" i.e.
           we are trying to return the byte index
           where the cursor would be placed _before_
           the indexed character. */
        gint ignored;
        gint bytes;
        const gchar *text = chunk->d.charinfo.text;
        gint len = chunk->d.charinfo.byte_count;
        
        bytes = count_bytes_that_fit (chunk->style->font,
                                     text,
                                     len,
                                     chunk->x,
                                     x + 1,
                                     &ignored);

        /* Bytes has to be less than chunk->byte_count
           because if it were equal the X value
           would be into the next chunk */
        g_assert (bytes < chunk->byte_count);
        return bytes;
      }
      break;

    case GTK_TEXT_DISPLAY_CHUNK_PIXMAP:
      return 0;
      break;
      
    case GTK_TEXT_DISPLAY_CHUNK_CURSOR:
    default:
      g_assert_not_reached ();
      break;
    }
  g_assert_not_reached ();
  return 0;
}


static gint
get_x_at_byte (GtkTextDisplayChunk *chunk, gint offset)
{
  g_return_val_if_fail (chunk->type == GTK_TEXT_DISPLAY_CHUNK_TEXT, 0);
  g_return_val_if_fail (offset <= chunk->d.charinfo.byte_count, 0);
  
  switch (chunk->type)
    {
    case GTK_TEXT_DISPLAY_CHUNK_TEXT:
      {
        return chunk->x + utf8_text_width (chunk->style->font,
                                          chunk->d.charinfo.text,
                                          offset);
      }
      break;

    case GTK_TEXT_DISPLAY_CHUNK_PIXMAP:
    case GTK_TEXT_DISPLAY_CHUNK_CURSOR:
    default:
      g_assert_not_reached ();
      break;
    }
  g_assert_not_reached ();
  return 0;
}

/* FIXME the new iterators should allow a nice cleanup of this
   function too... */
void
gtk_text_layout_get_iter_at_pixel (GtkTextLayout *layout,
                                   GtkTextIter *target_iter,
                                   gint x, gint y)
{
  GtkTextIter counter;
  GtkTextDisplayLine *prev;
  GtkTextDisplayChunk *chunk;
  GtkTextDisplayLineWrapInfo *wrapinfo;
  gint ignore;
  gint byte_index;
  
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  
  /* Adjust pixels to be on-screen. This gives nice
     behavior if the user is dragging with a pointer grab.
  */
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x > layout->width)
    x = layout->width;
  if (y > layout->height)
    y = layout->height;

  prev = gtk_text_layout_find_display_line_at_y (layout, y, NULL);
  if (prev == NULL)
    {
      /* Use last line */
      gint last_line_index = gtk_text_btree_line_count (layout->buffer->tree) - 1;
      GtkTextLine *last_line = gtk_text_btree_get_line (layout->buffer->tree,
                                                         last_line_index, &ignore);
      DisplayLineList *display_lines;
      GtkTextDisplayLine *iter;
      g_assert (last_line);
      display_lines = gtk_text_line_get_data (last_line,
                                              layout);

      iter = display_lines->lines;
      while (iter != NULL)
        {
          prev = iter;
          iter = iter->next;
        }
      g_assert (prev != NULL);
    }
  
  wrapinfo = gtk_text_view_display_line_wrap (layout, prev);

  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                   &counter,
                                   prev->line, prev->byte_offset);
  byte_index = gtk_text_iter_get_line_byte (&counter);
  
  chunk = wrapinfo->chunks;
  g_assert (chunk != NULL); /* one chunk is required */
  while (x >= (chunk->x + chunk->width))
    {
      if (chunk->next == NULL)
        {
          /* We're off the end, just go to end of display line;
             but make sure we don't go off the end of the btree
             line. */
          byte_index += chunk->byte_count;
          {
            GtkTextLine *line;
            gint max_index = 0;

            line = gtk_text_iter_get_line (&counter);

            max_index = gtk_text_line_byte_count (line);
            
            if (byte_index >= max_index)
              {
                byte_index = 0;
                gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                                 &counter,
                                                 gtk_text_line_next (line),
                                                 byte_index);
              }
          }

          gtk_text_iter_backward_char (&counter);

#if 0
          gtk_text_view_counter_get_char (&counter); /* DEBUG only, FIXME */
#endif     
          gtk_text_view_display_line_unwrap (layout, prev, wrapinfo);

          *target_iter = counter;
          return;
        }
      byte_index += chunk->byte_count;
      chunk = chunk->next;
    }

  if (chunk->byte_count > 1)
    byte_index += get_byte_at_x (chunk, x);

  gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                   &counter,
                                   gtk_text_iter_get_line (&counter),
                                   byte_index);
  
#if 0
  gtk_text_view_counter_get_char (&counter); /* DEBUG only, FIXME */
#endif
  
  gtk_text_view_display_line_unwrap (layout, prev, wrapinfo);

  *target_iter = counter;
}

void
gtk_text_layout_get_iter_location (GtkTextLayout     *layout,
                                   const GtkTextIter *iter,
                                   GdkRectangle      *rect)
{
  DisplayLineList *dline_list;
  GtkTextDisplayChunk *chunk;
  GtkTextDisplayLineWrapInfo *wrapinfo;
  gint offset;
  gint chunk_offset;
  gint x;
  GtkTextDisplayLine *dline;
  GtkTextLine *line;
  GtkTextBTree *tree;
  gint byte_index;
  
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (gtk_text_iter_get_btree (iter) == layout->buffer->tree);
  g_return_if_fail (rect != NULL);
  
  tree = gtk_text_iter_get_btree (iter);
  line = gtk_text_iter_get_line (iter);
  byte_index = gtk_text_iter_get_line_byte (iter);
  
  rect->y = gtk_text_btree_find_line_top (tree,
                                          line,
                                          layout);

  dline_list = gtk_text_line_get_data (line,
                                       layout);
  if (dline_list == NULL)
    dline_list = (DisplayLineList*)gtk_text_layout_wrap (layout,
                                                         line, NULL);

  g_assert (dline_list != NULL);

  /* Make sure we have the display lines computed */
  display_line_list_create_lines (dline_list, line, layout);
  
  dline = dline_list->lines;
  while (dline != NULL)
    {
      if (byte_index >= dline->byte_offset &&
          (dline->next == NULL ||
           byte_index < dline->next->byte_offset))
        break;
      else
        {
          rect->y += dline->height;
          dline = dline->next;
        }
    }
  g_assert (dline != NULL);

  wrapinfo = gtk_text_view_display_line_wrap (layout, dline);

  offset = dline->byte_offset;
  chunk = wrapinfo->chunks;
  g_assert (chunk != NULL); /* one chunk is required */
  while (chunk != NULL)
    {
      if (byte_index >= offset &&
          (byte_index < (offset + chunk->byte_count)))
        break;
      
      offset += chunk->byte_count;
      chunk = chunk->next;
    }

  g_assert (chunk != NULL);

  chunk_offset = byte_index - offset;

  x = -1;
  if (chunk->type == GTK_TEXT_DISPLAY_CHUNK_TEXT)
    {
      rect->x = get_x_at_byte (chunk, chunk_offset);

      if (chunk_offset < chunk->byte_count)
        {
          /* Width is distance to the next character. */
          GtkTextUniChar ch;
          gint bytes;
          bytes = gtk_text_utf_to_unichar (chunk->d.charinfo.text + chunk_offset,
                                           &ch);
          x = get_x_at_byte (chunk, chunk_offset + bytes);
        }
    }
  else
    {
      rect->x = chunk->x;
    }

  if (x < 0)
    {
      /* Use distance to next chunk if any */
      if (chunk->next)
        x = chunk->next->x;
      else
        x = rect->x; /* no width, we're at the end of a line */
    }

  rect->width = x - rect->x;
  rect->height = dline->height;
  
  gtk_text_view_display_line_unwrap (layout, dline, wrapinfo);

#if 0
  printf ("iter at (%d,%d) %dx%d\n",
         rect->x, rect->y, rect->width, rect->height);
#endif
}

/* This one is clearly not unicode-friendly.

   Also the algorithm is stupid, it could be a lot smarter by assuming
   that bytes are roughly equal in width in order to do a clever
   binary search for the proper length (selecting the next length to
   try by assuming average an average glyph width in pixels)
*/
static guint
count_bytes_that_fit (GdkFont    *font,
                     const gchar *utf8_str,
                     gint         utf8_len,
                     int          start_x, /* first pixel we can use */
                     int          end_x, /* can't use this pixel, or -1 for no limit */
                     int         *end_pos) /* last pixel we did use */ 
{
  gint width;
  gint i;
  
  g_return_val_if_fail (end_x < 0 || end_x > start_x, 0);
  g_return_val_if_fail (utf8_str != NULL, 0);
  g_return_val_if_fail (font != NULL, 0);
  g_return_val_if_fail (utf8_len > 0, 0);
  g_return_val_if_fail (end_pos != NULL, 0);
  
  if (end_x < 0)
    {
      /* We can definitely fit them all */      
      width = utf8_text_width (font, utf8_str, utf8_len);
      *end_pos = start_x + width;
      
      return utf8_len;
    }

  width = 0;
  i = 0;
  while (i < utf8_len)
    {
      gint ch_w;
      guchar l1_char;
      gint bytes;
      
      bytes = gtk_text_utf_to_latin1_char (utf8_str + i, &l1_char);
      
      /* FIXME the final char in the string should have its rbearing
         used rather than the width, to avoid chopping off italics */
      ch_w = gdk_char_width (font, l1_char);
      
      if ( (start_x + width + ch_w) >= end_x )
        break;
      else
        {
          width += ch_w;
          i += bytes; /* note that i is incremented whenever a character fits,
                         so i is the number of bytes that fit. */
        }
    }

  g_assert (i <= utf8_len);
  
  *end_pos = start_x + width;
  
  return i;
}

static gint
utf8_text_width (GdkFont *font, const gchar *utf8_str, gint utf8_len)
{
  gchar *str;
  gint len;
  gint width;
  
  str = gtk_text_utf_to_latin1(utf8_str, utf8_len);
  len = strlen (str);

  width = gdk_text_width (font, str, len);
  
  g_free (str);

  return width;
}


typedef void (*GtkSignal_NONE__INT_INT_INT_INT) (GtkObject  *object,
                                                 gint x, gint y,
                                                 gint width, gint height,
                                                 gpointer user_data);

void
gtk_marshal_NONE__INT_INT_INT_INT (GtkObject  *object,
                                   GtkSignalFunc func,
                                   gpointer func_data,
                                   GtkArg  *args)
{
  GtkSignal_NONE__INT_INT_INT_INT rfunc;

  rfunc = (GtkSignal_NONE__INT_INT_INT_INT) func;
  (*rfunc) (object,
            GTK_VALUE_INT (args[0]),
            GTK_VALUE_INT (args[1]),
            GTK_VALUE_INT (args[2]),
            GTK_VALUE_INT (args[3]),
            func_data);
}

void
gtk_text_layout_spew (GtkTextLayout *layout)
{
#if 0
  GtkTextDisplayLine *iter;
  guint wrapped = 0;
  guint paragraphs = 0;
  GtkTextLine *last_line = NULL;
  
  iter = layout->line_list;
  while (iter != NULL)
    {
      if (iter->line != last_line)
        {
          printf ("%5u  paragraph (%p)\n", paragraphs, iter->line);
          ++paragraphs;
          last_line = iter->line;
        }
      
      printf ("  %5u  y: %d len: %d start: %d bytes: %d\n",
             wrapped, iter->y, iter->length, iter->byte_offset,
             iter->byte_count);

      ++wrapped;
      iter = iter->next;
    }

  printf ("Layout %s recompute\n",
         layout->need_recompute ? "needs" : "doesn't need");

  printf ("Layout pars: %u lines: %u size: %d x %d Screen width: %d\n",
         paragraphs, wrapped, layout->width,
         layout->height, layout->screen_width);
#endif
}

