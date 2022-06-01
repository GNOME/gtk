#ifndef __GTK_CANVAS_BOX_PRIVATE_H__
#define __GTK_CANVAS_BOX_PRIVATE_H__

#include "gtkcanvasbox.h"

#include "gtkcanvaspointprivate.h"
#include "gtkcanvassizeprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCanvasBoxClass GtkCanvasBoxClass;
typedef struct _GtkCanvasBoxPoints GtkCanvasBoxPoints;
typedef struct _GtkCanvasBoxSize GtkCanvasBoxSize;

struct _GtkCanvasBoxPoints
{
  const GtkCanvasBoxClass *class;

  GtkCanvasPoint point1;
  GtkCanvasPoint point2;
};

struct _GtkCanvasBoxSize
{
  const GtkCanvasBoxClass *class;

  GtkCanvasPoint point;
  GtkCanvasSize size;
  float origin_x;
  float origin_y;
};

struct _GtkCanvasBox
{
  union {
    const GtkCanvasBoxClass *class;
    GtkCanvasBoxPoints points;
    GtkCanvasBoxSize size;
  };
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
