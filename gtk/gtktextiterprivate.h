#ifndef GTK_TEXT_ITER_PRIVATE_H
#define GTK_TEXT_ITER_PRIVATE_H

#include <gtk/gtktextiter.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkTextLineSegment *gtk_text_iter_get_indexable_segment(const GtkTextIter *iter);
GtkTextLineSegment *gtk_text_iter_get_any_segment(const GtkTextIter *iter);

GtkTextLine *gtk_text_iter_get_line(const GtkTextIter *iter);

GtkTextBTree *gtk_text_iter_get_btree(const GtkTextIter *iter);

gboolean gtk_text_iter_forward_indexable_segment(GtkTextIter *iter);
gboolean gtk_text_iter_backward_indexable_segment(GtkTextIter *iter);

gint gtk_text_iter_get_segment_byte(const GtkTextIter *iter);
gint gtk_text_iter_get_segment_char(const GtkTextIter *iter);

/* debug */
void gtk_text_iter_check(const GtkTextIter *iter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


