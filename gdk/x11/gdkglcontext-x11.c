/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-x11.c: X11 specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
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

#include "gdkglcontext-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"

#include "gdkx11display.h"
#include "gdkx11glcontext.h"
#include "gdkx11screen.h"
#include "gdkx11surface.h"
#include "gdkvisual-x11.h"
#include "gdkx11property.h"
#include <X11/Xatom.h>

#include "gdkinternals.h"

#include "gdkintl.h"

#include <cairo-xlib.h>

#include <epoxy/glx.h>

G_DEFINE_ABSTRACT_TYPE (GdkX11GLContext, gdk_x11_gl_context, GDK_TYPE_GL_CONTEXT)

static void
gdk_x11_gl_context_class_init (GdkX11GLContextClass *klass)
{
}

static void
gdk_x11_gl_context_init (GdkX11GLContext *self)
{
  self->do_frame_sync = TRUE;
}

GdkGLContext *
gdk_x11_surface_create_gl_context (GdkSurface    *surface,
                                   gboolean       attached,
                                   GdkGLContext  *share,
                                   GError       **error)
{
  GdkX11GLContext *context = NULL;
  GdkX11Display *display_x11;
  GdkDisplay *display;

  display = gdk_surface_get_display (surface);
  display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->egl_display)
    context = gdk_x11_gl_context_egl_new (surface, attached, share, error);
  else if (display_x11->have_glx)
    context = gdk_x11_gl_context_glx_new (surface, attached, share, error);
  else
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (context == NULL)
    return NULL;

  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

gboolean
gdk_x11_display_make_gl_context_current (GdkDisplay   *display,
                                         GdkGLContext *context)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->egl_display)
    return gdk_x11_gl_context_egl_make_current (display, context);
  else if (display_x11->have_glx)
    return gdk_x11_gl_context_glx_make_current (display, context);
  else
    g_assert_not_reached ();

  return FALSE;
}

void
gdk_x11_display_init_gl (GdkX11Display *self)
{
  GdkDisplay *display G_GNUC_UNUSED = GDK_DISPLAY (self);

  if (GDK_DISPLAY_DEBUG_CHECK (display, GL_DISABLE))
    return;

  if (!GDK_DISPLAY_DEBUG_CHECK (display, GL_GLX))
    {
      /* We favour EGL */
      if (gdk_x11_display_init_egl (self))
        return;
    }

  if (gdk_x11_display_init_glx (self))
    return;
}

