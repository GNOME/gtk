#ifndef __GSK_CAIRO_RENDERER_PRIVATE_H__
#define __GSK_CAIRO_RENDERER_PRIVATE_H__

#include <cairo.h>
#include <gsk/gskrenderer.h>

G_BEGIN_DECLS

#define GSK_TYPE_CAIRO_RENDERER (gsk_cairo_renderer_get_type ())

#define GSK_CAIRO_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_CAIRO_RENDERER, GskCairoRenderer))
#define GSK_IS_CAIRO_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_CAIRO_RENDERER))
#define GSK_CAIRO_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_CAIRO_RENDERER, GskCairoRendererClass))
#define GSK_IS_CAIRO_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_CAIRO_RENDERER))
#define GSK_CAIRO_RENDERER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_CAIRO_RENDERER, GskCairoRendererClass))

typedef struct _GskCairoRenderer                GskCairoRenderer;
typedef struct _GskCairoRendererClass           GskCairoRendererClass;

GType gsk_cairo_renderer_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GSK_CAIRO_RENDERER_PRIVATE_H__ */
