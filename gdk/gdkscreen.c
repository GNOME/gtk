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

#include "gdkscreenprivate.h"

#include "gdkinternals.h"
#include "gdkmonitorprivate.h"
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
 * the top level windows are to be displayed on. it is also used to
 * query the screen specification and default settings such as
 * the default visual (gdk_screen_get_system_visual()), the dimensions
 * of the physical monitors (gdk_screen_get_monitor_geometry()), etc.
 */


static void gdk_screen_finalize     (GObject        *object);
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
  PROP_FONT_OPTIONS,
  PROP_RESOLUTION
};

enum
{
  SIZE_CHANGED,
  COMPOSITED_CHANGED,
  MONITORS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkScreen, gdk_screen, G_TYPE_OBJECT)

static void
gdk_screen_class_init (GdkScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_screen_finalize;
  object_class->set_property = gdk_screen_set_property;
  object_class->get_property = gdk_screen_get_property;

  g_object_class_install_property (object_class,
				   PROP_FONT_OPTIONS,
				   g_param_spec_pointer ("font-options",
							 P_("Font options"),
							 P_("The default font options for the screen"),
							 G_PARAM_READWRITE|G_PARAM_STATIC_NAME|
							G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

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
   * GdkScreen::size-changed:
   * @screen: the object on which the signal is emitted
   * 
   * The ::size-changed signal is emitted when the pixel width or 
   * height of a screen changes.
   *
   * Since: 2.2
   */
  signals[SIZE_CHANGED] =
    g_signal_new (g_intern_static_string ("size-changed"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkScreenClass, size_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GdkScreen::composited-changed:
   * @screen: the object on which the signal is emitted
   *
   * The ::composited-changed signal is emitted when the composited
   * status of the screen changes
   *
   * Since: 2.10
   */
  signals[COMPOSITED_CHANGED] =
    g_signal_new (g_intern_static_string ("composited-changed"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkScreenClass, composited_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE,
		  0);
	
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
		  NULL,
		  G_TYPE_NONE,
		  0);
}

static void
gdk_screen_init (GdkScreen *screen)
{
  screen->resolution = -1.;
}

static void
gdk_screen_finalize (GObject *object)
{
  GdkScreen *screen = GDK_SCREEN (object);

  if (screen->font_options)
      cairo_font_options_destroy (screen->font_options);

  G_OBJECT_CLASS (gdk_screen_parent_class)->finalize (object);
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

static int
get_monitor_num (GdkMonitor *monitor)
{
  GdkDisplay *display;
  int n_monitors, i;

  display = gdk_monitor_get_display (monitor);
  n_monitors = gdk_display_get_n_monitors (display);
  for (i = 0; i < n_monitors; i++)
    {
      if (gdk_display_get_monitor (display, i) == monitor)
        return i;
    }
  return -1;
}

/**
 * gdk_screen_get_monitor_at_point:
 * @screen: a #GdkScreen.
 * @x: the x coordinate in the virtual screen.
 * @y: the y coordinate in the virtual screen.
 *
 * Returns the monitor number in which the point (@x,@y) is located.
 *
 * Returns: the monitor number in which the point (@x,@y) lies, or
 *   a monitor close to (@x,@y) if the point is not in any monitor.
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use gdk_display_get_monitor_at_point() instead
 **/
gint
gdk_screen_get_monitor_at_point (GdkScreen *screen,
				 gint       x,
				 gint       y)
{
  GdkDisplay *display;
  GdkMonitor *monitor;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

  display = gdk_screen_get_display (screen);
  monitor = gdk_display_get_monitor_at_point (display, x, y);
  return get_monitor_num (monitor);
}

/**
 * gdk_screen_get_monitor_at_window:
 * @screen: a #GdkScreen.
 * @window: a #GdkWindow
 *
 * Returns the number of the monitor in which the largest area of the
 * bounding rectangle of @window resides.
 *
 * Returns: the monitor number in which most of @window is located,
 *     or if @window does not intersect any monitors, a monitor,
 *     close to @window.
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use gdk_display_get_monitor_at_window() instead
 **/
gint
gdk_screen_get_monitor_at_window (GdkScreen *screen,
                                  GdkWindow *window)
{
  GdkDisplay *display;
  GdkMonitor *monitor;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  g_return_val_if_fail (GDK_IS_WINDOW (window), -1);

  display = gdk_screen_get_display (screen);
  monitor = gdk_display_get_monitor_at_window (display, window);
  return get_monitor_num (monitor);
}

/**
 * gdk_screen_width:
 *
 * Gets the width of the default screen in pixels. The returned
 * size is in ”application pixels”, not in ”device pixels” (see
 * gdk_screen_get_monitor_scale_factor()).
 *
 * Returns: the width of the default screen in pixels.
 *
 * Deprecated: 3.22: Use per-monitor information
 **/
gint
gdk_screen_width (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gdk_screen_get_width (gdk_screen_get_default ());
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gdk_screen_height:
 *
 * Gets the height of the default screen in pixels. The returned
 * size is in ”application pixels”, not in ”device pixels” (see
 * gdk_screen_get_monitor_scale_factor()).
 *
 * Returns: the height of the default screen in pixels.
 *
 * Deprecated: 3.22: Use per-monitor information
 **/
gint
gdk_screen_height (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gdk_screen_get_height (gdk_screen_get_default ());
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gdk_screen_width_mm:
 *
 * Returns the width of the default screen in millimeters.
 * Note that on many X servers this value will not be correct.
 *
 * Returns: the width of the default screen in millimeters,
 * though it is not always correct.
 *
 * Deprecated: 3.22: Use per-monitor information
 **/
gint
gdk_screen_width_mm (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gdk_screen_get_width_mm (gdk_screen_get_default ());
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gdk_screen_height_mm:
 *
 * Returns the height of the default screen in millimeters.
 * Note that on many X servers this value will not be correct.
 *
 * Returns: the height of the default screen in millimeters,
 * though it is not always correct.
 *
 * Deprecated: 3.22: Use per-monitor information
 **/
gint
gdk_screen_height_mm (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return gdk_screen_get_height_mm (gdk_screen_get_default ());
G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * gdk_screen_set_font_options:
 * @screen: a #GdkScreen
 * @options: (allow-none): a #cairo_font_options_t, or %NULL to unset any
 *   previously set default font options.
 *
 * Sets the default font options for the screen. These
 * options will be set on any #PangoContext’s newly created
 * with gdk_pango_context_get_for_screen(). Changing the
 * default set of font options does not affect contexts that
 * have already been created.
 *
 * Since: 2.10
 **/
void
gdk_screen_set_font_options (GdkScreen                  *screen,
			     const cairo_font_options_t *options)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));

  if (screen->font_options != options)
    {
      if (screen->font_options)
        cairo_font_options_destroy (screen->font_options);

      if (options)
        screen->font_options = cairo_font_options_copy (options);
      else
        screen->font_options = NULL;

      g_object_notify (G_OBJECT (screen), "font-options");
    }
}

/**
 * gdk_screen_get_font_options:
 * @screen: a #GdkScreen
 * 
 * Gets any options previously set with gdk_screen_set_font_options().
 * 
 * Returns: (nullable): the current font options, or %NULL if no
 *  default font options have been set.
 *
 * Since: 2.10
 **/
const cairo_font_options_t *
gdk_screen_get_font_options (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return screen->font_options;
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
    case PROP_FONT_OPTIONS:
      g_value_set_pointer (value, (gpointer) gdk_screen_get_font_options (screen));
      break;
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
    case PROP_FONT_OPTIONS:
      gdk_screen_set_font_options (screen, g_value_get_pointer (value));
      break;
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
 * gdk_screen_get_width:
 * @screen: a #GdkScreen
 *
 * Gets the width of @screen in pixels. The returned size is in
 * ”application pixels”, not in ”device pixels” (see
 * gdk_screen_get_monitor_scale_factor()).
 *
 * Returns: the width of @screen in pixels.
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use per-monitor information instead
 **/
gint
gdk_screen_get_width (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_GET_CLASS (screen)->get_width (screen);
}

/**
 * gdk_screen_get_height:
 * @screen: a #GdkScreen
 *
 * Gets the height of @screen in pixels. The returned size is in
 * ”application pixels”, not in ”device pixels” (see
 * gdk_screen_get_monitor_scale_factor()).
 *
 * Returns: the height of @screen in pixels.
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use per-monitor information instead
 **/
gint
gdk_screen_get_height (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_GET_CLASS (screen)->get_height (screen);
}

/**
 * gdk_screen_get_width_mm:
 * @screen: a #GdkScreen
 *
 * Gets the width of @screen in millimeters.
 *
 * Note that this value is somewhat ill-defined when the screen
 * has multiple monitors of different resolution. It is recommended
 * to use the monitor dimensions instead.
 *
 * Returns: the width of @screen in millimeters.
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use per-monitor information instead
 **/
gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_GET_CLASS (screen)->get_width_mm (screen);
}

/**
 * gdk_screen_get_height_mm:
 * @screen: a #GdkScreen
 *
 * Returns the height of @screen in millimeters.
 *
 * Note that this value is somewhat ill-defined when the screen
 * has multiple monitors of different resolution. It is recommended
 * to use the monitor dimensions instead.
 *
 * Returns: the heigth of @screen in millimeters.
 *
 * Deprecated: 3.22: Use per-monitor information instead
 *
 * Since: 2.2
 **/
gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_GET_CLASS (screen)->get_height_mm (screen);
}

/**
 * gdk_screen_get_number:
 * @screen: a #GdkScreen
 *
 * Gets the index of @screen among the screens in the display
 * to which it belongs. (See gdk_screen_get_display())
 *
 * Returns: the index
 *
 * Since: 2.2
 *
 * Deprecated: 3.22
 **/
gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_GET_CLASS (screen)->get_number (screen);
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

static GdkMonitor *
get_monitor (GdkScreen *screen,
             gint       n)
{
  GdkDisplay *display;

  display = gdk_screen_get_display (screen);
  return gdk_display_get_monitor (display, n);
}

/**
 * gdk_screen_get_n_monitors:
 * @screen: a #GdkScreen
 *
 * Returns the number of monitors which @screen consists of.
 *
 * Returns: number of monitors which @screen consists of
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use gdk_display_get_n_monitors() instead
 */
gint
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  display = gdk_screen_get_display (screen);
  return gdk_display_get_n_monitors (display);
}

/**
 * gdk_screen_get_primary_monitor:
 * @screen: a #GdkScreen.
 *
 * Gets the primary monitor for @screen.  The primary monitor
 * is considered the monitor where the “main desktop” lives.
 * While normal application windows typically allow the window
 * manager to place the windows, specialized desktop applications
 * such as panels should place themselves on the primary monitor.
 *
 * If no primary monitor is configured by the user, the return value
 * will be 0, defaulting to the first monitor.
 *
 * Returns: An integer index for the primary monitor, or 0 if none is configured.
 *
 * Since: 2.20
 *
 * Deprecated: 3.22: Use gdk_display_get_primary_monitor() instead
 */
gint
gdk_screen_get_primary_monitor (GdkScreen *screen)
{
  GdkDisplay *display;
  GdkMonitor *primary;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  display = gdk_screen_get_display (screen);
  primary = gdk_display_get_primary_monitor (display);
  if (primary)
    return get_monitor_num (primary);

  return 0;
}

/**
 * gdk_screen_get_monitor_width_mm:
 * @screen: a #GdkScreen
 * @monitor_num: number of the monitor, between 0 and gdk_screen_get_n_monitors (screen)
 *
 * Gets the width in millimeters of the specified monitor, if available.
 *
 * Returns: the width of the monitor, or -1 if not available
 *
 * Since: 2.14
 *
 * Deprecated: 3.22: Use gdk_monitor_get_width_mm() instead
 */
gint
gdk_screen_get_monitor_width_mm	(GdkScreen *screen,
				 gint       monitor_num)
{
  GdkMonitor *monitor;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

  monitor = get_monitor (screen, monitor_num);

  g_return_val_if_fail (monitor != NULL, -1);

  return gdk_monitor_get_width_mm (monitor);
}

/**
 * gdk_screen_get_monitor_height_mm:
 * @screen: a #GdkScreen
 * @monitor_num: number of the monitor, between 0 and gdk_screen_get_n_monitors (screen)
 *
 * Gets the height in millimeters of the specified monitor.
 *
 * Returns: the height of the monitor, or -1 if not available
 *
 * Since: 2.14
 *
 * Deprecated: 3.22: Use gdk_monitor_get_height_mm() instead
 */
gint
gdk_screen_get_monitor_height_mm (GdkScreen *screen,
                                  gint       monitor_num)
{
  GdkMonitor *monitor;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

  monitor = get_monitor (screen, monitor_num);

  g_return_val_if_fail (monitor != NULL, -1);

  return gdk_monitor_get_height_mm (monitor);
}

/**
 * gdk_screen_get_monitor_plug_name:
 * @screen: a #GdkScreen
 * @monitor_num: number of the monitor, between 0 and gdk_screen_get_n_monitors (screen)
 *
 * Returns the output name of the specified monitor.
 * Usually something like VGA, DVI, or TV, not the actual
 * product name of the display device.
 *
 * Returns: (nullable): a newly-allocated string containing the name
 *   of the monitor, or %NULL if the name cannot be determined
 *
 * Since: 2.14
 *
 * Deprecated: 3.22: Use gdk_monitor_get_model() instead
 */
gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen,
				  gint       monitor_num)
{
  GdkMonitor *monitor;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  monitor = get_monitor (screen, monitor_num);

  g_return_val_if_fail (monitor != NULL, NULL);

  return g_strdup (gdk_monitor_get_connector (monitor));
}

/**
 * gdk_screen_get_monitor_geometry:
 * @screen: a #GdkScreen
 * @monitor_num: the monitor number
 * @dest: (out) (allow-none): a #GdkRectangle to be filled with
 *     the monitor geometry
 *
 * Retrieves the #GdkRectangle representing the size and position of
 * the individual monitor within the entire screen area. The returned
 * geometry is in ”application pixels”, not in ”device pixels” (see
 * gdk_screen_get_monitor_scale_factor()).
 *
 * Monitor numbers start at 0. To obtain the number of monitors of
 * @screen, use gdk_screen_get_n_monitors().
 *
 * Note that the size of the entire screen area can be retrieved via
 * gdk_screen_get_width() and gdk_screen_get_height().
 *
 * Since: 2.2
 *
 * Deprecated: 3.22: Use gdk_monitor_get_geometry() instead
 */
void
gdk_screen_get_monitor_geometry (GdkScreen    *screen,
				 gint          monitor_num,
				 GdkRectangle *dest)
{
  GdkMonitor *monitor;

  g_return_if_fail (GDK_IS_SCREEN (screen));

  monitor = get_monitor (screen, monitor_num);

  g_return_if_fail (monitor != NULL);

  gdk_monitor_get_geometry (monitor, dest);
}

/**
 * gdk_screen_get_monitor_workarea:
 * @screen: a #GdkScreen
 * @monitor_num: the monitor number
 * @dest: (out) (allow-none): a #GdkRectangle to be filled with
 *     the monitor workarea
 *
 * Retrieves the #GdkRectangle representing the size and position of
 * the “work area” on a monitor within the entire screen area. The returned
 * geometry is in ”application pixels”, not in ”device pixels” (see
 * gdk_screen_get_monitor_scale_factor()).
 *
 * The work area should be considered when positioning menus and
 * similar popups, to avoid placing them below panels, docks or other
 * desktop components.
 *
 * Note that not all backends may have a concept of workarea. This
 * function will return the monitor geometry if a workarea is not
 * available, or does not apply.
 *
 * Monitor numbers start at 0. To obtain the number of monitors of
 * @screen, use gdk_screen_get_n_monitors().
 *
 * Since: 3.4
 *
 * Deprecated: 3.22: Use gdk_monitor_get_workarea() instead
 */
void
gdk_screen_get_monitor_workarea (GdkScreen    *screen,
                                 gint          monitor_num,
                                 GdkRectangle *dest)
{
  GdkMonitor *monitor;

  g_return_if_fail (GDK_IS_SCREEN (screen));

  monitor = get_monitor (screen, monitor_num);

  g_return_if_fail (monitor != NULL);

  gdk_monitor_get_workarea (monitor, dest);
}

/**
 * gdk_screen_list_visuals:
 * @screen: the relevant #GdkScreen.
 *
 * Lists the available visuals for the specified @screen.
 * A visual describes a hardware image data format.
 * For example, a visual might support 24-bit color, or 8-bit color,
 * and might expect pixels to be in a certain format.
 *
 * Call g_list_free() on the return value when you’re finished with it.
 *
 * Returns: (transfer container) (element-type GdkVisual):
 *     a list of visuals; the list must be freed, but not its contents
 *
 * Since: 2.2
 **/
GList *
gdk_screen_list_visuals (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->list_visuals (screen);
}

/**
 * gdk_screen_get_system_visual:
 * @screen: a #GdkScreen.
 *
 * Get the system’s default visual for @screen.
 * This is the visual for the root window of the display.
 * The return value should not be freed.
 *
 * Returns: (transfer none): the system visual
 *
 * Since: 2.2
 **/
GdkVisual *
gdk_screen_get_system_visual (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->get_system_visual (screen);
}

/**
 * gdk_screen_get_rgba_visual:
 * @screen: a #GdkScreen
 *
 * Gets a visual to use for creating windows with an alpha channel.
 * The windowing system on which GTK+ is running
 * may not support this capability, in which case %NULL will
 * be returned. Even if a non-%NULL value is returned, its
 * possible that the window’s alpha channel won’t be honored
 * when displaying the window on the screen: in particular, for
 * X an appropriate windowing manager and compositing manager
 * must be running to provide appropriate display.
 *
 * This functionality is not implemented in the Windows backend.
 *
 * For setting an overall opacity for a top-level window, see
 * gdk_window_set_opacity().
 *
 * Returns: (nullable) (transfer none): a visual to use for windows
 *     with an alpha channel or %NULL if the capability is not
 *     available.
 *
 * Since: 2.8
 **/
GdkVisual *
gdk_screen_get_rgba_visual (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->get_rgba_visual (screen);
}

/**
 * gdk_screen_is_composited:
 * @screen: a #GdkScreen
 *
 * Returns whether windows with an RGBA visual can reasonably
 * be expected to have their alpha channel drawn correctly on
 * the screen.
 *
 * On X11 this function returns whether a compositing manager is
 * compositing @screen.
 *
 * Returns: Whether windows with RGBA visuals can reasonably be
 * expected to have their alpha channels drawn correctly on the screen.
 *
 * Since: 2.10
 **/
gboolean
gdk_screen_is_composited (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  return GDK_SCREEN_GET_CLASS (screen)->is_composited (screen);
}

/**
 * gdk_screen_make_display_name:
 * @screen: a #GdkScreen
 *
 * Determines the name to pass to gdk_display_open() to get
 * a #GdkDisplay with this screen as the default screen.
 *
 * Returns: a newly allocated string, free with g_free()
 *
 * Since: 2.2
 *
 * Deprecated: 3.22
 **/
gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->make_display_name (screen);
}

/**
 * gdk_screen_get_active_window:
 * @screen: a #GdkScreen
 *
 * Returns the screen’s currently active window.
 *
 * On X11, this is done by inspecting the _NET_ACTIVE_WINDOW property
 * on the root window, as described in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec).
 * If there is no currently currently active
 * window, or the window manager does not support the
 * _NET_ACTIVE_WINDOW hint, this function returns %NULL.
 *
 * On other platforms, this function may return %NULL, depending on whether
 * it is implementable on that platform.
 *
 * The returned window should be unrefed using g_object_unref() when
 * no longer needed.
 *
 * Returns: (nullable) (transfer full): the currently active window,
 *   or %NULL.
 *
 * Since: 2.10
 *
 * Deprecated: 3.22
 **/
GdkWindow *
gdk_screen_get_active_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->get_active_window (screen);
}

/**
 * gdk_screen_get_window_stack:
 * @screen: a #GdkScreen
 *
 * Returns a #GList of #GdkWindows representing the current
 * window stack.
 *
 * On X11, this is done by inspecting the _NET_CLIENT_LIST_STACKING
 * property on the root window, as described in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec).
 * If the window manager does not support the
 * _NET_CLIENT_LIST_STACKING hint, this function returns %NULL.
 *
 * On other platforms, this function may return %NULL, depending on whether
 * it is implementable on that platform.
 *
 * The returned list is newly allocated and owns references to the
 * windows it contains, so it should be freed using g_list_free() and
 * its windows unrefed using g_object_unref() when no longer needed.
 *
 * Returns: (nullable) (transfer full) (element-type GdkWindow): a
 *     list of #GdkWindows for the current window stack, or %NULL.
 *
 * Since: 2.10
 **/
GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_GET_CLASS (screen)->get_window_stack (screen);
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

/**
 * gdk_screen_get_monitor_scale_factor:
 * @screen: screen to get scale factor for
 * @monitor_num: number of the monitor, between 0 and gdk_screen_get_n_monitors (screen)
 *
 * Returns the internal scale factor that maps from monitor coordinates
 * to the actual device pixels. On traditional systems this is 1, but
 * on very high density outputs this can be a higher value (often 2).
 *
 * This can be used if you want to create pixel based data for a
 * particular monitor, but most of the time you’re drawing to a window
 * where it is better to use gdk_window_get_scale_factor() instead.
 *
 * Returns: the scale factor
 *
 * Since: 3.10
 *
 * Deprecated: 3.22: Use gdk_monitor_get_scale_factor() instead
 */
gint
gdk_screen_get_monitor_scale_factor (GdkScreen *screen,
                                     gint       monitor_num)
{
  GdkMonitor *monitor;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 1);
  g_return_val_if_fail (monitor_num >= 0, 1);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_return_val_if_fail (monitor_num < gdk_screen_get_n_monitors (screen), 1);
G_GNUC_END_IGNORE_DEPRECATIONS

  monitor = get_monitor (screen, monitor_num);

  g_return_val_if_fail (monitor != NULL, 1);

  return gdk_monitor_get_scale_factor (monitor);
}
