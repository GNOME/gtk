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

/* ensure colors are allocated, etc. for drawing */
void                gtk_text_style_values_realize   (GtkTextStyleValues *values,
                                                     GdkColormap        *cmap,
                                                     GdkVisual          *visual);

/* free the stuff again */
void                gtk_text_style_values_unrealize (GtkTextStyleValues *values,
                                                     GdkColormap        *cmap,
                                                     GdkVisual          *visual);

#endif
