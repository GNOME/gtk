#include "config.h"

#include "gskgldriverprivate.h"

#include "gskdebugprivate.h"

#include <gdk/gdk.h>
#include <epoxy/gl.h>

typedef struct {
  GLuint texture_id;
  int width;
  int height;
  GLuint min_filter;
  GLuint mag_filter;
} Texture;

typedef struct {
  GLuint vao_id;
  GLuint buffer_id;
  GLuint position_id;
  GLuint uv_id;
} Vao;

struct _GskGLDriver
{
  GObject parent_instance;

  GdkGLContext *gl_context;

  GHashTable *textures;
  GArray *vaos;

  Texture *bound_source_texture;
  Texture *bound_mask_texture;
  Vao *bound_vao;

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

  glDeleteTextures (1, &t->texture_id);
  g_slice_free (Texture, t);
}

static void
vao_clear (gpointer data)
{
  Vao *v = data;

  glDeleteBuffers (1, &v->buffer_id);
  glDeleteVertexArrays (1, &v->vao_id);
}

static void
gsk_gl_driver_finalize (GObject *gobject)
{
  GskGLDriver *self = GSK_GL_DRIVER (gobject);

  g_clear_pointer (&self->textures, g_hash_table_unref);
  g_clear_pointer (&self->vaos, g_array_unref);

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

  self->vaos = g_array_new (FALSE, FALSE, sizeof (Vao));
  g_array_set_clear_func (self->vaos, vao_clear);
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

  g_hash_table_remove_all (driver->textures);
  g_array_set_size (driver->vaos, 0);

  driver->in_frame = FALSE;
}

int
gsk_gl_driver_create_texture (GskGLDriver     *driver,
                              int              width,
                              int              height,
                              int              min_filter,
                              int              mag_filter)
{
  guint texture_id;
  Texture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);

  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  t = texture_new ();
  t->texture_id = texture_id;
  t->width = width;
  t->height = height;
  t->min_filter = min_filter;
  t->mag_filter = mag_filter;
  g_hash_table_insert (driver->textures, GUINT_TO_POINTER (texture_id), t);

  return texture_id;
}

int
gsk_gl_driver_create_vao_for_quad (GskGLDriver   *driver,
                                   int            position_id,
                                   int            uv_id,
                                   int            n_quads,
                                   GskQuadVertex *quads)

{
  GLuint vao_id, buffer_id;
  Vao v;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);

  glGenVertexArrays (1, &vao_id);
  glBindVertexArray (vao_id);

  glGenBuffers (1, &buffer_id);
  glBindBuffer (GL_ARRAY_BUFFER, buffer_id);
  glBufferData (GL_ARRAY_BUFFER, n_quads, quads, GL_STATIC_DRAW);

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

  v.vao_id = vao_id;
  v.buffer_id =  buffer_id;
  v.position_id = position_id;
  v.uv_id = uv_id;
  g_array_append_val (driver->vaos, v);

  return vao_id;
}

static Texture *
gsk_gl_driver_get_texture (GskGLDriver *driver,
                           int          texture_id)
{
  return g_hash_table_lookup (driver->textures, GINT_TO_POINTER (texture_id));
}

static Vao *
gsk_gl_driver_get_vao (GskGLDriver *driver,
                       int          vao_id)
{
  int i;

  for (i = 0; i < driver->vaos->len; i++)
    {
      Vao *v = &g_array_index (driver->vaos, Vao, i);
      if (v->vao_id == vao_id)
        return v;
    }

  return NULL;
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
    return;

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
    return;

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
    return;

  if (driver->bound_vao != v)
    {
      glBindVertexArray (v->vao_id);
      glBindBuffer (GL_ARRAY_BUFFER, v->buffer_id);
      glEnableVertexAttribArray (v->position_id);
      glEnableVertexAttribArray (v->uv_id);

      driver->bound_vao = v;
    }
}

void
gsk_gl_driver_render_surface_to_texture (GskGLDriver     *driver,
                                         cairo_surface_t *surface,
                                         int              texture_id)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (driver));

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    return;

  glBindTexture (GL_TEXTURE_2D, t->texture_id);

  gdk_cairo_surface_upload_to_gl (surface, GL_TEXTURE_2D, t->width, t->height, NULL);

  if (t->min_filter != GL_NEAREST)
    glGenerateMipmap (GL_TEXTURE_2D);
}
