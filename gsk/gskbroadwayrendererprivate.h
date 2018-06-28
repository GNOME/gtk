#ifndef __GSK_BROADWAY_RENDERER_PRIVATE_H__
#define __GSK_BROADWAY_RENDERER_PRIVATE_H__

#include "broadway/gdkbroadway.h"
#include <gsk/gskrenderer.h>

G_BEGIN_DECLS

#define GSK_TYPE_BROADWAY_RENDERER (gsk_broadway_renderer_get_type ())

#define GSK_BROADWAY_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_BROADWAY_RENDERER, GskBroadwayRenderer))
#define GSK_IS_BROADWAY_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_BROADWAY_RENDERER))
#define GSK_BROADWAY_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_BROADWAY_RENDERER, GskBroadwayRendererClass))
#define GSK_IS_BROADWAY_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_BROADWAY_RENDERER))
#define GSK_BROADWAY_RENDERER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_BROADWAY_RENDERER, GskBroadwayRendererClass))

typedef struct _GskBroadwayRenderer                GskBroadwayRenderer;
typedef struct _GskBroadwayRendererClass           GskBroadwayRendererClass;

GType gsk_broadway_renderer_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GSK_BROADWAY_RENDERER_PRIVATE_H__ */
