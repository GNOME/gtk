#pragma once

#include <gtk/gtkenums.h>
#include <gtk/gtklayoutmanager.h>

G_BEGIN_DECLS

#define GTK_TYPE_BOX_LAYOUT (gtk_box_layout_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkBoxLayout, gtk_box_layout, GTK, BOX_LAYOUT, GtkLayoutManager)

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_box_layout_new              (GtkOrientation  orientation);

GDK_AVAILABLE_IN_ALL
void                    gtk_box_layout_set_homogeneous  (GtkBoxLayout   *box_layout,
                                                         gboolean        homogeneous);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_box_layout_get_homogeneous  (GtkBoxLayout   *box_layout);
GDK_AVAILABLE_IN_ALL
void                    gtk_box_layout_set_spacing      (GtkBoxLayout   *box_layout,
                                                         guint           spacing);
GDK_AVAILABLE_IN_ALL
guint                   gtk_box_layout_get_spacing      (GtkBoxLayout   *box_layout);

G_END_DECLS
