#ifndef __GTK_CANVAS_BOX_PRIVATE_H__
#define __GTK_CANVAS_BOX_PRIVATE_H__

#include "gtkcanvasbox.h"

#include "gtkcanvasvectorprivate.h"

G_BEGIN_DECLS

struct _GtkCanvasBox
{
  GtkCanvasVector point;
  GtkCanvasVector size;
  GtkCanvasVector origin;
};


void                    gtk_canvas_box_init                     (GtkCanvasBox           *self,
                                                                 const GtkCanvasVector  *point,
                                                                 const GtkCanvasVector  *size,
                                                                 const GtkCanvasVector  *origin);
void                    gtk_canvas_box_init_copy                (GtkCanvasBox           *self,
                                                                 const GtkCanvasBox     *source);
void                    gtk_canvas_box_finish                   (GtkCanvasBox           *self);

void                    gtk_canvas_box_init_variable            (GtkCanvasBox           *self);
void                    gtk_canvas_box_update_variable          (GtkCanvasBox           *self,
                                                                 const GtkCanvasBox     *other);

G_END_DECLS

#endif /* __GTK_CANVAS_BOX_PRIVATE_H__ */
