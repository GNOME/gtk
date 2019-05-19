#ifndef __GTK_ROOT_PRIVATE_H__
#define __GTK_ROOT_PRIVATE_H__

#include "gtkroot.h"

G_BEGIN_DECLS

enum {
  GTK_ROOT_PROP_FOCUS_WIDGET,
  GTK_ROOT_NUM_PROPERTIES
} GtkRootProperties;

guint gtk_root_install_properties (GObjectClass *object_class,
                                   guint         first_prop);

G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
