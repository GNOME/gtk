#ifndef GTK_TEXT_BTREE_H
#define GTK_TEXT_BTREE_H

#include <gtk/gtktextbuffer.h>
#include <gtk/gtktexttag.h>
#include <gtk/gtktextmark.h>
#include <gtk/gtktextchild.h>
#include <gtk/gtktextsegment.h>
#include <gtk/gtktextiter.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkTextBTree  *gtk_text_btree_new        (GtkTextTagTable *table,
                                          GtkTextBuffer   *buffer);
void           gtk_text_btree_ref        (GtkTextBTree    *tree);
void           gtk_text_btree_unref      (GtkTextBTree    *tree);
GtkTextBuffer *gtk_text_btree_get_buffer (GtkTextBTree    *tree);


guint gtk_text_btree_get_chars_changed_stamp    (GtkTextBTree *tree);
guint gtk_text_btree_get_segments_changed_stamp (GtkTextBTree *tree);
void  gtk_text_btree_segments_changed           (GtkTextBTree *tree);


/* Indexable segment mutation */

void gtk_text_btree_delete        (GtkTextIter *start,
                                   GtkTextIter *end);
void gtk_text_btree_insert        (GtkTextIter *iter,
                                   const gchar *text,
                                   gint         len);
void gtk_text_btree_insert_pixbuf (GtkTextIter *iter,
                                   GdkPixbuf   *pixbuf);



/* View stuff */
GtkTextLine *gtk_text_btree_find_line_by_y    (GtkTextBTree      *tree,
					       gpointer           view_id,
					       gint               ypixel,
					       gint              *line_top_y);
gint         gtk_text_btree_find_line_top     (GtkTextBTree      *tree,
					       GtkTextLine       *line,
					       gpointer           view_id);
void         gtk_text_btree_add_view          (GtkTextBTree      *tree,
					       GtkTextLayout     *layout);
void         gtk_text_btree_remove_view       (GtkTextBTree      *tree,
					       gpointer           view_id);
void         gtk_text_btree_invalidate_region (GtkTextBTree      *tree,
					       const GtkTextIter *start,
					       const GtkTextIter *end);
void         gtk_text_btree_get_view_size     (GtkTextBTree      *tree,
					       gpointer           view_id,
					       gint              *width,
					       gint              *height);
gboolean     gtk_text_btree_is_valid          (GtkTextBTree      *tree,
					       gpointer           view_id);
gboolean     gtk_text_btree_validate          (GtkTextBTree      *tree,
					       gpointer           view_id,
					       gint               max_pixels,
					       gint              *y,
					       gint              *old_height,
					       gint              *new_height);
void         gtk_text_btree_validate_line     (GtkTextBTree      *tree,
					       GtkTextLine       *line,
					       gpointer           view_id);

/* Tag */

void gtk_text_btree_tag (const GtkTextIter *start,
                         const GtkTextIter *end,
                         GtkTextTag        *tag,
                         gboolean           apply);

/* "Getters" */

GtkTextLine * gtk_text_btree_get_line          (GtkTextBTree      *tree,
                                                gint               line_number,
                                                gint              *real_line_number);
GtkTextLine * gtk_text_btree_get_line_at_char  (GtkTextBTree      *tree,
                                                gint               char_index,
                                                gint              *line_start_index,
                                                gint              *real_char_index);
GtkTextTag**  gtk_text_btree_get_tags          (const GtkTextIter *iter,
                                                gint              *num_tags);
gchar        *gtk_text_btree_get_text          (const GtkTextIter *start,
                                                const GtkTextIter *end,
                                                gboolean           include_hidden,
                                                gboolean           include_nonchars);
gint          gtk_text_btree_line_count        (GtkTextBTree      *tree);
gint          gtk_text_btree_char_count        (GtkTextBTree      *tree);
gboolean      gtk_text_btree_char_is_invisible (const GtkTextIter *iter);



/* Get iterators (these are implemented in gtktextiter.c) */
void     gtk_text_btree_get_iter_at_char         (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  gint                char_index);
void     gtk_text_btree_get_iter_at_line_char    (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  gint                line_number,
                                                  gint                char_index);
void     gtk_text_btree_get_iter_at_line_byte    (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  gint                line_number,
                                                  gint                byte_index);
gboolean gtk_text_btree_get_iter_from_string     (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  const gchar        *string);
gboolean gtk_text_btree_get_iter_at_mark_name    (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  const gchar        *mark_name);
void     gtk_text_btree_get_iter_at_mark         (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  GtkTextMark        *mark);
void     gtk_text_btree_get_last_iter            (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter);
void     gtk_text_btree_get_iter_at_line         (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  GtkTextLine        *line,
                                                  gint                byte_offset);
gboolean gtk_text_btree_get_iter_at_first_toggle (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  GtkTextTag         *tag);
gboolean gtk_text_btree_get_iter_at_last_toggle  (GtkTextBTree       *tree,
                                                  GtkTextIter        *iter,
                                                  GtkTextTag         *tag);


/* Manipulate marks */
GtkTextMark        *gtk_text_btree_set_mark                (GtkTextBTree       *tree,
                                                            GtkTextMark         *existing_mark,
							    const gchar        *name,
							    gboolean            left_gravity,
							    const GtkTextIter  *index,
                                                            gboolean           should_exist);
void                gtk_text_btree_remove_mark_by_name     (GtkTextBTree       *tree,
							    const gchar        *name);
void                gtk_text_btree_remove_mark             (GtkTextBTree       *tree,
							    GtkTextMark        *segment);
gboolean            gtk_text_btree_get_selection_bounds    (GtkTextBTree       *tree,
							    GtkTextIter        *start,
							    GtkTextIter        *end);
void                gtk_text_btree_place_cursor            (GtkTextBTree       *tree,
							    const GtkTextIter  *where);
gboolean            gtk_text_btree_mark_is_insert          (GtkTextBTree       *tree,
							    GtkTextMark        *segment);
gboolean            gtk_text_btree_mark_is_selection_bound (GtkTextBTree       *tree,
							    GtkTextMark        *segment);
GtkTextMark        *gtk_text_btree_get_mark_by_name        (GtkTextBTree       *tree,
							    const gchar        *name);
GtkTextLine *       gtk_text_btree_first_could_contain_tag (GtkTextBTree       *tree,
							    GtkTextTag         *tag);
GtkTextLine *       gtk_text_btree_last_could_contain_tag  (GtkTextBTree       *tree,
							    GtkTextTag         *tag);

/* Lines */

/* Chunk of data associated with a line; views can use this to store
   info at the line. They should "subclass" the header struct here. */
struct _GtkTextLineData {
  gpointer view_id;
  GtkTextLineData *next;
  gint height;
  gint width : 24;
  gint valid : 8;
};

/*
 * The data structure below defines a single line of text (from newline
 * to newline, not necessarily what appears on one line of the screen).
 *
 * You can consider this line a "paragraph" also
 */

struct _GtkTextLine {
  GtkTextBTreeNode *parent;		/* Pointer to parent node containing
                                 * line. */
  GtkTextLine *next;		/* Next in linked list of lines with
                                 * same parent node in B-tree.  NULL
                                 * means end of list. */
  GtkTextLineSegment *segments;	/* First in ordered list of segments
                                 * that make up the line. */
  GtkTextLineData *views;      /* data stored here by views */
};


gint                gtk_text_line_get_number                 (GtkTextLine         *line);
gboolean            gtk_text_line_char_has_tag               (GtkTextLine         *line,
                                                              GtkTextBTree        *tree,
                                                              gint                 char_in_line,
                                                              GtkTextTag          *tag);
gboolean            gtk_text_line_byte_has_tag               (GtkTextLine         *line,
                                                              GtkTextBTree        *tree,
                                                              gint                 byte_in_line,
                                                              GtkTextTag          *tag);
gboolean            gtk_text_line_is_last                    (GtkTextLine  *line,
                                                              GtkTextBTree *tree);
GtkTextLine *       gtk_text_line_next                       (GtkTextLine         *line);
GtkTextLine *       gtk_text_line_previous                   (GtkTextLine         *line);
void                gtk_text_line_add_data                   (GtkTextLine         *line,
                                                              GtkTextLineData     *data);
gpointer            gtk_text_line_remove_data                (GtkTextLine         *line,
                                                              gpointer             view_id);
gpointer            gtk_text_line_get_data                   (GtkTextLine         *line,
                                                              gpointer             view_id);
void                gtk_text_line_invalidate_wrap            (GtkTextLine         *line,
                                                              GtkTextLineData     *ld);
gint                gtk_text_line_char_count                 (GtkTextLine         *line);
gint                gtk_text_line_byte_count                 (GtkTextLine         *line);
gint                gtk_text_line_char_index                 (GtkTextLine         *line);
GtkTextLineSegment *gtk_text_line_byte_to_segment            (GtkTextLine         *line,
                                                              gint                 byte_offset,
                                                              gint                *seg_offset);
GtkTextLineSegment *gtk_text_line_char_to_segment            (GtkTextLine         *line,
                                                              gint                 char_offset,
                                                              gint                *seg_offset);
void                gtk_text_line_byte_locate                (GtkTextLine         *line,
                                                              gint                 byte_offset,
                                                              GtkTextLineSegment **segment,
                                                              GtkTextLineSegment **any_segment,
                                                              gint                *seg_byte_offset,
                                                              gint                *line_byte_offset);
void                gtk_text_line_char_locate                (GtkTextLine         *line,
                                                              gint                 char_offset,
                                                              GtkTextLineSegment **segment,
                                                              GtkTextLineSegment **any_segment,
                                                              gint                *seg_char_offset,
                                                              gint                *line_char_offset);
void                gtk_text_line_byte_to_char_offsets       (GtkTextLine         *line,
                                                              gint                 byte_offset,
                                                              gint                *line_char_offset,
                                                              gint                *seg_char_offset);
void                gtk_text_line_char_to_byte_offsets       (GtkTextLine         *line,
                                                              gint                 char_offset,
                                                              gint                *line_byte_offset,
                                                              gint                *seg_byte_offset);
GtkTextLineSegment *gtk_text_line_byte_to_any_segment        (GtkTextLine         *line,
                                                              gint                 byte_offset,
                                                              gint                *seg_offset);
GtkTextLineSegment *gtk_text_line_char_to_any_segment        (GtkTextLine         *line,
                                                              gint                 char_offset,
                                                              gint                *seg_offset);
gint                gtk_text_line_byte_to_char               (GtkTextLine         *line,
                                                              gint                 byte_offset);
gint                gtk_text_line_char_to_byte               (GtkTextLine         *line,
                                                              gint                 char_offset);
GtkTextLine    *    gtk_text_line_next_could_contain_tag     (GtkTextLine         *line,
                                                              GtkTextBTree        *tree,
                                                              GtkTextTag          *tag);
GtkTextLine    *    gtk_text_line_previous_could_contain_tag (GtkTextLine         *line,
                                                              GtkTextBTree        *tree,
                                                              GtkTextTag          *tag);


/* Debug */
void gtk_text_btree_check (GtkTextBTree *tree);
void gtk_text_btree_spew (GtkTextBTree *tree);
extern gboolean gtk_text_view_debug_btree;

/* ignore, exported only for gtktextsegment.c */
void toggle_segment_check_func (GtkTextLineSegment *segPtr,
                                GtkTextLine        *line);
void change_node_toggle_count  (GtkTextBTreeNode   *node,
                                GtkTextTagInfo     *info,
                                gint                delta);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


