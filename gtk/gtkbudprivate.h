#ifndef __GTK_BUD_PRIVATE_H__
#define __GTK_BUD_PRIVATE_H__

#include "gtkbud.h"

G_BEGIN_DECLS

GskRenderer * gtk_bud_get_renderer          (GtkBud *self);

void          gtk_bud_get_surface_transform (GtkBud *self,
                                             int    *x,
                                             int    *y);

G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
