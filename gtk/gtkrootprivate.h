#ifndef __GTK_ROOT_PRIVATE_H__
#define __GTK_ROOT_PRIVATE_H__

#include "gtkroot.h"

G_BEGIN_DECLS

GdkDisplay *            gtk_root_get_display            (GtkRoot                *root);
GskRenderer *           gtk_root_get_renderer           (GtkRoot                *self);

void                    gtk_root_get_surface_transform  (GtkRoot                *self,
                                                         int                    *x,
                                                         int                    *y);
GtkWidget *             gtk_root_get_focus              (GtkRoot                *self);
GtkWidget *             gtk_root_get_default            (GtkRoot                *self);
G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
