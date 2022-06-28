#ifndef __GTK_CANVAS_BOX_PRIVATE_H__
#define __GTK_CANVAS_BOX_PRIVATE_H__

#include "gtkcanvasbox.h"

#include "gtkcanvaspointprivate.h"
#include "gtkcanvassizeprivate.h"

G_BEGIN_DECLS

struct _GtkCanvasBox
{
  GtkCanvasPoint point;
  GtkCanvasSize size;
  float origin_x;
  float origin_y;
};


void                    gtk_canvas_box_init                     (GtkCanvasBox           *self,
                                                                 const GtkCanvasPoint   *point,
                                                                 const GtkCanvasSize    *size,
                                                                 float                   origin_x,
                                                                 float                   origin_y);
void                    gtk_canvas_box_init_copy                (GtkCanvasBox           *self,
                                                                 const GtkCanvasBox     *source);
void                    gtk_canvas_box_finish                   (GtkCanvasBox           *self);

G_END_DECLS

#endif /* __GTK_CANVAS_BOX_PRIVATE_H__ */
