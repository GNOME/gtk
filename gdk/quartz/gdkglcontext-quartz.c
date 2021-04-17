/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-quartz.c: Quartz specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
 * Copyright © 2014  Alexander Larsson
 * Copyright © 2014  Brion Vibber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkglcontext-quartz.h"

#include "gdkquartzdisplay.h"
#include "gdkquartzglcontext.h"
#include "gdkquartzwindow.h"
#include "gdkprivate-quartz.h"
#include "gdkquartz-cocoa-access.h"

#include "gdkinternals.h"

#include "gdkintl.h"

G_DEFINE_TYPE (GdkQuartzGLContext, gdk_quartz_gl_context, GDK_TYPE_GL_CONTEXT)

static void gdk_quartz_gl_context_dispose (GObject *gobject);

void
gdk_quartz_window_invalidate_for_new_frame (GdkWindow      *window,
                                            cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  window_rect.x = 0;
  window_rect.y = 0;
  window_rect.width = gdk_window_get_width (window);
  window_rect.height = gdk_window_get_height (window);

  /* If nothing else is known, repaint everything so that the back
  buffer is fully up-to-date for the swapbuffer */
  cairo_region_union_rectangle (update_area, &window_rect);
}

static gboolean
gdk_quartz_gl_context_realize (GdkGLContext *context,
                               GError      **error)
{
  return TRUE;
}

static void
gdk_quartz_gl_context_end_frame (GdkGLContext *context,
                                 cairo_region_t *painted,
                                 cairo_region_t *damage)
{
  GdkQuartzGLContext *context_quartz = GDK_QUARTZ_GL_CONTEXT (context);

  [context_quartz->gl_context flushBuffer];
}

static void
gdk_quartz_gl_context_class_init (GdkQuartzGLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->realize = gdk_quartz_gl_context_realize;
  context_class->end_frame = gdk_quartz_gl_context_end_frame;
  gobject_class->dispose = gdk_quartz_gl_context_dispose;
}

static void
gdk_quartz_gl_context_init (GdkQuartzGLContext *self)
{
}

gboolean
gdk_quartz_display_init_gl (GdkDisplay *display)
{
  return TRUE;
}

GdkGLContext *
gdk_quartz_window_create_gl_context (GdkWindow     *window,
                                     gboolean       attached,
                                     GdkGLContext  *share,
                                     GError       **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkQuartzGLContext *context;
  NSOpenGLContext *ctx;
  NSOpenGLPixelFormatAttribute attrs[] =
    {
      NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize, 24,
      NSOpenGLPFAAlphaSize, 8,
      0
    };
  NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

  if (format == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL pixel format"));
      return NULL;
    }

  ctx = [[NSOpenGLContext alloc] initWithFormat:format
                                 shareContext:share ? GDK_QUARTZ_GL_CONTEXT (share)->gl_context : nil];
  if (ctx == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return NULL;
    }

  [format release];

  if (attached)
    {
      NSView *view = gdk_quartz_window_get_nsview (window);

      if ([view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)])
        [view setWantsBestResolutionOpenGLSurface:YES];

      GLint sync_to_framerate = 1;
      [ctx setValues:&sync_to_framerate forParameter:NSOpenGLCPSwapInterval];

      [ctx setView:view];
    }

  GDK_NOTE (OPENGL,
            g_print ("Created NSOpenGLContext[%p]\n", ctx));

  context = g_object_new (GDK_TYPE_QUARTZ_GL_CONTEXT,
                          "window", window,
                          "display", display,
                          "shared-context", share,
                          NULL);

  context->gl_context = ctx;
  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

static void
gdk_quartz_gl_context_dispose (GObject *gobject)
{
  GdkQuartzGLContext *context_quartz = GDK_QUARTZ_GL_CONTEXT (gobject);

  if (context_quartz->gl_context != NULL)
    {
      [context_quartz->gl_context clearDrawable];
      [context_quartz->gl_context release];
      context_quartz->gl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_quartz_gl_context_parent_class)->dispose (gobject);
}

gboolean
gdk_quartz_display_make_gl_context_current (GdkDisplay   *display,
                                            GdkGLContext *context)
{
  GdkQuartzGLContext *context_quartz;

  if (context == NULL)
    {
      [NSOpenGLContext clearCurrentContext];
      return TRUE;
    }

  context_quartz = GDK_QUARTZ_GL_CONTEXT (context);

  [context_quartz->gl_context makeCurrentContext];

  return TRUE;
}
