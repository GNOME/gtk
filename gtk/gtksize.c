/* GTK - The GIMP Toolkit
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

#include "config.h"
#include <stdlib.h>
#include <math.h>
#include "gtk.h"
#include "gtksize.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "gtkalias.h"

/**
 * SECTION:gtksize
 * @short_description: Resolution independent rendering
 *
 * The #GtkSize type is used to implement resolution independent
 * rendering in applications. This involves designing the application
 * to use pixel-independent units. By using high bits in a #gint, the
 * standard integer type is overloaded to be able to carry information
 * about the unit; for example pixels or <ulink
 * url="http://en.wikipedia.org/wiki/Em_(typography)">em</ulink>'s. At
 * run-time, depending on physical characteristics of the output
 * device and user preferences, a size specified in units can be
 * converted to pixels. Here is a screenshot of #GtkFileChooserDialog
 * rendered at 1 em equaling 6 pixels
 *
 * <inlinegraphic fileref="gtk-ri-file-chooser-size-6.png" format="PNG"/>
 *
 * and for 1 em equaling 12 pixels
 *
 * <inlinegraphic fileref="gtk-ri-file-chooser-size-12.png" format="PNG"/>
 *
 * and finally for 1 em equaling 24 pixels
 *
 * <inlinegraphic fileref="gtk-ri-file-chooser-size-24.png" format="PNG"/>
 *
 * To specify a pixel size, simply treat #GtkSize as an integer:
 * <programlisting>
 * GtkSize size;
 * size = 42;
 * </programlisting>
 * To specify an em:
 * <programlisting>
 * GtkSize size;
 * size = gtk_size_em (2.5);
 * </programlisting>
 * To specify a millimeter:
 * <programlisting>
 * GtkSize size;
 * size = gtk_size_mm (25.4); // one inch
 * </programlisting>
 * Note that using millimeter or other physical units in user
 * interfaces is mostly always a bad idea; use em instead as the UI
 * elements will the scale correctly with the font. One notable
 * exception to this rule is interfaces designed for finger use where
 * it is desirable that a button matches the size of e.g. a
 * fingertip.
 *
 * Internally, #GtkSize stores ems and millimeters using fixed precision; as
 * such floating point numbers can be passed to the gtk_size_em() and
 * gtk_size_mm() functions. Use gtk_size_get_unit(), gtk_size_get_em()
 * and gtk_size_get_mm() to inspect a #GtkSize.
 *
 * To convert a #GtkSize to pixels simply use gtk_size_to_pixel() (or
 * gtk_size_to_pixel_double() to get a floating point number). As the
 * monitors attached to a system may have different resolution, the
 * pixel size depends on physical characteristics of the output device
 * as well as user preferences. Therefore, a #GdkScreen and monitor
 * number will need to be passed in.
 *
 * The #GtkSize type is backwards compatible with #gint and can be
 * used as a drop-in replacement assuming the range of values passed
 * in doesn't exceed #GTK_SIZE_MAXPIXEL (roughly a quarter of a
 * billion pixels). Also, #GtkSize is future proof insofar that it can
 * be extended in the future to store other kinds of sizes than em and
 * mm.
 *
 * Note that integer arithmetrics cannot be used on #GtkSize, instead
 * convert all arguments to pixel sizes
 * <programlisting>
 * GtkSize border_width = gtk_size_em (0.5);
 * GtkSize button_width = gtk_size_em (10);
 * gint num_buttons = 5;
 * gint total_width;
 *
 * // illegal
 * total_width = gtk_size_to_pixel (2 * border_width + num_buttons * button_width);
 *
 * // legal
 * total_width = 2 * gtk_size_to_pixel (screen, monitor_num, border_width) +
 *               num_buttons * gtk_size_to_pixel (screen, monitor_num, button_width);
 *
 * // legal
 * total_width = round (2.0 * gtk_size_to_pixel_double (screen, monitor_num, border_width) +
 *                      num_buttons * gtk_size_to_pixel_double (screen, monitor_num, button_width));
 * </programlisting>
 * The latter form is perfectly legal though seldom used as it's often
 * desirable to align UI elements (except text rendering) on a pixel
 * grid.
 *
 * #GtkUSize is #GtkSize's unsigned companion. The construction and
 * conversion functions for #GtkSize applies to #GtkUSize as well.
 *
 * <refsect2>
 *   <title>Porting Applications</title>
 *   Making an application resolution independent involves more than
 *   just calling gtk_enable_resolution_independence() before
 *   gtk_init() - a rough checklist includes
 *   <itemizedlist>
 *     <listitem>
 *       Make sure your application (and any library being used) is not
 *       accessing structure fields in widgets directly - use accessor
 *       functions instead. This is because, when resolution
 *       independent rendering is enabled, a widget may store the
 *       #GtkSize directly instead of a @gint. Note that some classes
 *       still store the pixel values (e.g. #GtkContainer's
 *       <literal>border_width</literal>) - see the header files for
 *       details; if the widget stores a #GtkSize instead of a #gint
 *       the header file will reflect that. However, never rely on
 *       this (it may change in the future), use accessor functions
 *       instead.
 *    </listitem>
 *    <listitem>
 *       Note that all accessor functions (such as
 *       gtk_box_get_spacing()) will still return pixel values. Use
 *       the companion unit getter (e.g. gtk_box_get_spacing_unit())
 *       instead if you want the #GtkSize. Similary for properties
 *       resp. style properties, g_object_get()
 *       resp. gtk_widget_style_get() will still return pixel sizes. Use
 *       gtk_widget_get_unit() and gtk_widget_style_get_unit() to
 *       preserve the unit.
 *    </listitem>
 *    <listitem>
 *      The application should never use pixel values, use
 *      gtk_size_em() instead. If the application was designed for a
 *      display with a 10 point font at 96 DPI (as most applications
 *      are), simply use the GTK_SIZE_ONE_TWELFTH_EM() macro passing
 *      in the pixel size. Note that #GtkBuilder now understands
 *      <literal>em</literal> and <literal>mm</literal> meaning that
 *      it's valid to use e.g. <literal>20.5em</literal> or
 *      <literal>35.2mm</literal> instead of pixel sizes.
 *    </listitem>
 *    <listitem>
 *       Keep in mind that the factors used for translating from
 *       #GtkSize to pixels may change at run-time. For example, the
 *       user may move a window onto a monitor with a different
 *       DPI. Subclasses of #GtkWidget can hook into the
 *       #GtkWidget::unit-changed signal (remember to chain up if
 *       setting the class function) to recompute pixel values or
 *       update #GdkPixbuf objects. Dialogs or windows can also
 *       subscribe to this signal to do the same. See
 *       <literal>assistant.c</literal> in gtk-demo for an example.
 *    </listitem>
 *    <listitem>
 *      Test your application. Try changing the DPI and font sizes on
 *      the fly. Use a multi monitor setup where the monitors have
 *      different DPI's. Ensure that your application works as
 *      intended.
 *    </listitem>
 *   </itemizedlist>
 * </refsect2>
 * <refsect2>
 *   <title>Porting Theme Engines</title>
 *   Theme engines overriding gtk_style_render_icon() need to take the
 *   monitor into account when determining the pixel size for the
 *   given #GtkIconSize. Use
 *   gtk_icon_size_lookup_for_settings_for_monitor() instead of
 *   gtk_icon_size_lookup_for_settings() with
 *   <literal>monitor_num</literal> obtained via
 *   gtk_widget_get_monitor_num(). TODO: need input from theme engine
 *   authors here.
 * </refsect2>
 * <refsect2>
 *   <title>Porting Themes</title>
 *   To specify ems use the exclamation mark as a prefix,
 *   e.g. <literal>!0.5</literal> and <literal>!2</literal> means
 *   resp. 0.5 and 2 ems.
 * </refsect2>
 */

/*
 * Storage format:
 *
 *  10987654321098765432109876543210
 *  SXXX
 *
 * Bits 28, 29 and 30 holds the unit type and is from the #GtkSizeUnit
 * enumeration. Not all of bits 28, 29, 30 can be set at once since it
 * would look like an unsigned integer. This gives 7 different units.
 *
 * Bit 31 holds the sign.
 *
 * Bit 0 through 27 is used for storing the value. This leaves the
 * range [-268,435,456, 268,435,456] to be used for pixel sizes.
 *
 * For em, we store the value times EM_PRECISION and for mm we store
 * the value times MM_PRECISION. This allows people to use non-integer
 * units.
 */

/* the amount of precision we use for storing ems */
#define EM_PRECISION 120

/* the amount of precision we use for storing mm's */
#define MM_PRECISION 100

/* if we can't determine the font, fall back to this size */
#define FALLBACK_FONT_SIZE_POINTS 12

/* if we can't determine the DPI, fall back to this value */
#define FALLBACK_DPI 96

typedef struct _GtkParamSpecSize GtkParamSpecSize;
struct _GtkParamSpecSize
{
  GParamSpecInt parent_instance;
};

typedef struct _GtkParamSpecUSize GtkParamSpecUSize;
struct _GtkParamSpecUSize
{
  GParamSpecUInt parent_instance;
};

static gboolean units_enabled = FALSE;
static gboolean application_wants_to_use_units = FALSE;
static gboolean have_checked = FALSE;

static void
check_if_resolution_independence_is_enabled (void)
{
  const char *s;

  if (G_LIKELY (have_checked))
    return;

  units_enabled = FALSE;

  s = g_getenv ("GTK_RESOLUTION_INDEPENDENCE_FORCE");
  if (s != NULL)
    {
      if (g_ascii_strcasecmp (s, "disabled") == 0 || g_ascii_strcasecmp (s, "0") == 0)
        {
          goto out;
        }

      if (g_ascii_strcasecmp (s, "enabled") == 0 || g_ascii_strcasecmp (s, "1") == 0)
        {
          units_enabled = TRUE;
          goto use_units;
        }
    }

  if (!application_wants_to_use_units)
    goto out;

 use_units:

  units_enabled = TRUE;

 out:
  have_checked = TRUE;
}

static GHashTable *screen_settings_hash = NULL;

typedef struct {
  gdouble resolution;
  gdouble pixels_per_em;
  gdouble pixels_per_mm;
} MonitorSettings;

typedef struct {
  GdkScreen *screen;
  gboolean needs_refresh;

  gint num_monitors;
  MonitorSettings *monitor_settings;
} ScreenSettings;

static void
screen_settings_free (ScreenSettings *screen_settings)
{
  g_free (screen_settings->monitor_settings);
  g_free (screen_settings);
}

static void
screen_settings_update (ScreenSettings *screen_settings, gboolean emit_signal)
{
  int n;
  char *font_name;
  gdouble font_size;
  GtkSettings *settings;
  MonitorSettings *old_monitor_settings;
  gint old_num_monitors;

  /* fall back to font size */
  font_size = FALLBACK_FONT_SIZE_POINTS;

  settings = gtk_settings_get_for_screen (screen_settings->screen);
  if (settings != NULL)
    {
      g_object_get (settings,
                    "gtk-font-name", &font_name,
                    NULL);
      if (font_name != NULL)
        {
          PangoFontDescription *font_desc;
          font_desc = pango_font_description_from_string (font_name);
          if (font_desc != NULL)
            {
              gint pango_size;
              gboolean is_absolute;
              pango_size = pango_font_description_get_size (font_desc);
              is_absolute = pango_font_description_get_size_is_absolute (font_desc);
              if (!is_absolute)
                {
                  font_size = ((gdouble) pango_size) / PANGO_SCALE;
                }
              /* TODO: handle the case where is_absolute is TRUE */
              pango_font_description_free (font_desc);
            }
        }
    }

  old_num_monitors = screen_settings->num_monitors;
  old_monitor_settings = screen_settings->monitor_settings;

  screen_settings->num_monitors = gdk_screen_get_n_monitors (screen_settings->screen);
  screen_settings->monitor_settings = g_new0 (MonitorSettings, screen_settings->num_monitors);
  for (n = 0; n < screen_settings->num_monitors; n++)
    {
      MonitorSettings *monitor_settings = &(screen_settings->monitor_settings[n]);

      monitor_settings->resolution = gdk_screen_get_resolution_for_monitor (screen_settings->screen, n);
      if (monitor_settings->resolution < 0)
        monitor_settings->resolution = FALLBACK_DPI; /* fall back */

      /* 10 points at 96 DPI is 12 pixels; convert accordingly */
      monitor_settings->pixels_per_em = 1.2 * font_size * monitor_settings->resolution / 96.0;

      /* 1 inch ~ 25.4 mm */
      monitor_settings->pixels_per_mm = monitor_settings->resolution / 25.4;

#if 0
      /* debug spew; useful for debugging */
      g_debug ("using 1em -> %g pixels, 1mm -> %g pixels for monitor %d of screen %p (resolution %g DPI, font '%s')\n",
               monitor_settings->pixels_per_em,
               monitor_settings->pixels_per_mm,
               n,
               screen_settings->screen,
               monitor_settings->resolution,
               font_name != NULL ? font_name : "<unknown>");
#endif
    }

  screen_settings->needs_refresh = FALSE;

  /* emit change signals only have the whole ScreenSettings structure have been updated */
  if (emit_signal && old_monitor_settings != NULL && settings != NULL)
    {
      gboolean have_invalidated_icon_caches = FALSE;

      for (n = 0; n < screen_settings->num_monitors && n < old_num_monitors; n++)
        {
          MonitorSettings *monitor_settings = &(screen_settings->monitor_settings[n]);

          if ((fabs (monitor_settings->pixels_per_em - (old_monitor_settings[n]).pixels_per_em) > 0.01) ||
              (fabs (monitor_settings->pixels_per_mm - (old_monitor_settings[n]).pixels_per_mm) > 0.01))
            {
              if (!have_invalidated_icon_caches)
                {
                  have_invalidated_icon_caches = TRUE;
                  _gtk_icon_set_invalidate_caches ();
                }
              g_signal_emit_by_name (settings, "unit-changed", n);
            }
        }
    }

  g_free (old_monitor_settings);

  g_free (font_name);
}

static void
invalidate_screen_settings_for_screen (GdkScreen *screen,
                                        ScreenSettings *screen_settings,
                                        GdkScreen *for_screen)
{
  if (screen == for_screen)
    screen_settings->needs_refresh = TRUE;
}

void
_gtk_size_invalidate_caches_for_screen (GdkScreen *screen)
{
  g_hash_table_foreach (screen_settings_hash,
                        (GHFunc) invalidate_screen_settings_for_screen,
                        screen);
}

static void monitors_changed  (GdkScreen    *screen,
                               gpointer      user_data);

static void dpi_changed       (GtkSettings  *settings,
                               GParamSpec   *spec,
                               gpointer      user_data);

static void font_name_changed (GtkSettings  *settings,
                               GParamSpec   *spec,
                               gpointer      user_data);

static void
screen_settings_screen_went_away (gpointer user_data, GObject *where_the_object_was)
{
  g_hash_table_remove (screen_settings_hash, where_the_object_was);
}

/* this will never return #NULL */
static ScreenSettings *
screen_settings_get (GdkScreen *screen)
{
  GtkSettings *settings;
  ScreenSettings *screen_settings;

  if (G_UNLIKELY (screen_settings_hash == NULL))
    {
      screen_settings_hash = g_hash_table_new_full (g_direct_hash,
                                                   g_direct_equal,
                                                   NULL,
                                                   (GDestroyNotify) screen_settings_free);
    }

  if (screen == NULL)
    screen = gdk_screen_get_default ();

  /* TODO: verify screen is a GdkScreen */

  screen_settings = g_hash_table_lookup (screen_settings_hash, screen);

  if (G_LIKELY (screen_settings != NULL))
    {
      if (G_UNLIKELY (screen_settings->needs_refresh))
        screen_settings_update (screen_settings, FALSE);
      goto out;
    }

  screen_settings = g_new0 (ScreenSettings, 1);
  screen_settings->screen = screen;
  screen_settings_update (screen_settings, FALSE);

  g_hash_table_insert (screen_settings_hash, (gpointer) screen, screen_settings);
  g_object_weak_ref (G_OBJECT (screen), screen_settings_screen_went_away, NULL);

  g_signal_connect (screen,
                    "monitors-changed",
                    G_CALLBACK (monitors_changed),
                    NULL);

  settings = gtk_settings_get_for_screen (screen);
  if (settings != NULL)
    {
      g_signal_connect (settings,
                        "notify::gtk-font-name",
                        G_CALLBACK (font_name_changed),
                        NULL);

      /* right now monitors-changed doesn't fire on dpi changes;
       * hook up to gtk-xft-dpi on GtkSettings for now
       */
      g_signal_connect (settings,
                        "notify::gtk-xft-dpi",
                        G_CALLBACK (dpi_changed),
                        NULL);
    }

 out:
  return screen_settings;
}

static void
monitors_changed (GdkScreen    *screen,
                  gpointer      user_data)
{
  ScreenSettings *screen_settings;

  screen_settings = screen_settings_get (screen);
  if (screen_settings != NULL)
    screen_settings_update (screen_settings, TRUE);
}

static void
dpi_changed (GtkSettings  *settings,
             GParamSpec   *spec,
             gpointer      user_data)
{
  ScreenSettings *screen_settings;

  screen_settings = screen_settings_get (settings->screen);
  if (screen_settings != NULL)
    screen_settings_update (screen_settings, TRUE);
}


static void
font_name_changed (GtkSettings  *settings,
                   GParamSpec   *spec,
                   gpointer      user_data)
{
  ScreenSettings *screen_settings;

  screen_settings = screen_settings_get (settings->screen);
  if (screen_settings != NULL)
    screen_settings_update (screen_settings, TRUE);
}



static void
screen_settings_get_pixel_conversion_factors (GdkScreen *screen,
                                              gint       monitor_num,
                                              gdouble   *pixels_per_em,
                                              gdouble   *pixels_per_mm)
{
  ScreenSettings *s;

  /* -1 means "use default".. that's 0 for now */
  if (monitor_num < 0)
    monitor_num = 0;

  s = screen_settings_get (screen);
  if (monitor_num >= s->num_monitors)
    {
      g_warning ("monitor number %d out of range for screen (screen has %d monitors)",
                 monitor_num, s->num_monitors);
      monitor_num = 0;
    }

  *pixels_per_em = s->monitor_settings[monitor_num].pixels_per_em;
  *pixels_per_mm = s->monitor_settings[monitor_num].pixels_per_mm;
}


/**
 * gtk_enable_resolution_independence:
 *
 * Resolution independent rendering is an opt-in feature; applications
 * need to enable it by default by calling this function before
 * invoking gtk_init().
 *
 * Since: 2.14
 **/
void
gtk_enable_resolution_independence (void)
{
  application_wants_to_use_units = TRUE;
}

/**
 * gtk_size_get_unit:
 * @size: A #GtkSize or #GtkUSize value
 *
 * Gets the unit for @size.
 *
 * Returns: a value from the #GtkSizeUnit enumeration
 **/
GtkSizeUnit
gtk_size_get_unit (GtkSize size)
{
  if (abs (size) <= ((1<<28) - 1))
    return GTK_SIZE_UNIT_PIXEL;

  return (size >> 28) & 0x07;
}

/**
 * gtk_size_em:
 * @em: Size in em
 *
 * This function returns @em as a #GtkSize. If resolution independent
 * rendering is not enabled (see
 * gtk_enable_resolution_independence()), then 12 * @em is
 * returned as a pixel value instead.
 *
 * Returns: a #GtkSize
 *
 * Since: 2.14
 */
GtkSize
gtk_size_em (gdouble em)
{
  check_if_resolution_independence_is_enabled ();

  /* if units are not enabled; assume 1em == 12 pixels */
  if (G_LIKELY (!units_enabled))
    return (gint) (em*12.0 + 0.5);

  if (em >= 0)
    return ((gint) (EM_PRECISION * em)) | (GTK_SIZE_UNIT_EM<<28);
  else
    return ((gint) (EM_PRECISION * (-em))) | (GTK_SIZE_UNIT_EM<<28) | (1<<31);
}

/**
 * gtk_size_mm:
 * @mm: Size in mm
 *
 * This function returns @mm as a #GtkSize. If resolution independent
 * rendering is not enabled (see
 * gtk_enable_resolution_independence()), then 4 * @mm is returned as
 * a pixel value instead.
 *
 * Returns: a #GtkSize
 *
 * Since: 2.14
 **/
GtkSize
gtk_size_mm (gdouble mm)
{
  check_if_resolution_independence_is_enabled ();

  if (G_LIKELY (!units_enabled))
    return (gint) (mm*MM_PRECISION + 0.5);

  /* right now hardcode 1mm == 1 pixel */

  if (mm >= 0)
    return ((gint) (MM_PRECISION * mm)) | (GTK_SIZE_UNIT_MM<<28);
  else
    return ((gint) (MM_PRECISION * (-mm))) | (GTK_SIZE_UNIT_MM<<28) | (1<<31);
}

/**
 * gtk_size_get_em:
 * @size: a #GtkSize of type #GTK_SIZE_UNIT_EM
 *
 * Gets the number of ems stored in @size. Returns -1 if the unit of
 * @size is not @GTK_SIZE_UNIT_EM.
 *
 * Returns: a #gdouble
 *
 * Since: 2.14
 **/
gdouble
gtk_size_get_em (GtkSize size)
{
  g_return_val_if_fail (gtk_size_get_unit (size) == GTK_SIZE_UNIT_EM, -1.0);
  if (size >= 0)
    return (size & ((1<<28)-1)) / ((gdouble) EM_PRECISION);
  else
    return -(size & ((1<<28)-1)) / ((gdouble) EM_PRECISION);
}

/**
 * gtk_size_get_mm:
 * @size: a #GtkSize of type #GTK_SIZE_UNIT_MM
 *
 * Gets the number of millimeters stored in @size. Returns -1 if the
 * unit of @size is not @GTK_SIZE_UNIT_MM.
 *
 * Returns: a #gdouble
 *
 * Since: 2.14
 **/
gdouble
gtk_size_get_mm (GtkSize size)
{
  g_return_val_if_fail (gtk_size_get_unit (size) == GTK_SIZE_UNIT_MM, -1.0);
  if (size >= 0)
    return (size & ((1<<28)-1)) / ((gdouble) MM_PRECISION);
  else
    return -(size & ((1<<28)-1)) / ((gdouble) MM_PRECISION);
}

static void
get_pixel_conversion_factors (GdkScreen *screen,
                              gint monitor_num,
                              gdouble *pixels_per_em,
                              gdouble *pixels_per_mm)
{
  if (screen == NULL || monitor_num == -1)
    {
      GTK_NOTE (MULTIHEAD,
		g_warning ("gtk_size_to_pixel ()) called with screen=%p monitor_num=%d", screen, monitor_num));
    }
  screen_settings_get_pixel_conversion_factors (screen, monitor_num, pixels_per_em, pixels_per_mm);
}

/**
 * gtk_size_to_pixel:
 * @screen: A #GdkScreen or #NULL to use the default #GdkScreen
 * @monitor_num: number of the monitor or -1 to use the default monitor
 * @size: A #GtkSize or #GtkUSize
 *
 * Converts @size to an integer representing the number of pixels
 * taking factors like font size etc. into account.
 *
 * See also gtk_widget_size_to_pixel().
 *
 * Returns: @size converted to pixel value
 *
 * Since: 2.14
 */
gint
gtk_size_to_pixel (GdkScreen *screen,
                   gint       monitor_num,
                   GtkSize    size)
{
  return (gint) (gtk_size_to_pixel_double (screen, monitor_num, size) + 0.5);
}

/**
 * gtk_size_to_pixel_double:
 * @screen: A #GdkScreen or #NULL to use the default #GdkScreen
 * @monitor_num: number of the monitor or -1 to use the default monitor
 * @size: A #GtkSize or #GtkUSize
 *
 * Like gtk_size_to_pixel() but returns the pixel size in a @gdouble.
 *
 * See also gtk_widget_size_to_pixel_double().
 *
 * Returns: @size converted to pixel value
 *
 * Since: 2.14
 */
gdouble
gtk_size_to_pixel_double (GdkScreen *screen,
                          gint       monitor_num,
                          GtkSize    size)
{
  gdouble val;
  gdouble pixels_per_em;
  gdouble pixels_per_mm;

  check_if_resolution_independence_is_enabled ();

  if (G_LIKELY (!units_enabled))
    return size;

  get_pixel_conversion_factors (screen, monitor_num, &pixels_per_em, &pixels_per_mm);

  switch (gtk_size_get_unit (size))
    {
    case GTK_SIZE_UNIT_PIXEL:
      val = size;
      //g_debug ("converted 0x%08x (%d pixels) to %g pixels", size, size, val);
      break;

    case GTK_SIZE_UNIT_EM:
      val = gtk_size_get_em (size) * pixels_per_em;
      //g_debug ("converted 0x%08x (%g em) to %g pixels", size, gtk_size_get_em (size), val);
      break;

    case GTK_SIZE_UNIT_MM:
      val = gtk_size_get_mm (size) * pixels_per_mm;
      //g_debug ("converted 0x%08x (%g mm) to %g pixels", size, gtk_size_get_mm (size), val);
      break;

    default:
      g_warning ("gtk_size_to_pixel_double(): unknown unit for size 0x%08x", size);
      val = -1.0;
      break;
    }

  return val;
}

/* -------------------------------------------------------------------------------- */

/**
 * gtk_size_to_string:
 * @size: A #GtkSize or #GtkUSize
 *
 * Gets a human readable textual representation of @size such as "2
 * px" or "0.5 em". The caller cannot rely on the string being machine
 * readable; the format may change in a future release.
 *
 * Returns: the textual representation of @size - free with g_free().
 *
 * Since: 2.14
 **/
gchar *
gtk_size_to_string (GtkSize size)
{
  gchar *s;

  switch (gtk_size_get_unit (size))
    {
    case GTK_SIZE_UNIT_PIXEL:
      if (size == GTK_SIZE_MAXPIXEL)
        s = g_strdup ("GTK_SIZE_MAXPIXEL");
      else if (size == -GTK_SIZE_MAXPIXEL)
        s = g_strdup ("-GTK_SIZE_MAXPIXEL");
      else
        s = g_strdup_printf ("%d px", size);
      break;

    case GTK_SIZE_UNIT_EM:
      s = g_strdup_printf ("%g em", gtk_size_get_em (size));
      break;

    case GTK_SIZE_UNIT_MM:
      s = g_strdup_printf ("%g mm", gtk_size_get_mm (size));
      break;

    default:
      s = g_strdup_printf ("unknown unit for size 0x%08x", size);
      break;
    }

  return s;
}

/* -------------------------------------------------------------------------------- */

static gboolean
gtk_param_size_validate (GParamSpec *pspec,
                         GValue     *value)
{
  GParamSpecInt *ispec = G_PARAM_SPEC_INT (pspec);
  gint oldval;

  if (gtk_size_get_unit (value->data[0].v_int) != GTK_SIZE_UNIT_PIXEL)
    return FALSE;

  oldval = value->data[0].v_int;
  value->data[0].v_int = CLAMP (value->data[0].v_int, ispec->minimum, ispec->maximum);

  return value->data[0].v_int != oldval;
}

static void
gtk_param_size_class_init (GParamSpecClass *class)
{
  class->value_type = G_TYPE_INT;
  class->value_validate = gtk_param_size_validate;
}

GType
gtk_param_size_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo type_info =
      {
        sizeof (GParamSpecClass),
        NULL,
        NULL,
        (GClassInitFunc) gtk_param_size_class_init,
        NULL,
        NULL,
        sizeof (GtkParamSpecSize),
        0,
        NULL,
        NULL
      };

      type = g_type_register_static (G_TYPE_PARAM_INT,
                                     "GtkParamSize",
                                     &type_info,
                                     0);
    }

  return type;
}

/**
 * gtk_param_spec_size:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpec instance specifying a #GtkSize property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: a newly created parameter specification
 *
 * Since: 2.14
 **/
GParamSpec *
gtk_param_spec_size (const gchar *name,
                     const gchar *nick,
                     const gchar *blurb,
                     GtkSize      minimum,
                     GtkSize      maximum,
                     GtkSize      default_value,
                     GParamFlags  flags)
{
  GtkParamSpecSize *pspec;
  GParamSpecInt *ispec;
  pspec = g_param_spec_internal (GTK_TYPE_PARAM_SIZE,
                                 name, nick, blurb, flags);
  ispec = G_PARAM_SPEC_INT (pspec);
  ispec->default_value = default_value;
  ispec->minimum = minimum;
  ispec->maximum = maximum;
  return G_PARAM_SPEC (pspec);
}

/**
 * gtk_value_set_size:
 * @value: a valid #GValue of type #G_TYPE_INT
 * @v_size: size value to be set
 * @widget: a #GtkWidget used for converting to pixel sizes or #NULL
 *
 * Sets the content of a #GValue of type #G_TYPE_INT to @v_size.
 *
 * Unless gtk_value_size_skip_conversion() have been called on
 * @v_size, the contents will be converted to pixel values using
 * gtk_widget_size_to_pixel() on @widget.
 *
 * Since: 2.14
 **/
void
gtk_value_set_size (GValue    *value,
                    GtkSize    v_size,
                    gpointer   widget)
{
  g_return_if_fail (G_VALUE_HOLDS_INT (value));
  if (value->data[1].v_int == 1)
    value->data[0].v_int = v_size;
  else
    value->data[0].v_int = gtk_widget_size_to_pixel (widget, v_size);
}

/**
 * gtk_value_get_size:
 * @value: a valid #GValue of type #G_TYPE_INT
 *
 * Get the contents of a #GValue of type #G_TYPE_INT.
 *
 * Returns: a #GtkSize
 *
 * Since: 2.14
 **/
GtkSize
gtk_value_get_size (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_INT (value), 0);
  return value->data[0].v_int;
}

/**
 * gtk_value_size_skip_conversion:
 * @value: a valid #GValue of type #G_TYPE_INT
 *
 * Specify that conversion to pixel values should be skipped in
 * gtk_value_set_size().
 *
 * Since: 2.14
 **/
void
gtk_value_size_skip_conversion (GValue *value)
{
  g_return_if_fail (G_VALUE_HOLDS_INT (value));
  value->data[1].v_int = 1;
}

/* -------------------------------------------------------------------------------- */

static gboolean
gtk_param_usize_validate (GParamSpec *pspec,
                          GValue     *value)
{
  GParamSpecUInt *uspec = G_PARAM_SPEC_UINT (pspec);
  gint oldval;

  if (gtk_size_get_unit (value->data[0].v_int) != GTK_SIZE_UNIT_PIXEL)
    return FALSE;

  oldval = value->data[0].v_int;
  value->data[0].v_int = CLAMP (value->data[0].v_int, uspec->minimum, uspec->maximum);

  return value->data[0].v_int != oldval;
}

static void
gtk_param_usize_class_init (GParamSpecClass *class)
{
  class->value_type = G_TYPE_UINT;
  class->value_validate = gtk_param_usize_validate;
}

GType
gtk_param_usize_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo type_info =
      {
        sizeof (GParamSpecClass),
        NULL,
        NULL,
        (GClassInitFunc) gtk_param_usize_class_init,
        NULL,
        NULL,
        sizeof (GtkParamSpecUSize),
        0,
        NULL,
        NULL
      };

      type = g_type_register_static (G_TYPE_PARAM_UINT,
                                     "GtkParamUSize",
                                     &type_info,
                                     0);
    }

  return type;
}

/**
 * gtk_param_spec_usize:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpec instance specifying a #GtkUSize property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: a newly created parameter specification
 *
 * Since: 2.14
 **/
GParamSpec *
gtk_param_spec_usize (const gchar *name,
                      const gchar *nick,
                      const gchar *blurb,
                      GtkUSize     minimum,
                      GtkUSize     maximum,
                      GtkUSize     default_value,
                      GParamFlags  flags)
{
  GtkParamSpecUSize *pspec;
  GParamSpecUInt *uspec;
  pspec = g_param_spec_internal (GTK_TYPE_PARAM_USIZE,
                                 name, nick, blurb, flags);
  uspec = G_PARAM_SPEC_UINT (pspec);
  uspec->default_value = default_value;
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  return G_PARAM_SPEC (pspec);
}

/**
 * gtk_value_set_usize:
 * @value: a valid #GValue of type #G_TYPE_UINT
 * @v_size: size value to be set
 * @widget: a #GtkWidget used for converting to pixel sizes or #NULL
 *
 * Sets the content of a #GValue of type #G_TYPE_UINT to @v_size.
 *
 * Unless gtk_value_usize_skip_conversion() have been called on
 * @v_size, the contents will be converted to pixel values using
 * gtk_widget_size_to_pixel() on @widget.
 *
 * Since: 2.14
 **/
void
gtk_value_set_usize (GValue    *value,
                     GtkUSize   v_size,
                     gpointer   widget)
{
  g_return_if_fail (G_VALUE_HOLDS_UINT (value));
  if (value->data[1].v_int == 1)
    value->data[0].v_uint = v_size;
  else
    value->data[0].v_uint = gtk_widget_size_to_pixel (widget, v_size);
}

/**
 * gtk_value_get_usize:
 * @value: a valid #GValue of type #G_TYPE_UINT
 *
 * Get the contents of a #GValue of type #G_TYPE_UINT.
 *
 * Returns: a #GtkUSize
 *
 * Since: 2.14
 **/
GtkUSize
gtk_value_get_usize (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UINT (value), 0);
  return value->data[0].v_uint;
}

/**
 * gtk_value_usize_skip_conversion:
 * @value: a valid #GValue of type #G_TYPE_UINT
 *
 * Specify that conversion to pixel values should be skipped in
 * gtk_value_set_usize().
 *
 * Since: 2.14
 **/
void
gtk_value_usize_skip_conversion (GValue *value)
{
  g_return_if_fail (G_VALUE_HOLDS_UINT (value));
  value->data[1].v_int = 1;
}

/* -------------------------------------------------------------------------------- */

#define __GTK_SIZE_C__
#include "gtkaliasdef.c"
