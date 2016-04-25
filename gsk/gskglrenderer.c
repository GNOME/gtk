#include "config.h"

#include "gskglrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskenums.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrendernodeiter.h"

#include "gskprivate.h"

#include <epoxy/gl.h>

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

  guint vao_id;
  guint texture_id;
  guint program_id;
  guint mvp_location;
  guint map_location;
  guint uv_location;
  guint position_location;
  guint alpha_location;
  guint buffer_id;
} RenderItem;

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

  guint program_id;
  guint mvp_location;
  guint map_location;
  guint uv_location;
  guint position_location;
  guint alpha_location;

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

static guint
create_shader (int         type,
               const char *code)
{
  guint shader;
  int status;

  shader = glCreateShader (type);
  glShaderSource (shader, 1, &code, NULL);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc0 (log_len + 1);
      glGetShaderInfoLog (shader, log_len, NULL, buffer);

      g_critical ("Compile failure in %s shader:\n%s",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                  buffer);
      g_free (buffer);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

static void
gsk_gl_renderer_create_program (GskGLRenderer *self)
{
  guint vertex_shader = 0, fragment_shader = 0;
  const char *fs_path, *vs_path;
  GBytes *source;
  int status;

  if (gdk_gl_context_get_use_es (self->context))
    {
      vs_path = "/org/gtk/libgsk/glsl/gles-base.vs.glsl";
      fs_path = "/org/gtk/libgsk/glsl/gles-base.fs.glsl";
    }
  else
    {
      vs_path = "/org/gtk/libgsk/glsl/gl3-base.vs.glsl";
      fs_path = "/org/gtk/libgsk/glsl/gl3-base.fs.glsl";
    }

  GSK_NOTE (OPENGL, g_print ("Compiling vertex shader\n"));
  source = g_resources_lookup_data (vs_path, 0, NULL);
  vertex_shader = create_shader (GL_VERTEX_SHADER, g_bytes_get_data (source, NULL));
  g_bytes_unref (source);
  if (vertex_shader == 0)
    goto out;

  GSK_NOTE (OPENGL, g_print ("Compiling fragment shader\n"));
  source = g_resources_lookup_data (fs_path, 0, NULL);
  fragment_shader = create_shader (GL_FRAGMENT_SHADER, g_bytes_get_data (source, NULL));
  g_bytes_unref (source);
  if (fragment_shader == 0)
    goto out;

  self->program_id = glCreateProgram ();
  glAttachShader (self->program_id, vertex_shader);
  glAttachShader (self->program_id, fragment_shader);
  glLinkProgram (self->program_id);

  glGetProgramiv (self->program_id, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      char *buffer = NULL;
      int log_len = 0;

      glGetProgramiv (self->program_id, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc0 (log_len + 1);
      glGetProgramInfoLog (self->program_id, log_len, NULL, buffer);

      g_critical ("Linking failure in shader:\n%s", buffer);
      g_free (buffer);

      glDeleteProgram (self->program_id);
      self->program_id = 0;

      goto out;
    }

  /* Find the location of each uniform and attribute we use in our
   * shaders
   */
  self->mvp_location = glGetUniformLocation (self->program_id, "mvp");
  self->map_location = glGetUniformLocation (self->program_id, "map");
  self->alpha_location = glGetUniformLocation (self->program_id, "alpha");
  self->position_location = glGetAttribLocation (self->program_id, "position");
  self->uv_location = glGetAttribLocation (self->program_id, "uv");

  GSK_NOTE (OPENGL, g_print ("Program [%d] { mvp:%u, map:%u, alpha:%u, position:%u, uv:%u }\n",
                             self->program_id,
                             self->mvp_location,
                             self->map_location,
                             self->alpha_location,
                             self->position_location,
                             self->uv_location));

  /* We can detach and destroy the shaders from the linked program */
  glDetachShader (self->program_id, vertex_shader);
  glDetachShader (self->program_id, fragment_shader);

out:
  if (vertex_shader != 0)
    glDeleteShader (vertex_shader);
  if (fragment_shader != 0)
    glDeleteShader (fragment_shader);
}

static void
gsk_gl_renderer_destroy_program (GskGLRenderer *self)
{
  if (self->program_id != 0)
    {
      glDeleteProgram (self->program_id);
      self->program_id = 0;
    }
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

  gsk_gl_renderer_create_buffers (self);
  gsk_gl_renderer_create_program (self);

  self->opaque_render_items = g_array_sized_new (FALSE, FALSE, sizeof (RenderItem), 16);
  g_array_set_clear_func (self->opaque_render_items, render_item_clear);

  self->transparent_render_items = g_array_sized_new (FALSE, FALSE, sizeof (RenderItem), 16);
  g_array_set_clear_func (self->opaque_render_items, render_item_clear);

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
  gsk_gl_renderer_destroy_program (self);

  if (self->context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();
}

static void
gsk_gl_renderer_resize_viewport (GskRenderer           *renderer,
                                 const graphene_rect_t *viewport)
{
}

static void
gsk_gl_renderer_update (GskRenderer             *renderer,
                        const graphene_matrix_t *modelview,
                        const graphene_matrix_t *projection)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);

  GSK_NOTE (OPENGL, g_print ("Updating the modelview/projection\n"));

  graphene_matrix_multiply (modelview, projection, &self->mvp);

  graphene_frustum_init_from_matrix (&self->frustum, &self->mvp);
}

static void
render_item_clear (gpointer data_)
{
  RenderItem *item = data_;

  GSK_NOTE (OPENGL, g_print ("Destroying render item [%p] buffer %u\n",
                             item,
                             item->buffer_id));
  glDeleteBuffers (1, &item->buffer_id);
  item->buffer_id = 0;

  GSK_NOTE (OPENGL, g_print ("Destroying render item [%p] texture %u\n",
                             item,
                             item->texture_id));
  glDeleteTextures (1, &item->texture_id);
  item->texture_id = 0;

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

  glBindVertexArray (item->vao_id);

  /* Generate the vertex buffer for the texture quad */
  if (item->buffer_id == 0)
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

      glGenBuffers (1, &item->buffer_id);
      glBindBuffer (GL_ARRAY_BUFFER, item->buffer_id);

      /* The data won't change */
      glBufferData (GL_ARRAY_BUFFER, sizeof (vertex_data), vertex_data, GL_STATIC_DRAW);
  
      /* Set up the buffers with the computed position and texels */
      glEnableVertexAttribArray (item->position_location);
      glVertexAttribPointer (item->position_location, 2, GL_FLOAT, GL_FALSE,
                             sizeof (struct vertex_info),
                             (void *) G_STRUCT_OFFSET (struct vertex_info, position));
      glEnableVertexAttribArray (item->uv_location);
      glVertexAttribPointer (item->uv_location, 2, GL_FLOAT, GL_FALSE,
                             sizeof (struct vertex_info),
                             (void *) G_STRUCT_OFFSET (struct vertex_info, uv));
    }
  else
    {
      /* We already set up the vertex buffer, so we just need to reuse it */
      glBindBuffer (GL_ARRAY_BUFFER, item->buffer_id);
      glEnableVertexAttribArray (item->position_location);
      glEnableVertexAttribArray (item->uv_location);
    }

  glUseProgram (item->program_id);

  /* Use texture unit 0 for the sampler */
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, item->texture_id);
  glUniform1i (item->map_location, 0);

  /* Pass the opacity component */
  glUniform1f (item->alpha_location, item->opaque ? 1 : item->opacity);

  /* Pass the mvp to the vertex shader */
  GSK_NOTE (OPENGL, graphene_matrix_print (&item->mvp));
  graphene_matrix_to_float (&item->mvp, mvp);
  glUniformMatrix4fv (item->mvp_location, 1, GL_FALSE, mvp);

  /* Draw the quad */
  GSK_NOTE (OPENGL, g_print ("Drawing item <%s>[%p] with opacity: %g\n",
                             item->name,
                             item,
                             item->opaque ? 1 : item->opacity));

  glDrawArrays (GL_TRIANGLES, 0, N_VERTICES);

  /* Reset the state */
  glBindTexture (GL_TEXTURE_2D, 0);
  glDisableVertexAttribArray (item->position_location);
  glDisableVertexAttribArray (item->uv_location);
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

static gboolean
check_in_frustum (const graphene_frustum_t *frustum,
                  RenderItem               *item)
{
  graphene_box_t aabb;

  graphene_box_init (&aabb, &item->min, &item->max);
  graphene_matrix_transform_box (&item->mvp, &aabb, &aabb);

  return graphene_frustum_intersects_box (frustum, &aabb);
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
gsk_gl_renderer_add_render_item (GskGLRenderer *self,
                                 GskRenderNode *node)
{
  graphene_rect_t viewport;
  int gl_min_filter, gl_mag_filter;
  cairo_surface_t *surface;
  GskRenderNodeIter iter;
  graphene_matrix_t mv, projection;
  graphene_rect_t bounds;
  GskRenderNode *child;
  RenderItem item;

  if (gsk_render_node_is_hidden (node))
    {
      GSK_NOTE (OPENGL, g_print ("Skipping hidden node <%s>[%p]\n",
                                 node->name != NULL ? node->name : "unnamed",
                                 node));
      return;
    }

  gsk_renderer_get_viewport (GSK_RENDERER (self), &viewport);

  gsk_render_node_get_bounds (node, &bounds);

  item.node = node;
  item.name = node->name != NULL ? node->name : "unnamed";

  /* The texture size */
  item.size = bounds.size;

  /* Each render item is an axis-aligned bounding box that we
   * transform using the given transformation matrix
   */
  item.min.x = (bounds.origin.x * 2) / bounds.size.width - 1;
  item.min.y = (bounds.origin.y * 2) / bounds.size.height - 1;
  item.min.z = 0.f;

  item.max.x = (bounds.origin.x + bounds.size.width) * 2 / bounds.size.width - 1;
  item.max.y = (bounds.origin.y + bounds.size.height) * 2 / bounds.size.height - 1;
  item.max.z = 0.f;

  /* The location of the item, in normalized world coordinates */
  gsk_render_node_get_world_matrix (node, &mv);
  item.mvp = mv;

  item.opaque = gsk_render_node_is_opaque (node);
  item.opacity = gsk_render_node_get_opacity (node);

  /* GL objects */
  item.vao_id = self->vao_id;
  item.buffer_id = 0;
  item.program_id = self->program_id;
  item.map_location = self->map_location;
  item.mvp_location = self->mvp_location;
  item.uv_location = self->uv_location;
  item.position_location = self->position_location;
  item.alpha_location = self->alpha_location;

  gsk_renderer_get_projection (GSK_RENDERER (self), &projection);
  item.z = project_item (&projection, &mv);

  /* Discard the item if it's outside of the frustum as determined by the
   * viewport and the projection matrix
   */
#if 0
  if (!check_in_frustum (&self->frustum, &item))
    {
      GSK_NOTE (OPENGL, g_print ("Node <%s>[%p] culled by frustum\n",
                                 node->name != NULL ? node->name : "unnamed",
                                 node));
      return;
    }
#endif

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

  surface_to_texture (surface, &bounds, gl_min_filter, gl_mag_filter, &item.texture_id);

  GSK_NOTE (OPENGL, g_print ("Adding node <%s>[%p] to render items\n",
                             node->name != NULL ? node->name : "unnamed",
                             node));
  if (gsk_render_node_is_opaque (node) && gsk_render_node_get_opacity (node) == 1.f)
    g_array_append_val (self->opaque_render_items, item);
  else
    g_array_append_val (self->transparent_render_items, item);

recurse_children:
  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, &child))
    gsk_gl_renderer_add_render_item (self, child);
}

static int
opaque_item_cmp (gconstpointer _a,
                 gconstpointer _b)
{
  const RenderItem *a = _a;
  const RenderItem *b = _b;

  if (a->z != b->z)
    {
      if (a->z > b->z)
        return 1;

      return -1;
    }

  if (a != b)
    {
      if ((gsize) a > (gsize) b)
        return 1;

      return -1;
    }

  return 0;
}

static int
transparent_item_cmp (gconstpointer _a,
                      gconstpointer _b)
{
  const RenderItem *a = _a;
  const RenderItem *b = _b;

  if (a->z != b->z)
    {
     if (a->z < b->z)
       return 1;

     return -1;
    }

  if (a != b)
    {
      if ((gsize) a < (gsize) b)
        return 1;

      return -1;
    }

  return 0;
}

static void
gsk_gl_renderer_validate_tree (GskRenderer   *renderer,
                               GskRenderNode *root)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  gboolean clear_items = FALSE;
  int i;

  if (self->context == NULL)
    return;

  gdk_gl_context_make_current (self->context);

  if (self->opaque_render_items->len > 0 || self->transparent_render_items->len > 0)
    {
      /* If we only changed the opacity and transformations then there is no
       * reason to clear the render items
       */
      for (i = 0; i < self->opaque_render_items->len; i++)
        {
          RenderItem *item = &g_array_index (self->opaque_render_items, RenderItem, i);
          GskRenderNodeChanges changes = gsk_render_node_get_last_state (item->node);

          if (changes == 0)
            continue;

          if ((changes & GSK_RENDER_NODE_CHANGES_UPDATE_OPACITY) != 0)
            {
              item->opaque = gsk_render_node_is_opaque (item->node);
              item->opacity = gsk_render_node_get_opacity (item->node);
              changes &= ~GSK_RENDER_NODE_CHANGES_UPDATE_OPACITY;
            }

          if (changes & GSK_RENDER_NODE_CHANGES_UPDATE_TRANSFORM)
            {
              gsk_render_node_get_world_matrix (item->node, &item->mvp);
              changes &= ~ GSK_RENDER_NODE_CHANGES_UPDATE_TRANSFORM;
            }

          if (changes != 0)
            {
              clear_items = TRUE;
              break;
            }
        }

      for (i = 0; i < self->transparent_render_items->len; i++)
        {
          RenderItem *item = &g_array_index (self->transparent_render_items, RenderItem, i);
          GskRenderNodeChanges changes = gsk_render_node_get_last_state (item->node);

          if (changes == 0)
            continue;

          if ((changes & GSK_RENDER_NODE_CHANGES_UPDATE_OPACITY) != 0)
            {
              item->opaque = gsk_render_node_is_opaque (item->node);
              item->opacity = gsk_render_node_get_opacity (item->node);
              changes &= ~GSK_RENDER_NODE_CHANGES_UPDATE_OPACITY;
            }

          if (changes & GSK_RENDER_NODE_CHANGES_UPDATE_TRANSFORM)
            {
              gsk_render_node_get_world_matrix (item->node, &item->mvp);
              changes &= ~ GSK_RENDER_NODE_CHANGES_UPDATE_TRANSFORM;
            }

          if (changes != 0)
            {
              clear_items = TRUE;
              break;
            }
        }
    }
  else
    clear_items = TRUE;

  if (!clear_items)
    {
      GSK_NOTE (OPENGL, g_print ("Tree is still valid\n"));
      goto out;
    }

  for (i = 0; i < self->opaque_render_items->len; i++)
    render_item_clear (&g_array_index (self->opaque_render_items, RenderItem, i));
  for (i = 0; i < self->transparent_render_items->len; i++)
    render_item_clear (&g_array_index (self->transparent_render_items, RenderItem, i));

  g_array_set_size (self->opaque_render_items, 0);
  g_array_set_size (self->transparent_render_items, 0);

  GSK_NOTE (OPENGL, g_print ("RenderNode -> RenderItem\n"));
  gsk_gl_renderer_add_render_item (self, gsk_renderer_get_root_node (renderer));

  GSK_NOTE (OPENGL, g_print ("Sorting render nodes\n"));
  g_array_sort (self->opaque_render_items, opaque_item_cmp);
  g_array_sort (self->transparent_render_items, transparent_item_cmp);

out:
  GSK_NOTE (OPENGL, g_print ("Total render items: %d (opaque:%d, transparent:%d)\n",
                             self->opaque_render_items->len + self->transparent_render_items->len,
                             self->opaque_render_items->len,
                             self->transparent_render_items->len));
}

static void
gsk_gl_renderer_clear (GskRenderer *renderer)
{
}

static void
gsk_gl_renderer_render (GskRenderer *renderer)
{
  GskGLRenderer *self = GSK_GL_RENDERER (renderer);
  graphene_rect_t viewport;
  int scale, status, clear_bits;
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
    {
      GSK_NOTE (OPENGL, g_print ("glViewport(0, 0, %g, %g)\n",
                                 viewport.size.width,
                                 viewport.size.height));
      glViewport (0, 0, viewport.size.width, viewport.size.height);
    }

  clear_bits = GL_COLOR_BUFFER_BIT;
  if (self->has_depth_buffer)
    clear_bits |= GL_DEPTH_BUFFER_BIT;
  if (self->has_stencil_buffer)
    clear_bits |= GL_STENCIL_BUFFER_BIT;

  GSK_NOTE (OPENGL, g_print ("Clearing viewport\n"));
  glClearColor (0, 0, 0, 0);
  glClear (clear_bits);

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
  scale = 1;
  gdk_cairo_draw_from_gl (gsk_renderer_get_draw_context (renderer),
                          gsk_renderer_get_window (renderer),
                          self->texture_id != 0 ? self->texture_id : self->render_buffer,
                          self->texture_id != 0 ? GL_TEXTURE : GL_RENDERBUFFER,
                          scale,
                          0, 0, viewport.size.width, viewport.size.height);

  gdk_gl_context_make_current (self->context);
}

static void
gsk_gl_renderer_class_init (GskGLRendererClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  gobject_class->dispose = gsk_gl_renderer_dispose;

  renderer_class->realize = gsk_gl_renderer_realize;
  renderer_class->unrealize = gsk_gl_renderer_unrealize;
  renderer_class->resize_viewport = gsk_gl_renderer_resize_viewport;
  renderer_class->update = gsk_gl_renderer_update;
  renderer_class->clear = gsk_gl_renderer_clear;
  renderer_class->validate_tree = gsk_gl_renderer_validate_tree;
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

void
gsk_gl_renderer_set_context (GskGLRenderer *renderer,
                             GdkGLContext  *context)
{
  g_return_if_fail (GSK_IS_GL_RENDERER (renderer));
  g_return_if_fail (context == NULL || GDK_IS_GL_CONTEXT (context));

  if (gsk_renderer_is_realized (GSK_RENDERER (renderer)))
    return;

  if (gdk_gl_context_get_display (context) != gsk_renderer_get_display (GSK_RENDERER (renderer)))
    return;

  g_set_object (&renderer->context, context);
}

GdkGLContext *
gsk_gl_renderer_get_context (GskGLRenderer *renderer)
{
  g_return_val_if_fail (GSK_IS_GL_RENDERER (renderer), NULL);

  return renderer->context;
}
