#pragma once

#include <gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_PATH (gtk_path_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkPath, gtk_path, GTK, PATH, GObject)

GDK_AVAILABLE_IN_ALL
GtkPath *               gtk_path_new                    (void);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_set_gsk_path           (GtkPath               *self,
                                                         GskPath               *gsk_path);

GDK_AVAILABLE_IN_ALL
GskPath *               gtk_path_get_gsk_path           (GtkPath               *self);

GDK_AVAILABLE_IN_ALL
gsize                   gtk_path_get_n_operations       (GtkPath               *self);

GDK_AVAILABLE_IN_ALL
GskPathOperation        gtk_path_get_operation          (GtkPath               *self,
                                                         gsize                  idx);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_set_operation          (GtkPath               *self,
                                                         gsize                  idx,
                                                         GskPathOperation       op);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_split_operation        (GtkPath                *self,
                                                         gsize                   idx,
                                                         float                   t);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_remove_operation       (GtkPath                *self,
                                                         gsize                   idx);

GDK_AVAILABLE_IN_ALL
gsize                   gtk_path_get_points_for_operation (GtkPath             *self,
                                                           gsize                idx);
GDK_AVAILABLE_IN_ALL
gsize                   gtk_path_get_n_points           (GtkPath               *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_get_point              (GtkPath               *self,
                                                         gsize                  idx,
                                                         graphene_point_t      *point);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_set_point              (GtkPath               *self,
                                                         gsize                  idx,
                                                         const graphene_point_t *point);

GDK_AVAILABLE_IN_ALL
gsize                   gtk_path_insert_point           (GtkPath                *self,
                                                         gsize                   idx,
                                                         const graphene_point_t *point);

GDK_AVAILABLE_IN_ALL
float                   gtk_path_get_conic_weight       (GtkPath                *self,
                                                         gsize                   idx);

GDK_AVAILABLE_IN_ALL
void                    gtk_path_set_conic_weight       (GtkPath                *self,
                                                         gsize                   idx,
                                                         float                   weight);

G_END_DECLS
