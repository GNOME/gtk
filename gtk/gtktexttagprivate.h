#ifndef GTK_TEXT_TAG_PRIVATE_H
#define GTK_TEXT_TAG_PRIVATE_H

#include <gtk/gtktexttag.h>

/* values should already have desired defaults; this function will override
 * the defaults with settings in the given tags, which should be sorted in
 * ascending order of priority
*/
void gtk_text_style_values_fill_from_tags (GtkTextStyleValues  *values,
                                           GtkTextTag         **tags,
                                           guint                n_tags);
void gtk_text_tag_array_sort              (GtkTextTag         **tag_array_p,
                                           guint                len);

/*
 * Style object created by folding a set of tags together
 */

typedef struct _GtkTextAppearance GtkTextAppearance;

struct _GtkTextAppearance
{
  GdkColor bg_color;
  GdkColor fg_color;
  GdkBitmap *bg_stipple;
  GdkBitmap *fg_stipple;

  guint underline : 4;		/* PangoUnderline */
  guint overstrike : 1;

  /* Whether to use background-related values; this is irrelevant for
   * the values struct when in a tag, but is used for the composite
   * values struct; it's true if any of the tags being composited
   * had background stuff set. */
  guint draw_bg : 1;

  /* This is only used when we are actually laying out and rendering
   * a paragraph; not when a GtkTextAppearance is part of a
   * GtkTextStyleValues.
   */
  guint inside_selection : 1;
};

struct _GtkTextStyleValues
{
  guint refcount;

  GtkTextAppearance appearance;
  
  gint border_width;
  GtkShadowType relief;
  GtkJustification justify;
  GtkTextDirection direction;
  
  PangoFontDescription *font_desc;
  
  /* lMargin1 */
  gint left_margin;
  
  /* lMargin2 */
  gint left_wrapped_line_margin;

  /* super/subscript offset, can be negative */
  gint offset;
  
  gint right_margin;

  gint pixels_above_lines;

  gint pixels_below_lines;

  gint pixels_inside_wrap;

  GtkTextTabArray *tab_array;
  
  GtkWrapMode wrap_mode;	/* How to handle wrap-around for this tag.
				 * Must be GTK_WRAPMODE_CHAR,
				 * GTK_WRAPMODE_NONE, GTK_WRAPMODE_WORD
                                 */

  gchar *language;
  
  /* hide the text  */
  guint elide : 1;

  /* Background is fit to full line height rather than
   * baseline +/- ascent/descent (font height) */
  guint bg_full_height : 1;
  
  /* can edit this text */
  guint editable : 1;

  /* colors are allocated etc. */
  guint realized : 1;

  guint pad1 : 1;
  guint pad2 : 1;
  guint pad3 : 1;
  guint pad4 : 1;
};

GtkTextStyleValues *gtk_text_style_values_new       (void);
void                gtk_text_style_values_copy      (GtkTextStyleValues *src,
                                                     GtkTextStyleValues *dest);
void                gtk_text_style_values_unref     (GtkTextStyleValues *values);
void                gtk_text_style_values_ref       (GtkTextStyleValues *values);

/* ensure colors are allocated, etc. for drawing */
void                gtk_text_style_values_realize   (GtkTextStyleValues *values,
                                                     GdkColormap        *cmap,
                                                     GdkVisual          *visual);

/* free the stuff again */
void                gtk_text_style_values_unrealize (GtkTextStyleValues *values,
                                                     GdkColormap        *cmap,
                                                     GdkVisual          *visual);

#endif
