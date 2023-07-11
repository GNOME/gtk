#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_CONTOUR (gtk_contour_get_type ())
G_DECLARE_FINAL_TYPE (GtkContour, gtk_contour, GTK, CONTOUR, GObject)

GDK_AVAILABLE_IN_ALL
GtkContour *     gtk_contour_new                (const graphene_point_t *start);

GDK_AVAILABLE_IN_ALL
unsigned int    gtk_contour_get_n_curves        (GtkContour             *self);

GDK_AVAILABLE_IN_ALL
unsigned int    gtk_contour_get_n_points        (GtkContour             *self);

GDK_AVAILABLE_IN_ALL
gboolean        gtk_contour_is_closed           (GtkContour             *self);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_close               (GtkContour             *self);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_set_point           (GtkContour             *self,
                                                 unsigned int            pos,
                                                 const graphene_point_t *point);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_get_point           (GtkContour             *self,
                                                 unsigned int            pos,
                                                 graphene_point_t       *point);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_get_curve           (GtkContour             *self,
                                                 unsigned int            pos,
                                                 GskPathOperation       *op,
                                                 graphene_point_t        pts[4],
                                                 float                  *weight);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_set_curve           (GtkContour             *self,
                                                 unsigned int            pos,
                                                 GskPathOperation        op,
                                                 graphene_point_t        pts[4],
                                                 float                   weight);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_set_line            (GtkContour             *self,
                                                 unsigned int            pos);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_set_quad            (GtkContour             *self,
                                                 unsigned int            pos,
                                                 const graphene_point_t *cp);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_set_cubic           (GtkContour             *self,
                                                 unsigned int            pos,
                                                 const graphene_point_t *cp1,
                                                 const graphene_point_t *cp2);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_set_conic           (GtkContour             *self,
                                                 unsigned int            pos,
                                                 const graphene_point_t *cp,
                                                 float                   weight);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_line_to             (GtkContour             *self,
                                                 const graphene_point_t *end);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_quad_to             (GtkContour             *self,
                                                 const graphene_point_t *cp,
                                                 const graphene_point_t *end);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_cubic_to            (GtkContour             *self,
                                                 const graphene_point_t *cp1,
                                                 const graphene_point_t *cp2,
                                                 const graphene_point_t *end);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_conic_to            (GtkContour             *self,
                                                 const graphene_point_t *cp,
                                                 float                   weight,
                                                 const graphene_point_t *end);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_append              (GtkContour             *self,
                                                 GtkContour             *contour);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_split               (GtkContour             *self,
                                                 unsigned int            pos,
                                                 GtkContour            **contour1,
                                                 GtkContour            **contour2);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_insert_point        (GtkContour             *self,
                                                 unsigned int            idx,
                                                 float                   t);

GDK_AVAILABLE_IN_ALL
gboolean        gtk_contour_find_closest_curve  (GtkContour             *self,
                                                 const graphene_point_t *point,
                                                 float                   threshold,
                                                 unsigned int           *idx,
                                                 graphene_point_t       *p,
                                                 float                  *pos);

GDK_AVAILABLE_IN_ALL
void            gtk_contour_add_to_path_builder (GtkContour             *self,
                                                 GskPathBuilder         *builder);

G_END_DECLS
