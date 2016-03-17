#ifndef __GSK_RENDER_NODE_ITER_H__
#define __GSK_RENDER_NODE_ITER_H__

#include <gsk/gskrendernode.h>

G_BEGIN_DECLS

#define GSK_TYPE_RENDER_NODE_ITER (gsk_render_node_iter_get_type())

typedef struct _GskRenderNodeIter	GskRenderNodeIter;

struct _GskRenderNodeIter
{
  /*< private >*/
  gpointer dummy1;
  gpointer dummy2;
  gint64 dummy3;
  gpointer dummy4;
  gpointer dummy5;
};

GDK_AVAILABLE_IN_3_22
GType gsk_render_node_iter_get_type (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_22
GskRenderNodeIter *     gsk_render_node_iter_new        (void);
GDK_AVAILABLE_IN_3_22
void                    gsk_render_node_iter_free       (GskRenderNodeIter *iter);
GDK_AVAILABLE_IN_3_22
void                    gsk_render_node_iter_init       (GskRenderNodeIter *iter,
                                                         GskRenderNode     *node);

GDK_AVAILABLE_IN_3_22
gboolean                gsk_render_node_iter_is_valid   (GskRenderNodeIter *iter);
GDK_AVAILABLE_IN_3_22
gboolean                gsk_render_node_iter_prev       (GskRenderNodeIter  *iter,
                                                         GskRenderNode     **child);
GDK_AVAILABLE_IN_3_22
gboolean                gsk_render_node_iter_next       (GskRenderNodeIter  *iter,
                                                         GskRenderNode     **child);
GDK_AVAILABLE_IN_3_22
void                    gsk_render_node_iter_remove     (GskRenderNodeIter *iter);

G_END_DECLS

#endif /* GSK_RENDER_NODE_ITER_H */
