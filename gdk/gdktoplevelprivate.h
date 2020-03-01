#ifndef __GDK_TOPLEVEL_PRIVATE_H__
#define __GDK_TOPLEVEL_PRIVATE_H__

#include "gdktoplevel.h"

G_BEGIN_DECLS


struct _GdkToplevelInterface
{
  GTypeInterface g_iface;

  gboolean      (* present)             (GdkToplevel       *toplevel,
                                         int                width,
                                         int                height,
                                         GdkToplevelLayout *layout);

  GdkSurfaceState (* get_state)         (GdkToplevel       *toplevel);

  void            (* set_title)         (GdkToplevel       *toplevel,
                                         const char        *title);

  void            (* set_startup_id)    (GdkToplevel       *toplevel,
                                         const char        *startup_id);

  void            (* set_transient_for) (GdkToplevel       *toplevel,
                                         GdkSurface        *surface);

  void            (* set_icon_list)     (GdkToplevel       *toplevel,
                                         GList             *surfaces);
};

G_END_DECLS

#endif /* __GDK_TOPLEVEL_PRIVATE_H__ */
