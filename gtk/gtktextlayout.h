#ifndef GTK_TEXT_LAYOUT_H
#define GTK_TEXT_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This is a "semi-private" header; it is intended for
 * use by the text widget, and the text canvas item,
 * but that's all. We may have to install it so the
 * canvas item can use it, but users are not supposed
 * to use it. 
 */

#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextiter.h>

/* forward declarations that have to be here to avoid including
 * gtktextbtree.h
 */
typedef struct _GtkTextLine     GtkTextLine;
typedef struct _GtkTextLineData GtkTextLineData;

#define GTK_TYPE_TEXT_LAYOUT             (gtk_text_layout_get_type())
#define GTK_TEXT_LAYOUT(obj)             (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_LAYOUT, GtkTextLayout))
#define GTK_TEXT_LAYOUT_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_LAYOUT, GtkTextLayoutClass))
#define GTK_IS_TEXT_LAYOUT(obj)          (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_LAYOUT))
#define GTK_IS_TEXT_LAYOUT_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_LAYOUT))
#define GTK_TEXT_LAYOUT_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_LAYOUT, GtkTextLayoutClass))

typedef struct _GtkTextLayout      GtkTextLayout;
typedef struct _GtkTextLayoutClass GtkTextLayoutClass;
typedef struct _GtkTextLineDisplay GtkTextLineDisplay;
typedef struct _GtkTextCursorDisplay GtkTextCursorDisplay;
typedef struct _GtkTextAttrAppearance GtkTextAttrAppearance;

struct _GtkTextLayout
{
  GtkObject parent_instance;

  /* width of the display area on-screen,
   * i.e. pixels we should wrap to fit inside. */
  gint screen_width;

  /* width/height of the total logical area being layed out */
  gint width;
  gint height;

  GtkTextBuffer *buffer;

  /* Default style used if no tags override it */
  GtkTextAttributes *default_style;

  /* Pango contexts used for creating layouts */
  PangoContext *ltr_context;
  PangoContext *rtl_context;

  /* A cache of one style; this is used to ensure
   * we don't constantly regenerate the style
   * over long runs with the same style. */
  GtkTextAttributes *one_style_cache;

  /* A cache of one line display. Getting the same line
   * many times in a row is the most common case.
   */
  GtkTextLineDisplay *one_display_cache;
  
  /* Whether we are allowed to wrap right now */
  gint wrap_loop_count;
};

struct _GtkTextLayoutClass
{
  GtkObjectClass parent_class;

  /* Some portion of the layout was invalidated
   */
  void (* invalidated) (GtkTextLayout *layout);

  /* A range of the layout changed appearance and possibly height
   */
  void (* changed)  (GtkTextLayout *layout,
		     gint           y,
		     gint           old_height,
		     gint           new_height);

  
  GtkTextLineData *(* wrap) (GtkTextLayout *layout,
                             GtkTextLine *line,
                             /* may be NULL */
                             GtkTextLineData *line_data);
  
  void (* get_log_attrs) (GtkTextLayout  *layout,
			  GtkTextLine    *line,
			  PangoLogAttr  **attrs,
			  gint           *n_attrs);
  void  (* invalidate)      (GtkTextLayout *layout,
                             const GtkTextIter *start,
                             const GtkTextIter *end);
  void  (* free_line_data) (GtkTextLayout   *layout,
			    GtkTextLine     *line,
			    GtkTextLineData *line_data);
};

struct _GtkTextAttrAppearance
{
  PangoAttribute attr;
  GtkTextAppearance appearance;
};

struct _GtkTextCursorDisplay
{
  gint x;
  gint y;
  gint height;
  guint is_strong : 1;
  guint is_weak : 1;
};

struct _GtkTextLineDisplay
{
  PangoLayout *layout;
  GSList *cursors;
  GSList *pixmaps;

  GtkTextDirection direction;

  gint width;			/* Width of layout */
  gint total_width;		/* width - margins, if no width set on layout, if width set on layout, -1 */
  gint height;
  gint x_offset;		/* Amount layout is shifted from left edge */
  gint left_margin;
  gint right_margin;
  gint top_margin;
  gint bottom_margin;

  gboolean size_only;
  GtkTextLine *line;
};

extern PangoAttrType gtk_text_attr_appearance_type;

GtkType        gtk_text_layout_get_type (void) G_GNUC_CONST;
GtkTextLayout *gtk_text_layout_new      (void);

void gtk_text_layout_set_buffer            (GtkTextLayout      *layout,
					    GtkTextBuffer      *buffer);
void gtk_text_layout_set_default_style     (GtkTextLayout      *layout,
					    GtkTextAttributes  *values);
void gtk_text_layout_set_contexts          (GtkTextLayout      *layout,
					    PangoContext       *ltr_context,
					    PangoContext       *rtl_context);
void gtk_text_layout_default_style_changed (GtkTextLayout      *layout);
void gtk_text_layout_set_screen_width      (GtkTextLayout      *layout,
					    gint                width);

/* Getting the size or the lines potentially results in a call to
 * recompute, which is pretty massively expensive. Thus it should
 * basically only be done in an idle handler.
 * 
 * Long-term, we would really like to be able to do these without
 * a full recompute so they may get cheaper over time.
 */
void    gtk_text_layout_get_size (GtkTextLayout  *layout,
	  		          gint           *width,
			          gint           *height);


GSList *gtk_text_layout_get_lines (GtkTextLayout *layout,
				   /* [top_y, bottom_y) */
				   gint           top_y, 
				   gint           bottom_y,
				   gint          *first_line_y);

void gtk_text_layout_wrap_loop_start (GtkTextLayout  *layout);
void gtk_text_layout_wrap_loop_end   (GtkTextLayout  *layout);

GtkTextLineDisplay *gtk_text_layout_get_line_display  (GtkTextLayout      *layout,
						       GtkTextLine        *line,
						       gboolean            size_only);
void                gtk_text_layout_free_line_display (GtkTextLayout      *layout,
						       GtkTextLineDisplay *display);

void gtk_text_layout_get_line_at_y     (GtkTextLayout     *layout,
					GtkTextIter       *target_iter,
					gint               y,
					gint              *line_top);
void gtk_text_layout_get_iter_at_pixel (GtkTextLayout     *layout,
					GtkTextIter       *iter,
					gint               x,
					gint               y);
void gtk_text_layout_invalidate        (GtkTextLayout     *layout,
					const GtkTextIter *start,
					const GtkTextIter *end);
void gtk_text_layout_free_line_data    (GtkTextLayout     *layout,
					GtkTextLine       *line,
					GtkTextLineData   *line_data);

gboolean gtk_text_layout_is_valid        (GtkTextLayout *layout);
void     gtk_text_layout_validate_yrange (GtkTextLayout *layout,
					  GtkTextIter   *anchor_line,
					  gint           y0,
					  gint           y1);
void     gtk_text_layout_validate        (GtkTextLayout *layout,
					  gint           max_pixels);

      /* This function should return the passed-in line data,
         OR remove the existing line data from the line, and
         return a NEW line data after adding it to the line.
         That is, invariant after calling the callback is that
         there should be exactly one line data for this view
         stored on the btree line. */
GtkTextLineData *gtk_text_layout_wrap (GtkTextLayout   *layout,
				       GtkTextLine     *line,
                                         /* may be NULL */
				       GtkTextLineData *line_data);
void     gtk_text_layout_get_log_attrs (GtkTextLayout  *layout,
					GtkTextLine    *line,
					PangoLogAttr  **attrs,
					gint           *n_attrs);
void     gtk_text_layout_changed              (GtkTextLayout     *layout,
					       gint               y,
					       gint               old_height,
					       gint               new_height);
void     gtk_text_layout_get_iter_location    (GtkTextLayout     *layout,
					       const GtkTextIter *iter,
					       GdkRectangle      *rect);
gint     gtk_text_layout_get_line_y           (GtkTextLayout     *layout,
					       const GtkTextIter *iter);
void     gtk_text_layout_get_cursor_locations (GtkTextLayout     *layout,
					       GtkTextIter       *iter,
					       GdkRectangle      *strong_pos,
					       GdkRectangle      *weak_pos);
gboolean gtk_text_layout_clamp_iter_to_vrange (GtkTextLayout     *layout,
					       GtkTextIter       *iter,
					       gint               top,
					       gint               bottom);

void gtk_text_layout_move_iter_to_previous_line (GtkTextLayout *layout,
						 GtkTextIter   *iter);
void gtk_text_layout_move_iter_to_next_line     (GtkTextLayout *layout,
						 GtkTextIter   *iter);
void gtk_text_layout_move_iter_to_x             (GtkTextLayout *layout,
						 GtkTextIter   *iter,
						 gint           x);
void gtk_text_layout_move_iter_visually         (GtkTextLayout *layout,
						 GtkTextIter   *iter,
						 gint           count);


void gtk_text_layout_spew (GtkTextLayout *layout);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
