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
#include <gtk/gtktextbtree.h>


#define GTK_TYPE_TEXT_LAYOUT (gtk_text_layout_get_type())
#define GTK_TEXT_LAYOUT(obj)  (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_LAYOUT, GtkTextLayout))
#define GTK_TEXT_LAYOUT_CLASS(klass)  (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_LAYOUT, GtkTextLayoutClass))
#define GTK_IS_TEXT_LAYOUT(obj)  (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_LAYOUT))
#define GTK_IS_TEXT_LAYOUT_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_LAYOUT))

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
  GtkTextStyleValues *default_style;

  /* Pango context used for creating layouts */
  PangoContext *context;

  /* A cache of one style; this is used to ensure
   * we don't constantly regenerate the style
   * over long runs with the same style. */
  GtkTextStyleValues *one_style_cache;

  /* Whether we are allowed to wrap right now */
  gint wrap_loop_count;
};

struct _GtkTextLayoutClass
{
  GtkObjectClass parent_class;

  void (* need_repaint)     (GtkTextLayout *layout,
                             gint x, gint y, gint width, gint height);

  
  GtkTextLineData *(* wrap) (GtkTextLayout *layout,
                             GtkTextLine *line,
                             /* may be NULL */
                             GtkTextLineData *line_data);
  
  void       (* invalidate) (GtkTextLayout *layout,
                             const GtkTextIter *start,
                             const GtkTextIter *end);  
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

  gint width;
  gint height;
  gint x_offset;
  gint top_margin;
  gint bottom_margin;
};

extern PangoAttrType gtk_text_attr_appearance_type;

GtkType        gtk_text_layout_get_type (void);
GtkTextLayout *gtk_text_layout_new      (void);

void gtk_text_layout_set_buffer            (GtkTextLayout      *layout,
					    GtkTextBuffer      *buffer);
void gtk_text_layout_set_default_style     (GtkTextLayout      *layout,
					    GtkTextStyleValues *values);
void gtk_text_layout_set_context           (GtkTextLayout      *layout,
					    PangoContext       *context);
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
						       GtkTextLine        *line);
void                gtk_text_layout_free_line_display (GtkTextLayout      *layout,
						       GtkTextLine        *line,
						       GtkTextLineDisplay *display);

void gtk_text_layout_get_iter_at_pixel (GtkTextLayout     *layout,
					GtkTextIter       *iter,
					gint               x,
					gint               y);
void gtk_text_layout_invalidate        (GtkTextLayout     *layout,
					const GtkTextIter *start,
					const GtkTextIter *end);

GtkTextLineData *gtk_text_layout_wrap (GtkTextLayout   *layout,
				       GtkTextLine     *line,
                                         /* may be NULL */
				       GtkTextLineData *line_data);

void gtk_text_layout_need_repaint      (GtkTextLayout     *layout,
					gint               x,
					gint               y,
					gint               width,
					gint               height);
void gtk_text_layout_get_iter_location (GtkTextLayout     *layout,
					const GtkTextIter *iter,
					GdkRectangle      *rect);

void gtk_text_layout_spew(GtkTextLayout *layout);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
