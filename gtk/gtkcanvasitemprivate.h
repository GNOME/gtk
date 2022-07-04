#ifndef __GTK_CANVAS_ITEM_PRIVATE_H__
#define __GTK_CANVAS_ITEM_PRIVATE_H__

#include "gtkcanvasitem.h"

#include "gtkcanvasvectorprivate.h"

G_BEGIN_DECLS

GtkCanvasItem *         gtk_canvas_item_new                      (GtkCanvas             *canvas,
                                                                  gpointer               item);

void                    gtk_canvas_item_validate_variables       (GtkCanvasItem         *self);
void                    gtk_canvas_item_allocate                 (GtkCanvasItem         *self,
                                                                  graphene_rect_t       *rect);
void                    gtk_canvas_item_allocate_widget          (GtkCanvasItem         *self,
                                                                  float                  dx,
                                                                  float                  dy);
gboolean                gtk_canvas_item_has_allocation           (GtkCanvasItem         *self);

void                    gtk_canvas_item_clear_canvas             (GtkCanvasItem         *self);

void                    gtk_canvas_item_setup                    (GtkCanvasItem         *self,
                                                                  GtkListItemFactory    *factory);
void                    gtk_canvas_item_teardown                 (GtkCanvasItem         *self,
                                                                  GtkListItemFactory    *factory);

G_END_DECLS

#endif /* __GTK_CANVAS_ITEM_PRIVATE_H__ */
