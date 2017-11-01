/*
 * gdkscreen.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#include "gdkinternals.h"
#include "gdkscreenprivate.h"
#include "gdkrectangle.h"
#include "gdkwindow.h"
#include "gdkintl.h"


/**
 * SECTION:gdkscreen
 * @Short_description: Object representing a physical screen
 * @Title: GdkScreen
 *
 * #GdkScreen objects are the GDK representation of the screen on
 * which windows can be displayed and on which the pointer moves.
 * X originally identified screens with physical screens, but
 * nowadays it is more common to have a single #GdkScreen which
 * combines several physical monitors (see gdk_screen_get_n_monitors()).
 *
 * GdkScreen is used throughout GDK and GTK+ to specify which screen
 * the top level windows are to be displayed on.
 */


enum
{
  SIZE_CHANGED,
  MONITORS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkScreen, gdk_screen, G_TYPE_OBJECT)

static void
gdk_screen_class_init (GdkScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /**
   * GdkScreen::monitors-changed:
   * @screen: the object on which the signal is emitted
   *
   * The ::monitors-changed signal is emitted when the number, size
   * or position of the monitors attached to the screen change. 
   *
   * Only for X11 and OS X for now. A future implementation for Win32
   * may be a possibility.
   *
   * Since: 2.14
   */
  signals[MONITORS_CHANGED] =
    g_signal_new (g_intern_static_string ("monitors-changed"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkScreenClass, monitors_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
}

static void
gdk_screen_init (GdkScreen *screen)
{
}

void 
_gdk_screen_close (GdkScreen *screen)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));

  if (!screen->closed)
    {
      screen->closed = TRUE;
      g_object_run_dispose (G_OBJECT (screen));
    }
}

/**
 * gdk_screen_get_display:
 * @screen: a #GdkScreen
 *
 * Gets the display to which the @screen belongs.
 *
 * Returns: (transfer none): the display to which @screen belongs
 *
 * Since: 2.2
 **/
GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->get_display (screen);
}
