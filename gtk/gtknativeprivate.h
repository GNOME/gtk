#ifndef __GTK_NATIVE_PRIVATE_H__
#define __GTK_NATIVE_PRIVATE_H__

#include "gtknative.h"

G_BEGIN_DECLS

void          gtk_native_get_surface_transform (GtkNative *self,
                                                int       *x,
                                                int       *y);

G_END_DECLS

#endif /* __GTK_NATIVE_PRIVATE_H__ */
