#include "config.h"

#include "gskglrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
#include "gskgldriverprivate.h"
#include "gskglprofilerprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrendernodeiter.h"
#include "gskshaderbuilderprivate.h"
#include "gsktextureprivate.h"

#include "gskprivate.h"

#include <epoxy/gl.h>

#define SHADER_VERSION_GLES             100
#define SHADER_VERSION_GL2_LEGACY       110
#define SHADER_VERSION_GL3_LEGACY       130
#define SHADER_VERSION_GL3              150

typedef struct {
  int render_target_id;
  int vao_id;
  int buffer_id;
  int texture_id;
  int program_id;

  int mvp_location;
  int source_location;
  int mask_location;
  int uv_location;
  int position_location;
  int alpha_location;
  int blendMode_location;
} RenderData;

typedef struct {
  /* Back pointer to the node, only meant for comparison */
  GskRenderNode *node;

  graphene_point3d_t min;
  graphene_point3d_t max;

  graphene_size_t size;

  graphene_matrix_t mvp;

  gboolean opaque : 1;
  float opacity;
  float z;

  const char *name;

  GskBlendMode blend_mode;

  RenderData render_data;
  RenderData *parent_data;

  GArray *children;
} RenderItem;

enum {
  MVP,
  SOURCE,
  MASK,
  ALPHA,
  BLEND_MODE,
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

typedef struct _GskGLTexture GskGLTexture;
struct _GskGLTexture {
  GskTexture texture;

  int texture_id;
};

struct _GskGLRenderer
{
  GskRenderer parent_instance;

  graphene_matrix_t mvp;
  graphene_frustum_t frustum;

  guint frame_buffer;
  guint depth_stencil_buffer;
  guint texture_id;

  GQuark uniforms[N_UNIFORMS];
  GQuark attributes[N_ATTRIBUTES];

  GdkGLContext *gl_context;
  GskGLDriver *gl_driver;
  GskGLProfiler *gl_profiler;
  GskShaderBuilder *shader_builder;

  int blend_program_id;
  int blit_program_id;

  GArray *render_items;

#ifdef G_ENABLE_DEBUG
  ProfileCounters profile_counters;
  ProfileTimers profile_timers;
#endif

  gboolean has_buffers : 1;
};

struct _GskGLRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_RENDERER)

static GskTexture *
gsk_gl_renderer_texture_new_for_data (GskRenderer  *renderer,
                                      const guchar *data,
                                      int           width,
                                      int           height,
                                      int           stride)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  GskGLTexture *texture;
  cairo_surface_t *surface;

  gdk_gl_context_make_current (self->gl_context);
  gsk_gl_driver_begin_frame (self->gl_driver);

  /* XXX: Make this work without having to create cairo surfaces */
  surface = cairo_image_surface_create_for_data ((guchar *) data, CAIRO_FORMAT_ARGB32, width, height, stride);

  texture = gsk_texture_new (GskGLTexture, renderer, width, height);

  texture->texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                      TRUE,
                                                      width, height);
  gsk_gl_driver_bind_source_texture (self->gl_driver, texture->texture_id);
  gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                           texture->texture_id,
                                           surface,
                                           GL_NEAREST,
                                           GL_NEAREST);

  cairo_surface_destroy (surface);

  gsk_gl_driver_end_frame (self->gl_driver);

  return &texture->texture;
}

static GskTexture *
gsk_gl_renderer_texture_new_for_pixbuf (GskRenderer *renderer,
                                        GdkPixbuf   *pixbuf)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  GskGLTexture *texture;
  cairo_surface_t *surface;
  int width, height;

  gdk_gl_context_make_current (self->gl_context);
  gsk_gl_driver_begin_frame (self->gl_driver);

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  texture = gsk_texture_new (GskGLTexture, renderer, width, height);

  texture->texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                      TRUE,
                                                      width, height);
  gsk_gl_driver_bind_source_texture (self->gl_driver, texture->texture_id);
  gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                           texture->texture_id,
                                           surface,
                                           GL_NEAREST,
                                           GL_NEAREST);

  gsk_gl_driver_end_frame (self->gl_driver);

  cairo_surface_destroy (surface);

  return &texture->texture;
}

static void
gsk_gl_renderer_texture_destroy (GskTexture *texture)
{
  GskGLTexture *gltexture = (GskGLTexture *) texture;
  GskGLRenderer *self = GSK_GL_RENDERER (gsk_texture_get_renderer (texture));

  gsk_gl_driver_release_texture (self->gl_driver, gltexture->texture_id);
}

static void
gsk_gl_renderer_dispose (GObject *gobject)
{
  GskGLRenderer *self = GSK_GL_RENDERER (gobject);

  g_clear_object (&self->gl_context);

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
                                                       FALSE,
                                                       width * scale_factor,
                                                       height * scale_factor);
      gsk_gl_driver_bind_source_texture (self->gl_driver, self->texture_id);
      gsk_gl_driver_init_texture_empty (self->gl_driver, self->texture_id);
    }

  gsk_gl_driver_create_render_target (self->gl_driver, self->texture_id, TRUE, TRUE);

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
gsk_gl_renderer_create_programs (GskGLRenderer *self)
{
  GskShaderBuilder *builder;
  GError *error = NULL;
  gboolean res = FALSE;

  builder = gsk_shader_builder_new ();

  gsk_shader_builder_set_resource_base_path (builder, "/org/gtk/libgsk/glsl");

  self->uniforms[MVP] = gsk_shader_builder_add_uniform (builder, "uMVP");
  self->uniforms[SOURCE] = gsk_shader_builder_add_uniform (builder, "uSource");
  self->uniforms[MASK] = gsk_shader_builder_add_uniform (builder, "uMask");
  self->uniforms[ALPHA] = gsk_shader_builder_add_uniform (builder, "uAlpha");
  self->uniforms[BLEND_MODE] = gsk_shader_builder_add_uniform (builder, "uBlendMode");
  
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

  self->blend_program_id =
    gsk_shader_builder_create_program (builder, "blend.vs.glsl", "blend.fs.glsl", &error);
  if (error != NULL)
    {
      g_critical ("Unable to create 'blend' program: %s", error->message);
      g_error_free (error);
      g_object_unref (builder);
      goto out;
    }

  self->blit_program_id =
    gsk_shader_builder_create_program (builder, "blit.vs.glsl", "blit.fs.glsl", &error);
  if (error != NULL)
    {
      g_critical ("Unable to create 'blit' program: %s", error->message);
      g_error_free (error);
      g_object_unref (builder);
      goto out;
    }

  /* Keep a pointer to query for the uniform and attribute locations
   * when rendering the scene
   */
  self->shader_builder = builder;

  res = TRUE;

out:
  return res;
}

static void
gsk_gl_renderer_destroy_programs (GskGLRenderer *self)
{
  g_clear_object (&self->shader_builder);
}

static gboolean
gsk_gl_renderer_realize (GskRenderer *renderer)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  GError *error = NULL;

  /* If we didn't get a GdkGLContext before realization, try creating
   * one now, for our exclusive use.
   */
  if (self->gl_context == NULL)
    {
      GdkWindow *window = gsk_renderer_get_window (renderer);

      if (window == NULL)
        return FALSE;

      self->gl_context = gdk_window_create_gl_context (window, &error);
      if (error != NULL)
        {
          g_critical ("Unable to create GL context for renderer: %s",
                      error->message);
          g_error_free (error);

          return FALSE;
        }
    }

  gdk_gl_context_realize (self->gl_context, &error);
  if (error != NULL)
    {
      g_critical ("Unable to realize GL renderer: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  gdk_gl_context_make_current (self->gl_context);

  g_assert (self->gl_driver == NULL);
  self->gl_driver = gsk_gl_driver_new (self->gl_context);
  self->gl_profiler = gsk_gl_profiler_new (self->gl_context);

  GSK_NOTE (OPENGL, g_print ("Creating buffers and programs\n"));
  if (!gsk_gl_renderer_create_programs (self))
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
  g_clear_pointer (&self->render_items, g_array_unref);

  gsk_gl_renderer_destroy_buffers (self);
  gsk_gl_renderer_destroy_programs (self);

  g_clear_object (&self->gl_profiler);
  g_clear_object (&self->gl_driver);

  if (self->gl_context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();
}

static void
gsk_gl_renderer_resize_viewport (GskGLRenderer         *self,
                                 const graphene_rect_t *viewport,
                                 int                    scale_factor)
{
  int width = viewport->size.width * scale_factor;
  int height = viewport->size.height * scale_factor;

  GSK_NOTE (OPENGL, g_print ("glViewport(0, 0, %d, %d) [scale:%d]\n",
                             width,
                             height,
                             scale_factor));

  glViewport (0, 0, width, height);
}

static void
gsk_gl_renderer_update_frustum (GskGLRenderer           *self,
                                const graphene_matrix_t *modelview,
                                const graphene_matrix_t *projection)
{
  GSK_NOTE (TRANSFORMS, g_print ("Updating the modelview/projection\n"));

  graphene_matrix_multiply (modelview, projection, &self->mvp);

  graphene_frustum_init_from_matrix (&self->frustum, &self->mvp);

  GSK_NOTE (TRANSFORMS,
            g_print ("Renderer MVP:\n");
            graphene_matrix_print (&self->mvp);
            g_print ("\n"));
}

#define N_VERTICES      6

static inline int
node_depth (GskRenderNode *node)
{
  int ret = 0;
  while (node->parent)
    {
      ret++;
      node = node->parent;
    }
  return ret;
}

static void
render_item (GskGLRenderer *self,
             RenderItem    *item)
{
  float mvp[16];
  float opacity;

  if (item->children != NULL)
    {
      if (gsk_gl_driver_bind_render_target (self->gl_driver, item->render_data.render_target_id))
        {
          glViewport (0, 0, item->size.width, item->size.height);

          glClearColor (0.0, 0.0, 0.0, 0.0);
          glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
    }

  gsk_gl_driver_bind_vao (self->gl_driver, item->render_data.vao_id);

  glUseProgram (item->render_data.program_id);

  if (item->render_data.texture_id != 0)
    {
      /* Use texture unit 0 for the source */
      glUniform1i (item->render_data.source_location, 0);
      gsk_gl_driver_bind_source_texture (self->gl_driver, item->render_data.texture_id);

      if (item->parent_data != NULL)
        {
          glUniform1i (item->render_data.blendMode_location, item->blend_mode);

          /* Use texture unit 1 for the mask */
          if (item->parent_data->texture_id != 0)
            {
              glUniform1i (item->render_data.mask_location, 1);
              gsk_gl_driver_bind_mask_texture (self->gl_driver, item->parent_data->texture_id);
            }
        }
    }

  /* Pass the opacity component */
  if (item->children != NULL || item->opaque)
    opacity = 1.0;
  else
    opacity = item->opacity;

  glUniform1f (item->render_data.alpha_location, opacity);

  /* Pass the mvp to the vertex shader */
  GSK_NOTE (TRANSFORMS, graphene_matrix_print (&item->mvp));
  graphene_matrix_to_float (&item->mvp, mvp);
  glUniformMatrix4fv (item->render_data.mvp_location, 1, GL_FALSE, mvp);

  /* Draw the quad */
  GSK_NOTE2 (OPENGL, TRANSFORMS,
             g_print ("%*sDrawing item <%s>[%p] (w:%g, h:%g) with opacity: %g blend mode: %d\n",
                      2 * node_depth (item->node), "",
                      item->name,
                      item,
                      item->size.width, item->size.height,
                      item->opaque ? 1 : item->opacity,
                      item->blend_mode));

  glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (gsk_renderer_get_profiler (GSK_RENDERER (self)),
                            self->profile_counters.draw_calls);
#endif

  /* Render all children items, so we can take the result
   * render target texture during the compositing
   */
  if (item->children != NULL)
    {
      int i;

      for (i = 0; i < item->children->len; i++)
        {
          RenderItem *child = &g_array_index (item->children, RenderItem, i);

          render_item (self, child);
        }

      /* Bind the parent render target */
      if (item->parent_data != NULL)
        gsk_gl_driver_bind_render_target (self->gl_driver, item->parent_data->render_target_id);

      /* Bind the same VAO, as the render target is created with the same size
       * and vertices as the texture target
       */
      gsk_gl_driver_bind_vao (self->gl_driver, item->render_data.vao_id);

      /* Since we're rendering the target texture, we only need the blit program */
      glUseProgram (self->blit_program_id);

      /* Use texture unit 0 for the render target */
      glUniform1i (item->render_data.source_location, 0);
      gsk_gl_driver_bind_source_texture (self->gl_driver, item->render_data.render_target_id);

      /* Pass the opacity component; if we got here, we know that the original render
       * target is neither fully opaque nor at full opacity
       */
      glUniform1f (item->render_data.alpha_location, item->opacity);

      /* Pass the mvp to the vertex shader */
      GSK_NOTE (TRANSFORMS, graphene_matrix_print (&item->mvp));
      graphene_matrix_to_float (&item->mvp, mvp);
      glUniformMatrix4fv (item->render_data.mvp_location, 1, GL_FALSE, mvp);

      /* Draw the quad */
      GSK_NOTE2 (OPENGL, TRANSFORMS,
                 g_print ("%*sDrawing offscreen item <%s>[%p] (w:%g, h:%g) with opacity: %g\n",
                          2 * node_depth (item->node), "",
                          item->name,
                          item,
                          item->size.width, item->size.height,
                          item->opacity));

      glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);
    }
}

static void
get_gl_scaling_filters (GskRenderNode *node,
                        int           *min_filter_r,
                        int           *mag_filter_r)
{
  switch (node->min_filter)
    {
    case GSK_SCALING_FILTER_NEAREST:
      *min_filter_r = GL_NEAREST;
      break;

    case GSK_SCALING_FILTER_LINEAR:
      *min_filter_r = GL_LINEAR;
      break;

    case GSK_SCALING_FILTER_TRILINEAR:
      *min_filter_r = GL_LINEAR_MIPMAP_LINEAR;
      break;
    }

  switch (node->mag_filter)
    {
    case GSK_SCALING_FILTER_NEAREST:
      *mag_filter_r = GL_NEAREST;
      break;

    /* There's no point in using anything above GL_LINEAR for
     * magnification filters
     */
    case GSK_SCALING_FILTER_LINEAR:
    case GSK_SCALING_FILTER_TRILINEAR:
      *mag_filter_r = GL_LINEAR;
      break;
    }
}

#if 0
static gboolean
check_in_frustum (const graphene_frustum_t *frustum,
                  RenderItem               *item)
{
  graphene_box_t aabb;

  graphene_box_init (&aabb, &item->min, &item->max);
  graphene_matrix_transform_box (&item->mvp, &aabb, &aabb);

  return graphene_frustum_intersects_box (frustum, &aabb);
}
#endif

static float
project_item (const graphene_matrix_t *projection,
              const graphene_matrix_t *modelview)
{
  graphene_vec4_t vec;

  graphene_matrix_get_row (modelview, 3, &vec);
  graphene_matrix_transform_vec4 (projection, &vec, &vec);

  return graphene_vec4_get_z (&vec) / graphene_vec4_get_w (&vec);
}

static gboolean
render_node_needs_render_target (GskRenderNode *node)
{
  if (!gsk_render_node_is_opaque (node))
    {
      double opacity = gsk_render_node_get_opacity (node);

      if (opacity < 1.0)
        return TRUE;
    }

  return FALSE;
}

static void
gsk_gl_renderer_add_render_item (GskGLRenderer           *self,
                                 const graphene_matrix_t *projection,
                                 GArray                  *render_items,
                                 GskRenderNode           *node,
                                 RenderItem              *parent)
{
  graphene_rect_t viewport;
  GskRenderNodeIter iter;
  graphene_matrix_t mv;
  graphene_rect_t bounds;
  GskRenderNode *child;
  RenderItem item;
  RenderItem *ritem = NULL;
  int program_id;
  int scale_factor;

  if (gsk_render_node_is_hidden (node))
    {
      GSK_NOTE (OPENGL, g_print ("Skipping hidden node <%s>[%p]\n",
                                 node->name != NULL ? node->name : "unnamed",
                                 node));
      return;
    }

  memset (&item, 0, sizeof (RenderItem));

  gsk_renderer_get_viewport (GSK_RENDERER (self), &viewport);

  scale_factor = gsk_renderer_get_scale_factor (GSK_RENDERER (self));
  if (scale_factor < 1)
    scale_factor = 1;

  gsk_render_node_get_bounds (node, &bounds);

  item.node = node;
  item.name = node->name != NULL ? node->name : "unnamed";

  /* The texture size */
  item.size.width = bounds.size.width * scale_factor;
  item.size.height = bounds.size.height * scale_factor;

  /* Each render item is an axis-aligned bounding box that we
   * transform using the given transformation matrix
   */
  item.min.x = bounds.origin.x;
  item.min.y = bounds.origin.y;
  item.min.z = 0.f;

  item.max.x = item.min.x + bounds.size.width;
  item.max.y = item.min.y + bounds.size.height;
  item.max.z = 0.f;

  /* The location of the item, in normalized world coordinates */
  gsk_render_node_get_world_matrix (node, &mv);
  graphene_matrix_multiply (&mv, &self->mvp, &item.mvp);
  item.z = project_item (projection, &mv);

  item.opaque = gsk_render_node_is_opaque (node);
  item.opacity = gsk_render_node_get_opacity (node);

  item.blend_mode = gsk_render_node_get_blend_mode (node);

  /* Back-pointer to the parent node */
  if (parent != NULL)
    item.parent_data = &(parent->render_data);
  else
    item.parent_data = NULL;

  /* Select the program to use */
  if (parent != NULL)
    program_id = self->blend_program_id;
  else
    program_id = self->blit_program_id;

  item.render_data.program_id = program_id;

  /* Retrieve all the uniforms and attributes */
  item.render_data.source_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[SOURCE]);
  item.render_data.mask_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[MASK]);
  item.render_data.mvp_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[MVP]);
  item.render_data.alpha_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[ALPHA]);
  item.render_data.blendMode_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[BLEND_MODE]);

  item.render_data.position_location =
    gsk_shader_builder_get_attribute_location (self->shader_builder, program_id, self->attributes[POSITION]);
  item.render_data.uv_location =
    gsk_shader_builder_get_attribute_location (self->shader_builder, program_id, self->attributes[UV]);

  if (render_node_needs_render_target (node))
    {
      item.render_data.render_target_id =
        gsk_gl_driver_create_texture (self->gl_driver, FALSE, item.size.width, item.size.height);
      gsk_gl_driver_init_texture_empty (self->gl_driver, item.render_data.render_target_id);
      gsk_gl_driver_create_render_target (self->gl_driver, item.render_data.render_target_id, TRUE, TRUE);

      item.children = g_array_sized_new (FALSE, FALSE, sizeof (RenderItem),
                                         gsk_render_node_get_n_children (node));
    }
  else
    {
      item.render_data.render_target_id = self->texture_id;
      item.children = NULL;
    }

  if (gsk_render_node_has_texture (node))
    {
      GskTexture *texture = gsk_render_node_get_texture (node);

      if (gsk_texture_get_renderer (texture) != GSK_RENDERER (self))
        {
          g_warning ("Given Texture belongs to wrong renderer, ignoring.");
          goto out;
        }
      item.render_data.texture_id = ((GskGLTexture *) texture)->texture_id;
    }
  else if (gsk_render_node_has_surface (node))
    {
      cairo_surface_t *surface = gsk_render_node_get_surface (node);
      int gl_min_filter = GL_NEAREST, gl_mag_filter = GL_NEAREST;

      get_gl_scaling_filters (node, &gl_min_filter, &gl_mag_filter);

      /* Upload the Cairo surface to a GL texture */
      item.render_data.texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                                  FALSE,
                                                                  item.size.width,
                                                                  item.size.height);
      gsk_gl_driver_bind_source_texture (self->gl_driver, item.render_data.texture_id);
      gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                               item.render_data.texture_id,
                                               surface,
                                               gl_min_filter,
                                               gl_mag_filter);
    }
  else
    {
      /* If the node does not draw anything, we skip it */
      if (item.render_data.render_target_id == self->texture_id)
        goto out;
    }

  /* Create the vertex buffers holding the geometry of the quad */
  {
    GskQuadVertex vertex_data[N_VERTICES] = {
      { { item.min.x, item.min.y }, { 0, 0 }, },
      { { item.min.x, item.max.y }, { 0, 1 }, },
      { { item.max.x, item.min.y }, { 1, 0 }, },

      { { item.max.x, item.max.y }, { 1, 1 }, },
      { { item.min.x, item.max.y }, { 0, 1 }, },
      { { item.max.x, item.min.y }, { 1, 0 }, },
    };

    item.render_data.vao_id =
      gsk_gl_driver_create_vao_for_quad (self->gl_driver,
                                         item.render_data.position_location,
                                         item.render_data.uv_location,
                                         N_VERTICES,
                                         vertex_data);
  }

  GSK_NOTE (OPENGL, g_print ("%*sAdding node <%s>[%p] to render items\n",
                             2 * node_depth (node), "",
                             node->name != NULL ? node->name : "unnamed",
                             node));
  g_array_append_val (render_items, item);
  ritem = &g_array_index (render_items, RenderItem, render_items->len - 1);

  if (item.children != NULL)
    render_items = item.children;

out:
  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, &child))
    gsk_gl_renderer_add_render_item (self, projection, render_items, child, ritem);
}

static gboolean
gsk_gl_renderer_validate_tree (GskGLRenderer           *self,
                               GskRenderNode           *root,
                               const graphene_matrix_t *projection)
{
  int n_nodes;

  if (self->gl_context == NULL)
    {
      GSK_NOTE (OPENGL, g_print ("No valid GL context associated to the renderer"));
      return FALSE;
    }

  n_nodes = gsk_render_node_get_size (root);

  gdk_gl_context_make_current (self->gl_context);

  self->render_items = g_array_sized_new (FALSE, FALSE, sizeof (RenderItem), n_nodes);

  gsk_gl_driver_begin_frame (self->gl_driver);

  GSK_NOTE (OPENGL, g_print ("RenderNode -> RenderItem\n"));
  gsk_gl_renderer_add_render_item (self, projection, self->render_items, root, NULL);

  GSK_NOTE (OPENGL, g_print ("Total render items: %d of max:%d\n",
                             self->render_items->len,
                             n_nodes));

  gsk_gl_driver_end_frame (self->gl_driver);

  return TRUE;
}

static void
gsk_gl_renderer_clear_tree (GskGLRenderer *self)
{
  int removed_textures, removed_vaos;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  g_clear_pointer (&self->render_items, g_array_unref);

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

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

static void
gsk_gl_renderer_render (GskRenderer   *renderer,
                        GskRenderNode *root)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_matrix_t modelview, projection;
  graphene_rect_t viewport;
  guint i;
  int scale_factor;
  GdkDrawingContext *context;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 gpu_time, cpu_time;
#endif

  if (self->gl_context == NULL)
    return;

  context = gsk_renderer_get_drawing_context (renderer);

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
#endif

  gdk_gl_context_make_current (self->gl_context);

  gsk_renderer_get_viewport (renderer, &viewport);
  scale_factor = gsk_renderer_get_scale_factor (renderer);

  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_renderer_create_buffers (self, viewport.size.width, viewport.size.height, scale_factor);
  gsk_gl_driver_end_frame (self->gl_driver);

  /* Set up the modelview and projection matrices to fit our viewport */
  graphene_matrix_init_scale (&modelview, scale_factor, scale_factor, 1.0);
  graphene_matrix_init_ortho (&projection,
                              0, viewport.size.width * scale_factor,
                              viewport.size.height * scale_factor, 0,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);

  gsk_gl_renderer_update_frustum (self, &modelview, &projection);

  if (!gsk_gl_renderer_validate_tree (self, root, &projection))
    goto out;

  gsk_gl_driver_begin_frame (self->gl_driver);

#ifdef G_ENABLE_DEBUG
  gsk_gl_profiler_begin_gpu_region (self->gl_profiler);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  /* Ensure that the viewport is up to date */
  if (gsk_gl_driver_bind_render_target (self->gl_driver, self->texture_id))
    gsk_gl_renderer_resize_viewport (self, &viewport, scale_factor);

  gsk_gl_renderer_clear (self);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  GSK_NOTE (OPENGL, g_print ("Rendering %u items\n", self->render_items->len));
  for (i = 0; i < self->render_items->len; i++)
    {
      RenderItem *item = &g_array_index (self->render_items, RenderItem, i);

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

out:
  {
    GdkWindow *window;
    cairo_t *cr;

    if (context != NULL)
      {
        /* XXX: Add GdkDrawingContext API */
        cr = gdk_drawing_context_get_cairo_context (context);
        window = gdk_drawing_context_get_window (context);
      }
    else
      {
        cr = gsk_renderer_get_cairo_context (renderer);
        window = gsk_renderer_get_window (renderer);
      }

    gdk_cairo_draw_from_gl (cr, window,
                            self->texture_id,
                            GL_TEXTURE,
                            scale_factor,
                            0, 0,
                            viewport.size.width * scale_factor,
                            viewport.size.height * scale_factor);
  }

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
  renderer_class->render = gsk_gl_renderer_render;

  renderer_class->texture_new_for_data = gsk_gl_renderer_texture_new_for_data;
  renderer_class->texture_new_for_pixbuf = gsk_gl_renderer_texture_new_for_pixbuf;
  renderer_class->texture_destroy = gsk_gl_renderer_texture_destroy;
}

static void
gsk_gl_renderer_init (GskGLRenderer *self)
{
  gsk_ensure_resources ();

  graphene_matrix_init_identity (&self->mvp);

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
