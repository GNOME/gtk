/* gdkmacosglcontext-private.h
 *
 * Copyright (C) 2020 Red Hat, Inc.
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

#ifndef __GDK_MACOS_GL_CONTEXT_PRIVATE_H__
#define __GDK_MACOS_GL_CONTEXT_PRIVATE_H__

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdksurface.h"
#include "gdkinternals.h"

#include "gdkmacosglcontext.h"
#include "gdkmacossurface.h"

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <AppKit/AppKit.h>

G_BEGIN_DECLS

struct _GdkMacosGLContext
{
  GdkGLContext parent_instance;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  NSOpenGLContext *gl_context;
  G_GNUC_END_IGNORE_DEPRECATIONS

  NSWindow *dummy_window;
  NSView *dummy_view;

  cairo_region_t *damage;

  guint is_attached : 1;
  guint needs_resize : 1;
};

struct _GdkMacosGLContextClass
{
  GdkGLContextClass parent_class;
};

GdkGLContext *_gdk_macos_gl_context_new          (GdkMacosSurface    *surface,
                                                  gboolean            attached,
                                                  GdkGLContext       *share,
                                                  GError            **error);
gboolean      _gdk_macos_gl_context_make_current (GdkMacosGLContext  *self);

G_END_DECLS

#endif /* __GDK_MACOS_GL_CONTEXT_PRIVATE_H__ */
