#ifndef __GTK_CANVAS_BOX_PRIVATE_H__
#define __GTK_CANVAS_BOX_PRIVATE_H__

#include "gtkcanvasbox.h"

#include "gtkcanvasvec2private.h"

G_BEGIN_DECLS

struct _GtkCanvasBox
{
  GtkCanvasVec2 point;
  GtkCanvasVec2 size;
  graphene_vec2_t origin;
};


void                    gtk_canvas_box_init                     (GtkCanvasBox           *self,
                                                                 const GtkCanvasVec2    *point,
                                                                 const GtkCanvasVec2    *size,
                                                                 const graphene_vec2_t  *origin);
void                    gtk_canvas_box_init_copy                (GtkCanvasBox           *self,
                                                                 const GtkCanvasBox     *source);
void                    gtk_canvas_box_finish                   (GtkCanvasBox           *self);

G_END_DECLS

#endif /* __GTK_CANVAS_BOX_PRIVATE_H__ */
