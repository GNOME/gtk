/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <glib.h>
#include "gdk.h"
#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"
#include "gdkscreen.h"
#include "gdkdisplaymanager.h"
#include "gdkalias.h"


extern void _gdk_visual_init            (void);
extern void _gdk_events_init            (void);
extern void _gdk_input_init             (void);
extern void _gdk_dnd_init               (void);
extern void _gdk_windowing_window_init  (GdkScreen *screen);
extern void _gdk_windowing_image_init   (void);
extern void _gdk_directfb_keyboard_init (void);

static gboolean   gdk_directfb_argb_font           = FALSE;
static gint       gdk_directfb_glyph_surface_cache = 8;


const GOptionEntry _gdk_windowing_args[] =
  {
    { "disable-aa-fonts",0,0,G_OPTION_ARG_INT,&gdk_directfb_monochrome_fonts,NULL,NULL    },
    { "argb-font",0,0, G_OPTION_ARG_INT, &gdk_directfb_argb_font,NULL,NULL},
    { "transparent-unfocused",0,0, G_OPTION_ARG_INT, &gdk_directfb_apply_focus_opacity,NULL,NULL },
    { "glyph-surface-cache",0,0,G_OPTION_ARG_INT,&gdk_directfb_glyph_surface_cache,NULL,NULL },
    { "enable-color-keying",0,0,G_OPTION_ARG_INT,&gdk_directfb_enable_color_keying,NULL,NULL },
    { NULL }
  };

/* Main entry point for gdk in 2.6 args are parsed
 */
GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  IDirectFB             *directfb;
  IDirectFBDisplayLayer *layer;
  IDirectFBInputDevice  *keyboard;
  DFBResult              ret;

  if (_gdk_display)
    {
      return GDK_DISPLAY_OBJECT (_gdk_display); /* single display only */
    }

  ret = DirectFBInit (NULL, NULL);
  if (ret != DFB_OK)
    {
      DirectFBError ("gdk_display_open: DirectFBInit", ret);
      return NULL;
    }

  ret = DirectFBCreate (&directfb);
  if (ret != DFB_OK)
    {
      DirectFBError ("gdk_display_open: DirectFBCreate", ret);
      return NULL;
    }

  _gdk_display = g_object_new (GDK_TYPE_DISPLAY_DFB, NULL);
  _gdk_display->directfb = directfb;

  ret = directfb->GetDisplayLayer (directfb, DLID_PRIMARY, &layer);
  if (ret != DFB_OK)
    {
      DirectFBError ("gdk_display_open: GetDisplayLayer", ret);
      directfb->Release (directfb);
      _gdk_display->directfb = NULL;
      return NULL;
    }


  ret = directfb->GetInputDevice (directfb, DIDID_KEYBOARD, &keyboard);
  if (ret != DFB_OK)
    {
      DirectFBError ("gdk_display_open: GetInputDevice", ret);
      directfb->Release (directfb);
      _gdk_display->directfb = NULL;
      return NULL;
    }

  _gdk_display->layer    = layer;
  _gdk_display->keyboard = keyboard;

  _gdk_directfb_keyboard_init ();

  _gdk_screen = g_object_new (GDK_TYPE_SCREEN, NULL);

  _gdk_visual_init ();
  _gdk_windowing_window_init (_gdk_screen);

  gdk_screen_set_default_colormap (_gdk_screen,
                                   gdk_screen_get_system_colormap (_gdk_screen));
  _gdk_windowing_image_init ();

  _gdk_events_init ();
  _gdk_input_init ();
  _gdk_dnd_init ();

  layer->EnableCursor (layer, 1);

  g_signal_emit_by_name (gdk_display_manager_get (),
			 "display_opened", _gdk_display);

  return GDK_DISPLAY_OBJECT (_gdk_display);
}

GType
gdk_display_dfb_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
        {
          sizeof (GdkDisplayDFBClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) NULL,
          NULL,                 /* class_finalize */
          NULL,                 /* class_data */
          sizeof (GdkDisplayDFB),
          0,                    /* n_preallocs */
          (GInstanceInitFunc) NULL,
        };

      object_type = g_type_register_static (GDK_TYPE_DISPLAY,
                                            "GdkDisplayDFB",
                                            &object_info, 0);
    }

  return object_type;
}

IDirectFBSurface *
gdk_display_dfb_create_surface (GdkDisplayDFB *display, int format, int width, int height)
{
  DFBResult              ret;
  IDirectFBSurface      *temp;
  DFBSurfaceDescription  dsc;
  dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
  dsc.width       = width;
  dsc.height      = height;
  dsc.pixelformat = format;
  ret             = display->directfb->CreateSurface (display->directfb, &dsc, &temp);
  if (ret)
    {
      DirectFBError ("gdk_display_dfb_create_surface ", ret);
      return NULL;
    }
  return temp;

}


/*************************************************************************************************
 * Displays and Screens
 */

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  _gdk_display = GDK_DISPLAY_DFB (display);
}

const gchar *
gdk_display_get_name (GdkDisplay *display)
{
  return gdk_get_display_arg_name ();
}

int
gdk_display_get_n_screens (GdkDisplay *display)
{
  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  return _gdk_screen;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
}

gboolean
gdk_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}


GdkWindow *gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return  _gdk_parent_root;
}


/*************************************************************************************************
 * Selection and Clipboard
 */

gboolean
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

gboolean gdk_display_request_selection_notification  (GdkDisplay *display,
                                                      GdkAtom     selection)

{
  g_warning("gdk_display_request_selection_notification Unimplemented function \n");
  return FALSE;
}

gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  g_warning("gdk_display_supports_clipboard_persistence Unimplemented function \n");
  return FALSE;
}

void
gdk_display_store_clipboard (GdkDisplay    *display,
                             GdkWindow     *clipboard_window,
                             guint32        time_,
                             const GdkAtom *targets,
                             gint           n_targets)
{

  g_warning("gdk_display_store_clipboard Unimplemented function \n");

}


/*************************************************************************************************
 * Pointer
 */

static gboolean _gdk_directfb_pointer_implicit_grab = FALSE;

GdkGrabStatus
gdk_directfb_pointer_grab (GdkWindow    *window,
                           gint          owner_events,
                           GdkEventMask  event_mask,
                           GdkWindow    *confine_to,
                           GdkCursor    *cursor,
                           guint32       time,
                           gboolean      implicit_grab)
{
  GdkWindow             *toplevel;
  GdkWindowImplDirectFB *impl;

  if (_gdk_directfb_pointer_grab_window)
    {
      if (implicit_grab && !_gdk_directfb_pointer_implicit_grab)
        return GDK_GRAB_ALREADY_GRABBED;

      gdk_pointer_ungrab (time);
    }

  toplevel = gdk_directfb_window_find_toplevel (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (toplevel)->impl);

  if (impl->window)
    {
      if (impl->window->GrabPointer (impl->window) == DFB_LOCKED)
        return GDK_GRAB_ALREADY_GRABBED;
    }

  if (event_mask & GDK_BUTTON_MOTION_MASK)
    event_mask |= (GDK_BUTTON1_MOTION_MASK |
                   GDK_BUTTON2_MOTION_MASK |
                   GDK_BUTTON3_MOTION_MASK);

  _gdk_directfb_pointer_implicit_grab     = implicit_grab;
  _gdk_directfb_pointer_grab_window       = g_object_ref (window);
  _gdk_directfb_pointer_grab_owner_events = owner_events;

  _gdk_directfb_pointer_grab_confine      = (confine_to ?
                                             g_object_ref (confine_to) : NULL);
  _gdk_directfb_pointer_grab_events       = event_mask;
  _gdk_directfb_pointer_grab_cursor       = (cursor ?
                                             gdk_cursor_ref (cursor) : NULL);


  gdk_directfb_window_send_crossing_events (NULL,
                                            window,
                                            GDK_CROSSING_GRAB);

  return GDK_GRAB_SUCCESS;
}

void
gdk_directfb_pointer_ungrab (guint32  time,
                             gboolean implicit_grab)
{
  GdkWindow             *toplevel;
  GdkWindow             *mousewin;
  GdkWindow             *old_grab_window;
  GdkWindowImplDirectFB *impl;

  if (implicit_grab && !_gdk_directfb_pointer_implicit_grab)
    return;

  if (!_gdk_directfb_pointer_grab_window)
    return;

  toplevel =
    gdk_directfb_window_find_toplevel (_gdk_directfb_pointer_grab_window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (toplevel)->impl);

  if (impl->window)
    impl->window->UngrabPointer (impl->window);

  if (_gdk_directfb_pointer_grab_confine)
    {
      g_object_unref (_gdk_directfb_pointer_grab_confine);
      _gdk_directfb_pointer_grab_confine = NULL;
    }

  if (_gdk_directfb_pointer_grab_cursor)
    {
      gdk_cursor_unref (_gdk_directfb_pointer_grab_cursor);
      _gdk_directfb_pointer_grab_cursor = NULL;
    }

  old_grab_window = _gdk_directfb_pointer_grab_window;

  _gdk_directfb_pointer_grab_window   = NULL;
  _gdk_directfb_pointer_implicit_grab = FALSE;

  mousewin = gdk_window_at_pointer (NULL, NULL);
  gdk_directfb_window_send_crossing_events (old_grab_window,
                                            mousewin,
                                            GDK_CROSSING_UNGRAB);
  g_object_unref (old_grab_window);
}

void
gdk_display_pointer_ungrab (GdkDisplay *display,
                            guint32 time)
{
  GdkPointerGrabInfo *grab = _gdk_display_get_last_pointer_grab (display);

  if (grab)
    {
      grab->serial_end = 0;
    }

  _gdk_display_pointer_grab_update (display, 0);
}


/*************************************************************************************************
 * Keyboard
 */

GdkGrabStatus
gdk_directfb_keyboard_grab (GdkDisplay *display,
                            GdkWindow  *window,
                            gint        owner_events,
                            guint32     time)
{
  GdkWindow             *toplevel;
  GdkWindowImplDirectFB *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (_gdk_directfb_keyboard_grab_window)
    gdk_keyboard_ungrab (time);

  toplevel = gdk_directfb_window_find_toplevel (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (toplevel)->impl);

  if (impl->window)
    {
      if (impl->window->GrabKeyboard (impl->window) == DFB_LOCKED)
        return GDK_GRAB_ALREADY_GRABBED;
    }

  _gdk_directfb_keyboard_grab_window = g_object_ref (window);
  _gdk_directfb_keyboard_grab_owner_events = owner_events;
  return GDK_GRAB_SUCCESS;
}

void
gdk_directfb_keyboard_ungrab (GdkDisplay *display,
                              guint32     time)
{
  GdkWindow             *toplevel;
  GdkWindowImplDirectFB *impl;

  if (!_gdk_directfb_keyboard_grab_window)
    return;

  toplevel = gdk_directfb_window_find_toplevel (_gdk_directfb_keyboard_grab_window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (toplevel)->impl);

  if (impl->window)
    impl->window->UngrabKeyboard (impl->window);

  g_object_unref (_gdk_directfb_keyboard_grab_window);
  _gdk_directfb_keyboard_grab_window = NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_grab
 *
 *   Grabs the keyboard to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_keyboard_ungrab
 *
 *--------------------------------------------------------------
 */

GdkGrabStatus
gdk_display_keyboard_grab (GdkDisplay *display,
                           GdkWindow  *window,
                           gint        owner_events,
                           guint32     time)
{
  return gdk_directfb_keyboard_grab (display, window, owner_events, time);
}

void
gdk_display_keyboard_ungrab (GdkDisplay *display,
                             guint32     time)
{
  return gdk_directfb_keyboard_ungrab (display, time);
}


/*************************************************************************************************
 * Misc Stuff
 */

void
gdk_display_beep (GdkDisplay *display)
{
}

void
gdk_display_sync (GdkDisplay *display)
{
}

void
gdk_display_flush (GdkDisplay *display)
{
}



/*************************************************************************************************
 * Notifications
 */

void
gdk_notify_startup_complete (void)
{
}

/**
 * gdk_notify_startup_complete_with_id:
 * @startup_id: a startup-notification identifier, for which notification
 *              process should be completed
 *
 * Indicates to the GUI environment that the application has finished
 * loading, using a given identifier.
 *
 * GTK+ will call this function automatically for #GtkWindow with custom
 * startup-notification identifier unless
 * gtk_window_set_auto_startup_notification() is called to disable
 * that feature.
 *
 * Since: 2.12
 **/
void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
}


/*************************************************************************************************
 * Compositing
 */

gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}


#define __GDK_DISPLAY_X11_C__
#include "gdkaliasdef.c"

