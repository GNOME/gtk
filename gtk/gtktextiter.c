#include "gtktextiter.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"
#include "gtkdebug.h"
#include <ctype.h>

typedef struct _GtkTextRealIter GtkTextRealIter;

struct _GtkTextRealIter {
  /* Always-valid information */
  GtkTextBTree *tree;
  GtkTextLine *line;
  /* At least one of these is always valid;
     if invalid, they are -1.

     If the line byte offset is valid, so is the segment byte offset;
     and ditto for char offsets. */
  gint line_byte_offset;
  gint line_char_offset;
  /* These two are valid if >= 0 */
  gint cached_char_index;
  gint cached_line_number;
  /* Stamps to detect the buffer changing under us */
  gint chars_changed_stamp;
  gint segments_changed_stamp;
  /* Valid if the segments_changed_stamp is up-to-date */
  GtkTextLineSegment *segment;     /* indexable segment we index */
  GtkTextLineSegment *any_segment; /* first segment in our location,
                                   maybe same as "segment" */
  /* One of these will always be valid if segments_changed_stamp is
     up-to-date. If invalid, they are -1.

     If the line byte offset is valid, so is the segment byte offset;
     and ditto for char offsets. */
  gint segment_byte_offset;
  gint segment_char_offset;
  /* These are here for binary-compatible expansion space. */
  gpointer pad1;
  gint pad2;
};

/* These "set" functions should not assume any fields
   other than the char stamp and the tree are valid.
*/
static void
iter_set_common(GtkTextRealIter *iter,
                GtkTextLine *line)
{
  /* Update segments stamp */
  iter->segments_changed_stamp =
    gtk_text_btree_get_segments_changed_stamp(iter->tree);
      
  iter->line = line;

  iter->line_byte_offset = -1;
  iter->line_char_offset = -1;
  iter->segment_byte_offset = -1;
  iter->segment_char_offset = -1;
  iter->cached_char_index = -1;
  iter->cached_line_number = -1;
}

static void
iter_set_from_byte_offset(GtkTextRealIter *iter,
                          GtkTextLine *line,
                          gint byte_offset)
{
  iter_set_common(iter, line);

  gtk_text_line_byte_locate(iter->line,
                             byte_offset,
                             &iter->segment,
                             &iter->any_segment,
                             &iter->segment_byte_offset,
                             &iter->line_byte_offset);

}

static void
iter_set_from_char_offset(GtkTextRealIter *iter,
                          GtkTextLine *line,
                          gint char_offset)
{
  iter_set_common(iter, line);

  gtk_text_line_char_locate(iter->line,
                             char_offset,
                             &iter->segment,
                             &iter->any_segment,
                             &iter->segment_char_offset,
                             &iter->line_char_offset);
}

static void
iter_set_from_segment(GtkTextRealIter *iter,
                      GtkTextLine *line,
                      GtkTextLineSegment *segment)
{
  GtkTextLineSegment *seg;
  gint byte_offset;

  /* This could theoretically be optimized by computing all the iter
     fields in this same loop, but I'm skipping it for now. */
  byte_offset = 0;
  seg = line->segments;
  while (seg != segment)
    {
      byte_offset += seg->byte_count;
      seg = seg->next;
    }

  iter_set_from_byte_offset(iter, line, byte_offset);
}

/* This function ensures that the segment-dependent information is
   truly computed lazily; often we don't need to do the full make_real
   work. */
static GtkTextRealIter*
gtk_text_iter_make_surreal(const GtkTextIter *_iter)
{
  GtkTextRealIter *iter = (GtkTextRealIter*)_iter;
  
  if (iter->chars_changed_stamp !=
      gtk_text_btree_get_chars_changed_stamp(iter->tree))
    {
      g_warning("Invalid text buffer iterator: either the iterator is uninitialized, or the characters/pixmaps/widgets in the buffer have been modified since the iterator was created.\nYou must use marks, character numbers, or line numbers to preserve a position across buffer modifications.\nYou can apply tags and insert marks without invalidating your iterators, however.");
      return NULL;
    }

  /* We don't update the segments information since we are becoming
     only surreal. However we do invalidate the segments information
     if appropriate, to be sure we segfault if we try to use it and we
     should have used make_real. */

  if (iter->segments_changed_stamp !=
      gtk_text_btree_get_segments_changed_stamp(iter->tree))
    {
      iter->segment = NULL;
      iter->any_segment = NULL;
      /* set to segfault-causing values. */
      iter->segment_byte_offset = -10000;
      iter->segment_char_offset = -10000;
    }
  
  return iter;
}

static GtkTextRealIter*
gtk_text_iter_make_real(const GtkTextIter *_iter)
{
  GtkTextRealIter *iter;
  
  iter = gtk_text_iter_make_surreal(_iter);
  
  if (iter->segments_changed_stamp !=
      gtk_text_btree_get_segments_changed_stamp(iter->tree))
    {
      if (iter->line_byte_offset >= 0)
        {
          iter_set_from_byte_offset(iter,
                                    iter->line,
                                    iter->line_byte_offset);
        }
      else
        {
          g_assert(iter->line_char_offset >= 0);
          
          iter_set_from_char_offset(iter,
                                    iter->line,
                                    iter->line_char_offset);
        }
    }

  g_assert(iter->segment != NULL);
  g_assert(iter->any_segment != NULL);
  g_assert(iter->segment->char_count > 0);
  
  return iter;
}

static GtkTextRealIter*
iter_init_common(GtkTextIter *_iter,
                 GtkTextBTree *tree)
{
  GtkTextRealIter *iter = (GtkTextRealIter*)_iter;

  g_return_val_if_fail(iter != NULL, NULL);
  g_return_val_if_fail(tree != NULL, NULL);

  iter->tree = tree;

  iter->chars_changed_stamp =
    gtk_text_btree_get_chars_changed_stamp(iter->tree);
  
  return iter;
}

static GtkTextRealIter*
iter_init_from_segment(GtkTextIter *iter,
                       GtkTextBTree *tree,
                       GtkTextLine *line,
                       GtkTextLineSegment *segment)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(line != NULL, NULL);

  real = iter_init_common(iter, tree);
  
  iter_set_from_segment(real, line, segment);
  
  return real;
}

static GtkTextRealIter*
iter_init_from_byte_offset(GtkTextIter *iter,
                           GtkTextBTree *tree,
                           GtkTextLine *line,
                           gint line_byte_offset)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(line != NULL, NULL);

  real = iter_init_common(iter, tree);
  
  iter_set_from_byte_offset(real, line, line_byte_offset);
  
  return real;
}

static GtkTextRealIter*
iter_init_from_char_offset(GtkTextIter *iter,
                           GtkTextBTree *tree,
                           GtkTextLine *line,
                           gint line_char_offset)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(line != NULL, NULL);

  real = iter_init_common(iter, tree);
  
  iter_set_from_char_offset(real, line, line_char_offset);
  
  return real;
}

static inline void
invalidate_segment(GtkTextRealIter *iter)
{
  iter->segments_changed_stamp -= 1;
}

static inline void
invalidate_char_index(GtkTextRealIter *iter)
{
  iter->cached_char_index = -1;
}

static inline void
invalidate_line_number(GtkTextRealIter *iter)
{
  iter->cached_line_number = -1;
}

static inline void
adjust_char_index(GtkTextRealIter *iter, gint count)
{
  if (iter->cached_char_index >= 0)
    iter->cached_char_index += count;
}

static inline void
adjust_line_number(GtkTextRealIter *iter, gint count)
{
  if (iter->cached_line_number >= 0)
    iter->cached_line_number += count;
}

static inline void
adjust_char_offsets(GtkTextRealIter *iter, gint count)
{
  if (iter->line_char_offset >= 0)
    {
      iter->line_char_offset += count;
      g_assert(iter->segment_char_offset >= 0);
      iter->segment_char_offset += count;
    }
}

static inline void
adjust_byte_offsets(GtkTextRealIter *iter, gint count)
{
  if (iter->line_byte_offset >= 0)
    {
      iter->line_byte_offset += count;
      g_assert(iter->segment_byte_offset >= 0);
      iter->segment_byte_offset += count;
    }
}

static inline void
ensure_char_offsets(GtkTextRealIter *iter)
{
  if (iter->line_char_offset < 0)
    {
      g_assert(iter->line_byte_offset >= 0);

      gtk_text_line_byte_to_char_offsets(iter->line,
                                          iter->line_byte_offset,
                                          &iter->line_char_offset,
                                          &iter->segment_char_offset);
    }
}

static inline void
ensure_byte_offsets(GtkTextRealIter *iter)
{
  if (iter->line_byte_offset < 0)
    {
      g_assert(iter->line_char_offset >= 0);

      gtk_text_line_char_to_byte_offsets(iter->line,
                                          iter->line_char_offset,
                                          &iter->line_byte_offset,
                                          &iter->segment_byte_offset);
    }
}

#if 1
static void
check_invariants(const GtkTextIter *iter)
{
  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    gtk_text_iter_check(iter);
}
#else
#define check_invariants(x)
#endif

GtkTextBuffer*
gtk_text_iter_get_buffer(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, NULL);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return NULL;

  check_invariants(iter);
  
  return gtk_text_btree_get_buffer(real->tree);
}

GtkTextIter*
gtk_text_iter_copy(const GtkTextIter *iter)
{
  GtkTextIter *new_iter;

  g_return_val_if_fail(iter != NULL, NULL);
  
  new_iter = g_new(GtkTextIter, 1);

  *new_iter = *iter;
  
  return new_iter;
}

void
gtk_text_iter_free(GtkTextIter *iter)
{
  g_return_if_fail(iter != NULL);

  g_free(iter);
}

GtkTextLineSegment*
gtk_text_iter_get_indexable_segment(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return NULL;

  check_invariants(iter);
  
  g_assert(real->segment != NULL);
  
  return real->segment;
}

GtkTextLineSegment*
gtk_text_iter_get_any_segment(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return NULL;

  check_invariants(iter);
  
  g_assert(real->any_segment != NULL);
  
  return real->any_segment;
}

gint
gtk_text_iter_get_segment_byte(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return 0;

  ensure_byte_offsets(real);

  check_invariants(iter);
  
  return real->segment_byte_offset;
}

gint
gtk_text_iter_get_segment_char(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return 0;

  ensure_char_offsets(real);

  check_invariants(iter);
  
  return real->segment_char_offset;
}

/* This function does not require a still-valid
   iterator */
GtkTextLine*
gtk_text_iter_get_line(const GtkTextIter *iter)
{
  const GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = (const GtkTextRealIter*)iter;

  return real->line;
}

/* This function does not require a still-valid
   iterator */
GtkTextBTree*
gtk_text_iter_get_btree(const GtkTextIter *iter)
{
  const GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = (const GtkTextRealIter*)iter;

  return real->tree;
}

/*
 * Conversions
 */

gint
gtk_text_iter_get_char_index(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return 0;

  if (real->cached_char_index < 0)
    {
      real->cached_char_index =
        gtk_text_line_char_index(real->line);
      ensure_char_offsets(real);
      real->cached_char_index += real->line_char_offset;
    }

  check_invariants(iter);
  
  return real->cached_char_index;
}

gint
gtk_text_iter_get_line_number(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return 0;

  if (real->cached_line_number < 0)
    real->cached_line_number =
      gtk_text_line_get_number(real->line);

  check_invariants(iter);
  
  return real->cached_line_number;
}

gint
gtk_text_iter_get_line_char(const GtkTextIter *iter)
{

  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return 0;

  ensure_char_offsets(real);

  check_invariants(iter);
  
  return real->line_char_offset;
}

gint
gtk_text_iter_get_line_byte(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return 0;

  ensure_byte_offsets(real);

  check_invariants(iter);
  
  return real->line_byte_offset;
}

/*
 * Dereferencing
 */

gunichar
gtk_text_iter_get_char(const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, 0);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return 0;

  check_invariants(iter);
  
  /* FIXME probably want to special-case the end iterator
     and either have an error or return 0 */
  
  if (real->segment->type == &gtk_text_char_type)
    {
      ensure_byte_offsets(real);
      
      return g_utf8_get_char (real->segment->body.chars +
                              real->segment_byte_offset);
    }
  else
    {
      /* Unicode "unknown character" 0xFFFD */
      return gtk_text_unknown_char;
    }
}

gchar*
gtk_text_iter_get_slice       (const GtkTextIter *start,
                                const GtkTextIter *end)
{
  g_return_val_if_fail(start != NULL, NULL);
  g_return_val_if_fail(end != NULL, NULL);

  check_invariants(start);
  check_invariants(end);
  
  return gtk_text_btree_get_text(start, end, TRUE, TRUE);
}

gchar*
gtk_text_iter_get_text       (const GtkTextIter *start,
                               const GtkTextIter *end)
{
  g_return_val_if_fail(start != NULL, NULL);
  g_return_val_if_fail(end != NULL, NULL);

  check_invariants(start);
  check_invariants(end);
  
  return gtk_text_btree_get_text(start, end, TRUE, FALSE);
}

gchar*
gtk_text_iter_get_visible_slice (const GtkTextIter  *start,
                                  const GtkTextIter  *end)
{
  g_return_val_if_fail(start != NULL, NULL);
  g_return_val_if_fail(end != NULL, NULL);

  check_invariants(start);
  check_invariants(end);
  
  return gtk_text_btree_get_text(start, end, FALSE, TRUE);
}

gchar*
gtk_text_iter_get_visible_text (const GtkTextIter  *start,
                                 const GtkTextIter  *end)
{
  g_return_val_if_fail(start != NULL, NULL);
  g_return_val_if_fail(end != NULL, NULL);
  
  check_invariants(start);
  check_invariants(end);
  
  return gtk_text_btree_get_text(start, end, FALSE, FALSE);
}

gboolean
gtk_text_iter_get_pixmap      (const GtkTextIter *iter,
                                GdkPixmap** pixmap,
                                GdkBitmap** mask)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(pixmap != NULL, FALSE);
  g_return_val_if_fail(mask != NULL, FALSE);

  *pixmap = NULL;
  *mask = NULL;
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;
  
  check_invariants(iter);
  
  if (real->segment->type != &gtk_text_pixmap_type)
    return FALSE;
  else
    {
      *pixmap = real->segment->body.pixmap.pixmap;
      *mask = real->segment->body.pixmap.pixmap;

      return TRUE;
    }
}

GSList*
gtk_text_iter_get_toggled_tags  (const GtkTextIter  *iter,
                                  gboolean             toggled_on)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  GSList *retval;
  
  g_return_val_if_fail(iter != NULL, NULL);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return NULL;

  check_invariants(iter);
  
  retval = NULL;
  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (toggled_on)
        {
          if (seg->type == &gtk_text_toggle_on_type)
            {
              retval = g_slist_prepend(retval, seg->body.toggle.info->tag);
            }
        }
      else
        {
          if (seg->type == &gtk_text_toggle_off_type)
            {
              retval = g_slist_prepend(retval, seg->body.toggle.info->tag);
            }
        }
      
      seg = seg->next;
    }

  /* The returned list isn't guaranteed to be in any special order,
     and it isn't. */
  return retval;
}

gboolean
gtk_text_iter_begins_tag    (const GtkTextIter  *iter,
                              GtkTextTag         *tag)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (seg->type == &gtk_text_toggle_on_type)
        {
          if (tag == NULL ||
              seg->body.toggle.info->tag == tag)
            return TRUE;
        }
      
      seg = seg->next;
    }

  return FALSE;
}

gboolean
gtk_text_iter_ends_tag   (const GtkTextIter  *iter,
                           GtkTextTag         *tag)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (seg->type == &gtk_text_toggle_off_type)
        {
          if (tag == NULL ||
              seg->body.toggle.info->tag == tag)
            return TRUE;
        }
      
      seg = seg->next;
    }

  return FALSE;
}

gboolean
gtk_text_iter_toggles_tag       (const GtkTextIter  *iter,
                                  GtkTextTag         *tag)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  seg = real->any_segment;
  while (seg != real->segment)
    {
      if ( (seg->type == &gtk_text_toggle_off_type ||
            seg->type == &gtk_text_toggle_on_type) &&
           (tag == NULL ||
            seg->body.toggle.info->tag == tag) )
        return TRUE;
      
      seg = seg->next;
    }

  return FALSE;
}

gboolean
gtk_text_iter_has_tag           (const GtkTextIter   *iter,
                                  GtkTextTag          *tag)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_TEXT_TAG(tag), FALSE);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return FALSE; 

  check_invariants(iter);
  
  if (real->line_byte_offset >= 0)
    {
      return gtk_text_line_byte_has_tag(real->line, real->tree,
                                         real->line_byte_offset, tag);
    }
  else
    {
      g_assert(real->line_char_offset >= 0);
      return gtk_text_line_char_has_tag(real->line, real->tree,
                                         real->line_char_offset, tag);
    }
}

gboolean
gtk_text_iter_starts_line (const GtkTextIter   *iter)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return FALSE;  

  check_invariants(iter);
  
  if (real->line_byte_offset >= 0)
    {
      return (real->line_byte_offset == 0);
    }
  else
    {
      g_assert(real->line_char_offset >= 0);
      return (real->line_char_offset == 0);
    }
}

gboolean
gtk_text_iter_ends_line (const GtkTextIter   *iter)
{
  g_return_val_if_fail(iter != NULL, FALSE);

  check_invariants(iter);
  
  return gtk_text_iter_get_char(iter) == '\n';
}

gint
gtk_text_iter_get_chars_in_line (const GtkTextIter   *iter)
{
  GtkTextRealIter *real;
  gint count;
  GtkTextLineSegment *seg;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return 0;  

  check_invariants(iter);
  
  if (real->line_char_offset >= 0)
    {
      /* We can start at the segments we've already found. */
      count = real->line_char_offset - real->segment_char_offset;
      seg = gtk_text_iter_get_indexable_segment(iter);
    }
  else
    {
      /* count whole line. */
      seg = real->line->segments;
      count = 0;
    }

  
  while (seg != NULL)
    {
      count += seg->char_count;
      
      seg = seg->next;
    }

  return count;
}

/*
 * Increments/decrements
 */

static gboolean
forward_line_leaving_caches_unmodified(GtkTextRealIter *real)
{
  GtkTextLine *new_line;
  
  new_line = gtk_text_line_next(real->line);

  g_assert(new_line != real->line);
  
  if (new_line != NULL)
    {      
      real->line = new_line;

      real->line_byte_offset = 0;
      real->line_char_offset = 0;
      
      real->segment_byte_offset = 0;
      real->segment_char_offset = 0;
      
      /* Find first segments in new line */
      real->any_segment = real->line->segments;
      real->segment = real->any_segment;
      while (real->segment->char_count == 0)
        real->segment = real->segment->next;
      
      return TRUE;
    }
  else
    {
      /* There is no way to move forward; we were already
         at the "end" index. (the end index is the last
         line pointer, segment_byte_offset of 0) */

      g_assert(real->line_char_offset == 0 ||
               real->line_byte_offset == 0);
      
      /* The only indexable segment allowed on the bogus
         line at the end is a single char segment containing
         a newline. */
      if (real->segments_changed_stamp ==
          gtk_text_btree_get_segments_changed_stamp(real->tree))
        {
          g_assert(real->segment->type == &gtk_text_char_type);
          g_assert(real->segment->char_count == 1);
        }
      /* We leave real->line as-is */
      
      return FALSE;
    }
}

static gboolean
forward_char(GtkTextRealIter *real)
{
  GtkTextIter *iter = (GtkTextIter*)real;

  check_invariants((GtkTextIter*)real);
  
  ensure_char_offsets(real);
  
  if ( (real->segment_char_offset + 1) == real->segment->char_count)
    {
      /* Need to move to the next segment; if no next segment,
         need to move to next line. */
      return gtk_text_iter_forward_indexable_segment(iter);
    }
  else
    {
      /* Just moving within a segment. Keep byte count
         up-to-date, if it was already up-to-date. */

      g_assert(real->segment->type == &gtk_text_char_type);
      
      if (real->line_byte_offset >= 0)
        {
          gint bytes;
          const char * start =
            real->segment->body.chars + real->segment_byte_offset;
          
          bytes = g_utf8_next_char (start) - start;

          real->line_byte_offset += bytes;
          real->segment_byte_offset += bytes;

          g_assert(real->segment_byte_offset < real->segment->byte_count);
        }

      real->line_char_offset += 1;
      real->segment_char_offset += 1;

      adjust_char_index(real, 1);
      
      g_assert(real->segment_char_offset < real->segment->char_count);

      /* We moved into the middle of a segment, so the any_segment
         must now be the segment we're in the middle of. */
      real->any_segment = real->segment;
      
      check_invariants((GtkTextIter*)real);
      
      return TRUE;
    }
}

gboolean
gtk_text_iter_forward_indexable_segment(GtkTextIter *iter)
{
  /* Need to move to the next segment; if no next segment,
     need to move to next line. */
  GtkTextLineSegment *seg;
  GtkTextLineSegment *any_seg;
  GtkTextRealIter *real;
  gint chars_skipped;
  gint bytes_skipped;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  if (real->line_char_offset >= 0)
    {
      chars_skipped = real->segment->char_count - real->segment_char_offset;
      g_assert(chars_skipped > 0);
    }
  else
    chars_skipped = 0;

  if (real->line_byte_offset >= 0)
    {
      bytes_skipped = real->segment->byte_count - real->segment_byte_offset;
      g_assert(bytes_skipped > 0);
    }
  else
    bytes_skipped = 0;
  
  /* Get first segment of any kind */
  any_seg = real->segment->next;
  /* skip non-indexable segments, if any */
  seg = any_seg;
  while (seg != NULL && seg->char_count == 0)
    seg = seg->next;
  
  if (seg != NULL)
    {
      real->any_segment = any_seg;
      real->segment = seg;

      if (real->line_byte_offset >= 0)
        {
          g_assert(bytes_skipped > 0);
          real->segment_byte_offset = 0;
          real->line_byte_offset += bytes_skipped;
        }

      if (real->line_char_offset >= 0)
        {
          g_assert(chars_skipped > 0);
          real->segment_char_offset = 0;
          real->line_char_offset += chars_skipped;
          adjust_char_index(real, chars_skipped);
        }

      check_invariants(iter);
      
      return TRUE;
    }
  else
    {      
      /* End of the line */
      if (forward_line_leaving_caches_unmodified(real))
        {
          adjust_line_number(real, 1);
          if (real->line_char_offset >= 0)
            adjust_char_index(real, chars_skipped);

          check_invariants(iter);

          g_assert(real->line_byte_offset == 0);
          g_assert(real->line_char_offset == 0);
          g_assert(real->segment_byte_offset == 0);
          g_assert(real->segment_char_offset == 0);
          g_assert(gtk_text_iter_starts_line(iter));

          check_invariants(iter);
          
          return TRUE;
        }
      else
        {
          /* End of buffer */

          check_invariants(iter);
          
          return FALSE;
        }
    }
}

gboolean
gtk_text_iter_backward_indexable_segment(GtkTextIter *iter)
{
  g_warning("FIXME");

}

gboolean
gtk_text_iter_forward_char(GtkTextIter *iter)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;
  else
    {
      check_invariants(iter);
      return forward_char(real);
    }
}

gboolean
gtk_text_iter_backward_char(GtkTextIter *iter)
{
  g_return_val_if_fail(iter != NULL, FALSE);

  check_invariants(iter);
  
  return gtk_text_iter_backward_chars(iter, 1);
}

/*
  Definitely we should try to linear scan as often as possible for
  movement within a single line, because we can't use the BTree to
  speed within-line searches up; for movement between lines, we would
  like to avoid the linear scan probably.
  
  Instead of using this constant, it might be nice to cache the line
  length in the iterator and linear scan if motion is within a single
  line.

  I guess you'd have to profile the various approaches.
*/
#define MAX_LINEAR_SCAN 300

gboolean
gtk_text_iter_forward_chars(GtkTextIter *iter, gint count)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);
  
  if (real == NULL)
    return FALSE;
  else if (count == 0)
    return FALSE;
  else if (count < 0)
    return gtk_text_iter_backward_chars(iter, 0 - count);
  else if (count < MAX_LINEAR_SCAN)
    {
      check_invariants(iter);
      
      while (count > 1)
        {
          if (!forward_char(real))
            return FALSE;
          --count;
        }
      
      return forward_char(real);
    }
  else
    {
      gint current_char_index;
      gint new_char_index;

      check_invariants(iter);
      
      current_char_index = gtk_text_iter_get_char_index(iter);

      if (current_char_index == gtk_text_btree_char_count(real->tree))
        return FALSE; /* can't move forward */
      
      new_char_index = current_char_index + count;
      gtk_text_iter_set_char_index(iter, new_char_index);

      check_invariants(iter);

      return TRUE;
    }
}

gboolean
gtk_text_iter_backward_chars(GtkTextIter *iter, gint count)
{
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);
  
  if (real == NULL)
    return FALSE;
  else if (count == 0)
    return FALSE;
  else if (count < 0)
    return gtk_text_iter_forward_chars(iter, 0 - count);

  ensure_char_offsets(real);
  check_invariants(iter);
  
  if (count <= real->segment_char_offset)
    {
      /* Optimize the within-segment case */      
      g_assert(real->segment->char_count > 0);
      g_assert(real->segment->type == &gtk_text_char_type);

      real->segment_char_offset -= count;
      g_assert(real->segment_char_offset >= 0);

      if (real->line_byte_offset >= 0)
        {
          gint new_byte_offset;
          gint i;

          new_byte_offset = 0;
          i = 0;
          while (i < real->segment_char_offset)
            {
              const char * start = real->segment->body.chars + new_byte_offset;
              new_byte_offset += g_utf8_next_char (start) - start;

              ++i;
            }
      
          real->line_byte_offset -= (real->segment_byte_offset - new_byte_offset);
          real->segment_byte_offset = new_byte_offset;
        }
      
      real->line_char_offset -= count;

      adjust_char_index(real, 0 - count);

      check_invariants(iter);
      
      return TRUE;
    }
  else
    {
      /* We need to go back into previous segments. For now,
         just keep this really simple. */
      gint current_char_index;
      gint new_char_index;
      
      current_char_index = gtk_text_iter_get_char_index(iter);

      if (current_char_index == 0)
        return FALSE; /* can't move backward */
      
      new_char_index = current_char_index - count;
      if (new_char_index < 0)
        new_char_index = 0;
      gtk_text_iter_set_char_index(iter, new_char_index);

      check_invariants(iter);
      
      return TRUE;
    }
}

gboolean
gtk_text_iter_forward_line(GtkTextIter *iter)
{
  GtkTextRealIter *real;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);
  
  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  if (forward_line_leaving_caches_unmodified(real))
    {
      invalidate_char_index(real);
      adjust_line_number(real, 1);

      check_invariants(iter);
      
      return TRUE;
    }
  else
    {
      check_invariants(iter);
      return FALSE;
    }
}

gboolean
gtk_text_iter_backward_line(GtkTextIter *iter)
{
  GtkTextLine *new_line;
  GtkTextRealIter *real;
  gboolean offset_will_change;
  gint offset;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);
  
  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  new_line = gtk_text_line_previous(real->line);

  offset_will_change = FALSE;
  if (real->line_char_offset > 0)
    offset_will_change = TRUE;
          
  if (new_line != NULL)
    {
      real->line = new_line;
      
      adjust_line_number(real, -1);
    }
  else
    {
      if (!offset_will_change)
        return FALSE;
    }

  invalidate_char_index(real);
  
  real->line_byte_offset = 0;
  real->line_char_offset = 0;

  real->segment_byte_offset = 0;
  real->segment_char_offset = 0;
  
  /* Find first segment in line */
  real->any_segment = real->line->segments;
  real->segment = gtk_text_line_byte_to_segment(real->line,
                                                 0, &offset);

  g_assert(offset == 0);

  /* Note that if we are on the first line, we snap to the start
     of the first line and return TRUE, so TRUE means the
     iterator changed, not that the line changed; this is maybe
     a bit weird. I'm not sure there's an obvious right thing
     to do though. */

  check_invariants(iter);
  
  return TRUE;
}

gboolean
gtk_text_iter_forward_lines(GtkTextIter *iter, gint count)
{
  if (count < 0)
    return gtk_text_iter_backward_lines(iter, 0 - count);
  else if (count == 0)
    return FALSE;
  else if (count == 1)
    {
      check_invariants(iter);
      return gtk_text_iter_forward_line(iter);
    }
  else
    {
      gint old_line;
      
      old_line = gtk_text_iter_get_line_number(iter);
      
      gtk_text_iter_set_line_number(iter, old_line + count);

      check_invariants(iter);
      
      return (gtk_text_iter_get_line_number(iter) != old_line);
    }
}

gboolean
gtk_text_iter_backward_lines(GtkTextIter *iter, gint count)
{
  if (count < 0)
    return gtk_text_iter_forward_lines(iter, 0 - count);
  else if (count == 0)
    return FALSE;
  else if (count == 1)
    {
      return gtk_text_iter_backward_line(iter);
    }
  else
    {
      gint old_line;
      
      old_line = gtk_text_iter_get_line_number(iter);
      
      gtk_text_iter_set_line_number(iter, MAX(old_line - count, 0));

      return (gtk_text_iter_get_line_number(iter) != old_line);
    }
}

static gboolean
is_word_char(gunichar ch, gpointer user_data)
{
  /* will likely need some i18n help FIXME */
  return isalpha(ch);
}

static gboolean
is_not_word_char(gunichar ch, gpointer user_data)
{
  return !is_word_char(ch, user_data);
}

static gboolean
gtk_text_iter_is_in_word(const GtkTextIter *iter)
{
  gint ch;

  ch = gtk_text_iter_get_char(iter);

  return is_word_char(ch, NULL);
}

gboolean
gtk_text_iter_forward_word_end(GtkTextIter      *iter)
{  
  gboolean in_word;
  GtkTextIter start;
  
  g_return_val_if_fail(iter != NULL, FALSE);

  start = *iter;
  
  in_word = gtk_text_iter_is_in_word(iter);

  if (!in_word)
    {
      if (!gtk_text_iter_forward_find_char(iter, is_word_char, NULL))
        return !gtk_text_iter_equal(iter, &start);
      else
        in_word = TRUE;
    }

  g_assert(in_word);
  
  gtk_text_iter_forward_find_char(iter, is_not_word_char, NULL);

  return !gtk_text_iter_equal(iter, &start);
}

gboolean
gtk_text_iter_backward_word_start(GtkTextIter      *iter)
{
  gboolean in_word;
  GtkTextIter start;
  
  g_return_val_if_fail(iter != NULL, FALSE);

  start = *iter;
  
  in_word = gtk_text_iter_is_in_word(iter);

  if (!in_word)
    {
      if (!gtk_text_iter_backward_find_char(iter, is_word_char, NULL))
        return !gtk_text_iter_equal(iter, &start);
      else
        in_word = TRUE;
    }

  g_assert(in_word);
  
  gtk_text_iter_backward_find_char(iter, is_not_word_char, NULL);
  gtk_text_iter_forward_char(iter); /* point to first char of word,
                                        not first non-word char. */
  
  return !gtk_text_iter_equal(iter, &start);
}

gboolean
gtk_text_iter_forward_word_ends(GtkTextIter      *iter,
                                 gint               count)
{
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(count > 0, FALSE);

  if (!gtk_text_iter_forward_word_end(iter))
    return FALSE;
  --count;
  
  while (count > 0)
    {
      if (!gtk_text_iter_forward_word_end(iter))
        break;
      --count;
    }
  return TRUE;
}

gboolean
gtk_text_iter_backward_word_starts(GtkTextIter      *iter,
                                    gint               count)
{
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(count > 0, FALSE);

  if (!gtk_text_iter_backward_word_start(iter))
    return FALSE;
  --count;
  
  while (count > 0)
    {
      if (!gtk_text_iter_backward_word_start(iter))
        break;
      --count;
    }
  return TRUE;
}

/* up/down lines maintain the char offset, while forward/backward lines
   always sets the char offset to 0. */
gboolean
gtk_text_iter_up_lines        (GtkTextIter *iter,
                                gint count)
{
  gint char_offset;

  if (count < 0)
    return gtk_text_iter_down_lines(iter, 0 - count);
  
  char_offset = gtk_text_iter_get_line_char(iter);

  if (!gtk_text_iter_backward_line(iter))
    return FALSE;
  --count;
  
  while (count > 0)
    {
      if (!gtk_text_iter_backward_line(iter))
        break;
      --count;
    }

  gtk_text_iter_set_line_char(iter, char_offset);
  
  return TRUE;
}

gboolean
gtk_text_iter_down_lines        (GtkTextIter *iter,
                                  gint count)
{
  gint char_offset;

  if (count < 0)
    return gtk_text_iter_up_lines(iter, 0 - count);
  
  char_offset = gtk_text_iter_get_line_char(iter);

  if (!gtk_text_iter_forward_line(iter))
    return FALSE;
  --count;
  
  while (count > 0)
    {
      if (!gtk_text_iter_forward_line(iter))
        break;
      --count;
    }

  gtk_text_iter_set_line_char(iter, char_offset);
  
  return TRUE;
}

void
gtk_text_iter_set_line_char(GtkTextIter *iter,
                             gint char_on_line)
{
  GtkTextRealIter *real;
  
  g_return_if_fail(iter != NULL);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return;

  check_invariants(iter);
  
  iter_set_from_char_offset(real, real->line, char_on_line);

  check_invariants(iter);
}

void
gtk_text_iter_set_line_number(GtkTextIter *iter, gint line_number)
{
  GtkTextLine *line;
  gint real_line;
  GtkTextRealIter *real;
  
  g_return_if_fail(iter != NULL);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return;

  check_invariants(iter);
  
  line = gtk_text_btree_get_line(real->tree, line_number, &real_line);

  iter_set_from_char_offset(real, line, 0);
  
  /* We might as well cache this, since we know it. */
  real->cached_line_number = real_line;

  check_invariants(iter);
}

void
gtk_text_iter_set_char_index(GtkTextIter *iter, gint char_index)
{
  GtkTextLine *line;
  GtkTextRealIter *real;
  gint line_start;
  gint real_char_index;
  
  g_return_if_fail(iter != NULL);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return;

  check_invariants(iter);
  
  if (real->cached_char_index >= 0 &&
      real->cached_char_index == char_index)
    return;

  line = gtk_text_btree_get_line_at_char(real->tree,
                                          char_index,
                                          &line_start,
                                          &real_char_index);

  iter_set_from_char_offset(real, line, real_char_index - line_start);
  
  /* Go ahead and cache this since we have it. */
  real->cached_char_index = real_char_index;

  check_invariants(iter);
}

void
gtk_text_iter_forward_to_end  (GtkTextIter       *iter)
{
  GtkTextBuffer *buffer;
  GtkTextRealIter *real;

  g_return_if_fail(iter != NULL);
  
  real = gtk_text_iter_make_surreal(iter);

  if (real == NULL)
    return;
  
  buffer = gtk_text_btree_get_buffer(real->tree);

  gtk_text_buffer_get_last_iter(buffer, iter);
}

gboolean
gtk_text_iter_forward_to_newline(GtkTextIter *iter)
{
  gint current_offset;
  gint new_offset;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  
  current_offset = gtk_text_iter_get_line_char(iter);
  new_offset = gtk_text_iter_get_chars_in_line(iter) - 1;

  if (current_offset < new_offset)
    {
      /* Move to end of this line. */
      gtk_text_iter_set_line_char(iter, new_offset);
      return TRUE;
    }
  else
    {
      /* Move to end of next line. */
      if (gtk_text_iter_forward_line(iter))
        {
          gtk_text_iter_forward_to_newline(iter);
          return TRUE;
        }
      else
        return FALSE;
    }
}

gboolean
gtk_text_iter_forward_find_tag_toggle (GtkTextIter *iter,
                                        GtkTextTag  *tag)
{
  GtkTextLine *next_line;
  GtkTextLine *current_line;
  GtkTextRealIter *real;

  g_return_val_if_fail(iter != NULL, FALSE);
  
  real = gtk_text_iter_make_real(iter);

  if (real == NULL)
    return FALSE;

  check_invariants(iter);
  
  current_line = real->line;
  next_line = gtk_text_line_next_could_contain_tag(current_line,
                                                    real->tree, tag);
  
  while (gtk_text_iter_forward_indexable_segment(iter))
    {
      /* If we went forward to a line that couldn't contain a toggle
         for the tag, then skip forward to a line that could contain
         it. This potentially skips huge hunks of the tree, so we
         aren't a purely linear search. */
      if (real->line != current_line)
        {
          if (next_line == NULL)
            {
              /* End of search. Set to end of buffer. */
              gtk_text_btree_get_last_iter(real->tree, iter);
              return FALSE;
            }
              
          if (real->line != next_line)
            iter_set_from_byte_offset(real, next_line, 0);

          current_line = real->line;
          next_line = gtk_text_line_next_could_contain_tag(current_line,
                                                            real->tree,
                                                            tag);
        }

      if (gtk_text_iter_toggles_tag(iter, tag))
        {
          /* If there's a toggle here, it isn't indexable so
             any_segment can't be the indexable segment. */
          g_assert(real->any_segment != real->segment);
          return TRUE;
        }
    }

  /* Reached end of buffer */
  return FALSE;
}

gboolean
gtk_text_iter_backward_find_tag_toggle (GtkTextIter *iter,
                                         GtkTextTag  *tag)
{

  g_warning("FIXME");
}

static gboolean
matches_pred(GtkTextIter *iter,
             GtkTextViewCharPredicate pred,
             gpointer user_data)
{
  gint ch;

  ch = gtk_text_iter_get_char(iter);

  return (*pred) (ch, user_data);
}

gboolean
gtk_text_iter_forward_find_char (GtkTextIter *iter,
                                  GtkTextViewCharPredicate pred,
                                  gpointer user_data)
{
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(pred != NULL, FALSE);

  while (gtk_text_iter_forward_char(iter))
    {
      if (matches_pred(iter, pred, user_data))
        return TRUE;
    }
  
  return FALSE;
}

gboolean
gtk_text_iter_backward_find_char (GtkTextIter *iter,
                                   GtkTextViewCharPredicate pred,
                                   gpointer user_data)
{
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(pred != NULL, FALSE);

  while (gtk_text_iter_backward_char(iter))
    {
      if (matches_pred(iter, pred, user_data))
        return TRUE;
    }
  
  return FALSE;
}

/*
 * Comparisons
 */

gboolean
gtk_text_iter_equal(const GtkTextIter *lhs, const GtkTextIter *rhs)
{
  GtkTextRealIter *real_lhs;
  GtkTextRealIter *real_rhs;

  real_lhs = (GtkTextRealIter*)lhs;
  real_rhs = (GtkTextRealIter*)rhs;

  check_invariants(lhs);
  check_invariants(rhs);
  
  if (real_lhs->line != real_rhs->line)
    return FALSE;
  else if (real_lhs->line_byte_offset >= 0 &&
           real_rhs->line_byte_offset >= 0)
    return real_lhs->line_byte_offset == real_rhs->line_byte_offset;
  else
    {
      /* the ensure_char_offsets() calls do nothing if the char offsets
         are already up-to-date. */
      ensure_char_offsets(real_lhs);
      ensure_char_offsets(real_rhs);
      return real_lhs->line_char_offset == real_rhs->line_char_offset; 
    }
}

gint
gtk_text_iter_compare(const GtkTextIter *lhs, const GtkTextIter *rhs)
{
  GtkTextRealIter *real_lhs;
  GtkTextRealIter *real_rhs;

  real_lhs = gtk_text_iter_make_surreal(lhs);
  real_rhs = gtk_text_iter_make_surreal(rhs);

  check_invariants(lhs);
  check_invariants(rhs);
  
  if (real_lhs == NULL ||
      real_rhs == NULL)
    return -1; /* why not */

  if (real_lhs->line == real_rhs->line)
    {
      gint left_index, right_index;

      if (real_lhs->line_byte_offset >= 0 &&
          real_rhs->line_byte_offset >= 0)
        {
          left_index = real_lhs->line_byte_offset;
          right_index = real_rhs->line_byte_offset;
        }
      else
        {
          /* the ensure_char_offsets() calls do nothing if
             the offsets are already up-to-date. */
          ensure_char_offsets(real_lhs);
          ensure_char_offsets(real_rhs);
          left_index = real_lhs->line_char_offset;
          right_index = real_rhs->line_char_offset;
        }
      
      if (left_index < right_index)
        return -1;
      else if (left_index > right_index)
        return 1;
      else
        return 0;
    }
  else
    {
      gint line1, line2;
      
      line1 = gtk_text_iter_get_line_number(lhs);
      line2 = gtk_text_iter_get_line_number(rhs);
      if (line1 < line2)
        return -1;
      else if (line1 > line2)
        return 1;
      else
        return 0;  
    }
}

gboolean
gtk_text_iter_in_region (const GtkTextIter *iter,
                          const GtkTextIter *start,
                          const GtkTextIter *end)
{
  return gtk_text_iter_compare(iter, start) >= 0 &&
    gtk_text_iter_compare(iter, end) < 0;
}

void
gtk_text_iter_reorder         (GtkTextIter *first,
                                GtkTextIter *second)
{
  g_return_if_fail(first != NULL);
  g_return_if_fail(second != NULL);

  if (gtk_text_iter_compare(first, second) > 0)
    {
      GtkTextIter tmp;

      tmp = *first;
      *first = *second;
      *second = tmp;
    }
}

/*
 * Init iterators from the BTree
 */

void
gtk_text_btree_get_iter_at_char (GtkTextBTree *tree,
                                  GtkTextIter *iter,
                                  gint char_index)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  gint real_char_index;
  gint line_start;
  GtkTextLine *line;
  
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tree != NULL);

  line = gtk_text_btree_get_line_at_char(tree, char_index,
                                          &line_start, &real_char_index);
  
  iter_init_from_char_offset(iter, tree, line, real_char_index - line_start);

 real->cached_char_index = real_char_index;

 check_invariants(iter);
}

void
gtk_text_btree_get_iter_at_line_char (GtkTextBTree *tree,
                                       GtkTextIter *iter,
                                       gint line_number,
                                       gint char_on_line)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  GtkTextLine *line;
  gint real_line;
  
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tree != NULL);
  
  line = gtk_text_btree_get_line(tree, line_number, &real_line);

  iter_init_from_char_offset(iter, tree, line, char_on_line);

  /* We might as well cache this, since we know it. */
  real->cached_line_number = real_line;

  check_invariants(iter);
}

void
gtk_text_btree_get_iter_at_line_byte (GtkTextBTree   *tree,
				      GtkTextIter    *iter,
				      gint            line_number,
				      gint            byte_index)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  GtkTextLine *line;
  gint real_line;
  
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tree != NULL);
  
  line = gtk_text_btree_get_line(tree, line_number, &real_line);

  iter_init_from_byte_offset(iter, tree, line, byte_index);

  /* We might as well cache this, since we know it. */
  real->cached_line_number = real_line;

  check_invariants(iter);
}

void
gtk_text_btree_get_iter_at_line      (GtkTextBTree   *tree,
                                       GtkTextIter    *iter,
                                       GtkTextLine    *line,
                                       gint             byte_offset)
{
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tree != NULL);
  g_return_if_fail(line != NULL);

  iter_init_from_byte_offset(iter, tree, line, byte_offset);

  check_invariants(iter);
}

gboolean
gtk_text_btree_get_iter_at_first_toggle (GtkTextBTree   *tree,
                                          GtkTextIter    *iter,
                                          GtkTextTag     *tag)
{
  GtkTextLine *line;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(tree != NULL, FALSE);

  line = gtk_text_btree_first_could_contain_tag(tree, tag);

  if (line == NULL)
    {
      /* Set iter to last in tree */
      gtk_text_btree_get_last_iter(tree, iter);
      check_invariants(iter);
      return FALSE;
    }
  else
    {
      iter_init_from_byte_offset(iter, tree, line, 0);
      gtk_text_iter_forward_find_tag_toggle(iter, tag);
      check_invariants(iter);
      return TRUE;
    }
}

gboolean
gtk_text_btree_get_iter_at_last_toggle  (GtkTextBTree   *tree,
                                          GtkTextIter    *iter,
                                          GtkTextTag     *tag)
{
  GtkTextLine *line;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(tree != NULL, FALSE);

  line = gtk_text_btree_last_could_contain_tag(tree, tag);

  if (line == NULL)
    {
      /* Set iter to first in tree */
      gtk_text_btree_get_iter_at_line_char(tree, iter, 0, 0);
      check_invariants(iter);
      return FALSE;
    }
  else
    {
      iter_init_from_byte_offset(iter, tree, line, -1);
      gtk_text_iter_backward_find_tag_toggle(iter, tag);
      check_invariants(iter);
      return TRUE;
    }
}

gboolean
gtk_text_btree_get_iter_from_string (GtkTextBTree *tree,
                                      GtkTextIter *iter,
                                      const gchar *string)
{
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(tree != NULL, FALSE);
  
  g_warning("FIXME");
}

gboolean
gtk_text_btree_get_iter_at_mark_name (GtkTextBTree *tree,
                                       GtkTextIter *iter,
                                       const gchar *mark_name)
{
  GtkTextMark *mark;
  
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(tree != NULL, FALSE);
  
  mark = gtk_text_btree_get_mark_by_name(tree, mark_name);

  if (mark == NULL)
    return FALSE;
  else
    {
      gtk_text_btree_get_iter_at_mark(tree, iter, mark);
      check_invariants(iter);
      return TRUE;
    }
}

void
gtk_text_btree_get_iter_at_mark (GtkTextBTree *tree,
                                  GtkTextIter *iter,
                                  GtkTextMark *mark)
{
  GtkTextLineSegment *seg = (GtkTextLineSegment*) mark;
  
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tree != NULL);
  g_return_if_fail(GTK_IS_TEXT_MARK (mark));
  
  iter_init_from_segment(iter, tree,
                         seg->body.mark.line, seg);
  g_assert(seg->body.mark.line == gtk_text_iter_get_line(iter));
  check_invariants(iter);
}

void
gtk_text_btree_get_last_iter         (GtkTextBTree   *tree,
                                       GtkTextIter    *iter)
{
  g_return_if_fail(iter != NULL);
  g_return_if_fail(tree != NULL);
  
  gtk_text_btree_get_iter_at_char(tree,
                                   iter,
                                   gtk_text_btree_char_count(tree));
  check_invariants(iter);
}

void
gtk_text_iter_spew (const GtkTextIter *iter, const gchar *desc)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  
  g_return_if_fail(iter != NULL);

  if (real->chars_changed_stamp != gtk_text_btree_get_chars_changed_stamp(real->tree))
    g_print(" %20s: <invalidated iterator>\n", desc);
  else
    {
      check_invariants(iter);
      g_print(" %20s: line %d / char %d / line char %d / line byte %d\n",
             desc,
             gtk_text_iter_get_line_number(iter),
             gtk_text_iter_get_char_index(iter),
             gtk_text_iter_get_line_char(iter),
             gtk_text_iter_get_line_byte(iter));
      check_invariants(iter);
    }
}

void
gtk_text_iter_check(const GtkTextIter *iter)
{
  const GtkTextRealIter *real = (const GtkTextRealIter*)iter;
  gint line_char_offset, line_byte_offset, seg_char_offset, seg_byte_offset;
  GtkTextLineSegment *byte_segment;
  GtkTextLineSegment *byte_any_segment;
  GtkTextLineSegment *char_segment;
  GtkTextLineSegment *char_any_segment;
  gboolean segments_updated;
  
  /* We are going to check our class invariants for the Iter class. */

  if (real->chars_changed_stamp !=
      gtk_text_btree_get_chars_changed_stamp(real->tree))
    g_error("iterator check failed: invalid iterator");
  
  if (real->line_char_offset < 0 && real->line_byte_offset < 0)
    g_error("iterator check failed: both char and byte offsets are invalid");

  segments_updated = (real->segments_changed_stamp ==
                      gtk_text_btree_get_segments_changed_stamp(real->tree));

#if 0
  printf("checking iter, segments %s updated, byte %d char %d\n",
         segments_updated ? "are" : "aren't",
         real->line_byte_offset,
         real->line_char_offset);
#endif

  if (real->line_byte_offset == 97 &&
      real->line_char_offset == 95)
    G_BREAKPOINT();
  
  if (segments_updated)
    {
      if (real->segment_char_offset < 0 && real->segment_byte_offset < 0)
        g_error("iterator check failed: both char and byte segment offsets are invalid");
      
      if (real->segment->char_count == 0)
        g_error("iterator check failed: segment is not indexable.");

      if (real->line_char_offset >= 0 && real->segment_char_offset < 0)
        g_error("segment char offset is not properly up-to-date");

      if (real->line_byte_offset >= 0 && real->segment_byte_offset < 0)
        g_error("segment byte offset is not properly up-to-date");

      if (real->segment_byte_offset >= 0 &&
          real->segment_byte_offset >= real->segment->byte_count)
        g_error("segment byte offset is too large.");

      if (real->segment_char_offset >= 0 &&
          real->segment_char_offset >= real->segment->char_count)
        g_error("segment char offset is too large.");
    }
  
  if (real->line_byte_offset >= 0)
    {
      gtk_text_line_byte_locate(real->line, real->line_byte_offset,
                                 &byte_segment, &byte_any_segment,
                                 &seg_byte_offset, &line_byte_offset);

      if (line_byte_offset != real->line_byte_offset)
        g_error("wrong byte offset was stored in iterator");
      
      if (segments_updated)
        {
          if (real->segment != byte_segment)
            g_error("wrong segment was stored in iterator");

          if (real->any_segment != byte_any_segment)
            g_error("wrong any_segment was stored in iterator");
          
          if (seg_byte_offset != real->segment_byte_offset)
            g_error("wrong segment byte offset was stored in iterator");
        }
    }
  
  if (real->line_char_offset >= 0)
    {
      gtk_text_line_char_locate(real->line, real->line_char_offset,
                                 &char_segment, &char_any_segment,
                                 &seg_char_offset, &line_char_offset);

      if (line_char_offset != real->line_char_offset)
        g_error("wrong char offset was stored in iterator");
      
      if (segments_updated)
        {
          if (real->segment != char_segment)
            g_error("wrong segment was stored in iterator");

          if (real->any_segment != char_any_segment)
            g_error("wrong any_segment was stored in iterator");
          
          if (seg_char_offset != real->segment_char_offset)
            g_error("wrong segment char offset was stored in iterator");
        }
    }
  
  if (real->line_char_offset >= 0 && real->line_byte_offset >= 0)
    {
      if (byte_segment != char_segment)
        g_error("char and byte offsets did not point to the same segment");

      if (byte_any_segment != char_any_segment)
        g_error("char and byte offsets did not point to the same any segment");

      /* Make sure the segment offsets are equivalent, if it's a char
         segment. */
      if (char_segment->type == &gtk_text_char_type)
        {
          gint byte_offset = 0;
          gint char_offset = 0;
          while (char_offset < seg_char_offset)
            {
              const char * start = char_segment->body.chars + byte_offset;
              byte_offset += g_utf8_next_char (start) - start;
              char_offset += 1;
            }

          if (byte_offset != seg_byte_offset)
            g_error("byte offset did not correspond to char offset");

          char_offset =
            g_utf8_strlen (char_segment->body.chars, seg_byte_offset);

          if (char_offset != seg_char_offset)
            g_error("char offset did not correspond to byte offset");
        }
    }

  if (real->cached_line_number >= 0)
    {
      gint should_be;

      should_be = gtk_text_line_get_number(real->line);
      if (real->cached_line_number != should_be)
        g_error("wrong line number was cached");
    }

  if (real->cached_char_index >= 0)
    {
      if (real->line_char_offset >= 0) /* only way we can check it
                                          efficiently, not a real
                                          invariant. */
        {
          gint char_index;

          char_index = gtk_text_line_char_index(real->line);
          char_index += real->line_char_offset;

          if (real->cached_char_index != char_index)
            g_error("wrong char index was cached");
        }
    }
}

