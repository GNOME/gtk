#pragma once

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_LAYOUT_CHILD (gtk_layout_child_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkLayoutChild, gtk_layout_child, GTK, LAYOUT_CHILD, GObject)

struct _GtkLayoutChildClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_layout_child_get_layout_manager     (GtkLayoutChild *layout_child);
GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_layout_child_get_child_widget       (GtkLayoutChild *layout_child);

G_END_DECLS
