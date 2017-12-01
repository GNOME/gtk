#ifndef __GSK_GL_RENDER_OPS_H__
#define __GSK_GL_RENDER_OPS_H__

#include <glib.h>
#include <graphene.h>
#include <gdk/gdk.h>

#include "gskgldriverprivate.h"
#include "gskroundedrectprivate.h"
#include "gskglrendererprivate.h"

#define GL_N_VERTICES 6
#define GL_N_PROGRAMS 9

enum {
  OP_NONE,
  OP_CHANGE_OPACITY         =  1,
  OP_CHANGE_COLOR           =  2,
  OP_CHANGE_PROJECTION      =  3,
  OP_CHANGE_MODELVIEW       =  4,
  OP_CHANGE_PROGRAM         =  5,
  OP_CHANGE_RENDER_TARGET   =  6,
  OP_CHANGE_CLIP            =  7,
  OP_CHANGE_VIEWPORT        =  8,
  OP_CHANGE_SOURCE_TEXTURE  =  9,
  OP_CHANGE_VAO             =  10,
  OP_CHANGE_LINEAR_GRADIENT =  11,
  OP_CHANGE_COLOR_MATRIX    =  12,
  OP_CHANGE_BLUR            =  13,
  OP_CHANGE_INSET_SHADOW    =  14,
  OP_CHANGE_OUTSET_SHADOW   =  15,
  OP_CLEAR                  =  16,
  OP_DRAW                   =  17,
};

typedef struct
{
  int index;        /* Into the renderer's program array */

  int id;
  /* Common locations (gl_common)*/
  int source_location;
  int mask_location;
  int position_location;
  int uv_location;
  int alpha_location;
  int blend_mode_location;
  int viewport_location;
  int projection_location;
  int modelview_location;
  int clip_location;
  int clip_corner_widths_location;
  int clip_corner_heights_location;

  union {
    struct {
      int color_location;
    } color;
    struct {
      int color_location;
    } coloring;
    struct {
      int color_matrix_location;
      int color_offset_location;
    } color_matrix;
    struct {
      int num_color_stops_location;
      int color_stops_location;
      int color_offsets_location;
      int start_point_location;
      int end_point_location;
    } linear_gradient;
    struct {
      int blur_radius_location;
      int blur_size_location;
    } blur;
    struct {
      int color_location;
      int spread_location;
      int offset_location;
      int outline_location;
      int corner_widths_location;
      int corner_heights_location;
    } inset_shadow;
    struct {
      int color_location;
      int spread_location;
      int offset_location;
      int outline_location;
      int corner_widths_location;
      int corner_heights_location;
    } outset_shadow;
  };

} Program;

typedef struct
{
  guint op;

  union {
    float opacity;
    graphene_matrix_t modelview; /* TODO: Make both matrix members just "matrix" */
    graphene_matrix_t projection;
    const Program *program;
    int texture_id;
    int render_target_id;
    GdkRGBA color;
    GskQuadVertex vertex_data[6];
    GskRoundedRect clip;
    graphene_rect_t viewport;
    struct {
      int n_color_stops;
      float color_offsets[8];
      float color_stops[4 * 8];
      graphene_point_t start_point;
      graphene_point_t end_point;
    } linear_gradient;
    struct {
      gsize vao_offset;
      gsize vao_size;
    } draw;
    struct {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;
    } color_matrix;
    struct {
      float radius;
      graphene_size_t size;
    } blur;
    struct {
      float outline[4];
      float corner_widths[4];
      float corner_heights[4];
      float radius;
      float spread;
      float offset[2];
      float color[4];
    } inset_shadow;
    struct {
      float outline[4];
      float corner_widths[4];
      float corner_heights[4];
      float radius;
      float spread;
      float offset[2];
      float color[4];
    } outset_shadow;
  };
} RenderOp;

typedef struct
{
  /* Per-Program State */
  struct {
    GskRoundedRect clip;
    graphene_matrix_t modelview;
    graphene_matrix_t projection;
    int source_texture;
    graphene_rect_t viewport;
    /* Per-program state */
    union {
      GdkRGBA color;
    };
  } program_state[GL_N_PROGRAMS];

  /* Current global state */
  const Program *current_program;
  int current_render_target;
  int current_vao;
  int current_texture;
  GskRoundedRect current_clip;
  graphene_matrix_t current_modelview;
  graphene_matrix_t current_projection;
  graphene_rect_t current_viewport;
  float current_opacity;

  gsize buffer_size;

  GArray *render_ops;
  GskGLRenderer *renderer;
} RenderOpBuilder;



void              ops_set_program       (RenderOpBuilder         *builder,
                                         const Program           *program);

GskRoundedRect    ops_set_clip          (RenderOpBuilder         *builder,
                                         const GskRoundedRect    *clip);

graphene_matrix_t ops_set_modelview     (RenderOpBuilder         *builder,
                                         const graphene_matrix_t *modelview);

graphene_matrix_t ops_set_projection    (RenderOpBuilder         *builder,
                                         const graphene_matrix_t *projection);

graphene_rect_t   ops_set_viewport      (RenderOpBuilder         *builder,
                                         const graphene_rect_t   *viewport);

void              ops_set_texture       (RenderOpBuilder         *builder,
                                         int                      texture_id);

int               ops_set_render_target (RenderOpBuilder         *builder,
                                         int                      render_target_id);

float             ops_set_opacity        (RenderOpBuilder        *builder,
                                          float                   opacity);
void              ops_set_color          (RenderOpBuilder        *builder,
                                          const GdkRGBA          *color);

void              ops_draw               (RenderOpBuilder        *builder,
                                          const GskQuadVertex     vertex_data[GL_N_VERTICES]);

void              ops_add                (RenderOpBuilder        *builder,
                                          const RenderOp         *op);

#endif
