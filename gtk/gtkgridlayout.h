#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtklayoutmanager.h>

G_BEGIN_DECLS

#define GTK_TYPE_GRID_LAYOUT (gtk_grid_layout_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkGridLayout, gtk_grid_layout, GTK, GRID_LAYOUT, GtkLayoutManager)

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_grid_layout_new                             (void);

GDK_AVAILABLE_IN_ALL
void                    gtk_grid_layout_set_row_homogeneous             (GtkGridLayout       *grid_layout,
                                                                         gboolean             homogeneous);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_grid_layout_get_row_homogeneous             (GtkGridLayout       *grid_layout);
GDK_AVAILABLE_IN_ALL
void                    gtk_grid_layout_set_row_spacing                 (GtkGridLayout       *grid_layout,
                                                                         guint                spacing);
GDK_AVAILABLE_IN_ALL
guint                   gtk_grid_layout_get_row_spacing                 (GtkGridLayout       *grid_layout);
GDK_AVAILABLE_IN_ALL
void                    gtk_grid_layout_set_column_homogeneous          (GtkGridLayout       *grid_layout,
                                                                         gboolean             homogeneous);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_grid_layout_get_column_homogeneous          (GtkGridLayout       *grid_layout);
GDK_AVAILABLE_IN_ALL
void                    gtk_grid_layout_set_column_spacing              (GtkGridLayout       *grid_layout,
                                                                         guint                spacing);
GDK_AVAILABLE_IN_ALL
guint                   gtk_grid_layout_get_column_spacing              (GtkGridLayout       *grid_layout);
GDK_AVAILABLE_IN_ALL
void                    gtk_grid_layout_set_row_baseline_position       (GtkGridLayout       *grid_layout,
                                                                         int                  row,
                                                                         GtkBaselinePosition  pos);
GDK_AVAILABLE_IN_ALL
GtkBaselinePosition     gtk_grid_layout_get_row_baseline_position       (GtkGridLayout       *grid_layout,
                                                                         int                  row);
GDK_AVAILABLE_IN_ALL
void                    gtk_grid_layout_set_baseline_row                (GtkGridLayout       *grid_layout,
                                                                         int                  row);
GDK_AVAILABLE_IN_ALL
int                     gtk_grid_layout_get_baseline_row                (GtkGridLayout       *grid_layout);

#define GTK_TYPE_GRID_LAYOUT_CHILD (gtk_grid_layout_child_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkGridLayoutChild, gtk_grid_layout_child, GTK, GRID_LAYOUT_CHILD, GtkLayoutChild)

GDK_AVAILABLE_IN_ALL
int                     gtk_grid_layout_child_get_left_attach           (GtkGridLayoutChild  *grid_child);
GDK_AVAILABLE_IN_ALL
int                     gtk_grid_layout_child_get_top_attach            (GtkGridLayoutChild  *grid_child);
GDK_AVAILABLE_IN_ALL
int                     gtk_grid_layout_child_get_width                 (GtkGridLayoutChild  *grid_child);
GDK_AVAILABLE_IN_ALL
int                     gtk_grid_layout_child_get_height                (GtkGridLayoutChild  *grid_child);

G_END_DECLS
