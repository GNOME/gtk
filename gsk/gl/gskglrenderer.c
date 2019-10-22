#include "config.h"

#include "gskglrenderer.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
#include "gskgldriverprivate.h"
#include "gskglprofilerprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktransformprivate.h"
#include "gskshaderbuilderprivate.h"
#include "gskglglyphcacheprivate.h"
#include "gskgliconcacheprivate.h"
#include "gskglrenderopsprivate.h"
#include "gskcairoblurprivate.h"
#include "gskglshadowcacheprivate.h"
#include "gskglnodesampleprivate.h"
#include "gsktransform.h"

#include "gskprivate.h"

#include "gdk/gdkgltextureprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdkrgbaprivate.h"

#include <epoxy/gl.h>

#define SHADER_VERSION_GLES             100
#define SHADER_VERSION_GL2_LEGACY       110
#define SHADER_VERSION_GL3_LEGACY       130
#define SHADER_VERSION_GL3              150

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

#define HIGHLIGHT_FALLBACK 0
#define DEBUG_OPS          0

#define SHADOW_EXTRA_SIZE  4

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

typedef enum
{
  FORCE_OFFSCREEN  = 1 << 0,
  RESET_CLIP       = 1 << 1,
  RESET_OPACITY    = 1 << 2,
  DUMP_FRAMEBUFFER = 1 << 3
} OffscreenFlags;

typedef struct
{
  int texture_id;
  float x;
  float y;
  float x2;
  float y2;
} TextureRegion;

static inline void
init_full_texture_region (TextureRegion *r,
                          int            texture_id)
{
  r->texture_id = texture_id;
  r->x = 0;
  r->y = 0;
  r->x2 = 1;
  r->y2 = 1;
}

static void G_GNUC_UNUSED
print_render_node_tree (GskRenderNode *root, int level)
{
#define INDENT 4
  const guint type = gsk_render_node_get_node_type (root);
  guint i;

  switch (type)
    {
      case GSK_CONTAINER_NODE:
        g_print ("%*s Container\n", level * INDENT, " ");
        for (i = 0; i < gsk_container_node_get_n_children (root); i++)
          print_render_node_tree (gsk_container_node_get_child (root, i), level + 1);
        break;

      case GSK_TRANSFORM_NODE:
        g_print ("%*s Transform\n", level * INDENT, " ");
        print_render_node_tree (gsk_transform_node_get_child (root), level + 1);
        break;

      case GSK_COLOR_MATRIX_NODE:
        g_print ("%*s Color Matrix\n", level * INDENT, " ");
        print_render_node_tree (gsk_color_matrix_node_get_child (root), level + 1);
        break;

      case GSK_CROSS_FADE_NODE:
        g_print ("%*s Crossfade(%.2f)\n", level * INDENT, " ",
                 gsk_cross_fade_node_get_progress (root));
        print_render_node_tree (gsk_cross_fade_node_get_start_child (root), level + 1);
        print_render_node_tree (gsk_cross_fade_node_get_end_child (root), level + 1);
        break;

      case GSK_TEXT_NODE:
        g_print ("%*s Text\n", level * INDENT, " ");
        break;

      case GSK_SHADOW_NODE:
        g_print ("%*s Shadow\n", level * INDENT, " ");
        print_render_node_tree (gsk_shadow_node_get_child (root), level + 1);
        break;

      case GSK_TEXTURE_NODE:
        g_print ("%*s Texture %p\n", level * INDENT, " ", gsk_texture_node_get_texture (root));
        break;

      case GSK_DEBUG_NODE:
        g_print ("%*s Debug: %s\n", level * INDENT, " ", gsk_debug_node_get_message (root));
        print_render_node_tree (gsk_debug_node_get_child (root), level + 1);
        break;

      case GSK_CLIP_NODE:
        g_print ("%*s Clip (%f, %f, %f, %f):\n", level * INDENT, " ",
                 root->bounds.origin.x, root->bounds.origin.y, root->bounds.size.width, root->bounds.size.height);
        print_render_node_tree (gsk_clip_node_get_child (root), level + 1);
        break;

      default:
        g_print ("%*s %s\n", level * INDENT, " ", root->node_class->type_name);
    }

#undef INDENT
}


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

static void G_GNUC_UNUSED
dump_node (GskRenderNode *node,
           const char    *filename)
{
  const int surface_width = ceilf (node->bounds.size.width);
  const int surface_height = ceilf (node->bounds.size.height);
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        surface_width,
                                        surface_height);

  cr = cairo_create (surface);
  cairo_save (cr);
  cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);
  cairo_destroy (cr);

  cairo_surface_write_to_png (surface, filename);
  cairo_surface_destroy (surface);
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

  if (graphene_vec4_get_w (offset) != 0.0f)
    return TRUE;

  graphene_matrix_get_row (matrix, 3, &row3);

  return !graphene_vec4_equal (graphene_vec4_w_axis (), &row3);
}

static inline void
gsk_rounded_rect_shrink_to_minimum (GskRoundedRect *self)
{
  self->bounds.size.width = ceilf (MAX (MAX (self->corner[0].width, self->corner[1].width),
                                        MAX (self->corner[2].width, self->corner[3].width)) * 2);
  self->bounds.size.height = ceilf (MAX (MAX (self->corner[0].height, self->corner[1].height),
                                         MAX (self->corner[2].height, self->corner[3].height)) * 2);
}

static inline gboolean
node_supports_transform (GskRenderNode *node)
{
  /* Some nodes can't handle non-trivial transforms without being
   * rendered to a texture (e.g. rotated clips, etc.). Some however
   * work just fine, mostly because they already draw their child
   * to a texture and just render the texture manipulated in some
   * way, think opacity or color matrix. */
  const guint node_type = gsk_render_node_get_node_type (node);

  switch (node_type)
    {
      case GSK_COLOR_NODE:
      case GSK_OPACITY_NODE:
      case GSK_COLOR_MATRIX_NODE:
      case GSK_TEXTURE_NODE:
      case GSK_TRANSFORM_NODE:
      case GSK_CROSS_FADE_NODE:
      case GSK_LINEAR_GRADIENT_NODE:
      case GSK_DEBUG_NODE:
      case GSK_TEXT_NODE:
        return TRUE;

      default:
        return FALSE;
    }
  return FALSE;
}


static void gsk_gl_renderer_setup_render_mode (GskGLRenderer   *self);
static void add_offscreen_ops                 (GskGLRenderer   *self,
                                               RenderOpBuilder       *builder,
                                               const graphene_rect_t *bounds,
                                               GskRenderNode         *child_node,
                                               TextureRegion         *region_out,
                                               gboolean              *is_offscreen,
                                               guint                  flags);
static void gsk_gl_renderer_add_render_ops     (GskGLRenderer   *self,
                                                GskRenderNode   *node,
                                                RenderOpBuilder *builder);

struct _GskGLRenderer
{
  GskRenderer parent_instance;

  int scale_factor;

  GdkGLContext *gl_context;
  GskGLDriver *gl_driver;
  GskGLProfiler *gl_profiler;

  union {
    Program programs[GL_N_PROGRAMS];
    struct {
      Program blit_program;
      Program color_program;
      Program coloring_program;
      Program color_matrix_program;
      Program linear_gradient_program;
      Program blur_program;
      Program inset_shadow_program;
      Program outset_shadow_program;
      Program unblurred_outset_shadow_program;
      Program border_program;
      Program cross_fade_program;
      Program blend_program;
      Program repeat_program;
    };
  };

  RenderOpBuilder op_builder;

  GskGLTextureAtlases *atlases;
  GskGLGlyphCache *glyph_cache;
  GskGLIconCache *icon_cache;
  GskGLShadowCache shadow_cache;

#ifdef G_ENABLE_DEBUG
  struct {
    GQuark frames;
  } profile_counters;
  struct {
    GQuark cpu_time;
    GQuark gpu_time;
  } profile_timers;
#endif

  cairo_region_t *render_region;
};

struct _GskGLRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_RENDERER)

static void G_GNUC_UNUSED
add_rect_ops (RenderOpBuilder       *builder,
              const graphene_rect_t *r)
{
  const float min_x = r->origin.x;
  const float min_y = r->origin.y;
  const float max_x = min_x + r->size.width;
  const float max_y = min_y + r->size.height;

  ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
    { { min_x, min_y }, { 0, 1 }, },
    { { min_x, max_y }, { 0, 0 }, },
    { { max_x, min_y }, { 1, 1 }, },

    { { max_x, max_y }, { 1, 0 }, },
    { { min_x, max_y }, { 0, 0 }, },
    { { max_x, min_y }, { 1, 1 }, },
  });
}

static void G_GNUC_UNUSED
add_rect_outline_ops (GskGLRenderer         *self,
                      RenderOpBuilder       *builder,
                      const graphene_rect_t *rect)
{
  GdkRGBA *color = gdk_rgba_copy (&GDK_RGBA ("F00")); /* Leaked */

  ops_set_program (builder, &self->color_program);
  ops_set_color (builder, color);

  add_rect_ops (builder,
                &GRAPHENE_RECT_INIT (rect->origin.x, rect->origin.y,
                                     1, rect->size.height));
  add_rect_ops (builder,
                &GRAPHENE_RECT_INIT (rect->origin.x, rect->origin.y,
                                     rect->size.width, 1));
  add_rect_ops (builder,
                &GRAPHENE_RECT_INIT (rect->origin.x + rect->size.width - 1, rect->origin.y,
                                     1, rect->size.height));

  add_rect_ops (builder,
                &GRAPHENE_RECT_INIT (rect->origin.x, rect->origin.y + rect->size.height - 1,
                                     rect->size.width, 1));
}

static inline void
rounded_rect_to_floats (GskGLRenderer        *self,
                        RenderOpBuilder      *builder,
                        const GskRoundedRect *rect,
                        float                *outline,
                        float                *corner_widths,
                        float                *corner_heights)
{
  const float scale = ops_get_scale (builder);
  int i;
  graphene_rect_t transformed_bounds;

  ops_transform_bounds_modelview (builder, &rect->bounds, &transformed_bounds);

  outline[0] = transformed_bounds.origin.x;
  outline[1] = transformed_bounds.origin.y;
  outline[2] = transformed_bounds.size.width;
  outline[3] = transformed_bounds.size.height;

  for (i = 0; i < 4; i ++)
    {
      corner_widths[i] = rect->corner[i].width * scale;
      corner_heights[i] = rect->corner[i].height * scale;
    }
}

static inline void
render_fallback_node (GskGLRenderer       *self,
                      GskRenderNode       *node,
                      RenderOpBuilder     *builder)
{
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;
  const float scale = ops_get_scale (builder);
  const int surface_width = ceilf (node->bounds.size.width) * scale;
  const int surface_height = ceilf (node->bounds.size.height) * scale;
  cairo_surface_t *surface;
  cairo_surface_t *rendered_surface;
  cairo_t *cr;
  int cached_id;
  int texture_id;
  const GskQuadVertex offscreen_vertex_data[GL_N_VERTICES] = {
    { { min_x, min_y }, { 0, 1 }, },
    { { min_x, max_y }, { 0, 0 }, },
    { { max_x, min_y }, { 1, 1 }, },

    { { max_x, max_y }, { 1, 0 }, },
    { { min_x, max_y }, { 0, 0 }, },
    { { max_x, min_y }, { 1, 1 }, },
  };


  if (surface_width <= 0 ||
      surface_height <= 0)
    return;

  cached_id = gsk_gl_driver_get_texture_for_pointer (self->gl_driver, node);

  if (cached_id != 0)
    {
      ops_set_program (builder, &self->blit_program);
      ops_set_texture (builder, cached_id);
      ops_draw (builder, offscreen_vertex_data);
      return;
    }


  /* We first draw the recording surface on an image surface,
   * just because the scaleY(-1) later otherwise screws up the
   * rendering... */
  {
    rendered_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                   surface_width,
                                                   surface_height);

    cairo_surface_set_device_scale (rendered_surface, scale, scale);
    cr = cairo_create (rendered_surface);

    cairo_save (cr);
    cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);
    gsk_render_node_draw (node, cr);
    cairo_restore (cr);
    cairo_destroy (cr);
  }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        surface_width,
                                        surface_height);
  cairo_surface_set_device_scale (surface, scale, scale);
  cr = cairo_create (surface);

  /* We draw upside down here, so it matches what GL does. */
  cairo_save (cr);
  cairo_scale (cr, 1, -1);
  cairo_translate (cr, 0, -surface_height / scale);
  cairo_set_source_surface (cr, rendered_surface, 0, 0);
  cairo_rectangle (cr, 0, 0, surface_width / scale, surface_height / scale);
  cairo_fill (cr);
  cairo_restore (cr);

#if HIGHLIGHT_FALLBACK
  if (gsk_render_node_get_node_type (node) != GSK_CAIRO_NODE)
    {
      cairo_move_to (cr, 0, 0);
      cairo_rectangle (cr, 0, 0, node->bounds.size.width, node->bounds.size.height);
      cairo_set_source_rgba (cr, 1, 0, 0, 1);
      cairo_stroke (cr);
    }
#endif
  cairo_destroy (cr);

  /* Upload the Cairo surface to a GL texture */
  texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                             surface_width,
                                             surface_height);
  gsk_gl_driver_bind_source_texture (self->gl_driver, texture_id);
  gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                           texture_id,
                                           surface,
                                           GL_NEAREST, GL_NEAREST);
  gdk_gl_context_label_object_printf  (self->gl_context, GL_TEXTURE, texture_id,
                                       "Fallback %s %d", node->node_class->type_name, texture_id);

  cairo_surface_destroy (surface);
  cairo_surface_destroy (rendered_surface);

  gsk_gl_driver_set_texture_for_pointer (self->gl_driver, node, texture_id);

  ops_set_program (builder, &self->blit_program);
  ops_set_texture (builder, texture_id);
  ops_draw (builder, offscreen_vertex_data);
}

static inline void
render_text_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder,
                  const GdkRGBA   *color,
                  gboolean         force_color)
{
  const PangoFont *font = gsk_text_node_peek_font (node);
  const PangoGlyphInfo *glyphs = gsk_text_node_peek_glyphs (node);
  const float text_scale = ops_get_scale (builder);
  guint num_glyphs = gsk_text_node_get_num_glyphs (node);
  int i;
  int x_position = 0;
  const graphene_point_t *offset = gsk_text_node_get_offset (node);
  float x = offset->x + builder->dx;
  float y = offset->y + builder->dy;
  GlyphCacheKey lookup;

  /* If the font has color glyphs, we don't need to recolor anything */
  if (!force_color && gsk_text_node_has_color_glyphs (node))
    {
      ops_set_program (builder, &self->blit_program);
    }
  else
    {
      ops_set_program (builder, &self->coloring_program);
      ops_set_color (builder, color);
    }

  memset (&lookup, 0, sizeof (CacheKeyData));
  lookup.data.font = (PangoFont *)font;
  lookup.data.scale = (guint) (text_scale * 1024);

  /* We use one quad per character, unlike the other nodes which
   * use at most one quad altogether */
  for (i = 0; i < num_glyphs; i++)
    {
      const PangoGlyphInfo *gi = &glyphs[i];
      const GskGLCachedGlyph *glyph;
      float glyph_x, glyph_y, glyph_x2, glyph_y2;
      float tx, ty, tx2, ty2;
      float cx;
      float cy;

      if (gi->glyph == PANGO_GLYPH_EMPTY)
        continue;

      cx = (float)(x_position + gi->geometry.x_offset) / PANGO_SCALE;
      cy = (float)(gi->geometry.y_offset) / PANGO_SCALE;

      glyph_cache_key_set_glyph_and_shift (&lookup, gi->glyph, x + cx, y + cy);

      gsk_gl_glyph_cache_lookup_or_add (self->glyph_cache,
                                        &lookup,
                                        self->gl_driver,
                                        &glyph);

      if (glyph->texture_id == 0)
        goto next;

      ops_set_texture (builder, glyph->texture_id);

      tx  = glyph->tx;
      ty  = glyph->ty;
      tx2 = tx + glyph->tw;
      ty2 = ty + glyph->th;

      glyph_x = floor (x + cx + 0.125) + glyph->draw_x;
      glyph_y = floor (y + cy + 0.125) + glyph->draw_y;
      glyph_x2 = glyph_x + glyph->draw_width;
      glyph_y2 = glyph_y + glyph->draw_height;

      ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
        { { glyph_x,  glyph_y  }, { tx,  ty  }, },
        { { glyph_x,  glyph_y2 }, { tx,  ty2 }, },
        { { glyph_x2, glyph_y  }, { tx2, ty  }, },

        { { glyph_x2, glyph_y2 }, { tx2, ty2 }, },
        { { glyph_x,  glyph_y2 }, { tx,  ty2 }, },
        { { glyph_x2, glyph_y  }, { tx2, ty  }, },
      });

next:
      x_position += gi->geometry.width;
    }
}

static inline void
render_border_node (GskGLRenderer   *self,
                    GskRenderNode   *node,
                    RenderOpBuilder *builder)
{
  const float scale = ops_get_scale (builder);
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;
  const GdkRGBA *colors = gsk_border_node_peek_colors (node);
  const GskRoundedRect *rounded_outline = gsk_border_node_peek_outline (node);
  const float *og_widths = gsk_border_node_peek_widths (node);
  GskRoundedRect outline;
  float widths[4];
  int i;
  struct {
    float w;
    float h;
  } sizes[4];


  for (i = 0; i < 4; i ++)
    widths[i] = og_widths[i];

  /* Top left */
  if (widths[3] > 0)
    sizes[0].w = MAX (widths[3], rounded_outline->corner[0].width);
  else
    sizes[0].w = 0;

  if (widths[0] > 0)
    sizes[0].h = MAX (widths[0], rounded_outline->corner[0].height);
  else
    sizes[0].h = 0;

  /* Top right */
  if (widths[1] > 0)
    sizes[1].w = MAX (widths[1], rounded_outline->corner[1].width);
  else
    sizes[1].w = 0;

  if (widths[0] > 0)
    sizes[1].h = MAX (widths[0], rounded_outline->corner[1].height);
  else
    sizes[1].h = 0;

  /* Bottom right */
  if (widths[1] > 0)
    sizes[2].w = MAX (widths[1], rounded_outline->corner[2].width);
  else
    sizes[2].w = 0;

  if (widths[2] > 0)
    sizes[2].h = MAX (widths[2], rounded_outline->corner[2].height);
  else
    sizes[2].h = 0;


  /* Bottom left */
  if (widths[3] > 0)
    sizes[3].w = MAX (widths[3], rounded_outline->corner[3].width);
  else
    sizes[3].w = 0;

  if (widths[2] > 0)
    sizes[3].h = MAX (widths[2], rounded_outline->corner[3].height);
  else
    sizes[3].h = 0;

  for (i = 0; i < 4; i ++)
    widths[i] *= scale;

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

        { { min_x + sizes[3].w, max_y - sizes[3].h }, { 1, 0 }, }, /* Lower right */
        { { min_x,              max_y              }, { 0, 0 }, }, /* Lower left */
        { { min_x + sizes[0].w, min_y + sizes[0].h }, { 1, 1 }, }, /* Upper right */
      }
    };
    int indices[4] = { 0, 1, 2, 3 };

    /* We sort them by color */
    sort_border_sides (colors, indices);

    /* Prepare outline */
    outline = *rounded_outline;
    ops_transform_bounds_modelview (builder, &outline.bounds, &outline.bounds);

    for (i = 0; i < 4; i ++)
      {
        outline.corner[i].width *= scale;
        outline.corner[i].height *= scale;
      }

    ops_set_program (builder, &self->border_program);
    ops_set_border_width (builder, widths);
    ops_set_border (builder, &outline);

    for (i = 0; i < 4; i ++)
      {
        if (widths[indices[i]] > 0)
          {
            ops_set_border_color (builder, &colors[indices[i]]);
            ops_draw (builder, side_data[indices[i]]);
          }
      }
  }
}

static inline void
render_color_node (GskGLRenderer       *self,
                   GskRenderNode       *node,
                   RenderOpBuilder     *builder,
                   const GskQuadVertex *vertex_data)
{
  ops_set_program (builder, &self->color_program);
  ops_set_color (builder, gsk_color_node_peek_color (node));
  ops_draw (builder, vertex_data);
}

static inline void
upload_texture (GskGLRenderer *self,
                GdkTexture    *texture,
                TextureRegion *out_region)
{
  if (texture->width <= 128 &&
      texture->height <= 128 &&
      !GDK_IS_GL_TEXTURE (texture))
    {
      const IconData *icon_data;

      gsk_gl_icon_cache_lookup_or_add (self->icon_cache,
                                       texture,
                                       &icon_data);

      out_region->texture_id = icon_data->texture_id;
      out_region->x = icon_data->x;
      out_region->y = icon_data->y;
      out_region->x2 = icon_data->x2;
      out_region->y2 = icon_data->y2;
    }
  else
    {
      out_region->texture_id =
          gsk_gl_driver_get_texture_for_texture (self->gl_driver,
                                                 texture,
                                                 GL_LINEAR,
                                                 GL_LINEAR);

      out_region->x  = 0;
      out_region->y  = 0;
      out_region->x2 = 1;
      out_region->y2 = 1;
    }
}

static inline void
render_texture_node (GskGLRenderer       *self,
                     GskRenderNode       *node,
                     RenderOpBuilder     *builder)
{
  GdkTexture *texture = gsk_texture_node_get_texture (node);
  const int max_texture_size = gsk_gl_driver_get_max_texture_size (self->gl_driver);
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;

  if (texture->width > max_texture_size || texture->height > max_texture_size)
    {
      const float scale_x = (max_x - min_x) / texture->width;
      const float scale_y = (max_y - min_y) / texture->height;
      TextureSlice *slices;
      guint n_slices;
      guint i;

      gsk_gl_driver_slice_texture (self->gl_driver, texture, &slices, &n_slices);

      ops_set_program (builder, &self->blit_program);
      for (i = 0; i < n_slices; i ++)
        {
          const TextureSlice *slice = &slices[i];
          float x1, x2, y1, y2;

          x1 = min_x + (scale_x * slice->rect.x);
          x2 = x1 + (slice->rect.width * scale_x);
          y1 = min_y + (scale_y * slice->rect.y);
          y2 = y1 + (slice->rect.height * scale_y);

          ops_set_texture (builder, slice->texture_id);
          ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
            { { x1, y1 }, { 0, 0 }, },
            { { x1, y2 }, { 0, 1 }, },
            { { x2, y1 }, { 1, 0 }, },

            { { x2, y2 }, { 1, 1 }, },
            { { x1, y2 }, { 0, 1 }, },
            { { x2, y1 }, { 1, 0 }, },
          });
        }
    }
  else
    {
      TextureRegion r;

      upload_texture (self, texture, &r);

      ops_set_program (builder, &self->blit_program);
      ops_set_texture (builder, r.texture_id);

      ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
        { { min_x, min_y }, { r.x,  r.y  }, },
        { { min_x, max_y }, { r.x,  r.y2 }, },
        { { max_x, min_y }, { r.x2, r.y  }, },

        { { max_x, max_y }, { r.x2, r.y2 }, },
        { { min_x, max_y }, { r.x,  r.y2 }, },
        { { max_x, min_y }, { r.x2, r.y  }, },
      });
    }
}

static inline void
render_transform_node (GskGLRenderer   *self,
                       GskRenderNode   *node,
                       RenderOpBuilder *builder)
{
  GskTransform *node_transform = gsk_transform_node_get_transform (node);
  const GskTransformCategory category = gsk_transform_get_category (node_transform);
  GskRenderNode *child = gsk_transform_node_get_child (node);

  switch (category)
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      gsk_gl_renderer_add_render_ops (self, child, builder);
    break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;

        gsk_transform_to_translate (node_transform, &dx, &dy);

        ops_offset (builder, dx, dy);
        gsk_gl_renderer_add_render_ops (self, child, builder);
        ops_offset (builder, -dx, -dy);
      }
    break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        ops_push_modelview (builder, node_transform);
        gsk_gl_renderer_add_render_ops (self, child, builder);
        ops_pop_modelview (builder);
      }
    break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    default:
      {

        if (node_supports_transform (child))
          {
            ops_push_modelview (builder, node_transform);
            gsk_gl_renderer_add_render_ops (self, child, builder);
            ops_pop_modelview (builder);
          }
        else
          {
            const float min_x = child->bounds.origin.x;
            const float min_y = child->bounds.origin.y;
            const float max_x = min_x + child->bounds.size.width;
            const float max_y = min_y + child->bounds.size.height;
            TextureRegion region;
            gboolean is_offscreen;
            /* For non-trivial transforms, we draw everything on a texture and then
             * draw the texture transformed. */
            /* TODO: We should compute a modelview containing only the "non-trivial"
             *       part (e.g. the rotation) and use that. We want to keep the scale
             *       for the texture.
             */
            add_offscreen_ops (self, builder,
                               &child->bounds,
                               child,
                               &region, &is_offscreen,
                               RESET_CLIP | RESET_OPACITY);

            ops_push_modelview (builder, node_transform);
            ops_set_texture (builder, region.texture_id);
            ops_set_program (builder, &self->blit_program);

            if (is_offscreen)
              {
                const GskQuadVertex offscreen_vertex_data[GL_N_VERTICES] = {
                  { { min_x, min_y }, { region.x,  region.y2 }, },
                  { { min_x, max_y }, { region.x,  region.y  }, },
                  { { max_x, min_y }, { region.x2, region.y2 }, },

                  { { max_x, max_y }, { region.x2, region.y  }, },
                  { { min_x, max_y }, { region.x,  region.y  }, },
                  { { max_x, min_y }, { region.x2, region.y2 }, },
                };

                ops_draw (builder, offscreen_vertex_data);
              }
            else
              {
                const GskQuadVertex onscreen_vertex_data[GL_N_VERTICES] = {
                  { { min_x, min_y }, { region.x,  region.y  }, },
                  { { min_x, max_y }, { region.x,  region.y2 }, },
                  { { max_x, min_y }, { region.x2, region.y  }, },

                  { { max_x, max_y }, { region.x2, region.y2 }, },
                  { { min_x, max_y }, { region.x,  region.y2 }, },
                  { { max_x, min_y }, { region.x2, region.y  }, },
                };

                ops_draw (builder, onscreen_vertex_data);
              }

            ops_pop_modelview (builder);
          }
      }
    }
}

static inline void
render_opacity_node (GskGLRenderer   *self,
                     GskRenderNode   *node,
                     RenderOpBuilder *builder)
{
  GskRenderNode *child = gsk_opacity_node_get_child (node);
  const float opacity = gsk_opacity_node_get_opacity (node);
  float prev_opacity;

  if (gsk_render_node_get_node_type (child) == GSK_CONTAINER_NODE)
    {
      const float min_x = builder->dx + node->bounds.origin.x;
      const float min_y = builder->dy + node->bounds.origin.y;
      const float max_x = min_x + node->bounds.size.width;
      const float max_y = min_y + node->bounds.size.height;
      gboolean is_offscreen;
      TextureRegion region;

      /* The semantics of an opacity node mandate that when, e.g., two color nodes overlap,
       * there may not be any blending between them */
      add_offscreen_ops (self, builder, &child->bounds,
                         child,
                         &region, &is_offscreen,
                         FORCE_OFFSCREEN | RESET_OPACITY | RESET_CLIP);

      prev_opacity = ops_set_opacity (builder,
                                      builder->current_opacity * opacity);

      ops_set_program (builder, &self->blit_program);
      ops_set_texture (builder, region.texture_id);

      ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
        { { min_x, min_y }, { region.x,  region.y2 }, },
        { { min_x, max_y }, { region.x,  region.y  }, },
        { { max_x, min_y }, { region.x2, region.y2 }, },

        { { max_x, max_y }, { region.x2, region.y  }, },
        { { min_x, max_y }, { region.x,  region.y  }, },
        { { max_x, min_y }, { region.x2, region.y2 }, },
      });
    }
  else
    {
      prev_opacity = ops_set_opacity (builder,
                                      builder->current_opacity * opacity);

      gsk_gl_renderer_add_render_ops (self, child, builder);
    }

  ops_set_opacity (builder, prev_opacity);
}

static inline void
render_linear_gradient_node (GskGLRenderer       *self,
                             GskRenderNode       *node,
                             RenderOpBuilder     *builder,
                             const GskQuadVertex *vertex_data)
{
  OpLinearGradient *op;
  int n_color_stops = MIN (8, gsk_linear_gradient_node_get_n_color_stops (node));
  const GskColorStop *stops = gsk_linear_gradient_node_peek_color_stops (node);
  const graphene_point_t *start = gsk_linear_gradient_node_peek_start (node);
  const graphene_point_t *end = gsk_linear_gradient_node_peek_end (node);
  int i;

  ops_set_program (builder, &self->linear_gradient_program);

  op = ops_begin (builder, OP_CHANGE_LINEAR_GRADIENT);

  for (i = 0; i < n_color_stops; i ++)
    {
      const GskColorStop *stop = stops + i;

      op->color_stops[(i * 4) + 0] = stop->color.red;
      op->color_stops[(i * 4) + 1] = stop->color.green;
      op->color_stops[(i * 4) + 2] = stop->color.blue;
      op->color_stops[(i * 4) + 3] = stop->color.alpha;
      op->color_offsets[i] = stop->offset;
    }

  op->n_color_stops = n_color_stops;
  op->start_point.x = start->x + builder->dx;
  op->start_point.y = start->y + builder->dy;
  op->end_point.x = end->x + builder->dx;
  op->end_point.y = end->y + builder->dy;

  ops_draw (builder, vertex_data);
}

static inline void
render_clip_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder)
{
  const graphene_rect_t *clip = gsk_clip_node_peek_clip (node);
  GskRenderNode *child = gsk_clip_node_get_child (node);
  graphene_rect_t transformed_clip;
  graphene_rect_t intersection;
  GskRoundedRect child_clip;

  ops_transform_bounds_modelview (builder, clip, &transformed_clip);

  graphene_rect_intersection (&transformed_clip,
                              &builder->current_clip->bounds,
                              &intersection);

  gsk_rounded_rect_init_from_rect (&child_clip, &intersection, 0.0f);

  ops_push_clip (builder, &child_clip);
  gsk_gl_renderer_add_render_ops (self, child, builder);
  ops_pop_clip (builder);
}

static inline void
get_inner_rect (const GskRoundedRect *rect,
                graphene_rect_t       *out)
{
  const float left = MAX (rect->corner[GSK_CORNER_TOP_LEFT].width,
                          rect->corner[GSK_CORNER_BOTTOM_LEFT].width);
  const float top  = MAX (rect->corner[GSK_CORNER_TOP_LEFT].height,
                          rect->corner[GSK_CORNER_TOP_RIGHT].height);

  out->origin.x = rect->bounds.origin.x + left;
  out->origin.y = rect->bounds.origin.y + top;

  out->size.width = rect->bounds.size.width - left -
                    MAX (rect->corner[GSK_CORNER_TOP_RIGHT].width,
                         rect->corner[GSK_CORNER_BOTTOM_RIGHT].width);

  out->size.height = rect->bounds.size.height - top -
                     MAX (rect->corner[GSK_CORNER_BOTTOM_LEFT].height,
                          rect->corner[GSK_CORNER_BOTTOM_RIGHT].height);
}

static inline void
render_rounded_clip_node (GskGLRenderer       *self,
                          GskRenderNode       *node,
                          RenderOpBuilder     *builder)
{
  const float scale = ops_get_scale (builder);
  GskRoundedRect child_clip = *gsk_rounded_clip_node_peek_clip (node);
  GskRoundedRect transformed_clip;
  GskRenderNode *child = gsk_rounded_clip_node_get_child (node);
  gboolean need_offscreen;
  int i;

  transformed_clip = child_clip;
  ops_transform_bounds_modelview (builder, &child_clip.bounds, &transformed_clip.bounds);

  if (!ops_has_clip (builder))
    need_offscreen = FALSE;
  else if (graphene_rect_contains_rect (&builder->current_clip->bounds,
                                        &transformed_clip.bounds))
    need_offscreen = FALSE;
  else
    need_offscreen = TRUE;

  if (!need_offscreen)
    {
      /* If they don't intersect at all, we can simply set
       * the new clip and add the render ops */
      for (i = 0; i < 4; i ++)
        {
          transformed_clip.corner[i].width *= scale;
          transformed_clip.corner[i].height *= scale;
        }

      ops_push_clip (builder, &transformed_clip);
      gsk_gl_renderer_add_render_ops (self, child, builder);
      ops_pop_clip (builder);
    }
  else
    {
      const float min_x = builder->dx + node->bounds.origin.x;
      const float min_y = builder->dy + node->bounds.origin.y;
      const float max_x = min_x + node->bounds.size.width;
      const float max_y = min_y + node->bounds.size.height;
      graphene_matrix_t scale_matrix;
      gboolean is_offscreen;
      TextureRegion region;
      /* NOTE: We are *not* transforming the clip by the current modelview here.
       *       We instead draw the untransformed clip to a texture and then transform
       *       that texture.
       *
       *       We do, however, apply the scale factor to the child clip of course.
       */

      graphene_matrix_init_scale (&scale_matrix, scale, scale, 1.0f);
      graphene_matrix_transform_bounds (&scale_matrix, &child_clip.bounds, &child_clip.bounds);

      /* Increase corner radius size by scale factor */
      for (i = 0; i < 4; i ++)
        {
          child_clip.corner[i].width *= scale;
          child_clip.corner[i].height *= scale;
        }

      ops_push_clip (builder, &child_clip);
      add_offscreen_ops (self, builder, &node->bounds,
                         child,
                         &region, &is_offscreen,
                         FORCE_OFFSCREEN | RESET_OPACITY);
      ops_pop_clip (builder);

      ops_set_program (builder, &self->blit_program);
      ops_set_texture (builder, region.texture_id);

      ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
        { { min_x, min_y }, { 0, 1 }, },
        { { min_x, max_y }, { 0, 0 }, },
        { { max_x, min_y }, { 1, 1 }, },

        { { max_x, max_y }, { 1, 0 }, },
        { { min_x, max_y }, { 0, 0 }, },
        { { max_x, min_y }, { 1, 1 }, },
      });
    }
  /* Else this node is entirely out of the current clip node and we don't draw it anyway. */
}

static inline void
render_color_matrix_node (GskGLRenderer       *self,
                          GskRenderNode       *node,
                          RenderOpBuilder     *builder)
{
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;
  GskRenderNode *child = gsk_color_matrix_node_get_child (node);
  TextureRegion region;
  gboolean is_offscreen;

  add_offscreen_ops (self, builder,
                     &node->bounds,
                     child,
                     &region, &is_offscreen,
                     RESET_CLIP | RESET_OPACITY);

  ops_set_program (builder, &self->color_matrix_program);
  ops_set_color_matrix (builder,
                        gsk_color_matrix_node_peek_color_matrix (node),
                        gsk_color_matrix_node_peek_color_offset (node));

  ops_set_texture (builder, region.texture_id);

  if (is_offscreen)
    {
      GskQuadVertex offscreen_vertex_data[GL_N_VERTICES] = {
        { { min_x, min_y }, { region.x,  region.y2 }, },
        { { min_x, max_y }, { region.x,  region.y  }, },
        { { max_x, min_y }, { region.x2, region.y2 }, },

        { { max_x, max_y }, { region.x2, region.y  }, },
        { { min_x, max_y }, { region.x,  region.y  }, },
        { { max_x, min_y }, { region.x2, region.y2 }, },
      };

      ops_draw (builder, offscreen_vertex_data);
    }
  else
    {
      const GskQuadVertex onscreen_vertex_data[GL_N_VERTICES] = {
        { { min_x, min_y }, { region.x,  region.y }, },
        { { min_x, max_y }, { region.x,  region.y2 }, },
        { { max_x, min_y }, { region.x2, region.y }, },

        { { max_x, max_y }, { region.x2, region.y2 }, },
        { { min_x, max_y }, { region.x,  region.y2 }, },
        { { max_x, min_y }, { region.x2, region.y }, },
      };

      ops_draw (builder, onscreen_vertex_data);
    }
}

static inline void
flip_vertex_data_y (GskQuadVertex vertex_data[GL_N_VERTICES])
{
  vertex_data[0].uv[1] = 1 - vertex_data[0].uv[1];
  vertex_data[1].uv[1] = 1 - vertex_data[1].uv[1];
  vertex_data[2].uv[1] = 1 - vertex_data[2].uv[1];
  vertex_data[3].uv[1] = 1 - vertex_data[3].uv[1];
  vertex_data[4].uv[1] = 1 - vertex_data[4].uv[1];
  vertex_data[5].uv[1] = 1 - vertex_data[5].uv[1];
}

static inline void
render_blur_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder,
                  GskQuadVertex   *vertex_data)
{
  const float blur_radius = gsk_blur_node_get_radius (node);
  GskRenderNode *child = gsk_blur_node_get_child (node);
  TextureRegion region;
  gboolean is_offscreen;
  OpBlur *op;

  if (blur_radius <= 0)
    {
      gsk_gl_renderer_add_render_ops (self, child, builder);
      return;
    }

  /* TODO(perf): We're forcing the child offscreen even if it's a texture
   * so the resulting offscreen texture is bigger by the gaussian blur factor
   * (see gsk_blur_node_new), but we didn't have to do that if the blur
   * shader could handle that situation. */

  add_offscreen_ops (self, builder,
                     &node->bounds,
                     child,
                     &region, &is_offscreen,
                     RESET_CLIP | FORCE_OFFSCREEN | RESET_OPACITY);

  ops_set_program (builder, &self->blur_program);

  op = ops_begin (builder, OP_CHANGE_BLUR);
  graphene_size_init_from_size (&op->size, &node->bounds.size);
  op->radius = blur_radius;

  ops_set_texture (builder, region.texture_id);

  if (is_offscreen)
    flip_vertex_data_y (vertex_data);

  ops_draw (builder, vertex_data);
}

static inline void
render_inset_shadow_node (GskGLRenderer       *self,
                          GskRenderNode       *node,
                          RenderOpBuilder     *builder,
                          const GskQuadVertex *vertex_data)
{
  const float scale = ops_get_scale (builder);
  OpShadow *op;

  /* TODO: Implement blurred inset shadows as well */
  if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
    {
      render_fallback_node (self, node, builder);
      return;
    }

  ops_set_program (builder, &self->inset_shadow_program);

  op = ops_begin (builder, OP_CHANGE_INSET_SHADOW);
  rgba_to_float (gsk_inset_shadow_node_peek_color (node), op->color);
  rounded_rect_to_floats (self, builder,
                          gsk_inset_shadow_node_peek_outline (node),
                          op->outline,
                          op->corner_widths,
                          op->corner_heights);
  op->radius = gsk_inset_shadow_node_get_blur_radius (node) * scale;
  op->spread = gsk_inset_shadow_node_get_spread (node) * scale;
  op->offset[0] = gsk_inset_shadow_node_get_dx (node) * scale;
  op->offset[1] = -gsk_inset_shadow_node_get_dy (node) * scale;

  ops_draw (builder, vertex_data);
}

static inline void
render_unblurred_outset_shadow_node (GskGLRenderer       *self,
                                     GskRenderNode       *node,
                                     RenderOpBuilder     *builder,
                                     const GskQuadVertex *vertex_data)
{
  const float scale = ops_get_scale (builder);
  const GskRoundedRect *outline = gsk_outset_shadow_node_peek_outline (node);
  const float spread = gsk_outset_shadow_node_get_spread (node);
  const float dx = gsk_outset_shadow_node_get_dx (node);
  const float dy = gsk_outset_shadow_node_get_dy (node);
  OpShadow *op;

  ops_set_program (builder, &self->unblurred_outset_shadow_program);
  op = ops_begin (builder, OP_CHANGE_UNBLURRED_OUTSET_SHADOW);
  rgba_to_float (gsk_outset_shadow_node_peek_color (node), op->color);

  rounded_rect_to_floats (self, builder,
                          outline,
                          op->outline,
                          op->corner_widths,
                          op->corner_heights);

  op->spread = spread * scale;
  op->offset[0] = dx * scale;
  op->offset[1] = - dy * scale;

  ops_draw (builder, vertex_data);
}

static inline void
render_outset_shadow_node (GskGLRenderer       *self,
                           GskRenderNode       *node,
                           RenderOpBuilder     *builder)
{
  const GskRoundedRect *outline = gsk_outset_shadow_node_peek_outline (node);
  GskRoundedRect offset_outline;
  const float blur_radius = gsk_outset_shadow_node_get_blur_radius (node);
  const float blur_extra = gsk_cairo_blur_compute_pixels (blur_radius);
  const float spread = gsk_outset_shadow_node_get_spread (node);
  const float dx = gsk_outset_shadow_node_get_dx (node);
  const float dy = gsk_outset_shadow_node_get_dy (node);
  const float min_x = builder->dx + outline->bounds.origin.x - spread - blur_extra / 2.0;
  const float min_y = builder->dy + outline->bounds.origin.y - spread - blur_extra / 2.0;
  const float max_x = min_x + outline->bounds.size.width  + (spread + blur_extra/2.0) * 2;
  const float max_y = min_y + outline->bounds.size.height + (spread + blur_extra/2.0) * 2;
  float texture_width, texture_height;
  OpBlur *op;
  OpShadow *shadow;
  graphene_matrix_t prev_projection;
  graphene_rect_t prev_viewport;
  graphene_matrix_t item_proj;
  int blurred_texture_id;
  int cached_tid;

  /* offset_outline is the minimal outline we need to draw the given drop shadow,
   * enlarged by the spread and offset by the blur radius. */
  offset_outline = *outline;
  /* Shrink our outline to the minimum size that can still hold all the border radii */
  gsk_rounded_rect_shrink_to_minimum (&offset_outline);
  /* Increase by the spread */
  gsk_rounded_rect_shrink (&offset_outline, -spread, -spread, -spread, -spread);
  /* No we need to incorporate the blur radius; since we blur an edge an equal blur_extra/2.0
   * on both sides, the minimum side of both width and height needs to be blur_extra */
  offset_outline.bounds.size.width = MAX (offset_outline.bounds.size.width, blur_extra);
  offset_outline.bounds.size.height = MAX (offset_outline.bounds.size.height, blur_extra);
  /* For the center part, we add a few pixels */
  offset_outline.bounds.size.width += SHADOW_EXTRA_SIZE;
  offset_outline.bounds.size.height += SHADOW_EXTRA_SIZE;
  offset_outline.bounds.origin.x = blur_extra / 2.0f;
  offset_outline.bounds.origin.y = blur_extra / 2.0f;

  texture_width = offset_outline.bounds.size.width   + blur_extra;
  texture_height = offset_outline.bounds.size.height + blur_extra;

  cached_tid = gsk_gl_shadow_cache_get_texture_id (&self->shadow_cache,
                                                   self->gl_driver,
                                                   &offset_outline,
                                                   blur_radius);
  if (cached_tid == 0)
    {
      int texture_id, render_target;
      int blurred_render_target;
      int prev_render_target;
      GskRoundedRect blit_clip;

      gsk_gl_driver_create_render_target (self->gl_driver, texture_width, texture_height,
                                          &texture_id, &render_target);
      gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, texture_id,
                                          "Outset Shadow Temp %d", texture_id);
      gdk_gl_context_label_object_printf  (self->gl_context, GL_FRAMEBUFFER, render_target,
                                           "Outset Shadow FB Temp %d", render_target);

      graphene_matrix_init_ortho (&item_proj,
                                  0, texture_width, 0, texture_height,
                                  ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE);
      graphene_matrix_scale (&item_proj, 1, -1, 1);

      prev_render_target = ops_set_render_target (builder, render_target);
      ops_begin (builder, OP_CLEAR);
      prev_projection = ops_set_projection (builder, &item_proj);
      ops_set_modelview (builder, NULL); /* Modelview */
      prev_viewport = ops_set_viewport (builder, &GRAPHENE_RECT_INIT (0, 0, texture_width, texture_height));

      /* Draw outline */
      ops_set_program (builder, &self->color_program);
      ops_push_clip (builder, &offset_outline);
      ops_set_color (builder, gsk_outset_shadow_node_peek_color (node));
      ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
        { { 0,                            }, { 0, 1 }, },
        { { 0,             texture_height }, { 0, 0 }, },
        { { texture_width,                }, { 1, 1 }, },

        { { texture_width, texture_height }, { 1, 0 }, },
        { { 0,             texture_height }, { 0, 0 }, },
        { { texture_width,                }, { 1, 1 }, },
      });

      gsk_gl_driver_create_render_target (self->gl_driver, texture_width, texture_height,
                                          &blurred_texture_id, &blurred_render_target);
      gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, blurred_texture_id,
                                          "Outset Shadow Cache %d", blurred_texture_id);
      gdk_gl_context_label_object_printf  (self->gl_context, GL_FRAMEBUFFER, render_target,
                                           "Outset Shadow Cache FB %d", render_target);

      ops_set_render_target (builder, blurred_render_target);
      ops_pop_clip (builder);
      ops_begin (builder, OP_CLEAR);

      gsk_rounded_rect_init_from_rect (&blit_clip,
                                       &GRAPHENE_RECT_INIT (0, 0, texture_width, texture_height), 0.0f);

      ops_set_program (builder, &self->blur_program);

      op = ops_begin (builder, OP_CHANGE_BLUR);
      op->size.width = texture_width;
      op->size.height = texture_height;
      op->radius = blur_radius;

      ops_push_clip (builder, &blit_clip);
      ops_set_texture (builder, texture_id);
      ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
        { { 0,             0              }, { 0, 1 }, },
        { { 0,             texture_height }, { 0, 0 }, },
        { { texture_width, 0              }, { 1, 1 }, },

        { { texture_width, texture_height }, { 1, 0 }, },
        { { 0,             texture_height }, { 0, 0 }, },
        { { texture_width, 0              }, { 1, 1 }, },
      });


      ops_pop_clip (builder);
      ops_set_viewport (builder, &prev_viewport);
      ops_pop_modelview (builder);
      ops_set_projection (builder, &prev_projection);
      ops_set_render_target (builder, prev_render_target);

      gsk_gl_driver_mark_texture_permanent (self->gl_driver, blurred_texture_id);
      gsk_gl_shadow_cache_commit (&self->shadow_cache,
                                  &offset_outline,
                                  blur_radius,
                                  blurred_texture_id);
    }
  else
    {
      blurred_texture_id = cached_tid;
    }

  ops_set_program (builder, &self->outset_shadow_program);
  ops_set_texture (builder, blurred_texture_id);

  shadow = ops_begin (builder, OP_CHANGE_OUTSET_SHADOW);
  rounded_rect_to_floats (self, builder,
                          outline,
                          shadow->outline,
                          shadow->corner_widths,
                          shadow->corner_heights);

  /* We use the one outset shadow op from above to draw all 8 sides/corners. */
  {
    const GskRoundedRect *o = &offset_outline;
    float top_height = MAX (o->corner[0].height, o->corner[1].height);
    float bottom_height = MAX (o->corner[2].height, o->corner[3].height);
    float left_width = MAX (o->corner[0].width, o->corner[3].width);
    float right_width = MAX (o->corner[1].width, o->corner[2].width);
    float x1, x2, y1, y2, tx1, tx2, ty1, ty2;

    top_height    = MAX (top_height,    blur_extra / 2.0f) + (blur_extra / 2.0f);
    bottom_height = MAX (bottom_height, blur_extra / 2.0f) + (blur_extra / 2.0f);
    left_width    = MAX (left_width,    blur_extra / 2.0f) + (blur_extra / 2.0f);
    right_width   = MAX (right_width,   blur_extra / 2.0f) + (blur_extra / 2.0f);

    /* Top left */
    if (top_height > 0 && left_width > 0)
      {
        x1 = min_x + dx;
        x2 = min_x + dx + left_width;
        y1 = min_y + dy;
        y2 = min_y + dy + top_height;
        tx1 = 0;
        tx2 = left_width / texture_width;
        ty1 = 1 - (top_height / texture_height);
        ty2 = 1;
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Top right */
    if (top_height > 0 && right_width > 0)
      {
        x1 = max_x + dx - right_width;
        x2 = max_x + dx;
        y1 = min_y + dy;
        y2 = min_y + dy + top_height;
        tx1 = 1 - (right_width / texture_width);
        tx2 = 1;
        ty1 = 1 - (top_height / texture_height);
        ty2 = 1;
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Bottom right */
    if (bottom_height > 0 && left_width > 0)
      {
        x1 = max_x + dx - right_width;
        x2 = max_x + dx;
        y1 = max_y + dy - bottom_height;
        y2 = max_y + dy;
        tx1 = 1 - (right_width / texture_width);
        tx2 = 1;
        ty1 = 0;
        ty2 = (bottom_height / texture_height);
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Bottom left */
    if (bottom_height > 0 && left_width > 0)
      {
        x1 = min_x + dx;
        x2 = min_x + dx + left_width;
        y1 = max_y + dy - bottom_height;
        y2 = max_y + dy;
        tx1 = 0;
        tx2 = left_width / texture_width;
        ty1 = 0;
        ty2 = bottom_height / texture_height;
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Left side */
    if (left_width > 0)
      {
        x1 = min_x + dx;
        x2 = min_x + dx + left_width;
        y1 = min_y + dy + top_height;
        y2 = max_y + dy - bottom_height;
        tx1 = 0;
        tx2 = left_width / texture_width;
        ty1 = 0.5f - SHADOW_EXTRA_SIZE / 2.0f / texture_height;
        ty2 = ty1 + (SHADOW_EXTRA_SIZE / texture_height);
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Right side */
    if (right_width > 0)
      {
        x1 = max_x + dx - right_width;
        x2 = max_x + dx;
        y1 = min_y + dy + top_height;
        y2 = max_y + dy - bottom_height;
        tx1 = 1 - (right_width / texture_width);
        tx2 = 1;
        ty1 = 0.5f - SHADOW_EXTRA_SIZE / 2.0f / texture_height;
        ty2 = ty1 + (SHADOW_EXTRA_SIZE / texture_height);
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Top side */
    if (top_height > 0)
      {
        x1 = min_x + dx + left_width;
        x2 = max_x + dx - right_width;
        y1 = min_y + dy;
        y2 = min_y + dy + top_height;
        tx1 = 0.5f - (SHADOW_EXTRA_SIZE / 2.0f / texture_width);
        tx2 = tx1 + (SHADOW_EXTRA_SIZE / texture_width);
        ty1 = 1 - (top_height / texture_height);
        ty2 = 1;
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    /* Bottom side */
    if (bottom_height > 0)
      {
        x1 = min_x + dx + left_width;
        x2 = max_x + dx - right_width;
        y1 = max_y + dy - bottom_height;
        y2 = max_y + dy;
        tx1 = 0.5f - (SHADOW_EXTRA_SIZE / 2.0f / texture_width);
        tx2 = tx1 + (SHADOW_EXTRA_SIZE / texture_width);
        ty1 = 0;
        ty2 = bottom_height / texture_height;
        ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
          { { x1, y1 }, { tx1, ty2 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },

          { { x2, y2 }, { tx2, ty1 }, },
          { { x1, y2 }, { tx1, ty1 }, },
          { { x2, y1 }, { tx2, ty2 }, },
        });
      }

    if (dx != 0 || dy != 0)
      {
        /* Middle */
        x1 = min_x + dx + left_width;
        x2 = max_x + dx - right_width;
        y1 = min_y + dy + top_height;
        y2 = max_y + dy - bottom_height;
        if (x2 > x1 && y2 > y1)
          {
            tx1 = (texture_width - SHADOW_EXTRA_SIZE)  / 2.0f / texture_width;
            tx2 = (texture_width + SHADOW_EXTRA_SIZE)  / 2.0f / texture_width;
            ty1 = (texture_height - SHADOW_EXTRA_SIZE) / 2.0f / texture_height;
            ty2 = (texture_height + SHADOW_EXTRA_SIZE) / 2.0f / texture_height;
            ops_draw (builder, (GskQuadVertex[GL_N_VERTICES]) {
              { { x1, y1 }, { tx1, ty2 }, },
              { { x1, y2 }, { tx1, ty1 }, },
              { { x2, y1 }, { tx2, ty2 }, },

              { { x2, y2 }, { tx2, ty1 }, },
              { { x1, y2 }, { tx1, ty1 }, },
              { { x2, y1 }, { tx2, ty2 }, },
            });
          }
      }

  }
}

static inline void
render_shadow_node (GskGLRenderer       *self,
                    GskRenderNode       *node,
                    RenderOpBuilder     *builder)
{
  float min_x;
  float min_y;
  float max_x;
  float max_y;
  GskRenderNode *original_child = gsk_shadow_node_get_child (node);
  GskRenderNode *shadow_child = original_child;
  gsize n_shadows = gsk_shadow_node_get_n_shadows (node);
  guint i;

  /* TODO: Implement blurred shadow nodes */;
  for (i = 0; i < n_shadows; i ++)
    {
      const GskShadow *shadow = gsk_shadow_node_peek_shadow (node, i);

      if (shadow->radius > 0)
        {
          render_fallback_node (self, node, builder);
          return;
        }
    }

  /* Shadow nodes recolor every pixel of the source texture, but leave the alpha in tact.
   * If the child is a color matrix node that doesn't touch the alpha, we can throw that away. */
  if (gsk_render_node_get_node_type (shadow_child) == GSK_COLOR_MATRIX_NODE &&
      !color_matrix_modifies_alpha (shadow_child))
    {
      shadow_child = gsk_color_matrix_node_get_child (shadow_child);
    }

  min_x = builder->dx + shadow_child->bounds.origin.x;
  min_y = builder->dy + shadow_child->bounds.origin.y;
  max_x = min_x + shadow_child->bounds.size.width;
  max_y = min_y + shadow_child->bounds.size.height;

  for (i = 0; i < n_shadows; i ++)
    {
      const GskShadow *shadow = gsk_shadow_node_peek_shadow (node, i);
      const float dx = shadow->dx;
      const float dy = shadow->dy;
      TextureRegion region;
      gboolean is_offscreen;

      g_assert (shadow->radius <= 0);

      if (gsk_render_node_get_node_type (shadow_child) == GSK_TEXT_NODE)
        {
          ops_offset (builder, dx, dy);
          render_text_node (self, shadow_child, builder, &shadow->color, TRUE);
          ops_offset (builder, - dx, - dy);
          continue;
        }

      if (gdk_rgba_is_clear (&shadow->color))
        continue;

      /* Draw the child offscreen, without the offset. */
      add_offscreen_ops (self, builder,
                         &shadow_child->bounds,
                         shadow_child, &region, &is_offscreen,
                         RESET_CLIP | RESET_OPACITY);

      ops_set_program (builder, &self->coloring_program);
      ops_set_color (builder, &shadow->color);
      ops_set_texture (builder, region.texture_id);
      if (is_offscreen)
        {
          const GskQuadVertex offscreen_vertex_data[GL_N_VERTICES] = {
            { { dx + min_x, dy + min_y }, { region.x,  region.y2 }, },
            { { dx + min_x, dy + max_y }, { region.x,  region.y }, },
            { { dx + max_x, dy + min_y }, { region.x2, region.y2 }, },

            { { dx + max_x, dy + max_y }, { region.x2, region.y }, },
            { { dx + min_x, dy + max_y }, { region.x,  region.y }, },
            { { dx + max_x, dy + min_y }, { region.x2, region.y2 }, },
          };

          ops_draw (builder, offscreen_vertex_data);
        }
      else
        {
          const GskQuadVertex onscreen_vertex_data[GL_N_VERTICES] = {
            { { dx + min_x, dy + min_y }, { region.x,  region.y }, },
            { { dx + min_x, dy + max_y }, { region.x,  region.y2 }, },
            { { dx + max_x, dy + min_y }, { region.x2, region.y }, },

            { { dx + max_x, dy + max_y }, { region.x2, region.y2 }, },
            { { dx + min_x, dy + max_y }, { region.x,  region.y2 }, },
            { { dx + max_x, dy + min_y }, { region.x2, region.y }, },
          };

          ops_draw (builder, onscreen_vertex_data);
        }
    }

  /* Now draw the child normally */
  gsk_gl_renderer_add_render_ops (self, original_child, builder);
}

static inline void
render_cross_fade_node (GskGLRenderer   *self,
                        GskRenderNode   *node,
                        RenderOpBuilder *builder,
                        GskQuadVertex    vertex_data[GL_N_VERTICES])
{
  GskRenderNode *start_node = gsk_cross_fade_node_get_start_child (node);
  GskRenderNode *end_node = gsk_cross_fade_node_get_end_child (node);
  float progress = gsk_cross_fade_node_get_progress (node);
  TextureRegion start_region;
  TextureRegion end_region;
  gboolean is_offscreen1, is_offscreen2;
  OpCrossFade *op;

  flip_vertex_data_y (vertex_data);

  /* TODO: We create 2 textures here as big as the cross-fade node, but both the
   * start and the end node might be a lot smaller than that. */

  add_offscreen_ops (self, builder,
                     &node->bounds,
                     start_node,
                     &start_region, &is_offscreen1,
                     FORCE_OFFSCREEN | RESET_CLIP | RESET_OPACITY);

  add_offscreen_ops (self, builder,
                     &node->bounds,
                     end_node,
                     &end_region, &is_offscreen2,
                     FORCE_OFFSCREEN | RESET_CLIP | RESET_OPACITY);

  ops_set_program (builder, &self->cross_fade_program);

  op = ops_begin (builder, OP_CHANGE_CROSS_FADE);
  op->progress = progress;
  op->source2 = end_region.texture_id;

  ops_set_texture (builder, start_region.texture_id);

  ops_draw (builder, vertex_data);
}

static inline void
render_blend_node (GskGLRenderer   *self,
                   GskRenderNode   *node,
                   RenderOpBuilder *builder,
                   GskQuadVertex    vertex_data[GL_N_VERTICES])
{
  GskRenderNode *top_child = gsk_blend_node_get_top_child (node);
  GskRenderNode *bottom_child = gsk_blend_node_get_bottom_child (node);
  TextureRegion top_region;
  TextureRegion bottom_region;
  gboolean is_offscreen1, is_offscreen2;
  OpBlend *op;

  flip_vertex_data_y (vertex_data);

  /* TODO: We create 2 textures here as big as the blend node, but both the
   * start and the end node might be a lot smaller than that. */
  add_offscreen_ops (self, builder,
                     &node->bounds,
                     bottom_child,
                     &bottom_region, &is_offscreen1,
                     FORCE_OFFSCREEN | RESET_CLIP);

  add_offscreen_ops (self, builder,
                     &node->bounds,
                     top_child,
                     &top_region, &is_offscreen2,
                     FORCE_OFFSCREEN | RESET_CLIP);

  ops_set_program (builder, &self->blend_program);
  ops_set_texture (builder, bottom_region.texture_id);

  op = ops_begin (builder, OP_CHANGE_BLEND);
  op->source2 = top_region.texture_id;
  op->mode = gsk_blend_node_get_blend_mode (node);

  ops_draw (builder, vertex_data);
}

static inline void
render_repeat_node (GskGLRenderer   *self,
                    GskRenderNode   *node,
                    RenderOpBuilder *builder)
{
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;
  GskRenderNode *child = gsk_repeat_node_get_child (node);
  const graphene_rect_t *child_bounds = gsk_repeat_node_peek_child_bounds (node);
  TextureRegion region;
  gboolean is_offscreen;
  OpRepeat *op;

  if (child_bounds != NULL &&
      !graphene_rect_equal (child_bounds, &child->bounds))
    {
      /* TODO: Implement these repeat nodes. */
      render_fallback_node (self, node, builder);
      return;
    }

  /* Draw the entire child on a texture */
  add_offscreen_ops (self, builder,
                     &child->bounds,
                     child,
                     &region, &is_offscreen,
                     RESET_CLIP | RESET_OPACITY);

  ops_set_program (builder, &self->repeat_program);
  ops_set_texture (builder, region.texture_id);

  op = ops_begin (builder, OP_CHANGE_REPEAT);
  op->child_bounds[0] = 0; /* Both currently unused */
  op->child_bounds[1] = 0;
  op->child_bounds[2] = node->bounds.size.width / child_bounds->size.width;
  op->child_bounds[3] = node->bounds.size.height / child_bounds->size.height;

  op->texture_rect[0] = region.x;
  op->texture_rect[2] = region.x2;

  if (is_offscreen)
    {
      op->texture_rect[1] = region.y2;
      op->texture_rect[3] = region.y;
    }
  else
    {
      op->texture_rect[1] = region.y;
      op->texture_rect[3] = region.y2;
    }

  if (is_offscreen)
    {
      const GskQuadVertex offscreen_vertex_data[GL_N_VERTICES] = {
        { { min_x, min_y }, { region.x,  region.y2 }, },
        { { min_x, max_y }, { region.x,  region.y  }, },
        { { max_x, min_y }, { region.x2, region.y2 }, },

        { { max_x, max_y }, { region.x2, region.y  }, },
        { { min_x, max_y }, { region.x,  region.y  }, },
        { { max_x, min_y }, { region.x2, region.y2 }, },
      };

      ops_draw (builder, offscreen_vertex_data);
    }
  else
    {
      const GskQuadVertex onscreen_vertex_data[GL_N_VERTICES] = {
        { { min_x, min_y }, { region.x,  region.y  }, },
        { { min_x, max_y }, { region.x,  region.y2 }, },
        { { max_x, min_y }, { region.x2, region.y  }, },

        { { max_x, max_y }, { region.x2, region.y2 }, },
        { { min_x, max_y }, { region.x,  region.y2 }, },
        { { max_x, min_y }, { region.x2, region.y  }, },
      };

      ops_draw (builder, onscreen_vertex_data);
    }
}

static inline void
apply_viewport_op (const Program    *program,
                   const OpViewport *op)
{
  OP_PRINT (" -> New Viewport: %f, %f, %f, %f",
            op->viewport.origin.x, op->viewport.origin.y,
            op->viewport.size.width, op->viewport.size.height);
  glUniform4f (program->viewport_location,
               op->viewport.origin.x, op->viewport.origin.y,
               op->viewport.size.width, op->viewport.size.height);
  glViewport (0, 0, op->viewport.size.width, op->viewport.size.height);
}

static inline void
apply_modelview_op (const Program  *program,
                    const OpMatrix *op)
{
  float mat[16];

  OP_PRINT (" -> Modelview");
  graphene_matrix_to_float (&op->matrix, mat);
  glUniformMatrix4fv (program->modelview_location, 1, GL_FALSE, mat);
}

static inline void
apply_projection_op (const Program  *program,
                     const OpMatrix *op)
{
  float mat[16];

  OP_PRINT (" -> Projection");
  graphene_matrix_to_float (&op->matrix, mat);
  glUniformMatrix4fv (program->projection_location, 1, GL_FALSE, mat);
}

static inline void
apply_program_op (const Program  *program,
                  const OpProgram *op)
{
  OP_PRINT (" -> Program: %d", op->program->index);
  glUseProgram (op->program->id);
}

static inline void
apply_render_target_op (GskGLRenderer        *self,
                        const Program        *program,
                        const OpRenderTarget *op)
{
  OP_PRINT (" -> Render Target: %d", op->render_target_id);

  glBindFramebuffer (GL_FRAMEBUFFER, op->render_target_id);

  if (op->render_target_id != 0)
    glDisable (GL_SCISSOR_TEST);
  else
    gsk_gl_renderer_setup_render_mode (self); /* Reset glScissor etc. */
}

static inline void
apply_color_op (const Program *program,
                const OpColor *op)
{
  OP_PRINT (" -> Color: (%f, %f, %f, %f)",
            op->rgba.red, op->rgba.green, op->rgba.blue, op->rgba.alpha);
  glUniform4f (program->color.color_location,
               op->rgba.red, op->rgba.green, op->rgba.blue, op->rgba.alpha);
}

static inline void
apply_opacity_op (const Program   *program,
                  const OpOpacity *op)
{
  OP_PRINT (" -> Opacity %f", op->opacity);
  glUniform1f (program->alpha_location, op->opacity);
}

static inline void
apply_source_texture_op (const Program   *program,
                         const OpTexture *op)
{
  g_assert(op->texture_id != 0);
  OP_PRINT (" -> New texture: %d", op->texture_id);
  /* Use texture unit 0 for the source */
  glUniform1i (program->source_location, 0);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, op->texture_id);
}

static inline void
apply_color_matrix_op (const Program       *program,
                       const OpColorMatrix *op)
{
  float mat[16];
  float vec[4];
  OP_PRINT (" -> Color Matrix");
  graphene_matrix_to_float (&op->matrix, mat);
  glUniformMatrix4fv (program->color_matrix.color_matrix_location, 1, GL_FALSE, mat);

  graphene_vec4_to_float (&op->offset, vec);
  glUniform4fv (program->color_matrix.color_offset_location, 1, vec);
}

static inline void
apply_clip_op (const Program *program,
               const OpClip  *op)
{
  OP_PRINT (" -> Clip (%f, %f, %f, %f) (%f, %f, %f, %f), (%f, %f, %f, %f)",
            op->clip.bounds.origin.x, op->clip.bounds.origin.y,
            op->clip.bounds.size.width, op->clip.bounds.size.height,
            op->clip.corner[0].width,
            op->clip.corner[1].width,
            op->clip.corner[2].width,
            op->clip.corner[3].width,
            op->clip.corner[0].height,
            op->clip.corner[1].height,
            op->clip.corner[2].height,
            op->clip.corner[3].height);
  glUniform4f (program->clip_location,
               op->clip.bounds.origin.x, op->clip.bounds.origin.y,
               op->clip.bounds.size.width, op->clip.bounds.size.height);

  glUniform4f (program->clip_corner_widths_location,
               op->clip.corner[0].width,
               op->clip.corner[1].width,
               op->clip.corner[2].width,
               op->clip.corner[3].width);
  glUniform4f (program->clip_corner_heights_location,
               op->clip.corner[0].height,
               op->clip.corner[1].height,
               op->clip.corner[2].height,
               op->clip.corner[3].height);
}

static inline void
apply_inset_shadow_op (const Program  *program,
                       const OpShadow *op)
{
  OP_PRINT (" -> inset shadow. Color: (%f, %f, %f, %f), Offset: (%f, %f), Spread: %f, Outline: (%f, %f, %f, %f) Corner widths: (%f, %f, %f, %f), Corner Heights: (%f, %f, %f, %f)",
            op->color[0],
            op->color[1],
            op->color[2],
            op->color[3],
            op->offset[0],
            op->offset[1],
            op->spread,
            op->outline[0],
            op->outline[1],
            op->outline[2],
            op->outline[3],
            op->corner_widths[0],
            op->corner_widths[1],
            op->corner_widths[2],
            op->corner_widths[3],
            op->corner_heights[0],
            op->corner_heights[1],
            op->corner_heights[2],
            op->corner_heights[3]);
  glUniform4fv (program->inset_shadow.color_location, 1, op->color);
  glUniform2fv (program->inset_shadow.offset_location, 1, op->offset);
  glUniform1f (program->inset_shadow.spread_location, op->spread);
  glUniform4fv (program->inset_shadow.outline_location, 1, op->outline);
  glUniform4fv (program->inset_shadow.corner_widths_location, 1, op->corner_widths);
  glUniform4fv (program->inset_shadow.corner_heights_location, 1, op->corner_heights);
}

static inline void
apply_unblurred_outset_shadow_op (const Program  *program,
                                  const OpShadow *op)
{
  OP_PRINT (" -> unblurred outset shadow");
  glUniform4fv (program->unblurred_outset_shadow.color_location, 1, op->color);
  glUniform2fv (program->unblurred_outset_shadow.offset_location, 1, op->offset);
  glUniform1f (program->unblurred_outset_shadow.spread_location, op->spread);
  glUniform4fv (program->unblurred_outset_shadow.outline_location, 1, op->outline);
  glUniform4fv (program->unblurred_outset_shadow.corner_widths_location, 1,
                op->corner_widths);
  glUniform4fv (program->unblurred_outset_shadow.corner_heights_location, 1,
                op->corner_heights);
}

static inline void
apply_outset_shadow_op (const Program  *program,
                        const OpShadow *op)
{
  OP_PRINT (" -> outset shadow");
  glUniform4fv (program->outset_shadow.outline_location, 1, op->outline);
  glUniform4fv (program->outset_shadow.corner_widths_location, 1, op->corner_widths);
  glUniform4fv (program->outset_shadow.corner_heights_location, 1, op->corner_heights);
}

static inline void
apply_linear_gradient_op (const Program          *program,
                          const OpLinearGradient *op)
{
  OP_PRINT (" -> Linear gradient");
  glUniform1i (program->linear_gradient.num_color_stops_location,
               op->n_color_stops);
  glUniform4fv (program->linear_gradient.color_stops_location,
                op->n_color_stops,
                op->color_stops);
  glUniform1fv (program->linear_gradient.color_offsets_location,
                op->n_color_stops,
                op->color_offsets);
  glUniform2f (program->linear_gradient.start_point_location,
               op->start_point.x, op->start_point.y);
  glUniform2f (program->linear_gradient.end_point_location,
               op->end_point.x, op->end_point.y);
}

static inline void
apply_border_op (const Program  *program,
                 const OpBorder *op)
{
  const GskRoundedRect *o = &op->outline;
  float outline[4];
  float widths[4];
  float heights[4];
  int i;
  OP_PRINT (" -> Border Outline");

  outline[0] = o->bounds.origin.x;
  outline[1] = o->bounds.origin.y;
  outline[2] = o->bounds.size.width;
  outline[3] = o->bounds.size.height;

  for (i = 0; i < 4; i ++)
    {
      widths[i] = o->corner[i].width;
      heights[i] = o->corner[i].height;
    }

  glUniform4fv (program->border.outline_location, 1, outline);
  glUniform4fv (program->border.corner_widths_location, 1, widths);
  glUniform4fv (program->border.corner_heights_location, 1, heights);
}

static inline void
apply_border_width_op (const Program  *program,
                       const OpBorder *op)
{
  OP_PRINT (" -> Border width (%f, %f, %f, %f)",
            op->widths[0], op->widths[1], op->widths[2], op->widths[3]);

  glUniform4fv (program->border.widths_location, 1, op->widths);
}

static inline void
apply_border_color_op (const Program  *program,
                       const OpBorder *op)
{
  OP_PRINT (" -> Border color (%f, %f, %f, %f)",
            op->color[0], op->color[1], op->color[2], op->color[3]);
  glUniform4fv (program->border.color_location, 1, op->color);
}

static inline void
apply_blur_op (const Program *program,
               const OpBlur  *op)
{
  OP_PRINT (" -> Blur");
  glUniform1f (program->blur.blur_radius_location, op->radius);
  glUniform2f (program->blur.blur_size_location, op->size.width, op->size.height);
  /*glUniform2f (program->blur.dir_location, op->dir[0], op->dir[1]);*/
}

static inline void
apply_cross_fade_op (const Program     *program,
                     const OpCrossFade *op)
{
  /* End texture id */
  glUniform1i (program->cross_fade.source2_location, 1);
  glActiveTexture (GL_TEXTURE0 + 1);
  glBindTexture (GL_TEXTURE_2D, op->source2);
  /* progress */
  glUniform1f (program->cross_fade.progress_location, op->progress);
}

static inline void
apply_blend_op (const Program *program,
                const OpBlend *op)
{
  /* End texture id */
  glUniform1i (program->blend.source2_location, 1);
  glActiveTexture (GL_TEXTURE0 + 1);
  glBindTexture (GL_TEXTURE_2D, op->source2);
  /* progress */
  glUniform1i (program->blend.mode_location, op->mode);
}

static inline void
apply_repeat_op (const Program  *program,
                 const OpRepeat *op)
{
  glUniform4fv (program->repeat.child_bounds_location, 1, op->child_bounds);
  glUniform4fv (program->repeat.texture_rect_location, 1, op->texture_rect);
}

static void
gsk_gl_renderer_dispose (GObject *gobject)
{
  GskGLRenderer *self = GSK_GL_RENDERER (gobject);

  ops_free (&self->op_builder);

  G_OBJECT_CLASS (gsk_gl_renderer_parent_class)->dispose (gobject);
}

static gboolean
gsk_gl_renderer_create_programs (GskGLRenderer  *self,
                                 GError        **error)
{
  GskShaderBuilder *builder;
  GError *shader_error = NULL;
  int i;
  static const struct {
    const char *name;
    const char *fs;
    const char *vs;
  } program_definitions[] = {
    { "blit",                      "blit.fs.glsl" },
    { "color",                     "color.fs.glsl" },
    { "coloring",                  "coloring.fs.glsl" },
    { "color matrix",              "color_matrix.fs.glsl" },
    { "linear gradient",           "linear_gradient.fs.glsl" },
    { "blur",                      "blur.fs.glsl" },
    { "inset shadow",              "inset_shadow.fs.glsl" },
    { "outset shadow",             "outset_shadow.fs.glsl" },
    { "unblurred outset shadow",   "unblurred_outset_shadow.fs.glsl" },
    { "border",                    "border.fs.glsl" },
    { "cross fade",                "cross_fade.fs.glsl" },
    { "blend",                     "blend.fs.glsl" },
    { "repeat",                    "repeat.fs.glsl" },
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
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), SHADERS))
    gsk_shader_builder_add_define (builder, "GSK_DEBUG", "1");
#endif

  gsk_shader_builder_set_common_vertex_shader (builder, "blit.vs.glsl",
                                               &shader_error);

  g_assert_no_error (shader_error);

  for (i = 0; i < GL_N_PROGRAMS; i ++)
    {
      Program *prog = &self->programs[i];

      prog->index = i;
      prog->id = gsk_shader_builder_create_program (builder,
                                                    program_definitions[i].fs,
                                                    program_definitions[i].vs,
                                                    &shader_error);

      if (shader_error != NULL)
        {
          g_propagate_prefixed_error (error, shader_error,
                                      "Unable to create '%s' program (from %s and %s):\n",
                                      program_definitions[i].name,
                                      program_definitions[i].fs,
                                      program_definitions[i].vs);

          g_object_unref (builder);
          return FALSE;
        }

      INIT_COMMON_UNIFORM_LOCATION (prog, alpha);
      INIT_COMMON_UNIFORM_LOCATION (prog, source);
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
  /*INIT_PROGRAM_UNIFORM_LOCATION (blur, dir);*/

  /* inset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, spread);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, offset);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, outline);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, corner_widths);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, corner_heights);

  /* outset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, outline);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, corner_widths);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, corner_heights);

  /* unblurred outset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, spread);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, offset);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, outline);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, corner_widths);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, corner_heights);

  /* border */
  INIT_PROGRAM_UNIFORM_LOCATION (border, color);
  INIT_PROGRAM_UNIFORM_LOCATION (border, widths);
  INIT_PROGRAM_UNIFORM_LOCATION (border, outline);
  INIT_PROGRAM_UNIFORM_LOCATION (border, corner_widths);
  INIT_PROGRAM_UNIFORM_LOCATION (border, corner_heights);

  /* cross fade */
  INIT_PROGRAM_UNIFORM_LOCATION (cross_fade, progress);
  INIT_PROGRAM_UNIFORM_LOCATION (cross_fade, source2);

  /* blend */
  INIT_PROGRAM_UNIFORM_LOCATION (blend, source2);
  INIT_PROGRAM_UNIFORM_LOCATION (blend, mode);

  /* repeat */
  INIT_PROGRAM_UNIFORM_LOCATION (repeat, child_bounds);
  INIT_PROGRAM_UNIFORM_LOCATION (repeat, texture_rect);

  g_object_unref (builder);
  return TRUE;
}

static GskGLTextureAtlases *
get_texture_atlases_for_display (GdkDisplay *display)
{
  GskGLTextureAtlases *atlases;

  if (g_getenv ("GSK_NO_SHARED_CACHES"))
    return gsk_gl_texture_atlases_new ();

  atlases = (GskGLTextureAtlases*)g_object_get_data (G_OBJECT (display), "gsk-gl-texture-atlases");
  if (atlases == NULL)
    {
      atlases = gsk_gl_texture_atlases_new ();
      g_object_set_data_full (G_OBJECT (display), "gsk-gl-texture-atlases",
                              atlases,
                              (GDestroyNotify) gsk_gl_texture_atlases_unref);
    }

  return gsk_gl_texture_atlases_ref (atlases);
}

static GskGLGlyphCache *
get_glyph_cache_for_display (GdkDisplay *display,
                             GskGLTextureAtlases *atlases)
{
  GskGLGlyphCache *glyph_cache;

  if (g_getenv ("GSK_NO_SHARED_CACHES"))
    return gsk_gl_glyph_cache_new (display, atlases);

  glyph_cache = (GskGLGlyphCache*)g_object_get_data (G_OBJECT (display), "gsk-gl-glyph-cache");
  if (glyph_cache == NULL)
    {
      glyph_cache = gsk_gl_glyph_cache_new (display, atlases);
      g_object_set_data_full (G_OBJECT (display), "gsk-gl-glyph-cache",
                              glyph_cache,
                              (GDestroyNotify) gsk_gl_glyph_cache_unref);
    }

  return gsk_gl_glyph_cache_ref (glyph_cache);
}

static GskGLIconCache *
get_icon_cache_for_display (GdkDisplay *display,
                            GskGLTextureAtlases *atlases)
{
  GskGLIconCache *icon_cache;

  if (g_getenv ("GSK_NO_SHARED_CACHES"))
    return gsk_gl_icon_cache_new (display, atlases);

  icon_cache = (GskGLIconCache*)g_object_get_data (G_OBJECT (display), "gsk-gl-icon-cache");
  if (icon_cache == NULL)
    {
      icon_cache = gsk_gl_icon_cache_new (display, atlases);
      g_object_set_data_full (G_OBJECT (display), "gsk-gl-icon-cache",
                              icon_cache,
                              (GDestroyNotify) gsk_gl_icon_cache_unref);
    }

  return gsk_gl_icon_cache_ref (icon_cache);
}

static gboolean
gsk_gl_renderer_realize (GskRenderer  *renderer,
                         GdkSurface    *surface,
                         GError      **error)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);

  /* If we didn't get a GdkGLContext before realization, try creating
   * one now, for our exclusive use.
   */
  if (self->gl_context == NULL)
    {
      self->gl_context = gdk_surface_create_gl_context (surface, error);
      if (self->gl_context == NULL)
        return FALSE;
    }

  if (!gdk_gl_context_realize (self->gl_context, error))
    return FALSE;

  gdk_gl_context_make_current (self->gl_context);

  g_assert (self->gl_driver == NULL);
  self->gl_profiler = gsk_gl_profiler_new (self->gl_context);
  self->gl_driver = gsk_gl_driver_new (self->gl_context);

  GSK_RENDERER_NOTE (renderer, OPENGL, g_message ("Creating buffers and programs"));
  if (!gsk_gl_renderer_create_programs (self, error))
    return FALSE;

  self->atlases = get_texture_atlases_for_display (gdk_surface_get_display (surface));
  self->glyph_cache = get_glyph_cache_for_display (gdk_surface_get_display (surface), self->atlases);
  self->icon_cache = get_icon_cache_for_display (gdk_surface_get_display (surface), self->atlases);
  gsk_gl_shadow_cache_init (&self->shadow_cache);

  return TRUE;
}

static void
gsk_gl_renderer_unrealize (GskRenderer *renderer)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  guint i;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  /* We don't need to iterate to destroy the associated GL resources,
   * as they will be dropped when we finalize the GskGLDriver
   */
  ops_reset (&self->op_builder);

  for (i = 0; i < GL_N_PROGRAMS; i ++)
    glDeleteProgram (self->programs[i].id);

  g_clear_pointer (&self->glyph_cache, gsk_gl_glyph_cache_unref);
  g_clear_pointer (&self->icon_cache, gsk_gl_icon_cache_unref);
  g_clear_pointer (&self->atlases, gsk_gl_texture_atlases_unref);
  gsk_gl_shadow_cache_free (&self->shadow_cache, self->gl_driver);

  g_clear_object (&self->gl_profiler);
  g_clear_object (&self->gl_driver);

  if (self->gl_context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();

  g_clear_object (&self->gl_context);
}

static void
gsk_gl_renderer_clear_tree (GskGLRenderer *self)
{
  int removed_textures;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  ops_reset (&self->op_builder);
  removed_textures = gsk_gl_driver_collect_textures (self->gl_driver);

  GSK_RENDERER_NOTE (GSK_RENDERER (self), OPENGL, g_message ("Collected: %d textures", removed_textures));
}

static void
gsk_gl_renderer_clear (GskGLRenderer *self)
{
  GSK_RENDERER_NOTE (GSK_RENDERER (self), OPENGL, g_message ("Clearing viewport"));
  glClearColor (0, 0, 0, 0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void
gsk_gl_renderer_setup_render_mode (GskGLRenderer *self)
{
  if (self->render_region == NULL)
    {
      glDisable (GL_SCISSOR_TEST);
    }
  else
    {
      GdkSurface *surface = gsk_renderer_get_surface (GSK_RENDERER (self));
      cairo_rectangle_int_t extents;
      int surface_height;

      g_assert (cairo_region_num_rectangles (self->render_region) == 1);

      surface_height = gdk_surface_get_height (surface) * self->scale_factor;
      cairo_region_get_rectangle (self->render_region, 0, &extents);

      glEnable (GL_SCISSOR_TEST);
      glScissor (extents.x * self->scale_factor,
                 surface_height - (extents.height * self->scale_factor) - (extents.y * self->scale_factor),
                 extents.width * self->scale_factor,
                 extents.height * self->scale_factor);
    }
}

static void
load_vertex_data (GskQuadVertex    vertex_data[GL_N_VERTICES],
                  GskRenderNode   *node,
                  RenderOpBuilder *builder)
{
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;

  vertex_data[0].position[0] = min_x;
  vertex_data[0].position[1] = min_y;
  vertex_data[0].uv[0] = 0;
  vertex_data[0].uv[1] = 0;

  vertex_data[1].position[0] = min_x;
  vertex_data[1].position[1] = max_y;
  vertex_data[1].uv[0] = 0;
  vertex_data[1].uv[1] = 1;

  vertex_data[2].position[0] = max_x;
  vertex_data[2].position[1] = min_y;
  vertex_data[2].uv[0] = 1;
  vertex_data[2].uv[1] = 0;

  vertex_data[3].position[0] = max_x;
  vertex_data[3].position[1] = max_y;
  vertex_data[3].uv[0] = 1;
  vertex_data[3].uv[1] = 1;

  vertex_data[4].position[0] = min_x;
  vertex_data[4].position[1] = max_y;
  vertex_data[4].uv[0] = 0;
  vertex_data[4].uv[1] = 1;

  vertex_data[5].position[0] = max_x;
  vertex_data[5].position[1] = min_y;
  vertex_data[5].uv[0] = 1;
  vertex_data[5].uv[1] = 0;
}

static void
gsk_gl_renderer_add_render_ops (GskGLRenderer   *self,
                                GskRenderNode   *node,
                                RenderOpBuilder *builder)
{
  GskQuadVertex vertex_data[GL_N_VERTICES];

  /* This can still happen, even if the render nodes are created using
   * GtkSnapshot, so let's juse be safe. */
  if (node->bounds.size.width == 0.0f || node->bounds.size.height == 0.0f ||
      isnan (node->bounds.size.width) || isnan (node->bounds.size.height))
    return;

  /* Check whether the render node is entirely out of the current
   * already transformed clip region */
  {
    graphene_rect_t transformed_node_bounds;

    ops_transform_bounds_modelview (builder,
                                    &node->bounds,
                                    &transformed_node_bounds);

    if (!graphene_rect_intersection (&builder->current_clip->bounds,
                                     &transformed_node_bounds, NULL))
      return;
  }

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

    case GSK_DEBUG_NODE:
      {
        const char *message = gsk_debug_node_get_message (node);
        if (message)
          ops_push_debug_group (builder, message);
        gsk_gl_renderer_add_render_ops (self,
                                        gsk_debug_node_get_child (node),
                                        builder);
        if (message)
          ops_pop_debug_group (builder);
      }
    break;

    case GSK_COLOR_NODE:
      load_vertex_data (vertex_data, node, builder);
      render_color_node (self, node, builder, vertex_data);
    break;

    case GSK_TEXTURE_NODE:
      render_texture_node (self, node, builder);
    break;

    case GSK_TRANSFORM_NODE:
      render_transform_node (self, node, builder);
    break;

    case GSK_OPACITY_NODE:
      render_opacity_node (self, node, builder);
    break;

    case GSK_LINEAR_GRADIENT_NODE:
      load_vertex_data (vertex_data, node, builder);
      render_linear_gradient_node (self, node, builder, vertex_data);
    break;

    case GSK_CLIP_NODE:
      render_clip_node (self, node, builder);
    break;

    case GSK_ROUNDED_CLIP_NODE:
      render_rounded_clip_node (self, node, builder);
    break;

    case GSK_TEXT_NODE:
      render_text_node (self, node, builder,
                        gsk_text_node_peek_color (node), FALSE);
    break;

    case GSK_COLOR_MATRIX_NODE:
      render_color_matrix_node (self, node, builder);
    break;

    case GSK_BLUR_NODE:
      load_vertex_data (vertex_data, node, builder);
      render_blur_node (self, node, builder, vertex_data);
    break;

    case GSK_INSET_SHADOW_NODE:
      load_vertex_data (vertex_data, node, builder);
      render_inset_shadow_node (self, node, builder, vertex_data);
    break;

    case GSK_OUTSET_SHADOW_NODE:
      if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
        {
          render_outset_shadow_node (self, node, builder);
        }
      else
        {
          load_vertex_data (vertex_data, node, builder);
          render_unblurred_outset_shadow_node (self, node, builder, vertex_data);
        }
    break;

    case GSK_SHADOW_NODE:
      render_shadow_node (self, node, builder);
    break;

    case GSK_BORDER_NODE:
      render_border_node (self, node, builder);
    break;

    case GSK_CROSS_FADE_NODE:
      load_vertex_data (vertex_data, node, builder);
      render_cross_fade_node (self, node, builder, vertex_data);
    break;

    case GSK_BLEND_NODE:
      load_vertex_data (vertex_data, node, builder);
      render_blend_node (self, node, builder, vertex_data);
    break;

    case GSK_REPEAT_NODE:
      render_repeat_node (self, node, builder);
    break;

    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_CAIRO_NODE:
    default:
      {
        render_fallback_node (self, node, builder);
      }
    }
}

static void
add_offscreen_ops (GskGLRenderer         *self,
                   RenderOpBuilder       *builder,
                   const graphene_rect_t *bounds,
                   GskRenderNode         *child_node,
                   TextureRegion         *texture_region_out,
                   gboolean              *is_offscreen,
                   guint                  flags)
{
  const float scale = ops_get_scale (builder);
  const float width  = ceilf (bounds->size.width  * scale);
  const float height = ceilf (bounds->size.height * scale);
  const float dx = builder->dx;
  const float dy = builder->dy;
  int render_target;
  int prev_render_target;
  graphene_matrix_t prev_projection;
  graphene_rect_t prev_viewport;
  graphene_matrix_t item_proj;
  float prev_opacity;
  int texture_id = 0;

  /* We need the child node as a texture. If it already is one, we don't need to draw
   * it on a framebuffer of course. */
  if (gsk_render_node_get_node_type (child_node) == GSK_TEXTURE_NODE &&
      (flags & FORCE_OFFSCREEN) == 0)
    {
      GdkTexture *texture = gsk_texture_node_get_texture (child_node);

      upload_texture (self, texture, texture_region_out);
      *is_offscreen = FALSE;
      return;
    }

  /* Check if we've already cached the drawn texture. */
  {
    const int cached_id = gsk_gl_driver_get_texture_for_pointer (self->gl_driver, child_node);

    if (cached_id != 0)
      {
        init_full_texture_region (texture_region_out, cached_id);
        /* We didn't render it offscreen, but hand out an offscreen texture id */
        *is_offscreen = TRUE;
        return;
      }
  }

  gsk_gl_driver_create_render_target (self->gl_driver, width, height, &texture_id, &render_target);
  gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, texture_id,
                                      "Offscreen<%s> %d", child_node->node_class->type_name, texture_id);
  gdk_gl_context_label_object_printf  (self->gl_context, GL_FRAMEBUFFER, render_target,
                                       "Offscreen<%s> FB %d", child_node->node_class->type_name, render_target);

  graphene_matrix_init_ortho (&item_proj,
                              bounds->origin.x * scale,
                              (bounds->origin.x + bounds->size.width) * scale,
                              bounds->origin.y * scale,
                              (bounds->origin.y + bounds->size.height) * scale,
                              ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE);
  graphene_matrix_scale (&item_proj, 1, -1, 1);

  prev_render_target = ops_set_render_target (builder, render_target);
  /* Clear since we use this rendertarget for the first time */
  ops_begin (builder, OP_CLEAR);
  prev_projection = ops_set_projection (builder, &item_proj);
  ops_set_modelview (builder, gsk_transform_scale (NULL, scale, scale));
  prev_viewport = ops_set_viewport (builder,
                                    &GRAPHENE_RECT_INIT (bounds->origin.x * scale,
                                                         bounds->origin.y * scale,
                                                         width, height));
  if (flags & RESET_CLIP)
    ops_push_clip (builder,
                   &GSK_ROUNDED_RECT_INIT (bounds->origin.x * scale,
                                           bounds->origin.y * scale,
                                           width, height));

  builder->dx = 0;
  builder->dy = 0;
  if (flags & RESET_OPACITY)
    prev_opacity = ops_set_opacity (builder, 1.0);

  gsk_gl_renderer_add_render_ops (self, child_node, builder);

#ifdef G_ENABLE_DEBUG
  if (G_UNLIKELY (flags & DUMP_FRAMEBUFFER))
    {
      static int k;
      ops_dump_framebuffer (builder, g_strdup_printf ("%s_%p_%d.png",
                                                      child_node->node_class->type_name,
                                                      child_node,
                                                      k ++),
                            width, height);
    }
#endif

  if (flags & RESET_OPACITY)
    ops_set_opacity (builder, prev_opacity);

  builder->dx = dx;
  builder->dy = dy;

  if (flags & RESET_CLIP)
    ops_pop_clip (builder);

  ops_set_viewport (builder, &prev_viewport);
  ops_pop_modelview (builder);
  ops_set_projection (builder, &prev_projection);
  ops_set_render_target (builder, prev_render_target);

  *is_offscreen = TRUE;
  init_full_texture_region (texture_region_out, texture_id);

  gsk_gl_driver_set_texture_for_pointer (self->gl_driver, child_node, texture_id);
}

static void
gsk_gl_renderer_render_ops (GskGLRenderer *self)
{
  const Program *program = NULL;
  gsize vertex_data_size = self->op_builder.vertices->len * sizeof (GskQuadVertex);
  float *vertex_data = (float*)self->op_builder.vertices->data;
  OpBufferIter iter;
  OpKind kind;
  gpointer ptr;

  /*g_message ("%s: Buffer size: %ld", __FUNCTION__, vertex_data_size);*/


  GLuint buffer_id, vao_id;
  glGenVertexArrays (1, &vao_id);
  glBindVertexArray (vao_id);

  glGenBuffers (1, &buffer_id);
  glBindBuffer (GL_ARRAY_BUFFER, buffer_id);

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

  op_buffer_iter_init (&iter, ops_get_buffer (&self->op_builder));
  while ((ptr = op_buffer_iter_next (&iter, &kind)))
    {
      if (kind == OP_NONE)
        continue;

      if (program == NULL &&
          kind != OP_PUSH_DEBUG_GROUP &&
          kind != OP_POP_DEBUG_GROUP &&
          kind != OP_CHANGE_PROGRAM &&
          kind != OP_CHANGE_RENDER_TARGET &&
          kind != OP_CLEAR)
        continue;

      OP_PRINT ("Op %u: %u", iter.pos - 2, kind);

      switch (kind)
        {
        case OP_CHANGE_PROJECTION:
          apply_projection_op (program, ptr);
          break;

        case OP_CHANGE_MODELVIEW:
          apply_modelview_op (program, ptr);
          break;

        case OP_CHANGE_PROGRAM:
          {
            const OpProgram *op = ptr;
            apply_program_op (program, op);
            program = op->program;
            break;
          }

        case OP_CHANGE_RENDER_TARGET:
          apply_render_target_op (self, program, ptr);
          break;

        case OP_CLEAR:
          glClearColor (0, 0, 0, 0);
          glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
          break;

        case OP_CHANGE_VIEWPORT:
          apply_viewport_op (program, ptr);
          break;

        case OP_CHANGE_OPACITY:
          apply_opacity_op (program, ptr);
          break;

        case OP_CHANGE_COLOR_MATRIX:
          apply_color_matrix_op (program, ptr);
          break;

        case OP_CHANGE_COLOR:
          /*g_assert (program == &self->color_program || program == &self->coloring_program ||*/
                    /*program == &self->shadow_program);*/
          apply_color_op (program, ptr);
          break;

        case OP_CHANGE_BORDER_COLOR:
          apply_border_color_op (program, ptr);
          break;

        case OP_CHANGE_CLIP:
          apply_clip_op (program, ptr);
          break;

        case OP_CHANGE_SOURCE_TEXTURE:
          apply_source_texture_op (program, ptr);
          break;

        case OP_CHANGE_CROSS_FADE:
          g_assert (program == &self->cross_fade_program);
          apply_cross_fade_op (program, ptr);
          break;

        case OP_CHANGE_BLEND:
          g_assert (program == &self->blend_program);
          apply_blend_op (program, ptr);
          break;

        case OP_CHANGE_LINEAR_GRADIENT:
          apply_linear_gradient_op (program, ptr);
          break;

        case OP_CHANGE_BLUR:
          apply_blur_op (program, ptr);
          break;

        case OP_CHANGE_INSET_SHADOW:
          apply_inset_shadow_op (program, ptr);
          break;

        case OP_CHANGE_OUTSET_SHADOW:
          apply_outset_shadow_op (program, ptr);
          break;

        case OP_CHANGE_BORDER:
          apply_border_op (program, ptr);
          break;

        case OP_CHANGE_BORDER_WIDTH:
          apply_border_width_op (program, ptr);
          break;

        case OP_CHANGE_UNBLURRED_OUTSET_SHADOW:
          apply_unblurred_outset_shadow_op (program, ptr);
          break;

        case OP_CHANGE_REPEAT:
          apply_repeat_op (program, ptr);
          break;

        case OP_DRAW:
          {
            const OpDraw *op = ptr;

            OP_PRINT (" -> draw %ld, size %ld and program %d\n",
                      op->vao_offset, op->vao_size, program->index);
            glDrawArrays (GL_TRIANGLES, op->vao_offset, op->vao_size);
            break;
          }

        case OP_DUMP_FRAMEBUFFER:
          {
            const OpDumpFrameBuffer *op = ptr;

            dump_framebuffer (op->filename, op->width, op->height);
            break;
          }

        case OP_PUSH_DEBUG_GROUP:
          {
            const OpDebugGroup *op = ptr;
            gdk_gl_context_push_debug_group (self->gl_context, op->text);
            break;
          }

        case OP_POP_DEBUG_GROUP:
          gdk_gl_context_pop_debug_group (self->gl_context);
          break;

        case OP_NONE:
        case OP_LAST:
        default:
          g_warn_if_reached ();
        }

      OP_PRINT ("\n");
    }

  glDeleteVertexArrays (1, &vao_id);
  glDeleteBuffers (1, &buffer_id);
}

static void
gsk_gl_renderer_do_render (GskRenderer           *renderer,
                           GskRenderNode         *root,
                           const graphene_rect_t *viewport,
                           int                    fbo_id,
                           int                    scale_factor)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_matrix_t projection;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 gpu_time, cpu_time, start_time;
#endif
  GPtrArray *removed;

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
#endif

  if (self->gl_context == NULL)
    {
      GSK_RENDERER_NOTE (renderer, OPENGL, g_message ("No valid GL context associated to the renderer"));
      return;
    }

  g_assert (gsk_gl_driver_in_frame (self->gl_driver));

  /* Set up the modelview and projection matrices to fit our viewport */
  graphene_matrix_init_ortho (&projection,
                              viewport->origin.x,
                              viewport->origin.x + viewport->size.width,
                              viewport->origin.y,
                              viewport->origin.y + viewport->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_scale (&projection, 1, -1, 1);

  removed = g_ptr_array_new ();
  gsk_gl_texture_atlases_begin_frame (self->atlases, removed);
  gsk_gl_glyph_cache_begin_frame (self->glyph_cache, self->gl_driver, removed);
  gsk_gl_icon_cache_begin_frame (self->icon_cache, removed);
  gsk_gl_shadow_cache_begin_frame (&self->shadow_cache, self->gl_driver);
  g_ptr_array_unref (removed);

  ops_set_projection (&self->op_builder, &projection);
  ops_set_viewport (&self->op_builder, viewport);
  ops_set_modelview (&self->op_builder, gsk_transform_scale (NULL, scale_factor, scale_factor));

  /* Initial clip is self->render_region! */
  if (self->render_region != NULL)
    {
      graphene_rect_t transformed_render_region;
      cairo_rectangle_int_t render_extents;

      cairo_region_get_extents (self->render_region, &render_extents);

      ops_transform_bounds_modelview (&self->op_builder,
                                      &GRAPHENE_RECT_INIT (render_extents.x,
                                                           render_extents.y,
                                                           render_extents.width,
                                                           render_extents.height),
                                      &transformed_render_region);
      ops_push_clip (&self->op_builder,
                     &GSK_ROUNDED_RECT_INIT (transformed_render_region.origin.x,
                                             transformed_render_region.origin.y,
                                             transformed_render_region.size.width,
                                             transformed_render_region.size.height));
    }
  else
    {
      ops_push_clip (&self->op_builder,
                     &GSK_ROUNDED_RECT_INIT (viewport->origin.x,
                                             viewport->origin.y,
                                             viewport->size.width,
                                             viewport->size.height));
    }

  if (fbo_id != 0)
    ops_set_render_target (&self->op_builder, fbo_id);

  gdk_gl_context_push_debug_group (self->gl_context, "Adding render ops");
  gsk_gl_renderer_add_render_ops (self, root, &self->op_builder);
  gdk_gl_context_pop_debug_group (self->gl_context);

  /* We correctly reset the state everywhere */
  g_assert_cmpint (self->op_builder.current_render_target, ==, fbo_id);
  ops_pop_modelview (&self->op_builder);
  ops_pop_clip (&self->op_builder);
  ops_finish (&self->op_builder);

  /*g_message ("Ops: %u", self->render_ops->len);*/

  /* Now actually draw things... */
#ifdef G_ENABLE_DEBUG
  gsk_gl_profiler_begin_gpu_region (self->gl_profiler);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  /* Actually do the rendering */
  if (fbo_id != 0)
    glBindFramebuffer (GL_FRAMEBUFFER, fbo_id);

  glViewport (0, 0, ceilf (viewport->size.width), ceilf (viewport->size.height));
  gsk_gl_renderer_setup_render_mode (self);
  gsk_gl_renderer_clear (self);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  /* Pre-multiplied alpha! */
  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation (GL_FUNC_ADD);

  gdk_gl_context_push_debug_group (self->gl_context, "Rendering ops");
  gsk_gl_renderer_render_ops (self);
  gdk_gl_context_pop_debug_group (self->gl_context);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (profiler, self->profile_counters.frames);

  start_time = gsk_profiler_timer_get_start (profiler, self->profile_timers.cpu_time);
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gpu_time = gsk_gl_profiler_end_gpu_region (self->gl_profiler);
  gsk_profiler_timer_set (profiler, self->profile_timers.gpu_time, gpu_time);

  gsk_profiler_push_samples (profiler);

  if (gdk_profiler_is_running ())
    gdk_profiler_add_mark (start_time, cpu_time, "render", "");

#endif
}

static GdkTexture *
gsk_gl_renderer_render_texture (GskRenderer           *renderer,
                                GskRenderNode         *root,
                                const graphene_rect_t *viewport)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  GdkTexture *texture;
  int width, height;
  guint texture_id;
  guint fbo_id;

  g_return_val_if_fail (self->gl_context != NULL, NULL);

  gdk_gl_context_make_current (self->gl_context);
  gdk_gl_context_push_debug_group_printf (self->gl_context,
                                          "Render %s<%p> to texture", root->node_class->type_name, root);

  width = ceilf (viewport->size.width);
  height = ceilf (viewport->size.height);

  self->scale_factor = gdk_surface_get_scale_factor (gsk_renderer_get_surface (renderer));

  /* Prepare our framebuffer */
  gsk_gl_driver_begin_frame (self->gl_driver);
  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  gdk_gl_context_label_object_printf  (self->gl_context, GL_TEXTURE, texture_id,
                                       "Texture %s<%p> %d", root->node_class->type_name, root, texture_id);

  if (gdk_gl_context_get_use_es (self->gl_context))
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  glGenFramebuffers (1, &fbo_id);
  glBindFramebuffer (GL_FRAMEBUFFER, fbo_id);
  gdk_gl_context_label_object_printf  (self->gl_context, GL_FRAMEBUFFER, fbo_id,
                                       "FB %s<%p> %d", root->node_class->type_name, root, fbo_id);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  g_assert_cmphex (glCheckFramebufferStatus (GL_FRAMEBUFFER), ==, GL_FRAMEBUFFER_COMPLETE);

  /* Render the actual scene */
  gsk_gl_renderer_do_render (renderer, root, viewport, fbo_id, 1);

  texture = gdk_gl_texture_new (self->gl_context,
                                texture_id,
                                width, height,
                                NULL, NULL);

  glDeleteFramebuffers (1, &fbo_id);
  gsk_gl_driver_end_frame (self->gl_driver);

  gdk_gl_context_pop_debug_group (self->gl_context);

  gsk_gl_renderer_clear_tree (self);
  return texture;
}

static void
gsk_gl_renderer_render (GskRenderer          *renderer,
                        GskRenderNode        *root,
                        const cairo_region_t *update_area)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_rect_t viewport;
  const cairo_region_t *damage;
  GdkRectangle whole_surface;
  GdkSurface *surface;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);
  gdk_gl_context_push_debug_group_printf (self->gl_context,
                                          "Render root node %p", root);

  surface = gsk_renderer_get_surface (renderer);
  whole_surface = (GdkRectangle) {
                      0, 0,
                      gdk_surface_get_width (surface) * self->scale_factor,
                      gdk_surface_get_height (surface) * self->scale_factor
                  };

  gdk_draw_context_begin_frame (GDK_DRAW_CONTEXT (self->gl_context),
                                update_area);

  damage = gdk_draw_context_get_frame_region (GDK_DRAW_CONTEXT (self->gl_context));

  if (cairo_region_contains_rectangle (damage, &whole_surface) == CAIRO_REGION_OVERLAP_IN)
    {
      self->render_region = NULL;
    }
  else
    {
      GdkRectangle extents;

      cairo_region_get_extents (damage, &extents);

      if (gdk_rectangle_equal (&extents, &whole_surface))
        self->render_region = NULL;
      else
        self->render_region = cairo_region_create_rectangle (&extents);
    }

  self->scale_factor = gdk_surface_get_scale_factor (surface);
  gdk_gl_context_make_current (self->gl_context);

  viewport.origin.x = 0;
  viewport.origin.y = 0;
  viewport.size.width = gdk_surface_get_width (surface) * self->scale_factor;
  viewport.size.height = gdk_surface_get_height (surface) * self->scale_factor;

  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_renderer_do_render (renderer, root, &viewport, 0, self->scale_factor);
  gsk_gl_driver_end_frame (self->gl_driver);

  gsk_gl_renderer_clear_tree (self);

  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->gl_context));
  gdk_gl_context_make_current (self->gl_context);

  gdk_gl_context_pop_debug_group (self->gl_context);

  g_clear_pointer (&self->render_region, cairo_region_destroy);
}

static void
gsk_gl_renderer_class_init (GskGLRendererClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  gobject_class->dispose = gsk_gl_renderer_dispose;

  renderer_class->realize = gsk_gl_renderer_realize;
  renderer_class->unrealize = gsk_gl_renderer_unrealize;
  renderer_class->render = gsk_gl_renderer_render;
  renderer_class->render_texture = gsk_gl_renderer_render_texture;
}

static void
gsk_gl_renderer_init (GskGLRenderer *self)
{
  gsk_ensure_resources ();

  ops_init (&self->op_builder);
  self->op_builder.renderer = self;

#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));

    self->profile_counters.frames = gsk_profiler_add_counter (profiler, "frames", "Frames", FALSE);

    self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
    self->profile_timers.gpu_time = gsk_profiler_add_timer (profiler, "gpu-time", "GPU time", FALSE, TRUE);
  }
#endif
}

/**
 * gsk_gl_renderer_new:
 *
 * Creates a new #GskRenderer using OpenGL. This is the default renderer
 * used by GTK.
 *
 * Returns: a new GL renderer
 **/
GskRenderer *
gsk_gl_renderer_new (void)
{
  return g_object_new (GSK_TYPE_GL_RENDERER, NULL);
}
