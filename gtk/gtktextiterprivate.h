#ifndef __GTK_TEXT_ITER_PRIVATE_H__
#define __GTK_TEXT_ITER_PRIVATE_H__

#include <gtk/gtktextiter.h>

G_BEGIN_DECLS

#include <gtk/gtktextiter.h>
#include <gtk/gtktextbtree.h>

GtkTextLineSegment *_gtk_text_iter_get_indexable_segment      (const GtkTextIter *iter);
GtkTextLineSegment *_gtk_text_iter_get_any_segment            (const GtkTextIter *iter);
GtkTextLine *       _gtk_text_iter_get_text_line              (const GtkTextIter *iter);
GtkTextBTree *      _gtk_text_iter_get_btree                  (const GtkTextIter *iter);
gboolean            _gtk_text_iter_forward_indexable_segment  (GtkTextIter       *iter);
gboolean            _gtk_text_iter_backward_indexable_segment (GtkTextIter       *iter);
gint                _gtk_text_iter_get_segment_byte           (const GtkTextIter *iter);
gint                _gtk_text_iter_get_segment_char           (const GtkTextIter *iter);


/* debug */
void _gtk_text_iter_check (const GtkTextIter *iter);

G_END_DECLS

#endif


