#include "config.h"

#include "gskglrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
#include "gskgldriverprivate.h"
#include "gskglprofilerprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktransformprivate.h"
#include "gskglshaderbuilderprivate.h"
#include "gskglglyphcacheprivate.h"
#include "gskgliconcacheprivate.h"
#include "gskglrenderopsprivate.h"
#include "gskcairoblurprivate.h"
#include "gskglshadowcacheprivate.h"
#include "gskglnodesampleprivate.h"
#include "gsktransform.h"
#include "glutilsprivate.h"
#include "gskglshaderprivate.h"

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

#define DEBUG_OPS          0

#define SHADOW_EXTRA_SIZE  4

#if DEBUG_OPS
#define OP_PRINT(format, ...) g_print(format, ## __VA_ARGS__)
#else
#define OP_PRINT(format, ...)
#endif

#define INIT_PROGRAM_UNIFORM_LOCATION(program_name, uniform_basename) \
              G_STMT_START{\
                programs->program_name ## _program.program_name.uniform_basename ## _location = \
                              glGetUniformLocation(programs->program_name ## _program.id, "u_" #uniform_basename);\
                if (programs->program_name ## _program.program_name.uniform_basename ## _location == -1) \
                  { \
                    g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_LINK_FAILED, \
                                 "Failed to find variable \"u_%s\" in shader program \"%s\"", #uniform_basename, #program_name); \
                    g_clear_pointer (&programs, gsk_gl_renderer_programs_unref); \
                    goto out; \
                  } \
              }G_STMT_END

#define INIT_COMMON_UNIFORM_LOCATION(program_ptr, uniform_basename) \
              G_STMT_START{\
                program_ptr->uniform_basename ## _location =  \
                              glGetUniformLocation(program_ptr->id, "u_" #uniform_basename);\
              }G_STMT_END

static Program *gsk_gl_renderer_lookup_custom_program (GskGLRenderer  *self,
                                                       GskGLShader *shader);
static Program *gsk_gl_renderer_create_custom_program (GskGLRenderer  *self,
                                                       GskGLShader *shader);

typedef enum
{
  FORCE_OFFSCREEN  = 1 << 0,
  RESET_CLIP       = 1 << 1,
  DUMP_FRAMEBUFFER = 1 << 3,
  NO_CACHE_PLZ     = 1 << 5,
  LINEAR_FILTER    = 1 << 6,
} OffscreenFlags;

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

      case GSK_COLOR_NODE:
        g_print ("%*s Color %s\n", level * INDENT, " ", gdk_rgba_to_string (gsk_color_node_get_color (root)));
        break;

      case GSK_SHADOW_NODE:
        g_print ("%*s Shadow\n", level * INDENT, " ");
        print_render_node_tree (gsk_shadow_node_get_child (root), level + 1);
        break;

      case GSK_GL_SHADER_NODE:
        g_print ("%*s GL Shader\n", level * INDENT, " ");
        for (i = 0; i < gsk_gl_shader_node_get_n_children (root); i++)
          print_render_node_tree (gsk_gl_shader_node_get_child (root, i), level + 1);
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
        g_print ("%*s %s\n", level * INDENT, " ", g_type_name_from_instance ((GTypeInstance *) root));
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

static inline bool G_GNUC_PURE
node_is_invisible (const GskRenderNode *node)
{
  return node->bounds.size.width == 0.0f ||
         node->bounds.size.height == 0.0f ||
         isnan (node->bounds.size.width) ||
         isnan (node->bounds.size.height);
}

static inline bool G_GNUC_PURE
graphene_rect_intersects (const graphene_rect_t *r1,
                          const graphene_rect_t *r2)
{
  /* Assume both rects are already normalized, as they usually are */
  if (r1->origin.x > (r2->origin.x + r2->size.width) ||
      (r1->origin.x + r1->size.width) < r2->origin.x)
    return false;

  if (r1->origin.y > (r2->origin.y + r2->size.height) ||
      (r1->origin.y + r1->size.height) < r2->origin.y)
    return false;

  return true;
}

static inline bool G_GNUC_PURE
_graphene_rect_contains_rect (const graphene_rect_t *r1,
                              const graphene_rect_t *r2)
{
  if (r2->origin.x >= r1->origin.x &&
      (r2->origin.x + r2->size.width) <= (r1->origin.x + r1->size.width) &&
      r2->origin.y >= r1->origin.y &&
      (r2->origin.y + r2->size.height) <= (r1->origin.y + r1->size.height))
    return true;

  return false;
}

static inline bool G_GNUC_PURE
equal_texture_nodes (GskRenderNode *node1,
                     GskRenderNode *node2)
{
  if (gsk_render_node_get_node_type (node1) != GSK_TEXTURE_NODE ||
      gsk_render_node_get_node_type (node2) != GSK_TEXTURE_NODE)
    return false;

  if (gsk_texture_node_get_texture (node1) !=
      gsk_texture_node_get_texture (node2))
    return false;

  return graphene_rect_equal (&node1->bounds, &node2->bounds);
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

static inline void
init_projection_matrix (graphene_matrix_t     *out_proj,
                        const graphene_rect_t *viewport)
{
  graphene_matrix_init_ortho (out_proj,
                              viewport->origin.x,
                              viewport->origin.x + viewport->size.width,
                              viewport->origin.y,
                              viewport->origin.y + viewport->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_scale (out_proj, 1, -1, 1);
}

static inline gboolean G_GNUC_PURE
color_matrix_modifies_alpha (GskRenderNode *node)
{
  const graphene_matrix_t *matrix = gsk_color_matrix_node_get_color_matrix (node);
  const graphene_vec4_t *offset = gsk_color_matrix_node_get_color_offset (node);
  graphene_vec4_t row3;

  if (graphene_vec4_get_w (offset) != 0.0f)
    return TRUE;

  graphene_matrix_get_row (matrix, 3, &row3);

  return !graphene_vec4_equal (graphene_vec4_w_axis (), &row3);
}

static inline void
gsk_rounded_rect_shrink_to_minimum (GskRoundedRect *self)
{
  self->bounds.size.width  = MAX (self->corner[0].width + self->corner[1].width,
                                  self->corner[3].width + self->corner[2].width);
  self->bounds.size.height = MAX (self->corner[0].height + self->corner[3].height,
                                  self->corner[1].height + self->corner[2].height);
}

static inline gboolean G_GNUC_PURE
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
      case GSK_CROSS_FADE_NODE:
      case GSK_LINEAR_GRADIENT_NODE:
      case GSK_DEBUG_NODE:
      case GSK_TEXT_NODE:
        return TRUE;

      case GSK_TRANSFORM_NODE:
        return node_supports_transform (gsk_transform_node_get_child (node));

      default:
        return FALSE;
    }
  return FALSE;
}

static inline void
load_vertex_data_with_region (GskQuadVertex          vertex_data[GL_N_VERTICES],
                              const graphene_rect_t *bounds,
                              RenderOpBuilder       *builder,
                              const TextureRegion   *r,
                              gboolean               flip_y)
{
  const float min_x = builder->dx + bounds->origin.x;
  const float min_y = builder->dy + bounds->origin.y;
  const float max_x = min_x + bounds->size.width;
  const float max_y = min_y + bounds->size.height;
  const float y1 = flip_y ? r->y2 : r->y;
  const float y2 = flip_y ? r->y  : r->y2;

  vertex_data[0].position[0] = min_x;
  vertex_data[0].position[1] = min_y;
  vertex_data[0].uv[0] = r->x;
  vertex_data[0].uv[1] = y1;

  vertex_data[1].position[0] = min_x;
  vertex_data[1].position[1] = max_y;
  vertex_data[1].uv[0] = r->x;
  vertex_data[1].uv[1] = y2;

  vertex_data[2].position[0] = max_x;
  vertex_data[2].position[1] = min_y;
  vertex_data[2].uv[0] = r->x2;
  vertex_data[2].uv[1] = y1;

  vertex_data[3].position[0] = max_x;
  vertex_data[3].position[1] = max_y;
  vertex_data[3].uv[0] = r->x2;
  vertex_data[3].uv[1] = y2;

  vertex_data[4].position[0] = min_x;
  vertex_data[4].position[1] = max_y;
  vertex_data[4].uv[0] = r->x;
  vertex_data[4].uv[1] = y2;

  vertex_data[5].position[0] = max_x;
  vertex_data[5].position[1] = min_y;
  vertex_data[5].uv[0] = r->x2;
  vertex_data[5].uv[1] = y1;
}

static inline void
load_float_vertex_data (GskQuadVertex    vertex_data[GL_N_VERTICES],
                        RenderOpBuilder *builder,
                        float            x,
                        float            y,
                        float            width,
                        float            height)
{
  const float min_x = builder->dx + x;
  const float min_y = builder->dy + y;
  const float max_x = min_x + width;
  const float max_y = min_y + height;

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
load_vertex_data (GskQuadVertex          vertex_data[GL_N_VERTICES],
                  const graphene_rect_t *bounds,
                  RenderOpBuilder       *builder)
{
  load_float_vertex_data (vertex_data, builder,
                          bounds->origin.x, bounds->origin.y,
                          bounds->size.width, bounds->size.height);
}

static void
fill_vertex_data (GskQuadVertex vertex_data[GL_N_VERTICES],
                  const float   min_x,
                  const float   min_y,
                  const float   max_x,
                  const float   max_y)
{
  vertex_data[0].position[0] = min_x;
  vertex_data[0].position[1] = min_y;
  vertex_data[0].uv[0] = 0;
  vertex_data[0].uv[1] = 1;

  vertex_data[1].position[0] = min_x;
  vertex_data[1].position[1] = max_y;
  vertex_data[1].uv[0] = 0;
  vertex_data[1].uv[1] = 0;

  vertex_data[2].position[0] = max_x;
  vertex_data[2].position[1] = min_y;
  vertex_data[2].uv[0] = 1;
  vertex_data[2].uv[1] = 1;

  vertex_data[3].position[0] = max_x;
  vertex_data[3].position[1] = max_y;
  vertex_data[3].uv[0] = 1;
  vertex_data[3].uv[1] = 0;

  vertex_data[4].position[0] = min_x;
  vertex_data[4].position[1] = max_y;
  vertex_data[4].uv[0] = 0;
  vertex_data[4].uv[1] = 0;

  vertex_data[5].position[0] = max_x;
  vertex_data[5].position[1] = min_y;
  vertex_data[5].uv[0] = 1;
  vertex_data[5].uv[1] = 1;
}

static void
load_offscreen_vertex_data (GskQuadVertex    vertex_data[GL_N_VERTICES],
                            GskRenderNode   *node,
                            RenderOpBuilder *builder)
{
  const float min_x = builder->dx + node->bounds.origin.x;
  const float min_y = builder->dy + node->bounds.origin.y;
  const float max_x = min_x + node->bounds.size.width;
  const float max_y = min_y + node->bounds.size.height;

  fill_vertex_data (vertex_data,
                    min_x, min_y,
                    max_x, max_y);
}


static void gsk_gl_renderer_setup_render_mode (GskGLRenderer   *self);
static gboolean add_offscreen_ops             (GskGLRenderer   *self,
                                               RenderOpBuilder       *builder,
                                               const graphene_rect_t *bounds,
                                               GskRenderNode         *child_node,
                                               TextureRegion         *region_out,
                                               gboolean              *is_offscreen,
                                               guint                  flags) G_GNUC_WARN_UNUSED_RESULT;
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

  GskGLRendererPrograms *programs;

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

static void
init_shader_builder (GskGLRenderer       *self,
                     GskGLShaderBuilder  *shader_builder)
{
#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), SHADERS))
    shader_builder->debugging = TRUE;
#endif

  if (gdk_gl_context_get_use_es (self->gl_context))
    {
      gsk_gl_shader_builder_set_glsl_version (shader_builder, SHADER_VERSION_GLES);
      shader_builder->gles = TRUE;
    }
  else if (gdk_gl_context_is_legacy (self->gl_context))
    {
      int maj, min;

      gdk_gl_context_get_version (self->gl_context, &maj, &min);

      if (maj == 3)
        gsk_gl_shader_builder_set_glsl_version (shader_builder, SHADER_VERSION_GL3_LEGACY);
      else
        gsk_gl_shader_builder_set_glsl_version (shader_builder, SHADER_VERSION_GL2_LEGACY);

      shader_builder->legacy = TRUE;
        }
  else
    {
      gsk_gl_shader_builder_set_glsl_version (shader_builder, SHADER_VERSION_GL3);
      shader_builder->gl3 = TRUE;
    }
}

static GdkRGBA BLACK = {0, 0, 0, 1};
static void G_GNUC_UNUSED
add_rect_outline_ops (GskGLRenderer         *self,
                      RenderOpBuilder       *builder,
                      const graphene_rect_t *rect)
{
  ops_set_program (builder, &self->programs->color_program);
  ops_set_color (builder, &BLACK);

  load_vertex_data (ops_draw (builder, NULL),
                    &GRAPHENE_RECT_INIT (rect->origin.x, rect->origin.y, 1, rect->size.height),
                    builder);
  load_vertex_data (ops_draw (builder, NULL),
                    &GRAPHENE_RECT_INIT (rect->origin.x, rect->origin.y, rect->size.width, 1),
                    builder);
  load_vertex_data (ops_draw (builder, NULL),
                    &GRAPHENE_RECT_INIT (rect->origin.x + rect->size.width - 1,rect->origin.y,
                                         1, rect->size.height),
                    builder);
  load_vertex_data (ops_draw (builder, NULL),
                    &GRAPHENE_RECT_INIT (rect->origin.x, rect->origin.y + rect->size.height - 1,
                                         rect->size.width, 1),
                    builder);
}

static inline GskRoundedRect
transform_rect (GskGLRenderer        *self,
                RenderOpBuilder      *builder,
                const GskRoundedRect *rect)
{
  GskRoundedRect r;

  r.bounds.origin.x = builder->dx + rect->bounds.origin.x;
  r.bounds.origin.y = builder->dy + rect->bounds.origin.y;
  r.bounds.size = rect->bounds.size;

  r.corner[0] = rect->corner[0];
  r.corner[1] = rect->corner[1];
  r.corner[2] = rect->corner[2];
  r.corner[3] = rect->corner[3];

  return r;
}

static inline void
render_fallback_node (GskGLRenderer   *self,
                      GskRenderNode   *node,
                      RenderOpBuilder *builder)
{
  const float scale_x = builder->scale_x;
  const float scale_y = builder->scale_y;
  const int surface_width = ceilf (node->bounds.size.width * scale_x);
  const int surface_height = ceilf (node->bounds.size.height * scale_y);
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_surface_t *rendered_surface;
  cairo_t *cr;
  int cached_id;
  int texture_id;
  GskTextureKey key;

  if (surface_width <= 0 ||
      surface_height <= 0)
    return;

  key.pointer = node;
  key.pointer_is_child = FALSE;
  key.scale_x = scale_x;
  key.scale_y = scale_y;
  key.filter = GL_NEAREST;

  cached_id = gsk_gl_driver_get_texture_for_key (self->gl_driver, &key);

  if (cached_id != 0)
    {
      ops_set_program (builder, &self->programs->blit_program);
      ops_set_texture (builder, cached_id);
      load_offscreen_vertex_data (ops_draw (builder, NULL), node, builder);
      return;
    }


  /* We first draw the recording surface on an image surface,
   * just because the scaleY(-1) later otherwise screws up the
   * rendering... */
  {
    rendered_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                   surface_width,
                                                   surface_height);

    cairo_surface_set_device_scale (rendered_surface, scale_x, scale_y);
    cr = cairo_create (rendered_surface);

    cairo_save (cr);
    cairo_translate (cr, - floorf (node->bounds.origin.x), - floorf (node->bounds.origin.y));
    gsk_render_node_draw (node, cr);
    cairo_restore (cr);
    cairo_destroy (cr);
  }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        surface_width,
                                        surface_height);
  cairo_surface_set_device_scale (surface, scale_x, scale_y);
  cr = cairo_create (surface);

  /* We draw upside down here, so it matches what GL does. */
  cairo_save (cr);
  cairo_scale (cr, 1, -1);
  cairo_translate (cr, 0, - surface_height / scale_y);
  cairo_set_source_surface (cr, rendered_surface, 0, 0);
  cairo_rectangle (cr, 0, 0, surface_width / scale_x, surface_height / scale_y);
  cairo_fill (cr);
  cairo_restore (cr);

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), FALLBACK))
    {
      cairo_move_to (cr, 0, 0);
      cairo_rectangle (cr, 0, 0, node->bounds.size.width, node->bounds.size.height);
      if (gsk_render_node_get_node_type (node) == GSK_CAIRO_NODE)
        cairo_set_source_rgba (cr, 0.3, 0, 1, 0.25);
      else
        cairo_set_source_rgba (cr, 1, 0, 0, 0.25);
      cairo_fill_preserve (cr);
      if (gsk_render_node_get_node_type (node) == GSK_CAIRO_NODE)
        cairo_set_source_rgba (cr, 0.3, 0, 1, 1);
      else
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

  texture = gdk_texture_new_for_surface (surface);
  gsk_gl_driver_init_texture (self->gl_driver,
                              texture_id,
                              texture,
                              GL_NEAREST, GL_NEAREST);

  if (gdk_gl_context_has_debug (self->gl_context))
    gdk_gl_context_label_object_printf  (self->gl_context, GL_TEXTURE, texture_id,
                                         "Fallback %s %d",
                                         g_type_name_from_instance ((GTypeInstance *) node),
                                         texture_id);

  g_object_unref (texture);
  cairo_surface_destroy (surface);
  cairo_surface_destroy (rendered_surface);

  gsk_gl_driver_set_texture_for_key (self->gl_driver, &key, texture_id);

  ops_set_program (builder, &self->programs->blit_program);
  ops_set_texture (builder, texture_id);
  load_offscreen_vertex_data (ops_draw (builder, NULL), node, builder);
}

static inline void
render_text_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder,
                  const GdkRGBA   *color,
                  gboolean         force_color)
{
  const PangoFont *font = gsk_text_node_get_font (node);
  const PangoGlyphInfo *glyphs = gsk_text_node_get_glyphs (node, NULL);
  const float text_scale = MAX (builder->scale_x, builder->scale_y); /* TODO: Fix for uneven scales? */
  const graphene_point_t *offset = gsk_text_node_get_offset (node);
  const guint num_glyphs = gsk_text_node_get_num_glyphs (node);
  const float x = offset->x + builder->dx;
  const float y = offset->y + builder->dy;
  int i;
  int x_position = 0;
  GlyphCacheKey lookup;

  /* If the font has color glyphs, we don't need to recolor anything */
  if (!force_color && gsk_text_node_has_color_glyphs (node))
    {
      ops_set_program (builder, &self->programs->blit_program);
    }
  else
    {
      ops_set_program (builder, &self->programs->coloring_program);
      ops_set_color (builder, color);
    }

  memset (&lookup, 0, sizeof (CacheKeyData));
  lookup.data.font = (PangoFont *)font;
  lookup.data.scale = (guint) (text_scale * 1024);

  /* We use one quad per character */
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
  const GdkRGBA *colors = gsk_border_node_get_colors (node);
  const GskRoundedRect *rounded_outline = gsk_border_node_get_outline (node);
  const float *widths = gsk_border_node_get_widths (node);
  int i;
  struct {
    float w;
    float h;
  } sizes[4];

  if (gsk_border_node_get_uniform (node))
    {
      ops_set_program (builder, &self->programs->inset_shadow_program);
      ops_set_inset_shadow (builder, transform_rect (self, builder, rounded_outline),
                            widths[0], &colors[0], 0, 0);

      load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
      return;
    }

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

  {
    const float min_x = builder->dx + node->bounds.origin.x;
    const float min_y = builder->dy + node->bounds.origin.y;
    const float max_x = min_x + node->bounds.size.width;
    const float max_y = min_y + node->bounds.size.height;
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
    GskRoundedRect outline;

    /* We sort them by color */
    sort_border_sides (colors, indices);

    /* Prepare outline */
    outline = transform_rect (self, builder, rounded_outline);

    ops_set_program (builder, &self->programs->border_program);
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
render_color_node (GskGLRenderer   *self,
                   GskRenderNode   *node,
                   RenderOpBuilder *builder)
{
  ops_set_program (builder, &self->programs->color_program);
  ops_set_color (builder, gsk_color_node_get_color (node));
  load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
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

  if (texture->width > max_texture_size || texture->height > max_texture_size)
    {
      const float min_x = builder->dx + node->bounds.origin.x;
      const float min_y = builder->dy + node->bounds.origin.y;
      const float max_x = min_x + node->bounds.size.width;
      const float max_y = min_y + node->bounds.size.height;
      const float scale_x = (max_x - min_x) / texture->width;
      const float scale_y = (max_y - min_y) / texture->height;
      TextureSlice *slices;
      guint n_slices;
      guint i;

      gsk_gl_driver_slice_texture (self->gl_driver, texture, &slices, &n_slices);

      ops_set_program (builder, &self->programs->blit_program);
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

      ops_set_program (builder, &self->programs->blit_program);
      ops_set_texture (builder, r.texture_id);

      load_vertex_data_with_region (ops_draw (builder, NULL),
                                    &node->bounds, builder,
                                    &r,
                                    FALSE);
    }
}

static Program *
compile_glshader (GskGLRenderer  *self,
                  GskGLShader    *shader,
                  GError        **error)
{
  GskGLShaderBuilder shader_builder;
  const char *shader_source;
  gsize shader_source_len;
  int n_uniforms;
  const GskGLUniform *uniforms;
  GBytes *bytes;
  int n_required_textures = gsk_gl_shader_get_n_textures (shader);
  int program_id;
  Program *program;

  bytes = gsk_gl_shader_get_source (shader);
  shader_source = g_bytes_get_data (bytes, &shader_source_len);
  uniforms = gsk_gl_shader_get_uniforms (shader, &n_uniforms);

  if (n_uniforms > G_N_ELEMENTS (program->glshader.args_locations))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                   "GLShaderNode supports max %d custom uniforms", (int)G_N_ELEMENTS (program->glshader.args_locations));
      return NULL;
    }

  if (n_required_textures > G_N_ELEMENTS (program->glshader.texture_locations))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                   "GLShaderNode supports max %d texture sources", (int)(G_N_ELEMENTS (program->glshader.texture_locations)));
      return NULL;
    }

  gsk_gl_shader_builder_init (&shader_builder,
                              "/org/gtk/libgsk/gl/preamble.glsl",
                              "/org/gtk/libgsk/gl/preamble.vs.glsl",
                              "/org/gtk/libgsk/gl/preamble.fs.glsl");

  init_shader_builder (self, &shader_builder);
  program_id = gsk_gl_shader_builder_create_program (&shader_builder,
                                                     "/org/gtk/libgsk/gl/custom.glsl",
                                                     shader_source, shader_source_len,
                                                     error);
  gsk_gl_shader_builder_finish (&shader_builder);

  if (program_id  < 0)
    return NULL;

  program = gsk_gl_renderer_create_custom_program (self, shader);

  program->id = program_id;
  INIT_COMMON_UNIFORM_LOCATION (program, alpha);
  INIT_COMMON_UNIFORM_LOCATION (program, clip_rect);
  INIT_COMMON_UNIFORM_LOCATION (program, viewport);
  INIT_COMMON_UNIFORM_LOCATION (program, projection);
  INIT_COMMON_UNIFORM_LOCATION (program, modelview);
  program->glshader.size_location = glGetUniformLocation(program->id, "u_size");
  program->glshader.texture_locations[0] = glGetUniformLocation(program->id, "u_texture1");
  program->glshader.texture_locations[1] = glGetUniformLocation(program->id, "u_texture2");
  program->glshader.texture_locations[2] = glGetUniformLocation(program->id, "u_texture3");
  program->glshader.texture_locations[3] = glGetUniformLocation(program->id, "u_texture4");

  /* We use u_texture1 for the texture 0 in the glshaders, so alias it here so we can use the regular setters */
  program->source_location = program->glshader.texture_locations[0];

  for (int i = 0; i < G_N_ELEMENTS (program->glshader.args_locations); i++)
    {
      if (i < n_uniforms)
        {
          program->glshader.args_locations[i] = glGetUniformLocation(program->id, uniforms[i].name);
          /* This isn't necessary a hard error, you might declare uniforms that are not actually
             always used, for instance if you have an "API" in uniforms for multiple shaders. */
          if (program->glshader.args_locations[i] == -1)
            g_debug ("Declared uniform `%s` not found in GskGLShader", uniforms[i].name);
        }
      else
        program->glshader.args_locations[i] = -1;
    }

  return program;
}

gboolean
gsk_gl_renderer_try_compile_gl_shader (GskGLRenderer  *self,
                                       GskGLShader    *shader,
                                       GError        **error)
{
  Program *program;

  gdk_gl_context_make_current (self->gl_context);

  /* Maybe we tried to compile it already? */
  program = gsk_gl_renderer_lookup_custom_program (self, shader);
  if (program != NULL)
    {
      if (program->id > 0)
        return TRUE;
      else
        {
          g_propagate_error (error, g_error_copy (program->glshader.compile_error));
          return FALSE;
        }
    }

  program = compile_glshader (self, shader, error);
  return program != NULL;
}

static inline void
render_gl_shader_node (GskGLRenderer       *self,
                       GskRenderNode       *node,
                       RenderOpBuilder     *builder)
{
  GskGLShader *shader = gsk_gl_shader_node_get_shader (node);
  Program *program = gsk_gl_renderer_lookup_custom_program (self, shader);
  int n_children = gsk_gl_shader_node_get_n_children (node);

  if (program == NULL)
    {
      GError *error = NULL;

      program = compile_glshader (self, shader, &error);
      if (program == NULL)
        {
          /* We create the program anyway (in a failed state), so that any compiler warnings or other are only reported once */
          program = gsk_gl_renderer_create_custom_program (self, shader);
          program->id = -1;
          program->glshader.compile_error = error;

          g_warning ("Failed to compile gl shader: %s", error->message);
        }
    }

  if (program->id >= 0 && n_children <= G_N_ELEMENTS (program->glshader.texture_locations))
    {
      GBytes *args;
      TextureRegion regions[4];
      gboolean is_offscreen[4];
      for (guint i = 0; i < n_children; i++)
        {
          GskRenderNode *child = gsk_gl_shader_node_get_child (node, i);
          if (!add_offscreen_ops (self, builder,
                                  &node->bounds,
                                  child,
                                  &regions[i], &is_offscreen[i],
                                  FORCE_OFFSCREEN | RESET_CLIP))
            return;
        }

      args = gsk_gl_shader_node_get_args (node);
      ops_set_program (builder, program);

      ops_set_gl_shader_args (builder, shader, node->bounds.size.width, node->bounds.size.height, g_bytes_get_data (args, NULL));
      for (guint i = 0; i < n_children; i++)
        {
          if (i == 0)
            ops_set_texture (builder, regions[i].texture_id);
          else
            ops_set_extra_texture (builder, regions[i].texture_id, i);
        }

      load_offscreen_vertex_data (ops_draw (builder, NULL), node, builder);
    }
  else
    {
      static GdkRGBA pink = { 255 / 255., 105 / 255., 180 / 255., 1.0 };
      ops_set_program (builder, &self->programs->color_program);
      ops_set_color (builder, &pink);
      load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
    }
}

/* Returns TRUE if applying @transform to @bounds
 * yields an axis-aligned rectangle
 */
static gboolean
result_is_axis_aligned (GskTransform          *transform,
                        const graphene_rect_t *bounds)
{
  graphene_matrix_t m;
  graphene_quad_t q;
  graphene_rect_t b;
  graphene_point_t b1, b2;
  const graphene_point_t *p;
  int i;

  gsk_transform_to_matrix (transform, &m);
  gsk_matrix_transform_rect (&m, bounds, &q);
  graphene_quad_bounds (&q, &b);
  graphene_rect_get_top_left (&b, &b1);
  graphene_rect_get_bottom_right (&b, &b2);

  for (i = 0; i < 4; i++)
    {
      p = graphene_quad_get_point (&q, i);
      if (fabs (p->x - b1.x) > FLT_EPSILON && fabs (p->x - b2.x) > FLT_EPSILON)
        return FALSE;
      if (fabs (p->y - b1.y) > FLT_EPSILON && fabs (p->y - b2.y) > FLT_EPSILON)
        return FALSE;
    }

  return TRUE;
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

    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
      {
        TextureRegion region;
        gboolean is_offscreen;

        if (node_supports_transform (child))
          {
            ops_push_modelview (builder, node_transform);
            gsk_gl_renderer_add_render_ops (self, child, builder);
            ops_pop_modelview (builder);
          }
        else
          {
            int filter_flag = 0;

            if (!result_is_axis_aligned (node_transform, &child->bounds))
              filter_flag = LINEAR_FILTER;

            if (add_offscreen_ops (self, builder,
                                   &child->bounds,
                                   child,
                                   &region, &is_offscreen,
                                   RESET_CLIP | filter_flag))
              {
                /* For non-trivial transforms, we draw everything on a texture and then
                 * draw the texture transformed. */
                /* TODO: We should compute a modelview containing only the "non-trivial"
                 *       part (e.g. the rotation) and use that. We want to keep the scale
                 *       for the texture.
                 */
                ops_push_modelview (builder, node_transform);
                ops_set_texture (builder, region.texture_id);
                ops_set_program (builder, &self->programs->blit_program);

                load_vertex_data_with_region (ops_draw (builder, NULL),
                                              &child->bounds, builder,
                                              &region,
                                              is_offscreen);
                ops_pop_modelview (builder);
              }
          }
      }
      break;

    default:
      g_assert_not_reached ();
      break;
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
      gboolean is_offscreen;
      TextureRegion region;

      /* The semantics of an opacity node mandate that when, e.g., two color nodes overlap,
       * there may not be any blending between them */
      if (!add_offscreen_ops (self, builder, &child->bounds,
                              child,
                              &region, &is_offscreen,
                              FORCE_OFFSCREEN | RESET_CLIP))
        return;

      prev_opacity = ops_set_opacity (builder,
                                      builder->current_opacity * opacity);

      if (builder->current_opacity >= ((float) 0x00ff / (float) 0xffff))
        {
          ops_set_program (builder, &self->programs->blit_program);
          ops_set_texture (builder, region.texture_id);

          load_vertex_data_with_region (ops_draw (builder, NULL),
                                        &node->bounds, builder,
                                        &region,
                                        is_offscreen);
        }
    }
  else
    {
      prev_opacity = ops_set_opacity (builder,
                                      builder->current_opacity * opacity);

      if (builder->current_opacity >= ((float) 0x00ff / (float) 0xffff))
        gsk_gl_renderer_add_render_ops (self, child, builder);
    }

  ops_set_opacity (builder, prev_opacity);
}

static inline void
render_linear_gradient_node (GskGLRenderer   *self,
                             GskRenderNode   *node,
                             RenderOpBuilder *builder)
{
  const int n_color_stops = gsk_linear_gradient_node_get_n_color_stops (node);

  if (n_color_stops < GL_MAX_GRADIENT_STOPS)
    {
      const GskColorStop *stops = gsk_linear_gradient_node_get_color_stops (node, NULL);
      const graphene_point_t *start = gsk_linear_gradient_node_get_start (node);
      const graphene_point_t *end = gsk_linear_gradient_node_get_end (node);

      ops_set_program (builder, &self->programs->linear_gradient_program);
      ops_set_linear_gradient (builder,
                               n_color_stops,
                               stops,
                               gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE,
                               builder->dx + start->x,
                               builder->dy + start->y,
                               builder->dx + end->x,
                               builder->dy + end->y);

      load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
    }
  else
    {
      render_fallback_node (self, node, builder);
    }
}

static inline void
render_radial_gradient_node (GskGLRenderer   *self,
                             GskRenderNode   *node,
                             RenderOpBuilder *builder)
{
  const int n_color_stops = gsk_radial_gradient_node_get_n_color_stops (node);

  if (n_color_stops < GL_MAX_GRADIENT_STOPS)
    {
      const GskColorStop *stops = gsk_radial_gradient_node_get_color_stops (node, NULL);
      const graphene_point_t *center = gsk_radial_gradient_node_get_center (node);
      const float start = gsk_radial_gradient_node_get_start (node);
      const float end = gsk_radial_gradient_node_get_end (node);
      const float hradius = gsk_radial_gradient_node_get_hradius (node);
      const float vradius = gsk_radial_gradient_node_get_vradius (node);

      ops_set_program (builder, &self->programs->radial_gradient_program);
      ops_set_radial_gradient (builder,
                               n_color_stops,
                               stops,
                               gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE,
                               builder->dx + center->x,
                               builder->dy + center->y,
                               start, end,
                               hradius * builder->scale_x,
                               vradius * builder->scale_y);

      load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
    }
  else
    {
      render_fallback_node (self, node, builder);
    }
}

static inline void
render_conic_gradient_node (GskGLRenderer   *self,
                             GskRenderNode   *node,
                             RenderOpBuilder *builder)
{
  const int n_color_stops = gsk_conic_gradient_node_get_n_color_stops (node);

  if (n_color_stops < GL_MAX_GRADIENT_STOPS)
    {
      const GskColorStop *stops = gsk_conic_gradient_node_get_color_stops (node, NULL);
      const graphene_point_t *center = gsk_conic_gradient_node_get_center (node);
      const float angle = gsk_conic_gradient_node_get_angle (node);

      ops_set_program (builder, &self->programs->conic_gradient_program);
      ops_set_conic_gradient (builder,
                              n_color_stops,
                              stops,
                              builder->dx + center->x,
                              builder->dy + center->y,
                              angle);

      load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
    }
  else
    {
      render_fallback_node (self, node, builder);
    }
}

static inline gboolean
rounded_inner_rect_contains_rect (const GskRoundedRect  *rounded,
                                  const graphene_rect_t *rect)
{
  const graphene_rect_t *rounded_bounds = &rounded->bounds;
  graphene_rect_t inner;
  float offset_x, offset_y;

  /* TODO: This is pretty conservative and we could to further, more
   *       fine-grained checks to avoid offscreen drawing. */

  offset_x = MAX (rounded->corner[GSK_CORNER_TOP_LEFT].width,
                  rounded->corner[GSK_CORNER_BOTTOM_LEFT].width);
  offset_y = MAX (rounded->corner[GSK_CORNER_TOP_LEFT].height,
                  rounded->corner[GSK_CORNER_TOP_RIGHT].height);


  inner.origin.x = rounded_bounds->origin.x + offset_x;
  inner.origin.y = rounded_bounds->origin.y + offset_y;
  inner.size.width = rounded_bounds->size.width - offset_x -
                     MAX (rounded->corner[GSK_CORNER_TOP_RIGHT].width,
                          rounded->corner[GSK_CORNER_BOTTOM_RIGHT].width);
  inner.size.height = rounded_bounds->size.height - offset_y -
                      MAX (rounded->corner[GSK_CORNER_BOTTOM_LEFT].height,
                           rounded->corner[GSK_CORNER_BOTTOM_RIGHT].height);

  return _graphene_rect_contains_rect (&inner, rect);
}

/* Current clip is NOT rounded but new one is definitely! */
static inline bool
intersect_rounded_rectilinear (const graphene_rect_t *non_rounded,
                               const GskRoundedRect  *rounded,
                               GskRoundedRect        *result)
{
  bool corners[4];

  /* Intersects with top left corner? */
  corners[0] = rounded_rect_has_corner (rounded, 0) &&
               graphene_rect_intersects (non_rounded,
                                         &rounded_rect_corner (rounded, 0));
  /* top right? */
  corners[1] = rounded_rect_has_corner (rounded, 1) &&
               graphene_rect_intersects (non_rounded,
                                         &rounded_rect_corner (rounded, 1));
  /* bottom right? */
  corners[2] = rounded_rect_has_corner (rounded, 2) &&
               graphene_rect_intersects (non_rounded,
                                         &rounded_rect_corner (rounded, 2));
  /* bottom left */
  corners[3] = rounded_rect_has_corner (rounded, 3) &&
               graphene_rect_intersects (non_rounded,
                                         &rounded_rect_corner (rounded, 3));

  if (corners[0] && !_graphene_rect_contains_rect (non_rounded, &rounded_rect_corner (rounded, 0)))
    return false;
  if (corners[1] && !_graphene_rect_contains_rect (non_rounded, &rounded_rect_corner (rounded, 1)))
    return false;
  if (corners[2] && !_graphene_rect_contains_rect (non_rounded, &rounded_rect_corner (rounded, 2)))
    return false;
  if (corners[3] && !_graphene_rect_contains_rect (non_rounded, &rounded_rect_corner (rounded, 3)))
    return false;

  /* We do intersect with at least one of the corners, but in such a way that the
   * intersection between the two clips can still be represented by a single rounded
   * rect in a trivial way. do that. */
  graphene_rect_intersection (non_rounded, &rounded->bounds, &result->bounds);

  for (int i = 0; i < 4; i++)
    {
      if (corners[i])
        result->corner[i] = rounded->corner[i];
      else
        result->corner[i].width = result->corner[i].height = 0;
    }

  return true;
}

/* This code intersects the current (maybe rounded) clip with the new
 * non-rounded clip */
static inline void
render_clipped_child (GskGLRenderer         *self,
                      RenderOpBuilder       *builder,
                      const graphene_rect_t *clip,
                      GskRenderNode         *child)
{
  graphene_rect_t transformed_clip;
  GskRoundedRect intersection;

  ops_transform_bounds_modelview (builder, clip, &transformed_clip);

  if (builder->clip_is_rectilinear)
    {
      memset (&intersection, 0, sizeof (GskRoundedRect));
      graphene_rect_intersection (&transformed_clip,
                                  &builder->current_clip->bounds,
                                  &intersection.bounds);

      ops_push_clip (builder, &intersection);
      gsk_gl_renderer_add_render_ops (self, child, builder);
      ops_pop_clip (builder);
    }
  else if (intersect_rounded_rectilinear (&transformed_clip,
                                          builder->current_clip,
                                          &intersection))
    {
      ops_push_clip (builder, &intersection);
      gsk_gl_renderer_add_render_ops (self, child, builder);
      ops_pop_clip (builder);
    }
  else
    {
      /* well fuck */
      const float scale_x = builder->scale_x;
      const float scale_y = builder->scale_y;
      const GskRoundedRect scaled_clip = GSK_ROUNDED_RECT_INIT ((builder->dx + clip->origin.x) * scale_x,
                                                                (builder->dy + clip->origin.y) * scale_y,
                                                                clip->size.width * scale_x,
                                                                clip->size.height * scale_y);
      gboolean is_offscreen;
      TextureRegion region;

      ops_push_clip (builder, &scaled_clip);
      if (!add_offscreen_ops (self, builder, &child->bounds,
                              child,
                              &region, &is_offscreen,
                              FORCE_OFFSCREEN))
        g_assert_not_reached ();
      ops_pop_clip (builder);

      ops_set_program (builder, &self->programs->blit_program);
      ops_set_texture (builder, region.texture_id);

      load_offscreen_vertex_data (ops_draw (builder, NULL), child, builder);
    }
}

static inline void
render_clip_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder)
{
  const graphene_rect_t *clip = gsk_clip_node_get_clip (node);
  GskRenderNode *child = gsk_clip_node_get_child (node);

  render_clipped_child (self, builder, clip, child);
}

static inline void
render_rounded_clip_node (GskGLRenderer       *self,
                          GskRenderNode       *node,
                          RenderOpBuilder     *builder)
{
  const float scale_x = builder->scale_x;
  const float scale_y = builder->scale_y;
  const GskRoundedRect *clip = gsk_rounded_clip_node_get_clip (node);
  GskRenderNode *child = gsk_rounded_clip_node_get_child (node);
  GskRoundedRect transformed_clip;
  gboolean need_offscreen;
  int i;

  if (node_is_invisible (child))
    return;

  ops_transform_bounds_modelview (builder, &clip->bounds, &transformed_clip.bounds);
  for (i = 0; i < 4; i ++)
    {
      transformed_clip.corner[i].width = clip->corner[i].width * scale_x;
      transformed_clip.corner[i].height = clip->corner[i].height * scale_y;
    }

  if (builder->clip_is_rectilinear)
    {
      GskRoundedRect intersected_clip;

      if (intersect_rounded_rectilinear (&builder->current_clip->bounds,
                                         &transformed_clip,
                                         &intersected_clip))
        {
          ops_push_clip (builder, &intersected_clip);
          gsk_gl_renderer_add_render_ops (self, child, builder);
          ops_pop_clip (builder);
          return;
        }
    }

  /* After this point we are really working with a new and a current clip
   * which both have rounded corners. */

  if (!ops_has_clip (builder))
    need_offscreen = FALSE;
  else if (rounded_inner_rect_contains_rect (builder->current_clip,
                                             &transformed_clip.bounds))
    need_offscreen = FALSE;
  else
    need_offscreen = TRUE;

  if (!need_offscreen)
    {
      /* If they don't intersect at all, we can simply set
       * the new clip and add the render ops */

      /* If the new clip entirely contains the current clip, the intersection is simply
       * the current clip, so we can ignore the new one */
      if (rounded_inner_rect_contains_rect (&transformed_clip, &builder->current_clip->bounds))
        {
          gsk_gl_renderer_add_render_ops (self, child, builder);
          return;
        }

      ops_push_clip (builder, &transformed_clip);
      gsk_gl_renderer_add_render_ops (self, child, builder);
      ops_pop_clip (builder);
    }
  else
    {
      gboolean is_offscreen;
      TextureRegion region;

      ops_push_clip (builder, &transformed_clip);
      if (!add_offscreen_ops (self, builder, &node->bounds,
                              child,
                              &region, &is_offscreen,
                              FORCE_OFFSCREEN))
        g_assert_not_reached ();
      ops_pop_clip (builder);

      ops_set_program (builder, &self->programs->blit_program);
      ops_set_texture (builder, region.texture_id);

      load_vertex_data_with_region (ops_draw (builder, NULL), &node->bounds, builder,
                                    &region, is_offscreen);
    }
}

static inline void
render_color_matrix_node (GskGLRenderer       *self,
                          GskRenderNode       *node,
                          RenderOpBuilder     *builder)
{
  GskRenderNode *child = gsk_color_matrix_node_get_child (node);
  TextureRegion region;
  gboolean is_offscreen;

  if (node_is_invisible (child))
    return;

  if (!add_offscreen_ops (self, builder,
                          &node->bounds,
                          child,
                          &region, &is_offscreen,
                          RESET_CLIP))
    g_assert_not_reached ();

  ops_set_program (builder, &self->programs->color_matrix_program);
  ops_set_color_matrix (builder,
                        gsk_color_matrix_node_get_color_matrix (node),
                        gsk_color_matrix_node_get_color_offset (node));

  ops_set_texture (builder, region.texture_id);

  load_vertex_data_with_region (ops_draw (builder, NULL),
                                &node->bounds, builder,
                                &region,
                                is_offscreen);
}

static inline int
blur_texture (GskGLRenderer       *self,
              RenderOpBuilder     *builder,
              const TextureRegion *region,
              const int            texture_to_blur_width,
              const int            texture_to_blur_height,
              float                blur_radius_x,
              float                blur_radius_y)
{
  const GskRoundedRect new_clip = GSK_ROUNDED_RECT_INIT (0, 0, texture_to_blur_width, texture_to_blur_height);
  int pass1_texture_id, pass1_render_target;
  int pass2_texture_id, pass2_render_target;
  int prev_render_target;
  graphene_matrix_t prev_projection;
  graphene_rect_t prev_viewport;
  graphene_matrix_t item_proj;
  OpBlur *op;

  g_assert (blur_radius_x > 0);
  g_assert (blur_radius_y > 0);

  gsk_gl_driver_create_render_target (self->gl_driver,
                                      MAX (texture_to_blur_width, 1), MAX (texture_to_blur_height, 1),
                                      GL_NEAREST, GL_NEAREST,
                                      &pass1_texture_id, &pass1_render_target);

  if (texture_to_blur_width <= 0 || texture_to_blur_height <= 0)
    {
      return pass1_texture_id;
    }

  gsk_gl_driver_create_render_target (self->gl_driver,
                                      texture_to_blur_width, texture_to_blur_height,
                                      GL_NEAREST, GL_NEAREST,
                                      &pass2_texture_id, &pass2_render_target);

  init_projection_matrix (&item_proj, &new_clip.bounds);

  ops_set_program (builder, &self->programs->blur_program);
  prev_projection = ops_set_projection (builder, &item_proj);
  ops_set_modelview (builder, NULL);
  prev_viewport = ops_set_viewport (builder, &new_clip.bounds);
  ops_push_clip (builder, &new_clip);

  prev_render_target = ops_set_render_target (builder, pass1_render_target);
  ops_begin (builder, OP_CLEAR);

  op = ops_begin (builder, OP_CHANGE_BLUR);
  op->size.width = texture_to_blur_width;
  op->size.height = texture_to_blur_height;
  op->radius = blur_radius_x;
  op->dir[0] = 1;
  op->dir[1] = 0;
  ops_set_texture (builder, region->texture_id);

  load_vertex_data_with_region (ops_draw (builder, NULL),
                                &new_clip.bounds,
                                builder, region,
                                FALSE);
#if 0
  {
    static int k;
    ops_dump_framebuffer (builder,
                          g_strdup_printf ("pass1_%d.png", k++),
                          texture_to_blur_width,
                          texture_to_blur_height);
  }
#endif
  op = ops_begin (builder, OP_CHANGE_BLUR);
  op->size.width = texture_to_blur_width;
  op->size.height = texture_to_blur_height;
  op->radius = blur_radius_y;
  op->dir[0] = 0;
  op->dir[1] = 1;
  ops_set_texture (builder, pass1_texture_id);
  ops_set_render_target (builder, pass2_render_target);
  ops_begin (builder, OP_CLEAR);
  load_vertex_data_with_region (ops_draw (builder, NULL), /* render pass 2 */
                                &new_clip.bounds,
                                builder, region,
                                FALSE);

#if 0
  {
    static int k;
    ops_dump_framebuffer (builder,
                          g_strdup_printf ("blurred%d.png", k++),
                          texture_to_blur_width,
                          texture_to_blur_height);
  }
#endif

  ops_set_render_target (builder, prev_render_target);
  ops_set_viewport (builder, &prev_viewport);
  ops_set_projection (builder, &prev_projection);
  ops_pop_modelview (builder);
  ops_pop_clip (builder);

  return pass2_texture_id;
}

static inline void
blur_node (GskGLRenderer   *self,
           GskRenderNode   *node,
           RenderOpBuilder *builder,
           float            blur_radius,
           guint            extra_flags,
           TextureRegion   *out_region,
           float           *out_vertex_data[4]) /* min, max, min, max */
{
  const float scale_x = builder->scale_x;
  const float scale_y = builder->scale_y;
  const float blur_extra = blur_radius * 2.0; /* 2.0 = shader radius_multiplier */
  float texture_width, texture_height;
  gboolean is_offscreen;
  TextureRegion region;
  int blurred_texture_id;

  g_assert (blur_radius > 0);

  /* Increase texture size for the given blur radius and scale it */
  texture_width  = ceilf ((node->bounds.size.width  + blur_extra));
  texture_height = ceilf ((node->bounds.size.height + blur_extra));

  /* Only blur this if the out region has no texture id yet */
  if (out_region->texture_id == 0)
    {
      if (!add_offscreen_ops (self, builder,
                              &GRAPHENE_RECT_INIT (node->bounds.origin.x - (blur_extra / 2.0),
                                                   node->bounds.origin.y - (blur_extra / 2.0),
                                                   texture_width, texture_height),
                              node,
                              &region, &is_offscreen,
                              RESET_CLIP | FORCE_OFFSCREEN | extra_flags))
        g_assert_not_reached ();

      blurred_texture_id = blur_texture (self, builder,
                                         &region,
                                         texture_width * scale_x, texture_height * scale_y,
                                         blur_radius * scale_x,
                                         blur_radius * scale_y);
      init_full_texture_region (out_region, blurred_texture_id);
    }

  if (out_vertex_data)
    {
      *out_vertex_data[0] = builder->dx + node->bounds.origin.x - (blur_extra / 2.0);
      *out_vertex_data[1] = builder->dx + node->bounds.origin.x + node->bounds.size.width + (blur_extra / 2.0);
      *out_vertex_data[2] = builder->dy + node->bounds.origin.y - (blur_extra / 2.0);
      *out_vertex_data[3] = builder->dy + node->bounds.origin.y + node->bounds.size.height + (blur_extra / 2.0);
    }
}

static inline void
render_blur_node (GskGLRenderer   *self,
                  GskRenderNode   *node,
                  RenderOpBuilder *builder)
{
  const float blur_radius = gsk_blur_node_get_radius (node);
  GskRenderNode *child = gsk_blur_node_get_child (node);
  TextureRegion blurred_region;
  GskTextureKey key;
  float min_x, max_x, min_y, max_y;

  if (node_is_invisible (child))
    return;

  if (blur_radius <= 0)
    {
      gsk_gl_renderer_add_render_ops (self, child, builder);
      return;
    }

  key.pointer = node;
  key.pointer_is_child = FALSE;
  key.scale_x = builder->scale_x;
  key.scale_y = builder->scale_y;
  key.filter = GL_NEAREST;
  blurred_region.texture_id = gsk_gl_driver_get_texture_for_key (self->gl_driver, &key);
  blur_node (self, child, builder, blur_radius, 0, &blurred_region,
             (float*[4]){&min_x, &max_x, &min_y, &max_y});

  g_assert (blurred_region.texture_id != 0);

  /* Draw the result */
  ops_set_program (builder, &self->programs->blit_program);
  ops_set_texture (builder, blurred_region.texture_id);
  fill_vertex_data (ops_draw (builder, NULL), min_x, min_y, max_x, max_y);


  /* Add to cache for the blur node */
  gsk_gl_driver_set_texture_for_key (self->gl_driver, &key, blurred_region.texture_id);
}

static inline void
render_unblurred_inset_shadow_node (GskGLRenderer   *self,
                                    GskRenderNode   *node,
                                    RenderOpBuilder *builder)
{
  const float dx = gsk_inset_shadow_node_get_dx (node);
  const float dy = gsk_inset_shadow_node_get_dy (node);
  const float spread = gsk_inset_shadow_node_get_spread (node);

  g_assert (gsk_inset_shadow_node_get_blur_radius (node) == 0);

  ops_set_program (builder, &self->programs->inset_shadow_program);
  ops_set_inset_shadow (builder, transform_rect (self, builder, gsk_inset_shadow_node_get_outline (node)),
                        spread,
                        gsk_inset_shadow_node_get_color (node),
                        dx, dy);

  load_vertex_data (ops_draw (builder, NULL), &node->bounds, builder);
}

static inline void
render_inset_shadow_node (GskGLRenderer   *self,
                          GskRenderNode   *node,
                          RenderOpBuilder *builder)
{
  const float scale_x = builder->scale_x;
  const float scale_y = builder->scale_y;
  const float blur_radius = gsk_inset_shadow_node_get_blur_radius (node);
  const float blur_extra = blur_radius * 2.0; /* 2.0 = shader radius_multiplier */
  const float dx = gsk_inset_shadow_node_get_dx (node);
  const float dy = gsk_inset_shadow_node_get_dy (node);
  const GskRoundedRect *node_outline = gsk_inset_shadow_node_get_outline (node);
  float texture_width;
  float texture_height;
  int blurred_texture_id;
  GskTextureKey key;

  g_assert (blur_radius > 0);

  texture_width = ceilf ((node_outline->bounds.size.width + blur_extra) * scale_x);
  texture_height = ceilf ((node_outline->bounds.size.height + blur_extra) * scale_y);

  key.pointer = node;
  key.pointer_is_child = FALSE;
  key.scale_x = scale_x;
  key.scale_y = scale_y;
  key.filter = GL_NEAREST;
  blurred_texture_id = gsk_gl_driver_get_texture_for_key (self->gl_driver, &key);
  if (blurred_texture_id == 0)
    {
      const float spread = gsk_inset_shadow_node_get_spread (node) + (blur_extra / 2.0);
      GskRoundedRect outline_to_blur;
      int render_target, texture_id;
      int prev_render_target;
      graphene_matrix_t prev_projection;
      graphene_rect_t prev_viewport;
      graphene_matrix_t item_proj;
      int i;

      /* TODO: In the following code, we have to be careful about where we apply the scale.
       * We're manually scaling stuff (e.g. the outline) so we can later use texture_width
       * and texture_height (which are already scaled) as the geometry and keep the modelview
       * at a scale of 1. That's kinda complicated though... */

      /* Outline of what we actually want to blur later.
       * Spread grows inside, so we don't need to account for that. But the blur will need
       * to read outside of the inset shadow, so we need to draw some color in there. */
      outline_to_blur = *node_outline;
      gsk_rounded_rect_shrink (&outline_to_blur,
                               - blur_extra / 2.0, - blur_extra / 2.0,
                               - blur_extra / 2.0, - blur_extra / 2.0);

      /* Fit to our texture */
      outline_to_blur.bounds.origin.x = 0;
      outline_to_blur.bounds.origin.y = 0;
      outline_to_blur.bounds.size.width *= scale_x;
      outline_to_blur.bounds.size.height *= scale_y;

      for (i = 0; i < 4; i ++)
        {
          outline_to_blur.corner[i].width *= scale_x;
          outline_to_blur.corner[i].height *= scale_y;
        }

      gsk_gl_driver_create_render_target (self->gl_driver,
                                          texture_width, texture_height,
                                          GL_NEAREST, GL_NEAREST,
                                          &texture_id, &render_target);

      init_projection_matrix (&item_proj,
                              &GRAPHENE_RECT_INIT (0, 0, texture_width, texture_height));

      prev_projection = ops_set_projection (builder, &item_proj);
      ops_set_modelview (builder, NULL);
      prev_viewport = ops_set_viewport (builder, &GRAPHENE_RECT_INIT (0, 0, texture_width, texture_height));
      ops_push_clip (builder, &GSK_ROUNDED_RECT_INIT (0, 0, texture_width, texture_height));

      prev_render_target = ops_set_render_target (builder, render_target);
      ops_begin (builder, OP_CLEAR);

      /* Actual inset shadow outline drawing */
      ops_set_program (builder, &self->programs->inset_shadow_program);
      ops_set_inset_shadow (builder, transform_rect (self, builder, &outline_to_blur),
                            spread * MAX (scale_x, scale_y),
                            gsk_inset_shadow_node_get_color (node),
                            dx * scale_x, dy * scale_y);

      load_float_vertex_data (ops_draw (builder, NULL), builder,
                              0, 0, texture_width, texture_height);

      ops_set_render_target (builder, prev_render_target);
      ops_set_viewport (builder, &prev_viewport);
      ops_set_projection (builder, &prev_projection);
      ops_pop_modelview (builder);
      ops_pop_clip (builder);

      blurred_texture_id = blur_texture (self, builder,
                                         &(TextureRegion) { texture_id, 0, 0, 1, 1 },
                                         texture_width,
                                         texture_height,
                                         blur_radius * scale_x,
                                         blur_radius * scale_y);
    }

  g_assert (blurred_texture_id != 0);

  /* Blur the rendered unblurred inset shadow */
  /* Use a clip to cut away the unwanted parts outside of the original outline */
  {
    const gboolean needs_clip = !gsk_rounded_rect_is_rectilinear (node_outline);
    const float tx1 = blur_extra / 2.0 * scale_x / texture_width;
    const float tx2 = 1.0 - tx1;
    const float ty1 = blur_extra / 2.0 * scale_y / texture_height;
    const float ty2 = 1.0 - ty1;

    gsk_gl_driver_set_texture_for_key (self->gl_driver, &key, blurred_texture_id);

    if (needs_clip)
      {
        GskRoundedRect node_clip;

        ops_transform_bounds_modelview (builder, &node_outline->bounds, &node_clip.bounds);
        for (int i = 0; i < 4; i ++)
          {
            node_clip.corner[i].width = node_outline->corner[i].width * scale_x;
            node_clip.corner[i].height = node_outline->corner[i].height * scale_y;
          }

        ops_push_clip (builder, &node_clip);
      }

    ops_set_program (builder, &self->programs->blit_program);
    ops_set_texture (builder, blurred_texture_id);

    load_vertex_data_with_region (ops_draw (builder, NULL),
                                  &node->bounds, builder,
                                  &(TextureRegion) { 0, tx1, ty1, tx2, ty2 },
                                  TRUE);

    if (needs_clip)
      ops_pop_clip (builder);
  }

}

/* Spread *grows* the outline. The offset moves the shadow and leaves the
 * inner rect where it was */
static inline void
render_unblurred_outset_shadow_node (GskGLRenderer   *self,
                                     GskRenderNode   *node,
                                     RenderOpBuilder *builder)
{
  const GskRoundedRect *outline = gsk_outset_shadow_node_get_outline (node);
  const float x = node->bounds.origin.x;
  const float y = node->bounds.origin.y;
  const float w = node->bounds.size.width;
  const float h = node->bounds.size.height;
  const float spread = gsk_outset_shadow_node_get_spread (node);
  const float dx = gsk_outset_shadow_node_get_dx (node);
  const float dy = gsk_outset_shadow_node_get_dy (node);
  const float edge_sizes[] = { // Top, right, bottom, left
    spread - dy, spread + dx, spread + dy, spread - dx
  };
  const float corner_sizes[][2] = { // top left, top right, bottom right, bottom left
    { outline->corner[0].width + spread - dx, outline->corner[0].height + spread - dy },
    { outline->corner[1].width + spread + dx, outline->corner[1].height + spread - dy },
    { outline->corner[2].width + spread + dx, outline->corner[2].height + spread + dy },
    { outline->corner[3].width + spread - dx, outline->corner[3].height + spread + dy },
  };

  ops_set_program (builder, &self->programs->unblurred_outset_shadow_program);
  ops_set_unblurred_outset_shadow (builder, transform_rect (self, builder, outline),
                                   spread,
                                   gsk_outset_shadow_node_get_color (node),
                                   dx, dy);

  /* Corners... */
  if (corner_sizes[0][0] > 0 && corner_sizes[0][1] > 0) /* Top left */
      load_float_vertex_data (ops_draw (builder, NULL), builder,
                              x, y,
                              corner_sizes[0][0], corner_sizes[0][1]);
  if (corner_sizes[1][0] > 0 && corner_sizes[1][1] > 0) /* Top right */
      load_float_vertex_data (ops_draw (builder, NULL), builder,
                              x + w - corner_sizes[1][0], y,
                              corner_sizes[1][0], corner_sizes[1][1]);
  if (corner_sizes[2][0] > 0 && corner_sizes[2][1] > 0) /* Bottom right */
      load_float_vertex_data (ops_draw (builder, NULL), builder,
                              x + w - corner_sizes[2][0], y + h - corner_sizes[2][1],
                              corner_sizes[2][0], corner_sizes[2][1]);
  if (corner_sizes[3][0] > 0 && corner_sizes[3][1] > 0) /* Bottom left */
      load_float_vertex_data (ops_draw (builder, NULL), builder,
                              x, y + h - corner_sizes[3][1],
                              corner_sizes[3][0], corner_sizes[3][1]);
  /* Edges... */;
  if (edge_sizes[0] > 0) /* Top */
    load_float_vertex_data (ops_draw (builder, NULL), builder,
                            x + corner_sizes[0][0], y,
                            w - corner_sizes[0][0] - corner_sizes[1][0], edge_sizes[0]);
  if (edge_sizes[1] > 0) /* Right */
    load_float_vertex_data (ops_draw (builder, NULL), builder,
                            x + w - edge_sizes[1], y + corner_sizes[1][1],
                            edge_sizes[1], h - corner_sizes[1][1] - corner_sizes[2][1]);
  if (edge_sizes[2] > 0) /* Bottom */
    load_float_vertex_data (ops_draw (builder, NULL), builder,
                            x + corner_sizes[3][0], y + h - edge_sizes[2],
                            w - corner_sizes[3][0] - corner_sizes[2][0], edge_sizes[2]);
  if (edge_sizes[3] > 0) /* Left */
    load_float_vertex_data (ops_draw (builder, NULL), builder,
                            x, y + corner_sizes[0][1],
                            edge_sizes[3], h - corner_sizes[0][1] - corner_sizes[3][1]);
}


static GdkRGBA COLOR_WHITE = { 1, 1, 1, 1 };
static inline void
render_outset_shadow_node (GskGLRenderer   *self,
                           GskRenderNode   *node,
                           RenderOpBuilder *builder)
{
  const float scale_x = builder->scale_x;
  const float scale_y = builder->scale_y;
  const GskRoundedRect *outline = gsk_outset_shadow_node_get_outline (node);
  const GdkRGBA *color = gsk_outset_shadow_node_get_color (node);
  const float blur_radius = gsk_outset_shadow_node_get_blur_radius (node);
  const float blur_extra = blur_radius * 2.0f; /* 2.0 = shader radius_multiplier */
  const int extra_blur_pixels = (int) ceilf(blur_extra / 2.0 * MAX (scale_x, scale_y)); /* TODO: No need to MAX() her actually */
  const float spread = gsk_outset_shadow_node_get_spread (node);
  const float dx = gsk_outset_shadow_node_get_dx (node);
  const float dy = gsk_outset_shadow_node_get_dy (node);
  GskRoundedRect scaled_outline;
  int texture_width, texture_height;
  OpOutsetShadow *shadow;
  int blurred_texture_id;
  int cached_tid;
  bool do_slicing;

  /* scaled_outline is the minimal outline we need to draw the given drop shadow,
   * enlarged by the spread and offset by the blur radius. */
  scaled_outline = *outline;

  if (outline->bounds.size.width < blur_extra ||
      outline->bounds.size.height < blur_extra)
    {
      do_slicing = false;
      gsk_rounded_rect_shrink (&scaled_outline, -spread, -spread, -spread, -spread);
    }
  else
    {
      /* Shrink our outline to the minimum size that can still hold all the border radii */
      gsk_rounded_rect_shrink_to_minimum (&scaled_outline);
      /* Increase by the spread */
      gsk_rounded_rect_shrink (&scaled_outline, -spread, -spread, -spread, -spread);
      /* Grow bounds but don't grow corners */
      graphene_rect_inset (&scaled_outline.bounds, - blur_extra / 2.0, - blur_extra / 2.0);
      /* For the center part, we add a few pixels */
      scaled_outline.bounds.size.width += SHADOW_EXTRA_SIZE;
      scaled_outline.bounds.size.height += SHADOW_EXTRA_SIZE;

      do_slicing = true;
    }

  texture_width  = (int)ceil ((scaled_outline.bounds.size.width  + blur_extra) * scale_x);
  texture_height = (int)ceil ((scaled_outline.bounds.size.height + blur_extra) * scale_y);

  scaled_outline.bounds.origin.x = extra_blur_pixels;
  scaled_outline.bounds.origin.y = extra_blur_pixels;
  scaled_outline.bounds.size.width = texture_width - (extra_blur_pixels * 2);
  scaled_outline.bounds.size.height = texture_height - (extra_blur_pixels * 2);

  for (int i = 0; i < 4; i ++)
    {
      scaled_outline.corner[i].width *= scale_x;
      scaled_outline.corner[i].height *= scale_y;
    }

  cached_tid = gsk_gl_shadow_cache_get_texture_id (&self->shadow_cache,
                                                   self->gl_driver,
                                                   &scaled_outline,
                                                   blur_radius);

  if (cached_tid == 0)
    {
      int texture_id, render_target;
      int prev_render_target;
      graphene_matrix_t prev_projection;
      graphene_rect_t prev_viewport;
      graphene_matrix_t item_proj;

      gsk_gl_driver_create_render_target (self->gl_driver,
                                          texture_width, texture_height,
                                          GL_NEAREST, GL_NEAREST,
                                          &texture_id, &render_target);
      if (gdk_gl_context_has_debug (self->gl_context))
        {
          gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, texture_id,
                                              "Outset Shadow Temp %d", texture_id);
          gdk_gl_context_label_object_printf  (self->gl_context, GL_FRAMEBUFFER, render_target,
                                               "Outset Shadow FB Temp %d", render_target);
        }

      ops_set_program (builder, &self->programs->color_program);
      init_projection_matrix (&item_proj,
                              &GRAPHENE_RECT_INIT (0, 0, texture_width, texture_height));

      prev_render_target = ops_set_render_target (builder, render_target);
      ops_begin (builder, OP_CLEAR);
      prev_projection = ops_set_projection (builder, &item_proj);
      ops_set_modelview (builder, NULL);
      prev_viewport = ops_set_viewport (builder, &GRAPHENE_RECT_INIT (0, 0, texture_width, texture_height));

      /* Draw outline */
      ops_push_clip (builder, &scaled_outline);
      ops_set_color (builder, &COLOR_WHITE);
      load_float_vertex_data (ops_draw (builder, NULL), builder,
                              0, 0, texture_width, texture_height);

      ops_pop_clip (builder);
      ops_set_viewport (builder, &prev_viewport);
      ops_pop_modelview (builder);
      ops_set_projection (builder, &prev_projection);
      ops_set_render_target (builder, prev_render_target);

      /* Now blur the outline */
      blurred_texture_id = blur_texture (self, builder,
                                         &(TextureRegion) { texture_id, 0, 0, 1, 1 },
                                         texture_width,
                                         texture_height,
                                         blur_radius * scale_x,
                                         blur_radius * scale_y);

      gsk_gl_driver_mark_texture_permanent (self->gl_driver, blurred_texture_id);
      gsk_gl_shadow_cache_commit (&self->shadow_cache,
                                  &scaled_outline,
                                  blur_radius,
                                  blurred_texture_id);
    }
  else
    {
      blurred_texture_id = cached_tid;
    }


  if (!do_slicing)
    {
      const float min_x = floorf (outline->bounds.origin.x - spread - (blur_extra / 2.0) + dx);
      const float min_y = floorf (outline->bounds.origin.y - spread - (blur_extra / 2.0) + dy);

      ops_set_program (builder, &self->programs->outset_shadow_program);
      ops_set_color (builder, color);
      ops_set_texture (builder, blurred_texture_id);

      shadow = ops_begin (builder, OP_CHANGE_OUTSET_SHADOW);
      shadow->outline.value = transform_rect (self, builder, outline);
      shadow->outline.send = TRUE;

      load_vertex_data_with_region (ops_draw (builder, NULL),
                                    &GRAPHENE_RECT_INIT (
                                      min_x, min_y,
                                      texture_width / scale_x, texture_height / scale_y
                                    ), builder,
                                    &(TextureRegion) { 0, 0, 0, 1, 1 },
                                    FALSE);
      return;
    }


  ops_set_program (builder, &self->programs->outset_shadow_program);
  ops_set_color (builder, color);
  ops_set_texture (builder, blurred_texture_id);

  shadow = ops_begin (builder, OP_CHANGE_OUTSET_SHADOW);
  shadow->outline.value = transform_rect (self, builder, outline);
  shadow->outline.send = TRUE;

  {
    const float min_x = floorf (outline->bounds.origin.x - spread - (blur_extra / 2.0) + dx);
    const float min_y = floorf (outline->bounds.origin.y - spread - (blur_extra / 2.0) + dy);
    const float max_x = ceilf (outline->bounds.origin.x + outline->bounds.size.width +
                               (blur_extra / 2.0) + dx + spread);
    const float max_y = ceilf (outline->bounds.origin.y + outline->bounds.size.height +
                               (blur_extra / 2.0) + dy + spread);
    cairo_rectangle_int_t slices[9];
    TextureRegion tregs[9];

    /* TODO: The slicing never changes and could just go into the cache */
    nine_slice_rounded_rect (&scaled_outline, slices);
    nine_slice_grow (slices, extra_blur_pixels);
    nine_slice_to_texture_coords (slices, texture_width, texture_height, tregs);

    /* Our texture coordinates MUST be scaled, while the actual vertex coords
     * MUST NOT be scaled. */

    /* Top left */
    if (slice_is_visible (&slices[NINE_SLICE_TOP_LEFT]))
      {
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        min_x, min_y,
                                        slices[NINE_SLICE_TOP_LEFT].width / scale_x,
                                        slices[NINE_SLICE_TOP_LEFT].height / scale_y
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_TOP_LEFT], TRUE);
      }

    /* Top center */
    if (slice_is_visible (&slices[NINE_SLICE_TOP_CENTER]))
      {
        const float width = (max_x - min_x) - (slices[NINE_SLICE_TOP_LEFT].width / scale_x +
                                               slices[NINE_SLICE_TOP_RIGHT].width / scale_x);
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        min_x + (slices[NINE_SLICE_TOP_LEFT].width / scale_x),
                                        min_y,
                                        width,
                                        slices[NINE_SLICE_TOP_CENTER].height / scale_y
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_TOP_CENTER], TRUE);
      }
    /* Top right */
    if (slice_is_visible (&slices[NINE_SLICE_TOP_RIGHT]))
      {
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        max_x - (slices[NINE_SLICE_TOP_RIGHT].width / scale_x),
                                        min_y,
                                        slices[NINE_SLICE_TOP_RIGHT].width / scale_x,
                                        slices[NINE_SLICE_TOP_RIGHT].height / scale_y
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_TOP_RIGHT], TRUE);
      }

    /* Bottom right */
    if (slice_is_visible (&slices[NINE_SLICE_BOTTOM_RIGHT]))
      {
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        max_x - (slices[NINE_SLICE_BOTTOM_RIGHT].width / scale_x),
                                        max_y - (slices[NINE_SLICE_BOTTOM_RIGHT].height / scale_y),
                                        slices[NINE_SLICE_BOTTOM_RIGHT].width / scale_x,
                                        slices[NINE_SLICE_BOTTOM_RIGHT].height / scale_y
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_BOTTOM_RIGHT], TRUE);
      }

    /* Bottom left */
    if (slice_is_visible (&slices[NINE_SLICE_BOTTOM_LEFT]))
      {
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        min_x,
                                        max_y - (slices[NINE_SLICE_BOTTOM_LEFT].height / scale_y),
                                        slices[NINE_SLICE_BOTTOM_LEFT].width / scale_x,
                                        slices[NINE_SLICE_BOTTOM_LEFT].height / scale_y
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_BOTTOM_LEFT], TRUE);
      }

    /* Left side */
    if (slice_is_visible (&slices[NINE_SLICE_LEFT_CENTER]))
      {
        const float height = (max_y - min_y) - (slices[NINE_SLICE_TOP_LEFT].height / scale_y +
                                                slices[NINE_SLICE_BOTTOM_LEFT].height / scale_y);
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        min_x,
                                        min_y + (slices[NINE_SLICE_TOP_LEFT].height / scale_y),
                                        slices[NINE_SLICE_LEFT_CENTER].width / scale_x,
                                        height
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_LEFT_CENTER], TRUE);
      }

    /* Right side */
    if (slice_is_visible (&slices[NINE_SLICE_RIGHT_CENTER]))
      {
        const float height = (max_y - min_y) - (slices[NINE_SLICE_TOP_RIGHT].height / scale_y +
                                                slices[NINE_SLICE_BOTTOM_RIGHT].height / scale_y);
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        max_x - (slices[NINE_SLICE_RIGHT_CENTER].width / scale_x),
                                        min_y + (slices[NINE_SLICE_TOP_LEFT].height / scale_y),
                                        slices[NINE_SLICE_RIGHT_CENTER].width / scale_x,
                                        height
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_RIGHT_CENTER], TRUE);
      }

    /* Bottom side */
    if (slice_is_visible (&slices[NINE_SLICE_BOTTOM_CENTER]))
      {
        const float width = (max_x - min_x) - (slices[NINE_SLICE_BOTTOM_LEFT].width / scale_x +
                                               slices[NINE_SLICE_BOTTOM_RIGHT].width / scale_x);
        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        min_x + (slices[NINE_SLICE_BOTTOM_LEFT].width / scale_x),
                                        max_y - (slices[NINE_SLICE_BOTTOM_CENTER].height / scale_y),
                                        width,
                                        slices[NINE_SLICE_BOTTOM_CENTER].height / scale_y
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_BOTTOM_CENTER], TRUE);
      }

    /* Middle */
    if (slice_is_visible (&slices[NINE_SLICE_CENTER]))
      {
        const float width = (max_x - min_x) - (slices[NINE_SLICE_LEFT_CENTER].width / scale_x +
                                               slices[NINE_SLICE_RIGHT_CENTER].width / scale_x);
        const float height = (max_y - min_y) - (slices[NINE_SLICE_TOP_CENTER].height / scale_y +
                                                slices[NINE_SLICE_BOTTOM_CENTER].height / scale_y);

        load_vertex_data_with_region (ops_draw (builder, NULL),
                                      &GRAPHENE_RECT_INIT (
                                        min_x + (slices[NINE_SLICE_LEFT_CENTER].width / scale_x),
                                        min_y + (slices[NINE_SLICE_TOP_CENTER].height / scale_y),
                                        width, height
                                      ),
                                      builder,
                                      &tregs[NINE_SLICE_CENTER], TRUE);
      }
  }
}

static inline void
render_shadow_node (GskGLRenderer   *self,
                    GskRenderNode   *node,
                    RenderOpBuilder *builder)
{
  const gsize n_shadows = gsk_shadow_node_get_n_shadows (node);
  GskRenderNode *original_child = gsk_shadow_node_get_child (node);
  GskRenderNode *shadow_child = original_child;
  guint i;

  /* Shadow nodes recolor every pixel of the source texture, but leave the alpha in tact.
   * If the child is a color matrix node that doesn't touch the alpha, we can throw that away. */
  if (gsk_render_node_get_node_type (shadow_child) == GSK_COLOR_MATRIX_NODE &&
      !color_matrix_modifies_alpha (shadow_child))
    {
      shadow_child = gsk_color_matrix_node_get_child (shadow_child);
    }

  for (i = 0; i < n_shadows; i ++)
    {
      const GskShadow *shadow = gsk_shadow_node_get_shadow (node, i);
      const float dx = shadow->dx;
      const float dy = shadow->dy;
      TextureRegion region;
      gboolean is_offscreen;
      graphene_rect_t bounds;

      if (shadow->radius == 0 &&
          gsk_render_node_get_node_type (shadow_child) == GSK_TEXT_NODE)
        {
          ops_offset (builder, dx, dy);
          render_text_node (self, shadow_child, builder, &shadow->color, TRUE);
          ops_offset (builder, - dx, - dy);
          continue;
        }

      if (gdk_rgba_is_clear (&shadow->color))
        continue;

      if (node_is_invisible (shadow_child))
        continue;

      if (shadow->radius > 0)
        {
          float min_x;
          float min_y;
          float max_x;
          float max_y;

          region.texture_id = 0;
          blur_node (self, shadow_child, builder, shadow->radius, NO_CACHE_PLZ, &region,
                     (float*[4]){&min_x, &max_x, &min_y, &max_y});
          bounds.origin.x = min_x - builder->dx;
          bounds.origin.y = min_y - builder->dy;
          bounds.size.width = max_x - min_x;
          bounds.size.height = max_y - min_y;
          is_offscreen = TRUE;
        }
      else if (dx == 0 && dy == 0)
        {
          continue; /* Invisible anyway */
        }
      else
        {
          if (!add_offscreen_ops (self, builder,
                                  &shadow_child->bounds,
                                  shadow_child, &region, &is_offscreen,
                                  RESET_CLIP | NO_CACHE_PLZ))
            g_assert_not_reached ();

          bounds = shadow_child->bounds;
        }

      ops_set_program (builder, &self->programs->coloring_program);
      ops_set_color (builder, &shadow->color);
      ops_set_texture (builder, region.texture_id);

      ops_offset (builder, dx, dy);
      load_vertex_data_with_region (ops_draw (builder, NULL),
                                    &bounds, builder,
                                    &region,
                                    is_offscreen);
      ops_offset (builder, -dx, -dy);
    }

  /* Now draw the child normally */
  gsk_gl_renderer_add_render_ops (self, original_child, builder);
}

static inline void
render_cross_fade_node (GskGLRenderer   *self,
                        GskRenderNode   *node,
                        RenderOpBuilder *builder)
{
  GskRenderNode *start_node = gsk_cross_fade_node_get_start_child (node);
  GskRenderNode *end_node = gsk_cross_fade_node_get_end_child (node);
  const float progress = gsk_cross_fade_node_get_progress (node);
  TextureRegion start_region;
  TextureRegion end_region;
  gboolean is_offscreen1, is_offscreen2;
  OpCrossFade *op;

  if (progress <= 0)
    {
      gsk_gl_renderer_add_render_ops (self, start_node, builder);
      return;
    }
  else if (progress >= 1)
    {
      gsk_gl_renderer_add_render_ops (self, end_node, builder);
      return;
    }

  if (equal_texture_nodes (start_node, end_node))
    {
      gsk_gl_renderer_add_render_ops (self, end_node, builder);
      return;
    }

  /* TODO: We create 2 textures here as big as the cross-fade node, but both the
   * start and the end node might be a lot smaller than that. */

  if (!add_offscreen_ops (self, builder,
                          &node->bounds,
                          start_node,
                          &start_region, &is_offscreen1,
                          FORCE_OFFSCREEN | RESET_CLIP))
    {
      gsk_gl_renderer_add_render_ops (self, end_node, builder);
      return;
    }

  if (!add_offscreen_ops (self, builder,
                          &node->bounds,
                          end_node,
                          &end_region, &is_offscreen2,
                          FORCE_OFFSCREEN | RESET_CLIP))
    {
      const float prev_opacity = ops_set_opacity (builder, builder->current_opacity * progress);
      gsk_gl_renderer_add_render_ops (self, start_node, builder);
      ops_set_opacity (builder, prev_opacity);

      return;
    }

  ops_set_program (builder, &self->programs->cross_fade_program);

  op = ops_begin (builder, OP_CHANGE_CROSS_FADE);
  op->progress = progress;
  op->source2 = end_region.texture_id;

  ops_set_texture (builder, start_region.texture_id);

  load_offscreen_vertex_data (ops_draw (builder, NULL), node, builder);
}

static inline void
render_blend_node (GskGLRenderer   *self,
                   GskRenderNode   *node,
                   RenderOpBuilder *builder)
{
  GskRenderNode *top_child = gsk_blend_node_get_top_child (node);
  GskRenderNode *bottom_child = gsk_blend_node_get_bottom_child (node);
  TextureRegion top_region;
  TextureRegion bottom_region;
  gboolean is_offscreen1, is_offscreen2;
  OpBlend *op;

  /* TODO: We create 2 textures here as big as the blend node, but both the
   * start and the end node might be a lot smaller than that. */
  if (!add_offscreen_ops (self, builder,
                          &node->bounds,
                          bottom_child,
                          &bottom_region, &is_offscreen1,
                          FORCE_OFFSCREEN | RESET_CLIP))
    {
      gsk_gl_renderer_add_render_ops (self, top_child, builder);
      return;
    }

  if (!add_offscreen_ops (self, builder,
                          &node->bounds,
                          top_child,
                          &top_region, &is_offscreen2,
                          FORCE_OFFSCREEN | RESET_CLIP))
    {
      load_vertex_data_with_region (ops_draw (builder, NULL),
                                    &node->bounds,
                                    builder,
                                    &bottom_region,
                                    TRUE);
      return;
    }

  ops_set_program (builder, &self->programs->blend_program);
  ops_set_texture (builder, bottom_region.texture_id);

  op = ops_begin (builder, OP_CHANGE_BLEND);
  op->source2 = top_region.texture_id;
  op->mode = gsk_blend_node_get_blend_mode (node);

  load_offscreen_vertex_data (ops_draw (builder, NULL), node, builder);
}

static inline void
render_repeat_node (GskGLRenderer   *self,
                    GskRenderNode   *node,
                    RenderOpBuilder *builder)
{
  GskRenderNode *child = gsk_repeat_node_get_child (node);
  const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (node);
  TextureRegion region;
  gboolean is_offscreen;
  OpRepeat *op;

  if (node_is_invisible (child))
    return;

  if (!graphene_rect_equal (child_bounds, &child->bounds))
    {
      /* TODO: Implement these repeat nodes. */
      render_fallback_node (self, node, builder);
      return;
    }

  /* If the size of the repeat node is smaller than the size of the
   * child node, we don't repeat at all and can just draw that part
   * of the child texture... */
  if (graphene_rect_contains_rect (child_bounds, &node->bounds))
    {
      render_clipped_child (self, builder, &node->bounds, child);
      return;
    }

  /* Draw the entire child on a texture */
  if (!add_offscreen_ops (self, builder,
                          &child->bounds,
                          child,
                          &region, &is_offscreen,
                          RESET_CLIP))
    g_assert_not_reached ();

  ops_set_program (builder, &self->programs->repeat_program);
  ops_set_texture (builder, region.texture_id);

  op = ops_begin (builder, OP_CHANGE_REPEAT);
  op->child_bounds[0] = (node->bounds.origin.x - child_bounds->origin.x) / child_bounds->size.width;
  op->child_bounds[1] = (node->bounds.origin.y - child_bounds->origin.y) / child_bounds->size.height;
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

  load_vertex_data_with_region (ops_draw (builder, NULL),
                                &node->bounds, builder,
                                &region,
                                is_offscreen);
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

  graphene_matrix_to_float (&op->matrix, mat);
  OP_PRINT (" -> Modelview { { %f,%f,%f,%f }, { %f,%f,%f,%f }, { %f,%f,%f,%f }, { %f,%f,%f,%f }",
            mat[0], mat[1], mat[2], mat[3],
            mat[4], mat[5], mat[6], mat[7],
            mat[8], mat[9], mat[10], mat[11],
            mat[12], mat[13], mat[14], mat[15]);
  glUniformMatrix4fv (program->modelview_location, 1, GL_FALSE, mat);
}

static inline void
apply_projection_op (const Program  *program,
                     const OpMatrix *op)
{
  float mat[16];

  graphene_matrix_to_float (&op->matrix, mat);
  OP_PRINT (" -> Projection { { %f,%f,%f,%f }, { %f,%f,%f,%f }, { %f,%f,%f,%f }, { %f,%f,%f,%f }",
            mat[0], mat[1], mat[2], mat[3],
            mat[4], mat[5], mat[6], mat[7],
            mat[8], mat[9], mat[10], mat[11],
            mat[12], mat[13], mat[14], mat[15]);
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
            op->rgba->red, op->rgba->green, op->rgba->blue, op->rgba->alpha);
  glUniform4fv (program->color.color_location, 1, (float *)op->rgba);
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
apply_source_extra_texture_op (const Program        *program,
                               const OpExtraTexture *op)
{
  g_assert(op->texture_id != 0);
  OP_PRINT (" -> New extra texture %d: %d", op->idx, op->texture_id);
  glUniform1i (program->glshader.texture_locations[op->idx], op->idx);
  glActiveTexture (GL_TEXTURE0 + op->idx);
  glBindTexture (GL_TEXTURE_2D, op->texture_id);
}

static inline void
apply_color_matrix_op (const Program       *program,
                       const OpColorMatrix *op)
{
  OP_PRINT (" -> Color matrix. Send matrix: %d. Send offset: %d.",
            op->matrix.send, op->offset.send);

  if (op->matrix.send)
    {
      float mat[16];
      graphene_matrix_to_float (op->matrix.value, mat);
      glUniformMatrix4fv (program->color_matrix.color_matrix_location, 1, GL_FALSE, mat);
    }

  if (op->offset.send)
    {
      float vec[4];
      graphene_vec4_to_float (op->offset.value, vec);
      glUniform4fv (program->color_matrix.color_offset_location, 1, vec);
    }
}

static inline void
apply_clip_op (const Program *program,
               const OpClip  *op)
{
  int count;

  if (op->send_corners)
    {
      OP_PRINT (" -> Clip: %s", gsk_rounded_rect_to_string (&op->clip));
      count = 3;
    }
  else
    {
      OP_PRINT (" -> clip: %f, %f, %f, %f",
                op->clip.bounds.origin.x, op->clip.bounds.origin.y,
                op->clip.bounds.size.width, op->clip.bounds.size.height);
      count = 1;
    }

  glUniform4fv (program->clip_rect_location, count, (float *)&op->clip.bounds);
}

static inline void
apply_inset_shadow_op (const Program  *program,
                       const OpShadow *op)
{
  OP_PRINT (" -> inset shadow. Color: %s, Offset: (%f, %f), Spread: %f, Outline: %s",
            op->color.send ? gdk_rgba_to_string (op->color.value) : "don't send",
            op->offset.send ? op->offset.value[0] : -1337.0,
            op->offset.send ? op->offset.value[1] : -1337.0,
            op->spread.send ? op->spread.value : -1337.0,
            op->outline.send ? gsk_rounded_rect_to_string (&op->outline.value) : "don't send");
  if (op->outline.send)
    {
      if (op->outline.send_corners)
        glUniform4fv (program->inset_shadow.outline_rect_location, 3, (float *)&op->outline.value);
      else
        glUniform4fv (program->inset_shadow.outline_rect_location, 1, (float *)&op->outline.value);
    }

  if (op->color.send)
    glUniform4fv (program->inset_shadow.color_location, 1, (float *)op->color.value);

  if (op->spread.send)
    glUniform1f (program->inset_shadow.spread_location, op->spread.value);

  if (op->offset.send)
    glUniform2fv (program->inset_shadow.offset_location, 1, op->offset.value);
}

static inline void
apply_unblurred_outset_shadow_op (const Program  *program,
                                  const OpShadow *op)
{
  OP_PRINT (" -> unblurred outset shadow");

  if (op->outline.send)
    {
      if (op->outline.send_corners)
        glUniform4fv (program->unblurred_outset_shadow.outline_rect_location, 3, (float *)&op->outline.value);
      else
        glUniform4fv (program->unblurred_outset_shadow.outline_rect_location, 1, (float *)&op->outline.value);
    }

  if (op->color.send)
    glUniform4fv (program->unblurred_outset_shadow.color_location, 1, (float *)op->color.value);

  if (op->spread.send)
    glUniform1f (program->unblurred_outset_shadow.spread_location, op->spread.value);

  if (op->offset.send)
    glUniform2fv (program->unblurred_outset_shadow.offset_location, 1, op->offset.value);
}

static inline void
apply_outset_shadow_op (const Program        *program,
                        const OpOutsetShadow *op)
{
  OP_PRINT (" -> outset shadow");
  glUniform4fv (program->outset_shadow.outline_rect_location, 3, (float *)&op->outline.value.bounds);
}

static inline void
apply_linear_gradient_op (const Program          *program,
                          const OpLinearGradient *op)
{
  OP_PRINT (" -> Linear gradient");
  if (op->n_color_stops.send)
    glUniform1i (program->linear_gradient.num_color_stops_location, op->n_color_stops.value);

  if (op->color_stops.send)
    glUniform1fv (program->linear_gradient.color_stops_location,
                  op->n_color_stops.value * 5,
                  (float *)op->color_stops.value);

  glUniform4f (program->linear_gradient.points_location,
               op->start_point[0], op->start_point[1],
               op->end_point[0] - op->start_point[0], op->end_point[1] - op->start_point[1]);
  glUniform1i (program->linear_gradient.repeat_location, op->repeat);
}

static inline void
apply_radial_gradient_op (const Program          *program,
                          const OpRadialGradient *op)
{
  float scale;
  float bias;

  OP_PRINT (" -> Radial gradient");
  if (op->n_color_stops.send)
    glUniform1i (program->radial_gradient.num_color_stops_location, op->n_color_stops.value);

  if (op->color_stops.send)
    glUniform1fv (program->radial_gradient.color_stops_location,
                  op->n_color_stops.value * 5,
                  (float *)op->color_stops.value);

  scale = 1.0f / (op->end - op->start);
  bias = -op->start * scale;

  glUniform1i (program->radial_gradient.repeat_location, op->repeat);
  glUniform2f (program->radial_gradient.range_location, scale, bias);
  glUniform4f (program->radial_gradient.geometry_location,
               op->center[0], op->center[1],
               1.0f / op->radius[0], 1.0f / op->radius[1]);
}

static inline void
apply_conic_gradient_op (const Program         *program,
                         const OpConicGradient *op)
{
  float bias;
  float scale;

  OP_PRINT (" -> Conic gradient");
  if (op->n_color_stops.send)
    glUniform1i (program->conic_gradient.num_color_stops_location, op->n_color_stops.value);

  if (op->color_stops.send)
    glUniform1fv (program->conic_gradient.color_stops_location,
                  op->n_color_stops.value * 5,
                  (float *)op->color_stops.value);

  scale = 0.5f * M_1_PI;
  bias = op->angle * scale + 2.0f;
  glUniform4f (program->conic_gradient.geometry_location, op->center[0], op->center[1], scale, bias);
}

static inline void
apply_border_op (const Program  *program,
                 const OpBorder *op)
{
  OP_PRINT (" -> Border Outline");

  glUniform4fv (program->border.outline_rect_location, 3, (float *)&op->outline.bounds);
}

static inline void
apply_gl_shader_args_op (const Program  *program,
                         const OpGLShader *op)
{
  int n_uniforms, i;
  const GskGLUniform *uniforms;

  OP_PRINT (" -> GL Shader Args");

  glUniform2fv (program->glshader.size_location, 1, op->size);

  uniforms = gsk_gl_shader_get_uniforms (op->shader, &n_uniforms);
  for (i = 0; i < n_uniforms; i++)
    {
      const GskGLUniform *u = &uniforms[i];
      const guchar *data = op->uniform_data + u->offset;

      switch (u->type)
        {
        default:
        case GSK_GL_UNIFORM_TYPE_NONE:
          break;
        case GSK_GL_UNIFORM_TYPE_FLOAT:
          glUniform1fv (program->glshader.args_locations[i], 1, (const float *)data);
          break;
        case GSK_GL_UNIFORM_TYPE_INT:
          glUniform1iv (program->glshader.args_locations[i], 1, (const gint32 *)data);
          break;
        case GSK_GL_UNIFORM_TYPE_UINT:
        case GSK_GL_UNIFORM_TYPE_BOOL:
          glUniform1uiv (program->glshader.args_locations[i], 1, (const guint32 *) data);
          break;
        case GSK_GL_UNIFORM_TYPE_VEC2:
          glUniform2fv (program->glshader.args_locations[i], 1, (const float *)data);
          break;
        case GSK_GL_UNIFORM_TYPE_VEC3:
          glUniform3fv (program->glshader.args_locations[i], 1, (const float *)data);
          break;
        case GSK_GL_UNIFORM_TYPE_VEC4:
          glUniform4fv (program->glshader.args_locations[i], 1, (const float *)data);
          break;
        }
    }
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
  OP_PRINT (" -> Border color: %s", gdk_rgba_to_string (op->color));
  glUniform4fv (program->border.color_location, 1, (float *)op->color);
}

static inline void
apply_blur_op (const Program *program,
               const OpBlur  *op)
{
  OP_PRINT (" -> Blur");
  glUniform1f (program->blur.blur_radius_location, op->radius);
  glUniform2f (program->blur.blur_size_location, op->size.width, op->size.height);
  glUniform2f (program->blur.blur_dir_location, op->dir[0], op->dir[1]);
}

static inline void
apply_cross_fade_op (const Program     *program,
                     const OpCrossFade *op)
{
  OP_PRINT (" -> Cross fade");
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
  OP_PRINT (" -> Repeat");
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

static void
program_init (Program *program)
{
  program->index = -1;
  program->state.opacity = 1.0f;
}

static void
program_finalize (Program *program)
{
  if (program->id > 0)
    glDeleteProgram (program->id);
  if (program->index == -1 &&
      program->glshader.compile_error != NULL)
    g_error_free (program->glshader.compile_error);

  gsk_transform_unref (program->state.modelview);
}

static void
program_free (Program *program)
{
  program_finalize (program);
  g_free (program);
}

static GskGLRendererPrograms *
gsk_gl_renderer_programs_new (void)
{
  GskGLRendererPrograms *programs;
  int i;

  programs = g_new0 (GskGLRendererPrograms, 1);
  programs->ref_count = 1;
  for (i = 0; i < GL_N_PROGRAMS; i ++)
    program_init (&programs->programs[i]);

  /* We use direct hash for performance, not string hash on the source, because we assume each caller
   * reuses a single GskGLShader for all uses and different callers will use different source content. */
  programs->custom_programs = g_hash_table_new_full (g_direct_hash, g_direct_equal, (GDestroyNotify)g_object_unref, (GDestroyNotify)program_free);

  return programs;
}

static GskGLRendererPrograms *
gsk_gl_renderer_programs_ref (GskGLRendererPrograms *programs)
{
  programs->ref_count++;
  return programs;
}

/* Must be called with the context current */
static void
gsk_gl_renderer_programs_unref (GskGLRendererPrograms *programs)
{
  int i;
  programs->ref_count--;
  if (programs->ref_count == 0)
    {
      for (i = 0; i < GL_N_PROGRAMS; i ++)
        program_finalize (&programs->programs[i]);

      g_hash_table_destroy (programs->custom_programs);
      g_free (programs);
    }
}

static Program *
gsk_gl_renderer_lookup_custom_program (GskGLRenderer  *self,
                                       GskGLShader *shader)
{
  return g_hash_table_lookup (self->programs->custom_programs, shader);
}

static Program *
gsk_gl_renderer_create_custom_program (GskGLRenderer  *self,
                                       GskGLShader *shader)
{
  Program *program = g_new0 (Program, 1);

  program_init (program);

  g_hash_table_insert (self->programs->custom_programs, g_object_ref (shader), program);

  return program;
}

static GskGLRendererPrograms *
gsk_gl_renderer_create_programs (GskGLRenderer  *self,
                                 GError        **error)
{
  GskGLShaderBuilder shader_builder;
  GskGLRendererPrograms *programs = NULL;
  int i;
  static const struct {
    const char *resource_path;
    const char *name;
  } program_definitions[] = {
    { "/org/gtk/libgsk/gl/blend.glsl",                     "blend" },
    { "/org/gtk/libgsk/gl/blit.glsl",                      "blit" },
    { "/org/gtk/libgsk/gl/blur.glsl",                      "blur" },
    { "/org/gtk/libgsk/gl/border.glsl",                    "border" },
    { "/org/gtk/libgsk/gl/color_matrix.glsl",              "color matrix" },
    { "/org/gtk/libgsk/gl/color.glsl",                     "color" },
    { "/org/gtk/libgsk/gl/coloring.glsl",                  "coloring" },
    { "/org/gtk/libgsk/gl/cross_fade.glsl",                "cross fade" },
    { "/org/gtk/libgsk/gl/inset_shadow.glsl",              "inset shadow" },
    { "/org/gtk/libgsk/gl/linear_gradient.glsl",           "linear gradient" },
    { "/org/gtk/libgsk/gl/radial_gradient.glsl",           "radial gradient" },
    { "/org/gtk/libgsk/gl/conic_gradient.glsl",            "conic gradient" },
    { "/org/gtk/libgsk/gl/outset_shadow.glsl",             "outset shadow" },
    { "/org/gtk/libgsk/gl/repeat.glsl",                    "repeat" },
    { "/org/gtk/libgsk/gl/unblurred_outset_shadow.glsl",   "unblurred_outset shadow" },
  };

  gsk_gl_shader_builder_init (&shader_builder,
                              "/org/gtk/libgsk/gl/preamble.glsl",
                              "/org/gtk/libgsk/gl/preamble.vs.glsl",
                              "/org/gtk/libgsk/gl/preamble.fs.glsl");

  g_assert (G_N_ELEMENTS (program_definitions) == GL_N_PROGRAMS);

  init_shader_builder (self, &shader_builder);

  programs = gsk_gl_renderer_programs_new ();

  for (i = 0; i < GL_N_PROGRAMS; i ++)
    {
      Program *prog = &programs->programs[i];

      prog->name = program_definitions[i].name;
      prog->index = i;
      prog->id = gsk_gl_shader_builder_create_program (&shader_builder,
                                                       program_definitions[i].resource_path,
                                                       NULL, 0, error);
      if (prog->id < 0)
        {
          g_clear_pointer (&programs, gsk_gl_renderer_programs_unref);
          goto out;
        }

      INIT_COMMON_UNIFORM_LOCATION (prog, alpha);
      INIT_COMMON_UNIFORM_LOCATION (prog, source);
      INIT_COMMON_UNIFORM_LOCATION (prog, clip_rect);
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
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, num_color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, repeat);
  INIT_PROGRAM_UNIFORM_LOCATION (linear_gradient, points);

  /* radial gradient */
  INIT_PROGRAM_UNIFORM_LOCATION (radial_gradient, color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (radial_gradient, num_color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (radial_gradient, repeat);
  INIT_PROGRAM_UNIFORM_LOCATION (radial_gradient, geometry);
  INIT_PROGRAM_UNIFORM_LOCATION (radial_gradient, range);

  /* conic gradient */
  INIT_PROGRAM_UNIFORM_LOCATION (conic_gradient, color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (conic_gradient, num_color_stops);
  INIT_PROGRAM_UNIFORM_LOCATION (conic_gradient, geometry);

  /* blur */
  INIT_PROGRAM_UNIFORM_LOCATION (blur, blur_radius);
  INIT_PROGRAM_UNIFORM_LOCATION (blur, blur_size);
  INIT_PROGRAM_UNIFORM_LOCATION (blur, blur_dir);

  /* inset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, spread);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, offset);
  INIT_PROGRAM_UNIFORM_LOCATION (inset_shadow, outline_rect);

  /* outset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (outset_shadow, outline_rect);

  /* unblurred outset shadow */
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, color);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, spread);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, offset);
  INIT_PROGRAM_UNIFORM_LOCATION (unblurred_outset_shadow, outline_rect);

  /* border */
  INIT_PROGRAM_UNIFORM_LOCATION (border, color);
  INIT_PROGRAM_UNIFORM_LOCATION (border, widths);
  INIT_PROGRAM_UNIFORM_LOCATION (border, outline_rect);

  /* cross fade */
  INIT_PROGRAM_UNIFORM_LOCATION (cross_fade, progress);
  INIT_PROGRAM_UNIFORM_LOCATION (cross_fade, source2);

  /* blend */
  INIT_PROGRAM_UNIFORM_LOCATION (blend, source2);
  INIT_PROGRAM_UNIFORM_LOCATION (blend, mode);

  /* repeat */
  INIT_PROGRAM_UNIFORM_LOCATION (repeat, child_bounds);
  INIT_PROGRAM_UNIFORM_LOCATION (repeat, texture_rect);


  /* We initialize the alpha uniform here, since the default value is important.
   * We can't do it in the shader like a reasonable person would because that doesn't
   * work in gles. */
  for (i = 0; i < GL_N_PROGRAMS; i++)
    {
      glUseProgram(programs->programs[i].id);
      glUniform1f (programs->programs[i].alpha_location, 1.0);
    }

out:
  gsk_gl_shader_builder_finish (&shader_builder);

  /* Check we indeed emitted an error if there was one */
  g_assert (programs || !error || *error);

  return programs;
}

static GskGLRendererPrograms *
get_programs_for_display (GskGLRenderer  *self,
                          GdkDisplay     *display,
                          GError        **error)
{
  GskGLRendererPrograms *programs;

  if (g_getenv ("GSK_NO_SHARED_PROGRAMS"))
    return gsk_gl_renderer_create_programs (self, error);

  programs = (GskGLRendererPrograms *)g_object_get_data (G_OBJECT (display), "gsk-gl-programs");
  if (programs == NULL)
    {
      programs = gsk_gl_renderer_create_programs (self, error);
      if (programs)
        g_object_set_data_full (G_OBJECT (display), "gsk-gl-programs",
                                programs,
                                (GDestroyNotify) gsk_gl_renderer_programs_unref);
    }

  if (programs)
    return gsk_gl_renderer_programs_ref (programs);
  return NULL;
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
  gint64 before G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;
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
  self->programs = get_programs_for_display (self, gdk_surface_get_display (surface), error);
  if (self->programs == NULL)
    return FALSE;
  self->op_builder.programs = self->programs;

  self->atlases = get_texture_atlases_for_display (gdk_surface_get_display (surface));
  self->glyph_cache = get_glyph_cache_for_display (gdk_surface_get_display (surface), self->atlases);
  self->icon_cache = get_icon_cache_for_display (gdk_surface_get_display (surface), self->atlases);
  gsk_gl_shadow_cache_init (&self->shadow_cache);

  gdk_profiler_end_mark (before, "gl renderer realize", NULL);

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
  ops_reset (&self->op_builder);
  self->op_builder.programs = NULL;

  g_clear_pointer (&self->programs, gsk_gl_renderer_programs_unref);
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
  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  ops_reset (&self->op_builder);

#ifdef G_ENABLE_DEBUG
  int removed_textures = gsk_gl_driver_collect_textures (self->gl_driver);
  GSK_RENDERER_NOTE (GSK_RENDERER (self), OPENGL, g_message ("Collected: %d textures", removed_textures));
#else
  gsk_gl_driver_collect_textures (self->gl_driver);
#endif
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
gsk_gl_renderer_add_render_ops (GskGLRenderer   *self,
                                GskRenderNode   *node,
                                RenderOpBuilder *builder)
{
  /* This can still happen, even if the render nodes are created using
   * GtkSnapshot, so let's just be safe. */
  if (node_is_invisible (node))
    return;

  /* Check whether the render node is entirely out of the current
   * already transformed clip region */
  {
    graphene_rect_t transformed_node_bounds;

    ops_transform_bounds_modelview (builder,
                                    &node->bounds,
                                    &transformed_node_bounds);

    if (!graphene_rect_intersects (&builder->current_clip->bounds,
                                   &transformed_node_bounds))
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
      render_color_node (self, node, builder);
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
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      render_linear_gradient_node (self, node, builder);
    break;

    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
      render_radial_gradient_node (self, node, builder);
    break;

    case GSK_CONIC_GRADIENT_NODE:
      render_conic_gradient_node (self, node, builder);
    break;

    case GSK_CLIP_NODE:
      render_clip_node (self, node, builder);
    break;

    case GSK_ROUNDED_CLIP_NODE:
      render_rounded_clip_node (self, node, builder);
    break;

    case GSK_TEXT_NODE:
      render_text_node (self, node, builder,
                        gsk_text_node_get_color (node), FALSE);
    break;

    case GSK_COLOR_MATRIX_NODE:
      render_color_matrix_node (self, node, builder);
    break;

    case GSK_BLUR_NODE:
      render_blur_node (self, node, builder);
    break;

    case GSK_INSET_SHADOW_NODE:
      if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
        render_inset_shadow_node (self, node, builder);
      else
        render_unblurred_inset_shadow_node (self, node, builder);
    break;

    case GSK_OUTSET_SHADOW_NODE:
      if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
        render_outset_shadow_node (self, node, builder);
      else
        render_unblurred_outset_shadow_node (self, node, builder);
    break;

    case GSK_SHADOW_NODE:
      render_shadow_node (self, node, builder);
    break;

    case GSK_BORDER_NODE:
      render_border_node (self, node, builder);
    break;

    case GSK_CROSS_FADE_NODE:
      render_cross_fade_node (self, node, builder);
    break;

    case GSK_BLEND_NODE:
      render_blend_node (self, node, builder);
    break;

    case GSK_REPEAT_NODE:
      render_repeat_node (self, node, builder);
    break;

    case GSK_GL_SHADER_NODE:
      render_gl_shader_node (self, node, builder);
    break;

    case GSK_CAIRO_NODE:
    default:
      {
        render_fallback_node (self, node, builder);
      }
    }
}

static gboolean
add_offscreen_ops (GskGLRenderer         *self,
                   RenderOpBuilder       *builder,
                   const graphene_rect_t *bounds,
                   GskRenderNode         *child_node,
                   TextureRegion         *texture_region_out,
                   gboolean              *is_offscreen,
                   guint                  flags)
{
  const float dx = builder->dx;
  const float dy = builder->dy;
  float scaled_width, scaled_height;
  float scale_x;
  float scale_y;
  int render_target;
  int prev_render_target;
  graphene_matrix_t prev_projection;
  graphene_rect_t prev_viewport;
  graphene_matrix_t item_proj;
  float prev_opacity = 1.0;
  int texture_id = 0;
  int filter;
  GskTextureKey key;
  int cached_id;
  graphene_rect_t viewport;

  if (node_is_invisible (child_node))
    {
      /* Just to be safe */
      *is_offscreen = FALSE;
      init_full_texture_region (texture_region_out, 0);
      return FALSE;
    }

  /* We need the child node as a texture. If it already is one, we don't need to draw
   * it on a framebuffer of course. */
  if (gsk_render_node_get_node_type (child_node) == GSK_TEXTURE_NODE &&
      (flags & FORCE_OFFSCREEN) == 0)
    {
      GdkTexture *texture = gsk_texture_node_get_texture (child_node);
      upload_texture (self, texture, texture_region_out);
      *is_offscreen = FALSE;
      return TRUE;
    }

  if (flags & LINEAR_FILTER)
    filter = GL_LINEAR;
  else
    filter = GL_NEAREST;

  /* Check if we've already cached the drawn texture. */
  key.pointer = child_node;
  key.pointer_is_child = TRUE; /* Don't conflict with the child using the cache too */
  key.parent_rect = *bounds;
  key.scale_x = builder->scale_x;
  key.scale_y = builder->scale_y;
  key.filter = filter;
  cached_id = gsk_gl_driver_get_texture_for_key (self->gl_driver, &key);

  if (cached_id != 0)
    {
      init_full_texture_region (texture_region_out, cached_id);
      /* We didn't render it offscreen, but hand out an offscreen texture id */
      *is_offscreen = TRUE;
      return TRUE;
    }

  scale_x = builder->scale_x;
  scale_y = builder->scale_y;

  /* Tweak the scale factor so that the required texture doesn't
   * exceed the max texture limit. This will render with a lower
   * resolution, but this is better than clipping.
   */
  {
    const int max_texture_size = gsk_gl_driver_get_max_texture_size (self->gl_driver);

    scaled_width = ceilf (bounds->size.width * scale_x);
    if (scaled_width > max_texture_size)
      {
        scale_x *= (float)max_texture_size / scaled_width;
        scaled_width = max_texture_size;
      }

    scaled_height = ceilf (bounds->size.height * scale_y);
    if (scaled_height > max_texture_size)
      {
        scale_y *= (float)max_texture_size / scaled_height;
        scaled_height = max_texture_size;
      }
  }

  gsk_gl_driver_create_render_target (self->gl_driver,
                                      scaled_width, scaled_height,
                                      filter, filter,
                                      &texture_id, &render_target);
  if (gdk_gl_context_has_debug (self->gl_context))
    {
      gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, texture_id,
                                          "Offscreen<%s> %d",
                                          g_type_name_from_instance ((GTypeInstance *) child_node),
                                          texture_id);
      gdk_gl_context_label_object_printf (self->gl_context, GL_FRAMEBUFFER, render_target,
                                          "Offscreen<%s> FB %d",
                                          g_type_name_from_instance ((GTypeInstance *) child_node),
                                          render_target);
    }

  ops_transform_bounds_modelview (builder, bounds, &viewport);
  /* Code above will scale the size with the scale we use in the render ops,
   * but for the viewport size, we need our own size limited by the texture size */
  viewport.size.width = scaled_width;
  viewport.size.height = scaled_height;

  init_projection_matrix (&item_proj, &viewport);
  prev_render_target = ops_set_render_target (builder, render_target);
  /* Clear since we use this rendertarget for the first time */
  ops_begin (builder, OP_CLEAR);
  prev_projection = ops_set_projection (builder, &item_proj);
  ops_set_modelview (builder, gsk_transform_scale (NULL, scale_x, scale_y));
  prev_viewport = ops_set_viewport (builder, &viewport);
  if (flags & RESET_CLIP)
    ops_push_clip (builder, &GSK_ROUNDED_RECT_INIT_FROM_RECT (viewport));

  builder->dx = dx;
  builder->dy = dy;

  prev_opacity = ops_set_opacity (builder, 1.0);

  gsk_gl_renderer_add_render_ops (self, child_node, builder);

#ifdef G_ENABLE_DEBUG
  if (G_UNLIKELY (flags & DUMP_FRAMEBUFFER))
    {
      static int k;
      ops_dump_framebuffer (builder,
                            g_strdup_printf ("%s_%p_%d.png",
                                             g_type_name_from_instance ((GTypeInstance *) child_node),
                                             child_node,
                                             k ++),
                            scaled_width, scaled_height);
    }
#endif

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

  if ((flags & NO_CACHE_PLZ) == 0)
    gsk_gl_driver_set_texture_for_key (self->gl_driver, &key, texture_id);

  return TRUE;
}

static void
gsk_gl_renderer_render_ops (GskGLRenderer *self)
{
  const Program *program = NULL;
  const gsize vertex_data_size = self->op_builder.vertices->len * sizeof (GskQuadVertex);
  const float *vertex_data = (float *)self->op_builder.vertices->data;
  OpBufferIter iter;
  OpKind kind;
  gpointer ptr;
  GLuint buffer_id, vao_id;

#if DEBUG_OPS
  g_print ("============================================\n");
#endif

  glGenVertexArrays (1, &vao_id);
  glBindVertexArray (vao_id);

  glGenBuffers (1, &buffer_id);
  glBindBuffer (GL_ARRAY_BUFFER, buffer_id);

  glBufferData (GL_ARRAY_BUFFER, vertex_data_size, vertex_data, GL_STATIC_DRAW);

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
          apply_render_target_op (self, ptr);
          break;

        case OP_CLEAR:
          OP_PRINT ("-> CLEAR");
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

        case OP_CHANGE_EXTRA_SOURCE_TEXTURE:
          apply_source_extra_texture_op (program, ptr);
          break;

        case OP_CHANGE_CROSS_FADE:
          g_assert (program == &self->programs->cross_fade_program);
          apply_cross_fade_op (program, ptr);
          break;

        case OP_CHANGE_BLEND:
          g_assert (program == &self->programs->blend_program);
          apply_blend_op (program, ptr);
          break;

        case OP_CHANGE_LINEAR_GRADIENT:
          apply_linear_gradient_op (program, ptr);
          break;

        case OP_CHANGE_RADIAL_GRADIENT:
          apply_radial_gradient_op (program, ptr);
          break;

        case OP_CHANGE_CONIC_GRADIENT:
          apply_conic_gradient_op (program, ptr);
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

        case OP_CHANGE_GL_SHADER_ARGS:
          apply_gl_shader_args_op (program, ptr);
          break;

        case OP_DRAW:
          {
            const OpDraw *op = ptr;

            OP_PRINT (" -> draw %ld, size %ld and program %d: %s",
                      op->vao_offset, op->vao_size, program->index,
                      program->name ?: "");
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
            OP_PRINT (" Debug: %s", op->text);
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
  gint64 gpu_time, cpu_time;
  gint64 start_time G_GNUC_UNUSED;
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

  removed = g_ptr_array_new ();
  gsk_gl_texture_atlases_begin_frame (self->atlases, removed);
  gsk_gl_glyph_cache_begin_frame (self->glyph_cache, self->gl_driver, removed);
  gsk_gl_icon_cache_begin_frame (self->icon_cache, removed);
  gsk_gl_shadow_cache_begin_frame (&self->shadow_cache, self->gl_driver);
  g_ptr_array_unref (removed);

  /* Set up the modelview and projection matrices to fit our viewport */
  init_projection_matrix (&projection, viewport);
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

  gdk_profiler_add_mark (start_time * 1000, cpu_time * 1000, "GL render", "");
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
                                          "Render %s<%p> to texture",
                                          g_type_name_from_instance ((GTypeInstance *) root),
                                          root);

  width = ceilf (viewport->size.width);
  height = ceilf (viewport->size.height);

  self->scale_factor = gdk_surface_get_scale_factor (gsk_renderer_get_surface (renderer));

  /* Prepare our framebuffer */
  gsk_gl_driver_begin_frame (self->gl_driver);
  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  if (gdk_gl_context_has_debug (self->gl_context))
    gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, texture_id,
                                        "Texture %s<%p> %d",
                                        g_type_name_from_instance ((GTypeInstance *) root),
                                        root,
                                        texture_id);

  if (gdk_gl_context_get_use_es (self->gl_context))
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  glGenFramebuffers (1, &fbo_id);
  glBindFramebuffer (GL_FRAMEBUFFER, fbo_id);

  if (gdk_gl_context_has_debug (self->gl_context))
    gdk_gl_context_label_object_printf (self->gl_context, GL_FRAMEBUFFER, fbo_id,
                                        "FB %s<%p> %d",
                                        g_type_name_from_instance ((GTypeInstance *) root),
                                        root,
                                        fbo_id);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

  /* Render the actual scene */
  gsk_gl_renderer_do_render (renderer, root, viewport, fbo_id, 1);

  glDeleteFramebuffers (1, &fbo_id);


  /* Render the now drawn framebuffer y-flipped so it's as GdkGLTexture expects it to be */
  {
    guint final_texture_id, final_fbo_id;
    graphene_matrix_t m;

    ops_reset (&self->op_builder);

    glGenFramebuffers (1, &final_fbo_id);
    glBindFramebuffer (GL_FRAMEBUFFER, final_fbo_id);
    glGenTextures (1, &final_texture_id);
    glBindTexture (GL_TEXTURE_2D, final_texture_id);

    if (gdk_gl_context_get_use_es (self->gl_context))
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    else
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture_id, 0);
    g_assert_cmphex (glCheckFramebufferStatus (GL_FRAMEBUFFER), ==, GL_FRAMEBUFFER_COMPLETE);

    ops_set_render_target (&self->op_builder, final_fbo_id);
    ops_push_clip (&self->op_builder, &GSK_ROUNDED_RECT_INIT (0, 0, width, height));
    ops_set_program (&self->op_builder, &self->programs->blit_program);

    ops_begin (&self->op_builder, OP_CLEAR);
    ops_set_texture (&self->op_builder, texture_id);
    ops_set_modelview (&self->op_builder, NULL);
    ops_set_viewport (&self->op_builder, &GRAPHENE_RECT_INIT (0, 0, width, height));
    init_projection_matrix (&m, &GRAPHENE_RECT_INIT (0, 0, width, height));
    graphene_matrix_scale (&m, 1, -1, 1); /* Undo the scale init_projection_matrix() does again */
    ops_set_projection (&self->op_builder, &m);

    fill_vertex_data (ops_draw (&self->op_builder, NULL),
                      0, 0, width, height);
    ops_pop_clip (&self->op_builder);
    gsk_gl_renderer_render_ops (self);

    ops_finish (&self->op_builder);

    glDeleteTextures (1, &texture_id);

    texture_id = final_texture_id;
  }

  texture = gdk_gl_texture_new (self->gl_context,
                                texture_id,
                                width, height,
                                NULL, NULL);

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

  surface = gsk_renderer_get_surface (renderer);
  self->scale_factor = gdk_surface_get_scale_factor (surface);

  gdk_gl_context_make_current (self->gl_context);
  gdk_gl_context_push_debug_group_printf (self->gl_context,
                                          "Render root node %p", root);

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

  gdk_gl_context_make_current (self->gl_context);

  viewport.origin.x = 0;
  viewport.origin.y = 0;
  viewport.size.width = whole_surface.width;
  viewport.size.height = whole_surface.height;

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
