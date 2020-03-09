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
  gboolean      (* minimize)            (GdkToplevel       *toplevel);
  gboolean      (* lower)               (GdkToplevel       *toplevel);
  void          (* focus)               (GdkToplevel       *toplevel,
                                         guint32            timestamp);
  gboolean      (* show_window_menu)    (GdkToplevel       *toplevel,
                                         GdkEvent          *event);
  gboolean      (* supports_edge_constraints) (GdkToplevel *toplevel);
};

typedef enum
{
  GDK_TOPLEVEL_PROP_STATE,
  GDK_TOPLEVEL_PROP_TITLE,
  GDK_TOPLEVEL_PROP_STARTUP_ID,
  GDK_TOPLEVEL_PROP_TRANSIENT_FOR,
  GDK_TOPLEVEL_PROP_MODAL,
  GDK_TOPLEVEL_PROP_ICON_LIST,
  GDK_TOPLEVEL_PROP_STICKY,
  GDK_TOPLEVEL_PROP_KEEP_ABOVE,
  GDK_TOPLEVEL_PROP_KEEP_BELOW,
  GDK_TOPLEVEL_PROP_ACCEPT_FOCUS,
  GDK_TOPLEVEL_PROP_FOCUS_ON_MAP,
  GDK_TOPLEVEL_PROP_DECORATED,
  GDK_TOPLEVEL_PROP_DELETABLE,
  GDK_TOPLEVEL_PROP_FULLSCREEN_MODE,
  GDK_TOPLEVEL_NUM_PROPERTIES
} GdkToplevelProperties;

guint gdk_toplevel_install_properties (GObjectClass *object_class,
                                       guint         first_prop);

G_END_DECLS

#endif /* __GDK_TOPLEVEL_PRIVATE_H__ */
