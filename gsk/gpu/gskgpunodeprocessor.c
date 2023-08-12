#include "config.h"

#include "gskgpunodeprocessorprivate.h"

#include "gskgpublitopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gskrendernodeprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

typedef struct _GskGpuNodeProcessor GskGpuNodeProcessor;

struct _GskGpuNodeProcessor
{
  GskGpuFrame           *frame;
  cairo_rectangle_int_t  scissor;
  graphene_point_t       offset;
  graphene_vec2_t        scale;
  GskTransform          *modelview;
  graphene_matrix_t      projection;
  /* GskGpuClip          clip; */
};

static void
gsk_gpu_node_processor_add_node (GskGpuNodeProcessor *self,
                                 GskGpuImage         *target,
                                 GskRenderNode       *node);

static void
gsk_gpu_node_processor_finish (GskGpuNodeProcessor *self)
{
}

static void
gsk_gpu_node_processor_init (GskGpuNodeProcessor         *self,
                             GskGpuFrame                 *frame,
                             gsize                        width,
                             gsize                        height,
                             const cairo_rectangle_int_t *clip,
                             const graphene_rect_t       *viewport)
{
  self->frame = frame;

  self->scissor = *clip;
  //gsk_vulkan_clip_init_empty (&state.clip, &GRAPHENE_RECT_INIT (0, 0, viewport->size.width, viewport->size.height));

  self->modelview = NULL;
  graphene_matrix_init_ortho (&self->projection,
                              0, width,
                              0, height,
                              2 * ORTHO_NEAR_PLANE - ORTHO_FAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_vec2_init (&self->scale, width / viewport->size.width,
                                    height / viewport->size.height);
  self->offset = GRAPHENE_POINT_INIT (-viewport->origin.x,
                                      -viewport->origin.y);

}

void
gsk_gpu_node_processor_process (GskGpuFrame                 *frame,
                                GskGpuImage                 *target,
                                const cairo_rectangle_int_t *clip,
                                GskRenderNode               *node,
                                const graphene_rect_t       *viewport)
{
  GskGpuNodeProcessor self;

  gsk_gpu_node_processor_init (&self,
                               frame,
                               gsk_gpu_image_get_width (target),
                               gsk_gpu_image_get_height (target),
                               clip,
                               viewport);

  gsk_gpu_node_processor_add_node (&self, target, node);

  gsk_gpu_node_processor_finish (&self);
}

static void
gsk_gpu_node_processor_add_node (GskGpuNodeProcessor *self,
                                 GskGpuImage         *target,
                                 GskRenderNode       *node)
{
  GskGpuImage *image;

  image = gsk_gpu_upload_cairo_op (self->frame,
                                   node,
                                   &self->scale,
                                   &node->bounds);

  gsk_gpu_blit_op (self->frame,
                   image,
                   target,
                   &(cairo_rectangle_int_t) {
                     0, 0,
                     gsk_gpu_image_get_width (image),
                     gsk_gpu_image_get_height (image)
                   },
                   &(cairo_rectangle_int_t) {
                     0, 0,
                     gsk_gpu_image_get_width (image),
                     gsk_gpu_image_get_height (image)
                   },
                   GSK_GPU_BLIT_LINEAR);
}
