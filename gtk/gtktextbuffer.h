#ifndef GTK_TEXT_BUFFER_H
#define GTK_TEXT_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * This is the PUBLIC representation of a text buffer.
 * GtkTextBTree is the PRIVATE internal representation of it.
 */

#include <gtk/gtkwidget.h>
#include <gtk/gtktexttagtable.h>
#include <gtk/gtktextiter.h>
#include <gtk/gtktextmark.h>

typedef struct _GtkTextBTree GtkTextBTree;

#define GTK_TYPE_TEXT_BUFFER            (gtk_text_buffer_get_type())
#define GTK_TEXT_BUFFER(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_BUFFER, GtkTextBuffer))
#define GTK_TEXT_BUFFER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_BUFFER, GtkTextBufferClass))
#define GTK_IS_TEXT_BUFFER(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_BUFFER))
#define GTK_IS_TEXT_BUFFER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_BUFFER))
#define GTK_TEXT_BUFFER_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_BUFFER, GtkTextBufferClass))

typedef struct _GtkTextBufferClass GtkTextBufferClass;

struct _GtkTextBuffer {
  GtkObject parent_instance;

  GtkTextTagTable *tag_table;
  GtkTextBTree *tree;

  /* Text currently pasted to the clipboard */
  gchar *clipboard_text;

  /* Whether the buffer has been modified since last save */
  gboolean modified;

  /* We use this for selections */
  GtkWidget *selection_widget;
  gboolean have_selection;
  gboolean selection_handlers_installed;
};

struct _GtkTextBufferClass {
  GtkObjectClass parent_class;

  void (* insert_text)     (GtkTextBuffer *buffer,
                            GtkTextIter *pos,
                            const gchar *text,
                            gint length);


  void (* delete_text)     (GtkTextBuffer *buffer,
                            GtkTextIter *start,
                            GtkTextIter *end);

  /* Only for text changed, marks/tags don't cause this
     to be emitted */
  void (* changed)         (GtkTextBuffer *buffer);


  /* New value for the modified flag */
  void (* modified_changed)   (GtkTextBuffer *buffer);

  /* Mark moved or created */
  void (* mark_set)           (GtkTextBuffer *buffer,
                               const GtkTextIter *location,
                               GtkTextMark *mark);

  void (* mark_deleted)       (GtkTextBuffer *buffer,
                               GtkTextMark *mark);

  void (* apply_tag)          (GtkTextBuffer *buffer,
                               GtkTextTag *tag,
                               const GtkTextIter *start_char,
                               const GtkTextIter *end_char);

  void (* remove_tag)         (GtkTextBuffer *buffer,
                               GtkTextTag *tag,
                               const GtkTextIter *start_char,
                               const GtkTextIter *end_char);

};

GtkType        gtk_text_buffer_get_type       (void);



/* table is NULL to create a new one */
GtkTextBuffer *gtk_text_buffer_new            (GtkTextTagTable *table);
gint           gtk_text_buffer_get_line_count (GtkTextBuffer   *buffer);
gint           gtk_text_buffer_get_char_count (GtkTextBuffer   *buffer);



/* Insert into the buffer */
void gtk_text_buffer_insert            (GtkTextBuffer *buffer,
                                        GtkTextIter   *iter,
                                        const gchar   *text,
                                        gint           len);
void gtk_text_buffer_insert_at_cursor  (GtkTextBuffer *buffer,
                                        const gchar   *text,
                                        gint           len);
void gtk_text_buffer_insert_at_char    (GtkTextBuffer *buffer,
                                        gint           char_pos,
                                        const gchar   *text,
                                        gint           len);
void gtk_text_buffer_insert_after_line (GtkTextBuffer *buffer,
                                        gint           after_this_line,
                                        const gchar   *text,
                                        gint           len);



/* Delete from the buffer */

void gtk_text_buffer_delete           (GtkTextBuffer *buffer,
                                       GtkTextIter   *start_iter,
                                       GtkTextIter   *end_iter);
void gtk_text_buffer_delete_chars     (GtkTextBuffer *buffer,
                                       gint           start_char,
                                       gint           end_char);
void gtk_text_buffer_delete_lines     (GtkTextBuffer *buffer,
                                       gint           start_line,
                                       gint           end_line);
void gtk_text_buffer_delete_from_line (GtkTextBuffer *buffer,
                                       gint           line,
                                       gint           start_char,
                                       gint           end_char);
/* Obtain strings from the buffer */
gchar          *gtk_text_buffer_get_text            (GtkTextBuffer     *buffer,
                                                     const GtkTextIter *start_iter,
                                                     const GtkTextIter *end_iter,
                                                     gboolean           include_hidden_chars);
gchar          *gtk_text_buffer_get_text_chars      (GtkTextBuffer     *buffer,
                                                     gint               start_char,
                                                     gint               end_char,
                                                     gboolean           include_hidden_chars);
gchar          *gtk_text_buffer_get_text_from_line  (GtkTextBuffer     *buffer,
                                                     gint               line,
                                                     gint               start_char,
                                                     gint               end_char,
                                                     gboolean           include_hidden_chars);
gchar          *gtk_text_buffer_get_slice           (GtkTextBuffer     *buffer,
                                                     const GtkTextIter *start_iter,
                                                     const GtkTextIter *end_iter,
                                                     gboolean           include_hidden_chars);
gchar          *gtk_text_buffer_get_slice_chars     (GtkTextBuffer     *buffer,
                                                     gint               start_char,
                                                     gint               end_char,
                                                     gboolean           include_hidden_chars);
gchar          *gtk_text_buffer_get_slice_from_line (GtkTextBuffer     *buffer,
                                                     gint               line,
                                                     gint               start_char,
                                                     gint               end_char,
                                                     gboolean           include_hidden_chars);

/* Insert a pixmap */
void gtk_text_buffer_insert_pixmap         (GtkTextBuffer *buffer,
                                            GtkTextIter   *iter,
                                            GdkPixmap     *pixmap,
                                            GdkBitmap     *mask);
void gtk_text_buffer_insert_pixmap_at_char (GtkTextBuffer *buffer,
                                            gint           char_index,
                                            GdkPixmap     *pixmap,
                                            GdkBitmap     *mask);



/* Mark manipulation */
GtkTextMark   *gtk_text_buffer_create_mark (GtkTextBuffer     *buffer,
                                            const gchar       *mark_name,
                                            const GtkTextIter *where,
                                            gboolean           left_gravity);
void           gtk_text_buffer_move_mark   (GtkTextBuffer     *buffer,
                                            GtkTextMark       *mark,
                                            const GtkTextIter *where);
void           gtk_text_buffer_delete_mark (GtkTextBuffer     *buffer,
                                            GtkTextMark       *mark);
GtkTextMark   *gtk_text_buffer_get_mark    (GtkTextBuffer     *buffer,
                                            const gchar       *name);


/* efficiently move insert and selection_bound to same location */
void gtk_text_buffer_place_cursor (GtkTextBuffer     *buffer,
                                   const GtkTextIter *where);



/* Tag manipulation */
void gtk_text_buffer_apply_tag_to_chars    (GtkTextBuffer     *buffer,
                                            const gchar       *name,
                                            gint               start_char,
                                            gint               end_char);
void gtk_text_buffer_remove_tag_from_chars (GtkTextBuffer     *buffer,
                                            const gchar       *name,
                                            gint               start_char,
                                            gint               end_char);
void gtk_text_buffer_apply_tag             (GtkTextBuffer     *buffer,
                                            const gchar       *name,
                                            const GtkTextIter *start_index,
                                            const GtkTextIter *end_index);
void gtk_text_buffer_remove_tag            (GtkTextBuffer     *buffer,
                                            const gchar       *name,
                                            const GtkTextIter *start_index,
                                            const GtkTextIter *end_index);



/* You can either ignore the return value, or use it to
   set the attributes of the tag */
GtkTextTag    *gtk_text_buffer_create_tag (GtkTextBuffer *buffer,
                                           const gchar   *tag_name);

/* Obtain iterators pointed at various places, then you can move the
   iterator around using the GtkTextIter operators */

void     gtk_text_buffer_get_iter_at_line_char (GtkTextBuffer *buffer,
                                                GtkTextIter   *iter,
                                                gint           line_number,
                                                gint           char_number);
void     gtk_text_buffer_get_iter_at_char      (GtkTextBuffer *buffer,
                                                GtkTextIter   *iter,
                                                gint           char_index);
void     gtk_text_buffer_get_iter_at_line      (GtkTextBuffer *buffer,
                                                GtkTextIter   *iter,
                                                gint           line_number);
void     gtk_text_buffer_get_last_iter         (GtkTextBuffer *buffer,
                                                GtkTextIter   *iter);
void     gtk_text_buffer_get_bounds            (GtkTextBuffer *buffer,
                                                GtkTextIter   *start,
                                                GtkTextIter   *end);
void     gtk_text_buffer_get_iter_at_mark      (GtkTextBuffer *buffer,
                                                GtkTextIter   *iter,
                                                GtkTextMark   *mark);


/* There's no get_first_iter because you just get the iter for
   line or char 0 */

/*
  Parses a string, read the man page for the Tk text widget; the only
  variation from that is we don't support getting the index at a
  certain pixel since the buffer has no pixel knowledge.  This
  function is mostly useful for language bindings I think.
*/
gboolean gtk_text_buffer_get_iter_from_string (GtkTextBuffer *buffer,
                                               GtkTextIter   *iter,
                                               const gchar   *iter_string);



GSList         *gtk_text_buffer_get_tags (GtkTextBuffer     *buffer,
                                          const GtkTextIter *iter);


/* Used to keep track of whether the buffer needs saving; anytime the
   buffer contents change, the modified flag is turned on. Whenever
   you save, turn it off. Tags and marks do not affect the modified
   flag, but if you would like them to you can connect a handler to
   the tag/mark signals and call set_modified in your handler */

gboolean        gtk_text_buffer_modified                (GtkTextBuffer *buffer);
void            gtk_text_buffer_set_modified            (GtkTextBuffer *buffer,
                                                         gboolean       setting);
void            gtk_text_buffer_set_clipboard_contents  (GtkTextBuffer *buffer,
                                                         const gchar   *text);
const gchar    *gtk_text_buffer_get_clipboard_contents  (GtkTextBuffer *buffer);
void            gtk_text_buffer_paste_primary_selection (GtkTextBuffer *buffer,
                                                         GtkTextIter   *override_location,
                                                         guint32        time);
gboolean        gtk_text_buffer_delete_selection        (GtkTextBuffer *buffer);
void            gtk_text_buffer_cut                     (GtkTextBuffer *buffer,
                                                         guint32        time);
void            gtk_text_buffer_copy                    (GtkTextBuffer *buffer,
                                                         guint32        time);
void            gtk_text_buffer_paste_clipboard         (GtkTextBuffer *buffer,
                                                         guint32        time);
gboolean        gtk_text_buffer_get_selection_bounds    (GtkTextBuffer *buffer,
                                                         GtkTextIter   *start,
                                                         GtkTextIter   *end);


/* This function is not implemented. */
gboolean gtk_text_buffer_find_string(GtkTextBuffer *buffer,
                                     GtkTextIter *iter,
                                     const gchar *str,
                                     const GtkTextIter *start,
                                     const GtkTextIter *end);

#if 0
/* Waiting on glib 1.4 regexp facility */
gboolean gtk_text_buffer_find_regexp(GtkTextBuffer *buffer,
                                     GRegexp *regexp,
                                     const GtkTextIter *start,
                                     const GtkTextIter *end);
#endif

/* INTERNAL private stuff */
void            gtk_text_buffer_spew                   (GtkTextBuffer      *buffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
