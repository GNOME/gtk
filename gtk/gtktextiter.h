#ifndef GTK_TEXT_ITER_H
#define GTK_TEXT_ITER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Iter: represents a location in the text. Becomes invalid if the
 * characters/pixmaps/widgets (indexable objects) in the text buffer
 * are changed.
 */

#include <gtk/gtktexttag.h>

typedef struct _GtkTextBuffer GtkTextBuffer;

struct _GtkTextIter {
  gpointer dummy1;
  gpointer dummy2;
  gint dummy3;
  gint dummy4;
  gint dummy10;
  gint dummy11;
  gint dummy5;
  gint dummy6;
  gpointer dummy7;
  gpointer dummy8;
  gint dummy9;
  gpointer pad1;
  gint pad2;
};


/* This is primarily intended for language bindings that want to avoid
   a "buffer" argument to text insertions, deletions, etc. */
GtkTextBuffer *gtk_text_iter_get_buffer(const GtkTextIter *iter);

/*
 * Life cycle
 */

GtkTextIter *gtk_text_iter_copy     (const GtkTextIter *iter);
void         gtk_text_iter_free     (GtkTextIter       *iter);

/*
 * Convert to different kinds of index
 */
gint     gtk_text_iter_get_char_index  (const GtkTextIter *iter);
gint     gtk_text_iter_get_line_number (const GtkTextIter *iter);
gint     gtk_text_iter_get_line_char   (const GtkTextIter *iter);
gint     gtk_text_iter_get_line_byte   (const GtkTextIter *iter);

/*
 * "Dereference" operators
 */
gunichar gtk_text_iter_get_char          (const GtkTextIter  *iter);

/* includes the 0xFFFD char for pixmaps/widgets, so char offsets
   into the returned string map properly into buffer char offsets */
gchar   *gtk_text_iter_get_slice         (const GtkTextIter  *start,
                                           const GtkTextIter  *end);

/* includes only text, no 0xFFFD */
gchar   *gtk_text_iter_get_text          (const GtkTextIter  *start,
                                           const GtkTextIter  *end);
/* exclude invisible chars */
gchar   *gtk_text_iter_get_visible_slice (const GtkTextIter  *start,
                                           const GtkTextIter  *end);
gchar   *gtk_text_iter_get_visible_text  (const GtkTextIter  *start,
                                           const GtkTextIter  *end);

/* Returns TRUE if the iterator pointed at a pixmap */
gboolean gtk_text_iter_get_pixmap        (const GtkTextIter  *iter,
                                           GdkPixmap          **pixmap,
                                           GdkBitmap          **mask);

/* Return list of tags toggled at this point (toggled_on determines
   whether the list is of on-toggles or off-toggles) */
GSList  *gtk_text_iter_get_toggled_tags  (const GtkTextIter  *iter,
                                           gboolean             toggled_on);

gboolean gtk_text_iter_begins_tag        (const GtkTextIter  *iter,
                                           GtkTextTag         *tag);

gboolean gtk_text_iter_ends_tag          (const GtkTextIter  *iter,
                                           GtkTextTag         *tag);

gboolean gtk_text_iter_toggles_tag       (const GtkTextIter  *iter,
                                           GtkTextTag         *tag);

gboolean gtk_text_iter_has_tag           (const GtkTextIter   *iter,
                                           GtkTextTag          *tag);

gboolean gtk_text_iter_starts_line       (const GtkTextIter   *iter);
gboolean gtk_text_iter_ends_line         (const GtkTextIter   *iter);

gint     gtk_text_iter_get_chars_in_line (const GtkTextIter   *iter);

/*
 * Moving around the buffer
 */
gboolean gtk_text_iter_forward_char    (GtkTextIter       *iter);
gboolean gtk_text_iter_backward_char   (GtkTextIter       *iter);
gboolean gtk_text_iter_forward_chars   (GtkTextIter       *iter,
                                         gint                count);
gboolean gtk_text_iter_backward_chars  (GtkTextIter       *iter,
                                         gint                count);
gboolean gtk_text_iter_forward_line    (GtkTextIter       *iter);
gboolean gtk_text_iter_backward_line   (GtkTextIter       *iter);
gboolean gtk_text_iter_forward_lines   (GtkTextIter       *iter,
                                         gint                count);
gboolean gtk_text_iter_backward_lines  (GtkTextIter       *iter,
                                         gint                count);
gboolean gtk_text_iter_forward_word_ends(GtkTextIter      *iter,
                                          gint               count);
gboolean gtk_text_iter_backward_word_starts(GtkTextIter      *iter,
                                             gint               count);
gboolean gtk_text_iter_forward_word_end(GtkTextIter      *iter);
gboolean gtk_text_iter_backward_word_start(GtkTextIter      *iter);

gboolean gtk_text_iter_up_lines        (GtkTextIter *iter,
                                         gint count);

gboolean gtk_text_iter_down_lines        (GtkTextIter *iter,
                                           gint count);

void     gtk_text_iter_set_char_index  (GtkTextIter       *iter,
                                         gint                char_index);
void     gtk_text_iter_set_line_number (GtkTextIter       *iter,
                                         gint                line_number);
void     gtk_text_iter_set_line_char   (GtkTextIter       *iter,
                                         gint                char_on_line);

void     gtk_text_iter_forward_to_end  (GtkTextIter       *iter);
gboolean gtk_text_iter_forward_to_newline(GtkTextIter     *iter);

/* returns TRUE if a toggle was found; NULL for the tag pointer
   means "any tag toggle", otherwise the next toggle of the
   specified tag is located. */
gboolean gtk_text_iter_forward_find_tag_toggle (GtkTextIter *iter,
                                                 GtkTextTag  *tag);

gboolean gtk_text_iter_backward_find_tag_toggle (GtkTextIter *iter,
                                                  GtkTextTag  *tag);

typedef gboolean (* GtkTextViewCharPredicate) (gunichar ch, gpointer user_data);

gboolean gtk_text_iter_forward_find_char      (GtkTextIter *iter,
					       GtkTextViewCharPredicate pred,
					       gpointer user_data);

gboolean gtk_text_iter_backward_find_char     (GtkTextIter *iter,
					       GtkTextViewCharPredicate pred,
					       gpointer user_data);

/*
 * Comparisons
 */
gboolean gtk_text_iter_equal           (const GtkTextIter *lhs,
                                         const GtkTextIter *rhs);
gint     gtk_text_iter_compare         (const GtkTextIter *lhs,
                                         const GtkTextIter *rhs);
gboolean gtk_text_iter_in_region       (const GtkTextIter *iter,
                                         const GtkTextIter *start,
                                         const GtkTextIter *end);

/* Put these two in ascending order */
void     gtk_text_iter_reorder         (GtkTextIter *first,
                                         GtkTextIter *second);

/* Debug */
void     gtk_text_iter_spew            (const GtkTextIter *iter,
                                         const gchar *desc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


