#include "config.h"

#include "gskgldriverprivate.h"

#include "gskdebugprivate.h"
#include "gskprofilerprivate.h"
#include "gdk/gdktextureprivate.h"

#include <gdk/gdk.h>
#include <epoxy/gl.h>

 typedef struct {
  GLuint fbo_id;
  GLuint depth_stencil_id;
} Fbo;

typedef struct {
  GLuint texture_id;
  int width;
  int height;
  GLuint min_filter;
  GLuint mag_filter;
  Fbo fbo;
  GdkTexture *user;
  guint in_use : 1;
  guint permanent : 1;
} Texture;

struct _GskGLDriver
{
  GObject parent_instance;

  GdkGLContext *gl_context;
  GskProfiler *profiler;
  struct {
    GQuark created_textures;
    GQuark reused_textures;
    GQuark surface_uploads;
  } counters;

  Fbo default_fbo;

  GHashTable *textures;

  const Texture *bound_source_texture;
  const Fbo *bound_fbo;

  int max_texture_size;

  gboolean in_frame : 1;
};

G_DEFINE_TYPE (GskGLDriver, gsk_gl_driver, G_TYPE_OBJECT)

static Texture *
texture_new (void)
{
  return g_slice_new0 (Texture);
}

static inline void
fbo_clear (const Fbo *f)
{
  if (f->depth_stencil_id != 0)
    glDeleteRenderbuffers (1, &f->depth_stencil_id);

  glDeleteFramebuffers (1, &f->fbo_id);
}

static void
texture_free (gpointer data)
{
  Texture *t = data;

  if (t->user)
    gdk_texture_clear_render_data (t->user);

  if (t->fbo.fbo_id != 0)
    fbo_clear (&t->fbo);

  glDeleteTextures (1, &t->texture_id);
  g_slice_free (Texture, t);
}


static void
gsk_gl_driver_finalize (GObject *gobject)
{
  GskGLDriver *self = GSK_GL_DRIVER (gobject);

  gdk_gl_context_make_current (self->gl_context);

  g_clear_pointer (&self->textures, g_hash_table_unref);
  g_clear_object (&self->profiler);

  if (self->gl_context == gdk_gl_context_get_current ())
    gdk_gl_context_clear_current ();

  G_OBJECT_CLASS (gsk_gl_driver_parent_class)->finalize (gobject);
}

static void
gsk_gl_driver_class_init (GskGLDriverClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gsk_gl_driver_finalize;
}

static void
gsk_gl_driver_init (GskGLDriver *self)
{
  self->textures = g_hash_table_new_full (NULL, NULL, NULL, texture_free);

  self->max_texture_size = -1;

#ifdef G_ENABLE_DEBUG
  self->profiler = gsk_profiler_new ();
  self->counters.created_textures = gsk_profiler_add_counter (self->profiler,
                                                              "created_textures",
                                                              "Textures created this frame",
                                                              TRUE);
  self->counters.reused_textures = gsk_profiler_add_counter (self->profiler,
                                                             "reused_textures",
                                                             "Textures reused this frame",
                                                             TRUE);
  self->counters.surface_uploads = gsk_profiler_add_counter (self->profiler,
                                                             "surface_uploads",
                                                             "Texture uploads from surfaces this frame",
                                                             TRUE);
#endif
}

GskGLDriver *
gsk_gl_driver_new (GdkGLContext *context)
{
  GskGLDriver *self;
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  self = (GskGLDriver *) g_object_new (GSK_TYPE_GL_DRIVER, NULL);
  self->gl_context = context;

  return self;
}

void
gsk_gl_driver_begin_frame (GskGLDriver *self)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (!self->in_frame);

  self->in_frame = TRUE;

  if (self->max_texture_size < 0)
    {
      glGetIntegerv (GL_MAX_TEXTURE_SIZE, (GLint *) &self->max_texture_size);
      GSK_NOTE (OPENGL, g_print ("GL max texture size: %d\n", self->max_texture_size));
    }

  glBindFramebuffer (GL_FRAMEBUFFER, 0);
  self->bound_fbo = &self->default_fbo;

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, 0);

  glActiveTexture (GL_TEXTURE0 + 1);
  glBindTexture (GL_TEXTURE_2D, 0);

  glBindVertexArray (0);
  glUseProgram (0);

  glActiveTexture (GL_TEXTURE0);

  gsk_profiler_reset (self->profiler);
}

void
gsk_gl_driver_end_frame (GskGLDriver *self)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (self->in_frame);

  self->bound_source_texture = NULL;
  self->bound_fbo = NULL;

  self->default_fbo.fbo_id = 0;

  GSK_NOTE (OPENGL,
            g_print ("Textures created: %ld\n"
                     " Textures reused: %ld\n"
                     " Surface uploads: %ld\n",
                     gsk_profiler_counter_get (self->profiler, self->counters.created_textures),
                     gsk_profiler_counter_get (self->profiler, self->counters.reused_textures),
                     gsk_profiler_counter_get (self->profiler, self->counters.surface_uploads)));
  GSK_NOTE (OPENGL,
            g_print ("*** Frame end: textures=%d\n",
                     g_hash_table_size (self->textures)));

  self->in_frame = FALSE;
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

      if (t->user || t->permanent)
        continue;

      if (t->in_use)
        {
          t->in_use = FALSE;

          if (t->fbo.fbo_id != 0)
            {
              fbo_clear (&t->fbo);
              t->fbo.fbo_id = 0;
            }
        }
      else
        g_hash_table_iter_remove (&iter);
    }

  return old_size - g_hash_table_size (driver->textures);
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

static const Fbo *
gsk_gl_driver_get_fbo (GskGLDriver *driver,
                       int          texture_id)
{
  Texture *t = gsk_gl_driver_get_texture (driver, texture_id);

  if (t->fbo.fbo_id == 0)
    return &driver->default_fbo;

  return &t->fbo;
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
create_texture (GskGLDriver *self,
                float        fwidth,
                float        fheight)
{
  guint texture_id;
  Texture *t;
  int width = ceilf (fwidth);
  int height = ceilf (fheight);

  g_assert (width > 0);
  g_assert (height > 0);

  if (width >= self->max_texture_size ||
      height >= self->max_texture_size)
    {
      g_critical ("Texture %d x %d is bigger than supported texture limit of %d; clipping...",
                  width, height,
                  self->max_texture_size);

      width = MIN (width, self->max_texture_size);
      height = MIN (height, self->max_texture_size);
    }

  t = find_texture_by_size (self->textures, width, height);
  if (t != NULL && !t->in_use && t->user == NULL)
    {
      GSK_NOTE (OPENGL, g_print ("Reusing Texture(%d) for size %dx%d\n",
                                 t->texture_id, t->width, t->height));
      t->in_use = TRUE;
      gsk_profiler_counter_inc (self->profiler, self->counters.reused_textures);
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
  g_hash_table_insert (self->textures, GINT_TO_POINTER (texture_id), t);
  gsk_profiler_counter_inc (self->profiler, self->counters.created_textures);

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
                                       GdkTexture  *texture,
                                       int          min_filter,
                                       int          mag_filter)
{
  Texture *t;
  cairo_surface_t *surface;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), -1);

  t = gdk_texture_get_render_data (texture, driver);

  if (t)
    {
      if (t->min_filter == min_filter && t->mag_filter == mag_filter)
        return t->texture_id;
    }

  t = create_texture (driver, gdk_texture_get_width (texture), gdk_texture_get_height (texture));

  if (gdk_texture_set_render_data (texture, driver, t, gsk_gl_driver_release_texture))
    t->user = texture;

  surface = gdk_texture_download_surface (texture);
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
gsk_gl_driver_create_permanent_texture (GskGLDriver *self,
                                        float        width,
                                        float        height)
{
  Texture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), -1);

  t = create_texture (self, width, height);
  t->permanent = TRUE;

  return t->texture_id;
}

int
gsk_gl_driver_create_texture (GskGLDriver *driver,
                              float        width,
                              float        height)
{
  Texture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);

  t = create_texture (driver, width, height);

  return t->texture_id;
}

int
gsk_gl_driver_create_render_target (GskGLDriver *driver,
                                    int          texture_id,
                                    gboolean     add_depth_buffer,
                                    gboolean     add_stencil_buffer)
{
  GLuint fbo_id, depth_stencil_buffer_id;
  Texture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), -1);
  g_return_val_if_fail (driver->in_frame, -1);

  t = gsk_gl_driver_get_texture (driver, texture_id);
  if (t == NULL)
    return -1;

  if (t->fbo.fbo_id != 0)
    fbo_clear (&t->fbo);

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

  t->fbo.fbo_id = fbo_id;
  t->fbo.depth_stencil_id = depth_stencil_buffer_id;

  g_assert_cmpint (glCheckFramebufferStatus (GL_FRAMEBUFFER), ==, GL_FRAMEBUFFER_COMPLETE);
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

gboolean
gsk_gl_driver_bind_render_target (GskGLDriver *driver,
                                  int          texture_id)
{
  int status;
  const Fbo *f;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), FALSE);
  g_return_val_if_fail (driver->in_frame, FALSE);

  if (texture_id == 0)
    {
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      driver->bound_fbo = &driver->default_fbo;
      goto out;
    }

  f = gsk_gl_driver_get_fbo (driver, texture_id);

  if (f != driver->bound_fbo)
    {
      glBindFramebuffer (GL_FRAMEBUFFER, f->fbo_id);

      driver->bound_fbo = f;
    }

out:

  if (texture_id != 0)
    {
      status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
      g_assert_cmpint (status, ==, GL_FRAMEBUFFER_COMPLETE);
    }

  return TRUE;
}

void
gsk_gl_driver_destroy_texture (GskGLDriver *driver,
                               int          texture_id)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (driver));

  g_hash_table_remove (driver->textures, GINT_TO_POINTER (texture_id));
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

  if (driver->bound_source_texture != t)
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
gsk_gl_driver_init_texture_with_surface (GskGLDriver     *self,
                                         int              texture_id,
                                         cairo_surface_t *surface,
                                         int              min_filter,
                                         int              mag_filter)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (self));

  t = gsk_gl_driver_get_texture (self, texture_id);
  if (t == NULL)
    {
      g_critical ("No texture %d found.", texture_id);
      return;
    }

  if (self->bound_source_texture != t)
    {
      g_critical ("You must bind the texture before initializing it.");
      return;
    }

  gsk_gl_driver_set_texture_parameters (self, min_filter, mag_filter);

  gdk_cairo_surface_upload_to_gl (surface, GL_TEXTURE_2D, t->width, t->height, NULL);
  gsk_profiler_counter_inc (self->profiler, self->counters.surface_uploads);

  t->min_filter = min_filter;
  t->mag_filter = mag_filter;

  if (t->min_filter != GL_NEAREST)
    glGenerateMipmap (GL_TEXTURE_2D);
}
