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

/*
 * Display Chunks
 */
typedef struct _GtkTextDisplayChunk GtkTextDisplayChunk;


typedef enum {
  GTK_TEXT_DISPLAY_CHUNK_ELIDED,
  GTK_TEXT_DISPLAY_CHUNK_TEXT,
  GTK_TEXT_DISPLAY_CHUNK_CURSOR,
  GTK_TEXT_DISPLAY_CHUNK_PIXMAP
} GtkTextDisplayChunkType;

/*
 * The structure below represents a chunk of stuff that is displayed
 * together on the screen.  This structure is allocated and freed by
 * generic display code but most of its fields are filled in by
 * segment-type-specific code.
 */

struct _GtkTextDisplayChunk
{
  GtkTextDisplayChunkType type;
  
  struct _GtkTextDisplayChunk *next;	/* Next chunk in the display line
					 * or NULL for the end of the list. */
  GtkTextStyleValues *style;           /* Display information */

  int byte_count; /* total of all GtkTextLineSegment::size displayed here */
  
  int x;          /* X position in GtkTextLayout coordinates */
  
  int ascent;
  int descent;
  int height;
  int width;

   /* a byte offset where we could break the chunk */
  int break_byte_offset;

  union {
    struct {
      /* char data for GTK_TEXT_DISPLAY_CHUNK_TEXT */
      int byte_count;
      const gchar *text;
    } charinfo;
    struct {
      GdkPixmap *pixmap;
      GdkBitmap *mask;
    } pixmap;
  } d;
};

/*
 * The following structure describes one line of the display, which may
 * be either part or all of one line of the text. GtkTextLine is an
 * actual newline-separated text line. GtkTextDisplayLine is a line
 * after wrapping.
 */

typedef struct _GtkTextDisplayLine GtkTextDisplayLine;

struct _GtkTextDisplayLine
{  
  GtkTextLine *line; /* line we are displaying. */
  gint byte_offset;   /* offset into the line where we start. */

  GtkTextDisplayLine *next;	/* Next in list of all display lines for
				 * this window.   The list is sorted in
				 * order from top to bottom.  Note:  the
				 * next DLine doesn't always correspond
				 * to the next line of text:  (a) can have
				 * multiple DLines for one text line, and
				 * (b) can have gaps where DLine's have been
				 * deleted because they're out of date. */
  
  /* The following fields are computed during the line wrapping
     process. */
  int byte_count;		/* Number of bytes accounted for by
				 * this display line, possibly
				 * including a trailing space or
				 * newline that isn't actually
				 * displayed. */
  int height;			/* Height of line, in pixels. */
  int length;			/* Total length of line, in pixels. */
};

/* This information is computed via the relatively expensive line-wrap
   process (once we throw away display lines rather than keeping them
   around, it is probably pointless to have this struct separate from
   GtkTextDisplayLine) */
typedef struct _GtkTextDisplayLineWrapInfo GtkTextDisplayLineWrapInfo;

struct _GtkTextDisplayLineWrapInfo
{
  int baseline;	        	/* Offset of text baseline from y, in
                                 * pixels. */
  int space_above;		/* How much extra space was added to the
				 * top of the line because of spacing
				 * options.  This is included in height
				 * and baseline. */
  int space_below;		/* How much extra space was added to the
				 * bottom of the line because of spacing
				 * options.  This is included in height. */

  GtkTextDisplayChunk *chunks;	/* Pointer to first chunk in list of all
                                 * of those that are displayed on this
                                 * line of the screen. */
};

#define GTK_TYPE_TEXT_VIEW_LAYOUT (gtk_text_layout_get_type())
#define GTK_TEXT_LAYOUT(obj)  (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_VIEW_LAYOUT, GtkTextLayout))
#define GTK_TEXT_LAYOUT_CLASS(klass)  (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_VIEW_LAYOUT, GtkTextLayoutClass))
#define GTK_IS_TEXT_VIEW_LAYOUT(obj)  (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_VIEW_LAYOUT))
#define GTK_IS_TEXT_VIEW_LAYOUT_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_VIEW_LAYOUT))

typedef struct _GtkTextLayoutClass GtkTextLayoutClass;

struct _GtkTextLayout {
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

  /* A cache of one style; this is used to ensure
   * we don't constantly regenerate the style
   * over long runs with the same style. */
  GtkTextStyleValues *one_style_cache;

  /* Whether we are allowed to wrap right now */
  gint wrap_loop_count;
};

struct _GtkTextLayoutClass {
  GtkObjectClass parent_class;

  void (* need_repaint) (GtkTextLayout *layout,
                         gint x, gint y, gint width, gint height);

  
  GtkTextLineData *(* wrap) (GtkTextLayout *layout,
                              GtkTextLine *line,
                              /* may be NULL */
                              GtkTextLineData *line_data);
  
  void              (* invalidate) (GtkTextLayout *layout,
                                    const GtkTextIter *start,
                                    const GtkTextIter *end);  
};

GtkType        gtk_text_layout_get_type (void);
GtkTextLayout *gtk_text_layout_new      (void);

void gtk_text_layout_set_buffer            (GtkTextLayout      *layout,
					    GtkTextBuffer      *buffer);
void gtk_text_layout_set_default_style     (GtkTextLayout      *layout,
					    GtkTextStyleValues *values);
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
void gtk_text_layout_get_size (GtkTextLayout *layout,
			       gint          *width,
			       gint          *height);


GSList *gtk_text_layout_get_lines(GtkTextLayout *layout,
				  /* [top_y, bottom_y) */
				  gint top_y, 
				  gint bottom_y,
				  gint *first_line_y);

GtkTextDisplayLineWrapInfo *gtk_text_view_display_line_wrap   (GtkTextLayout              *layout,
							       GtkTextDisplayLine         *line);
void                        gtk_text_view_display_line_unwrap (GtkTextLayout              *layout,
							       GtkTextDisplayLine         *line,
							       GtkTextDisplayLineWrapInfo *wrapinfo);
void                        gtk_text_layout_wrap_loop_start   (GtkTextLayout              *layout);
void                        gtk_text_layout_wrap_loop_end     (GtkTextLayout              *layout);

void gtk_text_layout_get_iter_at_pixel (GtkTextLayout     *layout,
					GtkTextIter       *iter,
					gint               x,
					gint               y);
void gtk_text_layout_invalidate        (GtkTextLayout     *layout,
					const GtkTextIter *start,
					const GtkTextIter *end);

GtkTextLineData *gtk_text_layout_wrap (GtkTextLayout *layout,
				       GtkTextLine *line,
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
