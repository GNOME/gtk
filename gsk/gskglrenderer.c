#include "config.h"

#include "gskglrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
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
  guint vao_id;
  guint buffer_id;
  guint texture_id;
  guint program_id;

  guint mvp_location;
  guint map_location;
  guint parentMap_location;
  guint uv_location;
  guint position_location;
  guint alpha_location;
  guint blendMode_location;
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
} RenderItem;

enum {
  MVP,
  MAP,
  PARENT_MAP,
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

  GdkGLContext *context;

  graphene_matrix_t mvp;
  graphene_frustum_t frustum;

  guint frame_buffer;
  guint render_buffer;
  guint depth_stencil_buffer;
  guint texture_id;

  GQuark uniforms[N_UNIFORMS];
  GQuark attributes[N_ATTRIBUTES];

  GskShaderBuilder *shader_builder;

  int blend_program_id;

  guint vao_id;

  GArray *opaque_render_items;
  GArray *transparent_render_items;

  gboolean has_buffers : 1;
  gboolean has_alpha : 1;
  gboolean has_stencil_buffer : 1;
  gboolean has_depth_buffer : 1;
};

struct _GskGLRendererClass
{
  GskRendererClass parent_class;
};

static void render_item_clear (gpointer data_);

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_RENDERER)

static void
gsk_gl_renderer_dispose (GObject *gobject)
{
  GskGLRenderer *self = GSK_GL_RENDERER (gobject);

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gsk_gl_renderer_parent_class)->dispose (gobject);
}

static void
gsk_gl_renderer_create_buffers (GskGLRenderer *self)
{
  if (self->has_buffers)
    return;

  GSK_NOTE (OPENGL, g_print ("Creating buffers\n"));

  glGenFramebuffersEXT (1, &self->frame_buffer);

  if (gsk_renderer_get_use_alpha (GSK_RENDERER (self)))
    {
      if (self->texture_id == 0)
        glGenTextures (1, &self->texture_id);

      if (self->render_buffer != 0)
        {
          glDeleteRenderbuffersEXT (1, &self->render_buffer);
          self->render_buffer = 0;
        }
    }
  else
    {
      if (self->render_buffer == 0)
        glGenRenderbuffersEXT (1, &self->render_buffer);

      if (self->texture_id != 0)
        {
          glDeleteTextures (1, &self->texture_id);
          self->texture_id = 0;
        }
    }

  if (self->has_depth_buffer || self->has_stencil_buffer)
    {
      if (self->depth_stencil_buffer == 0)
        glGenRenderbuffersEXT (1, &self->depth_stencil_buffer);
    }
  else
    {
      if (self->depth_stencil_buffer != 0)
        {
          glDeleteRenderbuffersEXT (1, &self->depth_stencil_buffer);
          self->depth_stencil_buffer = 0;
        }
    }

  /* We only have one VAO at the moment */
  glGenVertexArrays (1, &self->vao_id);
  glBindVertexArray (self->vao_id);

  self->has_buffers = TRUE;
}

static void
gsk_gl_renderer_allocate_buffers (GskGLRenderer *self,
                                  int            width,
                                  int            height)
{
  if (self->context == NULL)
    return;

  if (self->texture_id != 0)
    {
      glBindTexture (GL_TEXTURE_2D, self->texture_id);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      if (gdk_gl_context_get_use_es (self->context))
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      else
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }

  if (self->render_buffer != 0)
    {
      glBindRenderbuffer (GL_RENDERBUFFER, self->render_buffer);
      glRenderbufferStorage (GL_RENDERBUFFER, GL_RGB8, width, height);
    }

  if (self->has_depth_buffer || self->has_stencil_buffer)
    {
      glBindRenderbuffer (GL_RENDERBUFFER, self->depth_stencil_buffer);

      if (self->has_stencil_buffer)
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
      else
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    }
}

static void
gsk_gl_renderer_attach_buffers (GskGLRenderer *self)
{
  gsk_gl_renderer_create_buffers (self);

  GSK_NOTE (OPENGL, g_print ("Attaching buffers\n"));

  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, self->frame_buffer);

  if (self->texture_id != 0)
    {
      glFramebufferTexture2D (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, self->texture_id, 0);
    }
  else if (self->render_buffer != 0)
    {
      glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                    GL_RENDERBUFFER_EXT, self->render_buffer);
    }

  if (self->depth_stencil_buffer != 0)
    {
      if (self->has_depth_buffer)
        glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                      GL_RENDERBUFFER_EXT, self->depth_stencil_buffer);

      if (self->has_stencil_buffer)
        glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
                                      GL_RENDERBUFFER_EXT, self->depth_stencil_buffer);
    }
}

static void
gsk_gl_renderer_destroy_buffers (GskGLRenderer *self)
{
  if (self->context == NULL)
    return;

  if (!self->has_buffers)
    return;

  GSK_NOTE (OPENGL, g_print ("Destroying buffers\n"));

  gdk_gl_context_make_current (self->context);

  if (self->vao_id != 0)
    {
      glDeleteVertexArrays (1, &self->vao_id);
      self->vao_id = 0;
    }

  if (self->depth_stencil_buffer != 0)
    {
      glDeleteRenderbuffersEXT (1, &self->depth_stencil_buffer);
      self->depth_stencil_buffer = 0;
    }

  if (self->render_buffer != 0)
    {
      glDeleteRenderbuffersEXT (1, &self->render_buffer);
      self->render_buffer = 0;
    }

  if (self->texture_id != 0)
    {
      glDeleteTextures (1, &self->texture_id);
      self->texture_id = 0;
    }

  if (self->frame_buffer != 0)
    {
      glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
      glDeleteFramebuffersEXT (1, &self->frame_buffer);
      self->frame_buffer = 0;
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

  self->uniforms[MVP] = gsk_shader_builder_add_uniform (builder, "mvp");
  self->uniforms[MAP] = gsk_shader_builder_add_uniform (builder, "map");
  self->uniforms[PARENT_MAP] = gsk_shader_builder_add_uniform (builder, "parentMap");
  self->uniforms[ALPHA] = gsk_shader_builder_add_uniform (builder, "alpha");
  self->uniforms[BLEND_MODE] = gsk_shader_builder_add_uniform (builder, "blendMode");
  
  self->attributes[POSITION] = gsk_shader_builder_add_attribute (builder, "position");
  self->attributes[UV] = gsk_shader_builder_add_attribute (builder, "uv");

  if (gdk_gl_context_get_use_es (self->context))
    {
      gsk_shader_builder_set_version (builder, SHADER_VERSION_GLES);
      gsk_shader_builder_set_vertex_preamble (builder, "es2_common.vs.glsl");
      gsk_shader_builder_set_fragment_preamble (builder, "es2_common.fs.glsl");
      gsk_shader_builder_add_define (builder, "GSK_GLES", "1");
    }
  else if (gdk_gl_context_is_legacy (self->context))
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
      g_critical ("Unable to create program: %s", error->message);
      g_error_free (error);
      g_object_unref (builder);
      goto out;
    }

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
  if (self->context == NULL)
    {
      GdkWindow *window = gsk_renderer_get_window (renderer);

      if (window == NULL)
        return FALSE;

      self->context = gdk_window_create_gl_context (window, &error);
      if (error != NULL)
        {
          g_critical ("Unable to create GL context for renderer: %s",
                      error->message);
          g_error_free (error);

          return FALSE;
        }
    }

  gdk_gl_context_realize (self->context, &error);
  if (error != NULL)
    {
      g_critical ("Unable to realize GL renderer: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  gdk_gl_context_make_current (self->context);

  GSK_NOTE (OPENGL, g_print ("Creating buffers and programs\n"));
  if (!gsk_gl_renderer_create_programs (self))
    return FALSE;

  gsk_gl_renderer_create_buffers (self);

  return TRUE;
}

static void
gsk_gl_renderer_unrealize (GskRenderer *renderer)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);

  if (self->context == NULL)
    return;

  gdk_gl_context_make_current (self->context);

  g_clear_pointer (&self->opaque_render_items, g_array_unref);
  g_clear_pointer (&self->transparent_render_items, g_array_unref);

  gsk_gl_renderer_destroy_buffers (self);
  gsk_gl_renderer_destroy_programs (self);

  if (self->context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();
}

static void
gsk_gl_renderer_resize_viewport (GskGLRenderer         *self,
                                 const graphene_rect_t *viewport)
{
  GSK_NOTE (OPENGL, g_print ("glViewport(0, 0, %g, %g)\n",
                             viewport->size.width,
                             viewport->size.height));

  glViewport (viewport->origin.x, viewport->origin.y,
              viewport->size.width, viewport->size.height);
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

static void
render_item_clear (gpointer data_)
{
  RenderItem *item = data_;

  GSK_NOTE (OPENGL, g_print ("Destroying render item [%p] buffer %u\n",
                             item,
                             item->render_data.buffer_id));
  glDeleteBuffers (1, &item->render_data.buffer_id);
  item->render_data.buffer_id = 0;

  GSK_NOTE (OPENGL, g_print ("Destroying render item [%p] texture %u\n",
                             item,
                             item->render_data.texture_id));
  glDeleteTextures (1, &item->render_data.texture_id);
  item->render_data.texture_id = 0;

  graphene_matrix_init_identity (&item->mvp);

  item->opacity = 1;
}

#define N_VERTICES      6

static void
render_item (RenderItem *item)
{
  struct vertex_info {
    float position[2];
    float uv[2];
  };
  float mvp[16];

  glBindVertexArray (item->render_data.vao_id);

  /* Generate the vertex buffer for the texture quad */
  if (item->render_data.buffer_id == 0)
    {
      struct vertex_info vertex_data[] = {
        { { item->min.x, item->min.y }, { 0, 0 }, },
        { { item->min.x, item->max.y }, { 0, 1 }, },
        { { item->max.x, item->min.y }, { 1, 0 }, },

        { { item->max.x, item->max.y }, { 1, 1 }, },
        { { item->min.x, item->max.y }, { 0, 1 }, },
        { { item->max.x, item->min.y }, { 1, 0 }, },
      };

      GSK_NOTE (OPENGL, g_print ("Creating quad for render item [%p]\n", item));

      glGenBuffers (1, &(item->render_data.buffer_id));
      glBindBuffer (GL_ARRAY_BUFFER, item->render_data.buffer_id);

      /* The data won't change */
      glBufferData (GL_ARRAY_BUFFER, sizeof (vertex_data), vertex_data, GL_STATIC_DRAW);
  
      /* Set up the buffers with the computed position and texels */
      glEnableVertexAttribArray (item->render_data.position_location);
      glVertexAttribPointer (item->render_data.position_location, 2, GL_FLOAT, GL_FALSE,
                             sizeof (struct vertex_info),
                             (void *) G_STRUCT_OFFSET (struct vertex_info, position));
      glEnableVertexAttribArray (item->render_data.uv_location);
      glVertexAttribPointer (item->render_data.uv_location, 2, GL_FLOAT, GL_FALSE,
                             sizeof (struct vertex_info),
                             (void *) G_STRUCT_OFFSET (struct vertex_info, uv));
    }
  else
    {
      /* We already set up the vertex buffer, so we just need to reuse it */
      glBindBuffer (GL_ARRAY_BUFFER, item->render_data.buffer_id);
      glEnableVertexAttribArray (item->render_data.position_location);
      glEnableVertexAttribArray (item->render_data.uv_location);
    }

  glUseProgram (item->render_data.program_id);

  /* Use texture unit 0 for the sampler */
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, item->render_data.texture_id);
  glUniform1i (item->render_data.map_location, 0);

  if (item->parent_data != NULL)
    {
      if (item->parent_data->texture_id != 0)
        {
          glActiveTexture (GL_TEXTURE1);
          glBindTexture (GL_TEXTURE_2D, item->parent_data->texture_id);
          glUniform1i (item->render_data.parentMap_location, 1);
        }

      glUniform1i (item->render_data.blendMode_location, item->blend_mode);
    }

  /* Pass the opacity component */
  glUniform1f (item->render_data.alpha_location, item->opaque ? 1 : item->opacity);

  /* Pass the mvp to the vertex shader */
  GSK_NOTE (OPENGL, graphene_matrix_print (&item->mvp));
  graphene_matrix_to_float (&item->mvp, mvp);
  glUniformMatrix4fv (item->render_data.mvp_location, 1, GL_FALSE, mvp);

  /* Draw the quad */
  GSK_NOTE (OPENGL, g_print ("Drawing item <%s>[%p] with opacity: %g\n",
                             item->name,
                             item,
                             item->opaque ? 1 : item->opacity));

  glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);

  /* Reset the state */
  glBindTexture (GL_TEXTURE_2D, 0);
  glDisableVertexAttribArray (item->render_data.position_location);
  glDisableVertexAttribArray (item->render_data.uv_location);
  glUseProgram (0);
}

static void
surface_to_texture (cairo_surface_t *surface,
                    graphene_rect_t *clip,
                    int              min_filter,
                    int              mag_filter,
                    guint           *texture_out)
{
  guint texture_id;

  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  GSK_NOTE (OPENGL, g_print ("Uploading surface[%p] { w:%g, h:%g } to texid:%d\n",
                             surface,
                             clip->size.width,
                             clip->size.height,
                             texture_id));

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  gdk_cairo_surface_upload_to_gl (surface, GL_TEXTURE_2D, clip->size.width, clip->size.height, NULL);

  if (min_filter != GL_NEAREST)
    glGenerateMipmap (GL_TEXTURE_2D);

  GSK_NOTE (OPENGL, g_print ("New texture id %d from surface %p\n", texture_id, surface));

  *texture_out = texture_id;
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

static void
gsk_gl_renderer_add_render_item (GskGLRenderer *self,
                                 GskRenderNode *node,
                                 RenderItem    *parent)
{
  graphene_rect_t viewport;
  int gl_min_filter, gl_mag_filter;
  cairo_surface_t *surface;
  GskRenderNodeIter iter;
  graphene_matrix_t mv, projection;
  graphene_rect_t bounds;
  GskRenderNode *child;
  RenderItem item;
  int program_id;

  if (gsk_render_node_is_hidden (node))
    {
      GSK_NOTE (OPENGL, g_print ("Skipping hidden node <%s>[%p]\n",
                                 node->name != NULL ? node->name : "unnamed",
                                 node));
      return;
    }

  memset (&item, 0, sizeof (RenderItem));

  gsk_renderer_get_viewport (GSK_RENDERER (self), &viewport);

  gsk_render_node_get_bounds (node, &bounds);

  item.node = node;
  item.name = node->name != NULL ? node->name : "unnamed";

  /* The texture size */
  item.size = bounds.size;

  /* Each render item is an axis-aligned bounding box that we
   * transform using the given transformation matrix
   */
  item.min.x = bounds.origin.x;
  item.min.y = bounds.origin.y;
  item.min.z = 0.f;

  item.max.x = bounds.origin.x + bounds.size.width;
  item.max.y = bounds.origin.y + bounds.size.height;
  item.max.z = 0.f;

  /* The location of the item, in normalized world coordinates */
  gsk_render_node_get_world_matrix (node, &mv);
  graphene_matrix_multiply (&mv, &self->mvp, &item.mvp);

  item.opaque = gsk_render_node_is_opaque (node);
  item.opacity = gsk_render_node_get_opacity (node);

  item.blend_mode = gsk_render_node_get_blend_mode (node);

  /* GL objects */
  item.render_data.vao_id = self->vao_id;
  item.render_data.buffer_id = 0;

  program_id = self->blend_program_id;
  item.render_data.program_id = program_id;

  item.render_data.map_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[MAP]);
  item.render_data.parentMap_location =
    gsk_shader_builder_get_uniform_location (self->shader_builder, program_id, self->uniforms[PARENT_MAP]);
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

  if (parent != NULL)
    item.parent_data = &(parent->render_data);
  else
    item.parent_data = NULL;

  gsk_renderer_get_projection (GSK_RENDERER (self), &projection);
  item.z = project_item (&projection, &mv);

  /* TODO: This should really be an asset atlas, to avoid uploading a ton
   * of textures. Ideally we could use a single Cairo surface to get around
   * the GL texture limits and reorder the texture data on the CPU side and
   * do a single upload; alternatively, we could use a separate FBO and
   * render each texture into it
   */
  get_gl_scaling_filters (GSK_RENDERER (self), &gl_min_filter, &gl_mag_filter);
  surface = gsk_render_node_get_surface (node);

  /* If the node does not have any surface we skip drawing it, but we still
   * recurse.
   *
   * XXX: This needs to be re-done if the opacity is != 0, in which case we
   * need to composite the opacity level of the children
   */
  if (surface == NULL)
    goto recurse_children;

  surface_to_texture (surface, &bounds, gl_min_filter, gl_mag_filter, &(item.render_data.texture_id));

  GSK_NOTE (OPENGL, g_print ("Adding node <%s>[%p] to render items\n",
                             node->name != NULL ? node->name : "unnamed",
                             node));
  if (gsk_render_node_is_opaque (node) && gsk_render_node_get_opacity (node) == 1.f)
    g_array_append_val (self->opaque_render_items, item);
  else
    g_array_prepend_val (self->transparent_render_items, item);

recurse_children:
  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, &child))
    gsk_gl_renderer_add_render_item (self, child, &item);
}

static gboolean
gsk_gl_renderer_validate_tree (GskGLRenderer *self,
                               GskRenderNode *root)
{
  int n_nodes;

  if (self->context == NULL)
    {
      GSK_NOTE (OPENGL, g_print ("No valid GL context associated to the renderer"));
      return FALSE;
    }

  n_nodes = gsk_render_node_get_size (root);

  gdk_gl_context_make_current (self->context);

  self->opaque_render_items = g_array_sized_new (FALSE, FALSE, sizeof (RenderItem), n_nodes);
  self->transparent_render_items = g_array_sized_new (FALSE, FALSE, sizeof (RenderItem), n_nodes);

  g_array_set_clear_func (self->opaque_render_items, render_item_clear);
  g_array_set_clear_func (self->transparent_render_items, render_item_clear);

  GSK_NOTE (OPENGL, g_print ("RenderNode -> RenderItem\n"));
  gsk_gl_renderer_add_render_item (self, root, NULL);

  GSK_NOTE (OPENGL, g_print ("Total render items: %d of %d (opaque:%d, transparent:%d)\n",
                             self->opaque_render_items->len + self->transparent_render_items->len,
                             n_nodes,
                             self->opaque_render_items->len,
                             self->transparent_render_items->len));

  return TRUE;
}

static void
gsk_gl_renderer_clear_tree (GskGLRenderer *self)
{
  if (self->context == NULL)
    return;

  g_clear_pointer (&self->opaque_render_items, g_array_unref);
  g_clear_pointer (&self->transparent_render_items, g_array_unref);
}

static void
gsk_gl_renderer_clear (GskGLRenderer *self)
{
  int clear_bits = GL_COLOR_BUFFER_BIT;

  if (self->has_depth_buffer)
    clear_bits |= GL_DEPTH_BUFFER_BIT;
  if (self->has_stencil_buffer)
    clear_bits |= GL_STENCIL_BUFFER_BIT;

  GSK_NOTE (OPENGL, g_print ("Clearing viewport\n"));
  glClearColor (0, 0, 0, 0);
  glClear (clear_bits);
}

static void
gsk_gl_renderer_render (GskRenderer *renderer,
                        GskRenderNode *root,
                        GdkDrawingContext *context)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_matrix_t modelview, projection;
  graphene_rect_t viewport;
  gboolean use_alpha;
  int status;
  guint i;

  if (self->context == NULL)
    return;

  gdk_gl_context_make_current (self->context);

  gsk_renderer_get_viewport (renderer, &viewport);

  gsk_gl_renderer_create_buffers (self);
  gsk_gl_renderer_allocate_buffers (self, viewport.size.width, viewport.size.height);
  gsk_gl_renderer_attach_buffers (self);

  if (self->has_depth_buffer)
    glEnable (GL_DEPTH_TEST);
  else
    glDisable (GL_DEPTH_TEST);

  /* Ensure that the viewport is up to date */
  status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
  if (status == GL_FRAMEBUFFER_COMPLETE_EXT)
    gsk_gl_renderer_resize_viewport (self, &viewport);

  gsk_gl_renderer_clear (self);

  gsk_renderer_get_modelview (renderer, &modelview);
  gsk_renderer_get_projection (renderer, &projection);
  gsk_gl_renderer_update_frustum (self, &modelview, &projection);

  if (!gsk_gl_renderer_validate_tree (self, root))
    goto out;

  /* Opaque pass: front-to-back */
  GSK_NOTE (OPENGL, g_print ("Rendering %u opaque items\n", self->opaque_render_items->len));
  for (i = 0; i < self->opaque_render_items->len; i++)
    {
      RenderItem *item = &g_array_index (self->opaque_render_items, RenderItem, i);

      render_item (item);
    }

  glEnable (GL_BLEND);

  /* Transparent pass: back-to-front */
  GSK_NOTE (OPENGL, g_print ("Rendering %u transparent items\n", self->transparent_render_items->len));
  for (i = 0; i < self->transparent_render_items->len; i++)
    {
      RenderItem *item = &g_array_index (self->transparent_render_items, RenderItem, i);

      render_item (item);
    }

  glDisable (GL_BLEND);

  /* Draw the output of the GL rendering to the window */
  GSK_NOTE (OPENGL, g_print ("Drawing GL content on Cairo surface using a %s\n",
                             self->texture_id != 0 ? "texture" : "renderbuffer"));

out:
  use_alpha = gsk_renderer_get_use_alpha (renderer);

  gdk_cairo_draw_from_gl (gdk_drawing_context_get_cairo_context (context),
                          gdk_drawing_context_get_window (context),
                          use_alpha ? self->texture_id : self->render_buffer,
                          use_alpha ? GL_TEXTURE : GL_RENDERBUFFER,
                          gsk_renderer_get_scale_factor (renderer),
                          0, 0, viewport.size.width, viewport.size.height);

  gdk_gl_context_make_current (self->context);

  gsk_gl_renderer_clear_tree (self);
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

  self->has_depth_buffer = TRUE;
  self->has_stencil_buffer = TRUE;
}
