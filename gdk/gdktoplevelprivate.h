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

typedef enum {
  GDK_TOPLEVEL_PROP_STATE,
  GDK_TOPLEVEL_PROP_TITLE,
  GDK_TOPLEVEL_PROP_STARTUP_ID,
  GDK_TOPLEVEL_PROP_TRANSIENT_FOR,
  GDK_TOPLEVEL_PROP_ICON_LIST,
  GDK_TOPLEVEL_NUM_PROPERTIES
} GdkToplevelProperties;

guint gdk_toplevel_install_properties (GObjectClass *object_class,
                                       guint         first_prop);

G_END_DECLS

#endif /* __GDK_TOPLEVEL_PRIVATE_H__ */
