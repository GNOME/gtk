#ifndef __GDK_DRAWING_CONTEXT_PRIVATE_H__
#define __GDK_DRAWING_CONTEXT_PRIVATE_H__

#include "gdkdrawingcontext.h"

G_BEGIN_DECLS

#define GDK_DRAWING_CONTEXT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWING_CONTEXT, GdkDrawingContextClass))
#define GDK_IS_DRAWING_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWING_CONTEXT))
#define GDK_DRAWING_CONTEXT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWING_CONTEXT, GdkDrawingContextClass))

struct _GdkDrawingContext
{
  GObject parent_instance;

  GdkWindow *window;

  cairo_region_t *clip;
  cairo_t *cr;
};

struct _GdkDrawingContextClass
{
  GObjectClass parent_instance;
};

G_END_DECLS

#endif /* __GDK_DRAWING_CONTEXT_PRIVATE_H__ */
