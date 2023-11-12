/* gdkdmabufegl.c
 *
 * Copyright 2023  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#if defined(HAVE_DMABUF) && defined (HAVE_EGL)
#include "gdkdmabufprivate.h"

#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdebugprivate.h"
#include "gdkdmabuftextureprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglcontextprivate.h"
#include "gdktexturedownloader.h"

#include <graphene.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <drm_fourcc.h>
#include <epoxy/egl.h>

/* A dmabuf downloader implementation that downloads buffers via
 * gsk_renderer_render_texture + GL texture download.
 */

static gboolean
gdk_dmabuf_egl_downloader_collect_formats (const GdkDmabufDownloader *downloader,
                                           GdkDisplay                *display,
                                           GdkDmabufFormatsBuilder   *formats,
                                           GdkDmabufFormatsBuilder   *external)
{
  GdkGLContext *context = gdk_display_get_gl_context (display);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);
  int num_fourccs;
  int *fourccs;
  guint64 *modifiers;
  unsigned int *external_only;
  int n_mods;

  if (egl_display == EGL_NO_DISPLAY ||
      !display->have_egl_dma_buf_import)
    return FALSE;

  gdk_gl_context_make_current (context);

  eglQueryDmaBufFormatsEXT (egl_display, 0, NULL, &num_fourccs);
  fourccs = g_new (int, num_fourccs);
  eglQueryDmaBufFormatsEXT (egl_display, num_fourccs, fourccs, &num_fourccs);

  n_mods = 80;
  modifiers = g_new (guint64, n_mods);
  external_only = g_new (unsigned int, n_mods);

  for (int i = 0; i < num_fourccs; i++)
    {
      int num_modifiers;
      gboolean all_external;

      eglQueryDmaBufModifiersEXT (egl_display,
                                  fourccs[i],
                                  0,
                                  NULL,
                                  NULL,
                                  &num_modifiers);

      if (num_modifiers > n_mods)
        {
          n_mods = num_modifiers;
          modifiers = g_renew (guint64, modifiers, n_mods);
          external_only = g_renew (unsigned int, external_only, n_mods);
        }

      eglQueryDmaBufModifiersEXT (egl_display,
                                  fourccs[i],
                                  num_modifiers,
                                  modifiers,
                                  external_only,
                                  &num_modifiers);

      all_external = TRUE;

      for (int j = 0; j < num_modifiers; j++)
        {
          /* All linear formats we support are already added my the mmap downloader.
           * We don't add external formats, unless we can use them (via GLES)
           */
          if (modifiers[j] != DRM_FORMAT_MOD_LINEAR &&
              (!external_only[j] || gdk_gl_context_get_use_es (context)))
            {
              GDK_DISPLAY_DEBUG (display, DMABUF,
                                 "%s%s dmabuf format %.4s:%#" G_GINT64_MODIFIER "x",
                                 external_only[j] ? "external " : "",
                                 downloader->name,
                                 (char *) &fourccs[i],
                                 modifiers[j]);

              gdk_dmabuf_formats_builder_add_format (formats, fourccs[i], modifiers[j]);
            }
          if (external_only[j])
            gdk_dmabuf_formats_builder_add_format (external, fourccs[i], modifiers[j]);
          else
            all_external = FALSE;
        }

      /* Accept implicit modifiers as long as we accept the format at all.
       * This is a bit of a crapshot, but unfortunately needed for a bunch
       * of drivers.
       *
       * As an extra wrinkle, treat the implicit modifier as 'external only'
       * if all formats with the same fourcc are 'external only'.
       */
      if (!all_external || gdk_gl_context_get_use_es (context))
        gdk_dmabuf_formats_builder_add_format (formats, fourccs[i], DRM_FORMAT_MOD_INVALID);
      if (all_external)
        gdk_dmabuf_formats_builder_add_format (external, fourccs[i], DRM_FORMAT_MOD_INVALID);
    }

  g_free (modifiers);
  g_free (external_only);
  g_free (fourccs);

  return TRUE;
}

static gboolean
gdk_dmabuf_egl_downloader_add_formats (const GdkDmabufDownloader *downloader,
                                       GdkDisplay                *display,
                                       GdkDmabufFormatsBuilder   *builder)
{
  GdkDmabufFormatsBuilder *formats;
  GdkDmabufFormatsBuilder *external;
  gboolean retval = FALSE;

  g_assert (display->egl_dmabuf_formats == NULL);
  g_assert (display->egl_external_formats == NULL);

  formats = gdk_dmabuf_formats_builder_new ();
  external = gdk_dmabuf_formats_builder_new ();

  retval = gdk_dmabuf_egl_downloader_collect_formats (downloader, display, formats, external);

  display->egl_dmabuf_formats = gdk_dmabuf_formats_builder_free_to_formats (formats);
  display->egl_external_formats = gdk_dmabuf_formats_builder_free_to_formats (external);

  gdk_dmabuf_formats_builder_add_formats (builder, display->egl_dmabuf_formats);

  return retval;
}

static gboolean
gdk_dmabuf_egl_downloader_supports (const GdkDmabufDownloader  *downloader,
                                    GdkDisplay                 *display,
                                    const GdkDmabuf            *dmabuf,
                                    gboolean                    premultiplied,
                                    GdkMemoryFormat            *out_format,
                                    GError                    **error)
{
  if (gdk_dmabuf_formats_contains (display->egl_dmabuf_formats, dmabuf->fourcc, dmabuf->modifier))
    {
      *out_format = gdk_dmabuf_get_memory_format (display, dmabuf->fourcc, premultiplied);
      return TRUE;
    }

  g_set_error (error,
               GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
               "Unsupported dmabuf format: %.4s:%#" G_GINT64_MODIFIER "x",
               (char *) &dmabuf->fourcc, dmabuf->modifier);

  return FALSE;
}

/* Hack. We don't include gsk/gsk.h here to avoid a build order problem
 * with the generated header gskenumtypes.h, so we need to hack around
 * a bit to access the gsk api we need.
 */

typedef gpointer GskRenderer;

extern GskRenderer *   gsk_gl_renderer_new          (void);
extern gboolean        gsk_renderer_realize         (GskRenderer  *renderer,
                                                     GdkSurface   *surface,
                                                     GError      **error);
extern GdkTexture *    gsk_renderer_convert_texture (GskRenderer  *renderer,
                                                     GdkTexture   *texture);

typedef void (* InvokeFunc) (gpointer data);

typedef struct _InvokeData
{
  volatile int spinlock;
  InvokeFunc func;
  gpointer data;
} InvokeData;

static gboolean
gdk_dmabuf_egl_downloader_invoke_callback (gpointer data)
{
  InvokeData *invoke = data;
  GdkGLContext *previous;

  previous = gdk_gl_context_get_current ();

  invoke->func (invoke->data);

  if (previous)
    gdk_gl_context_make_current (previous);
  else
    gdk_gl_context_clear_current ();

  g_atomic_int_set (&invoke->spinlock, 1);

  return FALSE;
}

/* Run func in the main thread, taking care not to disturb
 * the current GL context of the caller.
 */
static void
gdk_dmabuf_egl_downloader_run (InvokeFunc func,
                               gpointer   data)
{
  InvokeData invoke = { 0, func, data };

  g_main_context_invoke (NULL, gdk_dmabuf_egl_downloader_invoke_callback, &invoke);

  while (g_atomic_int_get (&invoke.spinlock) == 0) ;
}

typedef struct _Download Download;

struct _Download
{
  GdkDmabufTexture *texture;
  GdkMemoryFormat format;
  guchar *data;
  gsize stride;
};

static GskRenderer *
get_gsk_renderer (GdkDisplay *display)
{
  if (!display->egl_gsk_renderer)
    {
      GskRenderer *renderer;
      GError *error = NULL;

      renderer = gsk_gl_renderer_new ();

      if (!gsk_renderer_realize (renderer, NULL, &error))
        {
          g_warning ("Failed to realize GL renderer: %s", error->message);
          g_error_free (error);
          g_object_unref (renderer);

          return NULL;
        }

      display->egl_gsk_renderer = renderer;
    }

  return display->egl_gsk_renderer;
}

static void
gdk_dmabuf_egl_downloader_do_download (gpointer data)
{
  Download *download = data;
  GdkDisplay *display;
  GskRenderer *renderer;
  GdkTexture *native;
  GdkTextureDownloader *downloader;

  display = gdk_dmabuf_texture_get_display (download->texture);

  renderer = get_gsk_renderer (display);

  native = gsk_renderer_convert_texture (renderer, GDK_TEXTURE (download->texture));

  downloader = gdk_texture_downloader_new (native);
  gdk_texture_downloader_set_format (downloader, download->format);
  gdk_texture_downloader_download_into (downloader, download->data, download->stride);
  gdk_texture_downloader_free (downloader);

  g_object_unref (native);
}

static void
gdk_dmabuf_egl_downloader_download (const GdkDmabufDownloader *downloader,
                                    GdkTexture                *texture,
                                    GdkMemoryFormat            format,
                                    guchar                    *data,
                                    gsize                      stride)
{
  Download download;
  const GdkDmabuf *dmabuf;

  download.texture = GDK_DMABUF_TEXTURE (texture);
  download.format = format;
  download.data = data;
  download.stride = stride;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));

  GDK_DISPLAY_DEBUG (gdk_dmabuf_texture_get_display (download.texture), DMABUF,
                     "Using %s for downloading a dmabuf (format %.4s:%#" G_GINT64_MODIFIER "x)",
                     downloader->name, (char *)&dmabuf->fourcc, dmabuf->modifier);


  gdk_dmabuf_egl_downloader_run (gdk_dmabuf_egl_downloader_do_download, &download);
}

const GdkDmabufDownloader *
gdk_dmabuf_get_egl_downloader (void)
{
  static const GdkDmabufDownloader downloader = {
    "egl",
    gdk_dmabuf_egl_downloader_add_formats,
    gdk_dmabuf_egl_downloader_supports,
    gdk_dmabuf_egl_downloader_download,
  };

  return &downloader;
}

#endif  /* HAVE_DMABUF && HAVE_EGL */
