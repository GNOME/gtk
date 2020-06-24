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

#include "gdkinternals.h"
#include "gdkintl.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_TYPE (GdkMacosGLContext, gdk_macos_gl_context, GDK_TYPE_GL_CONTEXT)

static void
gdk_macos_gl_context_end_frame (GdkDrawContext *context,
                                cairo_region_t *painted)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  [self->gl_context flushBuffer];
}

static void
gdk_macos_gl_context_dispose (GObject *gobject)
{
  GdkMacosGLContext *context_macos = GDK_MACOS_GL_CONTEXT (gobject);

  if (context_macos->gl_context != NULL)
    {
      [context_macos->gl_context clearDrawable];
      [context_macos->gl_context release];
      context_macos->gl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_macos_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_macos_gl_context_class_init (GdkMacosGLContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  object_class->dispose = gdk_macos_gl_context_dispose;

  draw_context_class->end_frame = gdk_macos_gl_context_end_frame;
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
  static const NSOpenGLPixelFormatAttribute attrs[] = {
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize, 24,
    NSOpenGLPFAAlphaSize, 8,
    0
  };

  NSOpenGLPixelFormat *format;
  GdkMacosGLContext *context = NULL;
  NSOpenGLContext *ctx;
  GdkDisplay *display;
  NSView *nsview;
  GLint sync_to_framerate = 1;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (surface), NULL);
  g_return_val_if_fail (!share || GDK_IS_MACOS_GL_CONTEXT (share), NULL);

  display = gdk_surface_get_display (GDK_SURFACE (surface));

  if (!(format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs]))
    {
      g_set_error_literal (error,
                           GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL pixel format"));
      goto failure;
    }

  ctx = [[NSOpenGLContext alloc] initWithFormat:format
                                 shareContext:share ? GDK_MACOS_GL_CONTEXT (share)->gl_context : nil];
  if (ctx == NULL)
    {
      g_set_error_literal (error,
                           GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      goto failure;
    }

  nsview = _gdk_macos_surface_get_view (surface);
  [nsview setWantsBestResolutionOpenGLSurface:YES];
  [ctx setValues:&sync_to_framerate forParameter:NSOpenGLCPSwapInterval];
  [ctx setView:nsview];

  GDK_NOTE (OPENGL,
            g_print ("Created NSOpenGLContext[%p]\n", ctx));

  context = g_object_new (GDK_TYPE_MACOS_GL_CONTEXT,
                          "surface", surface,
                          "shared-context", share,
                          NULL);

  context->gl_context = ctx;
  context->is_attached = attached;

failure:
  if (format != NULL)
    [format release];

  return GDK_GL_CONTEXT (context);
}

gboolean
_gdk_macos_gl_context_make_current (GdkMacosGLContext *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_GL_CONTEXT (self), FALSE);

  [self->gl_context makeCurrentContext];

  return TRUE;
}

G_GNUC_END_IGNORE_DEPRECATIONS
