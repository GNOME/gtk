#ifndef GTK_TEXT_VIEW_H
#define GTK_TEXT_VIEW_H

#include <gtk/gtkcontainer.h>
#include <gtk/gtkimcontext.h>
#include <gtk/gtktextbuffer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TEXT_VIEW             (gtk_text_view_get_type())
#define GTK_TEXT_VIEW(obj)             (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_VIEW, GtkTextView))
#define GTK_TEXT_VIEW_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_VIEW, GtkTextViewClass))
#define GTK_IS_TEXT_VIEW(obj)          (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_VIEW))
#define GTK_IS_TEXT_VIEW_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_VIEW))
#define GTK_TEXT_VIEW_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_VIEW, GtkTextViewClass))

typedef enum
{
  GTK_TEXT_WINDOW_WIDGET,
  GTK_TEXT_WINDOW_TEXT,
  GTK_TEXT_WINDOW_LEFT,
  GTK_TEXT_WINDOW_RIGHT,
  GTK_TEXT_WINDOW_TOP,
  GTK_TEXT_WINDOW_BOTTOM
} GtkTextWindowType;

typedef struct _GtkTextView GtkTextView;
typedef struct _GtkTextViewClass GtkTextViewClass;

/* Internal private type. */
typedef struct _GtkTextWindow GtkTextWindow;

struct _GtkTextView {
  GtkContainer parent_instance;

  struct _GtkTextLayout *layout;
  GtkTextBuffer *buffer;

  guint selection_drag_handler;
  guint selection_drag_scan_timeout;
  gint scrolling_accel_factor;

  gboolean overwrite_mode;

  GtkWrapMode wrap_mode;	/* Default wrap mode */

  gboolean editable;            /* default editability */

  gboolean cursor_visible;
  
  GtkTextWindow *text_window;
  GtkTextWindow *left_window;
  GtkTextWindow *right_window;
  GtkTextWindow *top_window;
  GtkTextWindow *bottom_window;
  
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
  void (* move)        (GtkTextView *text_view, GtkMovementStep step, gint count, gboolean extend_selection);
  /* move the "anchor" (what Emacs calls the mark) to the cursor position */
  void (* set_anchor)  (GtkTextView *text_view);
  /* Deletions */
  void (* insert)      (GtkTextView *text_view, const gchar *str);
  void (* delete)      (GtkTextView *text_view, GtkDeleteType type, gint count);
  /* cut copy paste */
  void (* cut_clipboard)   (GtkTextView *text_view);
  void (* copy_clipboard)  (GtkTextView *text_view);
  void (* paste_clipboard) (GtkTextView *text_view);
  /* overwrite */
  void (* toggle_overwrite) (GtkTextView *text_view);
  void (* set_scroll_adjustments)   (GtkTextView    *text_view,
				     GtkAdjustment  *hadjustment,
				     GtkAdjustment  *vadjustment);
};

GtkType        gtk_text_view_get_type              (void) G_GNUC_CONST;
GtkWidget *    gtk_text_view_new                   (void);
GtkWidget *    gtk_text_view_new_with_buffer       (GtkTextBuffer *buffer);
void           gtk_text_view_set_buffer            (GtkTextView   *text_view,
						    GtkTextBuffer *buffer);
GtkTextBuffer *gtk_text_view_get_buffer            (GtkTextView   *text_view);
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

void           gtk_text_view_set_editable          (GtkTextView   *text_view,
                                                    gboolean       setting);
gboolean       gtk_text_view_get_editable          (GtkTextView   *text_view);

void           gtk_text_view_set_cursor_visible    (GtkTextView   *text_view,
                                                    gboolean       setting);
gboolean       gtk_text_view_get_cursor_visible    (GtkTextView   *text_view);

void           gtk_text_view_get_iter_location     (GtkTextView   *text_view,
                                                    const GtkTextIter *iter,
                                                    GdkRectangle  *location);
void           gtk_text_view_get_iter_at_location  (GtkTextView   *text_view,
						    GtkTextIter   *iter,
						    gint           x,
						    gint           y);


void gtk_text_view_buffer_to_window_coords (GtkTextView       *text_view,
                                            GtkTextWindowType  win,
                                            gint               buffer_x,
                                            gint               buffer_y,
                                            gint              *window_x,
                                            gint              *window_y);
void gtk_text_view_window_to_buffer_coords (GtkTextView       *text_view,
                                            GtkTextWindowType  win,
                                            gint               window_x,
                                            gint               window_y,
                                            gint              *buffer_x,
                                            gint              *buffer_y);

GdkWindow*        gtk_text_view_get_window      (GtkTextView       *text_view,
                                                 GtkTextWindowType  win);
GtkTextWindowType gtk_text_view_get_window_type (GtkTextView       *text_view,
                                                 GdkWindow         *window);

void gtk_text_view_set_left_window_width    (GtkTextView *text_view,
                                             gint         width);
void gtk_text_view_set_right_window_width   (GtkTextView *text_view,
                                             gint         width);
void gtk_text_view_set_top_window_height    (GtkTextView *text_view,
                                             gint         height);
void gtk_text_view_set_bottom_window_height (GtkTextView *text_view,
                                             gint         height);

void gtk_text_view_set_text_window_size     (GtkTextView *text_view,
                                             gint         width,
                                             gint         height);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GTK_TEXT_VIEW_H */
