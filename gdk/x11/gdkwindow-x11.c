/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkwindow-x11.h"

#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkvisualprivate.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkframeclockprivate.h"
#include "gdkasync.h"
#include "gdkeventsource.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cairo-xlib.h>

#include "MwmUtil.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <X11/extensions/shape.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

const int _gdk_x11_event_mask_table[21] =
{
  ExposureMask,
  PointerMotionMask,
  PointerMotionHintMask,
  ButtonMotionMask,
  Button1MotionMask,
  Button2MotionMask,
  Button3MotionMask,
  ButtonPressMask,
  ButtonReleaseMask,
  KeyPressMask,
  KeyReleaseMask,
  EnterWindowMask,
  LeaveWindowMask,
  FocusChangeMask,
  StructureNotifyMask,
  PropertyChangeMask,
  VisibilityChangeMask,
  0,                    /* PROXIMITY_IN */
  0,                    /* PROXIMTY_OUT */
  SubstructureNotifyMask,
  ButtonPressMask      /* SCROLL; on X mouse wheel events is treated as mouse button 4/5 */
};

const gint _gdk_x11_event_mask_table_size = G_N_ELEMENTS (_gdk_x11_event_mask_table);

/* Forward declarations */
static void     gdk_x11_window_apply_fullscreen_mode (GdkWindow  *window);
static void     gdk_window_set_static_win_gravity (GdkWindow  *window,
						   gboolean    on);
static gboolean gdk_window_icon_name_set          (GdkWindow  *window);
static void     set_wm_name                       (GdkDisplay  *display,
						   Window       xwindow,
						   const gchar *name);
static void     move_to_current_desktop           (GdkWindow *window);
static void     gdk_window_x11_set_background     (GdkWindow      *window,
                                                   cairo_pattern_t *pattern);

static void        gdk_window_impl_x11_finalize   (GObject            *object);

#define WINDOW_IS_TOPLEVEL_OR_FOREIGN(window)           \
  (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL ||   \
   GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP ||       \
   GDK_WINDOW_TYPE (window) == GDK_WINDOW_FOREIGN)

#define WINDOW_IS_TOPLEVEL(window)                      \
  (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL ||   \
   GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)

/* Return whether time1 is considered later than time2 as far as xserver
 * time is concerned.  Accounts for wraparound.
 */
#define XSERVER_TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

struct _GdkX11Window {
  GdkWindow parent;
};

struct _GdkX11WindowClass {
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkX11Window, gdk_x11_window, GDK_TYPE_WINDOW)

static void
gdk_x11_window_class_init (GdkX11WindowClass *x11_window_class)
{
}

static void
gdk_x11_window_init (GdkX11Window *x11_window)
{
}


G_DEFINE_TYPE (GdkWindowImplX11, gdk_window_impl_x11, GDK_TYPE_WINDOW_IMPL)

static void
gdk_window_impl_x11_init (GdkWindowImplX11 *impl)
{  
  impl->device_cursor = g_hash_table_new_full (NULL, NULL,
                                               NULL, g_object_unref);
  impl->window_scale = 1;
  impl->frame_sync_enabled = TRUE;
}

GdkToplevelX11 *
_gdk_x11_window_get_toplevel (GdkWindow *window)
{
  GdkWindowImplX11 *impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (!WINDOW_IS_TOPLEVEL (window))
    return NULL;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (!impl->toplevel)
    {
      impl->toplevel = g_new0 (GdkToplevelX11, 1);
      impl->toplevel->have_focused = TRUE;
    }

  return impl->toplevel;
}

static const cairo_user_data_key_t gdk_x11_cairo_key;

/**
 * _gdk_x11_window_update_size:
 * @impl: a #GdkWindowImplX11.
 * 
 * Updates the state of the window (in particular the drawable's
 * cairo surface) when its size has changed.
 **/
void
_gdk_x11_window_update_size (GdkWindowImplX11 *impl)
{
  if (impl->cairo_surface)
    {
      cairo_xlib_surface_set_size (impl->cairo_surface,
                                   gdk_window_get_width (impl->wrapper) * impl->window_scale,
                                   gdk_window_get_height (impl->wrapper) * impl->window_scale);
    }
}

static void
set_sync_counter(Display     *display,
		 XSyncCounter counter,
                 gint64       value)
{
    XSyncValue sync_value;

    XSyncIntsToValue(&sync_value,
                     value & G_GINT64_CONSTANT(0xFFFFFFFF),
                     value >> 32);
    XSyncSetCounter(display, counter, sync_value);
}

static void
window_pre_damage (GdkWindow *window)
{
  GdkWindow *toplevel_window = gdk_window_get_toplevel (window);
  GdkWindowImplX11 *impl;

  if (!toplevel_window || !WINDOW_IS_TOPLEVEL (toplevel_window))
    return;

  impl = GDK_WINDOW_IMPL_X11 (toplevel_window->impl);

  if (impl->toplevel->in_frame &&
      impl->toplevel->current_counter_value % 2 == 0)
    {
      impl->toplevel->current_counter_value += 1;
      set_sync_counter(GDK_WINDOW_XDISPLAY (impl->wrapper),
		       impl->toplevel->extended_update_counter,
		       impl->toplevel->current_counter_value);
    }
}

static void
on_surface_changed (void *data)
{
  GdkWindow *window = data;

  window_pre_damage (window);
}

/* We want to know when cairo drawing causes damage to the window,
 * so we engage in the _NET_WM_FRAME_DRAWN protocol with the
 * window only when there actually is drawing. To do that we use
 * a technique (hack) suggested by Uli Schlachter - if we set
 * a dummy "mime data" on the cairo surface (this facility is
 * used to attach JPEG data to an imager), then cairo wil flush
 * and remove the mime data before making any changes to the window.
 */

static void
hook_surface_changed (GdkWindow *window)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (impl->cairo_surface)
    cairo_surface_set_mime_data (impl->cairo_surface,
                                 "x-gdk/change-notify",
                                 (unsigned char *)"X",
                                 1,
                                 on_surface_changed,
                                 window);
}

static void
unhook_surface_changed (GdkWindow *window)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (impl->cairo_surface)
    cairo_surface_set_mime_data (impl->cairo_surface,
                                 "x-gdk/change-notify",
                                 NULL, 0,
                                 NULL, NULL);
}

static void
gdk_x11_window_predict_presentation_time (GdkWindow *window)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  GdkFrameClock *clock;
  GdkFrameTimings *timings;
  gint64 presentation_time;
  gint64 refresh_interval;

  if (!WINDOW_IS_TOPLEVEL (window))
    return;

  clock = gdk_window_get_frame_clock (window);

  timings = gdk_frame_clock_get_current_timings (clock);

  gdk_frame_clock_get_refresh_info (clock,
                                    timings->frame_time,
                                    &refresh_interval, &presentation_time);

  if (presentation_time != 0)
    {
      if (timings->slept_before)
        {
          presentation_time += refresh_interval;
        }
      else
        {
          if (presentation_time < timings->frame_time + refresh_interval / 2)
            presentation_time += refresh_interval;
        }
    }
  else
    {
      if (timings->slept_before)
        presentation_time = timings->frame_time + refresh_interval + refresh_interval / 2;
      else
        presentation_time = timings->frame_time + refresh_interval;
    }

  if (presentation_time < impl->toplevel->throttled_presentation_time)
    presentation_time = impl->toplevel->throttled_presentation_time;

  timings->predicted_presentation_time = presentation_time;
}

static void
gdk_x11_window_begin_frame (GdkWindow *window,
                            gboolean   force_frame)
{
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (!WINDOW_IS_TOPLEVEL (window) ||
      impl->toplevel->extended_update_counter == None)
    return;

  impl->toplevel->in_frame = TRUE;

  if (impl->toplevel->configure_counter_value != 0 &&
      impl->toplevel->configure_counter_value_is_extended)
    {
      impl->toplevel->current_counter_value = impl->toplevel->configure_counter_value;
      if ((impl->toplevel->current_counter_value % 2) == 1)
        impl->toplevel->current_counter_value += 1;

      impl->toplevel->configure_counter_value = 0;

      window_pre_damage (window);
    }
  else if (force_frame)
    {
      /* When mapping the window, we really want to freeze the
         rendering of the window by the compositor until we've
         actually painted something into the window's buffer. */
      window_pre_damage (window);
    }
  else
    {
      hook_surface_changed (window);
    }
}

static void
gdk_x11_window_end_frame (GdkWindow *window)
{
  GdkFrameClock *clock;
  GdkFrameTimings *timings;
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (!WINDOW_IS_TOPLEVEL (window) ||
      impl->toplevel->extended_update_counter == None ||
      !impl->toplevel->in_frame)
    return;

  clock = gdk_window_get_frame_clock (window);
  timings = gdk_frame_clock_get_current_timings (clock);

  impl->toplevel->in_frame = FALSE;

  if (impl->toplevel->current_counter_value % 2 == 1)
    {
#ifdef G_ENABLE_DEBUG
      if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
        {
          XImage *image = XGetImage (GDK_WINDOW_XDISPLAY (window),
                                     GDK_WINDOW_XID (window),
                                     0, 0, 1, 1,
                                     (1 << 24) - 1,
                                     ZPixmap);
          XDestroyImage (image);
        }
#endif /* G_ENABLE_DEBUG */

      /* An increment of 3 means that the frame was not drawn as fast as possible,
       * but rather at a particular time. This can trigger different handling from
       * the compositor.
       */
      if (timings->slept_before)
        impl->toplevel->current_counter_value += 3;
      else
        impl->toplevel->current_counter_value += 1;

      set_sync_counter(GDK_WINDOW_XDISPLAY (impl->wrapper),
		       impl->toplevel->extended_update_counter,
		       impl->toplevel->current_counter_value);

      if (impl->frame_sync_enabled &&
          gdk_x11_screen_supports_net_wm_hint (gdk_window_get_screen (window),
					       gdk_atom_intern_static_string ("_NET_WM_FRAME_DRAWN")))
        {
          impl->toplevel->frame_pending = TRUE;
          _gdk_frame_clock_freeze (gdk_window_get_frame_clock (window));
          timings->cookie = impl->toplevel->current_counter_value;
        }
    }

  unhook_surface_changed (window);

  if (impl->toplevel->configure_counter_value != 0 &&
      !impl->toplevel->configure_counter_value_is_extended)
    {
      set_sync_counter (GDK_WINDOW_XDISPLAY (window),
                        impl->toplevel->update_counter,
                        impl->toplevel->configure_counter_value);

      impl->toplevel->configure_counter_value = 0;
    }

  if (!impl->toplevel->frame_pending)
    timings->complete = TRUE;
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static void
gdk_x11_cairo_surface_destroy (void *data)
{
  GdkWindowImplX11 *impl = data;

  impl->cairo_surface = NULL;
}

static cairo_surface_t *
gdk_x11_create_cairo_surface (GdkWindowImplX11 *impl,
			      int width,
			      int height)
{
  GdkVisual *visual;
    
  visual = gdk_window_get_visual (impl->wrapper);
  return cairo_xlib_surface_create (GDK_WINDOW_XDISPLAY (impl->wrapper),
                                    GDK_WINDOW_IMPL_X11 (impl)->xid,
                                    GDK_VISUAL_XVISUAL (visual),
                                    width, height);
}

static cairo_surface_t *
gdk_x11_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  if (!impl->cairo_surface)
    {
      impl->cairo_surface = gdk_x11_create_cairo_surface (impl,
                                                          gdk_window_get_width (window) * impl->window_scale,
                                                          gdk_window_get_height (window) * impl->window_scale);
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
      cairo_surface_set_device_scale (impl->cairo_surface, impl->window_scale, impl->window_scale);
#endif
      
      if (impl->cairo_surface)
	cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				     impl, gdk_x11_cairo_surface_destroy);

      if (WINDOW_IS_TOPLEVEL (window) && impl->toplevel->in_frame)
        hook_surface_changed (window);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static void
gdk_window_impl_x11_finalize (GObject *object)
{
  GdkWindow *wrapper;
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (object));

  impl = GDK_WINDOW_IMPL_X11 (object);

  wrapper = impl->wrapper;

  if (WINDOW_IS_TOPLEVEL (wrapper) && impl->toplevel->in_frame)
    unhook_surface_changed (wrapper);

  _gdk_x11_window_grab_check_destroy (wrapper);

  if (!GDK_WINDOW_DESTROYED (wrapper))
    {
      GdkDisplay *display = GDK_WINDOW_DISPLAY (wrapper);

      _gdk_x11_display_remove_window (display, impl->xid);
      if (impl->toplevel && impl->toplevel->focus_window)
        _gdk_x11_display_remove_window (display, impl->toplevel->focus_window);
    }

  g_free (impl->toplevel);

  if (impl->cursor)
    g_object_unref (impl->cursor);

  g_hash_table_destroy (impl->device_cursor);

  G_OBJECT_CLASS (gdk_window_impl_x11_parent_class)->finalize (object);
}

typedef struct {
  GdkDisplay *display;
  Pixmap pixmap;
} FreePixmapData;

static void
free_pixmap (gpointer datap)
{
  FreePixmapData *data = datap;

  if (!gdk_display_is_closed (data->display))
    {
      XFreePixmap (GDK_DISPLAY_XDISPLAY (data->display),
                   data->pixmap);
    }

  g_object_unref (data->display);
  g_slice_free (FreePixmapData, data);
}

static void
attach_free_pixmap_handler (cairo_surface_t *surface,
                            GdkDisplay      *display,
                            Pixmap           pixmap)
{
  static const cairo_user_data_key_t key;
  FreePixmapData *data;
  
  data = g_slice_new (FreePixmapData);
  data->display = g_object_ref (display);
  data->pixmap = pixmap;

  cairo_surface_set_user_data (surface, &key, data, free_pixmap);
}

/* Cairo does not guarantee we get an xlib surface if we call
 * cairo_surface_create_similar(). In some cases however, we must use a
 * pixmap or bitmap in the X11 API.
 * These functions ensure an Xlib surface.
 */
cairo_surface_t *
_gdk_x11_window_create_bitmap_surface (GdkWindow *window,
                                       int        width,
                                       int        height)
{
  cairo_surface_t *surface;
  Pixmap pixmap;

  pixmap = XCreatePixmap (GDK_WINDOW_XDISPLAY (window),
                          GDK_WINDOW_XID (window),
                          width, height, 1);
  surface = cairo_xlib_surface_create_for_bitmap (GDK_WINDOW_XDISPLAY (window),
                                                  pixmap,
                                                  GDK_X11_SCREEN (GDK_WINDOW_SCREEN (window))->xscreen,
                                                  width, height);
  attach_free_pixmap_handler (surface, GDK_WINDOW_DISPLAY (window), pixmap);

  return surface;
}

/* Create a surface backed with a pixmap without alpha on the same screen as window */
static cairo_surface_t *
gdk_x11_window_create_pixmap_surface (GdkWindow *window,
                                      int        width,
                                      int        height)
{
  GdkScreen *screen = gdk_window_get_screen (window);
  GdkVisual *visual = gdk_screen_get_system_visual (screen);
  cairo_surface_t *surface;
  Pixmap pixmap;

  pixmap = XCreatePixmap (GDK_WINDOW_XDISPLAY (window),
                          GDK_WINDOW_XID (window),
                          width, height,
                          gdk_visual_get_depth (visual));
  surface = cairo_xlib_surface_create (GDK_WINDOW_XDISPLAY (window),
                                       pixmap,
                                       GDK_VISUAL_XVISUAL (visual),
                                       width, height);
  attach_free_pixmap_handler (surface, GDK_WINDOW_DISPLAY (window), pixmap);

  return surface;
}

static void
tmp_unset_bg (GdkWindow *window)
{
  GdkWindowImplX11 *impl;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  impl->no_bg = TRUE;

  XSetWindowBackgroundPixmap (GDK_WINDOW_XDISPLAY (window),
                              GDK_WINDOW_XID (window), None);
}

static void
tmp_reset_bg (GdkWindow *window)
{
  GdkWindowImplX11 *impl;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  impl->no_bg = FALSE;

  gdk_window_x11_set_background (window, window->background);
}

/* Unsetting and resetting window backgrounds.
 *
 * In many cases it is possible to avoid flicker by unsetting the
 * background of windows. For example if the background of the
 * parent window is unset when a window is unmapped, a brief flicker
 * of background painting is avoided.
 */
void
_gdk_x11_window_tmp_unset_bg (GdkWindow *window,
			      gboolean   recurse)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (window->input_only || window->destroyed ||
      (window->window_type != GDK_WINDOW_ROOT &&
       !GDK_WINDOW_IS_MAPPED (window)))
    return;
  
  if (_gdk_window_has_impl (window) &&
      GDK_WINDOW_IS_X11 (window) &&
      window->window_type != GDK_WINDOW_ROOT &&
      window->window_type != GDK_WINDOW_FOREIGN)
    tmp_unset_bg (window);

  if (recurse)
    {
      GList *l;

      for (l = window->children; l != NULL; l = l->next)
	_gdk_x11_window_tmp_unset_bg (l->data, TRUE);
    }
}

void
_gdk_x11_window_tmp_unset_parent_bg (GdkWindow *window)
{
  if (GDK_WINDOW_TYPE (window->parent) == GDK_WINDOW_ROOT)
    return;
  
  window = _gdk_window_get_impl_window (window->parent);
  _gdk_x11_window_tmp_unset_bg (window,	FALSE);
}

void
_gdk_x11_window_tmp_reset_bg (GdkWindow *window,
			      gboolean   recurse)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->input_only || window->destroyed ||
      (window->window_type != GDK_WINDOW_ROOT &&
       !GDK_WINDOW_IS_MAPPED (window)))
    return;

  
  if (_gdk_window_has_impl (window) &&
      GDK_WINDOW_IS_X11 (window) &&
      window->window_type != GDK_WINDOW_ROOT &&
      window->window_type != GDK_WINDOW_FOREIGN)
    tmp_reset_bg (window);

  if (recurse)
    {
      GList *l;

      for (l = window->children; l != NULL; l = l->next)
	_gdk_x11_window_tmp_reset_bg (l->data, TRUE);
    }
}

void
_gdk_x11_window_tmp_reset_parent_bg (GdkWindow *window)
{
  if (GDK_WINDOW_TYPE (window->parent) == GDK_WINDOW_ROOT)
    return;
  
  window = _gdk_window_get_impl_window (window->parent);

  _gdk_x11_window_tmp_reset_bg (window, FALSE);
}

void
_gdk_x11_screen_init_root_window (GdkScreen *screen)
{
  GdkWindow *window;
  GdkWindowImplX11 *impl;
  GdkX11Screen *x11_screen;

  x11_screen = GDK_X11_SCREEN (screen);

  g_assert (x11_screen->root_window == NULL);

  window = x11_screen->root_window = _gdk_display_create_window (gdk_screen_get_display (screen));

  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_X11, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_X11 (window->impl);
  
  impl->xid = x11_screen->xroot_window;
  impl->wrapper = window;
  impl->window_scale = x11_screen->window_scale;
  
  window->window_type = GDK_WINDOW_ROOT;
  window->depth = DefaultDepthOfScreen (x11_screen->xscreen);

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  window->width = WidthOfScreen (x11_screen->xscreen) / impl->window_scale;
  window->height = HeightOfScreen (x11_screen->xscreen) / impl->window_scale;
  window->viewable = TRUE;

  /* see init_randr_support() in gdkscreen-x11.c */
  window->event_mask = GDK_STRUCTURE_MASK;

  _gdk_window_update_size (x11_screen->root_window);

  _gdk_x11_display_add_window (x11_screen->display,
                               &x11_screen->xroot_window,
                               x11_screen->root_window);
}

static void
set_wm_protocols (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  Atom protocols[4];
  int n = 0;
  
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW");
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS");
  protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PING");

#ifdef HAVE_XSYNC
  if (GDK_X11_DISPLAY (display)->use_sync)
    protocols[n++] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_SYNC_REQUEST");
#endif
  
  XSetWMProtocols (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window), protocols, n);
}

static const gchar *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();
  if (!title)
    title = "";

  return title;
}

static void
check_leader_window_title (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->leader_window && !display_x11->leader_window_title_set)
    {
      set_wm_name (display,
		   display_x11->leader_window,
		   get_default_title ());
      
      display_x11->leader_window_title_set = TRUE;
    }
}

static Window
create_focus_window (GdkDisplay *display,
		     XID         parent)
{
  GdkX11Display *display_x11;
  GdkEventMask event_mask;
  Display *xdisplay;
  Window focus_window;
  XSetWindowAttributes attrs;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  display_x11 = GDK_X11_DISPLAY (display);

  focus_window = XCreateWindow (xdisplay, parent,
                                -1, -1, 1, 1, 0,
                                0, /* depth */
                                InputOnly,
                                CopyFromParent,
                                0, &attrs);

  event_mask = (GDK_KEY_PRESS_MASK |
                GDK_KEY_RELEASE_MASK |
                GDK_FOCUS_CHANGE_MASK);

  gdk_x11_event_source_select_events ((GdkEventSource *) display_x11->event_source,
                                      focus_window,
                                      event_mask, 0);

  XMapWindow (xdisplay, focus_window);

  return focus_window;
}

static void
ensure_sync_counter (GdkWindow *window)
{
#ifdef HAVE_XSYNC
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
      GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);

      if (toplevel &&
	  toplevel->update_counter == None &&
	  GDK_X11_DISPLAY (display)->use_sync)
	{
	  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
	  XSyncValue value;
	  Atom atom;
	  XID counters[2];

	  XSyncIntToValue (&value, 0);
	  
	  toplevel->update_counter = XSyncCreateCounter (xdisplay, value);
	  toplevel->extended_update_counter = XSyncCreateCounter (xdisplay, value);
	  
	  atom = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_SYNC_REQUEST_COUNTER");

	  counters[0] = toplevel->update_counter;
	  counters[1] = toplevel->extended_update_counter;
	  XChangeProperty (xdisplay, GDK_WINDOW_XID (window),
			   atom, XA_CARDINAL,
			   32, PropModeReplace,
			   (guchar *)counters, 2);
	  
	  toplevel->current_counter_value = 0;
	}
    }
#endif
}

static void
setup_toplevel_window (GdkWindow *window, 
		       GdkWindow *parent)
{
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  GdkDisplay *display = gdk_window_get_display (window);
  Display *xdisplay = GDK_WINDOW_XDISPLAY (window);
  XID xid = GDK_WINDOW_XID (window);
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (GDK_WINDOW_SCREEN (parent));
  XSizeHints size_hints;
  long pid;
  Window leader_window;

  set_wm_protocols (window);

  if (!window->input_only)
    {
      /* The focus window is off the visible area, and serves to receive key
       * press events so they don't get sent to child windows.
       */
      toplevel->focus_window = create_focus_window (display, xid);
      _gdk_x11_display_add_window (x11_screen->display,
                                   &toplevel->focus_window,
                                   window);
    }

  check_leader_window_title (x11_screen->display);

  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  size_hints.flags = PSize;
  size_hints.width = window->width * impl->window_scale;
  size_hints.height = window->height * impl->window_scale;
  
  XSetWMNormalHints (xdisplay, xid, &size_hints);
  
  /* This will set WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (xdisplay, xid, NULL, NULL, NULL, 0, NULL, NULL, NULL);
  
  pid = getpid ();
  XChangeProperty (xdisplay, xid,
		   gdk_x11_get_xatom_by_name_for_display (x11_screen->display, "_NET_WM_PID"),
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *)&pid, 1);

  leader_window = GDK_X11_DISPLAY (x11_screen->display)->leader_window;
  if (!leader_window)
    leader_window = xid;
  XChangeProperty (xdisplay, xid, 
		   gdk_x11_get_xatom_by_name_for_display (x11_screen->display, "WM_CLIENT_LEADER"),
		   XA_WINDOW, 32, PropModeReplace,
		   (guchar *) &leader_window, 1);

  if (toplevel->focus_window != None)
    XChangeProperty (xdisplay, xid, 
                     gdk_x11_get_xatom_by_name_for_display (x11_screen->display, "_NET_WM_USER_TIME_WINDOW"),
                     XA_WINDOW, 32, PropModeReplace,
                     (guchar *) &toplevel->focus_window, 1);

  if (!window->focus_on_map)
    gdk_x11_window_set_user_time (window, 0);
  else if (GDK_X11_DISPLAY (x11_screen->display)->user_time != 0)
    gdk_x11_window_set_user_time (window, GDK_X11_DISPLAY (x11_screen->display)->user_time);

  ensure_sync_counter (window);

  /* Start off in a frozen state - we'll finish this when we first paint */
  gdk_x11_window_begin_frame (window, TRUE);
}

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkWindow     *window)
{
  gdk_x11_window_predict_presentation_time (window);
  gdk_x11_window_begin_frame (window, FALSE);
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkWindow     *window)
{
  gdk_x11_window_end_frame (window);

}

static void
connect_frame_clock (GdkWindow *window)
{
  GdkWindowImplX11 *impl;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);
  if (WINDOW_IS_TOPLEVEL (window) && !impl->frame_clock_connected)
    {
      GdkFrameClock *frame_clock = gdk_window_get_frame_clock (window);

      g_signal_connect (frame_clock, "before-paint",
                        G_CALLBACK (on_frame_clock_before_paint), window);
      g_signal_connect (frame_clock, "after-paint",
                        G_CALLBACK (on_frame_clock_after_paint), window);

      impl->frame_clock_connected = TRUE;
    }
}

void
_gdk_x11_display_create_window_impl (GdkDisplay    *display,
                                     GdkWindow     *window,
                                     GdkWindow     *real_parent,
                                     GdkScreen     *screen,
                                     GdkEventMask   event_mask,
                                     GdkWindowAttr *attributes,
                                     gint           attributes_mask)
{
  GdkWindowImplX11 *impl;
  GdkX11Screen *x11_screen;
  GdkX11Display *display_x11;

  Window xparent;
  Visual *xvisual;
  Display *xdisplay;

  XSetWindowAttributes xattributes;
  long xattributes_mask;
  XClassHint *class_hint;

  unsigned int class;
  const char *title;

  display_x11 = GDK_X11_DISPLAY (display);
  xparent = GDK_WINDOW_XID (real_parent);
  x11_screen = GDK_X11_SCREEN (screen);

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_X11, NULL);
  window->impl = GDK_WINDOW_IMPL (impl);
  impl->wrapper = GDK_WINDOW (window);
  impl->window_scale = x11_screen->window_scale;

  xdisplay = x11_screen->xdisplay;

  xattributes_mask = 0;

  xvisual = gdk_x11_visual_get_xvisual (window->visual);

  if (attributes_mask & GDK_WA_NOREDIR)
    {
      xattributes.override_redirect =
        (attributes->override_redirect == FALSE)?False:True;
      xattributes_mask |= CWOverrideRedirect;
    }
  else
    xattributes.override_redirect = False;

  impl->override_redirect = xattributes.override_redirect;

  if (window->parent && window->parent->guffaw_gravity)
    {
      xattributes.win_gravity = StaticGravity;
      xattributes_mask |= CWWinGravity;
    }

  /* Sanity checks */
  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (window->parent) != GDK_WINDOW_ROOT)
        {
          /* The common code warns for this case */
          xparent = GDK_SCREEN_XROOTWIN (screen);
        }
    }

  if (!window->input_only)
    {
      class = InputOutput;

      xattributes.background_pixel = BlackPixel (xdisplay, x11_screen->screen_num);

      xattributes.border_pixel = BlackPixel (xdisplay, x11_screen->screen_num);
      xattributes_mask |= CWBorderPixel | CWBackPixel;

      if (window->guffaw_gravity)
        xattributes.bit_gravity = StaticGravity;
      else
        xattributes.bit_gravity = NorthWestGravity;

      xattributes_mask |= CWBitGravity;

      xattributes.colormap = _gdk_visual_get_x11_colormap (window->visual);
      xattributes_mask |= CWColormap;

      if (window->window_type == GDK_WINDOW_TEMP)
        {
          xattributes.save_under = True;
          xattributes.override_redirect = True;
          xattributes.cursor = None;
          xattributes_mask |= CWSaveUnder | CWOverrideRedirect;

          impl->override_redirect = TRUE;
        }
    }
  else
    {
      class = InputOnly;
    }

  if (window->width * impl->window_scale > 32767 ||
      window->height * impl->window_scale > 32767)
    {
      g_warning ("Native Windows wider or taller than 32767 pixels are not supported");

      if (window->width * impl->window_scale > 32767)
        window->width = 32767 / impl->window_scale;
      if (window->height  * impl->window_scale > 32767)
        window->height = 32767 /  impl->window_scale;
    }

  impl->xid = XCreateWindow (xdisplay, xparent,
                             (window->x + window->parent->abs_x) * impl->window_scale,
                             (window->y + window->parent->abs_y) * impl->window_scale,
                             window->width * impl->window_scale, window->height * impl->window_scale,
                             0, window->depth, class, xvisual,
                             xattributes_mask, &xattributes);

  g_object_ref (window);
  _gdk_x11_display_add_window (x11_screen->display, &impl->xid, window);

  switch (GDK_WINDOW_TYPE (window))
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      if (attributes_mask & GDK_WA_TITLE)
        title = attributes->title;
      else
        title = get_default_title ();

      gdk_window_set_title (window, title);

      if (attributes_mask & GDK_WA_WMCLASS)
        {
          class_hint = XAllocClassHint ();
          class_hint->res_name = attributes->wmclass_name;
          class_hint->res_class = attributes->wmclass_class;
          XSetClassHint (xdisplay, impl->xid, class_hint);
          XFree (class_hint);
        }

      setup_toplevel_window (window, window->parent);
      break;

    case GDK_WINDOW_CHILD:
    default:
      break;
    }

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  gdk_x11_event_source_select_events ((GdkEventSource *) display_x11->event_source,
                                      GDK_WINDOW_XID (window), event_mask,
                                      StructureNotifyMask | PropertyChangeMask);

  connect_frame_clock (window);

  if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD)
    gdk_window_freeze_toplevel_updates_libgtk_only (window);
}

static GdkEventMask
x_event_mask_to_gdk_event_mask (long mask)
{
  GdkEventMask event_mask = 0;
  int i;

  for (i = 0; i < _gdk_x11_event_mask_table_size; i++)
    {
      if (mask & _gdk_x11_event_mask_table[i])
	event_mask |= 1 << (i + 1);
    }

  return event_mask;
}

/**
 * gdk_x11_window_foreign_new_for_display:
 * @display: (type GdkX11Display): the #GdkDisplay where the window handle comes from.
 * @window: an Xlib Window
 *
 * Wraps a native window in a #GdkWindow. The function will try to
 * look up the window using gdk_x11_window_lookup_for_display() first.
 * If it does not find it there, it will create a new window.
 *
 * This may fail if the window has been destroyed. If the window
 * was already known to GDK, a new reference to the existing
 * #GdkWindow is returned.
 *
 * Returns: (transfer full): a #GdkWindow wrapper for the native
 *   window, or %NULL if the window has been destroyed. The wrapper
 *   will be newly created, if one doesn’t exist already.
 *
 * Since: 2.24
 */
GdkWindow *
gdk_x11_window_foreign_new_for_display (GdkDisplay *display,
                                        Window      window)
{
  GdkScreen *screen;
  GdkWindow *win;
  GdkWindowImplX11 *impl;
  GdkX11Display *display_x11;
  XWindowAttributes attrs;
  Window root, parent;
  Window *children = NULL;
  guint nchildren;
  gboolean result;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  display_x11 = GDK_X11_DISPLAY (display);

  if ((win = gdk_x11_window_lookup_for_display (display, window)) != NULL)
    return g_object_ref (win);

  gdk_x11_display_error_trap_push (display);
  result = XGetWindowAttributes (display_x11->xdisplay, window, &attrs);
  if (gdk_x11_display_error_trap_pop (display) || !result)
    return NULL;

  /* FIXME: This is pretty expensive.
   * Maybe the caller should supply the parent
   */
  gdk_x11_display_error_trap_push (display);
  result = XQueryTree (display_x11->xdisplay, window, &root, &parent, &children, &nchildren);
  if (gdk_x11_display_error_trap_pop (display) || !result)
    return NULL;

  if (children)
    XFree (children);

  screen = _gdk_x11_display_screen_for_xrootwin (display, root);
  if (screen == NULL)
    return NULL;

  win = _gdk_display_create_window (display);
  win->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_X11, NULL);
  win->impl_window = win;
  win->visual = gdk_x11_screen_lookup_visual (screen,
                                              XVisualIDFromVisual (attrs.visual));

  impl = GDK_WINDOW_IMPL_X11 (win->impl);
  impl->wrapper = win;
  impl->window_scale = GDK_X11_SCREEN (screen)->window_scale;

  win->parent = gdk_x11_window_lookup_for_display (display, parent);

  if (!win->parent || GDK_WINDOW_TYPE (win->parent) == GDK_WINDOW_FOREIGN)
    win->parent = gdk_screen_get_root_window (screen);

  win->parent->children = g_list_prepend (win->parent->children, win);
  win->parent->impl_window->native_children =
    g_list_prepend (win->parent->impl_window->native_children, win);

  impl->xid = window;

  win->x = attrs.x / impl->window_scale;
  win->y = attrs.y / impl->window_scale;
  win->width = attrs.width / impl->window_scale;
  win->height = attrs.height  / impl->window_scale;
  win->window_type = GDK_WINDOW_FOREIGN;
  win->destroyed = FALSE;

  win->event_mask = x_event_mask_to_gdk_event_mask (attrs.your_event_mask);

  if (attrs.map_state == IsUnmapped)
    win->state = GDK_WINDOW_STATE_WITHDRAWN;
  else
    win->state = 0;
  win->viewable = TRUE;

  win->depth = attrs.depth;

  g_object_ref (win);
  _gdk_x11_display_add_window (display, &GDK_WINDOW_XID (win), win);

  /* Update the clip region, etc */
  _gdk_window_update_size (win);

  return win;
}

static void
gdk_toplevel_x11_free_contents (GdkDisplay *display,
				GdkToplevelX11 *toplevel)
{
  if (toplevel->icon_pixmap)
    {
      cairo_surface_destroy (toplevel->icon_pixmap);
      toplevel->icon_pixmap = NULL;
    }
  if (toplevel->icon_mask)
    {
      cairo_surface_destroy (toplevel->icon_mask);
      toplevel->icon_mask = NULL;
    }
  if (toplevel->group_leader)
    {
      g_object_unref (toplevel->group_leader);
      toplevel->group_leader = NULL;
    }
#ifdef HAVE_XSYNC
  if (toplevel->update_counter != None)
    {
      XSyncDestroyCounter (GDK_DISPLAY_XDISPLAY (display), 
			   toplevel->update_counter);
      toplevel->update_counter = None;

      toplevel->current_counter_value = 0;
    }
#endif
}

static void
gdk_x11_window_destroy (GdkWindow *window,
                        gboolean   recursing,
                        gboolean   foreign_destroy)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  GdkToplevelX11 *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));

  _gdk_x11_selection_window_destroyed (window);

  toplevel = _gdk_x11_window_get_toplevel (window);
  if (toplevel)
    gdk_toplevel_x11_free_contents (GDK_WINDOW_DISPLAY (window), toplevel);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
                                   NULL, NULL);
    }

  if (!recursing && !foreign_destroy)
    XDestroyWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

static void
gdk_x11_window_destroy_foreign (GdkWindow *window)
{
  /* It's somebody else's window, but in our hierarchy,
   * so reparent it to the root window, and then send
   * it a delete event, as if we were a WM
   */
  XClientMessageEvent xclient;
  GdkDisplay *display;

  display = GDK_WINDOW_DISPLAY (window);
  gdk_x11_display_error_trap_push (display);
  gdk_window_hide (window);
  gdk_window_reparent (window, NULL, 0, 0);

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "WM_PROTOCOLS");
  xclient.format = 32;
  xclient.data.l[0] = gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW");
  xclient.data.l[1] = CurrentTime;
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;
  
  XSendEvent (GDK_WINDOW_XDISPLAY (window),
              GDK_WINDOW_XID (window),
              False, 0, (XEvent *)&xclient);
  gdk_x11_display_error_trap_pop_ignored (display);
}

static GdkWindow *
get_root (GdkWindow *window)
{
  GdkScreen *screen = gdk_window_get_screen (window);

  return gdk_screen_get_root_window (screen);
}

/* This function is called when the XWindow is really gone.
 */
static void
gdk_x11_window_destroy_notify (GdkWindow *window)
{
  GdkWindowImplX11 *window_impl;

  window_impl = GDK_WINDOW_IMPL_X11 ((window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %#lx unexpectedly destroyed", GDK_WINDOW_XID (window));

      _gdk_window_destroy (window, TRUE);
    }

  _gdk_x11_display_remove_window (GDK_WINDOW_DISPLAY (window), GDK_WINDOW_XID (window));
  if (window_impl->toplevel && window_impl->toplevel->focus_window)
    _gdk_x11_display_remove_window (GDK_WINDOW_DISPLAY (window), window_impl->toplevel->focus_window);

  _gdk_x11_window_grab_check_destroy (window);

  g_object_unref (window);
}

static GdkDragProtocol
gdk_x11_window_get_drag_protocol (GdkWindow *window,
                                  GdkWindow **target)
{
  GdkDragProtocol protocol;
  GdkDisplay *display;
  guint version;
  Window xid;

  display = gdk_window_get_display (window);
  xid = _gdk_x11_display_get_drag_protocol (display,
                                            GDK_WINDOW_XID (window->impl_window),
                                            &protocol,
                                            &version);

  if (target)
    {
      if (xid != None)
        *target = gdk_x11_window_foreign_new_for_display (display, xid);
      else
        *target = NULL;
    }

  return protocol;
}

static void
update_wm_hints (GdkWindow *window,
		 gboolean   force)
{
  GdkToplevelX11 *toplevel = _gdk_x11_window_get_toplevel (window);
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  XWMHints wm_hints;

  if (!force &&
      !toplevel->is_leader &&
      window->state & GDK_WINDOW_STATE_WITHDRAWN)
    return;

  wm_hints.flags = StateHint | InputHint;
  wm_hints.input = window->accept_focus ? True : False;
  wm_hints.initial_state = NormalState;
  
  if (window->state & GDK_WINDOW_STATE_ICONIFIED)
    {
      wm_hints.flags |= StateHint;
      wm_hints.initial_state = IconicState;
    }

  if (toplevel->icon_pixmap)
    {
      wm_hints.flags |= IconPixmapHint;
      wm_hints.icon_pixmap = cairo_xlib_surface_get_drawable (toplevel->icon_pixmap);
    }

  if (toplevel->icon_mask)
    {
      wm_hints.flags |= IconMaskHint;
      wm_hints.icon_mask = cairo_xlib_surface_get_drawable (toplevel->icon_mask);
    }
  
  wm_hints.flags |= WindowGroupHint;
  if (toplevel->group_leader && !GDK_WINDOW_DESTROYED (toplevel->group_leader))
    {
      wm_hints.flags |= WindowGroupHint;
      wm_hints.window_group = GDK_WINDOW_XID (toplevel->group_leader);
    }
  else
    wm_hints.window_group = GDK_X11_DISPLAY (display)->leader_window;

  if (toplevel->urgency_hint)
    wm_hints.flags |= XUrgencyHint;
  
  XSetWMHints (GDK_WINDOW_XDISPLAY (window),
	       GDK_WINDOW_XID (window),
	       &wm_hints);
}

static void
set_initial_hints (GdkWindow *window)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  Window xwindow = GDK_WINDOW_XID (window);  
  GdkToplevelX11 *toplevel;
  Atom atoms[9];
  gint i;

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (!toplevel)
    return;

  update_wm_hints (window, TRUE);
  
  /* We set the spec hints regardless of whether the spec is supported,
   * since it can't hurt and it's kind of expensive to check whether
   * it's supported.
   */
  
  i = 0;

  if (window->state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MAXIMIZED_VERT");
      ++i;
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MAXIMIZED_HORZ");
      ++i;
      toplevel->have_maxhorz = toplevel->have_maxvert = TRUE;
    }

  if (window->state & GDK_WINDOW_STATE_ABOVE)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_ABOVE");
      ++i;
    }
  
  if (window->state & GDK_WINDOW_STATE_BELOW)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_BELOW");
      ++i;
    }
  
  if (window->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_STICKY");
      ++i;
      toplevel->have_sticky = TRUE;
    }

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_FULLSCREEN");
      ++i;
      toplevel->have_fullscreen = TRUE;
    }

  if (window->modal_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_MODAL");
      ++i;
    }

  if (toplevel->skip_taskbar_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_SKIP_TASKBAR");
      ++i;
    }

  if (toplevel->skip_pager_hint)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_SKIP_PAGER");
      ++i;
    }

  if (window->state & GDK_WINDOW_STATE_ICONIFIED)
    {
      atoms[i] = gdk_x11_get_xatom_by_name_for_display (display,
							"_NET_WM_STATE_HIDDEN");
      ++i;
      toplevel->have_hidden = TRUE;
    }

  if (i > 0)
    {
      XChangeProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
                       XA_ATOM, 32, PropModeReplace,
                       (guchar*) atoms, i);
    }
  else 
    {
      XDeleteProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"));
    }

  if (window->state & GDK_WINDOW_STATE_STICKY)
    {
      atoms[0] = 0xFFFFFFFF;
      XChangeProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"),
                       XA_CARDINAL, 32, PropModeReplace,
                       (guchar*) atoms, 1);
      toplevel->on_all_desktops = TRUE;
    }
  else
    {
      XDeleteProperty (xdisplay,
                       xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"));
    }

  toplevel->map_serial = NextRequest (xdisplay);
}

static void
gdk_window_x11_show (GdkWindow *window, gboolean already_mapped)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkToplevelX11 *toplevel;
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  Display *xdisplay = GDK_WINDOW_XDISPLAY (window);
  Window xwindow = GDK_WINDOW_XID (window);
  gboolean unset_bg;

  if (!already_mapped)
    set_initial_hints (window);
      
  if (WINDOW_IS_TOPLEVEL (window))
    {
      display = gdk_window_get_display (window);
      display_x11 = GDK_X11_DISPLAY (display);
      toplevel = _gdk_x11_window_get_toplevel (window);
      
      if (toplevel->user_time != 0 &&
	      display_x11->user_time != 0 &&
	  XSERVER_TIME_IS_LATER (display_x11->user_time, toplevel->user_time))
	gdk_x11_window_set_user_time (window, display_x11->user_time);
    }
  
  unset_bg = !window->input_only &&
    (window->window_type == GDK_WINDOW_CHILD ||
     impl->override_redirect) &&
    gdk_window_is_viewable (window);
  
  if (unset_bg)
    _gdk_x11_window_tmp_unset_bg (window, TRUE);
  
  XMapWindow (xdisplay, xwindow);
  
  if (unset_bg)
    _gdk_x11_window_tmp_reset_bg (window, TRUE);

  /* Fullscreen on current monitor is the default, no need to apply this mode
   * when mapping a window. This also ensures that the default behavior remains
   * consistent with pre-fullscreen mode implementation.
   */
  if (window->fullscreen_mode != GDK_FULLSCREEN_ON_CURRENT_MONITOR)
    gdk_x11_window_apply_fullscreen_mode (window);
}

static void
pre_unmap (GdkWindow *window)
{
  GdkWindow *start_window = NULL;

  if (window->input_only)
    return;

  if (window->window_type == GDK_WINDOW_CHILD)
    start_window = _gdk_window_get_impl_window ((GdkWindow *)window->parent);
  else if (window->window_type == GDK_WINDOW_TEMP)
    start_window = get_root (window);

  if (start_window)
    _gdk_x11_window_tmp_unset_bg (start_window, TRUE);
}

static void
post_unmap (GdkWindow *window)
{
  GdkWindow *start_window = NULL;
  
  if (window->input_only)
    return;

  if (window->window_type == GDK_WINDOW_CHILD)
    start_window = _gdk_window_get_impl_window ((GdkWindow *)window->parent);
  else if (window->window_type == GDK_WINDOW_TEMP)
    start_window = get_root (window);

  if (start_window)
    {
      _gdk_x11_window_tmp_reset_bg (start_window, TRUE);

      if (window->window_type == GDK_WINDOW_CHILD && window->parent)
	{
	  GdkRectangle invalid_rect;
      
	  gdk_window_get_position (window, &invalid_rect.x, &invalid_rect.y);
	  invalid_rect.width = gdk_window_get_width (window);
	  invalid_rect.height = gdk_window_get_height (window);
	  gdk_window_invalidate_rect ((GdkWindow *)window->parent,
				      &invalid_rect, TRUE);
	}
    }
}

static void
gdk_window_x11_hide (GdkWindow *window)
{
  /* We'll get the unmap notify eventually, and handle it then,
   * but checking here makes things more consistent if we are
   * just doing stuff ourself.
   */
  _gdk_x11_window_grab_check_unmap (window,
                                    NextRequest (GDK_WINDOW_XDISPLAY (window)));

  /* You can't simply unmap toplevel windows. */
  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP: /* ? */
      gdk_window_withdraw (window);
      return;
      
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_CHILD:
      break;
    }
  
  _gdk_window_clear_update_area (window);
  
  pre_unmap (window);
  XUnmapWindow (GDK_WINDOW_XDISPLAY (window),
		GDK_WINDOW_XID (window));
  post_unmap (window);
}

static void
gdk_window_x11_withdraw (GdkWindow *window)
{
  if (!window->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_WITHDRAWN);

      g_assert (!GDK_WINDOW_IS_MAPPED (window));

      pre_unmap (window);
      
      XWithdrawWindow (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window), 0);

      post_unmap (window);
    }
}

static inline void
window_x11_move (GdkWindow *window,
                 gint       x,
                 gint       y)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    {
      /* The window isn't actually damaged, but it's parent is */
      window_pre_damage (window);
      _gdk_x11_window_move_resize_child (window,
                                         x, y,
                                         window->width, window->height);
    }
  else
    {
      XMoveWindow (GDK_WINDOW_XDISPLAY (window),
                   GDK_WINDOW_XID (window),
                   x * impl->window_scale, y * impl->window_scale);

      if (impl->override_redirect)
        {
          window->x = x;
          window->y = y;
        }
    }
}

static inline void
window_x11_resize (GdkWindow *window,
                   gint       width,
                   gint       height)
{
  if (width < 1)
    width = 1;

  if (height < 1)
    height = 1;

  window_pre_damage (window);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    {
      _gdk_x11_window_move_resize_child (window,
                                         window->x, window->y,
                                         width, height);
    }
  else
    {
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

      XResizeWindow (GDK_WINDOW_XDISPLAY (window),
                     GDK_WINDOW_XID (window),
                     width * impl->window_scale, height * impl->window_scale);

      if (impl->override_redirect)
        {
          window->width = width;
          window->height = height;
          _gdk_x11_window_update_size (GDK_WINDOW_IMPL_X11 (window->impl));
        }
      else
        {
          if (width != window->width || height != window->height)
            window->resize_count += 1;
        }
    }
}

static inline void
window_x11_move_resize (GdkWindow *window,
                        gint       x,
                        gint       y,
                        gint       width,
                        gint       height)
{
  if (width < 1)
    width = 1;

  if (height < 1)
    height = 1;

  window_pre_damage (window);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
    {
      _gdk_x11_window_move_resize_child (window, x, y, width, height);
      _gdk_x11_window_update_size (GDK_WINDOW_IMPL_X11 (window->impl));
    }
  else
    {
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

      XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XID (window),
                         x * impl->window_scale, y * impl->window_scale,
                         width * impl->window_scale, height * impl->window_scale);

      if (impl->override_redirect)
        {
          window->x = x;
          window->y = y;

          window->width = width;
          window->height = height;

          _gdk_x11_window_update_size (GDK_WINDOW_IMPL_X11 (window->impl));
        }
      else
        {
          if (width != window->width || height != window->height)
            window->resize_count += 1;
        }
    }
}

static void
gdk_window_x11_move_resize (GdkWindow *window,
                            gboolean   with_move,
                            gint       x,
                            gint       y,
                            gint       width,
                            gint       height)
{
  if (with_move && (width < 0 && height < 0))
    window_x11_move (window, x, y);
  else
    {
      if (with_move)
        window_x11_move_resize (window, x, y, width, height);
      else
        window_x11_resize (window, width, height);
    }
}

static void
set_scale_recursive (GdkWindow *window, int scale)
{
  GdkWindow *child;
  GList *l;

  for (l = window->children; l; l = l->next)
    {
      child = l->data;

      if (child->impl != window->impl)
        _gdk_x11_window_set_window_scale (child, scale);
      else
        set_scale_recursive (child, scale);
    }
}

void
_gdk_x11_window_set_window_scale (GdkWindow *window,
				  int scale)
{
  GdkWindowImplX11 *impl;
  GdkToplevelX11 *toplevel;
  GdkWindowHints geom_mask;

  if (window->window_type == GDK_WINDOW_OFFSCREEN)
    return;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  impl->window_scale = scale;

  toplevel = _gdk_x11_window_get_toplevel (window);
  if (toplevel && window->window_type != GDK_WINDOW_FOREIGN)
    {
      /* These are affected by window scale: */
      geom_mask = toplevel->last_geometry_hints_mask &
        (GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_BASE_SIZE | GDK_HINT_RESIZE_INC);
      if (geom_mask)
        gdk_window_set_geometry_hints (window,
                                       &toplevel->last_geometry_hints,
                                       geom_mask);
    }

  if (window->window_type == GDK_WINDOW_FOREIGN)
    XMoveWindow (GDK_WINDOW_XDISPLAY (window),
                 GDK_WINDOW_XID (window),
                 (window->x + window->parent->abs_x) * impl->window_scale,
                 (window->y + window->parent->abs_y) * impl->window_scale);
  else if (WINDOW_IS_TOPLEVEL(window))
    XResizeWindow (GDK_WINDOW_XDISPLAY (window),
                   GDK_WINDOW_XID (window),
                   window->width * impl->window_scale,
                   window->height * impl->window_scale);
  else
    XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
                       GDK_WINDOW_XID (window),
                       (window->x + window->parent->abs_x) * impl->window_scale,
                       (window->y + window->parent->abs_y) * impl->window_scale,
                       window->width * impl->window_scale,
                       window->height * impl->window_scale);

  gdk_window_invalidate_rect (window, NULL, TRUE);

  set_scale_recursive (window, scale);
}

static gboolean
gdk_window_x11_reparent (GdkWindow *window,
                         GdkWindow *new_parent,
                         gint       x,
                         gint       y)
{
  GdkWindowImplX11 *impl;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  _gdk_x11_window_tmp_unset_bg (window, TRUE);
  _gdk_x11_window_tmp_unset_parent_bg (window);
  XReparentWindow (GDK_WINDOW_XDISPLAY (window),
		   GDK_WINDOW_XID (window),
		   GDK_WINDOW_XID (new_parent),
		   (new_parent->abs_x + x)  * impl->window_scale,
                   (new_parent->abs_y + y) * impl->window_scale);
  _gdk_x11_window_tmp_reset_parent_bg (window);
  _gdk_x11_window_tmp_reset_bg (window, TRUE);

  if (WINDOW_IS_TOPLEVEL (window))
    connect_frame_clock (window);
  else
    /* old frame clock was disposed, our signal handlers removed */
    impl->frame_clock_connected = FALSE;

  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    new_parent = gdk_screen_get_root_window (GDK_WINDOW_SCREEN (window));

  window->parent = new_parent;

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_FOREIGN:
      /* Reparenting to toplevel */
      
      if (!WINDOW_IS_TOPLEVEL (window) &&
	  GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
	{
	  /* This is also done in common code at a later stage, but we
	     need it in setup_toplevel, so do it here too */
	  if (window->toplevel_window_type != -1)
	    GDK_WINDOW_TYPE (window) = window->toplevel_window_type;
	  else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	    GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
	  
	  /* Wasn't a toplevel, set up */
	  setup_toplevel_window (window, new_parent);
	}

      break;
      
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_TEMP:
      if (WINDOW_IS_TOPLEVEL (window) &&
	  impl->toplevel)
	{
	  if (impl->toplevel->focus_window)
	    {
	      XDestroyWindow (GDK_WINDOW_XDISPLAY (window), impl->toplevel->focus_window);
              _gdk_x11_display_remove_window (GDK_WINDOW_DISPLAY (window), impl->toplevel->focus_window);
	    }
	  
	  gdk_toplevel_x11_free_contents (GDK_WINDOW_DISPLAY (window), 
					  impl->toplevel);
	  g_free (impl->toplevel);
	  impl->toplevel = NULL;
	}
    }

  return FALSE;
}

static void
gdk_window_x11_raise (GdkWindow *window)
{
  XRaiseWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

static void
gdk_window_x11_restack_under (GdkWindow *window,
			      GList *native_siblings /* in requested order, first is bottom-most */)
{
  Window *windows;
  int n_windows, i;
  GList *l;

  n_windows = g_list_length (native_siblings) + 1;
  windows = g_new (Window, n_windows);

  windows[0] = GDK_WINDOW_XID (window);
  /* Reverse order, as input order is bottom-most first */
  i = n_windows - 1;
  for (l = native_siblings; l != NULL; l = l->next)
    windows[i--] = GDK_WINDOW_XID (l->data);
 
  XRestackWindows (GDK_WINDOW_XDISPLAY (window), windows, n_windows);
  
  g_free (windows);
}

static void
gdk_window_x11_restack_toplevel (GdkWindow *window,
				 GdkWindow *sibling,
				 gboolean   above)
{
  XWindowChanges changes;

  changes.sibling = GDK_WINDOW_XID (sibling);
  changes.stack_mode = above ? Above : Below;
  XReconfigureWMWindow (GDK_WINDOW_XDISPLAY (window),
			GDK_WINDOW_XID (window),
			gdk_screen_get_number (GDK_WINDOW_SCREEN (window)),
			CWStackMode | CWSibling, &changes);
}

static void
gdk_window_x11_lower (GdkWindow *window)
{
  XLowerWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window));
}

/**
 * gdk_x11_window_move_to_current_desktop:
 * @window: (type GdkX11Window): a #GdkWindow
 * 
 * Moves the window to the correct workspace when running under a 
 * window manager that supports multiple workspaces, as described
 * in the [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 * Will not do anything if the window is already on all workspaces.
 * 
 * Since: 2.8
 */
void
gdk_x11_window_move_to_current_desktop (GdkWindow *window)
{
  GdkToplevelX11 *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (toplevel->on_all_desktops)
    return;
  
  move_to_current_desktop (window);
}

static void
move_to_current_desktop (GdkWindow *window)
{
  guint32 desktop;

  desktop = gdk_x11_screen_get_current_desktop (GDK_WINDOW_SCREEN (window));
  gdk_x11_window_move_to_desktop (window, desktop);
}

static guint32
get_netwm_cardinal_property (GdkWindow   *window,
                             const gchar *name)
{
  GdkScreen *screen = GDK_WINDOW_SCREEN (window);
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);
  GdkAtom atom;
  guint32 prop = 0;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;

  atom = gdk_atom_intern_static_string (name);

  if (!gdk_x11_screen_supports_net_wm_hint (screen, atom))
    return 0;

  XGetWindowProperty (x11_screen->xdisplay,
                      GDK_WINDOW_XID (window),
                      gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window), name),
                      0, G_MAXLONG,
                      False, XA_CARDINAL, &type, &format, &nitems,
                      &bytes_after, &data);
  if (type == XA_CARDINAL)
    {
      prop = *(gulong *)data;
      XFree (data);
    }

  return prop;
}

/**
 * gdk_x11_window_get_desktop:
 * @window: (type GdkX11Window): a #GdkWindow
 *
 * Gets the number of the workspace @window is on.
 *
 * Returns: the current workspace of @window
 *
 * Since: 3.10
 */
guint32
gdk_x11_window_get_desktop (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  return get_netwm_cardinal_property (window, "_NET_WM_DESKTOP");
}

/**
 * gdk_x11_window_move_to_desktop:
 * @window: (type GdkX11Window): a #GdkWindow
 * @desktop: the number of the workspace to move the window to
 *
 * Moves the window to the given workspace when running unde a
 * window manager that supports multiple workspaces, as described
 * in the [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec) specification.
 *
 * Since: 3.10
 */
void
gdk_x11_window_move_to_desktop (GdkWindow *window,
                                guint32    desktop)
{
  GdkAtom atom;
  XClientMessageEvent xclient;

  g_return_if_fail (GDK_IS_WINDOW (window));

  atom = gdk_atom_intern_static_string ("_NET_WM_DESKTOP");
  if (!gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window), atom))
    return;

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.serial = 0;
  xclient.send_event = True;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_atom_to_xatom_for_display (GDK_WINDOW_DISPLAY (window), atom);
  xclient.format = 32;

  xclient.data.l[0] = desktop;
  xclient.data.l[1] = 1; /* source indication */
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;

  XSendEvent (GDK_WINDOW_XDISPLAY (window),
              GDK_WINDOW_XROOTWIN (window),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}

static void
gdk_x11_window_focus (GdkWindow *window,
		      guint32    timestamp)
{
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  display = GDK_WINDOW_DISPLAY (window);

  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
					   gdk_atom_intern_static_string ("_NET_ACTIVE_WINDOW")))
    {
      XClientMessageEvent xclient;

      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_WINDOW_XID (window);
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display,
                                                                    "_NET_ACTIVE_WINDOW");
      xclient.format = 32;
      xclient.data.l[0] = 1; /* requestor type; we're an app */
      xclient.data.l[1] = timestamp;
      xclient.data.l[2] = None; /* currently active window */
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;
      
      XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XROOTWIN (window), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
  else
    {
      XRaiseWindow (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window));

      /* There is no way of knowing reliably whether we are viewable;
       * so trap errors asynchronously around the XSetInputFocus call
       */
      gdk_x11_display_error_trap_push (display);
      XSetInputFocus (GDK_DISPLAY_XDISPLAY (display),
                      GDK_WINDOW_XID (window),
                      RevertToParent,
                      timestamp);
      gdk_x11_display_error_trap_pop_ignored (display);
    }
}

static void
gdk_x11_window_set_type_hint (GdkWindow        *window,
			      GdkWindowTypeHint hint)
{
  GdkDisplay *display;
  Atom atom;
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  display = gdk_window_get_display (window);

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DIALOG");
      break;
    case GDK_WINDOW_TYPE_HINT_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLBAR");
      break;
    case GDK_WINDOW_TYPE_HINT_UTILITY:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_UTILITY");
      break;
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_SPLASH");
      break;
    case GDK_WINDOW_TYPE_HINT_DOCK:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DOCK");
      break;
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DESKTOP");
      break;
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case GDK_WINDOW_TYPE_HINT_COMBO:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_COMBO");
      break;
    case GDK_WINDOW_TYPE_HINT_DND:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_window_set_type_hint", hint);
      /* Fall thru */
    case GDK_WINDOW_TYPE_HINT_NORMAL:
      atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NORMAL");
      break;
    }

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&atom, 1);
}

static GdkWindowTypeHint
gdk_x11_window_get_type_hint (GdkWindow *window)
{
  GdkDisplay *display;
  GdkWindowTypeHint type;
  Atom type_return;
  gint format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;

  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;

  type = GDK_WINDOW_TYPE_HINT_NORMAL;

  display = gdk_window_get_display (window);

  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                          gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
                          0, G_MAXLONG, False, XA_ATOM, &type_return,
                          &format_return, &nitems_return, &bytes_after_return,
                          &data) == Success)
    {
      if ((type_return == XA_ATOM) && (format_return == 32) &&
          (data) && (nitems_return == 1))
        {
          Atom atom = *(Atom*)data;

          if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DIALOG"))
            type = GDK_WINDOW_TYPE_HINT_DIALOG;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_MENU"))
            type = GDK_WINDOW_TYPE_HINT_MENU;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLBAR"))
            type = GDK_WINDOW_TYPE_HINT_TOOLBAR;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_UTILITY"))
            type = GDK_WINDOW_TYPE_HINT_UTILITY;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_SPLASH"))
            type = GDK_WINDOW_TYPE_HINT_SPLASHSCREEN;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DOCK"))
            type = GDK_WINDOW_TYPE_HINT_DOCK;
          else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DESKTOP"))
            type = GDK_WINDOW_TYPE_HINT_DESKTOP;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"))
	    type = GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_POPUP_MENU"))
	    type = GDK_WINDOW_TYPE_HINT_POPUP_MENU;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_TOOLTIP"))
	    type = GDK_WINDOW_TYPE_HINT_TOOLTIP;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_NOTIFICATION"))
	    type = GDK_WINDOW_TYPE_HINT_NOTIFICATION;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_COMBO"))
	    type = GDK_WINDOW_TYPE_HINT_COMBO;
	  else if (atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DND"))
	    type = GDK_WINDOW_TYPE_HINT_DND;
        }

      if (type_return != None && data != NULL)
        XFree (data);
    }

  return type;
}

static void
gdk_wmspec_change_state (gboolean   add,
			 GdkWindow *window,
			 GdkAtom    state1,
			 GdkAtom    state2)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  XClientMessageEvent xclient;
  
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE");
  xclient.format = 32;
  xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xclient.data.l[1] = gdk_x11_atom_to_xatom_for_display (display, state1);
  xclient.data.l[2] = gdk_x11_atom_to_xatom_for_display (display, state2);
  xclient.data.l[3] = 1; /* source indication */
  xclient.data.l[4] = 0;
  
  XSendEvent (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XROOTWIN (window), False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      (XEvent *)&xclient);
}

static void
gdk_x11_window_set_modal_hint (GdkWindow *window,
			       gboolean   modal)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  window->modal_hint = modal;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (modal, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MODAL"), 
			     GDK_NONE);
}

static void
gdk_x11_window_set_skip_taskbar_hint (GdkWindow *window,
				      gboolean   skips_taskbar)
{
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  toplevel->skip_taskbar_hint = skips_taskbar;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (skips_taskbar, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_SKIP_TASKBAR"),
			     GDK_NONE);
}

static void
gdk_x11_window_set_skip_pager_hint (GdkWindow *window,
				    gboolean   skips_pager)
{
  GdkToplevelX11 *toplevel;
    
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  toplevel->skip_pager_hint = skips_pager;
  
  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (skips_pager, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_SKIP_PAGER"), 
			     GDK_NONE);
}

static void
gdk_x11_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  GdkToplevelX11 *toplevel;
    
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  toplevel->urgency_hint = urgent;
  
  update_wm_hints (window, FALSE);
}

static void
gdk_x11_window_set_geometry_hints (GdkWindow         *window,
				   const GdkGeometry *geometry,
				   GdkWindowHints     geom_mask)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  XSizeHints size_hints;
  GdkToplevelX11 *toplevel;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);
  if (toplevel)
    {
      if (geometry)
        toplevel->last_geometry_hints = *geometry;
      toplevel->last_geometry_hints_mask = geom_mask;
    }
  
  size_hints.flags = 0;
  
  if (geom_mask & GDK_HINT_POS)
    {
      size_hints.flags |= PPosition;
      /* We need to initialize the following obsolete fields because KWM 
       * apparently uses these fields if they are non-zero.
       * #@#!#!$!.
       */
      size_hints.x = 0;
      size_hints.y = 0;
    }

  if (geom_mask & GDK_HINT_USER_POS)
    {
      size_hints.flags |= USPosition;
    }

  if (geom_mask & GDK_HINT_USER_SIZE)
    {
      size_hints.flags |= USSize;
    }
  
  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = geometry->min_width * impl->window_scale;
      size_hints.min_height = geometry->min_height * impl->window_scale;
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = MAX (geometry->max_width, 1) * impl->window_scale;
      size_hints.max_height = MAX (geometry->max_height, 1) * impl->window_scale;
    }
  
  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      size_hints.flags |= PBaseSize;
      size_hints.base_width = geometry->base_width * impl->window_scale;
      size_hints.base_height = geometry->base_height * impl->window_scale;
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      size_hints.flags |= PResizeInc;
      size_hints.width_inc = geometry->width_inc * impl->window_scale;
      size_hints.height_inc = geometry->height_inc * impl->window_scale;
    }
  else if (impl->window_scale > 1)
    {
      size_hints.flags |= PResizeInc;
      size_hints.width_inc = impl->window_scale;
      size_hints.height_inc = impl->window_scale;
    }

  if (geom_mask & GDK_HINT_ASPECT)
    {
      size_hints.flags |= PAspect;
      if (geometry->min_aspect <= 1)
	{
	  size_hints.min_aspect.x = 65536 * geometry->min_aspect;
	  size_hints.min_aspect.y = 65536;
	}
      else
	{
	  size_hints.min_aspect.x = 65536;
	  size_hints.min_aspect.y = 65536 / geometry->min_aspect;;
	}
      if (geometry->max_aspect <= 1)
	{
	  size_hints.max_aspect.x = 65536 * geometry->max_aspect;
	  size_hints.max_aspect.y = 65536;
	}
      else
	{
	  size_hints.max_aspect.x = 65536;
	  size_hints.max_aspect.y = 65536 / geometry->max_aspect;;
	}
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      size_hints.flags |= PWinGravity;
      size_hints.win_gravity = geometry->win_gravity;
    }
  
  /* FIXME: Would it be better to delete this property if
   *        geom_mask == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     &size_hints);
}

static void
gdk_window_get_geometry_hints (GdkWindow      *window,
                               GdkGeometry    *geometry,
                               GdkWindowHints *geom_mask)
{
  GdkWindowImplX11 *impl;
  XSizeHints *size_hints;  
  glong junk_supplied_mask = 0;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (geometry != NULL);
  g_return_if_fail (geom_mask != NULL);

  *geom_mask = 0;
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  size_hints = XAllocSizeHints ();
  if (!size_hints)
    return;
  
  if (!XGetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
                          GDK_WINDOW_XID (window),
                          size_hints,
                          &junk_supplied_mask))
    size_hints->flags = 0;

  if (size_hints->flags & PMinSize)
    {
      *geom_mask |= GDK_HINT_MIN_SIZE;
      geometry->min_width = size_hints->min_width / impl->window_scale;
      geometry->min_height = size_hints->min_height / impl->window_scale;
    }

  if (size_hints->flags & PMaxSize)
    {
      *geom_mask |= GDK_HINT_MAX_SIZE;
      geometry->max_width = MAX (size_hints->max_width, 1) / impl->window_scale;
      geometry->max_height = MAX (size_hints->max_height, 1) / impl->window_scale;
    }

  if (size_hints->flags & PResizeInc)
    {
      *geom_mask |= GDK_HINT_RESIZE_INC;
      geometry->width_inc = size_hints->width_inc / impl->window_scale;
      geometry->height_inc = size_hints->height_inc / impl->window_scale;
    }

  if (size_hints->flags & PAspect)
    {
      *geom_mask |= GDK_HINT_ASPECT;

      geometry->min_aspect = (gdouble) size_hints->min_aspect.x / (gdouble) size_hints->min_aspect.y;
      geometry->max_aspect = (gdouble) size_hints->max_aspect.x / (gdouble) size_hints->max_aspect.y;
    }

  if (size_hints->flags & PWinGravity)
    {
      *geom_mask |= GDK_HINT_WIN_GRAVITY;
      geometry->win_gravity = size_hints->win_gravity;
    }

  XFree (size_hints);
}

static gboolean
utf8_is_latin1 (const gchar *str)
{
  const char *p = str;

  while (*p)
    {
      gunichar ch = g_utf8_get_char (p);

      if (ch > 0xff)
	return FALSE;
      
      p = g_utf8_next_char (p);
    }

  return TRUE;
}

/* Set the property to @utf8_str as STRING if the @utf8_str is fully
 * convertable to STRING, otherwise, set it as compound text
 */
static void
set_text_property (GdkDisplay  *display,
		   Window       xwindow,
		   Atom         property,
		   const gchar *utf8_str)
{
  gchar *prop_text = NULL;
  Atom prop_type;
  gint prop_length;
  gint prop_format;
  gboolean is_compound_text;
  
  if (utf8_is_latin1 (utf8_str))
    {
      prop_type = XA_STRING;
      prop_text = _gdk_x11_display_utf8_to_string_target (display, utf8_str);
      prop_length = prop_text ? strlen (prop_text) : 0;
      prop_format = 8;
      is_compound_text = FALSE;
    }
  else
    {
      GdkAtom gdk_type;

      gdk_x11_display_utf8_to_compound_text (display,
                                             utf8_str, &gdk_type, &prop_format,
                                             (guchar **)&prop_text, &prop_length);
      prop_type = gdk_x11_atom_to_xatom_for_display (display, gdk_type);
      is_compound_text = TRUE;
    }

  if (prop_text)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		       xwindow,
		       property,
		       prop_type, prop_format,
		       PropModeReplace, (guchar *)prop_text,
		       prop_length);

      if (is_compound_text)
	gdk_x11_free_compound_text ((guchar *)prop_text);
      else
	g_free (prop_text);
    }
}

/* Set WM_NAME and _NET_WM_NAME
 */
static void
set_wm_name (GdkDisplay  *display,
	     Window       xwindow,
	     const gchar *name)
{
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
		   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_NAME"),
		   gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		   PropModeReplace, (guchar *)name, strlen (name));
  
  set_text_property (display, xwindow,
		     gdk_x11_get_xatom_by_name_for_display (display, "WM_NAME"),
		     name);
}

static void
gdk_x11_window_set_title (GdkWindow   *window,
			  const gchar *title)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
  
  display = gdk_window_get_display (window);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  xwindow = GDK_WINDOW_XID (window);

  set_wm_name (display, xwindow, title);
  
  if (!gdk_window_icon_name_set (window))
    {
      XChangeProperty (xdisplay, xwindow,
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON_NAME"),
		       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
		       PropModeReplace, (guchar *)title, strlen (title));
      
      set_text_property (display, xwindow,
			 gdk_x11_get_xatom_by_name_for_display (display, "WM_ICON_NAME"),
			 title);
    }
}

static void
gdk_x11_window_set_role (GdkWindow   *window,
			 const gchar *role)
{
  GdkDisplay *display;

  display = gdk_window_get_display (window);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (role)
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                     gdk_x11_get_xatom_by_name_for_display (display, "WM_WINDOW_ROLE"),
                     XA_STRING, 8, PropModeReplace, (guchar *)role, strlen (role));
  else
    XDeleteProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                     gdk_x11_get_xatom_by_name_for_display (display, "WM_WINDOW_ROLE"));
}

static void
gdk_x11_window_set_startup_id (GdkWindow   *window,
			       const gchar *startup_id)
{
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  display = gdk_window_get_display (window);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (startup_id)
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                     gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"), 
                     gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
                     PropModeReplace, (unsigned char *)startup_id, strlen (startup_id));
  else
    XDeleteProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                     gdk_x11_get_xatom_by_name_for_display (display, "_NET_STARTUP_ID"));
}

static void
gdk_x11_window_set_transient_for (GdkWindow *window,
				  GdkWindow *parent)
{
  if (!GDK_WINDOW_DESTROYED (window) && !GDK_WINDOW_DESTROYED (parent) &&
      WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    XSetTransientForHint (GDK_WINDOW_XDISPLAY (window), 
			  GDK_WINDOW_XID (window),
			  GDK_WINDOW_XID (parent));
}

static gboolean
gdk_window_x11_set_back_color (GdkWindow *window,
                               double     red,
                               double     green,
                               double     blue,
                               double     alpha)
{
  GdkVisual *visual = gdk_window_get_visual (window);

  /* I suppose we could handle these, but that'd require fiddling with 
   * xrender formats... */
  if (alpha != 1.0)
    return FALSE;

  switch (visual->type)
    {
    case GDK_VISUAL_DIRECT_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
	{
	  /* If bits not used for color are used for something other than padding,
	   * it's likely alpha, so we set them to 1s.
	   */
	  guint padding, pixel;

	  /* Shifting by >= width-of-type isn't defined in C */
	  if (visual->depth >= 32)
	    padding = 0;
	  else
	    padding = ((~(guint32)0)) << visual->depth;
	  
	  pixel = ~ (visual->red_mask | visual->green_mask | visual->blue_mask | padding);
	  
	  pixel += (((int) (red   * ((1 << visual->red_prec  ) - 1))) << visual->red_shift  ) +
		   (((int) (green * ((1 << visual->green_prec) - 1))) << visual->green_shift) +
		   (((int) (blue  * ((1 << visual->blue_prec ) - 1))) << visual->blue_shift );

          XSetWindowBackground (GDK_WINDOW_XDISPLAY (window),
                                GDK_WINDOW_XID (window), pixel);
	}
      return TRUE;

    /* These require fiddling with the colormap, and as they're essentially unused
     * we're just gonna skip them for now.
     */
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
    default:
      break;
    }

  return FALSE;
}

static gboolean
matrix_is_identity (cairo_matrix_t *matrix)
{
  return matrix->xx == 1.0 && matrix->yy == 1.0 &&
    matrix->yx == 0.0 && matrix->xy == 0.0 &&
    matrix->x0 == 0.0 && matrix->y0 == 0.0;
}

static void
gdk_window_x11_set_background (GdkWindow      *window,
                               cairo_pattern_t *pattern)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  double r, g, b, a;
  cairo_surface_t *surface;
  cairo_matrix_t matrix;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (pattern == NULL)
    {
      GdkWindow *parent;

      /* X throws BadMatch if the parent has a different visual when
       * using ParentRelative */
      parent = gdk_window_get_parent (window);
      if (parent && gdk_window_get_visual (parent) == gdk_window_get_visual (window))
        XSetWindowBackgroundPixmap (GDK_WINDOW_XDISPLAY (window),
                                    GDK_WINDOW_XID (window), ParentRelative);
      else
        XSetWindowBackgroundPixmap (GDK_WINDOW_XDISPLAY (window),
                                    GDK_WINDOW_XID (window), None);
      return;
    }

  switch (cairo_pattern_get_type (pattern))
    {
    case CAIRO_PATTERN_TYPE_SOLID:
      cairo_pattern_get_rgba (pattern, &r, &g, &b, &a);
      if (gdk_window_x11_set_back_color (window, r, g, b, a))
        return;
      break;
    case CAIRO_PATTERN_TYPE_SURFACE:
      cairo_pattern_get_matrix (pattern, &matrix);
      if (cairo_pattern_get_surface (pattern, &surface) == CAIRO_STATUS_SUCCESS &&
          matrix_is_identity (&matrix) &&
          cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_XLIB &&
          cairo_xlib_surface_get_visual (surface) == GDK_VISUAL_XVISUAL (gdk_window_get_visual ((window))) &&
          cairo_xlib_surface_get_display (surface) == GDK_WINDOW_XDISPLAY (window))
        {
          double x, y, sx, sy;

          cairo_surface_get_device_offset (surface, &x, &y);
          sx = sy = 1.;
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
          cairo_surface_get_device_scale (surface, &sx, &sy);
#endif
          /* XXX: This still bombs for non-pixmaps, but there's no way to
           * detect we're not a pixmap in Cairo... */
          if (x == 0.0 && y == 0.0 &&
              sx == impl->window_scale && sy == impl->window_scale)
            {
              XSetWindowBackgroundPixmap (GDK_WINDOW_XDISPLAY (window),
                                          GDK_WINDOW_XID (window),
                                          cairo_xlib_surface_get_drawable (surface));
              return;
            }
        }
      /* fall through */
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    default:
      /* fallback: just use black */
      break;
    }

  XSetWindowBackgroundPixmap (GDK_WINDOW_XDISPLAY (window),
                              GDK_WINDOW_XID (window), None);
}

static void
gdk_window_x11_set_device_cursor (GdkWindow *window,
                                  GdkDevice *device,
                                  GdkCursor *cursor)
{
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (!cursor)
    g_hash_table_remove (impl->device_cursor, device);
  else
    {
      _gdk_x11_cursor_update_theme (cursor);
      g_hash_table_replace (impl->device_cursor,
                            device, g_object_ref (cursor));
    }

  if (!GDK_WINDOW_DESTROYED (window))
    GDK_DEVICE_GET_CLASS (device)->set_window_cursor (device, window, cursor);
}

GdkCursor *
_gdk_x11_window_get_cursor (GdkWindow *window)
{
  GdkWindowImplX11 *impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
    
  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  return impl->cursor;
}

static void
gdk_window_x11_get_geometry (GdkWindow *window,
                             gint      *x,
                             gint      *y,
                             gint      *width,
                             gint      *height)
{
  GdkWindowImplX11 *impl;
  Window root;
  gint tx;
  gint ty;
  guint twidth;
  guint theight;
  guint tborder_width;
  guint tdepth;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      impl = GDK_WINDOW_IMPL_X11 (window->impl);

      XGetGeometry (GDK_WINDOW_XDISPLAY (window),
		    GDK_WINDOW_XID (window),
		    &root, &tx, &ty, &twidth, &theight, &tborder_width, &tdepth);
      
      if (x)
	*x = tx / impl->window_scale;
      if (y)
	*y = ty / impl->window_scale;
      if (width)
	*width = twidth / impl->window_scale;
      if (height)
	*height = theight / impl->window_scale;
    }
}

static void
gdk_window_x11_get_root_coords (GdkWindow *window,
				gint       x,
				gint       y,
				gint      *root_x,
				gint      *root_y)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  Window child;
  gint tx;
  gint ty;
  
  XTranslateCoordinates (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XID (window),
                         GDK_WINDOW_XROOTWIN (window),
                         x * impl->window_scale, y * impl->window_scale, &tx, &ty,
                         &child);

  if (root_x)
    *root_x = tx / impl->window_scale;
  if (root_y)
    *root_y = ty / impl->window_scale;
}

static void
gdk_x11_window_get_frame_extents (GdkWindow    *window,
                                  GdkRectangle *rect)
{
  GdkDisplay *display;
  GdkWindowImplX11 *impl;
  Window xwindow;
  Window xparent;
  Window root;
  Window child;
  Window *children;
  guchar *data;
  Window *vroots;
  Atom type_return;
  guint nchildren;
  guint nvroots;
  gulong nitems_return;
  gulong bytes_after_return;
  gint format_return;
  gint i;
  guint ww, wh, wb, wd;
  gint wx, wy;
  gboolean got_frame_extents = FALSE;

  g_return_if_fail (rect != NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;

  while (window->parent && (window->parent)->parent)
    window = window->parent;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  /* Refine our fallback answer a bit using local information */
  rect->x = window->x * impl->window_scale;
  rect->y = window->y * impl->window_scale;
  rect->width = window->width * impl->window_scale;
  rect->height = window->height * impl->window_scale;

  if (GDK_WINDOW_DESTROYED (window) || impl->override_redirect)
    return;

  nvroots = 0;
  vroots = NULL;

  display = gdk_window_get_display (window);

  gdk_x11_display_error_trap_push (display);

  xwindow = GDK_WINDOW_XID (window);

  /* first try: use _NET_FRAME_EXTENTS */
  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
                                           gdk_atom_intern_static_string ("_NET_FRAME_EXTENTS")) &&
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
                          gdk_x11_get_xatom_by_name_for_display (display,
                                                                  "_NET_FRAME_EXTENTS"),
                          0, G_MAXLONG, False, XA_CARDINAL, &type_return,
                          &format_return, &nitems_return, &bytes_after_return,
                          &data)
      == Success)
    {
      if ((type_return == XA_CARDINAL) && (format_return == 32) &&
	  (nitems_return == 4) && (data))
        {
	  gulong *ldata = (gulong *) data;
	  got_frame_extents = TRUE;

	  /* try to get the real client window geometry */
	  if (XGetGeometry (GDK_DISPLAY_XDISPLAY (display), xwindow,
			    &root, &wx, &wy, &ww, &wh, &wb, &wd) &&
              XTranslateCoordinates (GDK_DISPLAY_XDISPLAY (display),
	  			     xwindow, root, 0, 0, &wx, &wy, &child))
            {
	      rect->x = wx;
	      rect->y = wy;
	      rect->width = ww;
	      rect->height = wh;
	    }

	  /* _NET_FRAME_EXTENTS format is left, right, top, bottom */
	  rect->x -= ldata[0];
	  rect->y -= ldata[2];
	  rect->width += ldata[0] + ldata[1];
	  rect->height += ldata[2] + ldata[3];
	}

      if (data)
	XFree (data);
    }

  if (got_frame_extents)
    goto out;

  /* no frame extents property available, which means we either have a WM that
     is not EWMH compliant or is broken - try fallback and walk up the window
     tree to get our window's parent which hopefully is the window frame */

  /* use NETWM_VIRTUAL_ROOTS if available */
  root = GDK_WINDOW_XROOTWIN (window);

  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
                                           gdk_atom_intern_static_string ("_NET_VIRTUAL_ROOTS")) &&
      XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), root,
			  gdk_x11_get_xatom_by_name_for_display (display, 
								 "_NET_VIRTUAL_ROOTS"),
			  0, G_MAXLONG, False, XA_WINDOW, &type_return,
			  &format_return, &nitems_return, &bytes_after_return,
			  &data)
      == Success)
    {
      if ((type_return == XA_WINDOW) && (format_return == 32) && (data))
	{
	  nvroots = nitems_return;
	  vroots = (Window *)data;
	}
    }

  xparent = GDK_WINDOW_XID (window);

  do
    {
      xwindow = xparent;

      if (!XQueryTree (GDK_DISPLAY_XDISPLAY (display), xwindow,
		       &root, &xparent,
		       &children, &nchildren))
	goto out;
      
      if (children)
	XFree (children);

      /* check virtual roots */
      for (i = 0; i < nvroots; i++)
	{
	  if (xparent == vroots[i])
	    {
	      root = xparent;
	      break;
           }
	}
    }
  while (xparent != root);

  if (XGetGeometry (GDK_DISPLAY_XDISPLAY (display), xwindow,
		    &root, &wx, &wy, &ww, &wh, &wb, &wd))
    {
      rect->x = wx;
      rect->y = wy;
      rect->width = ww;
      rect->height = wh;
    }

 out:
  if (vroots)
    XFree (vroots);

  rect->x /= impl->window_scale;
  rect->y /= impl->window_scale;
  rect->width /= impl->window_scale;
  rect->height /= impl->window_scale;
  gdk_x11_display_error_trap_pop_ignored (display);
}

static gboolean
gdk_window_x11_get_device_state (GdkWindow       *window,
                                 GdkDevice       *device,
                                 gdouble         *x,
                                 gdouble         *y,
                                 GdkModifierType *mask)
{
  GdkWindow *child;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  /*HIDPI: handle coords here?*/
  GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
                                              NULL, &child,
                                              NULL, NULL,
                                              x, y, mask);
  return child != NULL;
}

static GdkEventMask
gdk_window_x11_get_events (GdkWindow *window)
{
  XWindowAttributes attrs;
  GdkEventMask event_mask;
  GdkEventMask filtered;

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    {
      XGetWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			    GDK_WINDOW_XID (window),
			    &attrs);
      event_mask = x_event_mask_to_gdk_event_mask (attrs.your_event_mask);
      /* if property change was filtered out before, keep it filtered out */
      filtered = GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK;
      window->event_mask = event_mask & ((window->event_mask & filtered) | ~filtered);

      return event_mask;
    }
}
static void
gdk_window_x11_set_events (GdkWindow    *window,
                           GdkEventMask  event_mask)
{
  long xevent_mask = 0;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkX11Display *display_x11;

      if (GDK_WINDOW_XID (window) != GDK_WINDOW_XROOTWIN (window))
        xevent_mask = StructureNotifyMask | PropertyChangeMask;

      display_x11 = GDK_X11_DISPLAY (gdk_window_get_display (window));
      gdk_x11_event_source_select_events ((GdkEventSource *) display_x11->event_source,
                                          GDK_WINDOW_XID (window), event_mask,
                                          xevent_mask);
    }
}

static inline void
do_shape_combine_region (GdkWindow       *window,
			 const cairo_region_t *shape_region,
			 gint             offset_x,
			 gint             offset_y,
			 gint             shape)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (shape_region == NULL)
    {
      /* Use NULL mask to unset the shape */
      if (shape == ShapeBounding
	  ? gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window))
	  : gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
	{
	  if (shape == ShapeBounding)
	    {
	      _gdk_x11_window_tmp_unset_parent_bg (window);
	      _gdk_x11_window_tmp_unset_bg (window, TRUE);
	    }
	  XShapeCombineMask (GDK_WINDOW_XDISPLAY (window),
			     GDK_WINDOW_XID (window),
			     shape,
			     0, 0,
			     None,
			     ShapeSet);
 	  if (shape == ShapeBounding)
	    {
	      _gdk_x11_window_tmp_reset_parent_bg (window);
	      _gdk_x11_window_tmp_reset_bg (window, TRUE);
	    }
	}
      return;
    }
  
  if (shape == ShapeBounding
      ? gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window))
      : gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
    {
      gint n_rects = 0;
      XRectangle *xrects = NULL;

      _gdk_x11_region_get_xrectangles (shape_region,
                                       0, 0, impl->window_scale,
                                       &xrects, &n_rects);
      
      if (shape == ShapeBounding)
	{
	  _gdk_x11_window_tmp_unset_parent_bg (window);
	  _gdk_x11_window_tmp_unset_bg (window, TRUE);
	}
      XShapeCombineRectangles (GDK_WINDOW_XDISPLAY (window),
                               GDK_WINDOW_XID (window),
                               shape,
                               offset_x * impl->window_scale,
                               offset_y * impl->window_scale,
                               xrects, n_rects,
                               ShapeSet,
                               YXBanded);

      if (shape == ShapeBounding)
	{
	  _gdk_x11_window_tmp_reset_parent_bg (window);
	  _gdk_x11_window_tmp_reset_bg (window, TRUE);
	}
      
      g_free (xrects);
    }
}

static void
gdk_window_x11_shape_combine_region (GdkWindow       *window,
                                     const cairo_region_t *shape_region,
                                     gint             offset_x,
                                     gint             offset_y)
{
  do_shape_combine_region (window, shape_region, offset_x, offset_y, ShapeBounding);
}

static void 
gdk_window_x11_input_shape_combine_region (GdkWindow       *window,
					   const cairo_region_t *shape_region,
					   gint             offset_x,
					   gint             offset_y)
{
#ifdef ShapeInput
  do_shape_combine_region (window, shape_region, offset_x, offset_y, ShapeInput);
#endif
}


static void
gdk_x11_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  XSetWindowAttributes attr;
  
  if (!GDK_WINDOW_DESTROYED (window) &&
      WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    {
      GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

      attr.override_redirect = (override_redirect? True : False);
      XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			       GDK_WINDOW_XID (window),
			       CWOverrideRedirect,
			       &attr);

      impl->override_redirect = attr.override_redirect;
    }
}

static void
gdk_x11_window_set_accept_focus (GdkWindow *window,
				 gboolean accept_focus)
{
  accept_focus = accept_focus != FALSE;

  if (window->accept_focus != accept_focus)
    {
      window->accept_focus = accept_focus;

      if (!GDK_WINDOW_DESTROYED (window) &&
	  WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
	update_wm_hints (window, FALSE);
    }
}

static void
gdk_x11_window_set_focus_on_map (GdkWindow *window,
				 gboolean focus_on_map)
{
  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    {
      window->focus_on_map = focus_on_map;
      
      if ((!GDK_WINDOW_DESTROYED (window)) &&
	  (!window->focus_on_map) &&
	  WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
	gdk_x11_window_set_user_time (window, 0);
    }
}

/**
 * gdk_x11_window_set_user_time:
 * @window: (type GdkX11Window): A toplevel #GdkWindow
 * @timestamp: An XServer timestamp to which the property should be set
 *
 * The application can use this call to update the _NET_WM_USER_TIME
 * property on a toplevel window.  This property stores an Xserver
 * time which represents the time of the last user input event
 * received for this window.  This property may be used by the window
 * manager to alter the focus, stacking, and/or placement behavior of
 * windows when they are mapped depending on whether the new window
 * was created by a user action or is a "pop-up" window activated by a
 * timer or some other event.
 *
 * Note that this property is automatically updated by GDK, so this
 * function should only be used by applications which handle input
 * events bypassing GDK.
 *
 * Since: 2.6
 **/
void
gdk_x11_window_set_user_time (GdkWindow *window,
                              guint32    timestamp)
{
  GdkDisplay *display;
  GdkX11Display *display_x11;
  GdkToplevelX11 *toplevel;
  glong timestamp_long = (glong)timestamp;
  Window xid;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  display = gdk_window_get_display (window);
  display_x11 = GDK_X11_DISPLAY (display);
  toplevel = _gdk_x11_window_get_toplevel (window);

  if (!toplevel)
    {
      g_warning ("gdk_window_set_user_time called on non-toplevel\n");
      return;
    }

  if (toplevel->focus_window != None &&
      gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
                                           gdk_atom_intern_static_string ("_NET_WM_USER_TIME_WINDOW")))
    xid = toplevel->focus_window;
  else
    xid = GDK_WINDOW_XID (window);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xid,
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_USER_TIME"),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *)&timestamp_long, 1);

  if (timestamp_long != GDK_CURRENT_TIME &&
      (display_x11->user_time == GDK_CURRENT_TIME ||
       XSERVER_TIME_IS_LATER (timestamp_long, display_x11->user_time)))
    display_x11->user_time = timestamp_long;

  if (toplevel)
    toplevel->user_time = timestamp_long;
}

/**
 * gdk_x11_window_set_utf8_property:
 * @window: (type GdkX11Window): a #GdkWindow
 * @name: Property name, will be interned as an X atom
 * @value: (allow-none): Property value, or %NULL to delete
 *
 * This function modifies or removes an arbitrary X11 window
 * property of type UTF8_STRING.  If the given @window is
 * not a toplevel window, it is ignored.
 *
 * Since: 3.4
 */
void
gdk_x11_window_set_utf8_property  (GdkWindow *window,
				   const gchar *name,
				   const gchar *value)
{
  GdkDisplay *display;

  if (!WINDOW_IS_TOPLEVEL (window))
    return;

  display = gdk_window_get_display (window);

  if (value != NULL)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, name),
                       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
                       PropModeReplace, (guchar *)value, strlen (value));
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, name));
    }
}

/**
 * gdk_x11_window_set_hide_titlebar_when_maximized:
 * @window: (type GdkX11Window): a #GdkWindow
 * @hide_titlebar_when_maximized: whether to hide the titlebar when
 *                                maximized
 *
 * Set a hint for the window manager, requesting that the titlebar
 * should be hidden when the window is maximized.
 *
 * Note that this property is automatically updated by GTK+, so this
 * function should only be used by applications which do not use GTK+
 * to create toplevel windows.
 *
 * Since: 3.4
 */
void
gdk_x11_window_set_hide_titlebar_when_maximized (GdkWindow *window,
                                                 gboolean   hide_titlebar_when_maximized)
{
  GdkDisplay *display;

  if (!WINDOW_IS_TOPLEVEL (window))
    return;

  display = gdk_window_get_display (window);

  if (hide_titlebar_when_maximized)
    {
      guint32 hide = 1;
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, "_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED"),
                       XA_CARDINAL, 32,
                       PropModeReplace, (guchar *)&hide, 1);
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, "_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED"));
    }
}

static void
gdk_x11_window_set_shadow_width (GdkWindow *window,
                                 int        left,
                                 int        right,
                                 int        top,
                                 int        bottom)
{
  Atom frame_extents;
  gulong data[4] = { left, right, top, bottom };

  frame_extents = gdk_x11_get_xatom_by_name_for_display (gdk_window_get_display (window),
                                                         "_GTK_FRAME_EXTENTS");
  XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                   GDK_WINDOW_XID (window),
                   frame_extents, XA_CARDINAL,
                   32, PropModeReplace,
                   (guchar *) &data, 4);
}

/**
 * gdk_x11_window_set_frame_extents:
 * @window: (type GdkX11Window): a #GdkWindow
 * @left: The left extent
 * @right: The right extent
 * @top: The top extent
 * @bottom: The bottom extent
 *
 * This is the same as gdk_window_set_shadow_width() but it only works
 * on GdkX11Window.
 *
 * Since: 3.10
 *
 * Deprecated: 3.12: Use gdk_window_set_shadow_width() instead.
 */
void
gdk_x11_window_set_frame_extents (GdkWindow *window,
                                  int        left,
                                  int        right,
                                  int        top,
                                  int        bottom)
{
  gdk_x11_window_set_shadow_width (window, left, right, top, bottom);
}

/**
 * gdk_x11_window_set_theme_variant:
 * @window: (type GdkX11Window): a #GdkWindow
 * @variant: the theme variant to export
 *
 * GTK+ applications can request a dark theme variant. In order to
 * make other applications - namely window managers using GTK+ for
 * themeing - aware of this choice, GTK+ uses this function to
 * export the requested theme variant as _GTK_THEME_VARIANT property
 * on toplevel windows.
 *
 * Note that this property is automatically updated by GTK+, so this
 * function should only be used by applications which do not use GTK+
 * to create toplevel windows.
 *
 * Since: 3.2
 */
void
gdk_x11_window_set_theme_variant (GdkWindow *window,
                                  char      *variant)
{
  gdk_x11_window_set_utf8_property (window, "_GTK_THEME_VARIANT", variant);
}

#define GDK_SELECTION_MAX_SIZE(display)                                 \
  MIN(262144,                                                           \
      XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) == 0     \
       ? XMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100         \
       : XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100)

static void
gdk_window_update_icon (GdkWindow *window,
                        GList     *icon_list)
{
  GdkToplevelX11 *toplevel;
  GdkPixbuf *best_icon;
  GList *tmp_list;
  int best_size;
  
  toplevel = _gdk_x11_window_get_toplevel (window);

  if (toplevel->icon_pixmap != NULL)
    {
      cairo_surface_destroy (toplevel->icon_pixmap);
      toplevel->icon_pixmap = NULL;
    }
  
  if (toplevel->icon_mask != NULL)
    {
      cairo_surface_destroy (toplevel->icon_mask);
      toplevel->icon_mask = NULL;
    }
  
#define IDEAL_SIZE 48
  
  best_size = G_MAXINT;
  best_icon = NULL;
  for (tmp_list = icon_list; tmp_list; tmp_list = tmp_list->next)
    {
      GdkPixbuf *pixbuf = tmp_list->data;
      int this;
  
      /* average width and height - if someone passes in a rectangular
       * icon they deserve what they get.
       */
      this = gdk_pixbuf_get_width (pixbuf) + gdk_pixbuf_get_height (pixbuf);
      this /= 2;
  
      if (best_icon == NULL)
        {
          best_icon = pixbuf;
          best_size = this;
        }
      else
        {
          /* icon is better if it's 32 pixels or larger, and closer to
           * the ideal size than the current best.
           */
          if (this >= 32 &&
              (ABS (best_size - IDEAL_SIZE) <
               ABS (this - IDEAL_SIZE)))
            {
              best_icon = pixbuf;
              best_size = this;
            }
        }
    }

  if (best_icon)
    {
      int width = gdk_pixbuf_get_width (best_icon);
      int height = gdk_pixbuf_get_height (best_icon);
      cairo_t *cr;

      toplevel->icon_pixmap = gdk_x11_window_create_pixmap_surface (window,
                                                                    width,
                                                                    height);

      cr = cairo_create (toplevel->icon_pixmap);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      gdk_cairo_set_source_pixbuf (cr, best_icon, 0, 0);
      if (gdk_pixbuf_get_has_alpha (best_icon))
        {
          /* Saturate the image, so it has bilevel alpha */
          cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
          cairo_paint (cr);
          cairo_set_operator (cr, CAIRO_OPERATOR_SATURATE);
          cairo_paint (cr);
          cairo_pop_group_to_source (cr);
        }
      cairo_paint (cr);
      cairo_destroy (cr);

      if (gdk_pixbuf_get_has_alpha (best_icon))
        {
          toplevel->icon_mask = _gdk_x11_window_create_bitmap_surface (window,
                                                                       width,
                                                                       height);

          cr = cairo_create (toplevel->icon_mask);
          gdk_cairo_set_source_pixbuf (cr, best_icon, 0, 0);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_paint (cr);
          cairo_destroy (cr);
        }
    }

  update_wm_hints (window, FALSE);
}

static void
gdk_x11_window_set_icon_list (GdkWindow *window,
			      GList     *pixbufs)
{
  gulong *data;
  guchar *pixels;
  gulong *p;
  gint size;
  GList *l;
  GdkPixbuf *pixbuf;
  gint width, height, stride;
  gint x, y;
  gint n_channels;
  GdkDisplay *display;
  gint n;
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  display = gdk_window_get_display (window);
  
  l = pixbufs;
  size = 0;
  n = 0;
  while (l)
    {
      pixbuf = l->data;
      g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      
      /* silently ignore overlarge icons */
      if (size + 2 + width * height > GDK_SELECTION_MAX_SIZE(display))
	{
	  g_warning ("gdk_window_set_icon_list: icons too large");
	  break;
	}
     
      n++;
      size += 2 + width * height;
      
      l = g_list_next (l);
    }

  data = g_malloc (size * sizeof (gulong));

  l = pixbufs;
  p = data;
  while (l && n > 0)
    {
      pixbuf = l->data;
      
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      stride = gdk_pixbuf_get_rowstride (pixbuf);
      n_channels = gdk_pixbuf_get_n_channels (pixbuf);
      
      *p++ = width;
      *p++ = height;

      pixels = gdk_pixbuf_get_pixels (pixbuf);

      for (y = 0; y < height; y++)
	{
	  for (x = 0; x < width; x++)
	    {
	      guchar r, g, b, a;
	      
	      r = pixels[y*stride + x*n_channels + 0];
	      g = pixels[y*stride + x*n_channels + 1];
	      b = pixels[y*stride + x*n_channels + 2];
	      if (n_channels >= 4)
		a = pixels[y*stride + x*n_channels + 3];
	      else
		a = 255;
	      
	      *p++ = a << 24 | r << 16 | g << 8 | b ;
	    }
	}

      l = g_list_next (l);
      n--;
    }

  if (size > 0)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON"),
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (guchar*) data, size);
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
		       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON"));
    }
  
  g_free (data);

  gdk_window_update_icon (window, pixbufs);
}

static gboolean
gdk_window_icon_name_set (GdkWindow *window)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (window),
					       g_quark_from_static_string ("gdk-icon-name-set")));
}

static void
gdk_x11_window_set_icon_name (GdkWindow   *window,
			      const gchar *name)
{
  GdkDisplay *display;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  display = gdk_window_get_display (window);

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
                      GUINT_TO_POINTER (name != NULL));

  if (name != NULL)
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON_NAME"),
                       gdk_x11_get_xatom_by_name_for_display (display, "UTF8_STRING"), 8,
                       PropModeReplace, (guchar *)name, strlen (name));

      set_text_property (display, GDK_WINDOW_XID (window),
                         gdk_x11_get_xatom_by_name_for_display (display, "WM_ICON_NAME"),
                         name);
    }
  else
    {
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_ICON_NAME"));
      XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
                       GDK_WINDOW_XID (window),
                       gdk_x11_get_xatom_by_name_for_display (display, "WM_ICON_NAME"));
    }
}

static void
gdk_x11_window_iconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      XIconifyWindow (GDK_WINDOW_XDISPLAY (window),
		      GDK_WINDOW_XID (window),
		      gdk_screen_get_number (GDK_WINDOW_SCREEN (window)));
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_ICONIFIED);
      gdk_wmspec_change_state (TRUE, window,
                               gdk_atom_intern_static_string ("_NET_WM_STATE_HIDDEN"),
                               GDK_NONE);
    }
}

static void
gdk_x11_window_deiconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      gdk_window_show (window);
      gdk_wmspec_change_state (FALSE, window,
                               gdk_atom_intern_static_string ("_NET_WM_STATE_HIDDEN"),
                               GDK_NONE);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_ICONIFIED,
                                   0);
      gdk_wmspec_change_state (FALSE, window,
                               gdk_atom_intern_static_string ("_NET_WM_STATE_HIDDEN"),
                               GDK_NONE);
    }
}

static void
gdk_x11_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      /* "stick" means stick to all desktops _and_ do not scroll with the
       * viewport. i.e. glue to the monitor glass in all cases.
       */
      
      XClientMessageEvent xclient;

      /* Request stick during viewport scroll */
      gdk_wmspec_change_state (TRUE, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_STICKY"),
			       GDK_NONE);

      /* Request desktop 0xFFFFFFFF */
      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_WINDOW_XID (window);
      xclient.display = GDK_WINDOW_XDISPLAY (window);
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window), 
									"_NET_WM_DESKTOP");
      xclient.format = 32;

      xclient.data.l[0] = 0xFFFFFFFF;
      xclient.data.l[1] = 0;
      xclient.data.l[2] = 0;
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;

      XSendEvent (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XROOTWIN (window), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_STICKY);
    }
}

static void
gdk_x11_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      /* Request unstick from viewport */
      gdk_wmspec_change_state (FALSE, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_STICKY"),
			       GDK_NONE);

      move_to_current_desktop (window);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_STICKY,
                                   0);

    }
}

static void
gdk_x11_window_maximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (TRUE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_VERT"),
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_HORZ"));
  else
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_MAXIMIZED);
}

static void
gdk_x11_window_unmaximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (FALSE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_VERT"),
			     gdk_atom_intern_static_string ("_NET_WM_STATE_MAXIMIZED_HORZ"));
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

static void
gdk_x11_window_apply_fullscreen_mode (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  /* _NET_WM_FULLSCREEN_MONITORS gives an indication to the window manager as
   * to which monitors so span across when the window is fullscreen, but it's
   * not a state in itself so this would have no effect if the window is not
   * mapped.
   */

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      XClientMessageEvent xclient;
      gint                gdk_monitors[4];
      gint                i;

      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.window = GDK_WINDOW_XID (window);
      xclient.display = GDK_WINDOW_XDISPLAY (window);
      xclient.format = 32;

      switch (window->fullscreen_mode)
	{
	case GDK_FULLSCREEN_ON_CURRENT_MONITOR:

	  /* FIXME: This is not part of the EWMH spec!
	   *
	   * There is no documented mechanism to remove the property
	   * _NET_WM_FULLSCREEN_MONITORS once set, so we use use a set of
	   * invalid, largest possible value.
	   *
	   * When given values larger than actual possible monitor values, most
	   * window managers who support the _NET_WM_FULLSCREEN_MONITORS spec
	   * will simply unset _NET_WM_FULLSCREEN_MONITORS and revert to their
	   * default behavior.
	   *
	   * Successfully tested on mutter/metacity, kwin, compiz and xfwm4.
	   *
	   * Note, this (non documented) mechanism is unlikely to be an issue
	   * as it's used only for transitionning back from "all monitors" to
	   * "current monitor" mode.
	   *
	   * Applications who don't change the default mode won't trigger this
	   * mechanism.
	   */
	  for (i = 0; i < 4; ++i)
	    xclient.data.l[i] = G_MAXLONG;

	  break;

	case GDK_FULLSCREEN_ON_ALL_MONITORS:

	  _gdk_x11_screen_get_edge_monitors (GDK_WINDOW_SCREEN (window),
					     &gdk_monitors[0],
					     &gdk_monitors[1],
					     &gdk_monitors[2],
					     &gdk_monitors[3]);
	  /* Translate all 4 monitors from the GDK set into XINERAMA indices */
	  for (i = 0; i < 4; ++i)
	    {
	      xclient.data.l[i] = _gdk_x11_screen_get_xinerama_index (GDK_WINDOW_SCREEN (window),
								      gdk_monitors[i]);
	      /* Sanity check, if XINERAMA is not available, we could have invalid
	       * negative values for the XINERAMA indices.
	       */
	      if (xclient.data.l[i] < 0)
		{
		  g_warning ("gdk_x11_window_apply_fullscreen_mode: Invalid XINERAMA monitor index");
		  return;
		}
	    }
	  break;

	default:
	  g_warning ("gdk_x11_window_apply_fullscreen_mode: Unhandled fullscreen mode %d",
		     window->fullscreen_mode);
	  return;
	}

      /* Send fullscreen monitors client message */
      xclient.data.l[4] = 1; /* source indication */
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
								    "_NET_WM_FULLSCREEN_MONITORS");
      XSendEvent (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XROOTWIN (window), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *)&xclient);
    }
}

static void
gdk_x11_window_fullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      gdk_wmspec_change_state (TRUE, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_FULLSCREEN"),
                               GDK_NONE);
      /* Actual XRandR layout may have change since we computed the fullscreen
       * monitors in GDK_FULLSCREEN_ON_ALL_MONITORS mode.
       */
      if (window->fullscreen_mode == GDK_FULLSCREEN_ON_ALL_MONITORS)
	gdk_x11_window_apply_fullscreen_mode (window);
    }
  else
    gdk_synthesize_window_state (window,
                                 0,
                                 GDK_WINDOW_STATE_FULLSCREEN);
}

static void
gdk_x11_window_unfullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_wmspec_change_state (FALSE, window,
			     gdk_atom_intern_static_string ("_NET_WM_STATE_FULLSCREEN"),
                             GDK_NONE);

  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_FULLSCREEN,
				 0);
}

static void
gdk_x11_window_set_keep_above (GdkWindow *window,
			       gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      if (setting)
	gdk_wmspec_change_state (FALSE, window,
				 gdk_atom_intern_static_string ("_NET_WM_STATE_BELOW"),
				 GDK_NONE);
      gdk_wmspec_change_state (setting, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_ABOVE"),
			       GDK_NONE);
    }
  else
    gdk_synthesize_window_state (window,
    				 setting ? GDK_WINDOW_STATE_BELOW : GDK_WINDOW_STATE_ABOVE,
				 setting ? GDK_WINDOW_STATE_ABOVE : 0);
}

static void
gdk_x11_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      if (setting)
	gdk_wmspec_change_state (FALSE, window,
				 gdk_atom_intern_static_string ("_NET_WM_STATE_ABOVE"),
				 GDK_NONE);
      gdk_wmspec_change_state (setting, window,
			       gdk_atom_intern_static_string ("_NET_WM_STATE_BELOW"),
			       GDK_NONE);
    }
  else
    gdk_synthesize_window_state (window,
				 setting ? GDK_WINDOW_STATE_ABOVE : GDK_WINDOW_STATE_BELOW,
				 setting ? GDK_WINDOW_STATE_BELOW : 0);
}

static GdkWindow *
gdk_x11_window_get_group (GdkWindow *window)
{
  GdkToplevelX11 *toplevel;
  
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return NULL;
  
  toplevel = _gdk_x11_window_get_toplevel (window);

  return toplevel->group_leader;
}

static void
gdk_x11_window_set_group (GdkWindow *window,
			  GdkWindow *leader)
{
  GdkToplevelX11 *toplevel;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  g_return_if_fail (leader == NULL || GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) ||
      (leader != NULL && GDK_WINDOW_DESTROYED (leader)) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (leader == NULL)
    leader = gdk_display_get_default_group (gdk_window_get_display (window));
  
  if (toplevel->group_leader != leader)
    {
      if (toplevel->group_leader)
	g_object_unref (toplevel->group_leader);
      toplevel->group_leader = g_object_ref (leader);
      (_gdk_x11_window_get_toplevel (leader))->is_leader = TRUE;      
    }

  update_wm_hints (window, FALSE);
}

static MotifWmHints *
gdk_window_get_mwm_hints (GdkWindow *window)
{
  GdkDisplay *display;
  Atom hints_atom = None;
  guchar *data;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  display = gdk_window_get_display (window);
  
  hints_atom = gdk_x11_get_xatom_by_name_for_display (display, _XA_MOTIF_WM_HINTS);

  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, &data);

  if (type == None)
    return NULL;
  
  return (MotifWmHints *)data;
}

static void
gdk_window_set_mwm_hints (GdkWindow *window,
			  MotifWmHints *new_hints)
{
  GdkDisplay *display;
  Atom hints_atom = None;
  guchar *data;
  MotifWmHints *hints;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  display = gdk_window_get_display (window);
  
  hints_atom = gdk_x11_get_xatom_by_name_for_display (display, _XA_MOTIF_WM_HINTS);

  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, &data);
  
  if (type == None)
    hints = new_hints;
  else
    {
      hints = (MotifWmHints *)data;
	
      if (new_hints->flags & MWM_HINTS_FUNCTIONS)
	{
	  hints->flags |= MWM_HINTS_FUNCTIONS;
	  hints->functions = new_hints->functions;
	}
      if (new_hints->flags & MWM_HINTS_DECORATIONS)
	{
	  hints->flags |= MWM_HINTS_DECORATIONS;
	  hints->decorations = new_hints->decorations;
	}
    }
  
  XChangeProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		   hints_atom, hints_atom, 32, PropModeReplace,
		   (guchar *)hints, sizeof (MotifWmHints)/sizeof (long));
  
  if (hints != new_hints)
    XFree (hints);
}

static void
gdk_x11_window_set_decorations (GdkWindow      *window,
				GdkWMDecoration decorations)
{
  MotifWmHints hints;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
  
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&hints, 0, sizeof(hints));
  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = decorations;
  
  gdk_window_set_mwm_hints (window, &hints);
}

static gboolean
gdk_x11_window_get_decorations(GdkWindow       *window,
			       GdkWMDecoration *decorations)
{
  MotifWmHints *hints;
  gboolean result = FALSE;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return FALSE;
  
  hints = gdk_window_get_mwm_hints (window);
  
  if (hints)
    {
      if (hints->flags & MWM_HINTS_DECORATIONS)
	{
	  if (decorations)
	    *decorations = hints->decorations;
	  result = TRUE;
	}
      
      XFree (hints);
    }

  return result;
}

static void
gdk_x11_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  MotifWmHints hints;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
  
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&hints, 0, sizeof(hints));
  hints.flags = MWM_HINTS_FUNCTIONS;
  hints.functions = functions;
  
  gdk_window_set_mwm_hints (window, &hints);
}

cairo_region_t *
_gdk_x11_xwindow_get_shape (Display *xdisplay,
                            Window   window,
                            gint     scale,
                            gint     shape_type)
{
  cairo_region_t *shape;
  GdkRectangle *rl;
  XRectangle *xrl;
  gint rn, ord, i;

  shape = NULL;
  rn = 0;

  /* Note that XShapeGetRectangles returns NULL in two situations:
   * - the server doesn't support the SHAPE extension
   * - the shape is empty
   *
   * Since we can't discriminate these here, we always return
   * an empty shape. It is the callers responsibility to check
   * whether the server supports the SHAPE extensions beforehand.
   */
  xrl = XShapeGetRectangles (xdisplay, window, shape_type, &rn, &ord);

  if (rn == 0)
    return cairo_region_create (); /* Empty */

  if (ord != YXBanded)
    {
      /* This really shouldn't happen with any xserver, as they
       * generally convert regions to YXBanded internally
       */
      g_warning ("non YXBanded shape masks not supported");
      XFree (xrl);
      return NULL;
    }

  /* NOTE: The scale divisions here may lose some precision if someone
     else set the shape to be non-scale precision */
  rl = g_new (GdkRectangle, rn);
  for (i = 0; i < rn; i++)
    {
      rl[i].x = xrl[i].x / scale;
      rl[i].y = xrl[i].y / scale;
      rl[i].width = xrl[i].width / scale;
      rl[i].height = xrl[i].height / scale;
    }
  XFree (xrl);

  shape = cairo_region_create_rectangles (rl, rn);
  g_free (rl);

  return shape;
}


static cairo_region_t *
gdk_x11_window_get_shape (GdkWindow *window)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_display_supports_shapes (GDK_WINDOW_DISPLAY (window)))
    return _gdk_x11_xwindow_get_shape (GDK_WINDOW_XDISPLAY (window),
                                       GDK_WINDOW_XID (window),
                                       impl->window_scale,
                                       ShapeBounding);

  return NULL;
}

static cairo_region_t *
gdk_x11_window_get_input_shape (GdkWindow *window)
{
#if defined(ShapeInput)
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_display_supports_input_shapes (GDK_WINDOW_DISPLAY (window)))
    return _gdk_x11_xwindow_get_shape (GDK_WINDOW_XDISPLAY (window),
                                       GDK_WINDOW_XID (window),
                                       impl->window_scale,
                                       ShapeInput);
#endif

  return NULL;
}

static void
gdk_window_set_static_bit_gravity (GdkWindow *window,
                                   gboolean   on)
{
  XSetWindowAttributes xattributes;
  guint xattributes_mask = 0;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->input_only)
    return;
  
  xattributes.bit_gravity = StaticGravity;
  xattributes_mask |= CWBitGravity;
  xattributes.bit_gravity = on ? StaticGravity : ForgetGravity;
  XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			   GDK_WINDOW_XID (window),
			   CWBitGravity,  &xattributes);
}

static void
gdk_window_set_static_win_gravity (GdkWindow *window,
                                   gboolean   on)
{
  XSetWindowAttributes xattributes;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  xattributes.win_gravity = on ? StaticGravity : NorthWestGravity;
  
  XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			   GDK_WINDOW_XID (window),
			   CWWinGravity,  &xattributes);
}

static gboolean
gdk_window_x11_set_static_gravities (GdkWindow *window,
                                     gboolean   use_static)
{
  GList *tmp_list;
  
  if (!use_static == !window->guffaw_gravity)
    return TRUE;

  window->guffaw_gravity = use_static;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      gdk_window_set_static_bit_gravity (window, use_static);
      
      tmp_list = window->children;
      while (tmp_list)
	{
	  gdk_window_set_static_win_gravity (tmp_list->data, use_static);
	  
	  tmp_list = tmp_list->next;
	}
    }
  
  return TRUE;
}

/* From the WM spec */
#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */

static void
wmspec_send_message (GdkDisplay *display,
                     GdkWindow  *window,
                     gint        root_x,
                     gint        root_y,
                     gint        action,
                     gint        button)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  XClientMessageEvent xclient;

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type =
    gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_MOVERESIZE");
  xclient.format = 32;
  xclient.data.l[0] = root_x * impl->window_scale;
  xclient.data.l[1] = root_y * impl->window_scale;
  xclient.data.l[2] = action;
  xclient.data.l[3] = button;
  xclient.data.l[4] = 1;  /* source indication */

  XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XROOTWIN (window), False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}

static void
handle_wmspec_button_release (GdkDisplay *display,
                              XEvent     *xevent)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkWindow *window;

#if defined (HAVE_XGENERICEVENTS) && defined (XINPUT_2)
  XIEvent *xiev = (XIEvent *) xevent->xcookie.data;
  XIDeviceEvent *xidev = (XIDeviceEvent *) xiev;

  if (xevent->xany.type == GenericEvent)
    window = gdk_x11_window_lookup_for_display (display, xidev->event);
  else
#endif
    window = gdk_x11_window_lookup_for_display (display, xevent->xany.window);

  if (display_x11->wm_moveresize_button != 0 && window != NULL)
    {
      if ((xevent->xany.type == ButtonRelease &&
           xevent->xbutton.button == display_x11->wm_moveresize_button)
#if defined (HAVE_XGENERICEVENTS) && defined (XINPUT_2)
          ||
          (xevent->xany.type == GenericEvent &&
           xiev->evtype == XI_ButtonRelease &&
           xidev->detail == display_x11->wm_moveresize_button)
#endif
          )
        {
          display_x11->wm_moveresize_button = 0;
          wmspec_send_message (display, window, 0, 0, _NET_WM_MOVERESIZE_CANCEL, 0);
        }
    }
}

static void
wmspec_moveresize (GdkWindow *window,
                   gint       direction,
                   GdkDevice *device,
                   gint       button,
                   gint       root_x,
                   gint       root_y,
                   guint32    timestamp)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);

  if (button != 0)
    gdk_device_ungrab (device, timestamp); /* Release passive grab */
  GDK_X11_DISPLAY (display)->wm_moveresize_button = button;

  wmspec_send_message (display, window, root_x, root_y, direction, button);
}

static void
wmspec_resize_drag (GdkWindow     *window,
                    GdkWindowEdge  edge,
                    GdkDevice     *device,
                    gint           button,
                    gint           root_x,
                    gint           root_y,
                    guint32        timestamp)
{
  gint direction;
  
  if (button == 0)
    direction = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
  else
    switch (edge)
      {
      /* Let the compiler turn a switch into a table, instead
       * of doing the table manually, this way is easier to verify.
       */
      case GDK_WINDOW_EDGE_NORTH_WEST:
        direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
        break;

      case GDK_WINDOW_EDGE_NORTH:
        direction = _NET_WM_MOVERESIZE_SIZE_TOP;
        break;

      case GDK_WINDOW_EDGE_NORTH_EAST:
        direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
        break;

      case GDK_WINDOW_EDGE_WEST:
        direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
        break;

      case GDK_WINDOW_EDGE_EAST:
        direction = _NET_WM_MOVERESIZE_SIZE_RIGHT;
        break;

      case GDK_WINDOW_EDGE_SOUTH_WEST:
        direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
        break;

      case GDK_WINDOW_EDGE_SOUTH:
        direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
        break;

      case GDK_WINDOW_EDGE_SOUTH_EAST:
        direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
        break;

      default:
        g_warning ("gdk_window_begin_resize_drag: bad resize edge %d!",
                   edge);
        return;
      }
  
  wmspec_moveresize (window, direction, device, button, root_x, root_y, timestamp);
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;

  GdkWindow *moveresize_window;
  GdkWindow *moveresize_emulation_window;
  gboolean is_resize;
  GdkWindowEdge resize_edge;
  GdkDevice *device;
  gint moveresize_button;
  gint moveresize_x;
  gint moveresize_y;
  gint moveresize_orig_x;
  gint moveresize_orig_y;
  gint moveresize_orig_width;
  gint moveresize_orig_height;
  GdkWindowHints moveresize_geom_mask;
  GdkGeometry moveresize_geometry;
  Time moveresize_process_time;
  XEvent *moveresize_pending_event;
};

static MoveResizeData *
get_move_resize_data (GdkDisplay *display,
		      gboolean    create)
{
  MoveResizeData *mv_resize;
  static GQuark move_resize_quark = 0;

  if (!move_resize_quark)
    move_resize_quark = g_quark_from_static_string ("gdk-window-moveresize");
  
  mv_resize = g_object_get_qdata (G_OBJECT (display), move_resize_quark);

  if (!mv_resize && create)
    {
      mv_resize = g_new0 (MoveResizeData, 1);
      mv_resize->display = display;
      
      g_object_set_qdata (G_OBJECT (display), move_resize_quark, mv_resize);
    }

  return mv_resize;
}

static void
check_maximize (MoveResizeData *mv_resize,
                gdouble         x_root,
                gdouble         y_root)
{
  GdkWindowState state;
  gint y;

  if (mv_resize->is_resize)
    return;

  state = gdk_window_get_state (mv_resize->moveresize_window);

  if (state & GDK_WINDOW_STATE_MAXIMIZED)
    return;

  y = mv_resize->moveresize_orig_y + (y_root - mv_resize->moveresize_y);

  if (y < 10)
    gdk_window_maximize (mv_resize->moveresize_window);
}

static void
check_unmaximize (MoveResizeData *mv_resize,
                  gdouble         x_root,
                  gdouble         y_root)
{
  GdkWindowState state;
  gint dx, dy;

  if (mv_resize->is_resize)
    return;

  state = gdk_window_get_state (mv_resize->moveresize_window);

  if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_TILED)) == 0)
    return;

  dx = x_root - mv_resize->moveresize_x;
  dy = y_root - mv_resize->moveresize_y;

  if (ABS (dx) > 20 || ABS (dy) > 20)
    gdk_window_unmaximize (mv_resize->moveresize_window);
}

static void
update_pos (MoveResizeData *mv_resize,
	    gint            new_root_x,
	    gint            new_root_y)
{
  gint dx, dy;

  check_unmaximize (mv_resize, new_root_x, new_root_y);
  dx = new_root_x - mv_resize->moveresize_x;
  dy = new_root_y - mv_resize->moveresize_y;

  if (mv_resize->is_resize)
    {
      gint x, y, w, h;

      x = mv_resize->moveresize_orig_x;
      y = mv_resize->moveresize_orig_y;

      w = mv_resize->moveresize_orig_width;
      h = mv_resize->moveresize_orig_height;

      switch (mv_resize->resize_edge)
	{
	case GDK_WINDOW_EDGE_NORTH_WEST:
	  x += dx;
	  y += dy;
	  w -= dx;
	  h -= dy;
	  break;
	case GDK_WINDOW_EDGE_NORTH:
	  y += dy;
	  h -= dy;
	  break;
	case GDK_WINDOW_EDGE_NORTH_EAST:
	  y += dy;
	  h -= dy;
	  w += dx;
	  break;
	case GDK_WINDOW_EDGE_SOUTH_WEST:
	  h += dy;
	  x += dx;
	  w -= dx;
	  break;
	case GDK_WINDOW_EDGE_SOUTH_EAST:
	  w += dx;
	  h += dy;
	  break;
	case GDK_WINDOW_EDGE_SOUTH:
	  h += dy;
	  break;
	case GDK_WINDOW_EDGE_EAST:
	  w += dx;
	  break;
	case GDK_WINDOW_EDGE_WEST:
	  x += dx;
	  w -= dx;
	  break;
	}

      x = MAX (x, 0);
      y = MAX (y, 0);
      w = MAX (w, 1);
      h = MAX (h, 1);

      if (mv_resize->moveresize_geom_mask)
	{
	  gdk_window_constrain_size (&mv_resize->moveresize_geometry,
				     mv_resize->moveresize_geom_mask,
				     w, h, &w, &h);
	}

      gdk_window_move_resize (mv_resize->moveresize_window, x, y, w, h);
    }
  else
    {
      gint x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_window_move (mv_resize->moveresize_window, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_window_destroy (mv_resize->moveresize_emulation_window);
  mv_resize->moveresize_emulation_window = NULL;
  g_object_unref (mv_resize->moveresize_window);
  mv_resize->moveresize_window = NULL;

  if (mv_resize->moveresize_pending_event)
    {
      g_free (mv_resize->moveresize_pending_event);
      mv_resize->moveresize_pending_event = NULL;
    }
}

static int
lookahead_motion_predicate (Display *xdisplay,
			    XEvent  *event,
			    XPointer arg)
{
  gboolean *seen_release = (gboolean *)arg;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (*seen_release)
    return False;

  switch (event->xany.type)
    {
    case ButtonRelease:
      *seen_release = TRUE;
      break;
    case MotionNotify:
      mv_resize->moveresize_process_time = event->xmotion.time;
      break;
    default:
      break;
    }

  return False;
}

static gboolean
moveresize_lookahead (MoveResizeData *mv_resize,
		      XEvent         *event)
{
  XEvent tmp_event;
  gboolean seen_release = FALSE;

  if (mv_resize->moveresize_process_time)
    {
      if (event->xmotion.time == mv_resize->moveresize_process_time)
        {
          mv_resize->moveresize_process_time = 0;
          return TRUE;
        }
      else
        return FALSE;
    }

  XCheckIfEvent (event->xany.display, &tmp_event,
                 lookahead_motion_predicate, (XPointer) & seen_release);

  return mv_resize->moveresize_process_time == 0;
}

gboolean
_gdk_x11_moveresize_handle_event (XEvent *event)
{
  guint button_mask = 0;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (event->xany.display);
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);
  GdkWindowImplX11 *impl;

  if (!mv_resize || !mv_resize->moveresize_window)
    {
      handle_wmspec_button_release (display, event);
      return FALSE;
    }

  impl = GDK_WINDOW_IMPL_X11 (mv_resize->moveresize_window->impl);

  if (mv_resize->moveresize_button != 0)
    button_mask = GDK_BUTTON1_MASK << (mv_resize->moveresize_button - 1);

  switch (event->xany.type)
    {
    case MotionNotify:
      if (mv_resize->moveresize_window->resize_count > 0)
        {
          if (mv_resize->moveresize_pending_event)
            *mv_resize->moveresize_pending_event = *event;
          else
            mv_resize->moveresize_pending_event =
              g_memdup (event, sizeof (XEvent));

          break;
        }
      if (!moveresize_lookahead (mv_resize, event))
        break;

      update_pos (mv_resize,
                  event->xmotion.x_root / impl->window_scale,
                  event->xmotion.y_root / impl->window_scale);

      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->xmotion.state & button_mask) == 0)
        {
          check_maximize (mv_resize,
                          event->xmotion.x_root / impl->window_scale,
                          event->xmotion.y_root / impl->window_scale);
          finish_drag (mv_resize);
        }
      break;

    case ButtonRelease:
      update_pos (mv_resize,
                  event->xbutton.x_root / impl->window_scale,
                  event->xbutton.y_root / impl->window_scale);

      if (event->xbutton.button == mv_resize->moveresize_button)
        {
          check_maximize (mv_resize,
                          event->xmotion.x_root / impl->window_scale,
                          event->xmotion.y_root / impl->window_scale);
          finish_drag (mv_resize);
        }
      break;

#if defined (HAVE_XGENERICEVENTS) && defined (XINPUT_2)
    case GenericEvent:
      {
        /* we just assume this is an XI2 event */
        XIEvent *ev = (XIEvent *) event->xcookie.data;
        XIDeviceEvent *xev = (XIDeviceEvent *)ev;
        gint state;
        switch (ev->evtype)
          {
          case XI_Motion:
            update_pos (mv_resize, xev->root_x / impl->window_scale, xev->root_y / impl->window_scale);
            state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
            if ((state & button_mask) == 0)
              {
                check_maximize (mv_resize,
                                xev->root_x / impl->window_scale,
                                xev->root_y / impl->window_scale);
                finish_drag (mv_resize);
              }
            break;

          case XI_ButtonRelease:
            update_pos (mv_resize, xev->root_x / impl->window_scale, xev->root_y / impl->window_scale);
            if (xev->detail == mv_resize->moveresize_button)
              {
                check_maximize (mv_resize,
                                xev->root_x / impl->window_scale,
                                xev->root_y / impl->window_scale);
                finish_drag (mv_resize);
              }
            break;
          }
      }
      break;
#endif

    }
  return TRUE;
}

gboolean
_gdk_x11_moveresize_configure_done (GdkDisplay *display,
                                    GdkWindow  *window)
{
  XEvent *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || window != mv_resize->moveresize_window)
    return FALSE;

  if (mv_resize->moveresize_pending_event)
    {
      tmp_event = mv_resize->moveresize_pending_event;
      mv_resize->moveresize_pending_event = NULL;
      _gdk_x11_moveresize_handle_event (tmp_event);
      g_free (tmp_event);
    }

  return TRUE;
}

static void
create_moveresize_window (MoveResizeData *mv_resize,
                          guint32         timestamp)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGrabStatus status;

  g_assert (mv_resize->moveresize_emulation_window == NULL);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  mv_resize->moveresize_emulation_window = 
    gdk_window_new (gdk_screen_get_root_window (gdk_display_get_default_screen (mv_resize->display)),
		    &attributes,
		    attributes_mask);

  gdk_window_show (mv_resize->moveresize_emulation_window);

  status = gdk_device_grab (mv_resize->device,
                            mv_resize->moveresize_emulation_window,
                            GDK_OWNERSHIP_NONE,
                            FALSE,
                            GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
                            NULL,
                            timestamp);

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the window
       * already.
       */
      finish_drag (mv_resize);
    }

  mv_resize->moveresize_process_time = 0;
}

/* 
   Calculate mv_resize->moveresize_orig_x and mv_resize->moveresize_orig_y
   so that calling XMoveWindow with these coordinates will not move the 
   window.
   Note that this depends on the WM to implement ICCCM-compliant reference
   point handling.
*/
static void 
calculate_unmoving_origin (MoveResizeData *mv_resize)
{
  GdkRectangle rect;
  gint width, height;

  if (mv_resize->moveresize_geom_mask & GDK_HINT_WIN_GRAVITY &&
      mv_resize->moveresize_geometry.win_gravity == GDK_GRAVITY_STATIC)
    {
      gdk_window_get_origin (mv_resize->moveresize_window,
			     &mv_resize->moveresize_orig_x,
			     &mv_resize->moveresize_orig_y);
    }
  else
    {
      gdk_window_get_frame_extents (mv_resize->moveresize_window, &rect);
      gdk_window_get_geometry (mv_resize->moveresize_window, 
			       NULL, NULL, &width, &height);
      
      switch (mv_resize->moveresize_geometry.win_gravity) 
	{
	case GDK_GRAVITY_NORTH_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_NORTH:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;	  
	case GDK_GRAVITY_NORTH_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_CENTER:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_SOUTH_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	case GDK_GRAVITY_SOUTH:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	case GDK_GRAVITY_SOUTH_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	default:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y;
	  break; 
	}
    }  
}

static void
emulate_resize_drag (GdkWindow     *window,
                     GdkWindowEdge  edge,
                     GdkDevice     *device,
                     gint           button,
                     gint           root_x,
                     gint           root_y,
                     guint32        timestamp)
{
  MoveResizeData *mv_resize = get_move_resize_data (GDK_WINDOW_DISPLAY (window), TRUE);

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->device = device;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_window = g_object_ref (window);

  mv_resize->moveresize_orig_width = gdk_window_get_width (window);
  mv_resize->moveresize_orig_height = gdk_window_get_height (window);

  mv_resize->moveresize_geom_mask = 0;
  gdk_window_get_geometry_hints (window,
				 &mv_resize->moveresize_geometry,
				 &mv_resize->moveresize_geom_mask);

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static void
emulate_move_drag (GdkWindow     *window,
                   GdkDevice     *device,
                   gint           button,
                   gint           root_x,
                   gint           root_y,
                   guint32        timestamp)
{
  MoveResizeData *mv_resize = get_move_resize_data (GDK_WINDOW_DISPLAY (window), TRUE);
  
  mv_resize->is_resize = FALSE;
  mv_resize->device = device;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;

  mv_resize->moveresize_window = g_object_ref (window);

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static gboolean
_should_perform_ewmh_drag (GdkWindow *window,
                           GdkDevice *device)
{
  GdkPointerWindowInfo *info;
  GdkDisplay *display;

  display = gdk_window_get_display (window);
  info = _gdk_display_get_pointer_info (display, device);

  if ((!info->last_slave || gdk_device_get_source (info->last_slave) != GDK_SOURCE_TOUCHSCREEN) &&
      gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
                                           gdk_atom_intern_static_string ("_NET_WM_MOVERESIZE")))
    return TRUE;

  return FALSE;
}

static void
gdk_x11_window_begin_resize_drag (GdkWindow     *window,
                                  GdkWindowEdge  edge,
                                  GdkDevice     *device,
                                  gint           button,
                                  gint           root_x,
                                  gint           root_y,
                                  guint32        timestamp)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  /* Avoid EWMH for touch devices */
  if (_should_perform_ewmh_drag (window, device))
    wmspec_resize_drag (window, edge, device, button, root_x, root_y, timestamp);
  else
    emulate_resize_drag (window, edge, device, button, root_x, root_y, timestamp);
}

static void
gdk_x11_window_begin_move_drag (GdkWindow *window,
                                GdkDevice *device,
				gint       button,
				gint       root_x,
				gint       root_y,
				guint32    timestamp)
{
  gint direction;

  if (GDK_WINDOW_DESTROYED (window) || !WINDOW_IS_TOPLEVEL (window))
    return;

  if (button == 0)
    direction = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
  else
    direction = _NET_WM_MOVERESIZE_MOVE;

  /* Avoid EWMH for touch devices */
  if (_should_perform_ewmh_drag (window, device))
    wmspec_moveresize (window, direction, device, button, root_x, root_y, timestamp);
  else
    emulate_move_drag (window, device, button, root_x, root_y, timestamp);
}

static gboolean
gdk_x11_window_beep (GdkWindow *window)
{
  GdkDisplay *display;

  display = GDK_WINDOW_DISPLAY (window);

#ifdef HAVE_XKB
  if (GDK_X11_DISPLAY (display)->use_xkb)
    {
      XkbBell (GDK_DISPLAY_XDISPLAY (display),
               GDK_WINDOW_XID (window),
               0,
               None);
      return TRUE;
    }
#endif

  return FALSE;
}

static void
gdk_x11_window_set_opacity (GdkWindow *window,
			    gdouble    opacity)
{
  GdkDisplay *display;
  guint32 cardinal;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  display = gdk_window_get_display (window);

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  cardinal = opacity * 0xffffffff;

  if (cardinal == 0xffffffff)
    XDeleteProperty (GDK_DISPLAY_XDISPLAY (display),
		     GDK_WINDOW_XID (window),
		     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_OPACITY"));
  else
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		     GDK_WINDOW_XID (window),
		     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_OPACITY"),
		     XA_CARDINAL, 32,
		     PropModeReplace,
		     (guchar *) &cardinal, 1);
}

static void
gdk_x11_window_set_composited (GdkWindow *window,
                               gboolean   composited)
{
#if defined(HAVE_XCOMPOSITE) && defined(HAVE_XDAMAGE) && defined (HAVE_XFIXES)
  GdkWindowImplX11 *impl;
  GdkDisplay *display;
  Display *dpy;
  Window xid;

  impl = GDK_WINDOW_IMPL_X11 (window->impl);

  display = gdk_window_get_display (window);
  dpy = GDK_DISPLAY_XDISPLAY (display);
  xid = GDK_WINDOW_XID (window);

  if (composited)
    {
      XCompositeRedirectWindow (dpy, xid, CompositeRedirectManual);
      impl->damage = XDamageCreate (dpy, xid, XDamageReportBoundingBox);
    }
  else
    {
      XCompositeUnredirectWindow (dpy, xid, CompositeRedirectManual);
      XDamageDestroy (dpy, impl->damage);
      impl->damage = None;
    }
#endif
}

void
_gdk_x11_display_before_process_all_updates (GdkDisplay *display)
{
}

void
_gdk_x11_display_after_process_all_updates (GdkDisplay *display)
{
  /* Sync after all drawing, otherwise the client can get "ahead" of
     the server rendering during animations, such that we fill up
     the Xserver pipes with sync rendering ops not letting other
     clients (including the VM) do anything. */
  XSync (GDK_DISPLAY_XDISPLAY (display), FALSE);
}

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  Window xwindow = GPOINTER_TO_UINT (arg);
  GdkDisplay *gdk_display = gdk_x11_lookup_xdisplay (display);

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == xwindow &&
      xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (gdk_display,
								       "GDK_TIMESTAMP_PROP"))
    return True;

  return False;
}

/**
 * gdk_x11_get_server_time:
 * @window: (type GdkX11Window): a #GdkWindow, used for communication
 *          with the server.  The window must have
 *          GDK_PROPERTY_CHANGE_MASK in its events mask or a hang will
 *          result.
 *
 * Routine to get the current X server time stamp.
 *
 * Returns: the time stamp.
 **/
guint32
gdk_x11_get_server_time (GdkWindow *window)
{
  Display *xdisplay;
  Window   xwindow;
  guchar c = 'a';
  XEvent xevent;
  Atom timestamp_prop_atom;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (!GDK_WINDOW_DESTROYED (window), 0);

  xdisplay = GDK_WINDOW_XDISPLAY (window);
  xwindow = GDK_WINDOW_XID (window);
  timestamp_prop_atom =
    gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
					   "GDK_TIMESTAMP_PROP");

  XChangeProperty (xdisplay, xwindow, timestamp_prop_atom,
		   timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (xdisplay, &xevent,
	    timestamp_predicate, GUINT_TO_POINTER(xwindow));

  return xevent.xproperty.time;
}

/**
 * gdk_x11_window_get_xid:
 * @window: (type GdkX11Window): a native #GdkWindow.
 * 
 * Returns the X resource (window) belonging to a #GdkWindow.
 * 
 * Returns: the ID of @drawable’s X resource.
 **/
XID
gdk_x11_window_get_xid (GdkWindow *window)
{
  /* Try to ensure the window has a native window */
  if (!_gdk_window_has_impl (window))
    {
      gdk_window_ensure_native (window);

      /* We sync here to ensure the window is created in the Xserver when
       * this function returns. This is required because the returned XID
       * for this window must be valid immediately, even with another
       * connection to the Xserver */
      gdk_display_sync (gdk_window_get_display (window));
    }
  
  if (!GDK_WINDOW_IS_X11 (window))
    {
      g_warning (G_STRLOC " drawable is not a native X11 window");
      return None;
    }
  
  return GDK_WINDOW_IMPL_X11 (window->impl)->xid;
}

static gint
gdk_x11_window_get_scale_factor (GdkWindow *window)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return 1;

  return impl->window_scale;
}

/**
 * gdk_x11_window_set_frame_sync_enabled:
 * @window: (type GdkX11Window): a native #GdkWindow
 * @frame_sync_enabled: whether frame-synchronization should be enabled
 *
 * This function can be used to disable frame synchronization for a window.
 * Normally frame synchronziation will be enabled or disabled based on whether
 * the system has a compositor that supports frame synchronization, but if
 * the window is not directly managed by the window manager, then frame
 * synchronziation may need to be disabled. This is the case for a window
 * embedded via the XEMBED protocol.
 *
 * Since: 3.8
 */
void
gdk_x11_window_set_frame_sync_enabled (GdkWindow *window,
                                       gboolean   frame_sync_enabled)
{
  /* Try to ensure the window has a native window */
  if (!_gdk_window_has_impl (window))
    gdk_window_ensure_native (window);

  if (!GDK_WINDOW_IS_X11 (window))
    {
      g_warning (G_STRLOC " drawable is not a native X11 window");
      return;
    }

  GDK_WINDOW_IMPL_X11 (window->impl)->frame_sync_enabled = FALSE;
}

static void
gdk_x11_window_set_opaque_region (GdkWindow      *window,
                                  cairo_region_t *region)
{
  GdkWindowImplX11 *impl = GDK_WINDOW_IMPL_X11 (window->impl);
  GdkDisplay *display;
  int nitems;
  gulong *data;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (region != NULL)
    {
      int i, nrects;

      nrects = cairo_region_num_rectangles (region);
      nitems = nrects * 4;
      data = g_new (gulong, nitems);

      for (i = 0; i < nrects; i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (region, i, &rect);
          data[i*4+0] = rect.x * impl->window_scale;
          data[i*4+1] = rect.y * impl->window_scale;
          data[i*4+2] = rect.width * impl->window_scale;
          data[i*4+3] = rect.height * impl->window_scale;
        }
    }
  else
    {
      nitems = 0;
      data = NULL;
    }

  display = gdk_window_get_display (window);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (window),
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_OPAQUE_REGION"),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *) data, nitems);

  if (data != NULL)
    g_free (data);
}

static gboolean
gdk_x11_window_show_window_menu (GdkWindow *window,
                                 GdkEvent  *event)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  GdkDevice *device;
  int device_id;
  double x_root, y_root;
  XClientMessageEvent xclient = { 0 };

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      break;
    default:
      return FALSE;
    }

  if (!gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
                                            gdk_atom_intern_static_string ("_GTK_SHOW_WINDOW_MENU")))
    return FALSE;

  gdk_event_get_root_coords (event, &x_root, &y_root);
  device = gdk_event_get_device (event);
  g_object_get (G_OBJECT (device),
                "device-id", &device_id,
                NULL);

  /* Ungrab the implicit grab */
  gdk_device_ungrab (device, gdk_event_get_time (event));

  xclient.type = ClientMessage;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_GTK_SHOW_WINDOW_MENU");
  xclient.data.l[0] = device_id;
  xclient.data.l[1] = x_root;
  xclient.data.l[2] = y_root;
  xclient.format = 32;

  XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XROOTWIN (window), False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);

  return TRUE;
}

static void
gdk_window_impl_x11_class_init (GdkWindowImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);
  
  object_class->finalize = gdk_window_impl_x11_finalize;
  
  impl_class->ref_cairo_surface = gdk_x11_ref_cairo_surface;
  impl_class->show = gdk_window_x11_show;
  impl_class->hide = gdk_window_x11_hide;
  impl_class->withdraw = gdk_window_x11_withdraw;
  impl_class->set_events = gdk_window_x11_set_events;
  impl_class->get_events = gdk_window_x11_get_events;
  impl_class->raise = gdk_window_x11_raise;
  impl_class->lower = gdk_window_x11_lower;
  impl_class->restack_under = gdk_window_x11_restack_under;
  impl_class->restack_toplevel = gdk_window_x11_restack_toplevel;
  impl_class->move_resize = gdk_window_x11_move_resize;
  impl_class->set_background = gdk_window_x11_set_background;
  impl_class->reparent = gdk_window_x11_reparent;
  impl_class->set_device_cursor = gdk_window_x11_set_device_cursor;
  impl_class->get_geometry = gdk_window_x11_get_geometry;
  impl_class->get_root_coords = gdk_window_x11_get_root_coords;
  impl_class->get_device_state = gdk_window_x11_get_device_state;
  impl_class->shape_combine_region = gdk_window_x11_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_x11_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_window_x11_set_static_gravities;
  impl_class->queue_antiexpose = _gdk_x11_window_queue_antiexpose;
  impl_class->destroy = gdk_x11_window_destroy;
  impl_class->destroy_foreign = gdk_x11_window_destroy_foreign;
  impl_class->get_shape = gdk_x11_window_get_shape;
  impl_class->get_input_shape = gdk_x11_window_get_input_shape;
  impl_class->beep = gdk_x11_window_beep;

  impl_class->focus = gdk_x11_window_focus;
  impl_class->set_type_hint = gdk_x11_window_set_type_hint;
  impl_class->get_type_hint = gdk_x11_window_get_type_hint;
  impl_class->set_modal_hint = gdk_x11_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_x11_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_x11_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_x11_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_x11_window_set_geometry_hints;
  impl_class->set_title = gdk_x11_window_set_title;
  impl_class->set_role = gdk_x11_window_set_role;
  impl_class->set_startup_id = gdk_x11_window_set_startup_id;
  impl_class->set_transient_for = gdk_x11_window_set_transient_for;
  impl_class->get_frame_extents = gdk_x11_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_x11_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_x11_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_x11_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_x11_window_set_icon_list;
  impl_class->set_icon_name = gdk_x11_window_set_icon_name;
  impl_class->iconify = gdk_x11_window_iconify;
  impl_class->deiconify = gdk_x11_window_deiconify;
  impl_class->stick = gdk_x11_window_stick;
  impl_class->unstick = gdk_x11_window_unstick;
  impl_class->maximize = gdk_x11_window_maximize;
  impl_class->unmaximize = gdk_x11_window_unmaximize;
  impl_class->fullscreen = gdk_x11_window_fullscreen;
  impl_class->apply_fullscreen_mode = gdk_x11_window_apply_fullscreen_mode;
  impl_class->unfullscreen = gdk_x11_window_unfullscreen;
  impl_class->set_keep_above = gdk_x11_window_set_keep_above;
  impl_class->set_keep_below = gdk_x11_window_set_keep_below;
  impl_class->get_group = gdk_x11_window_get_group;
  impl_class->set_group = gdk_x11_window_set_group;
  impl_class->set_decorations = gdk_x11_window_set_decorations;
  impl_class->get_decorations = gdk_x11_window_get_decorations;
  impl_class->set_functions = gdk_x11_window_set_functions;
  impl_class->set_functions = gdk_x11_window_set_functions;
  impl_class->begin_resize_drag = gdk_x11_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_x11_window_begin_move_drag;
  impl_class->set_opacity = gdk_x11_window_set_opacity;
  impl_class->set_composited = gdk_x11_window_set_composited;
  impl_class->destroy_notify = gdk_x11_window_destroy_notify;
  impl_class->get_drag_protocol = gdk_x11_window_get_drag_protocol;
  impl_class->register_dnd = _gdk_x11_window_register_dnd;
  impl_class->drag_begin = _gdk_x11_window_drag_begin;
  impl_class->sync_rendering = _gdk_x11_window_sync_rendering;
  impl_class->simulate_key = _gdk_x11_window_simulate_key;
  impl_class->simulate_button = _gdk_x11_window_simulate_button;
  impl_class->get_property = _gdk_x11_window_get_property;
  impl_class->change_property = _gdk_x11_window_change_property;
  impl_class->delete_property = _gdk_x11_window_delete_property;
  impl_class->get_scale_factor = gdk_x11_window_get_scale_factor;
  impl_class->set_opaque_region = gdk_x11_window_set_opaque_region;
  impl_class->set_shadow_width = gdk_x11_window_set_shadow_width;
  impl_class->show_window_menu = gdk_x11_window_show_window_menu;
}
