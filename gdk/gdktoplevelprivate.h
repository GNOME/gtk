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
};

G_END_DECLS

#endif /* __GDK_TOPLEVEL_PRIVATE_H__ */
