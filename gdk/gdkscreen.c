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


static void gdk_screen_set_property (GObject        *object,
				     guint           prop_id,
				     const GValue   *value,
				     GParamSpec     *pspec);
static void gdk_screen_get_property (GObject        *object,
				     guint           prop_id,
				     GValue         *value,
				     GParamSpec     *pspec);

enum
{
  PROP_0,
  PROP_RESOLUTION
};

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

  object_class->set_property = gdk_screen_set_property;
  object_class->get_property = gdk_screen_get_property;

  g_object_class_install_property (object_class,
				   PROP_RESOLUTION,
				   g_param_spec_double ("resolution",
							P_("Font resolution"),
							P_("The resolution for fonts on the screen"),
							-1.0,
							10000.0,
							-1.0,
							G_PARAM_READWRITE|G_PARAM_STATIC_NAME|
							G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

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
  screen->resolution = -1.;
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
 * gdk_screen_set_resolution:
 * @screen: a #GdkScreen
 * @dpi: the resolution in “dots per inch”. (Physical inches aren’t actually
 *   involved; the terminology is conventional.)
 
 * Sets the resolution for font handling on the screen. This is a
 * scale factor between points specified in a #PangoFontDescription
 * and cairo units. The default value is 96, meaning that a 10 point
 * font will be 13 units high. (10 * 96. / 72. = 13.3).
 *
 * Since: 2.10
 **/
void
gdk_screen_set_resolution (GdkScreen *screen,
			   gdouble    dpi)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));

  if (dpi < 0)
    dpi = -1.0;

  screen->resolution_set = TRUE;

  if (screen->resolution != dpi)
    {
      screen->resolution = dpi;

      g_object_notify (G_OBJECT (screen), "resolution");
    }
}

/* Just like gdk_screen_set_resolution(), but doesn't change
 * screen->resolution. This is us to allow us to distinguish
 * resolution changes that the backend picks up from resolution
 * changes made through the public API - perhaps using
 * g_object_set(<GtkSetting>, "gtk-xft-dpi", ...);
 */
void
_gdk_screen_set_resolution (GdkScreen *screen,
                            gdouble    dpi)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));

  if (dpi < 0)
    dpi = -1.0;

  if (screen->resolution != dpi)
    {
      screen->resolution = dpi;

      g_object_notify (G_OBJECT (screen), "resolution");
    }
}

/**
 * gdk_screen_get_resolution:
 * @screen: a #GdkScreen
 * 
 * Gets the resolution for font handling on the screen; see
 * gdk_screen_set_resolution() for full details.
 * 
 * Returns: the current resolution, or -1 if no resolution
 * has been set.
 *
 * Since: 2.10
 **/
gdouble
gdk_screen_get_resolution (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1.0);

  return screen->resolution;
}

static void
gdk_screen_get_property (GObject      *object,
			 guint         prop_id,
			 GValue       *value,
			 GParamSpec   *pspec)
{
  GdkScreen *screen = GDK_SCREEN (object);

  switch (prop_id)
    {
    case PROP_RESOLUTION:
      g_value_set_double (value, gdk_screen_get_resolution (screen));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_screen_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GdkScreen *screen = GDK_SCREEN (object);

  switch (prop_id)
    {
    case PROP_RESOLUTION:
      gdk_screen_set_resolution (screen, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
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

/**
 * gdk_screen_get_root_window:
 * @screen: a #GdkScreen
 *
 * Gets the root window of @screen.
 *
 * Returns: (transfer none): the root window
 *
 * Since: 2.2
 **/
GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->get_root_window (screen);
}

/**
 * gdk_screen_get_setting:
 * @screen: the #GdkScreen where the setting is located
 * @name: the name of the setting
 * @value: location to store the value of the setting
 *
 * Retrieves a desktop-wide setting such as double-click time
 * for the #GdkScreen @screen.
 *
 * FIXME needs a list of valid settings here, or a link to
 * more information.
 *
 * Returns: %TRUE if the setting existed and a value was stored
 *   in @value, %FALSE otherwise.
 *
 * Since: 2.2
 **/
gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return GDK_SCREEN_GET_CLASS (screen)->get_setting (screen, name, value);
}
