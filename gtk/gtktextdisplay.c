/* gtktextdisplay.c - display layed-out text
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

#include "gtktextdisplay.h"
#include "gtktextiterprivate.h"

/*
 *
 * I consider this file to be Sucky (tm) so if you want to rewrite it...
 *
 *
 */


static void
set_gc_from_values (GdkGC* gc,
		    gboolean foreground,
		    GtkTextStyleValues *values)
{
  if (foreground)
    {
      gdk_gc_set_foreground (gc, &values->fg_color);

      if (values->fg_stipple)
        {
          gdk_gc_set_fill (gc, GDK_STIPPLED);
          gdk_gc_set_stipple (gc, values->fg_stipple);
        }
      else
        {
          gdk_gc_set_fill (gc, GDK_SOLID);
        }
    }
  else
    {
      gdk_gc_set_foreground (gc, &values->bg_color);

      if (values->bg_stipple)
        {
          gdk_gc_set_fill (gc, GDK_STIPPLED);
          gdk_gc_set_stipple (gc, values->bg_stipple);
        }
      else
        {
          gdk_gc_set_fill (gc, GDK_SOLID);
        }
    }
}

static void
draw_text (GdkDrawable *drawable,
	   GdkGC* gc,
	   gint x, 
	   gint baseline,
	   GtkTextDisplayChunk *chunk)
{
  const gchar *str = chunk->d.charinfo.text;
  guint bytes = chunk->d.charinfo.byte_count;
  gchar *latin1;

  latin1 = gtk_text_utf_to_latin1(str, bytes);
  
  gdk_draw_text (drawable,
		 chunk->style->font,
		 gc,
		 x,
		 baseline,
		 latin1,
		 strlen (latin1));

  g_free (latin1);

  if (chunk->style->underline)
    {
      gint where = baseline + 1;
      gdk_draw_line (drawable,
		     gc,
		     x, where,
		     x + chunk->width, where);
    }

  if (chunk->style->overstrike)
    {
      gint where = baseline - chunk->ascent/2;
      gdk_draw_line (drawable,
		     gc,
		     x, where,
		     x + chunk->width, where);
    }
}

static void
draw_chunk_background (GdkDrawable *drawable,
		       GdkGC* gc,
		       gint x, gint y,
		       gint width, gint height,
		       GtkTextDisplayChunk *chunk)
{
  gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);
  
  switch (chunk->type)
    {
    case GTK_TEXT_DISPLAY_CHUNK_TEXT:
      break;
    case GTK_TEXT_DISPLAY_CHUNK_CURSOR:
      break;
    case GTK_TEXT_DISPLAY_CHUNK_PIXMAP:
      break;
    default:
      break;
    }
}

static void
draw_chunk_foreground (GdkDrawable *drawable,
		       GdkGC* gc,
		       gint x, gint y, gint baseline,
		       GtkTextDisplayChunk *chunk)
{
  switch (chunk->type)
    {
    case GTK_TEXT_DISPLAY_CHUNK_TEXT:
      draw_text (drawable, gc, x, y + baseline, chunk);
      break;
    case GTK_TEXT_DISPLAY_CHUNK_CURSOR:
      gdk_draw_line (drawable, gc,
		     x, y + baseline - chunk->ascent,
		     x, y + baseline + chunk->descent);
      break;
    case GTK_TEXT_DISPLAY_CHUNK_PIXMAP:
      {
        gint ypos;

        /* We want the bottom of the pixmap on the baseline */
        ypos = y + baseline - chunk->ascent;
      
        if (chunk->d.pixmap.mask)
          {
            /* FIXME we are hosing the GC here, it is supposed
               to have a different clip */
            gdk_gc_set_clip_mask (gc, chunk->d.pixmap.mask);
            gdk_gc_set_clip_origin (gc, x, ypos);
          }
        
        gdk_draw_pixmap (drawable, gc,
			 chunk->d.pixmap.pixmap,
			 0, 0,
			 x,
			 ypos,
			 -1, -1);
        
        if (chunk->d.pixmap.mask)
          {
            gdk_gc_set_clip_mask (gc, NULL);
            gdk_gc_set_clip_origin (gc, 0, 0);
          }
      }
      break;
    default:
      break;
    }
}

/* do_chunk is the insides of the loop in the
   display function; it's here because we want
   to do the cursor chunk out-of-order so this
   avoids cut and paste or loop cruft */
static void
release_last_style (GtkTextStyleValues** last_style,
		    GtkWidget *widget)
{
  if (*last_style)
    {
      gtk_text_view_style_values_unrealize (*last_style,
					    gtk_widget_get_colormap (widget),
					    gtk_widget_get_visual (widget));
      gtk_text_view_style_values_unref (*last_style);
      *last_style = NULL;
    }
}

static void
do_chunk (GtkTextLayout *layout,
	  GtkTextDisplayChunk *chunk,
	  GtkWidget *widget,
	  GdkDrawable *drawable,
	  GtkTextDisplayLineWrapInfo *wrapinfo,
	  gboolean inside_selection,
	  GdkGC* fg_gc,
	  GdkGC* bg_gc,
	  gint line_y,
	  gint line_height,
	  gint x_offset,
	  GtkTextStyleValues** last_style,
	  GtkTextDisplayChunk** cursor_chunk)
{
  gint chunk_x;

  /* if cursor_chunk is NULL, then we are drawing the cursor;
     otherwise we are supposed to return the cursor */
  g_assert (cursor_chunk != NULL || chunk->type == GTK_TEXT_DISPLAY_CHUNK_CURSOR);
  if (cursor_chunk &&
      chunk->type == GTK_TEXT_DISPLAY_CHUNK_CURSOR)
    {
      *cursor_chunk = chunk;
      return;
    }
  
  chunk_x = chunk->x - x_offset;

  if (chunk->style != *last_style)
    {
      release_last_style (last_style, widget);
      
      if (inside_selection)
        {
          *last_style = gtk_text_view_style_values_new ();
          gtk_text_view_style_values_copy (chunk->style, *last_style);

          (*last_style)->fg_color = widget->style->fg[GTK_STATE_SELECTED];
          (*last_style)->bg_color = widget->style->bg[GTK_STATE_SELECTED];
          (*last_style)->bg_full_height = TRUE;
          (*last_style)->draw_bg = TRUE;
        }
      else
        {
          *last_style = chunk->style;
          gtk_text_view_style_values_ref (*last_style);
        }
      
      gtk_text_view_style_values_realize (*last_style,
					  gtk_widget_get_colormap (widget),
					  gtk_widget_get_visual (widget));
    }
          
  if ((*last_style)->draw_bg)
    {
      gint bg_height, bg_y;
      gint bg_width;
      
      set_gc_from_values (bg_gc,
			  FALSE,
			  *last_style);

      if ((*last_style)->bg_full_height)
        {
          bg_height = line_height;
          bg_y = line_y;
        }
      else
        {
          bg_height = chunk->ascent + chunk->descent;
          bg_y = line_y + wrapinfo->baseline - chunk->ascent;
        }

      bg_width = chunk->width;

      /* Draw blue selection background all the way
         to the end of the line */
      if (chunk->next == NULL && inside_selection)
        bg_width = layout->width - chunk_x;
      
      /* FIXME pass in last_style not chunk */
      draw_chunk_background (drawable,
			     bg_gc,
			     chunk_x,
			     bg_y,
			     bg_width,
			     bg_height,
			     chunk);
    }

  set_gc_from_values (fg_gc,
		      TRUE,
		      *last_style);

  /* FIXME pass in last_style not chunk */
  draw_chunk_foreground (drawable,
			 fg_gc,
			 chunk_x,
			 line_y,
			 wrapinfo->baseline - (*last_style)->offset,
			 chunk);
}

void
gtk_text_layout_draw (GtkTextLayout *layout,
		      GtkWidget *widget,
		      GdkDrawable *drawable,
		      /* Location of the buffer
			 in layout coordinates */
		      gint x_offset,
		      gint y_offset,
		      /* Region of the layout to
			 render */
		      gint x,
		      gint y,
		      gint width,
		      gint height)
{
  GtkTextStyleValues *last_style;
  GdkRectangle clip;
  GdkGC* fg_gc;
  GdkGC* bg_gc;
  gint buffer_x, buffer_y;
  gboolean inside_selection;
  GtkTextIter selection_start;
  GtkTextIter selection_end;
  GSList *line_list;
  GSList *list;
  GtkTextDisplayLine *start_line;
  gint current_y;
  gboolean have_selection = FALSE;
  
  g_return_if_fail (GTK_IS_TEXT_VIEW_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (x_offset >= 0);
  g_return_if_fail (y_offset >= 0);
  g_return_if_fail (x >= 0);
  g_return_if_fail (y >= 0);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  if (width == 0 ||
      height == 0)
    return;

  line_list =  gtk_text_layout_get_lines (layout,
					  y,
					  y + height + 1, /* one past what we draw */
					  &current_y);
  
  if (line_list == NULL)
    return; /* nothing on the screen */

  /* Selection-drawing is handled here rather than when we wrap the lines
     for several reasons:
     - it doesn't affect line wrapping or widget size
     - it requires information from widget->style
     - it is more efficient to deal with it here
     - this code is less complicated than the layout code and
     can better withstand funky special cases
     - this code sucks anyway so may as well get crufted :-)
  */

  start_line = line_list->data;

  inside_selection = FALSE;
  
  if (gtk_text_buffer_get_selection_bounds (layout->buffer,
                                            &selection_start,
                                            &selection_end))
    {
      /* See if first line is inside the selection. */
      GtkTextIter first_line;
      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                       &first_line,
                                       start_line->line,
                                       start_line->byte_offset);
      
      if (gtk_text_iter_compare (&first_line, &selection_start) >= 0)
        {
          if (gtk_text_iter_compare (&first_line, &selection_end) < 0)
            inside_selection = TRUE;
        }

      have_selection = TRUE;
    }
  
  /* Convert to buffer coordinates */
  buffer_x = x - x_offset;
  buffer_y = y - y_offset;

  /* Don't draw outside the buffer */
  if (buffer_x < 0)
    buffer_x = 0;
  if (buffer_y < 0)
    buffer_y = 0;
  
  /* This is slower than it needs to be,
     should store a GC on the widget and pass it
     in here. */
  fg_gc = gdk_gc_new (drawable);
  bg_gc = gdk_gc_new (drawable);

  clip.x = buffer_x;
  clip.y = buffer_y;
  clip.width = width;
  clip.height = height;

  gdk_gc_set_clip_rectangle (fg_gc, &clip);
  gdk_gc_set_clip_rectangle (bg_gc, &clip);

  gtk_text_layout_wrap_loop_start (layout);
  
  last_style = NULL;
  
  list = line_list;
  
  while (list != NULL)
    {
      GSList *cursor_list = NULL;
      GSList *cursors_selected_list = NULL;
      GSList *tmp1, *tmp2;
      GtkTextDisplayChunk *chunk;
      GtkTextDisplayChunk *cursor = NULL;
      GtkTextDisplayLineWrapInfo *wrapinfo;
      gint line_y;
      GtkTextDisplayLine *display_line;
      GtkTextIter iter;
      
      display_line = list->data;
      
      gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                       &iter,
                                       display_line->line,
                                       display_line->byte_offset);
      
      line_y = current_y - y_offset;

      wrapinfo = gtk_text_view_display_line_wrap (layout, display_line);
      
      chunk = wrapinfo->chunks;
      while (chunk != NULL)
        {
          if (have_selection)
            {
              /* We assume that the selection starts at a display
                 chunk boundary, since marks break up the char
                 segments in the btree - logically though, the checks
                 are for >= and <= rather than plain equal.
                 Just using equal for speed  */
              if (inside_selection &&
                  gtk_text_iter_equal (&iter,
                                       &selection_end))
                {
                  inside_selection = FALSE;
                  /* make sure we don't use a cached style */
                  release_last_style (&last_style, widget);
                }
              else if (!inside_selection &&
                       gtk_text_iter_equal (&iter,
                                            &selection_start))
                {
                  inside_selection = TRUE;
                  /* make sure we don't use a cached style */
                  release_last_style (&last_style, widget);
                }
            }

          do_chunk (layout, chunk, widget,
		    drawable, wrapinfo, inside_selection,                   
		    fg_gc, bg_gc, line_y, display_line->height,
		    x_offset, &last_style, &cursor);

          /* save cursors for later */
          if (cursor != NULL)
            {
              cursors_selected_list =
                g_slist_prepend (cursors_selected_list,
				 GINT_TO_POINTER (inside_selection));

              cursor_list = g_slist_prepend (cursor_list, cursor);
              cursor = NULL;
            }
              
          gtk_text_btree_get_iter_at_line (layout->buffer->tree,
                                           &iter,
                                           display_line->line,
                                           gtk_text_iter_get_line_byte (&iter) + chunk->byte_count);
          
          chunk = chunk->next;
        }

      /* We paint the cursors last, because they overlap another chunk
         and need to appear on top. */

      tmp1 = cursor_list;
      tmp2 = cursors_selected_list;
      while (tmp1 != NULL)
        {
          GtkTextDisplayChunk *cursor;
          gboolean cursor_inside_selection;

          g_assert (tmp2 != NULL);

          cursor = tmp1->data;
          cursor_inside_selection = GPOINTER_TO_INT (tmp2->data);

          do_chunk (layout, cursor, widget,
		    drawable, wrapinfo, cursor_inside_selection,
		    fg_gc, bg_gc, line_y, display_line->height,
		    x_offset, &last_style, NULL);
          
          tmp1 = g_slist_next (tmp1);
          tmp2 = g_slist_next (tmp2);
        }
      
      gtk_text_view_display_line_unwrap (layout, display_line, wrapinfo);

      release_last_style (&last_style, widget);

      current_y += display_line->height;
      
      list = g_slist_next (list);
    }

  gtk_text_layout_wrap_loop_end (layout);

  g_slist_free (line_list);
  
  gdk_gc_unref (fg_gc);
  gdk_gc_unref (bg_gc);
}
