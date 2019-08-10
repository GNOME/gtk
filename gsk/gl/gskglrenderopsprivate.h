#ifndef __GSK_GL_RENDER_OPS_H__
#define __GSK_GL_RENDER_OPS_H__

#include <glib.h>
#include <graphene.h>
#include <gdk/gdk.h>

#include "gskgldriverprivate.h"
#include "gskroundedrectprivate.h"
#include "gskglrenderer.h"
#include "gskrendernodeprivate.h"

#define GL_N_VERTICES 6
#define GL_N_PROGRAMS 13



typedef struct
{
  float translate_x;
  float translate_y;
  float scale_x;
  float scale_y;

  float dx_before;
  float dy_before;
} OpsMatrixMetadata;

typedef struct
{
  GskTransform *transform;
  OpsMatrixMetadata metadata;
} MatrixStackEntry;

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
  OP_CHANGE_BORDER          =  16,
  OP_CHANGE_BORDER_COLOR    =  17,
  OP_CHANGE_BORDER_WIDTH    =  18,
  OP_CHANGE_CROSS_FADE      =  19,
  OP_CHANGE_UNBLURRED_OUTSET_SHADOW = 20,
  OP_CLEAR                  =  21,
  OP_DRAW                   =  22,
  OP_DUMP_FRAMEBUFFER       =  23,
  OP_PUSH_DEBUG_GROUP       =  24,
  OP_POP_DEBUG_GROUP        =  25,
  OP_CHANGE_BLEND           =  26,
  OP_CHANGE_REPEAT          =  27,
};

typedef struct
{
  int index;        /* Into the renderer's program array */

  int id;
  /* Common locations (gl_common)*/
  int source_location;
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
      int dir_location;
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
      int outline_location;
      int corner_widths_location;
      int corner_heights_location;
    } outset_shadow;
    struct {
      int outline_location;
      int corner_widths_location;
      int corner_heights_location;
      int color_location;
      int spread_location;
      int offset_location;
    } unblurred_outset_shadow;
    struct {
      int color_location;
      int widths_location;
      int outline_location;
      int corner_widths_location;
      int corner_heights_location;
    } border;
    struct {
      int source2_location;
      int progress_location;
    } cross_fade;
    struct {
      int source2_location;
      int mode_location;
    } blend;
    struct {
      int child_bounds_location;
      int texture_rect_location;
    } repeat;
  };

} Program;

typedef struct
{
  guint op;

  union {
    float opacity;
    graphene_matrix_t modelview;
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
      float dir[2];
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
    struct {
      float outline[4];
      float corner_widths[4];
      float corner_heights[4];
      float radius;
      float spread;
      float offset[2];
      float color[4];
    } unblurred_outset_shadow;
    struct {
      float color[4];
    } shadow;
    struct {
      float widths[4];
      float color[4];
      GskRoundedRect outline;
    } border;
    struct {
      float progress;
      int source2;
    } cross_fade;
    struct {
      int source2;
      int mode;
    } blend;
    struct {
      float child_bounds[4];
      float texture_rect[4];
    } repeat;
    struct {
      char *filename;
      int width;
      int height;
    } dump;
    struct {
      char text[180]; /* Size of linear_gradient, so 'should be enough' without growing RenderOp */
    } debug_group;
  };
} RenderOp;

typedef struct
{
  GskTransform *modelview;
  GskRoundedRect clip;
  graphene_matrix_t projection;
  int source_texture;
  graphene_rect_t viewport;
  float opacity;
  /* Per-program state */
  union {
    GdkRGBA color;
    struct {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;
    } color_matrix;
    struct {
      float widths[4];
      float color[4];
      GskRoundedRect outline;
    } border;
  };
} ProgramState;

typedef struct
{
  ProgramState program_state[GL_N_PROGRAMS];
  const Program *current_program;
  int current_render_target;
  int current_texture;

  graphene_matrix_t current_projection;
  graphene_rect_t current_viewport;
  float current_opacity;
  float dx, dy;

  gsize buffer_size;

  GArray *render_ops;
  GskGLRenderer *renderer;

  /* Stack of modelview matrices */
  GArray *mv_stack;
  GskTransform *current_modelview;

  /* Same thing */
  GArray *clip_stack;
  /* Pointer into clip_stack */
  const GskRoundedRect *current_clip;
} RenderOpBuilder;


void              ops_dump_framebuffer   (RenderOpBuilder         *builder,
                                          const char              *filename,
                                          int                      width,
                                          int                      height);
void              ops_init               (RenderOpBuilder         *builder);
void              ops_free               (RenderOpBuilder         *builder);
void              ops_push_debug_group    (RenderOpBuilder         *builder,
                                           const char              *text);
void              ops_pop_debug_group     (RenderOpBuilder         *builder);

void              ops_finish             (RenderOpBuilder         *builder);
void              ops_push_modelview     (RenderOpBuilder         *builder,
                                          GskTransform            *transform);
void              ops_set_modelview      (RenderOpBuilder         *builder,
                                          GskTransform            *transform);
void              ops_pop_modelview      (RenderOpBuilder         *builder);
float             ops_get_scale          (const RenderOpBuilder   *builder);

void              ops_set_program        (RenderOpBuilder         *builder,
                                          const Program           *program);

void              ops_push_clip          (RenderOpBuilder         *builder,
                                          const GskRoundedRect    *clip);
void              ops_pop_clip           (RenderOpBuilder         *builder);
gboolean          ops_has_clip           (RenderOpBuilder         *builder);

void              ops_transform_bounds_modelview (const RenderOpBuilder *builder,
                                                  const graphene_rect_t *src,
                                                  graphene_rect_t       *dst);

graphene_matrix_t ops_set_projection     (RenderOpBuilder         *builder,
                                          const graphene_matrix_t *projection);

graphene_rect_t   ops_set_viewport       (RenderOpBuilder         *builder,
                                          const graphene_rect_t   *viewport);

void              ops_set_texture        (RenderOpBuilder         *builder,
                                          int                      texture_id);

int               ops_set_render_target  (RenderOpBuilder         *builder,
                                          int                      render_target_id);

float             ops_set_opacity        (RenderOpBuilder         *builder,
                                          float                    opacity);
void              ops_set_color          (RenderOpBuilder         *builder,
                                          const GdkRGBA           *color);

void              ops_set_color_matrix   (RenderOpBuilder         *builder,
                                          const graphene_matrix_t *matrix,
                                          const graphene_vec4_t   *offset);

void              ops_set_border         (RenderOpBuilder         *builder,
                                          const GskRoundedRect    *outline);
void              ops_set_border_width   (RenderOpBuilder         *builder,
                                          const float             *widths);

void              ops_set_border_color   (RenderOpBuilder         *builder,
                                          const GdkRGBA           *color);

void              ops_draw               (RenderOpBuilder        *builder,
                                          const GskQuadVertex     vertex_data[GL_N_VERTICES]);

void              ops_offset             (RenderOpBuilder        *builder,
                                          float                   x,
                                          float                   y);

void              ops_add                (RenderOpBuilder        *builder,
                                          const RenderOp         *op);

#endif
