#include "config.h"

#include "gskglrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
#include "gskgldriverprivate.h"
#include "gskglprofilerprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskshaderbuilderprivate.h"
#include "gdk/gdktextureprivate.h"

#include "gskprivate.h"

#include <epoxy/gl.h>

#define SHADER_VERSION_GLES             100
#define SHADER_VERSION_GL2_LEGACY       110
#define SHADER_VERSION_GL3_LEGACY       130
#define SHADER_VERSION_GL3              150

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

#define HIGHLIGHT_FALLBACK 0

static void G_GNUC_UNUSED
dump_framebuffer (const char *filename, int w, int h)
{
  int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, w);
  guchar *data = g_malloc (h * stride);
  cairo_surface_t *s;

  glReadPixels (0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, data);
  s = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_RGB24, w, h, stride);
  cairo_surface_write_to_png (s, filename);

  cairo_surface_destroy (s);
  g_free (data);
}


static void
gsk_gl_renderer_setup_render_mode (GskGLRenderer *self);

typedef struct {
  int id;
  /* Common locations (gl_common)*/
  int mvp_location;
  int source_location;
  int mask_location;
  int uv_location;
  int position_location;
  int alpha_location;
  int blendMode_location;
  int viewport_location;
  int projection_location;
  int modelview_location;
  int clip_location;
  int clip_corner_widths_location;
  int clip_corner_heights_location;

  /* Shader-specific locations */
  union {
    struct {
      int color_location;
    };
    struct {
      int color_matrix_location;
      int color_offset_location;
    };
    struct {
      int n_color_stops_location;
      int color_stops_location;
      int color_offsets_location;
      int start_point_location;
      int end_point_location;
    };
    struct {
      int clip_bounds_location;
      int corner_widths_location;
      int corner_heights_location;
    };
  };
} Program;

#define INIT_PROGRAM_UNIFORM_LOCATION(program_name, location_name, uniform_name) \
              G_STMT_START{\
                self->program_name.location_name = glGetUniformLocation(self->program_name.id, uniform_name);\
                g_assert (self->program_name.location_name != 0); \
              }G_STMT_END

enum {
  MODE_BLIT = 1,
  MODE_COLOR,
  MODE_TEXTURE,
  MODE_COLOR_MATRIX,
  MODE_LINEAR_GRADIENT,
  N_MODES
};

typedef struct {
  int mode;

  graphene_point3d_t min;
  graphene_point3d_t max;

  graphene_size_t size;

  graphene_matrix_t mvp;
  graphene_matrix_t projection;
  graphene_matrix_t modelview;

  /* (Rounded) Clip */
  GskRoundedRect rounded_clip;

  float opacity;
  float z;

  union {
    struct {
      GdkRGBA color;
    } color_data;
    struct {
      graphene_matrix_t color_matrix;
      graphene_vec4_t color_offset;
    } color_matrix_data;
    struct {
      int n_color_stops;
      float color_offsets[8];
      float color_stops[4 * 8];
      graphene_point_t start_point;
      graphene_point_t end_point;
    } linear_gradient_data;
  };

  const char *name;

  GskBlendMode blend_mode;

  /* The render target this item will draw itself on */
  int parent_render_target;
  /* In case this item creates a new render target, this is its id */
  int render_target;
  int vao_id;
  int texture_id;
  const Program *program;

  GArray *children;
} RenderItem;

static void
destroy_render_item (RenderItem *item)
{
  if (item->children)
    g_array_unref (item->children);
}


enum {
  MVP,
  SOURCE,
  MASK,
  ALPHA,
  BLEND_MODE,
  VIEWPORT,
  PROJECTION,
  MODELVIEW,
  CLIP,
  CLIP_CORNER_WIDTHS,
  CLIP_CORNER_HEIGHTS,
  N_UNIFORMS
};

enum {
  POSITION,
  UV,
  N_ATTRIBUTES
};

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark frames;
  GQuark draw_calls;
} ProfileCounters;

typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;
#endif

typedef enum {
  RENDER_FULL,
  RENDER_SCISSOR
} RenderMode;

struct _GskGLRenderer
{
  GskRenderer parent_instance;

  int scale_factor;

  graphene_matrix_t mvp;
  graphene_rect_t viewport;

  guint frame_buffer;
  guint depth_stencil_buffer;
  guint texture_id;

  GQuark uniforms[N_UNIFORMS];
  GQuark attributes[N_ATTRIBUTES];

  GdkGLContext *gl_context;
  GskGLDriver *gl_driver;
  GskGLProfiler *gl_profiler;

  Program blend_program;
  Program blit_program;
  Program color_program;
  Program color_matrix_program;
  Program linear_gradient_program;
  Program clip_program;

  GArray *render_items;

#ifdef G_ENABLE_DEBUG
  ProfileCounters profile_counters;
  ProfileTimers profile_timers;
#endif

  RenderMode render_mode;

  gboolean has_buffers : 1;
};

struct _GskGLRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_RENDERER)

static void
gsk_gl_renderer_dispose (GObject *gobject)
{
  GskGLRenderer *self = GSK_GL_RENDERER (gobject);

  g_clear_pointer (&self->render_items, g_array_unref);

  G_OBJECT_CLASS (gsk_gl_renderer_parent_class)->dispose (gobject);
}

static void
gsk_gl_renderer_create_buffers (GskGLRenderer *self,
                                int            width,
                                int            height,
                                int            scale_factor)
{
  if (self->has_buffers)
    return;

  GSK_NOTE (OPENGL, g_print ("Creating buffers (w:%d, h:%d, scale:%d)\n", width, height, scale_factor));

  if (self->texture_id == 0)
    {
      self->texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                       width * scale_factor,
                                                       height * scale_factor);
      gsk_gl_driver_bind_source_texture (self->gl_driver, self->texture_id);
      gsk_gl_driver_init_texture_empty (self->gl_driver, self->texture_id);
    }

  gsk_gl_driver_create_render_target (self->gl_driver, self->texture_id, TRUE, TRUE);
  gsk_gl_driver_bind_render_target (self->gl_driver, self->texture_id);

  self->has_buffers = TRUE;
}

static void
gsk_gl_renderer_destroy_buffers (GskGLRenderer *self)
{
  if (self->gl_context == NULL)
    return;

  if (!self->has_buffers)
    return;

  GSK_NOTE (OPENGL, g_print ("Destroying buffers\n"));

  gdk_gl_context_make_current (self->gl_context);

  if (self->texture_id != 0)
    {
      gsk_gl_driver_destroy_texture (self->gl_driver, self->texture_id);
      self->texture_id = 0;
    }

  self->has_buffers = FALSE;
}

static void
init_common_locations (GskGLRenderer    *self,
                       GskShaderBuilder *builder,
                       Program          *prog)
{
  prog->source_location =
    gsk_shader_builder_get_uniform_location (builder, prog->id, self->uniforms[SOURCE]);
  prog->mask_location =
    gsk_shader_builder_get_uniform_location (builder, prog->id, self->uniforms[MASK]);
  prog->mvp_location =
    gsk_shader_builder_get_uniform_location (builder, prog->id, self->uniforms[MVP]);
  prog->alpha_location =
    gsk_shader_builder_get_uniform_location (builder, prog->id, self->uniforms[ALPHA]);
  prog->blendMode_location =
    gsk_shader_builder_get_uniform_location (builder, prog->id, self->uniforms[BLEND_MODE]);
  prog->viewport_location = gsk_shader_builder_get_uniform_location (builder, prog->id,
                                                                     self->uniforms[VIEWPORT]);
  prog->projection_location = gsk_shader_builder_get_uniform_location (builder, prog->id,
                                                                       self->uniforms[PROJECTION]);
  prog->modelview_location = gsk_shader_builder_get_uniform_location (builder, prog->id,
                                                                      self->uniforms[MODELVIEW]);
  prog->clip_location = gsk_shader_builder_get_uniform_location (builder, prog->id,
                                                                 self->uniforms[CLIP]);
  prog->clip_corner_widths_location = gsk_shader_builder_get_uniform_location (builder, prog->id,
                                                                               self->uniforms[CLIP_CORNER_WIDTHS]);
  prog->clip_corner_heights_location = gsk_shader_builder_get_uniform_location (builder, prog->id,
                                                                               self->uniforms[CLIP_CORNER_HEIGHTS]);

  prog->position_location =
    gsk_shader_builder_get_attribute_location (builder, prog->id, self->attributes[POSITION]);
  prog->uv_location =
    gsk_shader_builder_get_attribute_location (builder, prog->id, self->attributes[UV]);
}

static gboolean
gsk_gl_renderer_create_programs (GskGLRenderer  *self,
                                 GError        **error)
{
  GskShaderBuilder *builder;
  GError *shader_error = NULL;
  gboolean res = FALSE;

  builder = gsk_shader_builder_new ();

  gsk_shader_builder_set_resource_base_path (builder, "/org/gtk/libgsk/glsl");

  self->uniforms[MVP] = gsk_shader_builder_add_uniform (builder, "uMVP");
  self->uniforms[SOURCE] = gsk_shader_builder_add_uniform (builder, "uSource");
  self->uniforms[MASK] = gsk_shader_builder_add_uniform (builder, "uMask");
  self->uniforms[ALPHA] = gsk_shader_builder_add_uniform (builder, "uAlpha");
  self->uniforms[BLEND_MODE] = gsk_shader_builder_add_uniform (builder, "uBlendMode");
  self->uniforms[VIEWPORT] = gsk_shader_builder_add_uniform (builder, "uViewport");
  self->uniforms[PROJECTION] = gsk_shader_builder_add_uniform (builder, "uProjection");
  self->uniforms[MODELVIEW] = gsk_shader_builder_add_uniform (builder, "uModelview");
  self->uniforms[CLIP] = gsk_shader_builder_add_uniform (builder, "uClip");
  self->uniforms[CLIP_CORNER_WIDTHS] = gsk_shader_builder_add_uniform (builder, "uClipCornerWidths");
  self->uniforms[CLIP_CORNER_HEIGHTS] = gsk_shader_builder_add_uniform (builder, "uClipCornerHeights");

  self->attributes[POSITION] = gsk_shader_builder_add_attribute (builder, "aPosition");
  self->attributes[UV] = gsk_shader_builder_add_attribute (builder, "aUv");

  if (gdk_gl_context_get_use_es (self->gl_context))
    {
      gsk_shader_builder_set_version (builder, SHADER_VERSION_GLES);
      gsk_shader_builder_set_vertex_preamble (builder, "es2_common.vs.glsl");
      gsk_shader_builder_set_fragment_preamble (builder, "es2_common.fs.glsl");
      gsk_shader_builder_add_define (builder, "GSK_GLES", "1");
    }
  else if (gdk_gl_context_is_legacy (self->gl_context))
    {
      int maj, min;
      gdk_gl_context_get_version (self->gl_context, &maj, &min);

      if (maj == 3)
        gsk_shader_builder_set_version (builder, SHADER_VERSION_GL3_LEGACY);
      else
        gsk_shader_builder_set_version (builder, SHADER_VERSION_GL2_LEGACY);

      gsk_shader_builder_set_vertex_preamble (builder, "gl_common.vs.glsl");
      gsk_shader_builder_set_fragment_preamble (builder, "gl_common.fs.glsl");
      gsk_shader_builder_add_define (builder, "GSK_LEGACY", "1");
    }
  else
    {
      gsk_shader_builder_set_version (builder, SHADER_VERSION_GL3);
      gsk_shader_builder_set_vertex_preamble (builder, "gl3_common.vs.glsl");
      gsk_shader_builder_set_fragment_preamble (builder, "gl3_common.fs.glsl");
      gsk_shader_builder_add_define (builder, "GSK_GL3", "1");
    }

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDER_MODE_CHECK (SHADERS))
    gsk_shader_builder_add_define (builder, "GSK_DEBUG", "1");
#endif

  self->blend_program.id = 
    gsk_shader_builder_create_program (builder, "blend.vs.glsl", "blend.fs.glsl", &shader_error);
  if (shader_error != NULL)
    {
      g_propagate_prefixed_error (error,
                                  shader_error,
                                  "Unable to create 'blend' program: ");
      goto out;
    }
  init_common_locations (self, builder, &self->blend_program);

  self->blit_program.id =
    gsk_shader_builder_create_program (builder, "blit.vs.glsl", "blit.fs.glsl", &shader_error);
  if (shader_error != NULL)
    {
      g_propagate_prefixed_error (error,
                                  shader_error,
                                  "Unable to create 'blit' program: ");
      goto out;
    }
  init_common_locations (self, builder, &self->blit_program);

  self->color_program.id =
    gsk_shader_builder_create_program (builder, "color.vs.glsl", "color.fs.glsl", &shader_error);
  if (shader_error != NULL)
    {
      g_propagate_prefixed_error (error,
                                  shader_error,
                                  "Unable to create 'color' program: ");
      goto out;
    }
  init_common_locations (self, builder, &self->color_program);
  INIT_PROGRAM_UNIFORM_LOCATION (color_program, color_location, "uColor");

  self->color_matrix_program.id = gsk_shader_builder_create_program (builder,
                                                                     "color_matrix.vs.glsl",
                                                                     "color_matrix.fs.glsl",
                                                                     &shader_error);
  if (shader_error != NULL)
    {
      g_propagate_prefixed_error (error,
                                  shader_error,
                                  "Unable to create 'color_matrix' program: ");
      goto out;
    }
  init_common_locations (self, builder, &self->color_matrix_program);
  INIT_PROGRAM_UNIFORM_LOCATION (color_matrix_program, color_matrix_location, "uColorMatrix");
  INIT_PROGRAM_UNIFORM_LOCATION (color_matrix_program, color_offset_location, "uColorOffset");

  self->linear_gradient_program.id = gsk_shader_builder_create_program (builder,
                                                                        "blit.vs.glsl",
                                                                        "linear_gradient.fs.glsl",
                                                                        &shader_error);
  if (shader_error != NULL)
    {
      g_propagate_prefixed_error (error,
                                  shader_error,
                                  "Unable to create 'linear_gradient' program: ");
      goto out;
    }
  init_common_locations (self, builder, &self->linear_gradient_program);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient_program, color_stops_location, "uColorStops");
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient_program, color_offsets_location, "uColorOffsets");
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient_program, n_color_stops_location, "uNumColorStops");
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient_program, start_point_location, "uStartPoint");
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient_program, end_point_location, "uEndPoint");

  res = TRUE;

out:

  g_object_unref (builder);
  return res;
}

static gboolean
gsk_gl_renderer_realize (GskRenderer  *renderer,
                         GdkWindow    *window,
                         GError      **error)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);

  self->scale_factor = gdk_window_get_scale_factor (window);

  /* If we didn't get a GdkGLContext before realization, try creating
   * one now, for our exclusive use.
   */
  if (self->gl_context == NULL)
    {
      self->gl_context = gdk_window_create_gl_context (window, error);
      if (self->gl_context == NULL)
        return FALSE;
    }

  if (!gdk_gl_context_realize (self->gl_context, error))
    return FALSE;

  gdk_gl_context_make_current (self->gl_context);

  g_assert (self->gl_driver == NULL);
  self->gl_profiler = gsk_gl_profiler_new (self->gl_context);
  self->gl_driver = gsk_gl_driver_new (self->gl_context);

  GSK_NOTE (OPENGL, g_print ("Creating buffers and programs\n"));
  if (!gsk_gl_renderer_create_programs (self, error))
    return FALSE;

  return TRUE;
}

static void
gsk_gl_renderer_unrealize (GskRenderer *renderer)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  /* We don't need to iterate to destroy the associated GL resources,
   * as they will be dropped when we finalize the GskGLDriver
   */
  g_array_set_size (self->render_items, 0);


  glDeleteProgram (self->blend_program.id);
  glDeleteProgram (self->blit_program.id);
  glDeleteProgram (self->color_program.id);
  glDeleteProgram (self->color_matrix_program.id);
  glDeleteProgram (self->linear_gradient_program.id);

  gsk_gl_renderer_destroy_buffers (self);

  g_clear_object (&self->gl_profiler);
  g_clear_object (&self->gl_driver);

  if (self->gl_context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();

  g_clear_object (&self->gl_context);
}

static GdkDrawingContext *
gsk_gl_renderer_begin_draw_frame (GskRenderer          *renderer,
                                  const cairo_region_t *update_area)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  cairo_region_t *damage;
  GdkDrawingContext *result;
  GdkRectangle whole_window;
  GdkWindow *window;

  window = gsk_renderer_get_window (renderer);
  whole_window = (GdkRectangle) {
                     0, 0,
                     gdk_window_get_width (window) * self->scale_factor,
                     gdk_window_get_height (window) * self->scale_factor
                 };
  damage = gdk_gl_context_get_damage (self->gl_context);
  cairo_region_union (damage, update_area);

  if (cairo_region_contains_rectangle (damage, &whole_window) == CAIRO_REGION_OVERLAP_IN)
    {
      self->render_mode = RENDER_FULL;
    }
  else
    {
      GdkRectangle extents;

      cairo_region_get_extents (damage, &extents);
      cairo_region_union_rectangle (damage, &extents);

      if (gdk_rectangle_equal (&extents, &whole_window))
        self->render_mode = RENDER_FULL;
      else
        self->render_mode = RENDER_SCISSOR;
    }

  result = gdk_window_begin_draw_frame (window,
                                        GDK_DRAW_CONTEXT (self->gl_context),
                                        damage);

  cairo_region_destroy (damage);

  return result;
}

static void
gsk_gl_renderer_resize_viewport (GskGLRenderer         *self,
                                 const graphene_rect_t *viewport)
{
  int width = viewport->size.width;
  int height = viewport->size.height;

  GSK_NOTE (OPENGL, g_print ("glViewport(0, 0, %d, %d) [scale:%d]\n",
                             width,
                             height,
                             self->scale_factor));

  graphene_rect_init (&self->viewport, 0, 0, width, height);
  glViewport (0, 0, width, height);
}

#define N_VERTICES      6

static void
render_item (GskGLRenderer    *self,
             const RenderItem *item)
{
  float mat[16];
  const gboolean draw_children = item->children != NULL &&
                                 item->children->len > 0;
  const gboolean drawing_offscreen = item->parent_render_target != 0;

  /*g_message ("Rendering %s with %u children and parent render target %d", item->name, item->children ? item->children->len : 0, item->parent_render_target);*/

  if (draw_children)
    {
      guint i;
      guint p;
      graphene_rect_t prev_viewport;

      prev_viewport = self->viewport;

      gsk_gl_driver_bind_render_target (self->gl_driver, item->render_target);
      glDisable (GL_SCISSOR_TEST);
      glClearColor (0.0, 0.0, 0.0, 0.0);
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
      graphene_rect_init (&self->viewport,
                          item->min.x, item->min.y, item->size.width * self->scale_factor, item->size.height * self->scale_factor);
      glViewport (0, 0, item->size.width * self->scale_factor, item->size.height * self->scale_factor);

      p = item->children->len;
      for (i = 0; i < p; i ++)
        {
          const RenderItem *child = &g_array_index (item->children, RenderItem, i);

          g_assert (child->parent_render_target == item->render_target);
          render_item (self, child);
        }

      /* At this point, all the child items should've been drawn */
      gsk_gl_driver_bind_render_target (self->gl_driver, 0);
      /* TODO: Manage pre-/post-framebuffer-state in the driver? */
      /* Resets the scissor test, etc. */
      if (!drawing_offscreen)
        gsk_gl_renderer_setup_render_mode (self);

      graphene_rect_init_from_rect (&self->viewport, &prev_viewport);
      glViewport (0, 0, self->viewport.size.width, self->viewport.size.height);
    }


  if (drawing_offscreen)
    g_assert (gsk_gl_driver_bind_render_target (self->gl_driver, item->parent_render_target));

  glUseProgram (item->program->id);

  switch(item->mode)
    {
      case MODE_COLOR:
        {
          glUniform4f (item->program->color_location,
                       item->color_data.color.red,
                       item->color_data.color.green,
                       item->color_data.color.blue,
                       item->color_data.color.alpha);
        }
      break;

      case MODE_TEXTURE:
        {
          g_assert(item->texture_id != 0);
          /* Use texture unit 0 for the source */
          glUniform1i (item->program->source_location, 0);
          gsk_gl_driver_bind_source_texture (self->gl_driver, item->texture_id);
        }
      break;

      case MODE_BLIT:
        g_assert (item->program == &self->blit_program);
        glUniform1i (item->program->source_location, 0);
        if (item->render_target != 0)
          gsk_gl_driver_bind_source_texture (self->gl_driver, item->render_target);
        else
          gsk_gl_driver_bind_source_texture (self->gl_driver, item->texture_id);
      break;

      case MODE_COLOR_MATRIX:
        {
          float vec[4];

          glUniform1i (item->program->source_location, 0);
          if (item->render_target != 0)
            gsk_gl_driver_bind_source_texture (self->gl_driver, item->render_target);
          else
            gsk_gl_driver_bind_source_texture (self->gl_driver, item->texture_id);

          graphene_matrix_to_float (&item->color_matrix_data.color_matrix, mat);
          glUniformMatrix4fv (item->program->color_matrix_location, 1, GL_FALSE, mat);

          graphene_vec4_to_float (&item->color_matrix_data.color_offset, vec);
          glUniform4fv (item->program->color_offset_location, 1, vec);
        }
      break;

      case MODE_LINEAR_GRADIENT:
        {
          glUniform1i (item->program->n_color_stops_location,
                       item->linear_gradient_data.n_color_stops);
          glUniform4fv (item->program->color_stops_location,
                        item->linear_gradient_data.n_color_stops,
                        item->linear_gradient_data.color_stops);
          glUniform1fv (item->program->color_offsets_location,
                        item->linear_gradient_data.n_color_stops,
                        item->linear_gradient_data.color_offsets);
          glUniform2f (item->program->start_point_location,
                       item->linear_gradient_data.start_point.x, item->linear_gradient_data.start_point.y);
          glUniform2f (item->program->end_point_location,
                       item->linear_gradient_data.end_point.x, item->linear_gradient_data.end_point.y);
        }
      break;

      default:
        g_assert_not_reached ();
    }

  /* Common uniforms */
  graphene_matrix_to_float (&item->mvp, mat);
  glUniformMatrix4fv (item->program->mvp_location, 1, GL_FALSE, mat);

  graphene_matrix_to_float (&item->projection, mat);
  glUniformMatrix4fv (item->program->projection_location, 1, GL_TRUE, mat);

  graphene_matrix_to_float (&item->modelview, mat);
  glUniformMatrix4fv (item->program->modelview_location, 1, GL_TRUE, mat);
  glUniform1f (item->program->alpha_location, item->opacity);
  glUniform4f (item->program->viewport_location,
               self->viewport.origin.x, self->viewport.origin.y,
               self->viewport.size.width, self->viewport.size.height);
  glUniform4f (item->program->clip_location,
               item->rounded_clip.bounds.origin.x, item->rounded_clip.bounds.origin.y,
               item->rounded_clip.bounds.size.width, item->rounded_clip.bounds.size.height);

  glUniform4f (item->program->clip_corner_widths_location,
               MAX (item->rounded_clip.corner[0].width, 1),
               MAX (item->rounded_clip.corner[1].width, 1),
               MAX (item->rounded_clip.corner[2].width, 1),
               MAX (item->rounded_clip.corner[3].width, 1));
  glUniform4f (item->program->clip_corner_heights_location,
               MAX (item->rounded_clip.corner[0].height, 1),
               MAX (item->rounded_clip.corner[1].height, 1),
               MAX (item->rounded_clip.corner[2].height, 1),
               MAX (item->rounded_clip.corner[3].height, 1));

  gsk_gl_driver_bind_vao (self->gl_driver, item->vao_id);
  glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);
}

static void
get_gl_scaling_filters (GskRenderNode *node,
                        int           *min_filter_r,
                        int           *mag_filter_r)
{
  *min_filter_r = GL_NEAREST;
  *mag_filter_r = GL_NEAREST;
}

static float
project_item (const graphene_matrix_t *projection,
              const graphene_matrix_t *modelview)
{
  graphene_vec4_t vec;

  graphene_matrix_get_row (modelview, 3, &vec);
  graphene_matrix_transform_vec4 (projection, &vec, &vec);

  return graphene_vec4_get_z (&vec) / graphene_vec4_get_w (&vec);
}

static void
init_framebuffer_for_node (GskGLRenderer           *self,
                           RenderItem              *item,
                           GskRenderNode           *node,
                           const graphene_matrix_t *projection,
                           graphene_matrix_t       *out_projection)
{
  item->render_target = gsk_gl_driver_create_texture (self->gl_driver,
                                                      item->size.width  * self->scale_factor,
                                                      item->size.height * self->scale_factor);
  gsk_gl_driver_bind_source_texture (self->gl_driver, item->render_target);
  gsk_gl_driver_init_texture_empty (self->gl_driver, item->render_target);
  gsk_gl_driver_create_render_target (self->gl_driver, item->render_target, TRUE, TRUE);

  item->children = g_array_new (FALSE, FALSE, sizeof (RenderItem));
  g_array_set_clear_func (item->children, (GDestroyNotify)destroy_render_item);

  g_assert (projection != NULL);
  graphene_matrix_init_ortho (out_projection,
                              item->min.x,
                              item->min.x + item->size.width  * self->scale_factor,
                              item->min.y,
                              item->min.y + item->size.height * self->scale_factor,
                              ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE);
  graphene_matrix_scale (out_projection, 1, -1, 1);
}

static void
gsk_gl_renderer_add_render_item (GskGLRenderer           *self,
                                 const graphene_matrix_t *projection,
                                 const graphene_matrix_t *modelview,
                                 GArray                  *render_items,
                                 GskRenderNode           *node,
                                 int                      render_target,
                                 const GskRoundedRect    *parent_clip)
{
  RenderItem item;

  /* We handle container nodes here directly */
  if (gsk_render_node_get_node_type (node) == GSK_CONTAINER_NODE)
    {
      guint i, p;

      for (i = 0, p = gsk_container_node_get_n_children (node); i < p; i++)
        {
          GskRenderNode *child = gsk_container_node_get_child (node, i);
          gsk_gl_renderer_add_render_item (self, projection, modelview, render_items,
                                           child, render_target, parent_clip);
        }
      return;
    }

  memset (&item, 0, sizeof (RenderItem));

  item.name = node->name != NULL ? node->name : "unnamed";


  item.size.width = node->bounds.size.width;
  item.size.height = node->bounds.size.height;
  /* Each render item is an axis-aligned bounding box that we
   * transform using the given transformation matrix
   */
  item.min.x = node->bounds.origin.x;
  item.min.y = node->bounds.origin.y;

  item.max.x = item.min.x + node->bounds.size.width;
  item.max.y = item.min.y + node->bounds.size.height;

  /* The location of the item, in normalized world coordinates */
  graphene_matrix_multiply (modelview, projection, &item.mvp);
  item.z = project_item (projection, modelview);

  item.opacity = 1.0;
  item.projection = *projection;
  item.modelview = *modelview;
  item.rounded_clip = *parent_clip;

  item.blend_mode = GSK_BLEND_MODE_DEFAULT;

  item.parent_render_target = render_target;

  item.mode = MODE_BLIT;
  item.program = &self->blit_program;
  item.render_target = 0;
  item.children = NULL;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_OPACITY_NODE:
      {
        GskRenderNode *child = gsk_opacity_node_get_child (node);

        if (gsk_render_node_get_node_type (child) != GSK_TEXTURE_NODE)
          {
            graphene_matrix_t p;
            graphene_matrix_t identity;

            graphene_matrix_init_identity (&identity);
            init_framebuffer_for_node (self, &item, node, projection, &p);
            gsk_gl_renderer_add_render_item (self, &p, &identity, item.children, child,
                                             item.render_target, parent_clip);
          }
        else
          {
            GdkTexture *texture = gsk_texture_node_get_texture (child);
            int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;

            get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

            item.texture_id = gsk_gl_driver_get_texture_for_texture (self->gl_driver,
                                                                     texture,
                                                                     gl_min_filter,
                                                                     gl_mag_filter);
          }

        item.mode = MODE_BLIT;
        item.opacity = gsk_opacity_node_get_opacity (node);
      }
      break;

    case GSK_CLIP_NODE:
      {
        GskRenderNode *child = gsk_clip_node_get_child (node);
        graphene_rect_t transformed_clip;
        graphene_rect_t intersection;
        GskRoundedRect child_clip;

        transformed_clip = *gsk_clip_node_peek_clip (node);
        graphene_matrix_transform_bounds (modelview, &transformed_clip, &transformed_clip);

        /* Since we do the intersection here, we also need to transform by the modelview matrix.
         * We can't do it in the shader. Same with rounded clips */
        graphene_rect_intersection (&transformed_clip,
                                    &parent_clip->bounds,
                                    &intersection);

        gsk_rounded_rect_init_from_rect (&child_clip, &intersection, 0.0f);

        gsk_gl_renderer_add_render_item (self, projection, modelview, render_items, child,
                                         render_target, &child_clip);
      }
      return;

    case GSK_ROUNDED_CLIP_NODE:
      {
        GskRenderNode *child = gsk_rounded_clip_node_get_child (node);
        const GskRoundedRect *rounded_clip = gsk_rounded_clip_node_peek_clip (node);
        graphene_rect_t transformed_clip;
        graphene_rect_t intersection;
        GskRoundedRect child_clip;

        transformed_clip = rounded_clip->bounds;
        graphene_matrix_transform_bounds (modelview, &transformed_clip, &transformed_clip);

        graphene_rect_intersection (&transformed_clip, &parent_clip->bounds,
                                    &intersection);
        gsk_rounded_rect_init (&child_clip, &intersection,
                               &rounded_clip->corner[0],
                               &rounded_clip->corner[1],
                               &rounded_clip->corner[2],
                               &rounded_clip->corner[3]);

        gsk_gl_renderer_add_render_item (self, projection, modelview, render_items, child,
                                         render_target, &child_clip);
      }
      return;

    case GSK_COLOR_MATRIX_NODE:
      {
        GskRenderNode *child = gsk_color_matrix_node_get_child (node);

        if (gsk_render_node_get_node_type (child) != GSK_TEXTURE_NODE)
          {

            graphene_matrix_t p;
            graphene_matrix_t identity;

            graphene_matrix_init_identity (&identity);
            init_framebuffer_for_node (self, &item, node, projection, &p);
            gsk_gl_renderer_add_render_item (self, &p, &identity, item.children, child,
                                             item.render_target, parent_clip);
          }
        else
          {
            GdkTexture *texture = gsk_texture_node_get_texture (child);
            int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;

            get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

            item.texture_id = gsk_gl_driver_get_texture_for_texture (self->gl_driver,
                                                                     texture,
                                                                     gl_min_filter,
                                                                     gl_mag_filter);
          }

        item.mode = MODE_COLOR_MATRIX;
        item.program = &self->color_matrix_program;
        item.color_matrix_data.color_matrix = *gsk_color_matrix_node_peek_color_matrix (node);
        item.color_matrix_data.color_offset = *gsk_color_matrix_node_peek_color_offset (node);
      }
      break;

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;

        get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

        item.texture_id = gsk_gl_driver_get_texture_for_texture (self->gl_driver,
                                                                 texture,
                                                                 gl_min_filter,
                                                                 gl_mag_filter);
        item.mode = MODE_TEXTURE;
      }
      break;

    case GSK_CAIRO_NODE:
      {
        const cairo_surface_t *surface = gsk_cairo_node_peek_surface (node);
        int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;

        if (surface == NULL)
          return;

        get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

        item.texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                        item.size.width,
                                                        item.size.height);
        gsk_gl_driver_bind_source_texture (self->gl_driver, item.texture_id);
        gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                                 item.texture_id,
                                                 (cairo_surface_t *)surface,
                                                 gl_min_filter,
                                                 gl_mag_filter);
        item.mode = MODE_TEXTURE;
      }
      break;

    case GSK_COLOR_NODE:
      {
        item.mode = MODE_COLOR;
        item.program = &self->color_program;
        item.color_data.color= *gsk_color_node_peek_color (node);
      }
      break;

    case GSK_LINEAR_GRADIENT_NODE:
      {
        int n_color_stops = MIN (8, gsk_linear_gradient_node_get_n_color_stops (node));
        const GskColorStop *stops = gsk_linear_gradient_node_peek_color_stops (node);
        const graphene_point_t *start = gsk_linear_gradient_node_peek_start (node);
        const graphene_point_t *end = gsk_linear_gradient_node_peek_end (node);
        int i;

        item.mode = MODE_LINEAR_GRADIENT;
        item.program = &self->linear_gradient_program;

        for (i = 0; i < n_color_stops; i ++)
          {
            const GskColorStop *stop = stops + i;

            item.linear_gradient_data.color_stops[(i * 4) + 0] = stop->color.red;
            item.linear_gradient_data.color_stops[(i * 4) + 1] = stop->color.green;
            item.linear_gradient_data.color_stops[(i * 4) + 2] = stop->color.blue;
            item.linear_gradient_data.color_stops[(i * 4) + 3] = stop->color.alpha;
            item.linear_gradient_data.color_offsets[i] = stop->offset;
          }

        item.linear_gradient_data.n_color_stops = n_color_stops;
        item.linear_gradient_data.start_point = *start;
        item.linear_gradient_data.end_point = *end;
      }
      break;

    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t transform, transformed_mv;

        graphene_matrix_init_from_matrix (&transform, gsk_transform_node_peek_transform (node));
        graphene_matrix_multiply (&transform, modelview, &transformed_mv);
        gsk_gl_renderer_add_render_item (self,
                                         projection,
                                         &transformed_mv,
                                         render_items,
                                         gsk_transform_node_get_child (node),
                                         render_target,
                                         parent_clip);
      }
      return;

    case GSK_NOT_A_RENDER_NODE:
    case GSK_CONTAINER_NODE:
      g_assert_not_reached ();
      return;

    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
    case GSK_SHADOW_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_BLEND_NODE:
    case GSK_REPEAT_NODE:
    default:
      {
        cairo_surface_t *surface;
        cairo_t *cr;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                              ceilf (item.size.width) * self->scale_factor,
                                              ceilf (item.size.height) * self->scale_factor);
        cairo_surface_set_device_scale (surface, self->scale_factor, self->scale_factor);
        cr = cairo_create (surface);

        cairo_save (cr);
        cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);
        gsk_render_node_draw (node, cr);
        cairo_restore (cr);

#if HIGHLIGHT_FALLBACK
        cairo_move_to (cr, 0, 0);
        cairo_rectangle (cr, 0, 0, item.size.width, item.size.height);
        cairo_set_source_rgba (cr, 1, 0, 0, 1);
        cairo_stroke (cr);
#endif

        cairo_destroy (cr);

        /* Upload the Cairo surface to a GL texture */
        item.texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                        item.size.width * self->scale_factor,
                                                        item.size.height * self->scale_factor);
        gsk_gl_driver_bind_source_texture (self->gl_driver, item.texture_id);
        gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                                 item.texture_id,
                                                 surface,
                                                 GL_NEAREST, GL_NEAREST);

        cairo_surface_destroy (surface);
        item.mode = MODE_TEXTURE;
      }
      break;
    }

  /* Create the vertex buffers holding the geometry of the quad */
  if (item.render_target == 0)
    {
      GskQuadVertex vertex_data[N_VERTICES] = {
        { { item.min.x, item.min.y }, { 0, 0 }, },
        { { item.min.x, item.max.y }, { 0, 1 }, },
        { { item.max.x, item.min.y }, { 1, 0 }, },

        { { item.max.x, item.max.y }, { 1, 1 }, },
        { { item.min.x, item.max.y }, { 0, 1 }, },
        { { item.max.x, item.min.y }, { 1, 0 }, },
      };

      item.vao_id = gsk_gl_driver_create_vao_for_quad (self->gl_driver,
                                                       item.program->position_location,
                                                       item.program->uv_location,
                                                       N_VERTICES,
                                                       vertex_data);
    }
  else
    {
      GskQuadVertex vertex_data[N_VERTICES] = {
        { { item.min.x, item.min.y }, { 0, 1 }, },
        { { item.min.x, item.max.y }, { 0, 0 }, },
        { { item.max.x, item.min.y }, { 1, 1 }, },

        { { item.max.x, item.max.y }, { 1, 0 }, },
        { { item.min.x, item.max.y }, { 0, 0 }, },
        { { item.max.x, item.min.y }, { 1, 1 }, },
      };

      item.vao_id = gsk_gl_driver_create_vao_for_quad (self->gl_driver,
                                                       item.program->position_location,
                                                       item.program->uv_location,
                                                       N_VERTICES,
                                                       vertex_data);
    }

  GSK_NOTE (OPENGL, g_print ("Adding node <%s>[%p] to render items\n",
                             node->name != NULL ? node->name : "unnamed",
                             node));
  g_array_append_val (render_items, item);
}

static void
gsk_gl_renderer_validate_tree (GskGLRenderer           *self,
                               GskRenderNode           *root,
                               const graphene_matrix_t *projection)
{
  graphene_matrix_t modelview;
  GskRoundedRect viewport_clip;

  graphene_matrix_init_scale (&modelview, self->scale_factor, self->scale_factor, 1.0f);

  gdk_gl_context_make_current (self->gl_context);

  gsk_rounded_rect_init_from_rect (&viewport_clip, &self->viewport, 0.0f);

  gsk_gl_renderer_add_render_item (self, projection, &modelview, self->render_items, root,
                                   self->texture_id, &viewport_clip);
}

static void
gsk_gl_renderer_clear_tree (GskGLRenderer *self)
{
  int removed_textures, removed_vaos;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  g_array_remove_range (self->render_items, 0, self->render_items->len);

  removed_textures = gsk_gl_driver_collect_textures (self->gl_driver);
  removed_vaos = gsk_gl_driver_collect_vaos (self->gl_driver);

  GSK_NOTE (OPENGL, g_print ("Collected: %d textures, %d vaos\n",
                             removed_textures,
                             removed_vaos));
}

static void
gsk_gl_renderer_clear (GskGLRenderer *self)
{
  GSK_NOTE (OPENGL, g_print ("Clearing viewport\n"));
  glClearColor (0, 0, 0, 0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void
gsk_gl_renderer_setup_render_mode (GskGLRenderer *self)
{
  switch (self->render_mode)
  {
    case RENDER_FULL:
      glDisable (GL_SCISSOR_TEST);
      break;

    case RENDER_SCISSOR:
      {
        GdkDrawingContext *context = gsk_renderer_get_drawing_context (GSK_RENDERER (self));
        GdkWindow *window = gsk_renderer_get_window (GSK_RENDERER (self));
        cairo_region_t *clip = gdk_drawing_context_get_clip (context);
        cairo_rectangle_int_t extents;
        int window_height;

        /* Fall back to RENDER_FULL */
        if (clip == NULL)
          {
            glDisable (GL_SCISSOR_TEST);
            return;
          }

        g_assert (cairo_region_num_rectangles (clip) == 1);

        window_height = gdk_window_get_height (window) * self->scale_factor;

        cairo_region_get_extents (clip, &extents);
        /*cairo_region_get_rectangle (clip, 0, &extents);*/

        glEnable (GL_SCISSOR_TEST);
        glScissor (extents.x * self->scale_factor,
                   window_height - (extents.height * self->scale_factor) - (extents.y * self->scale_factor),
                   extents.width * self->scale_factor,
                   extents.height * self->scale_factor);

        cairo_region_destroy (clip);
        break;
      }

    default:
      g_assert_not_reached ();
      break;
  }
}


static void
gsk_gl_renderer_do_render (GskRenderer           *renderer,
                           GskRenderNode         *root,
                           const graphene_rect_t *viewport,
                           int                    scale_factor)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_matrix_t modelview, projection;
  guint i;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 gpu_time, cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
#endif

  if (self->gl_context == NULL)
    {
      GSK_NOTE (OPENGL, g_print ("No valid GL context associated to the renderer"));
      return;
    }

  self->viewport = *viewport;

  /* Set up the modelview and projection matrices to fit our viewport */
  graphene_matrix_init_scale (&modelview, scale_factor, scale_factor, 1.0);
  graphene_matrix_init_ortho (&projection,
                              viewport->origin.x,
                              viewport->origin.x + viewport->size.width,
                              viewport->origin.y,
                              viewport->origin.y + viewport->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);

  if (self->texture_id == 0)
    graphene_matrix_scale (&projection, 1, -1, 1);

  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_renderer_validate_tree (self, root, &projection);

#ifdef G_ENABLE_DEBUG
  gsk_gl_profiler_begin_gpu_region (self->gl_profiler);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  /* Ensure that the viewport is up to date */
  gsk_gl_driver_bind_render_target (self->gl_driver, self->texture_id);
  gsk_gl_renderer_resize_viewport (self, viewport);

  gsk_gl_renderer_setup_render_mode (self);

  gsk_gl_renderer_clear (self);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  glEnable (GL_BLEND);
  /* Pre-multiplied alpha! */
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation (GL_FUNC_ADD);

  for (i = 0; i < self->render_items->len; i++)
    {
      const RenderItem *item = &g_array_index (self->render_items, RenderItem, i);

      render_item (self, item);
    }

  /* Draw the output of the GL rendering to the window */
  gsk_gl_driver_end_frame (self->gl_driver);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (profiler, self->profile_counters.frames);

  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gpu_time = gsk_gl_profiler_end_gpu_region (self->gl_profiler);
  gsk_profiler_timer_set (profiler, self->profile_timers.gpu_time, gpu_time);

  gsk_profiler_push_samples (profiler);
#endif
}

static GdkTexture *
gsk_gl_renderer_render_texture (GskRenderer           *renderer,
                                GskRenderNode         *root,
                                const graphene_rect_t *viewport)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  GdkTexture *texture;
  int stride;
  guchar *data;
  int width, height;

  g_return_val_if_fail (self->gl_context != NULL, NULL);

  self->render_mode = RENDER_FULL;
  width = ceilf (viewport->size.width);
  height = ceilf (viewport->size.height);

  gdk_gl_context_make_current (self->gl_context);

  /* Prepare our framebuffer */
  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_renderer_create_buffers (self, width, height, 1);
  gsk_gl_renderer_clear (self);
  gsk_gl_driver_end_frame (self->gl_driver);

  /* Render the actual scene */
  gsk_gl_renderer_do_render (renderer, root, viewport, 1);

  /* Prepare memory for the glReadPixels call */
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
  data = g_malloc (height * stride);

  /* Bind our framebuffer again and read from it */
  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_driver_bind_render_target (self->gl_driver, self->texture_id);
  glReadPixels (0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
  gsk_gl_driver_end_frame (self->gl_driver);

  /* Create texture from the downloaded data */
  texture = gdk_texture_new_for_data (data, width, height, stride);

  return texture;
}

static void
gsk_gl_renderer_render (GskRenderer   *renderer,
                        GskRenderNode *root)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  GdkWindow *window = gsk_renderer_get_window (renderer);
  graphene_rect_t viewport;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  viewport.origin.x = 0;
  viewport.origin.y = 0;
  viewport.size.width = gdk_window_get_width (window) * self->scale_factor;
  viewport.size.height = gdk_window_get_height (window) * self->scale_factor;

  gsk_gl_renderer_do_render (renderer, root, &viewport, self->scale_factor);

  gdk_gl_context_make_current (self->gl_context);
  gsk_gl_renderer_clear_tree (self);
  gsk_gl_renderer_destroy_buffers (self);
}

static void
gsk_gl_renderer_class_init (GskGLRendererClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  gobject_class->dispose = gsk_gl_renderer_dispose;

  renderer_class->realize = gsk_gl_renderer_realize;
  renderer_class->unrealize = gsk_gl_renderer_unrealize;
  renderer_class->begin_draw_frame = gsk_gl_renderer_begin_draw_frame;
  renderer_class->render = gsk_gl_renderer_render;
  renderer_class->render_texture = gsk_gl_renderer_render_texture;
}

static void
gsk_gl_renderer_init (GskGLRenderer *self)
{
  gsk_ensure_resources ();

  graphene_matrix_init_identity (&self->mvp);

  self->render_items = g_array_new (FALSE, FALSE, sizeof (RenderItem));
  g_array_set_clear_func (self->render_items, (GDestroyNotify)destroy_render_item);
  self->scale_factor = 1;

#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));

    self->profile_counters.frames = gsk_profiler_add_counter (profiler, "frames", "Frames", FALSE);
    self->profile_counters.draw_calls = gsk_profiler_add_counter (profiler, "draws", "glDrawArrays", TRUE);

    self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
    self->profile_timers.gpu_time = gsk_profiler_add_timer (profiler, "gpu-time", "GPU time", FALSE, TRUE);
  }
#endif
}
