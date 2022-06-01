#ifndef __GTK_CANVAS_SIZE_PRIVATE_H__
#define __GTK_CANVAS_SIZE_PRIVATE_H__

#include "gtkcanvassize.h"

G_BEGIN_DECLS

typedef struct _GtkCanvasSizeClass GtkCanvasSizeClass;
typedef struct _GtkCanvasSizeAbsolute GtkCanvasSizeAbsolute;
typedef struct _GtkCanvasSizeBox GtkCanvasSizeBox;
typedef struct _GtkCanvasSizeMeasure GtkCanvasSizeMeasure;

struct _GtkCanvasSizeAbsolute
{
  const GtkCanvasSizeClass *class;

  float width;
  float height;
};

struct _GtkCanvasSizeBox
{
  const GtkCanvasSizeClass *class;

  GtkCanvasBox *box;
};

struct _GtkCanvasSizeMeasure
{
  const GtkCanvasSizeClass *class;

  GtkCanvasItem *item;
  GtkCanvasItemMeasurement measure;
};

struct _GtkCanvasSize
{
  union {
    const GtkCanvasSizeClass *class;
    GtkCanvasSizeAbsolute absolute;
    GtkCanvasSizeBox box;
    GtkCanvasSizeMeasure measure;
  };
};


void                    gtk_canvas_size_init_measure_item       (GtkCanvasSize          *self,
                                                                 GtkCanvasItem          *item,
                                                                 GtkCanvasItemMeasurement measure);
void                    gtk_canvas_size_init_copy               (GtkCanvasSize         *self,
                                                                 const GtkCanvasSize   *source);
void                    gtk_canvas_size_finish                  (GtkCanvasSize         *self);

G_END_DECLS

#endif /* __GTK_CANVAS_SIZE_PRIVATE_H__ */
