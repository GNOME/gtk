#ifndef GTK_TEXT_CHILD_H
#define GTK_TEXT_CHILD_H

#include <gtk/gtktexttypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkTextPixbuf GtkTextPixbuf;

struct _GtkTextPixbuf {
  GdkPixbuf *pixbuf;
};

GtkTextLineSegment *gtk_text_pixbuf_segment_new(GdkPixbuf *pixbuf);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
