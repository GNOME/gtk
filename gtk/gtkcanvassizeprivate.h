#ifndef __GTK_CANVAS_SIZE_PRIVATE_H__
#define __GTK_CANVAS_SIZE_PRIVATE_H__

#include "gtkcanvassize.h"

#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GtkCanvasSizeClass GtkCanvasSizeClass;
typedef struct _GtkCanvasSizeAbsolute GtkCanvasSizeAbsolute;
typedef struct _GtkCanvasSizeBox GtkCanvasSizeBox;
typedef struct _GtkCanvasSizeDistance GtkCanvasSizeDistance;
typedef struct _GtkCanvasSizeMeasure GtkCanvasSizeMeasure;
typedef struct _GtkCanvasSizeReference GtkCanvasSizeReference;

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

struct _GtkCanvasSizeDistance
{
  const GtkCanvasSizeClass *class;

  GtkCanvasPoint *from;
  GtkCanvasPoint *to;
};

struct _GtkCanvasSizeMeasure
{
  const GtkCanvasSizeClass *class;

  GtkCanvasItem *item;
  GtkCanvasItemMeasurement measure;
};

struct _GtkCanvasSizeReference
{
  const GtkCanvasSizeClass *class;

  graphene_size_t *reference;
};

struct _GtkCanvasSize
{
  union {
    const GtkCanvasSizeClass *class;
    GtkCanvasSizeAbsolute absolute;
    GtkCanvasSizeBox box;
    GtkCanvasSizeDistance distance;
    GtkCanvasSizeMeasure measure;
    GtkCanvasSizeReference reference;
  };
};


void                    gtk_canvas_size_init_distance           (GtkCanvasSize          *size,
                                                                 const GtkCanvasPoint   *from,
                                                                 const GtkCanvasPoint   *to);
void                    gtk_canvas_size_init_measure_item       (GtkCanvasSize          *self,
                                                                 GtkCanvasItem          *item,
                                                                 GtkCanvasItemMeasurement measure);
void                    gtk_canvas_size_init_copy               (GtkCanvasSize         *self,
                                                                 const GtkCanvasSize   *source);
void                    gtk_canvas_size_finish                  (GtkCanvasSize         *self);

/* NB: Takes ownership of reference */
void                    gtk_canvas_size_init_reference          (GtkCanvasSize         *size,
                                                                 graphene_size_t       *reference);

G_END_DECLS

#endif /* __GTK_CANVAS_SIZE_PRIVATE_H__ */
