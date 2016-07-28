#include "config.h"

#include "gskglrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
#include "gskgldriverprivate.h"
#include "gskglprofilerprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrendernodeiter.h"
#include "gskshaderbuilderprivate.h"

#include "gskprivate.h"

#include <epoxy/gl.h>

#define SHADER_VERSION_GLES             110
#define SHADER_VERSION_GL_LEGACY        120
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

  int gl_min_filter;
  int gl_mag_filter;

  int blend_program_id;
  int blit_program_id;

  GArray *render_items;

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
      gsk_shader_builder_set_version (builder, SHADER_VERSION_GL_LEGACY);
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
  GSK_NOTE (OPENGL, g_print ("Updating the modelview/projection\n"));

  graphene_matrix_multiply (projection, modelview, &self->mvp);

  graphene_frustum_init_from_matrix (&self->frustum, &self->mvp);

  GSK_NOTE (OPENGL, g_print ("Renderer MVP:\n"));
  GSK_NOTE (OPENGL, graphene_matrix_print (&self->mvp));
}

#define N_VERTICES      6

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

  /* Pass the opacity component */
  if (item->children != NULL || item->opaque)
    opacity = 1.0;
  else
    opacity = item->opacity;

  glUniform1f (item->render_data.alpha_location, opacity);

  /* Pass the mvp to the vertex shader */
  GSK_NOTE (OPENGL, graphene_matrix_print (&item->mvp));
  graphene_matrix_to_float (&item->mvp, mvp);
  glUniformMatrix4fv (item->render_data.mvp_location, 1, GL_FALSE, mvp);

  /* Draw the quad */
  GSK_NOTE (OPENGL, g_print ("Drawing item <%s>[%p] (w:%g, h:%g) with opacity: %g\n",
                             item->name,
                             item,
                             item->size.width, item->size.height,
                             item->opaque ? 1 : item->opacity));

  glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);

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
      GSK_NOTE (OPENGL, graphene_matrix_print (&item->mvp));
      graphene_matrix_to_float (&item->mvp, mvp);
      glUniformMatrix4fv (item->render_data.mvp_location, 1, GL_FALSE, mvp);

      /* Draw the quad */
      GSK_NOTE (OPENGL, g_print ("Drawing offscreen item <%s>[%p] (w:%g, h:%g) with opacity: %g\n",
                                 item->name,
                                 item,
                                 item->size.width, item->size.height,
                                 item->opacity));

      glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);
    }
}

static void
get_gl_scaling_filters (GskRenderer *renderer,
                        int         *min_filter_r,
                        int         *mag_filter_r)
{
  GskScalingFilter min_filter, mag_filter;

  gsk_renderer_get_scaling_filters (renderer, &min_filter, &mag_filter);

  switch (min_filter)
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

  switch (mag_filter)
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
gsk_gl_renderer_add_render_item (GskGLRenderer *self,
                                 GArray        *render_items,
                                 GskRenderNode *node,
                                 RenderItem    *parent)
{
  graphene_rect_t viewport;
  cairo_surface_t *surface;
  GskRenderNodeIter iter;
  graphene_matrix_t mv, projection;
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

  scale_factor = gsk_render_node_get_scale_factor (node);
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
  item.min.x = bounds.origin.x * scale_factor;
  item.min.y = bounds.origin.y * scale_factor;
  item.min.z = 0.f;

  item.max.x = item.min.x + item.size.width;
  item.max.y = item.min.y + item.size.height;
  item.max.z = 0.f;

  /* The location of the item, in normalized world coordinates */
  gsk_render_node_get_world_matrix (node, &mv);
  graphene_matrix_multiply (&mv, &self->mvp, &item.mvp);

  item.opaque = gsk_render_node_is_opaque (node);
  item.opacity = gsk_render_node_get_opacity (node);

  item.blend_mode = gsk_render_node_get_blend_mode (node);

  /* Back-pointer to the parent node */
  if (parent != NULL)
    item.parent_data = &(parent->render_data);
  else
    item.parent_data = NULL;

  /* Select the render target; -1 is the default */
  if (render_node_needs_render_target (node))
    {
      item.render_data.render_target_id =
        gsk_gl_driver_create_texture (self->gl_driver, item.size.width, item.size.height);
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
                                         sizeof (GskQuadVertex) * N_VERTICES,
                                         vertex_data);
  }

  gsk_renderer_get_projection (GSK_RENDERER (self), &projection);
  item.z = project_item (&projection, &mv);

  /* TODO: This should really be an asset atlas, to avoid uploading a ton
   * of textures. Ideally we could use a single Cairo surface to get around
   * the GL texture limits and reorder the texture data on the CPU side and
   * do a single upload; alternatively, we could use a separate FBO and
   * render each texture into it
   */
  surface = gsk_render_node_get_surface (node);

  if (surface != NULL)
    {
      /* Upload the Cairo surface to a GL texture */
      item.render_data.texture_id = gsk_gl_driver_create_texture (self->gl_driver,
                                                                  item.size.width,
                                                                  item.size.height);
      gsk_gl_driver_bind_source_texture (self->gl_driver, item.render_data.texture_id);
      gsk_gl_driver_init_texture_with_surface (self->gl_driver,
                                               item.render_data.texture_id,
                                               surface,
                                               self->gl_min_filter,
                                               self->gl_mag_filter);
    }

  GSK_NOTE (OPENGL, g_print ("Adding node <%s>[%p] to render items\n",
                             node->name != NULL ? node->name : "unnamed",
                             node));
  g_array_append_val (render_items, item);
  ritem = &g_array_index (render_items,
                          RenderItem,
                          render_items->len - 1);

  if (item.children != NULL)
    render_items = item.children;

  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, &child))
    gsk_gl_renderer_add_render_item (self, render_items, child, ritem);
}

static gboolean
gsk_gl_renderer_validate_tree (GskGLRenderer *self,
                               GskRenderNode *root)
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
  gsk_gl_renderer_add_render_item (self, self->render_items, root, NULL);

  GSK_NOTE (OPENGL, g_print ("Total render items: %d of max:%d\n",
                             self->render_items->len,
                             n_nodes));

  gsk_gl_driver_end_frame (self->gl_driver);

  return TRUE;
}

static void
render_item_clear (RenderItem    *item,
                   GskGLRenderer *self)
{
  gsk_gl_driver_destroy_texture (self->gl_driver, item->render_data.texture_id);
  gsk_gl_driver_destroy_vao (self->gl_driver, item->render_data.vao_id);
}

static void
gsk_gl_renderer_clear_tree (GskGLRenderer *self)
{
  int i;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  for (i = 0; i < self->render_items->len; i++)
    {
      RenderItem *item = &g_array_index (self->render_items, RenderItem, i);

      render_item_clear (item, self);
    }

  g_clear_pointer (&self->render_items, g_array_unref);
}

static void
gsk_gl_renderer_clear (GskGLRenderer *self)
{
  GSK_NOTE (OPENGL, g_print ("Clearing viewport\n"));
  glClearColor (0, 0, 0, 0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void
gsk_gl_renderer_render (GskRenderer *renderer,
                        GskRenderNode *root,
                        GdkDrawingContext *context)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_matrix_t modelview, projection;
  graphene_rect_t viewport;
  guint i;
  guint64 gpu_time;
  int scale_factor;

  if (self->gl_context == NULL)
    return;

  gdk_gl_context_make_current (self->gl_context);

  gsk_renderer_get_viewport (renderer, &viewport);
  scale_factor = gsk_renderer_get_scale_factor (renderer);

  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_renderer_create_buffers (self, viewport.size.width, viewport.size.height, scale_factor);
  gsk_gl_driver_end_frame (self->gl_driver);

  gsk_renderer_get_modelview (renderer, &modelview);
  gsk_renderer_get_projection (renderer, &projection);
  gsk_gl_renderer_update_frustum (self, &modelview, &projection);

  get_gl_scaling_filters (GSK_RENDERER (self),
                          &self->gl_min_filter,
                          &self->gl_mag_filter);
  if (!gsk_gl_renderer_validate_tree (self, root))
    goto out;

  gsk_gl_driver_begin_frame (self->gl_driver);
  gsk_gl_profiler_begin_gpu_region (self->gl_profiler);

  /* Ensure that the viewport is up to date */
  if (gsk_gl_driver_bind_render_target (self->gl_driver, self->texture_id))
    gsk_gl_renderer_resize_viewport (self, &viewport, scale_factor);

  gsk_gl_renderer_clear (self);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  glEnable (GL_BLEND);
  glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
  glBlendEquation (GL_FUNC_ADD);

  /* Transparent pass: back-to-front */
  GSK_NOTE (OPENGL, g_print ("Rendering %u items\n", self->render_items->len));
  for (i = 0; i < self->render_items->len; i++)
    {
      RenderItem *item = &g_array_index (self->render_items, RenderItem, i);

      render_item (self, item);
    }

  /* Draw the output of the GL rendering to the window */
  gsk_gl_driver_end_frame (self->gl_driver);
  gpu_time = gsk_gl_profiler_end_gpu_region (self->gl_profiler);
  GSK_NOTE (OPENGL, g_print ("GPU time: %" G_GUINT64_FORMAT " nsec\n", gpu_time));

out:
  /* XXX: Add GdkDrawingContext API */
  gdk_cairo_draw_from_gl (gdk_drawing_context_get_cairo_context (context),
                          gdk_drawing_context_get_window (context),
                          self->texture_id,
                          GL_TEXTURE,
                          scale_factor,
                          0, 0,
                          viewport.size.width * scale_factor,
                          viewport.size.height * scale_factor);

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
}

static void
gsk_gl_renderer_init (GskGLRenderer *self)
{
  gsk_ensure_resources ();

  graphene_matrix_init_identity (&self->mvp);
}
