#ifndef GTK_TEXT_CHILD_H
#define GTK_TEXT_CHILD_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtktexttypes.h>

typedef struct _GtkTextPixmap GtkTextPixmap;

struct _GtkTextPixmap {
  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

GtkTextLineSegment *gtk_text_pixmap_segment_new(GdkPixmap *pixmap, GdkBitmap *mask);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
