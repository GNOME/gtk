#ifndef __GSK_GL_RENDER_OPS_H__
#define __GSK_GL_RENDER_OPS_H__

#include <glib.h>
#include <graphene.h>
#include <gdk/gdk.h>

#include "gskgldriverprivate.h"
#include "gskroundedrectprivate.h"
#include "gskglrenderer.h"
#include "gskrendernodeprivate.h"

#include "opbuffer.h"

#define GL_N_VERTICES 6
#define GL_N_PROGRAMS 14
#define GL_MAX_GRADIENT_STOPS 6

typedef struct
{
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

struct _Program
{
  int index;        /* Into the renderer's program array */

  int id;
  /* Common locations (gl_common)*/
  int source_location;
  int position_location;
  int uv_location;
  int alpha_location;
  int viewport_location;
  int projection_location;
  int modelview_location;
  int clip_rect_location;
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
      int start_point_location;
      int end_point_location;
    } linear_gradient;
    struct {
      int num_color_stops_location;
      int color_stops_location;
      int center_location;
      int start_location;
      int end_location;
      int radius_location;
    } radial_gradient;
    struct {
      int blur_radius_location;
      int blur_size_location;
      int blur_dir_location;
    } blur;
    struct {
      int color_location;
      int spread_location;
      int offset_location;
      int outline_rect_location;
    } inset_shadow;
    struct {
      int color_location;
      int outline_rect_location;
    } outset_shadow;
    struct {
      int outline_rect_location;
      int color_location;
      int spread_location;
      int offset_location;
    } unblurred_outset_shadow;
    struct {
      int color_location;
      int widths_location;
      int outline_rect_location;
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
};

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
      GdkRGBA color;
      GskRoundedRect outline;
    } border;
    struct {
      GskRoundedRect outline;
      float dx;
      float dy;
      float spread;
      GdkRGBA color;
    } inset_shadow;
    struct {
      GskRoundedRect outline;
      float dx;
      float dy;
      float spread;
      GdkRGBA color;
    } unblurred_outset_shadow;
    struct {
      int n_color_stops;
      GskColorStop color_stops[GL_MAX_GRADIENT_STOPS];
      float start_point[2];
      float end_point[2];
    } linear_gradient;
    struct {
      int n_color_stops;
      GskColorStop color_stops[GL_MAX_GRADIENT_STOPS];
      float center[2];
      float start;
      float end;
      float radius[2]; /* h/v */
    } radial_gradient;
  };
} ProgramState;

typedef struct {
  int ref_count;
  union {
    Program programs[GL_N_PROGRAMS];
    struct {
      Program blend_program;
      Program blit_program;
      Program blur_program;
      Program border_program;
      Program color_matrix_program;
      Program color_program;
      Program coloring_program;
      Program cross_fade_program;
      Program inset_shadow_program;
      Program linear_gradient_program;
      Program radial_gradient_program;
      Program outset_shadow_program;
      Program repeat_program;
      Program unblurred_outset_shadow_program;
    };
  };
  ProgramState state[GL_N_PROGRAMS];
} GskGLRendererPrograms;

typedef struct
{
  GskGLRendererPrograms *programs;
  const Program *current_program;
  int current_render_target;
  int current_texture;

  graphene_matrix_t current_projection;
  graphene_rect_t current_viewport;
  float current_opacity;
  float dx, dy;
  float scale_x, scale_y;

  OpBuffer render_ops;
  GArray *vertices;

  GskGLRenderer *renderer;

  /* Stack of modelview matrices */
  GArray *mv_stack;
  GskTransform *current_modelview;

  /* Same thing */
  GArray *clip_stack;
  /* Pointer into clip_stack */
  const GskRoundedRect *current_clip;
  bool clip_is_rectilinear;
} RenderOpBuilder;


void              ops_dump_framebuffer   (RenderOpBuilder         *builder,
                                          const char              *filename,
                                          int                      width,
                                          int                      height);
void              ops_init               (RenderOpBuilder         *builder);
void              ops_free               (RenderOpBuilder         *builder);
void              ops_reset              (RenderOpBuilder         *builder);
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
void              ops_set_inset_shadow   (RenderOpBuilder         *self,
                                          const GskRoundedRect     outline,
                                          float                    spread,
                                          const GdkRGBA           *color,
                                          float                    dx,
                                          float                    dy);
void              ops_set_unblurred_outset_shadow   (RenderOpBuilder         *self,
                                                     const GskRoundedRect     outline,
                                                     float                    spread,
                                                     const GdkRGBA           *color,
                                                     float                    dx,
                                                     float                    dy);

void              ops_set_linear_gradient (RenderOpBuilder     *self,
                                           guint                n_color_stops,
                                           const GskColorStop  *color_stops,
                                           float                start_x,
                                           float                start_y,
                                           float                end_x,
                                           float                end_y);
void              ops_set_radial_gradient (RenderOpBuilder        *self,
                                           guint                   n_color_stops,
                                           const GskColorStop     *color_stops,
                                           float                   center_x,
                                           float                   center_y,
                                           float                   start,
                                           float                   end,
                                           float                   hradius,
                                           float                   vradius);

GskQuadVertex *   ops_draw               (RenderOpBuilder        *builder,
                                          const GskQuadVertex     vertex_data[GL_N_VERTICES]);

void              ops_offset             (RenderOpBuilder        *builder,
                                          float                   x,
                                          float                   y);

gpointer          ops_begin              (RenderOpBuilder        *builder,
                                          OpKind                  kind);
OpBuffer         *ops_get_buffer         (RenderOpBuilder        *builder);

#endif
