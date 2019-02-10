#ifndef __GTK_DOUBLE_SELECTION_H__
#define __GTK_DOUBLE_SELECTION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_DOUBLE_SELECTION (gtk_double_selection_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkDoubleSelection, gtk_double_selection, GTK, DOUBLE_SELECTION, GObject)

GDK_AVAILABLE_IN_ALL
GtkDoubleSelection * gtk_double_selection_new  (GListModel *model);

G_END_DECLS

#endif /* __GTK_DOUBLE_SELECTION_H__ */
