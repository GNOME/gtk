#ifndef __GTK_CANVAS_POINT_PRIVATE_H__
#define __GTK_CANVAS_POINT_PRIVATE_H__

#include "gtkcanvaspoint.h"

G_BEGIN_DECLS

typedef struct _GtkCanvasPointClass GtkCanvasPointClass;
typedef struct _GtkCanvasPointBox GtkCanvasPointBox;
typedef struct _GtkCanvasPointItem GtkCanvasPointItem;
typedef struct _GtkCanvasPointOffset GtkCanvasPointOffset;

struct _GtkCanvasPointBox
{
  const GtkCanvasPointClass *class;

  GtkCanvasBox *box;
  float origin_x;
  float origin_y;
  float offset_x;
  float offset_y;
};

struct _GtkCanvasPointOffset
{
  const GtkCanvasPointClass *class;

  GtkCanvasPoint *other;
  float dx;
  float dy;
};

struct _GtkCanvasPoint
{
  union {
    const GtkCanvasPointClass *class;
    GtkCanvasPointBox box;
    GtkCanvasPointOffset offset;
  };
};


void                    gtk_canvas_point_init                   (GtkCanvasPoint *        point,
                                                                 float                   x,
                                                                 float                   y);
void                    gtk_canvas_point_init_copy              (GtkCanvasPoint         *self,
                                                                 const GtkCanvasPoint   *source);
void                    gtk_canvas_point_finish                 (GtkCanvasPoint         *self);

G_END_DECLS

#endif /* __GTK_CANVAS_POINT_PRIVATE_H__ */
