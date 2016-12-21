#include "config.h"

#include "gskgldriverprivate.h"

#include "gskdebugprivate.h"
#include "gsktextureprivate.h"

#include <gdk/gdk.h>
#include <epoxy/gl.h>

typedef struct {
  GLuint texture_id;
  int width;
  int height;
  GLuint min_filter;
  GLuint mag_filter;
  GArray *fbos;
  GskTexture *user;
  gboolean in_use : 1;
} Texture;

typedef struct {
  GLuint vao_id;
  GLuint buffer_id;
  GLuint position_id;
  GLuint uv_id;
  GskQuadVertex *quads;
  int n_quads;
  gboolean in_use : 1;
} Vao;

typedef struct {
  GLuint fbo_id;
  GLuint depth_stencil_id;
} Fbo;

struct _GskGLDriver
{
  GObject parent_instance;

  GdkGLContext *gl_context;

  Fbo default_fbo;

  GHashTable *textures;
  GHashTable *vaos;

  Texture *bound_source_texture;
  Texture *bound_mask_texture;
  Vao *bound_vao;
  Fbo *bound_fbo;

  int max_texture_size;

  gboolean in_frame : 1;
};

enum
{
  PROP_GL_CONTEXT = 1,

  N_PROPS
};

static GParamSpec *gsk_gl_driver_properties[N_PROPS];

G_DEFINE_TYPE (GskGLDriver, gsk_gl_driver, G_TYPE_OBJECT)

static Texture *
texture_new (void)
{
  return g_slice_new0 (Texture);
}

static void
texture_free (gpointer data)
{
  Texture *t = data;

  if (t->user)
    gsk_texture_clear_render_data (t->user);

  g_clear_pointer (&t->fbos, g_array_unref);
  glDeleteTextures (1, &t->texture_id);
  g_slice_free (Texture, t);
}

static void
fbo_clear (gpointer data)
{
  Fbo *f = data;

  if (f->depth_stencil_id != 0)
    glDeleteRenderbuffers (1, &f->depth_stencil_id);

  glDeleteFramebuffers (1, &f->fbo_id);
}

static Vao *
vao_new (void)
{
  return g_slice_new0 (Vao);
}

static void
vao_free (gpointer data)
{
  Vao *v = data;

  g_free (v->quads);
  glDeleteBuffers (1, &v->buffer_id);
  glDeleteVertexArrays (1, &v->vao_id);
  g_slice_free (Vao, v);
}

static void
gsk_gl_driver_finalize (GObject *gobject)
{
  GskGLDriver *self = GSK_GL_DRIVER (gobject);

  gdk_gl_context_make_current (self->gl_context);

  g_clear_pointer (&self->textures, g_hash_table_unref);
  g_clear_pointer (&self->vaos, g_hash_table_unref);

  if (self->gl_context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();

  g_clear_object (&self->gl_context);

  G_OBJECT_CLASS (gsk_gl_driver_parent_class)->finalize (gobject);
}

static void
gsk_gl_driver_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GskGLDriver *self = GSK_GL_DRIVER (gobject);

  switch (prop_id)
    {
    case PROP_GL_CONTEXT:
      self->gl_context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gsk_gl_driver_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GskGLDriver *self = GSK_GL_DRIVER (gobject);

  switch (prop_id)
    {
    case PROP_GL_CONTEXT:
      g_value_set_object (value, self->gl_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gsk_gl_driver_class_init (GskGLDriverClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gsk_gl_driver_set_property;
  gobject_class->get_property = gsk_gl_driver_get_property;
  gobject_class->finalize = gsk_gl_driver_finalize;

  gsk_gl_driver_properties[PROP_GL_CONTEXT] =
    g_param_spec_object ("gl-context", "GL Context", "The GL context used by the driver",
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, gsk_gl_driver_properties);
}

static void
gsk_gl_driver_init (GskGLDriver *self)
{
  self->textures = g_hash_table_new_full (NULL, NULL, NULL, texture_free);
  self->vaos = g_hash_table_new_full (NULL, NULL, NULL, vao_free);

  self->max_texture_size = -1;
}

GskGLDriver *
gsk_gl_driver_new (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return g_object_new (GSK_TYPE_GL_DRIVER,
                       "gl-context", context,
                       NULL);
}

void
gsk_gl_driver_begin_frame (GskGLDriver *driver)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (driver));
  g_return_if_fail (!driver->in_frame);

  driver->in_frame = TRUE;

  if (driver->max_texture_size < 0)
    {
      glGetIntegerv (GL_MAX_TEXTURE_SIZE, (GLint *) &driver->max_texture_size);
      GSK_NOTE (OPENGL, g_print ("GL max texture size: %d\n", driver->max_texture_size));
    }

  glBindFramebuffer (GL_FRAMEBUFFER, 0);
  driver->bound_fbo = &driver->default_fbo;

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, 0);

  glActiveTexture (GL_TEXTURE0 + 1);
  glBindTexture (GL_TEXTURE_2D, 0);

  glBindVertexArray (0);
  glUseProgram (0);

  glActiveTexture (GL_TEXTURE0);
}

void
gsk_gl_driver_end_frame (GskGLDriver *driver)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (driver));
  g_return_if_fail (driver->in_frame);

  glBindTexture (GL_TEXTURE_2D, 0);
  glUseProgram (0);
  glBindVertexArray (0);

  driver->bound_source_texture = NULL;
  driver->bound_mask_texture = NULL;
  driver->bound_vao = NULL;
  driver->bound_fbo = NULL;

  driver->default_fbo.fbo_id = 0;

  GSK_NOTE (OPENGL,
            g_print ("*** Frame end: textures=%d, vaos=%d\n",
                     g_hash_table_size (driver->textures),
                     g_hash_table_size (driver->vaos)));

  driver->in_frame = FALSE;
}

int
gsk_gl_driver_collect_textures (GskGLDriver *driver)
{
  GHashTableIter iter;
  gpointer value_p = NULL;
  int old_size;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), 0);
  g_return_val_if_fail (!driver->in_frame, 0);

  old_size = g_hash_table_size (driver->textures);

  g_hash_table_iter_init (&iter, driver->textures);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Texture *t = value_p;

      if (t->user)
        continue;

      if (t->in_use)
        {
          t->in_use = FALSE;
          g_clear_pointer (&t->fbos, g_array_unref);
        }
      else
        g_hash_table_iter_remove (&iter);
    }

  return old_size - g_hash_table_size (driver->textures);
}

int
gsk_gl_driver_collect_vaos (GskGLDriver *driver)
{
  GHashTableIter iter;
  gpointer value_p = NULL;
  int old_size;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), 0);
  g_return_val_if_fail (!driver->in_frame, 0);

  old_size = g_hash_table_size (driver->vaos);

  g_hash_table_iter_init (&iter, driver->vaos);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Vao *v = value_p;

      if (v->in_use)
        v->in_use = FALSE;
      else
        g_hash_table_iter_remove (&iter);
    }

  return old_size - g_hash_table_size (driver->vaos);
}

int
gsk_gl_driver_get_max_texture_size (GskGLDriver *driver)
{
  if (driver->max_texture_size < 0)
    {
      if (gdk_gl_context_get_use_es (driver->gl_context))
        return 2048;

      return 1024;
    }

  return driver->max_texture_size;
}

static Texture *
gsk_gl_driver_get_texture (GskGLDriver *driver,
                           int          texture_id)
{
  Texture *t;

  if (g_hash_table_lookup_extended (driver->textures, GINT_TO_POINTER (texture_id), NULL, (gpointer *) &t))
    return t;

  return NULL;
}

static Vao *
gsk_gl_driver_get_vao (GskGLDriver *driver,
                       int          vao_id)
{
  Vao *v;

  if (g_hash_table_lookup_extended (driver->vaos, GINT_TO_POINTER (vao_id), NULL, (gpointer *) &v))
    return v;

  return NULL;
}

static Fbo *
gsk_gl_driver_get_fbo (GskGLDriver *driver,
                       int          texture_id)
{
  Texture *t = gsk_gl_driver_get_texture (driver, texture_id);

  if (t->fbos == NULL)
    return &driver->default_fbo;

  return &g_array_index (t->fbos, Fbo, 0);
}

static Texture *
find_texture_by_size (GHashTable *textures,
                      int         width,
                      int         height)
{
  GHashTableIter iter;
  gpointer value_p = NULL;

  g_hash_table_iter_init (&iter, textures);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Texture *t = value_p;

      if (t->width == width && t->height == height)
        return t;
    }

  return NULL;
}

static Texture *
create_texture (GskGLDriver *driver,
                int          width,
                int          height)
{
  guint texture_id;
  Texture *t;

  if (width >= driver->max_texture_size ||
      height >= driver->max_texture_size)
    {
      g_critical ("Texture %d x %d is bigger than supported texture limit of %d; clipping...",
                  width, height,
                  driver->max_texture_size);

      width = MIN (width, driver->max_texture_size);
      height = MIN (height, driver->max_texture_size);
    }

  t = find_texture_by_size (driver->textures, width, height);
  if (t != NULL && !t->in_use && t->user == NULL)
    {
      GSK_NOTE (OPENGL, g_print ("Reusing Texture(%d) for size %dx%d\n",
                                 t->texture_id, t->width, t->height));
      t->in_use = TRUE;
      return t;
    }

  glGenTextures (1, &texture_id);

  t = texture_new ();
  t->texture_id = texture_id;
  t->width = width;
  t->height = height;
  t->min_filter = GL_NEAREST;
  t->mag_filter = GL_NEAREST;
  t->in_use = TRUE;
  g_hash_table_insert (driver->textures, GINT_TO_POINTER (texture_id), t);

  return t;
}

static void
gsk_gl_driver_release_texture (gpointer data)
{
  Texture *t = data;

  t->user = NULL;
}

int
gsk_gl_driver_get_texture_for_texture (GskGLDriver *driver,
                                       GskTexture  *texture,
                                       int          min_filter,
                                       int          mag_filter)
{
  Texture *t;
  cairo_surface_t *surface;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);
  g_return_val_if_fail (GSK_IS_TEXTURE (texture), -1);

  t = gsk_texture_get_render_data (texture, driver);

  if (t)
    {
      if (t->min_filter == min_filter && t->mag_filter == mag_filter)
        return t->texture_id;
    }
  
  t = create_texture (driver, gsk_texture_get_width (texture), gsk_texture_get_height (texture));

  if (gsk_texture_set_render_data (texture, driver, t, gsk_gl_driver_release_texture))
    t->user = texture;

  surface = gsk_texture_download_surface (texture);
  gsk_gl_driver_bind_source_texture (driver, t->texture_id);
  gsk_gl_driver_init_texture_with_surface (driver,
                                           t->texture_id,
                                           surface,
                                           min_filter,
                                           mag_filter);
  cairo_surface_destroy (surface);

  return t->texture_id;
}

int
gsk_gl_driver_create_texture (GskGLDriver *driver,
                              int          width,
                              int          height)
{
  Texture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);

  t = create_texture (driver, width, height);

  return t->texture_id;
}

static Vao *
find_vao (GHashTable    *vaos,
          int            position_id,
          int            uv_id,
          int            n_quads,
          GskQuadVertex *quads)
{
  GHashTableIter iter;
  gpointer value_p = NULL;

  g_hash_table_iter_init (&iter, vaos);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Vao *v = value_p;

      if (v->position_id != position_id || v->uv_id != uv_id)
        continue;

      if (v->n_quads != n_quads)
        continue;

      if (memcmp (v->quads, quads, sizeof (GskQuadVertex) * n_quads) == 0)
        return v;
    }

  return NULL;
}

int
gsk_gl_driver_create_vao_for_quad (GskGLDriver   *driver,
                                   int            position_id,
                                   int            uv_id,
                                   int            n_quads,
                                   GskQuadVertex *quads)

{
  GLuint vao_id, buffer_id;
  Vao *v;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);
  g_return_val_if_fail (driver->in_frame, -1);

  v = find_vao (driver->vaos, position_id, uv_id, n_quads, quads);
  if (v != NULL && !v->in_use)
    {
      GSK_NOTE (OPENGL, g_print ("Reusing VAO(%d)\n", v->vao_id));
      v->in_use = TRUE;
      return v->vao_id;
    }

  glGenVertexArrays (1, &vao_id);
  glBindVertexArray (vao_id);

  glGenBuffers (1, &buffer_id);
  glBindBuffer (GL_ARRAY_BUFFER, buffer_id);
  glBufferData (GL_ARRAY_BUFFER, sizeof (GskQuadVertex) * n_quads, quads, GL_STATIC_DRAW);

  glEnableVertexAttribArray (position_id);
  glVertexAttribPointer (position_id, 2, GL_FLOAT, GL_FALSE,
                         sizeof (GskQuadVertex),
                         (void *) G_STRUCT_OFFSET (GskQuadVertex, position));

  glEnableVertexAttribArray (uv_id);
  glVertexAttribPointer (uv_id, 2, GL_FLOAT, GL_FALSE,
                         sizeof (GskQuadVertex),
                         (void *) G_STRUCT_OFFSET (GskQuadVertex, uv));

  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glBindVertexArray (0);

  v = vao_new ();
  v->vao_id = vao_id;
  v->buffer_id =  buffer_id;
  v->position_id = position_id;
  v->uv_id = uv_id;
  v->n_quads = n_quads;
  v->quads = g_memdup (quads, sizeof (GskQuadVertex) * n_quads);
  v->in_use = TRUE;
  g_hash_table_insert (driver->vaos, GINT_TO_POINTER (vao_id), v);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (OPENGL))
    {
      int i;
      g_print ("New VAO(%d) for quad[%d] : {\n", v->vao_id, n_quads);
      for (i = 0; i < n_quads; i++)
        {
          g_print ("  { x:%.2f, y:%.2f } { u:%.2f, v:%.2f }\n",
                  quads[i].position[0], quads[i].position[1],
                  quads[i].uv[0], quads[i].uv[1]);
        }
      g_print ("}\n");
    }
#endif

  return vao_id;
}

int
gsk_gl_driver_create_render_target (GskGLDriver *driver,
                                    int          texture_id,
                                    gboolean     add_depth_buffer,
                                    gboolean     add_stencil_buffer)
{
  GLuint fbo_id, depth_stencil_buffer_id;
  Texture *t;
  Fbo f;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);
  g_return_val_if_fail (driver->in_frame, -1);

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    return -1;

  if (t->fbos == NULL)
    {
      t->fbos = g_array_new (FALSE, FALSE, sizeof (Fbo));
      g_array_set_clear_func (t->fbos, fbo_clear);
    }

  glGenFramebuffers (1, &fbo_id);
  glBindFramebuffer (GL_FRAMEBUFFER, fbo_id);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->texture_id, 0);

  if (add_depth_buffer || add_stencil_buffer)
    glGenRenderbuffersEXT (1, &depth_stencil_buffer_id);
  else
    depth_stencil_buffer_id = 0;

  glBindRenderbuffer (GL_RENDERBUFFER, depth_stencil_buffer_id);

  if (add_depth_buffer || add_stencil_buffer)
    {
      if (add_stencil_buffer)
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, t->width, t->height);
      else
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t->width, t->height);

      if (add_depth_buffer)
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_RENDERBUFFER, depth_stencil_buffer_id);

      if (add_stencil_buffer)
        glFramebufferRenderbufferEXT (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, depth_stencil_buffer_id);
    }

  f.fbo_id = fbo_id;
  f.depth_stencil_id = depth_stencil_buffer_id;

  g_array_append_val (t->fbos, f);

  glBindFramebuffer (GL_FRAMEBUFFER, driver->default_fbo.fbo_id);

  return fbo_id;
}

void
gsk_gl_driver_bind_source_texture (GskGLDriver *driver,
                                   int          texture_id)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (driver));
  g_return_if_fail (driver->in_frame);

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    {
      g_critical ("No texture %d found.", texture_id);
      return;
    }

  if (driver->bound_source_texture != t)
    {
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, t->texture_id);

      driver->bound_source_texture = t;
    }
}

void
gsk_gl_driver_bind_mask_texture (GskGLDriver *driver,
                                 int          texture_id)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (driver));
  g_return_if_fail (driver->in_frame);

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    {
      g_critical ("No texture %d found.", texture_id);
      return;
    }

  if (driver->bound_mask_texture != t)
    {
      glActiveTexture (GL_TEXTURE0 + 1);
      glBindTexture (GL_TEXTURE_2D, t->texture_id);

      glActiveTexture (GL_TEXTURE0);

      driver->bound_mask_texture = t;
    }
}

void
gsk_gl_driver_bind_vao (GskGLDriver *driver,
                        int          vao_id)
{
  Vao *v;

  g_return_if_fail (GSK_IS_GL_DRIVER (driver));
  g_return_if_fail (driver->in_frame);

  v = gsk_gl_driver_get_vao (driver, vao_id);
  if (v == NULL)
    {
      g_critical ("No VAO %d found.", vao_id);
      return;
    }

  if (driver->bound_vao != v)
    {
      glBindVertexArray (v->vao_id);
      glBindBuffer (GL_ARRAY_BUFFER, v->buffer_id);
      glEnableVertexAttribArray (v->position_id);
      glEnableVertexAttribArray (v->uv_id);

      driver->bound_vao = v;
    }
}

gboolean
gsk_gl_driver_bind_render_target (GskGLDriver *driver,
                                  int          texture_id)
{
  int status;
  Fbo *f;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), FALSE);
  g_return_val_if_fail (driver->in_frame, FALSE);

  if (texture_id == 0)
    {
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      driver->bound_fbo = &driver->default_fbo;
      goto out;
    }

  f = gsk_gl_driver_get_fbo (driver, texture_id);
  if (f == NULL)
    {
      g_critical ("No render target associated to texture %d found.", texture_id);
      return FALSE;
    }

  if (f != driver->bound_fbo)
    {
      glBindFramebuffer (GL_FRAMEBUFFER, f->fbo_id);

      driver->bound_fbo = f;
    }

out:
  status = glCheckFramebufferStatus (GL_FRAMEBUFFER);

  return status == GL_FRAMEBUFFER_COMPLETE;
}

void
gsk_gl_driver_destroy_texture (GskGLDriver *driver,
                               int          texture_id)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (driver));

  g_hash_table_remove (driver->textures, GINT_TO_POINTER (texture_id));
}

void
gsk_gl_driver_destroy_vao (GskGLDriver *driver,
                           int          vao_id)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (driver));

  g_hash_table_remove (driver->vaos, GINT_TO_POINTER (vao_id));
}

static void
gsk_gl_driver_set_texture_parameters (GskGLDriver *driver,
                                      int          min_filter,
                                      int          mag_filter)
{
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void
gsk_gl_driver_init_texture_empty (GskGLDriver *driver,
                                  int          texture_id)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (driver));

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    {
      g_critical ("No texture %d found.", texture_id);
      return;
    }

  if (!(driver->bound_source_texture == t || driver->bound_mask_texture == t))
    {
      g_critical ("You must bind the texture before initializing it.");
      return;
    }

  gsk_gl_driver_set_texture_parameters (driver, t->min_filter, t->mag_filter);

  if (gdk_gl_context_get_use_es (driver->gl_context))
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, t->width, t->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  glBindTexture (GL_TEXTURE_2D, 0);
}

void
gsk_gl_driver_init_texture_with_surface (GskGLDriver     *driver,
                                         int              texture_id,
                                         cairo_surface_t *surface,
                                         int              min_filter,
                                         int              mag_filter)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (driver));

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    {
      g_critical ("No texture %d found.", texture_id);
      return;
    }

  if (!(driver->bound_source_texture == t || driver->bound_mask_texture == t))
    {
      g_critical ("You must bind the texture before initializing it.");
      return;
    }

  gsk_gl_driver_set_texture_parameters (driver, min_filter, mag_filter);

  gdk_cairo_surface_upload_to_gl (surface, GL_TEXTURE_2D, t->width, t->height, NULL);

  t->min_filter = min_filter;
  t->mag_filter = mag_filter;

  if (t->min_filter != GL_NEAREST)
    glGenerateMipmap (GL_TEXTURE_2D);

  glBindTexture (GL_TEXTURE_2D, 0);
}
