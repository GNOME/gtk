/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkmacosglcontext-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"

#include "gdkinternals.h"
#include "gdkintl.h"

#include <OpenGL/gl.h>

#import "GdkMacosGLView.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_TYPE (GdkMacosGLContext, gdk_macos_gl_context, GDK_TYPE_GL_CONTEXT)

static const char *
get_renderer_name (GLint id)
{
  static char renderer_name[32];

  switch (id & kCGLRendererIDMatchingMask)
  {
  case kCGLRendererGenericID: return "Generic";
  case kCGLRendererGenericFloatID: return "Generic Float";
  case kCGLRendererAppleSWID: return "Apple Software Renderer";
  case kCGLRendererATIRage128ID: return "ATI Rage 128";
  case kCGLRendererATIRadeonID: return "ATI Radeon";
  case kCGLRendererATIRageProID: return "ATI Rage Pro";
  case kCGLRendererATIRadeon8500ID: return "ATI Radeon 8500";
  case kCGLRendererATIRadeon9700ID: return "ATI Radeon 9700";
  case kCGLRendererATIRadeonX1000ID: return "ATI Radeon X1000";
  case kCGLRendererATIRadeonX2000ID: return "ATI Radeon X2000";
  case kCGLRendererATIRadeonX3000ID: return "ATI Radeon X3000";
  case kCGLRendererATIRadeonX4000ID: return "ATI Radeon X4000";
  case kCGLRendererGeForce2MXID: return "GeForce 2 MX";
  case kCGLRendererGeForce3ID: return "GeForce 3";
  case kCGLRendererGeForceFXID: return "GeForce FX";
  case kCGLRendererGeForce8xxxID: return "GeForce 8xxx";
  case kCGLRendererGeForceID: return "GeForce";
  case kCGLRendererVTBladeXP2ID: return "VT Blade XP 2";
  case kCGLRendererIntel900ID: return "Intel 900";
  case kCGLRendererIntelX3100ID: return "Intel X3100";
  case kCGLRendererIntelHDID: return "Intel HD";
  case kCGLRendererIntelHD4000ID: return "Intel HD 4000";
  case kCGLRendererIntelHD5000ID: return "Intel HD 5000";
  case kCGLRendererMesa3DFXID: return "Mesa 3DFX";

  default:
    snprintf (renderer_name, sizeof renderer_name, "0x%08x", id & kCGLRendererIDMatchingMask);
    renderer_name[sizeof renderer_name-1] = 0;
    return renderer_name;
  }
}

static NSOpenGLContext *
get_ns_open_gl_context (GdkMacosGLContext  *self,
                        GError            **error)
{
  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if (self->gl_context == nil)
    {
      g_set_error_literal (error,
                           GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           "Cannot access NSOpenGLContext for surface");
      return NULL;
    }

  return self->gl_context;
}

static NSOpenGLPixelFormat *
create_pixel_format (int      major,
                     int      minor,
                     GError **error)
{
  NSOpenGLPixelFormatAttribute attrs[] = {
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFABackingStore,
    NSOpenGLPFAColorSize, 24,
    NSOpenGLPFAAlphaSize, 8,
    0
  };

  if (major == 3 && minor == 2)
    attrs[1] = NSOpenGLProfileVersion3_2Core;
  else if (major == 4 && minor == 1)
    attrs[1] = NSOpenGLProfileVersion4_1Core;

  NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

  if (format == NULL)
    g_set_error (error,
                 GDK_GL_ERROR,
                 GDK_GL_ERROR_NOT_AVAILABLE,
                 "Failed to create pixel format");

  return g_steal_pointer (&format);
}

static NSView *
ensure_gl_view (GdkMacosGLContext *self)
{
  GdkMacosSurface *surface;
  NSWindow *nswindow;
  NSView *nsview;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  surface = GDK_MACOS_SURFACE (gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self)));
  nsview = _gdk_macos_surface_get_view (surface);
  nswindow = _gdk_macos_surface_get_native (surface);

  if G_UNLIKELY (!GDK_IS_MACOS_GL_VIEW (nsview))
    {
      NSRect frame;

      frame = [[nswindow contentView] bounds];
      nsview = [[GdkMacosGLView alloc] initWithFrame:frame];
      [nsview setWantsBestResolutionOpenGLSurface:YES];
      [nsview setPostsFrameChangedNotifications: YES];
      [nsview setNeedsDisplay:YES];
      [nswindow setContentView:nsview];
      [nsview release];

      if (self->dummy_view != NULL)
        {
          NSView *dummy_view = g_steal_pointer (&self->dummy_view);
          [dummy_view release];
        }

      if (self->dummy_window != NULL)
        {
          NSWindow *dummy_window = g_steal_pointer (&self->dummy_window);
          [dummy_window release];
        }
    }

  return [nswindow contentView];
}

static gboolean
gdk_macos_gl_context_real_realize (GdkGLContext  *context,
                                   GError       **error)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;
  GdkSurface *surface;
  NSOpenGLContext *shared_gl_context = nil;
  NSOpenGLContext *gl_context;
  NSOpenGLPixelFormat *pixelFormat;
  CGLContextObj cgl_context;
  GdkGLContext *shared;
  GdkGLContext *shared_data;
  NSOpenGLContext *existing;
  GLint sync_to_framerate = 1;
  GLint validate = 0;
  GLint swapRect[4];
  int major, minor;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if (self->gl_context != nil)
    return TRUE;

  existing = [NSOpenGLContext currentContext];

  gdk_gl_context_get_required_version (context, &major, &minor);

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  shared = gdk_gl_context_get_shared_context (context);
  shared_data = gdk_surface_get_shared_data_gl_context (surface);

  if (shared != NULL)
    {
      if (!(shared_gl_context = get_ns_open_gl_context (GDK_MACOS_GL_CONTEXT (shared), error)))
        return FALSE;
    }
  else if (shared_data != NULL)
    {
      if (!(shared_gl_context = get_ns_open_gl_context (GDK_MACOS_GL_CONTEXT (shared_data), error)))
        return FALSE;
    }

  GDK_DISPLAY_NOTE (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)),
                    OPENGL,
                    g_message ("Creating NSOpenGLContext (version %d.%d)",
                               major, minor));

  if (!(pixelFormat = create_pixel_format (major, minor, error)))
    return FALSE;

  gl_context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                          shareContext:shared_gl_context];

  [pixelFormat release];

  if (gl_context == nil)
    {
      g_set_error_literal (error,
                           GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           "Failed to create NSOpenGLContext");
      return FALSE;
    }

  cgl_context = [gl_context CGLContextObj];

  swapRect[0] = 0;
  swapRect[1] = 0;
  swapRect[2] = surface->width;
  swapRect[3] = surface->height;

  CGLSetParameter (cgl_context, kCGLCPSwapRectangle, swapRect);
  CGLSetParameter (cgl_context, kCGLCPSwapInterval, &sync_to_framerate);

  CGLEnable (cgl_context, kCGLCESwapRectangle);
  if (validate)
    CGLEnable (cgl_context, kCGLCEStateValidation);

  self->dummy_window = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                   styleMask:0
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO
                                                      screen:nil];
  self->dummy_view = [[NSView alloc] initWithFrame:NSZeroRect];
  [self->dummy_window setContentView:self->dummy_view];
  [gl_context setView:self->dummy_view];

  GLint renderer_id = 0;
  [gl_context getValues:&renderer_id forParameter:NSOpenGLContextParameterCurrentRendererID];
  GDK_DISPLAY_NOTE (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)),
                    OPENGL,
                    g_message ("Created NSOpenGLContext[%p] using %s",
                               gl_context,
                               get_renderer_name (renderer_id)));

  self->gl_context = g_steal_pointer (&gl_context);

  if (existing != NULL)
    [existing makeCurrentContext];

  return TRUE;
}

static gboolean
opaque_region_covers_surface (GdkMacosGLContext *self)
{
  GdkSurface *surface;
  cairo_region_t *region;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));
  region = GDK_MACOS_SURFACE (surface)->opaque_region;

  if (region != NULL &&
      cairo_region_num_rectangles (region) == 1)
    {
      cairo_rectangle_int_t extents;

      cairo_region_get_extents (region, &extents);

      if (extents.x == 0 &&
          extents.y == 0 &&
          extents.width == surface->width &&
          extents.height == surface->height)
        return TRUE;
    }

  return FALSE;
}

static void
gdk_macos_gl_context_begin_frame (GdkDrawContext *context,
                                  cairo_region_t *painted)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;
  GdkSurface *surface;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  surface = gdk_draw_context_get_surface (context);

  g_clear_pointer (&self->damage, cairo_region_destroy);
  self->damage = cairo_region_copy (painted);

  /* If begin frame is called, that means we are trying to draw to
   * the NSWindow using our view. That might be a GdkMacosCairoView
   * but we need it to be a GL view. Also, only in this case do we
   * want to replace our damage region for the next frame (to avoid
   * doing it multiple times).
   */
  if (!self->is_attached &&
      gdk_gl_context_get_shared_context (GDK_GL_CONTEXT (context)))
    ensure_gl_view (self);

  if (self->needs_resize)
    {
      CGLContextObj cgl_context = [self->gl_context CGLContextObj];
      GLint opaque;

      self->needs_resize = FALSE;

      if (self->dummy_view != NULL)
        {
          NSRect frame = NSMakeRect (0, 0, surface->width, surface->height);

          [self->dummy_window setFrame:frame display:NO];
          [self->dummy_view setFrame:frame];
        }

      /* Possibly update our opaque setting depending on a resize. We can
       * rely on getting a resize if decoarated is changed, so this reduces
       * how much we adjust the parameter.
       */
      if (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface))
        opaque = GDK_MACOS_TOPLEVEL_SURFACE (surface)->decorated;
      else
        opaque = FALSE;

      /* If we are maximized, we might be able to make it opaque */
      if (opaque == FALSE)
        opaque = opaque_region_covers_surface (self);

      CGLSetParameter (cgl_context, kCGLCPSurfaceOpacity, &opaque);

      [self->gl_context update];
    }

  GDK_DRAW_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->begin_frame (context, painted);

  if (!self->is_attached)
    {
      NSView *nsview = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));

      g_assert (self->gl_context != NULL);
      g_assert (GDK_IS_MACOS_GL_VIEW (nsview));

      [(GdkMacosGLView *)nsview setOpenGLContext:self->gl_context];
    }
}

static void
gdk_macos_gl_context_end_frame (GdkDrawContext *context,
                                cairo_region_t *painted)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));
  g_assert (self->gl_context != nil);

  GDK_DRAW_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->end_frame (context, painted);

  if (!self->is_attached)
    {
      GdkSurface *surface = gdk_draw_context_get_surface (context);
      CGLContextObj glctx = [self->gl_context CGLContextObj];
      cairo_rectangle_int_t flush_rect;
      GLint swapRect[4];

      /* Coordinates are in display coordinates, where as flush_rect is
       * in GDK coordinates. Must flip Y to match display coordinates where
       * 0,0 is the bottom-left corner.
       */
      cairo_region_get_extents (painted, &flush_rect);
      swapRect[0] = flush_rect.x;                   /* left */
      swapRect[1] = surface->height - flush_rect.y; /* bottom */
      swapRect[2] = flush_rect.width;               /* width */
      swapRect[3] = flush_rect.height;              /* height */
      CGLSetParameter (glctx, kCGLCPSwapRectangle, swapRect);

      [self->gl_context flushBuffer];
    }
}

static void
gdk_macos_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)draw_context;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  self->needs_resize = TRUE;

  g_clear_pointer (&self->damage, cairo_region_destroy);
}

static cairo_region_t *
gdk_macos_gl_context_get_damage (GdkGLContext *context)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if (self->damage != NULL)
    return cairo_region_copy (self->damage);

  return GDK_GL_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->get_damage (context);
}

static void
gdk_macos_gl_context_dispose (GObject *gobject)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (gobject);

  if (self->dummy_view != nil)
    {
      NSView *nsview = g_steal_pointer (&self->dummy_view);
      [nsview release];
    }

  if (self->dummy_window != nil)
    {
      NSWindow *nswindow = g_steal_pointer (&self->dummy_window);
      [nswindow release];
    }

  if (self->gl_context != nil)
    {
      NSOpenGLContext *gl_context = g_steal_pointer (&self->gl_context);

      if (gl_context == [NSOpenGLContext currentContext])
        [NSOpenGLContext clearCurrentContext];

      [gl_context clearDrawable];
      [gl_context release];
    }

  g_clear_pointer (&self->damage, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_macos_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_macos_gl_context_class_init (GdkMacosGLContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkGLContextClass *gl_class = GDK_GL_CONTEXT_CLASS (klass);

  object_class->dispose = gdk_macos_gl_context_dispose;

  draw_context_class->begin_frame = gdk_macos_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_macos_gl_context_end_frame;
  draw_context_class->surface_resized = gdk_macos_gl_context_surface_resized;

  gl_class->get_damage = gdk_macos_gl_context_get_damage;
  gl_class->realize = gdk_macos_gl_context_real_realize;
}

static void
gdk_macos_gl_context_init (GdkMacosGLContext *self)
{
}

GdkGLContext *
_gdk_macos_gl_context_new (GdkMacosSurface  *surface,
                           gboolean          attached,
                           GdkGLContext     *share,
                           GError          **error)
{
  GdkMacosGLContext *context;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (surface), NULL);
  g_return_val_if_fail (!share || GDK_IS_MACOS_GL_CONTEXT (share), NULL);

  context = g_object_new (GDK_TYPE_MACOS_GL_CONTEXT,
                          "surface", surface,
                          "shared-context", share,
                          NULL);

  context->is_attached = !!attached;

  return GDK_GL_CONTEXT (context);
}

gboolean
_gdk_macos_gl_context_make_current (GdkMacosGLContext *self)
{
  NSOpenGLContext *current;

  g_return_val_if_fail (GDK_IS_MACOS_GL_CONTEXT (self), FALSE);

  if (self->gl_context == NULL)
    return FALSE;

  current = [NSOpenGLContext currentContext];

  if (self->gl_context != current)
    {
      /* The OpenGL mac programming guide suggests that glFlush() is called
       * before switching current contexts to ensure that the drawing commands
       * are submitted.
       */
      if (current != NULL)
        glFlush ();

      [self->gl_context makeCurrentContext];
    }

  return TRUE;
}

G_GNUC_END_IGNORE_DEPRECATIONS
