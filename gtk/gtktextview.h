#ifndef GTK_TEXT_VIEW_H
#define GTK_TEXT_VIEW_H

#include <gtk/gtkcontainer.h>
#include <gtk/gtkimcontext.h>
#include <gtk/gtktextbuffer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  GTK_TEXT_MOVEMENT_CHAR,       /* move by forw/back chars */
  GTK_TEXT_MOVEMENT_POSITIONS,  /* move by left/right chars */
  GTK_TEXT_MOVEMENT_WORD,       /* move by forward/back words */
  GTK_TEXT_MOVEMENT_LINE,       /* move up/down lines (wrapped lines) */
  GTK_TEXT_MOVEMENT_PARAGRAPH,  /* move up/down paragraphs (newline-ended lines) */
  GTK_TEXT_MOVEMENT_PARAGRAPH_ENDS,   /* move to either end of a paragraph */
  GTK_TEXT_MOVEMENT_BUFFER_ENDS       /* move to ends of the buffer */
} GtkTextViewMovementStep;

typedef enum {
  GTK_TEXT_SCROLL_TO_TOP,
  GTK_TEXT_SCROLL_TO_BOTTOM,
  GTK_TEXT_SCROLL_PAGE_DOWN,
  GTK_TEXT_SCROLL_PAGE_UP
} GtkTextViewScrollType;

typedef enum {
  GTK_TEXT_DELETE_CHAR,
  GTK_TEXT_DELETE_HALF_WORD, /* delete only the portion of the word to the
                                 left/right of cursor if we're in the middle
                                 of a word */
  GTK_TEXT_DELETE_WHOLE_WORD,
  GTK_TEXT_DELETE_HALF_LINE,
  GTK_TEXT_DELETE_WHOLE_LINE,
  GTK_TEXT_DELETE_HALF_PARAGRAPH,  /* like C-k in Emacs (or its reverse) */
  GTK_TEXT_DELETE_WHOLE_PARAGRAPH, /* C-k in pico, kill whole line */
  GTK_TEXT_DELETE_WHITESPACE,      /* M-\ in Emacs */
  GTK_TEXT_DELETE_WHITESPACE_LEAVE_ONE /* M-space in Emacs */
} GtkTextViewDeleteType;

#define GTK_TYPE_TEXT_VIEW             (gtk_text_view_get_type())
#define GTK_TEXT_VIEW(obj)             (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_VIEW, GtkTextView))
#define GTK_TEXT_VIEW_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_VIEW, GtkTextViewClass))
#define GTK_IS_TEXT_VIEW(obj)          (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_VIEW))
#define GTK_IS_TEXT_VIEW_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_VIEW))
#define GTK_TEXT_VIEW_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_VIEW, GtkTextViewClass))

typedef struct _GtkTextView GtkTextView;
typedef struct _GtkTextViewClass GtkTextViewClass;

struct _GtkTextView {
  GtkContainer parent_instance;

  struct _GtkTextLayout *layout;
  GtkTextBuffer *buffer;

  guint selection_drag_handler;
  guint selection_drag_scan_timeout;
  gint scrolling_accel_factor;

  gboolean overwrite_mode;

  GtkWrapMode wrap_mode;	/* Default wrap mode */

  GdkWindow *bin_window;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  gint xoffset;			/* Offsets between widget coordinates and buffer coordinates */
  gint yoffset;
  gint width;			/* Width and height of the buffer */
  gint height;
  
  /* The virtual cursor position is normally the same as the
   * actual (strong) cursor position, except in two circumstances:
   *
   * a) When the cursor is moved vertically with the keyboard
   * b) When the text view is scrolled with the keyboard
   *
   * In case a), virtual_cursor_x is preserved, but not virtual_cursor_y
   * In case b), both virtual_cursor_x and virtual_cursor_y are preserved.
   */
  gint virtual_cursor_x;	   /* -1 means use actual cursor position */
  gint virtual_cursor_y;	   /* -1 means use actual cursor position */
  
  GtkTextMark *first_para_mark;	   /* Mark at the beginning of the first onscreen paragraph */
  gint first_para_pixels;	   /* Offset of top of screen in the first onscreen paragraph */

  GtkTextMark *dnd_mark;
  guint blink_timeout;

  guint first_validate_idle;     	/* Idle to revalidate onscreen portion, runs before resize */
  guint incremental_validate_idle;      /* Idle to revalidate offscreen portions, runs after redraw */

  GtkIMContext *im_context;
};

struct _GtkTextViewClass {
  GtkContainerClass parent_class;

  /* These are all RUN_ACTION signals for keybindings */

  /* move insertion point */
  void (* move_insert) (GtkTextView *text_view, GtkTextViewMovementStep step, gint count, gboolean extend_selection);
  /* move the "anchor" (what Emacs calls the mark) to the cursor position */
  void (* set_anchor)  (GtkTextView *text_view);
  /* Scroll */
  void (* scroll_text) (GtkTextView *text_view, GtkTextViewScrollType type);
  /* Deletions */
  void (* delete_text) (GtkTextView *text_view, GtkTextViewDeleteType type, gint count);
  /* cut copy paste */
  void (* cut_text)    (GtkTextView *text_view);
  void (* copy_text)    (GtkTextView *text_view);
  void (* paste_text)    (GtkTextView *text_view);
  /* overwrite */
  void (* toggle_overwrite) (GtkTextView *text_view);
  void  (*set_scroll_adjustments)   (GtkTextView    *text_view,
				     GtkAdjustment  *hadjustment,
				     GtkAdjustment  *vadjustment);
};

GtkType        gtk_text_view_get_type              (void);
GtkWidget *    gtk_text_view_new                   (void);
GtkWidget *    gtk_text_view_new_with_buffer       (GtkTextBuffer *buffer);
void           gtk_text_view_set_buffer            (GtkTextView   *text_view,
						    GtkTextBuffer *buffer);
GtkTextBuffer *gtk_text_view_get_buffer            (GtkTextView   *text_view);
void           gtk_text_view_get_iter_at_pixel     (GtkTextView   *text_view,
						    GtkTextIter   *iter,
						    gint           x,
						    gint           y);
gboolean       gtk_text_view_scroll_to_mark        (GtkTextView   *text_view,
                                                    GtkTextMark   *mark,
						    gint           mark_within_margin);
gboolean       gtk_text_view_move_mark_onscreen    (GtkTextView   *text_view,
                                                    GtkTextMark   *mark);
gboolean       gtk_text_view_place_cursor_onscreen (GtkTextView   *text_view);

void           gtk_text_view_get_visible_rect      (GtkTextView   *text_view,
						    GdkRectangle  *visible_rect);
void           gtk_text_view_set_wrap_mode         (GtkTextView   *text_view,
						    GtkWrapMode    wrap_mode);
GtkWrapMode    gtk_text_view_get_wrap_mode         (GtkTextView   *text_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GTK_TEXT_VIEW_H */
