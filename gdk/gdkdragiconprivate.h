#ifndef __GDK_DRAG_ICON_PRIVATE_H__
#define __GDK_DRAG_ICON_PRIVATE_H__

#include "gdkdragicon.h"

G_BEGIN_DECLS


struct _GdkDragIconInterface
{
  GTypeInterface g_iface;

  gboolean      (* present)             (GdkDragIcon    *drag_icon,
                                         int             width,
                                         int             height);
};

G_END_DECLS

#endif /* __GDK_DRAG_ICON_PRIVATE_H__ */
