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

#ifdef HAVE_XDAMAGE
static void
finish_frame (GdkGLContext *context)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);

  if (context_x11->xdamage == 0)
    return;

  if (context_x11->frame_fence == 0)
    return;

  glDeleteSync (context_x11->frame_fence);
  context_x11->frame_fence = 0;
  _gdk_x11_surface_set_frame_still_painting (surface, FALSE);
}

static gboolean
on_gl_surface_xevent (GdkGLContext   *context,
                      XEvent         *xevent,
                      GdkX11Display  *display_x11)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  XDamageNotifyEvent *damage_xevent;

  if (!context_x11->is_attached)
    return FALSE;

  if (xevent->type != (display_x11->damage_event_base + XDamageNotify))
    return FALSE;

  damage_xevent = (XDamageNotifyEvent *) xevent;

  if (damage_xevent->damage != context_x11->xdamage)
    return FALSE;

  if (context_x11->frame_fence)
    {
      GdkX11GLContextClass *context_class = GDK_X11_GL_CONTEXT_GET_CLASS (context);
      GLenum wait_result;

      context_class->bind_for_frame_fence (context_x11);

      wait_result = glClientWaitSync (context_x11->frame_fence, 0, 0);

      switch (wait_result)
        {
          /* We assume that if the fence has been signaled, that this damage
           * event is the damage event that was triggered by the GL drawing
           * associated with the fence. That's, technically, not necessarly
           * always true. The X server could have generated damage for
           * an unrelated event (say the size of the window changing), at
           * just the right moment such that we're picking it up instead.
           *
           * We're choosing not to handle this edge case, but if it does ever
           * happen in the wild, it could lead to slight underdrawing by
           * the compositor for one frame. In the future, if we find out
           * this edge case is noticeable, we can compensate by copying the
           * painted region from gdk_x11_gl_context_end_frame and subtracting
           * damaged areas from the copy as they come in. Once the copied
           * region goes empty, we know that there won't be any underdraw,
           * and can mark painting has finished. It's not worth the added
           * complexity and resource usage to do this bookkeeping, however,
           * unless the problem is practically visible.
           */
          case GL_ALREADY_SIGNALED:
          case GL_CONDITION_SATISFIED:
          case GL_WAIT_FAILED:
            if (wait_result == GL_WAIT_FAILED)
              g_warning ("failed to wait on GL fence associated with last swap buffers call");
            finish_frame (context);
            break;

          /* We assume that if the fence hasn't been signaled, that this
           * damage event is not the damage event that was triggered by the
           * GL drawing associated with the fence. That's only true for
           * the Nvidia vendor driver. When using open source drivers, damage
           * is emitted immediately on swap buffers, before the fence ever
           * has a chance to signal.
           */
          case GL_TIMEOUT_EXPIRED:
            break;
          default:
            g_error ("glClientWaitSync returned unexpected result: %x", (guint) wait_result);
        }
    }

  return FALSE;
}

static void
on_surface_state_changed (GdkGLContext *context)
{
  GdkSurface *surface = gdk_gl_context_get_surface (context);

  if (GDK_SURFACE_IS_MAPPED (surface))
    return;

  /* If we're about to withdraw the surface, then we don't care if the frame is
   * still getting rendered by the GPU. The compositor is going to remove the surface
   * from the scene anyway, so wrap up the frame.
   */
  finish_frame (context);
}
#endif

static gboolean
gdk_x11_gl_context_realize (GdkGLContext  *context,
                            GError       **error)
{
#ifdef HAVE_XDAMAGE
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  Display *dpy = gdk_x11_display_get_xdisplay (display);

  if (display_x11->have_damage &&
      display_x11->have_glx &&
      display_x11->has_async_glx_swap_buffers)
    {
      gdk_x11_display_error_trap_push (display);
      context_x11->xdamage = XDamageCreate (dpy,
                                            gdk_x11_surface_get_xid (surface),
                                            XDamageReportRawRectangles);
      if (gdk_x11_display_error_trap_pop (display))
        {
          context_x11->xdamage = 0;
        }
      else
        {
          g_signal_connect_object (G_OBJECT (display),
                                   "xevent",
                                   G_CALLBACK (on_gl_surface_xevent),
                                   context,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (G_OBJECT (surface),
                                   "notify::state",
                                   G_CALLBACK (on_surface_state_changed),
                                   context,
                                   G_CONNECT_SWAPPED);

        }
    }
#endif

  return TRUE;
}

static void
gdk_x11_gl_context_dispose (GObject *gobject)
{
  GdkX11GLContext *self = GDK_X11_GL_CONTEXT (gobject);

#ifdef HAVE_XDAMAGE
  self->xdamage = 0;
#endif

  G_OBJECT_CLASS (gdk_x11_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_x11_gl_context_real_bind_for_frame_fence (GdkX11GLContext *self)
{
}

static void
gdk_x11_gl_context_class_init (GdkX11GLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_x11_gl_context_dispose;

  context_class->realize = gdk_x11_gl_context_realize;

  klass->bind_for_frame_fence = gdk_x11_gl_context_real_bind_for_frame_fence;
}

static void
gdk_x11_gl_context_init (GdkX11GLContext *self)
{
  self->do_frame_sync = TRUE;
}

gboolean
gdk_x11_screen_init_gl (GdkX11Screen *screen)
{
  GdkDisplay *display G_GNUC_UNUSED = GDK_SCREEN_DISPLAY (screen);

  if (GDK_DISPLAY_DEBUG_CHECK (display, GL_DISABLE))
    return FALSE;

  if (!GDK_DISPLAY_DEBUG_CHECK (display, GL_GLX))
    {
      /* We favour EGL */
      if (gdk_x11_screen_init_egl (screen))
        return TRUE;
    }

  if (gdk_x11_screen_init_glx (screen))
    return TRUE;

  return FALSE;
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

  if (!gdk_x11_screen_init_gl (GDK_SURFACE_SCREEN (surface)))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  display_x11 = GDK_X11_DISPLAY (display);
  if (display_x11->have_egl)
    context = gdk_x11_gl_context_egl_new (surface, attached, share, error);
  else if (display_x11->have_glx)
    context = gdk_x11_gl_context_glx_new (surface, attached, share, error);
  else
    g_assert_not_reached ();

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

  if (display_x11->have_egl)
    return gdk_x11_gl_context_egl_make_current (display, context);
  else if (display_x11->have_glx)
    return gdk_x11_gl_context_glx_make_current (display, context);
  else
    g_assert_not_reached ();

  return FALSE;
}
