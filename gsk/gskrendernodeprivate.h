#ifndef __GSK_RENDER_NODE_PRIVATE_H__
#define __GSK_RENDER_NODE_PRIVATE_H__

#include "gskrendernode.h"
#include <cairo.h>

G_BEGIN_DECLS

/*<private>
 * GskMatrixCategory:
 * @GSK_MATRIX_CATEGORY_UNKNOWN: The category of the matrix has not been
 *     determined.
 * @GSK_MATRIX_CATEGORY_ANY: Analyzing the matrix concluded that it does
 *     not fit in any other category.
 * @GSK_MATRIX_CATEGORY_INVERTIBLE: The matrix is linear independant and
 *     should therefor be invertible. Note that this is not guaranteed
 *     to actually be true due to rounding errors when inverting.
 * @GSK_MATRIX_CATEGORY_2D: The matrix is a 2D matrix. This is equivalent
 *     to graphene_matrix_is_2d() returning %TRUE. In particular, this
 *     means that Cairo can deal with the matrix.
 * @GSK_MATRIX_CATEGORY_2D_AFFINE: The matrix is a combination of 2D scale
 *     and 2D translation operations. In particular, this means that any
 *     rectangle can be transformed exactly using this matrix.
 * @GSK_MATRIX_CATEGORY_2D_TRANSLATE: The matrix is a 2D translation.
 * @GSK_MATRIX_CATEGORY_IDENTITY: The matrix is the identity matrix.
 *
 * The categories of matrices relevant for GSK and GTK. Note that any
 * category includes matrices of all later categories. So if you want
 * to for example check if a matrix is a 2D matrix,
 * `category >= GSK_MATRIX_CATEGORY_2D` is the way to do this.
 *
 * Also keep in mind that rounding errors may cause matrices to not
 * conform to their categories. Otherwise, matrix operations done via
 * mutliplication will not worsen categories. So for the matrix
 * multiplication `C = A * B`, `category(C) = MIN (category(A), category(B))`.
 */
typedef enum
{
  GSK_MATRIX_CATEGORY_UNKNOWN,
  GSK_MATRIX_CATEGORY_ANY,
  GSK_MATRIX_CATEGORY_INVERTIBLE,
  GSK_MATRIX_CATEGORY_2D_AFFINE,
  GSK_MATRIX_CATEGORY_2D_TRANSLATE,
  GSK_MATRIX_CATEGORY_IDENTITY
} GskMatrixCategory;

typedef struct _GskRenderNodeClass GskRenderNodeClass;

#define GSK_IS_RENDER_NODE_TYPE(node,type) (GSK_IS_RENDER_NODE (node) && (node)->node_class->node_type == (type))

struct _GskRenderNode
{
  const GskRenderNodeClass *node_class;

  volatile int ref_count;

  graphene_rect_t bounds;
};

struct _GskRenderNodeClass
{
  GskRenderNodeType node_type;
  gsize struct_size;
  const char *type_name;

  void            (* finalize)    (GskRenderNode  *node);
  void            (* draw)        (GskRenderNode  *node,
                                   cairo_t        *cr);
  gboolean        (* can_diff)    (GskRenderNode  *node1,
                                   GskRenderNode  *node2);
  void            (* diff)        (GskRenderNode  *node1,
                                   GskRenderNode  *node2,
                                   cairo_region_t *region);
  GVariant *      (* serialize)   (GskRenderNode  *node);
  GskRenderNode * (* deserialize) (GVariant       *variant,
                                   GError        **error);
};

GskRenderNode * gsk_render_node_new              (const GskRenderNodeClass  *node_class,
                                                  gsize                      extra_size);

gboolean        gsk_render_node_can_diff         (GskRenderNode             *node1,
                                                  GskRenderNode             *node2);
void            gsk_render_node_diff             (GskRenderNode             *node1,
                                                  GskRenderNode             *node2,
                                                  cairo_region_t            *region);
void            gsk_render_node_diff_impossible  (GskRenderNode             *node1,
                                                  GskRenderNode             *node2,
                                                  cairo_region_t            *region);

GVariant *      gsk_render_node_serialize_node   (GskRenderNode             *node);
GskRenderNode * gsk_render_node_deserialize_node (GskRenderNodeType          type,
                                                  GVariant                  *variant,
                                                  GError                   **error);

GskRenderNode * gsk_cairo_node_new_for_surface   (const graphene_rect_t    *bounds,
                                                  cairo_surface_t          *surface);


G_END_DECLS

#endif /* __GSK_RENDER_NODE_PRIVATE_H__ */
