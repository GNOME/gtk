/* gdkdrmglcontext-private.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdksurface.h"

#include "gdkdrmdisplay.h"
#include "gdkdrmglcontext.h"
#include "gdkdrmsurface.h"

#include <epoxy/gl.h>

G_BEGIN_DECLS

struct _GdkDrmGLContext
{
  GdkGLContext parent_instance;

  struct gbm_surface *gbm_surface;
  int width;
  int height;
  struct gbm_bo *previous_bo;
  guint32 previous_fb_id;
};

struct _GdkDrmGLContextClass
{
  GdkGLContextClass parent_class;
};

GdkGLContext *_gdk_drm_gl_context_new (GdkDrmDisplay  *display,
                                       GError        **error);

G_END_DECLS
