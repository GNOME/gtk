#ifndef __PAINTABLE_H__
#define __PAINTABLE_H__

#include <gtk/gtk.h>

void            gtk_nuclear_snapshot           (GtkSnapshot     *snapshot,
                                                double           width,
                                                double           height,
                                                double           rotation);

GdkPaintable *  gtk_nuclear_icon_new            (double          rotation);
GdkPaintable *  gtk_nuclear_animation_new       (void);
GtkMediaStream *gtk_nuclear_media_stream_new    (void);

#endif /* __PAINTABLE_H__ */
