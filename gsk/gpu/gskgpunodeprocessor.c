#include "config.h"

#include "gskgpunodeprocessorprivate.h"

#include "gskgpuclipprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgputextureopprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gskdebugprivate.h"
#include "gskrendernodeprivate.h"
#include "gskroundedrectprivate.h"
#include "gsktransformprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

typedef struct _GskGpuNodeProcessor GskGpuNodeProcessor;

typedef enum {
  GSK_GPU_GLOBAL_MATRIX = (1 << 0),
  GSK_GPU_GLOBAL_SCALE  = (1 << 1),
  GSK_GPU_GLOBAL_CLIP   = (1 << 2)
} GskGpuGlobals;

struct _GskGpuNodeProcessor
{
  GskGpuFrame           *frame;
  cairo_rectangle_int_t  scissor;
  graphene_point_t       offset;
  graphene_matrix_t      projection;
  graphene_vec2_t        scale;
  GskTransform          *modelview;
  GskGpuClip             clip;

  GskGpuGlobals          pending_globals;
};

static void
gsk_gpu_node_processor_add_node (GskGpuNodeProcessor *self,
                                 GskRenderNode       *node);

static void
gsk_gpu_node_processor_finish (GskGpuNodeProcessor *self)
{
  g_clear_pointer (&self->modelview, gsk_transform_unref);
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
  gsk_gpu_clip_init_empty (&self->clip, &GRAPHENE_RECT_INIT (0, 0, viewport->size.width, viewport->size.height));

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
  self->pending_globals = GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
}

static void
gsk_gpu_node_processor_emit_globals_op (GskGpuNodeProcessor *self)
{
  graphene_matrix_t mvp;

  if (self->modelview)
    {
      gsk_transform_to_matrix (self->modelview, &mvp);
      graphene_matrix_multiply (&mvp, &self->projection, &mvp);
    }
  else
    graphene_matrix_init_from_matrix (&mvp, &self->projection);

  gsk_gpu_globals_op (self->frame,
                      &self->scale,
                      &mvp,
                      &self->clip.rect);

  self->pending_globals &= ~(GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP);
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

  gsk_gpu_node_processor_add_node (&self, node);

  gsk_gpu_node_processor_finish (&self);
}

static void
gsk_gpu_node_processor_add_fallback_node (GskGpuNodeProcessor *self,
                                          GskRenderNode       *node)
{
  GskGpuImage *image;

  image = gsk_gpu_upload_cairo_op (self->frame,
                                   node,
                                   &self->scale,
                                   &node->bounds);

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                      image,
                      GSK_GPU_SAMPLER_DEFAULT,
                      &node->bounds,
                      &self->offset,
                      &node->bounds);
}

static const struct
{
  GskGpuGlobals ignored_globals;
  void                  (* process_node)                        (GskGpuNodeProcessor    *self,
                                                                 GskRenderNode          *node);
} nodes_vtable[] = {
  [GSK_NOT_A_RENDER_NODE] = {
    0,
    NULL,
  },
  [GSK_CONTAINER_NODE] = {
    0,
    NULL,
  },
  [GSK_CAIRO_NODE] = {
    0,
    NULL,
  },
  [GSK_COLOR_NODE] = {
    0,
    NULL,
  },
  [GSK_LINEAR_GRADIENT_NODE] = {
    0,
    NULL,
  },
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = {
    0,
    NULL,
  },
  [GSK_RADIAL_GRADIENT_NODE] = {
    0,
    NULL,
  },
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = {
    0,
    NULL,
  },
  [GSK_CONIC_GRADIENT_NODE] = {
    0,
    NULL,
  },
  [GSK_BORDER_NODE] = {
    0,
    NULL,
  },
  [GSK_TEXTURE_NODE] = {
    0,
    NULL,
  },
  [GSK_INSET_SHADOW_NODE] = {
    0,
    NULL,
  },
  [GSK_OUTSET_SHADOW_NODE] = {
    0,
    NULL,
  },
  [GSK_TRANSFORM_NODE] = {
    0,
    NULL,
  },
  [GSK_OPACITY_NODE] = {
    0,
    NULL,
  },
  [GSK_COLOR_MATRIX_NODE] = {
    0,
    NULL,
  },
  [GSK_REPEAT_NODE] = {
    0,
    NULL,
  },
  [GSK_CLIP_NODE] = {
    0,
    NULL,
  },
  [GSK_ROUNDED_CLIP_NODE] = {
    0,
    NULL,
  },
  [GSK_SHADOW_NODE] = {
    0,
    NULL,
  },
  [GSK_BLEND_NODE] = {
    0,
    NULL,
  },
  [GSK_CROSS_FADE_NODE] = {
    0,
    NULL,
  },
  [GSK_TEXT_NODE] = {
    0,
    NULL,
  },
  [GSK_BLUR_NODE] = {
    0,
    NULL,
  },
  [GSK_DEBUG_NODE] = {
    0,
    NULL,
  },
  [GSK_GL_SHADER_NODE] = {
    0,
    NULL,
  },
  [GSK_TEXTURE_SCALE_NODE] = {
    0,
    NULL,
  },
  [GSK_MASK_NODE] = {
    0,
    NULL,
  },
  [GSK_FILL_NODE] = {
    0,
    NULL,
  },
  [GSK_STROKE_NODE] = {
    0,
    NULL,
  },
  [GSK_SUBSURFACE_NODE] = {
    0,
    NULL,
  },
};

static void
gsk_gpu_node_processor_add_node (GskGpuNodeProcessor *self,
                                 GskRenderNode       *node)
{
  GskRenderNodeType node_type;
  GskGpuGlobals required_globals;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn */
  if (node->bounds.size.width == 0 || node->bounds.size.height == 0)
    return;
  if (!gsk_gpu_clip_may_intersect_rect (&self->clip, &self->offset, &node->bounds))
    return;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unkonwn node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      gsk_gpu_node_processor_add_fallback_node (self, node);
      return;
    }

  required_globals = self->pending_globals & ~nodes_vtable[node_type].ignored_globals;
  if (required_globals & (GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP))
    gsk_gpu_node_processor_emit_globals_op (self);
  g_assert ((self->pending_globals & ~nodes_vtable[node_type].ignored_globals) == 0);

  if (nodes_vtable[node_type].process_node)
    {
      nodes_vtable[node_type].process_node (self, node);
    }
  else
    {
      GSK_DEBUG (FALLBACK, "Unsupported node '%s'",
                 g_type_name_from_instance ((GTypeInstance *) node));
      gsk_gpu_node_processor_add_fallback_node (self, node);
    }
}
