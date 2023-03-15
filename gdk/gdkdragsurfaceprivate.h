#ifndef __GDK_DRAG_SURFACE_PRIVATE_H__
#define __GDK_DRAG_SURFACE_PRIVATE_H__

#include "gdkdragsurface.h"
#include "gdkdragsurfacesize.h"

G_BEGIN_DECLS


struct _GdkDragSurfaceInterface
{
  GTypeInterface g_iface;

  gboolean (* present) (GdkDragSurface *drag_surface,
                        int             width,
                        int             height);
};

void gdk_drag_surface_notify_compute_size (GdkDragSurface     *surface,
                                           GdkDragSurfaceSize *size);

G_END_DECLS

#endif /* __GDK_DRAG_SURFACE_PRIVATE_H__ */
