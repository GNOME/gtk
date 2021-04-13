#include "config.h"

#include "gskgldriverprivate.h"

#include "gskdebugprivate.h"
#include "gskprofilerprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkgltextureprivate.h"
#include "gdkmemorytextureprivate.h"

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

  /* TODO: Make this optional and not for every texture... */
  TextureSlice *slices;
  guint n_slices;
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

  GHashTable *textures;         /* texture_id -> Texture */
  GHashTable *pointer_textures; /* pointer -> texture_id */

  const Texture *bound_source_texture;

  int max_texture_size;

  gboolean in_frame : 1;
};

G_DEFINE_TYPE (GskGLDriver, gsk_gl_driver, G_TYPE_OBJECT)

static void
upload_gdk_texture (GdkTexture      *source_texture,
                    int              target,
                    int              x_offset,
                    int              y_offset,
                    int              width,
                    int              height)
{
  cairo_surface_t *surface = NULL;
  GdkMemoryFormat data_format;
  const guchar *data;
  gsize data_stride;
  gsize bpp;

  g_return_if_fail (source_texture != NULL);
  g_return_if_fail (x_offset + width <= gdk_texture_get_width (source_texture));
  g_return_if_fail (y_offset + height <= gdk_texture_get_height (source_texture));

  /* Note: GdkGLTextures are already handled before we reach this and reused as-is */

  if (GDK_IS_MEMORY_TEXTURE (source_texture))
    {
      GdkMemoryTexture *memory_texture = GDK_MEMORY_TEXTURE (source_texture);
      data = gdk_memory_texture_get_data (memory_texture);
      data_format = gdk_memory_texture_get_format (memory_texture);
      data_stride = gdk_memory_texture_get_stride (memory_texture);
    }
  else
    {
      /* Fall back to downloading to a surface */
      surface = gdk_texture_download_surface (source_texture);
      cairo_surface_flush (surface);
      data = cairo_image_surface_get_data (surface);
      data_format = GDK_MEMORY_DEFAULT;
      data_stride = cairo_image_surface_get_stride (surface);
    }

  bpp = gdk_memory_format_bytes_per_pixel (data_format);

  gdk_gl_context_upload_texture (gdk_gl_context_get_current (),
                                 data + x_offset * bpp + y_offset * data_stride,
                                 width, height, data_stride,
                                 data_format, target);

  if (surface)
    cairo_surface_destroy (surface);
}

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
  guint i;

  if (t->user)
    gdk_texture_clear_render_data (t->user);

  if (t->fbo.fbo_id != 0)
    fbo_clear (&t->fbo);

  if (t->texture_id != 0)
    {
      glDeleteTextures (1, &t->texture_id);
    }
  else
    {
      g_assert_cmpint (t->n_slices, >, 0);

      for (i = 0; i < t->n_slices; i ++)
        glDeleteTextures (1, &t->slices[i].texture_id);
    }

  g_slice_free (Texture, t);
}

static void
gsk_gl_driver_set_texture_parameters (GskGLDriver *self,
                                      int          min_filter,
                                      int          mag_filter)
{
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void
gsk_gl_driver_finalize (GObject *gobject)
{
  GskGLDriver *self = GSK_GL_DRIVER (gobject);

  gdk_gl_context_make_current (self->gl_context);

  g_clear_pointer (&self->textures, g_hash_table_unref);
  g_clear_pointer (&self->pointer_textures, g_hash_table_unref);
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
      GSK_NOTE (OPENGL, g_message ("GL max texture size: %d", self->max_texture_size));
    }

  glBindFramebuffer (GL_FRAMEBUFFER, 0);

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, 0);

  glActiveTexture (GL_TEXTURE0 + 1);
  glBindTexture (GL_TEXTURE_2D, 0);

  glBindVertexArray (0);
  glUseProgram (0);

  glActiveTexture (GL_TEXTURE0);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_reset (self->profiler);
#endif
}

gboolean
gsk_gl_driver_in_frame (GskGLDriver *self)
{
  return self->in_frame;
}

void
gsk_gl_driver_end_frame (GskGLDriver *self)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (self->in_frame);

  self->bound_source_texture = NULL;

  self->default_fbo.fbo_id = 0;

#ifdef G_ENABLE_DEBUG
  GSK_NOTE (OPENGL,
            g_message ("Textures created: %" G_GINT64_FORMAT "\n"
                     " Textures reused: %" G_GINT64_FORMAT "\n"
                     " Surface uploads: %" G_GINT64_FORMAT,
                     gsk_profiler_counter_get (self->profiler, self->counters.created_textures),
                     gsk_profiler_counter_get (self->profiler, self->counters.reused_textures),
                     gsk_profiler_counter_get (self->profiler, self->counters.surface_uploads)));
#endif

  GSK_NOTE (OPENGL,
            g_message ("*** Frame end: textures=%d",
                     g_hash_table_size (self->textures)));

  self->in_frame = FALSE;
}

int
gsk_gl_driver_collect_textures (GskGLDriver *self)
{
  GHashTableIter iter;
  gpointer value_p = NULL;
  int old_size;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), 0);
  g_return_val_if_fail (!self->in_frame, 0);

  old_size = g_hash_table_size (self->textures);

  g_hash_table_iter_init (&iter, self->textures);
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
        {
          /* Remove from self->pointer_textures. */
          /* TODO: Is there a better way for this? */
          if (self->pointer_textures)
            {
              GHashTableIter pointer_iter;
              gpointer value;
              gpointer p;

              g_hash_table_iter_init (&pointer_iter, self->pointer_textures);
              while (g_hash_table_iter_next (&pointer_iter, &p, &value))
                {
                  if (GPOINTER_TO_INT (value) == t->texture_id)
                    {
                      g_hash_table_iter_remove (&pointer_iter);
                      break;
                    }
                }
            }

          g_hash_table_iter_remove (&iter);
        }
    }

  return old_size - g_hash_table_size (self->textures);
}


GdkGLContext *
gsk_gl_driver_get_gl_context (GskGLDriver *self)
{
  return self->gl_context;
}

int
gsk_gl_driver_get_max_texture_size (GskGLDriver *self)
{
  if (self->max_texture_size < 0)
    {
      if (gdk_gl_context_get_use_es (self->gl_context))
        return 2048;

      return 1024;
    }

  return self->max_texture_size;
}

static Texture *
gsk_gl_driver_get_texture (GskGLDriver *self,
                           int          texture_id)
{
  Texture *t;

  if (g_hash_table_lookup_extended (self->textures, GINT_TO_POINTER (texture_id), NULL, (gpointer *) &t))
    return t;

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

  if (width > self->max_texture_size ||
      height > self->max_texture_size)
    {
      g_critical ("Texture %d x %d is bigger than supported texture limit of %d; clipping...",
                  width, height,
                  self->max_texture_size);

      width = MIN (width, self->max_texture_size);
      height = MIN (height, self->max_texture_size);
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
#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (self->profiler, self->counters.created_textures);
#endif

  return t;
}

static void
gsk_gl_driver_release_texture (gpointer data)
{
  Texture *t = data;

  t->user = NULL;
}

void
gsk_gl_driver_slice_texture (GskGLDriver   *self,
                             GdkTexture    *texture,
                             TextureSlice **out_slices,
                             guint         *out_n_slices)
{
  const int max_texture_size = gsk_gl_driver_get_max_texture_size (self) / 4; // XXX Too much?
  const int cols = (texture->width / max_texture_size) + 1;
  const int rows = (texture->height / max_texture_size) + 1;
  int col, row;
  int x = 0, y = 0; /* Position in the texture */
  TextureSlice *slices;
  Texture *tex;

  g_assert (texture->width > max_texture_size || texture->height > max_texture_size);


  tex = gdk_texture_get_render_data (texture, self);

  if (tex != NULL)
    {
      g_assert (tex->n_slices > 0);
      *out_slices = tex->slices;
      *out_n_slices = tex->n_slices;
      return;
    }

  slices = g_new0 (TextureSlice, cols * rows);

  for (col = 0; col < cols; col ++)
    {
      const int slice_width = MIN (max_texture_size, texture->width - x);

      for (row = 0; row < rows; row ++)
        {
          const int slice_height = MIN (max_texture_size, texture->height - y);
          const int slice_index = (col * rows) + row;
          guint texture_id;

          glGenTextures (1, &texture_id);

#ifdef G_ENABLE_DEBUG
          gsk_profiler_counter_inc (self->profiler, self->counters.created_textures);
#endif
          glBindTexture (GL_TEXTURE_2D, texture_id);
          gsk_gl_driver_set_texture_parameters (self, GL_NEAREST, GL_NEAREST);
          upload_gdk_texture (texture, GL_TEXTURE_2D, x, y, slice_width, slice_height);

#ifdef G_ENABLE_DEBUG
          gsk_profiler_counter_inc (self->profiler, self->counters.surface_uploads);
#endif

          slices[slice_index].rect = (GdkRectangle){x, y, slice_width, slice_height};
          slices[slice_index].texture_id = texture_id;

          y += slice_height;
        }

      y = 0;
      x += slice_width;
    }

  /* Allocate one Texture for the entire thing. */
  tex = texture_new ();
  tex->width = texture->width;
  tex->height = texture->height;
  tex->min_filter = GL_NEAREST;
  tex->mag_filter = GL_NEAREST;
  tex->in_use = TRUE;
  tex->slices = slices;
  tex->n_slices = cols * rows;

  /* Use texture_free as destroy notify here since we are not inserting this Texture
   * into self->textures! */
  gdk_texture_set_render_data (texture, self, tex, texture_free);

  *out_slices = slices;
  *out_n_slices = cols * rows;
}

int
gsk_gl_driver_get_texture_for_texture (GskGLDriver *self,
                                       GdkTexture  *texture,
                                       int          min_filter,
                                       int          mag_filter)
{
  Texture *t;
  GdkTexture *downloaded_texture = NULL;
  GdkTexture *source_texture;

  if (GDK_IS_GL_TEXTURE (texture))
    {
      GdkGLContext *texture_context = gdk_gl_texture_get_context ((GdkGLTexture *)texture);
      GdkGLContext *shared_context = gdk_gl_context_get_shared_context (self->gl_context);

      if (texture_context == self->gl_context ||
          (gdk_gl_context_get_shared_context (texture_context) == shared_context && shared_context != NULL))
        {
          /* A GL texture from the same GL context is a simple task... */
          return gdk_gl_texture_get_id ((GdkGLTexture *)texture);
        }
      else
        {
          cairo_surface_t *surface;

          /* In this case, we have to temporarily make the texture's context the current one,
           * download its data into our context and then create a texture from it. */
          if (texture_context)
            gdk_gl_context_make_current (texture_context);

          surface = gdk_texture_download_surface (texture);
          downloaded_texture = gdk_texture_new_for_surface (surface);
          cairo_surface_destroy (surface);

          gdk_gl_context_make_current (self->gl_context);

          source_texture = downloaded_texture;
        }
    }
  else
    {
      t = gdk_texture_get_render_data (texture, self);

      if (t)
        {
          if (t->min_filter == min_filter && t->mag_filter == mag_filter)
            return t->texture_id;
        }

      source_texture = texture;
    }

  t = create_texture (self, gdk_texture_get_width (texture), gdk_texture_get_height (texture));

  if (gdk_texture_set_render_data (texture, self, t, gsk_gl_driver_release_texture))
    t->user = texture;

  gsk_gl_driver_bind_source_texture (self, t->texture_id);
  gsk_gl_driver_init_texture (self,
                              t->texture_id,
                              source_texture,
                              min_filter,
                              mag_filter);
  gdk_gl_context_label_object_printf (self->gl_context, GL_TEXTURE, t->texture_id,
                                      "GdkTexture<%p> %d", texture, t->texture_id);

  if (downloaded_texture)
    g_object_unref (downloaded_texture);

  return t->texture_id;
}

static guint
texture_key_hash (gconstpointer v)
{
  const GskTextureKey *k = (GskTextureKey *)v;

  return GPOINTER_TO_UINT (k->pointer)
         + (guint)(k->scale_x * 100)
         + (guint)(k->scale_y * 100)
         + (guint)k->filter * 2 +
         + (guint)k->pointer_is_child;
}

static gboolean
texture_key_equal (gconstpointer v1, gconstpointer v2)
{
  const GskTextureKey *k1 = (GskTextureKey *)v1;
  const GskTextureKey *k2 = (GskTextureKey *)v2;

  return k1->pointer == k2->pointer &&
         k1->scale_x == k2->scale_x &&
         k1->scale_y == k2->scale_y &&
         k1->filter == k2->filter &&
         k1->pointer_is_child == k2->pointer_is_child &&
         (!k1->pointer_is_child || graphene_rect_equal (&k1->parent_rect, &k2->parent_rect));
}

int
gsk_gl_driver_get_texture_for_key (GskGLDriver   *self,
                                   GskTextureKey *key)
{
  int id = 0;

  if (G_UNLIKELY (self->pointer_textures == NULL))
    self->pointer_textures = g_hash_table_new_full (texture_key_hash, texture_key_equal, g_free, NULL);

  id = GPOINTER_TO_INT (g_hash_table_lookup (self->pointer_textures, key));

  if (id != 0)
    {
      Texture *t;

      t = g_hash_table_lookup (self->textures, GINT_TO_POINTER (id));

      if (t != NULL)
        t->in_use = TRUE;
    }

  return id;
}

void
gsk_gl_driver_set_texture_for_key (GskGLDriver   *self,
                                   GskTextureKey *key,
                                   int            texture_id)
{
  GskTextureKey *k;

  if (G_UNLIKELY (self->pointer_textures == NULL))
    self->pointer_textures = g_hash_table_new_full (texture_key_hash, texture_key_equal, g_free, NULL);

  k = g_new (GskTextureKey, 1);
  *k = *key;

  g_hash_table_insert (self->pointer_textures, k, GINT_TO_POINTER (texture_id));
}

int
gsk_gl_driver_create_texture (GskGLDriver *self,
                              float        width,
                              float        height)
{
  Texture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), -1);

  t = create_texture (self, width, height);

  return t->texture_id;
}

void
gsk_gl_driver_create_render_target (GskGLDriver *self,
                                    int          width,
                                    int          height,
                                    int          min_filter,
                                    int          mag_filter,
                                    int         *out_texture_id,
                                    int         *out_render_target_id)
{
  GLuint fbo_id;
  Texture *texture;

  g_return_if_fail (self->in_frame);

  texture = create_texture (self, width, height);
  gsk_gl_driver_bind_source_texture (self, texture->texture_id);
  gsk_gl_driver_init_texture_empty (self, texture->texture_id, min_filter, mag_filter);

  glGenFramebuffers (1, &fbo_id);
  glBindFramebuffer (GL_FRAMEBUFFER, fbo_id);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->texture_id, 0);

#if 0
  if (add_depth_buffer || add_stencil_buffer)
    {
      glGenRenderbuffersEXT (1, &depth_stencil_buffer_id);
      gdk_gl_context_label_object_printf (self->gl_context, GL_RENDERBUFFER, depth_stencil_buffer_id,
                                          "%s buffer for %d", add_depth_buffer ? "Depth" : "Stencil", texture_id);
    }
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
  texture->fbo.depth_stencil_id = depth_stencil_buffer_id;
    }
#endif

  texture->fbo.fbo_id = fbo_id;

  g_assert_cmphex (glCheckFramebufferStatus (GL_FRAMEBUFFER), ==, GL_FRAMEBUFFER_COMPLETE);

  glBindFramebuffer (GL_FRAMEBUFFER, self->default_fbo.fbo_id);

  *out_texture_id = texture->texture_id;
  *out_render_target_id = fbo_id;
}

/* Mark the texture permanent, meaning it won'e be reused by the GLDriver.
 * E.g. to store it in some other cache. */
void
gsk_gl_driver_mark_texture_permanent (GskGLDriver *self,
                                      int          texture_id)
{
  Texture *t = gsk_gl_driver_get_texture (self, texture_id);

  g_assert (t != NULL);

  t->permanent = TRUE;
}

void
gsk_gl_driver_bind_source_texture (GskGLDriver *self,
                                   int          texture_id)
{
  Texture *t;

  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (self->in_frame);

  t = gsk_gl_driver_get_texture (self, texture_id);
  if (t == NULL)
    {
      g_critical ("No texture %d found.", texture_id);
      return;
    }

  if (self->bound_source_texture != t)
    {
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, t->texture_id);

      self->bound_source_texture = t;
    }
}

void
gsk_gl_driver_destroy_texture (GskGLDriver *self,
                               int          texture_id)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (self));

  g_hash_table_remove (self->textures, GINT_TO_POINTER (texture_id));
}


void
gsk_gl_driver_init_texture_empty (GskGLDriver *self,
                                  int          texture_id,
                                  int          min_filter,
                                  int          mag_filter)
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

  t->min_filter = min_filter;
  t->mag_filter = mag_filter;

  gsk_gl_driver_set_texture_parameters (self, t->min_filter, t->mag_filter);

  if (gdk_gl_context_get_use_es (self->gl_context))
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, t->width, t->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  glBindTexture (GL_TEXTURE_2D, 0);
}

static gboolean
filter_uses_mipmaps (int filter)
{
  return filter != GL_NEAREST && filter != GL_LINEAR;
}

void
gsk_gl_driver_init_texture (GskGLDriver     *self,
                            int              texture_id,
                            GdkTexture      *texture,
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

  upload_gdk_texture (texture, GL_TEXTURE_2D, 0, 0, t->width, t->height);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (self->profiler, self->counters.surface_uploads);
#endif

  t->min_filter = min_filter;
  t->mag_filter = mag_filter;

  if (filter_uses_mipmaps (t->min_filter))
    glGenerateMipmap (GL_TEXTURE_2D);
}
