/*
 * Copyright © 2023 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdksubsurface-wayland-private.h"

#include "gdkmemoryformatprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkdmabuftextureprivate.h"
#include "gdksurface-wayland-private.h"
#include "gdksubsurfaceprivate.h"
#include "gdkdebugprivate.h"
#include "gdkglcontextprivate.h"
#include "gdkgltextureprivate.h"
#include "gsk/gskrectprivate.h"

#include "linux-dmabuf-unstable-v1-client-protocol.h"

G_DEFINE_TYPE (GdkWaylandSubsurface, gdk_wayland_subsurface, GDK_TYPE_SUBSURFACE)

static void
gdk_wayland_subsurface_init (GdkWaylandSubsurface *self)
{
}

static void
gdk_wayland_subsurface_finalize (GObject *object)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (object);

  g_clear_object (&self->texture);
  g_clear_pointer (&self->frame_callback, wl_callback_destroy);
  g_clear_pointer (&self->opaque_region, wl_region_destroy);
  g_clear_pointer (&self->viewport, wp_viewport_destroy);
  g_clear_pointer (&self->color, gdk_wayland_color_surface_free);
  g_clear_pointer (&self->subsurface, wl_subsurface_destroy);
  g_clear_pointer (&self->surface, wl_surface_destroy);
  g_clear_pointer (&self->bg_viewport, wp_viewport_destroy);
  g_clear_pointer (&self->bg_subsurface, wl_subsurface_destroy);
  g_clear_pointer (&self->bg_surface, wl_surface_destroy);

  G_OBJECT_CLASS (gdk_wayland_subsurface_parent_class)->finalize (object);
}

static void
shm_buffer_release (void             *data,
                    struct wl_buffer *buffer)
{
  cairo_surface_t *surface = data;

  /* Note: the wl_buffer is destroyed as cairo user data */
  cairo_surface_destroy (surface);
}

static const struct wl_buffer_listener shm_buffer_listener = {
  shm_buffer_release,
};

static struct wl_buffer *
get_shm_wl_buffer (GdkWaylandSubsurface *self,
                   GdkTexture           *texture)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SUBSURFACE (self)->parent));
  int width, height;
  cairo_surface_t *surface;
  GdkTextureDownloader *downloader;
  struct wl_buffer *buffer;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  surface = gdk_wayland_display_create_shm_surface (display, width, height, &GDK_FRACTIONAL_SCALE_INIT_INT (1));

  downloader = gdk_texture_downloader_new (texture);

  gdk_texture_downloader_download_into (downloader,
                                        cairo_image_surface_get_data (surface),
                                        cairo_image_surface_get_stride (surface));

  gdk_texture_downloader_free (downloader);

  buffer = _gdk_wayland_shm_surface_get_wl_buffer (surface);
  wl_buffer_add_listener (buffer, &shm_buffer_listener, surface);

  return buffer;
}

static void
dmabuf_buffer_release (void             *data,
                       struct wl_buffer *buffer)
{
  GdkTexture *texture = data;

  g_object_unref (texture);
  g_clear_pointer (&buffer, wl_buffer_destroy);
}

static const struct wl_buffer_listener dmabuf_buffer_listener = {
  dmabuf_buffer_release,
};

typedef struct {
  struct wl_buffer *buffer;
  gboolean done;
} CreateBufferData;

static void
params_buffer_created (void                              *data,
                       struct zwp_linux_buffer_params_v1 *params,
                       struct wl_buffer                  *buffer)
{
  CreateBufferData *cd = data;

  cd->buffer = buffer;
  cd->done = TRUE;
}

static void
params_buffer_failed (void                              *data,
                      struct zwp_linux_buffer_params_v1 *params)
{
  CreateBufferData *cd = data;

  cd->buffer = NULL;
  cd->done = TRUE;
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
  params_buffer_created,
  params_buffer_failed,
};

static struct wl_buffer *
get_dmabuf_wl_buffer (GdkWaylandSubsurface            *self,
                      const GdkDmabuf                 *dmabuf,
                      int                              width,
                      int                              height,
                      const struct wl_buffer_listener *listener,
                      void *                           data)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SUBSURFACE (self)->parent));
  struct zwp_linux_buffer_params_v1 *params;
  struct wl_buffer *buffer;
  CreateBufferData cd = { NULL, FALSE };
  struct wl_event_queue *event_queue;

  params = zwp_linux_dmabuf_v1_create_params (display->linux_dmabuf);

  for (gsize i = 0; i < dmabuf->n_planes; i++)
    zwp_linux_buffer_params_v1_add (params,
                                    dmabuf->planes[i].fd,
                                    i,
                                    dmabuf->planes[i].offset,
                                    dmabuf->planes[i].stride,
                                    dmabuf->modifier >> 32,
                                    dmabuf->modifier & 0xffffffff);

  event_queue = wl_display_create_queue (display->wl_display);

  wl_proxy_set_queue ((struct wl_proxy *) params, event_queue);

  zwp_linux_buffer_params_v1_add_listener (params, &params_listener, &cd);

  zwp_linux_buffer_params_v1_create (params,
                                     width,
                                     height,
                                     dmabuf->fourcc,
                                     0);

  while (!cd.done)
    gdk_wayland_display_dispatch_queue (GDK_DISPLAY (display), event_queue);

  zwp_linux_buffer_params_v1_destroy (params);
  wl_event_queue_destroy (event_queue);

  buffer = cd.buffer;

  if (buffer)
    {
      wl_proxy_set_queue ((struct wl_proxy *) buffer, NULL);
      wl_buffer_add_listener (buffer, listener, data);
    }
  else
    {
      listener->release (data, NULL);
    }

  return buffer;
}

static struct wl_buffer *
get_dmabuf_texture_wl_buffer (GdkWaylandSubsurface *self,
                              GdkTexture           *texture)
{
  return get_dmabuf_wl_buffer (self,
                               gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture)),
                               gdk_texture_get_width (texture),
                               gdk_texture_get_height (texture),
                               &dmabuf_buffer_listener,
                               g_object_ref (texture));
}

typedef struct {
  GdkTexture *texture;
  GdkDmabuf dmabuf;
} GLBufferData;

static void
gl_buffer_release (void             *data,
                   struct wl_buffer *buffer)
{
  GLBufferData *gldata = data;

  g_object_unref (gldata->texture);
  gdk_dmabuf_close_fds (&gldata->dmabuf);
  g_free (gldata);

  if (buffer)
    wl_buffer_destroy (buffer);
}

static const struct wl_buffer_listener gl_buffer_listener = {
  gl_buffer_release,
};

static struct wl_buffer *
get_gl_texture_wl_buffer (GdkWaylandSubsurface *self,
                          GdkTexture           *texture)
{
  GdkDisplay *display = gdk_surface_get_display (GDK_SUBSURFACE (self)->parent);
  GdkGLTexture *gltexture = GDK_GL_TEXTURE (texture);
  GdkGLContext *glcontext;
  GLBufferData gldata;

  glcontext = gdk_display_get_gl_context (display);
  if (!gdk_gl_context_is_shared (glcontext, gdk_gl_texture_get_context (gltexture)))
    return NULL;

  /* Can we avoid this when a right context is current already? */
  gdk_gl_context_make_current (glcontext);

  if (!gdk_gl_context_export_dmabuf (glcontext,
                                     gdk_gl_texture_get_id (gltexture),
                                     &gldata.dmabuf))
    return NULL;

  gldata.texture = g_object_ref (texture);

  return get_dmabuf_wl_buffer (self,
                               &gldata.dmabuf,
                               gdk_texture_get_width (texture),
                               gdk_texture_get_height (texture),
                               &gl_buffer_listener,
                               g_memdup2 (&gldata, sizeof (gldata)));
}

static struct wl_buffer *
get_wl_buffer (GdkWaylandSubsurface *self,
               GdkTexture           *texture)
{
  GdkDisplay *display = gdk_surface_get_display (GDK_SUBSURFACE (self)->parent);
  struct wl_buffer *buffer = NULL;

  if (GDK_IS_DMABUF_TEXTURE (texture))
    buffer = get_dmabuf_texture_wl_buffer (self, texture);
  else if (GDK_IS_GL_TEXTURE (texture))
    buffer = get_gl_texture_wl_buffer (self, texture);

  if (GDK_DISPLAY_DEBUG_CHECK (display, FORCE_OFFLOAD))
    {
      if (!buffer)
         buffer = get_shm_wl_buffer (self, texture);
    }

  return buffer;
}

static void
sp_buffer_release (void             *data,
                   struct wl_buffer *buffer)
{
  wl_buffer_destroy (buffer);
}

static const struct wl_buffer_listener sp_buffer_listener = {
  sp_buffer_release,
};

static struct wl_buffer *
get_sp_buffer (GdkWaylandSubsurface *self)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SUBSURFACE (self)->parent));
  struct wl_buffer *buffer = NULL;

  if (display->single_pixel_buffer)
    buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer (display->single_pixel_buffer,
                                                                       0, 0, 0, 0xffffffffU);

  if (buffer)
    wl_buffer_add_listener (buffer, &sp_buffer_listener, self);

  return buffer;
}

static inline enum wl_output_transform
gdk_texture_transform_to_wl (GdkDihedral transform)
{
  return (enum wl_output_transform) transform;
}

static inline GdkDihedral
wl_output_transform_to_gdk (enum wl_output_transform transform)
{
  return (GdkDihedral) transform;
}

G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_NORMAL == (int) GDK_DIHEDRAL_NORMAL);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_90 == (int) GDK_DIHEDRAL_90);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_180 == (int) GDK_DIHEDRAL_180);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_270 == (int) GDK_DIHEDRAL_270);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_FLIPPED == (int) GDK_DIHEDRAL_FLIPPED);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_FLIPPED_90 == (int) GDK_DIHEDRAL_FLIPPED_90);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_FLIPPED_180 == (int) GDK_DIHEDRAL_FLIPPED_180);
G_STATIC_ASSERT ((int) WL_OUTPUT_TRANSFORM_FLIPPED_270 == (int) GDK_DIHEDRAL_FLIPPED_270);

static void
ensure_bg_surface (GdkWaylandSubsurface *self)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (GDK_SUBSURFACE (self)->parent);
  GdkDisplay *display = gdk_surface_get_display (GDK_SUBSURFACE (self)->parent);
  GdkWaylandDisplay *disp = GDK_WAYLAND_DISPLAY (display);
  struct wl_region *region;

  if (self->bg_surface)
    return;

  self->bg_surface = wl_compositor_create_surface (disp->compositor);
  self->bg_subsurface = wl_subcompositor_get_subsurface (disp->subcompositor,
                                                         self->bg_surface,
                                                         impl->display_server.wl_surface);
  self->bg_viewport = wp_viewporter_get_viewport (disp->viewporter, self->bg_surface);

  /* We are opaque */
  wl_surface_set_opaque_region (self->bg_surface, self->opaque_region);

  /* No input, please */
  region = wl_compositor_create_region (disp->compositor);
  wl_surface_set_input_region (self->bg_surface, region);
  wl_region_destroy (region);
}

static inline gboolean
scaled_rect_is_integral (const graphene_rect_t *rect,
                         float                  scale,
                         graphene_rect_t       *device_rect)
{
  cairo_rectangle_int_t device_int;

  gsk_rect_scale (rect, scale, scale, device_rect);

  device_int.x = device_rect->origin.x;
  device_int.y = device_rect->origin.y;
  device_int.width = device_rect->size.width;
  device_int.height = device_rect->size.height;

  return device_int.x == device_rect->origin.x &&
         device_int.y == device_rect->origin.y &&
         device_int.width == device_rect->size.width &&
         device_int.height == device_rect->size.height;
}

static gboolean
gdk_wayland_subsurface_attach (GdkSubsurface         *sub,
                               GdkTexture            *texture,
                               const graphene_rect_t *source,
                               const graphene_rect_t *dest,
                               GdkDihedral            transform,
                               const graphene_rect_t *background,
                               gboolean               above,
                               GdkSubsurface         *sibling)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);
  GdkWaylandSurface *parent = GDK_WAYLAND_SURFACE (sub->parent);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (sub->parent));
  struct wl_buffer *buffer = NULL;
  gboolean result = FALSE;
  GdkWaylandSubsurface *sib = sibling ? GDK_WAYLAND_SUBSURFACE (sibling) : NULL;
  gboolean will_be_above;
  double scale;
  graphene_rect_t device_rect;
  gboolean has_background;
  enum wl_output_transform tf;
  gboolean dest_changed = FALSE;
  gboolean source_changed = FALSE;
  gboolean transform_changed = FALSE;
  gboolean stacking_changed = FALSE;
  gboolean needs_commit = FALSE;
  gboolean background_changed = FALSE;
  gboolean needs_bg_commit = FALSE;
  gboolean color_state_changed = FALSE;

  if (sibling)
    will_be_above = sibling->above_parent;
  else
    will_be_above = above;

  if (sub->parent == NULL)
    {
      g_warning ("Can't attach to destroyed subsurface %p", self);
      return FALSE;
    }

  if (self->dest.x != dest->origin.x ||
      self->dest.y != dest->origin.y ||
      self->dest.width != dest->size.width ||
      self->dest.height != dest->size.height)
    {
      self->dest.x = dest->origin.x;
      self->dest.y = dest->origin.y;
      self->dest.width = dest->size.width;
      self->dest.height = dest->size.height;
      dest_changed = TRUE;
    }

  if (!gsk_rect_equal (&self->source, source))
    {
      self->source.origin.x = source->origin.x;
      self->source.origin.y = source->origin.y;
      self->source.size.width = source->size.width;
      self->source.size.height = source->size.height;
      source_changed = TRUE;
    }

  tf = gdk_texture_transform_to_wl (transform);
  if (self->transform != tf)
    {
      self->transform = tf;
      transform_changed = TRUE;
    }

  if (sibling != gdk_subsurface_get_sibling (sub, above) ||
      will_be_above != gdk_subsurface_is_above_parent (sub))
    stacking_changed = TRUE;

  if (self->texture == NULL)
    {
      dest_changed = TRUE;
      source_changed = TRUE;
      transform_changed = TRUE;
      stacking_changed = TRUE;
    }

  scale = gdk_fractional_scale_to_double (&parent->scale);

  if (background)
    {
      background_changed =
          !self->bg_attached ||
          self->bg_rect.x != background->origin.x ||
          self->bg_rect.y != background->origin.y ||
          self->bg_rect.width != background->size.width ||
          self->bg_rect.height != background->size.height;
      self->bg_rect.x = background->origin.x;
      self->bg_rect.y = background->origin.y;
      self->bg_rect.width = background->size.width;
      self->bg_rect.height = background->size.height;
    }
  else
    {
      background_changed = self->bg_attached;
      self->bg_rect.x = 0;
      self->bg_rect.y = 0;
      self->bg_rect.width = 0;
      self->bg_rect.height = 0;
    }

  has_background = self->bg_rect.width > 0 && self->bg_rect.height > 0;

  if (!scaled_rect_is_integral (dest, 1, &device_rect))
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Non-integral coordinates %g %g %g %g",
                         self,
                         dest->origin.x, dest->origin.y,
                         dest->size.width, dest->size.height);
    }
  else if (!scaled_rect_is_integral (dest, scale, &device_rect))
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Non-integral device coordinates %g %g %g %g (scale %.2f)",
                         self,
                         device_rect.origin.x, device_rect.origin.y,
                         device_rect.size.width, device_rect.size.height,
                         scale);
    }
  else if (background && !scaled_rect_is_integral (background, 1, &device_rect))
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Non-integral background coordinates %g %g %g %g",
                         self,
                         background->origin.x, background->origin.y,
                         background->size.width, background->size.height);
    }
  else if (background && !scaled_rect_is_integral (background, scale, &device_rect))
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Non-integral background device coordinates %g %g %g %g (scale %.2f)",
                         self,
                         device_rect.origin.x, device_rect.origin.y,
                         device_rect.size.width, device_rect.size.height,
                         scale);
    }
  else if (!will_be_above &&
           gdk_memory_format_alpha (gdk_texture_get_format (texture)) != GDK_MEMORY_ALPHA_OPAQUE &&
           !has_background)
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Non-opaque texture (%dx%d) below",
                         self,
                         gdk_texture_get_width (texture),
                         gdk_texture_get_height (texture));
    }
  else if (has_background && !display->single_pixel_buffer)
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Texture has background, but no single-pixel buffer support",
                         self);
    }
  else if (self->color &&
           !gdk_wayland_color_surface_can_set_color_state (self->color, gdk_texture_get_color_state (texture)))
    {
      GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                         "[%p] 🗙 Texture colorstate %s not supported",
                         self,
                         gdk_color_state_get_name (gdk_texture_get_color_state (texture)));
    }
  else
    {
      gboolean was_transparent;

      if (self->texture)
        was_transparent = gdk_memory_format_alpha (gdk_texture_get_format (self->texture)) != GDK_MEMORY_ALPHA_OPAQUE;
      else
        was_transparent = FALSE;

      if (self->texture && texture)
        color_state_changed = !gdk_color_state_equal (gdk_texture_get_color_state (self->texture),
                                                      gdk_texture_get_color_state (texture));
      else
        color_state_changed = TRUE;

      if (g_set_object (&self->texture, texture))
        {
          buffer = get_wl_buffer (self, texture);
          if (buffer != NULL)
            {
              gboolean is_transparent;

              is_transparent = gdk_memory_format_alpha (gdk_texture_get_format (texture)) != GDK_MEMORY_ALPHA_OPAQUE;
              if (is_transparent != was_transparent)
                {
                  if (is_transparent)
                    wl_surface_set_opaque_region (self->surface, NULL);
                  else
                    wl_surface_set_opaque_region (self->surface, self->opaque_region);
                }

              GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                                 "[%p] %s Attaching %s (%dx%d, %s) at %d %d %d %d%s%s%s",
                                 self,
                                 G_OBJECT_TYPE_NAME (texture),
                                 will_be_above
                                   ? (has_background ? "▲" : "△")
                                   : (has_background ? "▼" : "▽"),
                                 gdk_texture_get_width (texture),
                                 gdk_texture_get_height (texture),
                                 gdk_color_state_get_name (gdk_texture_get_color_state (texture)),
                                 self->dest.x, self->dest.y,
                                 self->dest.width, self->dest.height,
                                 transform != GDK_DIHEDRAL_NORMAL ? " (" : "",
                                 transform != GDK_DIHEDRAL_NORMAL ? gdk_dihedral_get_name (transform) : "",
                                 transform != GDK_DIHEDRAL_NORMAL ? ")" : ""
                                 );
              result = TRUE;
            }
          else
            {
              GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                                 "[%p] 🗙 Failed to create wl_buffer for %s",
                                 self,
                                 G_OBJECT_TYPE_NAME (texture));
            }
        }
      else
        {
          if (dest_changed)
            GDK_DISPLAY_DEBUG (gdk_surface_get_display (sub->parent), OFFLOAD,
                               "[%p] %s Moving texture (%dx%d) to %d %d %d %d",
                               self,
                               will_be_above
                                 ? (has_background ? "▲" : "△")
                                 : (has_background ? "▼" : "▽"),
                               gdk_texture_get_width (texture),
                               gdk_texture_get_height (texture),
                               self->dest.x, self->dest.y,
                               self->dest.width, self->dest.height);

          buffer = NULL;
          result = TRUE;
        }
    }

  if (result)
    {
      if (transform_changed)
        {
          wl_surface_set_buffer_transform (self->surface, self->transform);
          needs_commit = TRUE;
        }

      if (dest_changed)
        {
          wl_subsurface_set_position (self->subsurface, self->dest.x, self->dest.y);
          wp_viewport_set_destination (self->viewport, self->dest.width, self->dest.height);
          needs_commit = TRUE;
        }

      if (source_changed)
        {
          wp_viewport_set_source (self->viewport,
                                  wl_fixed_from_double (self->source.origin.x),
                                  wl_fixed_from_double (self->source.origin.y),
                                  wl_fixed_from_double (self->source.size.width),
                                  wl_fixed_from_double (self->source.size.height));
          needs_commit = TRUE;
        }

      if (buffer)
        {
          wl_surface_attach (self->surface, buffer, 0, 0);
          wl_surface_damage_buffer (self->surface,
                                    0, 0,
                                    gdk_texture_get_width (texture),
                                    gdk_texture_get_height (texture));

          if (self->color && color_state_changed)
            gdk_wayland_color_surface_set_color_state (self->color, gdk_texture_get_color_state (texture));

          needs_commit = TRUE;
        }

      if (has_background)
        {
          ensure_bg_surface (self);

          if (background_changed)
            {
              wl_subsurface_set_position (self->bg_subsurface, self->bg_rect.x, self->bg_rect.y);
              wp_viewport_set_destination (self->bg_viewport, self->bg_rect.width, self->bg_rect.height);
              needs_bg_commit = TRUE;
            }

          if (!self->bg_attached)
            {
              self->bg_attached = TRUE;

              wp_viewport_set_source (self->bg_viewport,
                                      wl_fixed_from_int (0),
                                      wl_fixed_from_int (0),
                                      wl_fixed_from_int (1),
                                      wl_fixed_from_int (1));
              wl_surface_attach (self->bg_surface, get_sp_buffer (self), 0, 0);
              wl_surface_damage_buffer (self->bg_surface, 0, 0, 1, 1);
              needs_bg_commit = TRUE;
            }
        }
      else
        {
          if (self->bg_attached)
            {
              self->bg_attached = FALSE;
              wl_surface_attach (self->bg_surface, NULL, 0, 0);
              needs_bg_commit = TRUE;
            }
        }

      result = TRUE;
    }
  else
    {
      if (g_set_object (&self->texture, NULL))
        {
          wl_surface_attach (self->surface, NULL, 0, 0);
          needs_commit = TRUE;
        }

      if (self->bg_attached)
        {
          self->bg_attached = FALSE;
          wl_surface_attach (self->bg_surface, NULL, 0, 0);
          needs_bg_commit = TRUE;
        }
    }

  if (stacking_changed)
    {
      if (sib)
        {
          if (above)
            wl_subsurface_place_above (self->subsurface, sib->surface);
          else
            wl_subsurface_place_below (self->subsurface, sib->surface);
        }
      else
        {
          if (above)
            wl_subsurface_place_above (self->subsurface,
                                       GDK_WAYLAND_SURFACE (sub->parent)->display_server.wl_surface);
          else
            wl_subsurface_place_below (self->subsurface,
                                       GDK_WAYLAND_SURFACE (sub->parent)->display_server.wl_surface);
        }
      needs_commit = TRUE;

      if (self->bg_attached)
        {
          wl_subsurface_place_below (self->bg_subsurface, self->surface);
          needs_bg_commit = TRUE;
        }
    }

  if (needs_commit)
    wl_surface_commit (self->surface);

  if (needs_bg_commit)
    wl_surface_commit (self->bg_surface);

  ((GdkWaylandSurface *)sub->parent)->has_pending_subsurface_commits = needs_commit || needs_bg_commit;
  GDK_WAYLAND_SURFACE (sub->parent)->opaque_region_dirty = stacking_changed || dest_changed || background_changed;

  return result;
}

static void
gdk_wayland_subsurface_detach (GdkSubsurface *sub)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);

  if (sub->parent == NULL)
    {
      g_warning ("Can't detach from destroyed subsurface %p", self);
      return;
    }

  g_set_object (&self->texture, NULL);
  wl_surface_attach (self->surface, NULL, 0, 0);
  wl_surface_set_opaque_region (self->surface, self->opaque_region);
  wl_surface_commit (self->surface);

  if (self->bg_attached)
    {
      wl_surface_attach (self->bg_surface, NULL, 0, 0);
      wl_surface_commit (self->bg_surface);
      self->bg_attached = FALSE;
    }

  ((GdkWaylandSurface *)sub->parent)->has_pending_subsurface_commits = TRUE;
  GDK_WAYLAND_SURFACE (sub->parent)->opaque_region_dirty = TRUE;
}

static GdkTexture *
gdk_wayland_subsurface_get_texture (GdkSubsurface *sub)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);

  return self->texture;
}

static void
gdk_wayland_subsurface_get_texture_rect (GdkSubsurface   *sub,
                                         graphene_rect_t *rect)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);

  rect->origin.x = self->dest.x;
  rect->origin.y = self->dest.y;
  rect->size.width = self->dest.width;
  rect->size.height = self->dest.height;
}

static void
gdk_wayland_subsurface_get_source_rect (GdkSubsurface   *sub,
                                        graphene_rect_t *rect)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);

  rect->origin.x = self->source.origin.x;
  rect->origin.y = self->source.origin.y;
  rect->size.width = self->source.size.width;
  rect->size.height = self->source.size.height;
}

static GdkDihedral
gdk_wayland_subsurface_get_transform (GdkSubsurface *sub)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);

  return wl_output_transform_to_gdk (self->transform);
}

static gboolean
gdk_wayland_subsurface_get_background_rect (GdkSubsurface   *sub,
                                            graphene_rect_t *rect)
{
  GdkWaylandSubsurface *self = GDK_WAYLAND_SUBSURFACE (sub);

  rect->origin.x = self->bg_rect.x;
  rect->origin.y = self->bg_rect.y;
  rect->size.width = self->bg_rect.width;
  rect->size.height = self->bg_rect.height;

  return rect->size.width > 0 && rect->size.height > 0;
}

static void
gdk_wayland_subsurface_class_init (GdkWaylandSubsurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkSubsurfaceClass *subsurface_class = GDK_SUBSURFACE_CLASS (class);

  object_class->finalize = gdk_wayland_subsurface_finalize;

  subsurface_class->attach = gdk_wayland_subsurface_attach;
  subsurface_class->detach = gdk_wayland_subsurface_detach;
  subsurface_class->get_texture = gdk_wayland_subsurface_get_texture;
  subsurface_class->get_source_rect = gdk_wayland_subsurface_get_source_rect;
  subsurface_class->get_texture_rect = gdk_wayland_subsurface_get_texture_rect;
  subsurface_class->get_transform = gdk_wayland_subsurface_get_transform;
  subsurface_class->get_background_rect = gdk_wayland_subsurface_get_background_rect;
};

static void
frame_callback (void               *data,
                struct wl_callback *callback,
                uint32_t            time)
{
  GdkSubsurface *sub = data;

  g_assert (((GdkWaylandSubsurface *)sub)->frame_callback == callback);
  g_assert (!GDK_SURFACE_DESTROYED (sub->parent));

  gdk_wayland_surface_frame_callback (sub->parent, time);
}

static const struct wl_callback_listener frame_listener = {
  frame_callback
};

void
gdk_wayland_subsurface_request_frame (GdkSubsurface *sub)
{
  GdkWaylandSubsurface *self = (GdkWaylandSubsurface *)sub;

  self->frame_callback = wl_surface_frame (self->surface);
  wl_proxy_set_queue ((struct wl_proxy *) self->frame_callback, NULL);
  wl_callback_add_listener (self->frame_callback, &frame_listener, self);
  wl_surface_commit (self->surface);
}

void
gdk_wayland_subsurface_clear_frame_callback (GdkSubsurface *sub)
{
  GdkWaylandSubsurface *self = (GdkWaylandSubsurface *)sub;

  g_clear_pointer (&self->frame_callback, wl_callback_destroy);
}

GdkSubsurface *
gdk_wayland_surface_create_subsurface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *disp = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandSubsurface *sub;
  struct wl_region *region;

  if (disp->subcompositor == NULL || disp->viewporter == NULL)
    {
      GDK_DISPLAY_DEBUG (display, OFFLOAD, "Can't use subsurfaces without subcompositor and viewporter");
      return NULL;
    }

  sub = g_object_new (GDK_TYPE_WAYLAND_SUBSURFACE, NULL);

  sub->surface = wl_compositor_create_surface (disp->compositor);
  sub->subsurface = wl_subcompositor_get_subsurface (disp->subcompositor,
                                                     sub->surface,
                                                     impl->display_server.wl_surface);
  if (disp->color)
    sub->color = gdk_wayland_color_surface_new (disp->color, sub->surface, NULL, NULL);

  sub->viewport = wp_viewporter_get_viewport (disp->viewporter, sub->surface);

  /* No input, please */
  region = wl_compositor_create_region (disp->compositor);
  wl_surface_set_input_region (sub->surface, region);
  wl_region_destroy (region);

  /* Keep a max-sized opaque region so we don't have to update it
   * when the size of the texture changes.
   */
  sub->opaque_region = wl_compositor_create_region (disp->compositor);
  wl_region_add (sub->opaque_region, 0, 0, G_MAXINT, G_MAXINT);
  wl_surface_set_opaque_region (sub->surface, sub->opaque_region);

  GDK_DISPLAY_DEBUG (display, OFFLOAD, "Subsurface %p of surface %p created", sub, impl);

  return GDK_SUBSURFACE (sub);
}

