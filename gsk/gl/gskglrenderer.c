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
#include "gskglglyphcacheprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gskglrenderopsprivate.h"

#include "gskprivate.h"

#include <epoxy/gl.h>
#include <cairo-ft.h>

#define SHADER_VERSION_GLES             100
#define SHADER_VERSION_GL2_LEGACY       110
#define SHADER_VERSION_GL3_LEGACY       130
#define SHADER_VERSION_GL3              150

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

#define HIGHLIGHT_FALLBACK 0
#define DEBUG_OPS          0

#if DEBUG_OPS
#define OP_PRINT(format, ...) g_print(format, ## __VA_ARGS__)
#else
#define OP_PRINT(format, ...)
#endif

#define INIT_PROGRAM_UNIFORM_LOCATION(program_name, uniform_basename) \
              G_STMT_START{\
                self->program_name ## _program.program_name.uniform_basename ## _location = \
                              glGetUniformLocation(self->program_name ## _program.id, "u_" #uniform_basename);\
                g_assert_cmpint (self->program_name ## _program.program_name.uniform_basename ## _location, >, -1); \
              }G_STMT_END

#define INIT_COMMON_UNIFORM_LOCATION(program_ptr, uniform_basename) \
              G_STMT_START{\
                program_ptr->uniform_basename ## _location =  \
                              glGetUniformLocation(program_ptr->id, "u_" #uniform_basename);\
              }G_STMT_END


static void G_GNUC_UNUSED
dump_framebuffer (const char *filename, int w, int h)
{
  int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, w);
  guchar *data = g_malloc (h * stride);
  cairo_surface_t *s;

  glReadPixels (0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, data);
  s = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32, w, h, stride);
  cairo_surface_write_to_png (s, filename);

  cairo_surface_destroy (s);
  g_free (data);
}

static gboolean
font_has_color_glyphs (const PangoFont *font)
{
  cairo_scaled_font_t *scaled_font;
  gboolean has_color = FALSE;

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);
  if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_FT)
    {
      FT_Face ft_face = cairo_ft_scaled_font_lock_face (scaled_font);
      has_color = (FT_HAS_COLOR (ft_face) != 0);
      cairo_ft_scaled_font_unlock_face (scaled_font);
    }

  return has_color;
}

static inline void
rounded_rect_to_floats (const GskRoundedRect *rect,
                        float                *outline,
                        float                *corner_widths,
                        float                *corner_heights)
{
  int i;

  outline[0] = rect->bounds.origin.x;
  outline[1] = rect->bounds.origin.y;
  outline[2] = rect->bounds.size.width;
  outline[3] = rect->bounds.size.height;

  for (i = 0; i < 4; i ++)
    {
      corner_widths[i] = MAX (rect->corner[i].width, 1);
      corner_heights[i] = MAX (rect->corner[i].height, 1);
    }
}

static inline void
rgba_to_float (const GdkRGBA *c,
               float         *f)
{
  f[0] = c->red;
  f[1] = c->green;
  f[2] = c->blue;
  f[3] = c->alpha;
}

static inline void
sort_border_sides (const GdkRGBA *colors,
                   int           *indices)
{
  gboolean done[4] = {0, 0, 0, 0};
  int i, k;
  int cur = 0;

  for (i = 0; i < 3; i ++)
    {
      if (done[i])
        continue;

      indices[cur] = i;
      done[i] = TRUE;
      cur ++;

      for (k = i + 1; k < 4; k ++)
        {
          if (gdk_rgba_equal (&colors[k], &colors[i]))
            {
              indices[cur] = k;
              done[k] = TRUE;
              cur ++;
            }
        }

      if (cur >= 4)
        break;
    }
}

static inline gboolean
color_matrix_modifies_alpha (GskRenderNode *node)
{
  const graphene_matrix_t *matrix = gsk_color_matrix_node_peek_color_matrix (node);
  const graphene_vec4_t *offset = gsk_color_matrix_node_peek_color_offset (node);
  graphene_vec4_t row3;
  graphene_vec4_t id_row3;

  if (graphene_vec4_get_w (offset) != 0.0f)
    return TRUE;

  graphene_vec4_init (&id_row3, 0, 0, 0, 1);
  graphene_matrix_get_row (matrix, 3, &row3);

  return !graphene_vec4_equal (&id_row3, &row3);
}

static void gsk_gl_renderer_setup_render_mode (GskGLRenderer   *self);
static void add_offscreen_ops                 (GskGLRenderer   *self,
                                               RenderOpBuilder *builder,
                                               float            min_x,
                                               float            max_x,
                                               float            min_y,
                                               float            max_y,
                                               GskRenderNode   *child_node,
                                               int             *texture_id,
                                               gboolean        *is_offscreen);
typedef enum
{
  RENDER_FULL,
  RENDER_SCISSOR
} RenderMode;

struct _GskGLRenderer
{
  GskRenderer parent_instance;

  int scale_factor;

  graphene_rect_t viewport;

  guint texture_id;


  GdkGLContext *gl_context;
  GskGLDriver *gl_driver;
  GskGLProfiler *gl_profiler;

  union {
    Program programs[GL_N_PROGRAMS];
    struct {
      Program blend_program;
      Program blit_program;
      Program color_program;
      Program coloring_program;
      Program color_matrix_program;
      Program linear_gradient_program;
      Program blur_program;
      Program inset_shadow_program;
      Program outset_shadow_program;
      Program shadow_program;
      Program border_program;
    };
  };

  GArray *render_ops;

  GskGLGlyphCache glyph_cache;

#ifdef G_ENABLE_DEBUG
  struct {
    GQuark frames;
    GQuark draw_calls;
  } profile_counters;
  struct {
    GQuark cpu_time;
    GQuark gpu_time;
  } profile_timers;
#endif

  RenderMode render_mode;

  gboolean has_buffers : 1;
};

struct _GskGLRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_RENDERER)

static inline void
render_fallback_node (GskGLRenderer       *self,
                      GskRenderNode       *node,
                      RenderOpBuilder     *builder,
                      const GskQuadVertex *vertex_data)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  int texture_id;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceilf (node->bounds.size.width) * self->scale_factor,
                                        ceilf (node->bounds.size.height) * self->scale_factor);
  cairo_surface_set_device_scale (surface, self->scale_factor, self->scale_factor);
  cr = cairo_create (surface);

  cairo_save (cr);
  cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);

#if HIGHLIGHT_FALLBACK
  cairo_move_to (cr, 0, 0);
  cairo_rectangle (cr, 0, 0, node->bounds.size.width, node->bounds.size.height);
  cairo_set_source_rgba (cr, 1, 0, 0, 1);
  cairo_stroke (cr);
#endif
  cairo_destroy (cr);

  /* Upload the Cairo surface to a GL texture */
  texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                             node->bounds.size.width * self->scale_factor,
                                             node->bounds.size.height * self->scale_factor);

  gsk_gl_driver_bind_source_texture (self->gl_driver, texture_id);
  gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                           texture_id,
                                           surface,
                                           GL_NEAREST, GL_NEAREST);

  cairo_surface_destroy (surface);

  ops_set_program (builder, &self->blit_program);
  ops_set_texture (builder, texture_id);
  ops_draw (builder, vertex_data);
}

static inline void
render_text_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder,
                  const GdkRGBA   *color,
                  gboolean         force_color,
                  float            dx,
                  float            dy)
{
  const PangoFont *font = gsk_text_node_peek_font (node);
  const PangoGlyphInfo *glyphs = gsk_text_node_peek_glyphs (node);
  guint num_glyphs = gsk_text_node_get_num_glyphs (node);
  int i;
  int x_position = 0;
  int x = gsk_text_node_get_x (node) + dx;
  int y = gsk_text_node_get_y (node) + dy;

  /* If the font has color glyphs, we don't need to recolor anything */
  if (!force_color && font_has_color_glyphs (font))
    {
      ops_set_program (builder, &self->blit_program);
    }
  else
    {
      ops_set_program (builder, &self->coloring_program);
      ops_set_color (builder, color);
    }

  /* We use one quad per character, unlike the other nodes which
   * use at most one quad altogether */
  for (i = 0; i < num_glyphs; i++)
    {
      const PangoGlyphInfo *gi = &glyphs[i];
      const GskGLCachedGlyph *glyph;
      int glyph_x, glyph_y, glyph_w, glyph_h;
      float tx, ty, tx2, ty2;
      double cx;
      double cy;

      if (gi->glyph == PANGO_GLYPH_EMPTY ||
          (gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG) > 0)
        continue;

      glyph = gsk_gl_glyph_cache_lookup (&self->glyph_cache,
                                         TRUE,
                                         (PangoFont *)font,
                                         gi->glyph,
                                         self->scale_factor);

      /* e.g. whitespace */
      if (glyph->draw_width <= 0 || glyph->draw_height <= 0)
        {
          x_position += gi->geometry.width;
          continue;
        }
      cx = (double)(x_position + gi->geometry.x_offset) / PANGO_SCALE;
      cy = (double)(gi->geometry.y_offset) / PANGO_SCALE;

      ops_set_texture (builder, gsk_gl_glyph_cache_get_glyph_image (&self->glyph_cache,
                                                                   glyph)->texture_id);

      {
        tx  = glyph->tx;
        ty  = glyph->ty;
        tx2 = tx + glyph->tw;
        ty2 = ty + glyph->th;

        glyph_x = x + cx + glyph->draw_x;
        glyph_y = y + cy + glyph->draw_y;
        glyph_w = glyph->draw_width;
        glyph_h = glyph->draw_height;

        GskQuadVertex vertex_data[GL_N_VERTICES] = {
          { { glyph_x,           glyph_y           }, { tx,  ty  }, },
          { { glyph_x,           glyph_y + glyph_h }, { tx,  ty2 }, },
          { { glyph_x + glyph_w, glyph_y           }, { tx2, ty  }, },

          { { glyph_x + glyph_w, glyph_y + glyph_h }, { tx2, ty2 }, },
          { { glyph_x,           glyph_y + glyph_h }, { tx,  ty2 }, },
          { { glyph_x + glyph_w, glyph_y           }, { tx2, ty  }, },
        };

        ops_draw (builder, vertex_data);
      }

      x_position += gi->geometry.width;
    }
}

static inline void
render_border_node (GskGLRenderer   *self,
                    GskRenderNode   *node,
                    RenderOpBuilder *builder)
{
  float min_x = node->bounds.origin.x;
  float min_y = node->bounds.origin.y;
  float max_x = min_x + node->bounds.size.width;
  float max_y = min_y + node->bounds.size.height;
  const GdkRGBA *colors = gsk_border_node_peek_colors (node);
  const GskRoundedRect *rounded_outline = gsk_border_node_peek_outline (node);
  const float *widths = gsk_border_node_peek_widths (node);
  const gboolean needs_clip = TRUE;/*!gsk_rounded_rect_is_rectilinear (rounded_outline);*/
  GskRoundedRect prev_clip;
  graphene_rect_t transformed_clip;
  graphene_rect_t intersection;
  GskRoundedRect child_clip;
  struct {
    float w;
    float h;
  } sizes[4];

  /* Top left */
  sizes[0].w = MAX (MAX (widths[3], rounded_outline->corner[0].width), 1);
  sizes[0].h = MAX (MAX (widths[1], rounded_outline->corner[0].height), 1);

  /* Top right */
  sizes[1].w = MAX (MAX (widths[1], rounded_outline->corner[1].width), 1);
  sizes[1].h = MAX (MAX (widths[0], rounded_outline->corner[1].height), 1);

  /* Bottom right */
  sizes[2].w = MAX (MAX (widths[1], rounded_outline->corner[2].width), 1);
  sizes[2].h = MAX (MAX (widths[2], rounded_outline->corner[2].height), 1);

  /* Bottom left */
  sizes[3].w = MAX (MAX (widths[3], rounded_outline->corner[3].width), 1);
  sizes[3].h = MAX (MAX (widths[2], rounded_outline->corner[3].height), 1);

  if (needs_clip)
    {
      ops_set_program (builder, &self->border_program);

      transformed_clip = rounded_outline->bounds;
      graphene_matrix_transform_bounds (&builder->current_modelview, &transformed_clip, &transformed_clip);

      graphene_rect_intersection (&transformed_clip, &builder->current_clip.bounds,
                                  &intersection);
      gsk_rounded_rect_init (&child_clip, &intersection,
                             &rounded_outline->corner[0],
                             &rounded_outline->corner[1],
                             &rounded_outline->corner[2],
                             &rounded_outline->corner[3]);

      prev_clip = ops_set_clip (builder, &child_clip);

      ops_set_border (builder, widths);
    }
  else
    {
      ops_set_program (builder, &self->color_program);
    }

    {
      const GskQuadVertex side_data[4][6] = {
        /* Top */
        {
          { { min_x,              min_y              }, { 0, 1 }, }, /* Upper left */
          { { min_x + sizes[0].w, min_y + sizes[0].h }, { 0, 0 }, }, /* Lower left */
          { { max_x,              min_y              }, { 1, 1 }, }, /* Upper right */

          { { max_x - sizes[1].w, min_y + sizes[1].h }, { 1, 0 }, }, /* Lower right */
          { { min_x + sizes[0].w, min_y + sizes[0].h }, { 0, 0 }, }, /* Lower left */
          { { max_x,              min_y              }, { 1, 1 }, }, /* Upper right */
        },
        /* Right */
        {
          { { max_x - sizes[1].w, min_y + sizes[1].h }, { 0, 1 }, }, /* Upper left */
          { { max_x - sizes[2].w, max_y - sizes[2].h }, { 0, 0 }, }, /* Lower left */
          { { max_x,              min_y              }, { 1, 1 }, }, /* Upper right */

          { { max_x,              max_y              }, { 1, 0 }, }, /* Lower right */
          { { max_x - sizes[2].w, max_y - sizes[2].h }, { 0, 0 }, }, /* Lower left */
          { { max_x,              min_y              }, { 1, 1 }, }, /* Upper right */
        },
        /* Bottom */
        {
          { { min_x + sizes[3].w, max_y - sizes[3].h }, { 0, 1 }, }, /* Upper left */
          { { min_x,              max_y              }, { 0, 0 }, }, /* Lower left */
          { { max_x - sizes[2].w, max_y - sizes[2].h }, { 1, 1 }, }, /* Upper right */

          { { max_x,              max_y              }, { 1, 0 }, }, /* Lower right */
          { { min_x            ,  max_y              }, { 0, 0 }, }, /* Lower left */
          { { max_x - sizes[2].w, max_y - sizes[2].h }, { 1, 1 }, }, /* Upper right */
        },
        /* Left */
        {
          { { min_x,              min_y              }, { 0, 1 }, }, /* Upper left */
          { { min_x,              max_y              }, { 0, 0 }, }, /* Lower left */
          { { min_x + sizes[0].w, min_y + sizes[0].h }, { 1, 1 }, }, /* Upper right */

          { { min_x + sizes[3].w, max_y - sizes[2].h }, { 1, 0 }, }, /* Lower right */
          { { min_x,              max_y              }, { 0, 0 }, }, /* Lower left */
          { { min_x + sizes[0].w, min_y + sizes[0].h }, { 1, 1 }, }, /* Upper right */
        }
      };
      int indices[4] = { 0, 1, 2, 3 };
      int i;

      /* We sort them by color */
      sort_border_sides (colors, indices);

      for (i = 0; i < 4; i ++)
        {
          ops_set_border_color (builder, &colors[indices[i]]);
          ops_draw (builder, side_data[indices[i]]);
        }
    }

  if (needs_clip)
    ops_set_clip (builder, &prev_clip);
}

static void
gsk_gl_renderer_dispose (GObject *gobject)
{
  GskGLRenderer *self = GSK_GL_RENDERER (gobject);

  g_clear_pointer (&self->render_ops, g_array_unref);

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

static gboolean
gsk_gl_renderer_create_programs (GskGLRenderer  *self,
                                 GError        **error)
{
  GskShaderBuilder *builder;
  GError *shader_error = NULL;
  int i;
  static struct {
    const char *name;
    const char *vs;
    const char *fs;
  } program_definitions[] = {
    { "blend",           "blend.vs.glsl", "blend.fs.glsl" },
    { "blit",            "blit.vs.glsl",  "blit.fs.glsl" },
    { "color",           "blit.vs.glsl",  "color.fs.glsl" },
    { "coloring",        "blit.vs.glsl",  "coloring.fs.glsl" },
    { "color matrix",    "blit.vs.glsl",  "color_matrix.fs.glsl" },
    { "linear gradient", "blit.vs.glsl",  "linear_gradient.fs.glsl" },
    { "blur",            "blit.vs.glsl",  "blur.fs.glsl" },
    { "inset shadow",    "blit.vs.glsl",  "inset_shadow.fs.glsl" },
    { "outset shadow",   "blit.vs.glsl",  "outset_shadow.fs.glsl" },
    { "shadow",          "blit.vs.glsl",  "shadow.fs.glsl" },
    { "border",          "blit.vs.glsl",  "border.fs.glsl" },
  };

  builder = gsk_shader_builder_new ();

  gsk_shader_builder_set_resource_base_path (builder, "/org/gtk/libgsk/glsl");

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

  for (i = 0; i < GL_N_PROGRAMS; i ++)
    {
      Program *prog = &self->programs[i];

      prog->index = i;
      prog->id = gsk_shader_builder_create_program (builder,
                                                    program_definitions[i].vs,
                                                    program_definitions[i].fs,
                                                    &shader_error);

      if (shader_error != NULL)
        {
          g_propagate_prefixed_error (error, shader_error,
                                      "Unable to create '%s' program (from %s and %s):\n",
                                      program_definitions[i].name,
                                      program_definitions[i].vs,
                                      program_definitions[i].fs);

          g_object_unref (builder);
          return FALSE;
        }

      INIT_COMMON_UNIFORM_LOCATION (prog, alpha);
      INIT_COMMON_UNIFORM_LOCATION (prog, source);
      INIT_COMMON_UNIFORM_LOCATION (prog, mask);
      INIT_COMMON_UNIFORM_LOCATION (prog, clip);
      INIT_COMMON_UNIFORM_LOCATION (prog, clip_corner_widths);
      INIT_COMMON_UNIFORM_LOCATION (prog, clip_corner_heights);
      INIT_COMMON_UNIFORM_LOCATION (prog, viewport);
      INIT_COMMON_UNIFORM_LOCATION (prog, projection);
      INIT_COMMON_UNIFORM_LOCATION (prog, modelview);
    }

  /* color */
  INIT_PROGRAM_UNIFORM_LOCATION (color, color);

  /* coloring */
  INIT_PROGRAM_UNIFORM_LOCATION (coloring, color);

  /* color matrix */
  INIT_PROGRAM_UNIFORM_LOCATION (color_matrix, color_matrix);
  INIT_PROGRAM_UNIFORM_LOCATION (color_matrix, color_offset);

  /* linear gradient */
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, color_offsets);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, num_color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, start_point);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, end_point);

  /* blur */
  INIT_PROGRAM_UNIFORM_LOCATION (blur, blur_radius);
  INIT_PROGRAM_UNIFORM_LOCATION (blur, blur_size);

  /* inset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, spread);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, offset);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, outline);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, corner_widths);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, corner_heights);

  /* outset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, spread);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, offset);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, outline);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, corner_widths);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, corner_heights);

  /* shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (shadow, color);

  /* border */
  INIT_PROGRAM_UNIFORM_LOCATION (border, color);
  INIT_PROGRAM_UNIFORM_LOCATION (border, widths);

  g_object_unref (builder);
  return TRUE;
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

  gsk_gl_glyph_cache_init (&self->glyph_cache, self->gl_driver);

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
  g_array_set_size (self->render_ops, 0);


  glDeleteProgram (self->blend_program.id);
  glDeleteProgram (self->blit_program.id);
  glDeleteProgram (self->color_program.id);
  glDeleteProgram (self->coloring_program.id);
  glDeleteProgram (self->color_matrix_program.id);
  glDeleteProgram (self->linear_gradient_program.id);

  gsk_gl_renderer_destroy_buffers (self);

  gsk_gl_glyph_cache_free (&self->glyph_cache);

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


static void
get_gl_scaling_filters (GskRenderNode *node,
                        int           *min_filter_r,
                        int           *mag_filter_r)
{
  *min_filter_r = GL_LINEAR;
  *mag_filter_r = GL_LINEAR;
}

static void
gsk_gl_renderer_clear_tree (GskGLRenderer *self)
{
  int removed_textures;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  g_array_remove_range (self->render_ops, 0, self->render_ops->len);

  removed_textures = gsk_gl_driver_collect_textures (self->gl_driver);

  GSK_NOTE (OPENGL, g_print ("Collected: %d textures\n",
                             removed_textures));
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

        /*cairo_region_get_extents (clip, &extents);*/
        cairo_region_get_rectangle (clip, 0, &extents);

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
gsk_gl_renderer_add_render_ops (GskGLRenderer   *self,
                                GskRenderNode   *node,
                                RenderOpBuilder *builder)
{
  float min_x = node->bounds.origin.x;
  float min_y = node->bounds.origin.y;
  float max_x = min_x + node->bounds.size.width;
  float max_y = min_y + node->bounds.size.height;

  /* Default vertex data */
  const GskQuadVertex vertex_data[GL_N_VERTICES] = {
    { { min_x, min_y }, { 0, 0 }, },
    { { min_x, max_y }, { 0, 1 }, },
    { { max_x, min_y }, { 1, 0 }, },

    { { max_x, max_y }, { 1, 1 }, },
    { { min_x, max_y }, { 0, 1 }, },
    { { max_x, min_y }, { 1, 0 }, },
  };

#if DEBUG_OPS
  if (gsk_render_node_get_node_type (node) != GSK_CONTAINER_NODE)
    g_message ("Adding ops for node %s with type %u", node->name,
               gsk_render_node_get_node_type (node));
#endif

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();

    case GSK_CONTAINER_NODE:
      {
        guint i, p;

        for (i = 0, p = gsk_container_node_get_n_children (node); i < p; i ++)
          {
            GskRenderNode *child = gsk_container_node_get_child (node, i);

            gsk_gl_renderer_add_render_ops (self, child, builder);
          }
      }
    break;

    case GSK_COLOR_NODE:
      {
        ops_set_program (builder, &self->color_program);
        ops_set_color (builder, gsk_color_node_peek_color (node));
        ops_draw (builder, vertex_data);
      }
    break;

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;
        int texture_id;

        get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

        texture_id = gsk_gl_driver_get_texture_for_texture (self->gl_driver,
                                                            texture,
                                                            gl_min_filter,
                                                            gl_mag_filter);
        ops_set_program (builder, &self->blit_program);
        ops_set_texture (builder, texture_id);
        ops_draw (builder, vertex_data);
      }
    break;

    case GSK_CAIRO_NODE:
      {
        const cairo_surface_t *surface = gsk_cairo_node_peek_surface (node);
        int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;
        int texture_id;

        if (surface == NULL)
          return;

        get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

        texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                   max_x - min_x,
                                                   max_y - min_y);
        gsk_gl_driver_bind_source_texture (self->gl_driver, texture_id);
        gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                                 texture_id,
                                                 (cairo_surface_t *)surface,
                                                 gl_min_filter,
                                                 gl_mag_filter);
        ops_set_program (builder, &self->blit_program);
        ops_set_texture (builder, texture_id);
        ops_draw (builder, vertex_data);
      }
    break;

    case GSK_TRANSFORM_NODE:
      {
        GskRenderNode *child = gsk_transform_node_get_child (node);
        graphene_matrix_t prev_mv;
        graphene_matrix_t transform, transformed_mv;

        graphene_matrix_init_from_matrix (&transform, gsk_transform_node_peek_transform (node));
        graphene_matrix_multiply (&transform, &builder->current_modelview, &transformed_mv);
        prev_mv = ops_set_modelview (builder, &transformed_mv);

        gsk_gl_renderer_add_render_ops (self, child, builder);

        ops_set_modelview (builder, &prev_mv);
      }
    break;

    case GSK_OPACITY_NODE:
      {
        int texture_id;
        gboolean is_offscreen;
        float prev_opacity;


        add_offscreen_ops (self, builder, min_x, max_x, min_y, max_y,
                           gsk_opacity_node_get_child (node),
                           &texture_id, &is_offscreen);

        /* Now draw the texture with the node's opacity */
        ops_set_program (builder, &self->blit_program);
        prev_opacity = ops_set_opacity (builder, gsk_opacity_node_get_opacity (node));
        ops_set_texture (builder, texture_id);

        if (is_offscreen)
          {
            GskQuadVertex vertex_data[GL_N_VERTICES] = {
              { { min_x, min_y }, { 0, 1 }, },
              { { min_x, max_y }, { 0, 0 }, },
              { { max_x, min_y }, { 1, 1 }, },

              { { max_x, max_y }, { 1, 0 }, },
              { { min_x, max_y }, { 0, 0 }, },
              { { max_x, min_y }, { 1, 1 }, },
            };
            ops_draw (builder, vertex_data);
          }
        else
          {
            ops_draw (builder, vertex_data);
          }
        ops_set_opacity (builder, prev_opacity);
      }
    break;

    case GSK_LINEAR_GRADIENT_NODE:
      {
        RenderOp op;
        int n_color_stops = MIN (8, gsk_linear_gradient_node_get_n_color_stops (node));
        const GskColorStop *stops = gsk_linear_gradient_node_peek_color_stops (node);
        const graphene_point_t *start = gsk_linear_gradient_node_peek_start (node);
        const graphene_point_t *end = gsk_linear_gradient_node_peek_end (node);
        int i;

        for (i = 0; i < n_color_stops; i ++)
          {
            const GskColorStop *stop = stops + i;

            op.linear_gradient.color_stops[(i * 4) + 0] = stop->color.red;
            op.linear_gradient.color_stops[(i * 4) + 1] = stop->color.green;
            op.linear_gradient.color_stops[(i * 4) + 2] = stop->color.blue;
            op.linear_gradient.color_stops[(i * 4) + 3] = stop->color.alpha;
            op.linear_gradient.color_offsets[i] = stop->offset;
          }

        ops_set_program (builder, &self->linear_gradient_program);
        op.op = OP_CHANGE_LINEAR_GRADIENT;
        op.linear_gradient.n_color_stops = n_color_stops;
        op.linear_gradient.start_point = *start;
        op.linear_gradient.end_point = *end;
        ops_add (builder, &op);

        ops_draw (builder, vertex_data);
      }
    break;

    case GSK_CLIP_NODE:
      {
        GskRoundedRect prev_clip;
        GskRenderNode *child = gsk_clip_node_get_child (node);
        graphene_rect_t transformed_clip;
        graphene_rect_t intersection;
        GskRoundedRect child_clip;

        transformed_clip = *gsk_clip_node_peek_clip (node);
        graphene_matrix_transform_bounds (&builder->current_modelview, &transformed_clip, &transformed_clip);

        graphene_rect_intersection (&transformed_clip,
                                    &builder->current_clip.bounds,
                                    &intersection);

        gsk_rounded_rect_init_from_rect (&child_clip, &intersection, 0.0f);

        prev_clip = ops_set_clip (builder, &child_clip);
        gsk_gl_renderer_add_render_ops (self, child, builder);
        ops_set_clip (builder, &prev_clip);
      }
    break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        GskRoundedRect prev_clip;
        GskRenderNode *child = gsk_rounded_clip_node_get_child (node);
        const GskRoundedRect *rounded_clip = gsk_rounded_clip_node_peek_clip (node);
        graphene_rect_t transformed_clip;
        graphene_rect_t intersection;
        GskRoundedRect child_clip;

        transformed_clip = rounded_clip->bounds;
        graphene_matrix_transform_bounds (&builder->current_modelview, &transformed_clip, &transformed_clip);

        graphene_rect_intersection (&transformed_clip, &builder->current_clip.bounds,
                                    &intersection);
        gsk_rounded_rect_init (&child_clip, &intersection,
                               &rounded_clip->corner[0],
                               &rounded_clip->corner[1],
                               &rounded_clip->corner[2],
                               &rounded_clip->corner[3]);

        prev_clip = ops_set_clip (builder, &child_clip);
        gsk_gl_renderer_add_render_ops (self, child, builder);
        ops_set_clip (builder, &prev_clip);
      }
    break;

    case GSK_TEXT_NODE:
      {
        render_text_node (self, node, builder,
                          gsk_text_node_peek_color (node), FALSE, 0, 0);
      }
    break;

    case GSK_COLOR_MATRIX_NODE:
      {
        int texture_id;
        gboolean is_offscreen;

        add_offscreen_ops (self, builder, min_x, max_x, min_y, max_y,
                           gsk_color_matrix_node_get_child (node),
                           &texture_id, &is_offscreen);

        ops_set_program (builder, &self->color_matrix_program);
        ops_set_color_matrix (builder,
                              gsk_color_matrix_node_peek_color_matrix (node),
                              gsk_color_matrix_node_peek_color_offset (node));

        ops_set_texture (builder, texture_id);

        if (is_offscreen)
          {
            GskQuadVertex vertex_data[GL_N_VERTICES] = {
              { { min_x, min_y }, { 0, 1 }, },
              { { min_x, max_y }, { 0, 0 }, },
              { { max_x, min_y }, { 1, 1 }, },

              { { max_x, max_y }, { 1, 0 }, },
              { { min_x, max_y }, { 0, 0 }, },
              { { max_x, min_y }, { 1, 1 }, },
            };

            ops_draw (builder, vertex_data);
          }
        else
          {
            ops_draw (builder, vertex_data);
          }

      }
    break;

    case GSK_BLUR_NODE:
      {
        int texture_id;
        gboolean is_offscreen;
        RenderOp op;
        add_offscreen_ops (self, builder, min_x, max_x, min_y, max_y,
                           gsk_blur_node_get_child (node),
                           &texture_id, &is_offscreen);

        ops_set_program (builder, &self->blur_program);
        op.op = OP_CHANGE_BLUR;
        graphene_size_init_from_size (&op.blur.size, &node->bounds.size);
        op.blur.radius = gsk_blur_node_get_radius (node);
        ops_add (builder, &op);

        ops_set_texture (builder, texture_id);

        if (is_offscreen)
          {
            GskQuadVertex vertex_data[GL_N_VERTICES] = {
              { { min_x, min_y }, { 0, 1 }, },
              { { min_x, max_y }, { 0, 0 }, },
              { { max_x, min_y }, { 1, 1 }, },

              { { max_x, max_y }, { 1, 0 }, },
              { { min_x, max_y }, { 0, 0 }, },
              { { max_x, min_y }, { 1, 1 }, },
            };

            ops_draw (builder, vertex_data);
          }
        else
          {
            ops_draw (builder, vertex_data);
          }
      }
    break;

    case GSK_INSET_SHADOW_NODE:
      {
        RenderOp op;

        /* TODO: Implement blurred inset shadows as well */
        if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
          {
            render_fallback_node (self, node, builder, vertex_data);
            break;
          }

        op.op = OP_CHANGE_INSET_SHADOW;
        rgba_to_float (gsk_inset_shadow_node_peek_color (node), op.inset_shadow.color);
        rounded_rect_to_floats (gsk_inset_shadow_node_peek_outline (node),
                                op.inset_shadow.outline,
                                op.inset_shadow.corner_widths,
                                op.inset_shadow.corner_heights);
        op.inset_shadow.radius = gsk_inset_shadow_node_get_blur_radius (node);
        op.inset_shadow.spread = gsk_inset_shadow_node_get_spread (node);
        op.inset_shadow.offset[0] = gsk_inset_shadow_node_get_dx (node);
        op.inset_shadow.offset[1] = -gsk_inset_shadow_node_get_dy (node);

        ops_set_program (builder, &self->inset_shadow_program);
        ops_add (builder, &op);
        ops_draw (builder, vertex_data);
      }
    break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        RenderOp op;

        /* TODO: Implement blurred outset shadows as well */
        if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
          {
            render_fallback_node (self, node, builder, vertex_data);
            break;
          }

        op.op = OP_CHANGE_OUTSET_SHADOW;
        rgba_to_float (gsk_outset_shadow_node_peek_color (node), op.outset_shadow.color);
        rounded_rect_to_floats (gsk_outset_shadow_node_peek_outline (node),
                                op.outset_shadow.outline,
                                op.outset_shadow.corner_widths,
                                op.outset_shadow.corner_heights);
        op.outset_shadow.radius = gsk_outset_shadow_node_get_blur_radius (node);
        op.outset_shadow.spread = gsk_outset_shadow_node_get_spread (node);
        op.outset_shadow.offset[0] = gsk_outset_shadow_node_get_dx (node);
        op.outset_shadow.offset[1] = -gsk_outset_shadow_node_get_dy (node);

        ops_set_program (builder, &self->outset_shadow_program);
        ops_add (builder, &op);
        ops_draw (builder, vertex_data);
      }
    break;

    case GSK_SHADOW_NODE:
      {
        GskRenderNode *child = gsk_shadow_node_get_child (node);
        gsize n_shadows = gsk_shadow_node_get_n_shadows (node);
        guint i;

        /* TODO: Implement blurred shadow nodes */;
        for (i = 0; i < n_shadows; i ++)
          {
            const GskShadow *shadow = gsk_shadow_node_peek_shadow (node, i);

            if (shadow->radius > 0)
              {
                render_fallback_node (self, node, builder, vertex_data);
                return;
              }
          }

        for (i = 0; i < n_shadows; i ++)
          {
            const GskShadow *shadow = gsk_shadow_node_peek_shadow (node, i);
            int texture_id;
            gboolean is_offscreen;
            float dx, dy;

            g_assert (shadow->radius <= 0);

            if (gsk_render_node_get_node_type (child) == GSK_TEXT_NODE)
              {
                render_text_node (self, child, builder, &shadow->color, TRUE,
                                  shadow->dx, shadow->dy);
                continue;
              }

            add_offscreen_ops (self, builder, min_x, max_x, min_y, max_y, child, &texture_id, &is_offscreen);

            ops_set_program (builder, &self->shadow_program);
            ops_set_color (builder, &shadow->color);
            ops_set_texture (builder, texture_id);

            dx = shadow->dx;
            dy = shadow->dy;

            if (is_offscreen)
              {
                const GskQuadVertex vertex_data[GL_N_VERTICES] = {
                  { { dx + min_x, dy + min_y }, { 0, 1 }, },
                  { { dx + min_x, dy + max_y }, { 0, 0 }, },
                  { { dx + max_x, dy + min_y }, { 1, 1 }, },

                  { { dx + max_x, dy + max_y }, { 1, 0 }, },
                  { { dx + min_x, dy + max_y }, { 0, 0 }, },
                  { { dx + max_x, dy + min_y }, { 1, 1 }, },
                };
                ops_draw (builder, vertex_data);
              }
            else
              {
                const GskQuadVertex vertex_data[GL_N_VERTICES] = {
                  { { dx + min_x, dy + min_y }, { 0, 0 }, },
                  { { dx + min_x, dy + max_y }, { 0, 1 }, },
                  { { dx + max_x, dy + min_y }, { 1, 0 }, },

                  { { dx + max_x, dy + max_y }, { 1, 1 }, },
                  { { dx + min_x, dy + max_y }, { 0, 1 }, },
                  { { dx + max_x, dy + min_y }, { 1, 0 }, },
                };

                ops_draw (builder, vertex_data);
              }
          }

        /* Now draw the child normally */
        gsk_gl_renderer_add_render_ops (self, child, builder);
      }
    break;

    case GSK_BORDER_NODE:
      {
        /* TODO: The cairo backend is exactly as broken as the code in the following
           function. test case:

          .foo {
            border-left:   50px solid #0f0;
            border-top:    10px solid red;
            border-bottom: 50px solid teal;
            border-right:  100px solid pink;


            border-radius: 100px; 
            min-height:100px;

            float:left;
          }

          Or, like, just circular things.
         */
        render_border_node (self, node, builder);
      }
    break;

    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_BLEND_NODE:
    case GSK_REPEAT_NODE:
    default:
      {
        render_fallback_node (self, node, builder, vertex_data);
      }
    }
}

static void
add_offscreen_ops (GskGLRenderer   *self,
                   RenderOpBuilder *builder,
                   float            min_x,
                   float            max_x,
                   float            min_y,
                   float            max_y,
                   GskRenderNode   *child_node,
                   int             *texture_id,
                   gboolean        *is_offscreen)
{
  int render_target;
  int prev_render_target;
  RenderOp op;
  graphene_matrix_t identity;
  graphene_matrix_t prev_projection;
  graphene_matrix_t prev_modelview;
  graphene_rect_t prev_viewport;
  graphene_matrix_t item_proj;

  /* We need the child node as a texture. If it already is one, we don't need to draw
   * it on a framebuffer of course. */
  if (gsk_render_node_get_node_type (child_node) == GSK_TEXTURE_NODE)
    {
      GdkTexture *texture = gsk_texture_node_get_texture (child_node);
      int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;

      get_gl_scaling_filters (child_node, &gl_min_filter, &gl_mag_filter);

      *texture_id = gsk_gl_driver_get_texture_for_texture (self->gl_driver,
                                                           texture,
                                                           gl_min_filter,
                                                           gl_mag_filter);
      *is_offscreen = FALSE;
      return;
    }

  *texture_id = gsk_gl_driver_create_texture (self->gl_driver, max_x - min_x, max_y - min_y);
  gsk_gl_driver_bind_source_texture (self->gl_driver, *texture_id);
  gsk_gl_driver_init_texture_empty (self->gl_driver, *texture_id);
  render_target = gsk_gl_driver_create_render_target (self->gl_driver, *texture_id, TRUE, TRUE);

  graphene_matrix_init_ortho (&item_proj,
                              min_x, max_x,
                              min_y, max_y,
                              ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE);
  graphene_matrix_scale (&item_proj, 1, -1, 1);
  graphene_matrix_init_identity (&identity);

  prev_render_target = ops_set_render_target (builder, render_target);
  /* Clear since we use this rendertarget for the first time */
  op.op = OP_CLEAR;
  ops_add (builder, &op);
  prev_projection = ops_set_projection (builder, &item_proj);
  prev_modelview = ops_set_modelview (builder, &identity);
  prev_viewport = ops_set_viewport (builder, &GRAPHENE_RECT_INIT (min_x, min_y, max_x - min_x, max_y - min_y));

  gsk_gl_renderer_add_render_ops (self, child_node, builder);

  ops_set_viewport (builder, &prev_viewport);
  ops_set_modelview (builder, &prev_modelview);
  ops_set_projection (builder, &prev_projection);
  ops_set_render_target (builder, prev_render_target);

  *is_offscreen = TRUE;
}

static void
gsk_gl_renderer_render_ops (GskGLRenderer *self,
                            gsize          vertex_data_size)
{
  float mat[16];
  float vec[4];
  guint i;
  guint n_ops = self->render_ops->len;
  const Program *program = NULL;
  gsize buffer_index = 0;
  float *vertex_data = g_malloc (vertex_data_size);

  /*g_message ("%s: Buffer size: %ld", __FUNCTION__, vertex_data_size);*/


  GLuint buffer_id, vao_id;
  glGenVertexArrays (1, &vao_id);
  glBindVertexArray (vao_id);

  glGenBuffers (1, &buffer_id);
  glBindBuffer (GL_ARRAY_BUFFER, buffer_id);


  // Fill buffer data
  for (i = 0; i < n_ops; i ++)
    {
      const RenderOp *op = &g_array_index (self->render_ops, RenderOp, i);

      if (op->op == OP_CHANGE_VAO)
        {
          memcpy (vertex_data + buffer_index, &op->vertex_data, sizeof (GskQuadVertex) * GL_N_VERTICES);
          buffer_index += sizeof (GskQuadVertex) * GL_N_VERTICES / sizeof (float);
        }
    }

  // Set buffer data
  glBufferData (GL_ARRAY_BUFFER, vertex_data_size, vertex_data, GL_STATIC_DRAW);

  // Describe buffer contents

  /* 0 = position location */
  glEnableVertexAttribArray (0);
  glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE,
                         sizeof (GskQuadVertex),
                         (void *) G_STRUCT_OFFSET (GskQuadVertex, position));
  /* 1 = texture coord location */
  glEnableVertexAttribArray (1);
  glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE,
                         sizeof (GskQuadVertex),
                         (void *) G_STRUCT_OFFSET (GskQuadVertex, uv));

  for (i = 0; i < n_ops; i ++)
    {
      const RenderOp *op = &g_array_index (self->render_ops, RenderOp, i);

      if (op->op == OP_NONE ||
          op->op == OP_CHANGE_VAO)
        continue;

      OP_PRINT ("Op %u: %u", i, op->op);

      switch (op->op)
        {
        case OP_CHANGE_PROJECTION:
          graphene_matrix_to_float (&op->projection, mat);
          glUniformMatrix4fv (program->projection_location, 1, GL_FALSE, mat);
          OP_PRINT (" -> Projection");
          /*graphene_matrix_print (&op->projection);*/
          break;

        case OP_CHANGE_MODELVIEW:
          graphene_matrix_to_float (&op->modelview, mat);
          glUniformMatrix4fv (program->modelview_location, 1, GL_FALSE, mat);
          OP_PRINT (" -> Modelview");
          /*graphene_matrix_print (&op->modelview);*/
          break;

        case OP_CHANGE_PROGRAM:
          program = op->program;
          glUseProgram (op->program->id);
          OP_PRINT (" -> Program: %d", op->program->index);
          break;

        case OP_CHANGE_RENDER_TARGET:
          OP_PRINT (" -> Render Target: %d", op->render_target_id);

          glBindFramebuffer (GL_FRAMEBUFFER, op->render_target_id);

          if (op->render_target_id != 0)
            glDisable (GL_SCISSOR_TEST);
          else
            gsk_gl_renderer_setup_render_mode (self); /* Reset glScissor etc. */

          break;

        case OP_CLEAR:
          glClearColor (0, 0, 0, 0);
          glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
          break;

        case OP_CHANGE_VIEWPORT:
          OP_PRINT (" -> New Viewport: %f, %f, %f, %f", op->viewport.origin.x, op->viewport.origin.y, op->viewport.size.width, op->viewport.size.height);
          glUniform4f (program->viewport_location,
                       op->viewport.origin.x, op->viewport.origin.y,
                       op->viewport.size.width, op->viewport.size.height);
          glViewport (0, 0, op->viewport.size.width, op->viewport.size.height);
          break;

        case OP_CHANGE_OPACITY:
          OP_PRINT (" -> Opacity %f", op->opacity);
          glUniform1f (program->alpha_location, op->opacity);
          break;

        case OP_CHANGE_COLOR_MATRIX:
          OP_PRINT (" -> Color Matrix");
          g_assert (program == &self->color_matrix_program);
          graphene_matrix_to_float (&op->color_matrix.matrix, mat);
          glUniformMatrix4fv (program->color_matrix.color_matrix_location, 1, GL_FALSE, mat);

          graphene_vec4_to_float (&op->color_matrix.offset, vec);
          glUniform4fv (program->color_matrix.color_offset_location, 1, vec);
          break;

        case OP_CHANGE_COLOR:
          OP_PRINT (" -> Color: (%f, %f, %f, %f)", op->color.red, op->color.green, op->color.blue, op->color.alpha);
          g_assert (program == &self->color_program || program == &self->coloring_program ||
                    program == &self->shadow_program);
          /* TODO: We use color.color_location here and this is right for all three of the programs above,
           *       but that's just a coincidence. */
          glUniform4f (program->color.color_location,
                       op->color.red, op->color.green, op->color.blue, op->color.alpha);
          break;

        case OP_CHANGE_BORDER_COLOR:
          OP_PRINT (" -> Border color (%f, %f, %f, %f)",
                    op->border.color[0], op->border.color[1], op->border.color[2], op->border.color[3]);
          g_assert (program == &self->border_program);
          glUniform4fv (program->border.color_location, 1, op->border.color);
          break;

        case OP_CHANGE_CLIP:
          OP_PRINT (" -> Clip (%f, %f, %f, %f) (%f, %f, %f, %f), (%f, %f, %f, %f)",
                    op->clip.bounds.origin.x, op->clip.bounds.origin.y,
                    op->clip.bounds.size.width, op->clip.bounds.size.height,
                    MAX (op->clip.corner[0].width, 1),
                    MAX (op->clip.corner[1].width, 1),
                    MAX (op->clip.corner[2].width, 1),
                    MAX (op->clip.corner[3].width, 1),
                    MAX (op->clip.corner[0].height, 1),
                    MAX (op->clip.corner[1].height, 1),
                    MAX (op->clip.corner[2].height, 1),
                    MAX (op->clip.corner[3].height, 1));
          glUniform4f (program->clip_location,
                       op->clip.bounds.origin.x, op->clip.bounds.origin.y,
                       op->clip.bounds.size.width, op->clip.bounds.size.height);

          glUniform4f (program->clip_corner_widths_location,
                       MAX (op->clip.corner[0].width, 1),
                       MAX (op->clip.corner[1].width, 1),
                       MAX (op->clip.corner[2].width, 1),
                       MAX (op->clip.corner[3].width, 1));
          glUniform4f (program->clip_corner_heights_location,
                       MAX (op->clip.corner[0].height, 1),
                       MAX (op->clip.corner[1].height, 1),
                       MAX (op->clip.corner[2].height, 1),
                       MAX (op->clip.corner[3].height, 1));
          break;

        case OP_CHANGE_SOURCE_TEXTURE:
          g_assert(op->texture_id != 0);
          OP_PRINT (" -> New texture: %d", op->texture_id);
          /* Use texture unit 0 for the source */
          glUniform1i (program->source_location, 0);
          glActiveTexture (GL_TEXTURE0);
          glBindTexture (GL_TEXTURE_2D, op->texture_id);

          break;

        case OP_CHANGE_LINEAR_GRADIENT:
            OP_PRINT (" -> Linear gradient");
            glUniform1i (program->linear_gradient.num_color_stops_location,
                         op->linear_gradient.n_color_stops);
            glUniform4fv (program->linear_gradient.color_stops_location,
                          op->linear_gradient.n_color_stops,
                          op->linear_gradient.color_stops);
            glUniform1fv (program->linear_gradient.color_offsets_location,
                          op->linear_gradient.n_color_stops,
                          op->linear_gradient.color_offsets);
            glUniform2f (program->linear_gradient.start_point_location,
                         op->linear_gradient.start_point.x, op->linear_gradient.start_point.y);
            glUniform2f (program->linear_gradient.end_point_location,
                         op->linear_gradient.end_point.x, op->linear_gradient.end_point.y);
          break;

       case OP_CHANGE_BLUR:
          OP_PRINT (" -> Blur");
          g_assert (program == &self->blur_program);
          glUniform1f (program->blur.blur_radius_location, op->blur.radius);
          glUniform2f (program->blur.blur_size_location, op->blur.size.width, op->blur.size.height);
          break;

       case OP_CHANGE_INSET_SHADOW:
          OP_PRINT (" -> inset shadow. Color: (%f, %f, %f, %f), Offset: (%f, %f), Spread: %f, Outline: (%f, %f, %f, %f) Corner widths: (%f, %f, %f, %f), Corner Heights: (%f, %f, %f, %f)",
                    op->inset_shadow.color[0],
                    op->inset_shadow.color[1],
                    op->inset_shadow.color[2],
                    op->inset_shadow.color[3],
                    op->inset_shadow.offset[0],
                    op->inset_shadow.offset[1],
                    op->inset_shadow.spread,
                    op->inset_shadow.outline[0],
                    op->inset_shadow.outline[1],
                    op->inset_shadow.outline[2],
                    op->inset_shadow.outline[3],
                    op->inset_shadow.corner_widths[0],
                    op->inset_shadow.corner_widths[1],
                    op->inset_shadow.corner_widths[2],
                    op->inset_shadow.corner_widths[3],
                    op->inset_shadow.corner_heights[0],
                    op->inset_shadow.corner_heights[1],
                    op->inset_shadow.corner_heights[2],
                    op->inset_shadow.corner_heights[3]);
          g_assert (program == &self->inset_shadow_program);
          glUniform4fv (program->inset_shadow.color_location, 1, op->inset_shadow.color);
          glUniform2fv (program->inset_shadow.offset_location, 1, op->inset_shadow.offset);
          glUniform1f (program->inset_shadow.spread_location, op->inset_shadow.spread);
          glUniform4fv (program->inset_shadow.outline_location, 1, op->inset_shadow.outline);
          glUniform4fv (program->inset_shadow.corner_widths_location, 1, op->inset_shadow.corner_widths);
          glUniform4fv (program->inset_shadow.corner_heights_location, 1, op->inset_shadow.corner_heights);
          break;

       case OP_CHANGE_OUTSET_SHADOW:
          OP_PRINT (" -> outset shadow");
          g_assert (program == &self->outset_shadow_program);
          glUniform4fv (program->outset_shadow.color_location, 1, op->outset_shadow.color);
          glUniform2fv (program->outset_shadow.offset_location, 1, op->outset_shadow.offset);
          glUniform1f (program->outset_shadow.spread_location, op->outset_shadow.spread);
          glUniform4fv (program->outset_shadow.outline_location, 1, op->outset_shadow.outline);
          glUniform4fv (program->outset_shadow.corner_widths_location, 1, op->outset_shadow.corner_widths);
          glUniform4fv (program->outset_shadow.corner_heights_location, 1, op->outset_shadow.corner_heights);
          break;

        case OP_CHANGE_BORDER:
          OP_PRINT (" -> Border (%f, %f, %f, %f)",
                    op->border.widths[0], op->border.widths[1], op->border.widths[2], op->border.widths[3]);
          g_assert (program == &self->border_program);
          glUniform4fv (program->border.widths_location, 1, op->border.widths);
          break;

        case OP_DRAW:
          OP_PRINT (" -> draw %ld, size %ld and program %d\n",
                    op->draw.vao_offset, op->draw.vao_size, program->index);
          glDrawArrays (GL_TRIANGLES, op->draw.vao_offset, op->draw.vao_size);
          break;

        default:
          g_warn_if_reached ();
        }

      OP_PRINT ("\n");
    }

  /* Done drawing, destroy the buffer again.
   * TODO: Can we reuse the memory, though? */
  g_free (vertex_data);
}

static void
gsk_gl_renderer_do_render (GskRenderer           *renderer,
                           GskRenderNode         *root,
                           const graphene_rect_t *viewport,
                           int                    scale_factor)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  RenderOpBuilder render_op_builder;
  graphene_matrix_t modelview, projection;
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
  gsk_gl_glyph_cache_begin_frame (&self->glyph_cache);

  memset (&render_op_builder, 0, sizeof (render_op_builder));
  render_op_builder.renderer = self;
  render_op_builder.current_projection = projection;
  render_op_builder.current_modelview = modelview;
  render_op_builder.current_viewport = *viewport;
  render_op_builder.current_render_target = self->texture_id;
  render_op_builder.current_opacity = 1.0f;
  render_op_builder.render_ops = self->render_ops;
  gsk_rounded_rect_init_from_rect (&render_op_builder.current_clip, &self->viewport, 0.0f);
  gsk_gl_renderer_add_render_ops (self, root, &render_op_builder);

  /*g_message ("Ops: %u", self->render_ops->len);*/

  /* Now actually draw things... */
#ifdef G_ENABLE_DEBUG
  gsk_gl_profiler_begin_gpu_region (self->gl_profiler);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  gsk_gl_renderer_resize_viewport (self, viewport);
  gsk_gl_renderer_setup_render_mode (self);
  gsk_gl_renderer_clear (self);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  /* Pre-multiplied alpha! */
  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation (GL_FUNC_ADD);

  gsk_gl_renderer_render_ops (self, render_op_builder.buffer_size);

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


  self->scale_factor = 1;
  self->render_ops = g_array_new (FALSE, FALSE, sizeof (RenderOp));

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
