#ifndef GTK_TEXT_DISPLAY_H
#define GTK_TEXT_DISPLAY_H

#include <gtk/gtktextlayout.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
  A semi-public header intended for use by code that also
  uses GtkTextLayout
*/

/* The drawable should be pre-initialized to your preferred
   background. */
void gtk_text_layout_draw (GtkTextLayout *layout,
                            /* Widget to grab some style info from */
                            GtkWidget *widget, 
                            /* Drawable to render to */
                            GdkDrawable *drawable,
                            /* Position of the drawable
                               in layout coordinates */
                            gint x_offset,
                            gint y_offset,
                            /* Region of the layout to
                               render. x,y must be inside
                               the drawable. */
                            gint x,
                            gint y,
                            gint width,
                            gint height);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
