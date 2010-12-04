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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_BROADWAY_H__
#define __GDK_PRIVATE_BROADWAY_H__

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkinternals.h>
#include "gdkwindow-broadway.h"
#include "gdkdisplay-broadway.h"

typedef struct _GdkCursorPrivate       GdkCursorPrivate;

void _gdk_broadway_resync_windows (void);

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  GdkDisplay *display;
};

gboolean _gdk_broadway_window_queue_antiexpose  (GdkWindow *window,
						 cairo_region_t *area);
void     _gdk_broadway_window_translate         (GdkWindow *window,
						 cairo_region_t *area,
						 gint       dx,
						 gint       dy);

void     _gdk_selection_window_destroyed   (GdkWindow            *window);

void _gdk_keymap_keys_changed     (GdkDisplay      *display);
gint _gdk_broadway_get_group_for_state (GdkDisplay      *display,
					GdkModifierType  state);
void _gdk_keymap_add_virtual_modifiers_compat (GdkKeymap       *keymap,
                                               GdkModifierType *modifiers);
gboolean _gdk_keymap_key_is_modifier   (GdkKeymap       *keymap,
					guint            keycode);

void _gdk_broadway_initialize_locale (void);

void _gdk_screen_broadway_events_init   (GdkScreen *screen);

void _gdk_events_init           (GdkDisplay *display);
void _gdk_events_uninit         (GdkDisplay *display);
void _gdk_events_got_input      (GdkDisplay *display,
				 const char *message);

void _gdk_windowing_window_init (GdkScreen *screen);
void _gdk_visual_init           (GdkScreen *screen);
void _gdk_dnd_init		(GdkDisplay *display);

void _gdk_broadway_cursor_update_theme (GdkCursor *cursor);
void _gdk_broadway_cursor_display_finalize (GdkDisplay *display);

#define GDK_SCREEN_DISPLAY(screen)    (GDK_SCREEN_BROADWAY (screen)->display)
#define GDK_WINDOW_SCREEN(win)	      (GDK_WINDOW_IMPL_BROADWAY (((GdkWindow *)win)->impl)->screen)
#define GDK_WINDOW_DISPLAY(win)       (GDK_SCREEN_BROADWAY (GDK_WINDOW_SCREEN (win))->display)
#define GDK_WINDOW_IS_BROADWAY(win)   (GDK_IS_WINDOW_IMPL_BROADWAY (((GdkWindow *)win)->impl))

#endif /* __GDK_PRIVATE_BROADWAY_H__ */
