#ifndef __GTK_ROOT_PRIVATE_H__
#define __GTK_ROOT_PRIVATE_H__

#include "gtkroot.h"

G_BEGIN_DECLS

GdkDisplay *            gtk_root_get_display            (GtkRoot                *root);

void                    gtk_root_get_surface_transform  (GtkRoot                *self,
                                                         int                    *x,
                                                         int                    *y);
G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
