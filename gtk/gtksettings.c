/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */


#include "config.h"

#include "gtksettingsprivate.h"

#include "gtkcssstylesheetprivate.h"
#include "gtkhslaprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkversion.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

#include "gdk/gdk-private.h"

#include <string.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include <pango/pangofc-fontmap.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include <pango/pangofc-fontmap.h>
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "quartz/gdkquartz.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#define PRINT_PREVIEW_COMMAND "open -b com.apple.Preview %f"
#else
#define PRINT_PREVIEW_COMMAND "evince --unlink-tempfile --preview --print-settings %s %f"
#endif

/**
 * SECTION:gtksettings
 * @Short_description: Sharing settings between applications
 * @Title: Settings
 *
 * GtkSettings provide a mechanism to share global settings between
 * applications.
 *
 * On the X window system, this sharing is realized by an
 * [XSettings](http://www.freedesktop.org/wiki/Specifications/xsettings-spec)
 * manager that is usually part of the desktop environment, along with
 * utilities that let the user change these settings. In the absence of
 * an Xsettings manager, GTK reads default values for settings from
 * `settings.ini` files in
 * `/etc/gtk-4.0`, `$XDG_CONFIG_DIRS/gtk-4.0`
 * and `$XDG_CONFIG_HOME/gtk-4.0`.
 * These files must be valid key files (see #GKeyFile), and have
 * a section called Settings. Themes can also provide default values
 * for settings by installing a `settings.ini` file
 * next to their `gtk.css` file.
 *
 * Applications can override system-wide settings by setting the property
 * of the GtkSettings object with g_object_set(). This should be restricted
 * to special cases though; GtkSettings are not meant as an application
 * configuration facility.
 *
 * There is one GtkSettings instance per display. It can be obtained with
 * gtk_settings_get_for_display(), but in many cases, it is more convenient
 * to use gtk_widget_get_settings(). gtk_settings_get_default() returns the
 * GtkSettings instance for the default display.
 */


#define DEFAULT_TIMEOUT_INITIAL 500
#define DEFAULT_TIMEOUT_REPEAT   50
#define DEFAULT_TIMEOUT_EXPAND  500

typedef struct _GtkSettingsClass GtkSettingsClass;

struct _GtkSettingsClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

typedef struct _GtkSettingsPropertyValue GtkSettingsPropertyValue;
typedef struct _GtkSettingsValuePrivate GtkSettingsValuePrivate;

typedef struct _GtkSettingsPrivate GtkSettingsPrivate;

struct _GtkSettingsPrivate
{
  GData *queued_settings;      /* of type GtkSettingsValue* */
  GtkSettingsPropertyValue *property_values;
  GdkDisplay *display;
  GtkStyleCascade *style_cascade;
  GtkCssStyleSheet *theme_style_sheet;
  gint font_size;
  gboolean font_size_absolute;
  gchar *font_family;
  cairo_font_options_t *font_options;
};

struct _GtkSettingsValuePrivate
{
  GtkSettingsValue public;
  GtkSettingsSource source;
};

struct _GtkSettingsPropertyValue
{
  GValue value;
  GtkSettingsSource source;
};

enum {
  PROP_0,
  PROP_DOUBLE_CLICK_TIME,
  PROP_DOUBLE_CLICK_DISTANCE,
  PROP_CURSOR_BLINK,
  PROP_CURSOR_BLINK_TIME,
  PROP_CURSOR_BLINK_TIMEOUT,
  PROP_SPLIT_CURSOR,
  PROP_THEME_NAME,
  PROP_ICON_THEME_NAME,
  PROP_DND_DRAG_THRESHOLD,
  PROP_FONT_NAME,
  PROP_XFT_ANTIALIAS,
  PROP_XFT_HINTING,
  PROP_XFT_HINTSTYLE,
  PROP_XFT_RGBA,
  PROP_XFT_DPI,
  PROP_CURSOR_THEME_NAME,
  PROP_CURSOR_THEME_SIZE,
  PROP_ALTERNATIVE_BUTTON_ORDER,
  PROP_ALTERNATIVE_SORT_ARROWS,
  PROP_ENABLE_ANIMATIONS,
  PROP_ERROR_BELL,
  PROP_PRINT_BACKENDS,
  PROP_PRINT_PREVIEW_COMMAND,
  PROP_ENABLE_ACCELS,
  PROP_IM_MODULE,
  PROP_RECENT_FILES_MAX_AGE,
  PROP_FONTCONFIG_TIMESTAMP,
  PROP_SOUND_THEME_NAME,
  PROP_ENABLE_INPUT_FEEDBACK_SOUNDS,
  PROP_ENABLE_EVENT_SOUNDS,
  PROP_PRIMARY_BUTTON_WARPS_SLIDER,
  PROP_APPLICATION_PREFER_DARK_THEME,
  PROP_ENTRY_SELECT_ON_FOCUS,
  PROP_ENTRY_PASSWORD_HINT_TIMEOUT,
  PROP_LABEL_SELECT_ON_FOCUS,
  PROP_SHELL_SHOWS_APP_MENU,
  PROP_SHELL_SHOWS_MENUBAR,
  PROP_SHELL_SHOWS_DESKTOP,
  PROP_DECORATION_LAYOUT,
  PROP_TITLEBAR_DOUBLE_CLICK,
  PROP_TITLEBAR_MIDDLE_CLICK,
  PROP_TITLEBAR_RIGHT_CLICK,
  PROP_DIALOGS_USE_HEADER,
  PROP_ENABLE_PRIMARY_PASTE,
  PROP_RECENT_FILES_ENABLED,
  PROP_LONG_PRESS_TIME,
  PROP_KEYNAV_USE_CARET,
  PROP_OVERLAY_SCROLLING
};

/* --- prototypes --- */
static void     gtk_settings_finalize            (GObject               *object);
static void     gtk_settings_get_property        (GObject               *object,
                                                  guint                  property_id,
                                                  GValue                *value,
                                                  GParamSpec            *pspec);
static void     gtk_settings_set_property        (GObject               *object,
                                                  guint                  property_id,
                                                  const GValue          *value,
                                                  GParamSpec            *pspec);
static void     gtk_settings_notify              (GObject               *object,
                                                  GParamSpec            *pspec);
static guint    settings_install_property_parser (GtkSettingsClass      *class,
                                                  GParamSpec            *pspec);
static void    settings_update_double_click      (GtkSettings           *settings);

static void    settings_update_cursor_theme      (GtkSettings           *settings);
static void    settings_update_font_options      (GtkSettings           *settings);
static void    settings_update_font_values       (GtkSettings           *settings);
static gboolean settings_update_fontconfig       (GtkSettings           *settings);
static void    settings_update_theme             (GtkSettings           *settings);
static gboolean settings_update_xsetting         (GtkSettings           *settings,
                                                  GParamSpec            *pspec,
                                                  gboolean               force);
static void    settings_update_xsettings         (GtkSettings           *settings);

static void gtk_settings_load_from_key_file      (GtkSettings           *settings,
                                                  const gchar           *path,
                                                  GtkSettingsSource      source);

/* --- variables --- */
static GQuark            quark_gtk_settings = 0;
static GSList           *object_list = NULL;
static guint             class_n_properties = 0;

static GPtrArray *display_settings;


G_DEFINE_TYPE_WITH_PRIVATE (GtkSettings, gtk_settings, G_TYPE_OBJECT);

/* --- functions --- */
static void
gtk_settings_init (GtkSettings *settings)
{
  GtkSettingsPrivate *priv;
  GParamSpec **pspecs, **p;
  guint i = 0;
  gchar *path;
  const gchar * const *config_dirs;

  priv = gtk_settings_get_instance_private (settings);

  g_datalist_init (&priv->queued_settings);
  object_list = g_slist_prepend (object_list, settings);

  priv->style_cascade = _gtk_style_cascade_new ();
  priv->theme_style_sheet = gtk_css_style_sheet_new ();
  gtk_css_style_sheet_set_priority (priv->theme_style_sheet, GTK_STYLE_PROVIDER_PRIORITY_THEME);

  /* build up property array for all yet existing properties and queue
   * notification for them (at least notification for internal properties
   * will instantly be caught)
   */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), NULL);
  for (p = pspecs; *p; p++)
    if ((*p)->owner_type == G_OBJECT_TYPE (settings))
      i++;
  priv->property_values = g_new0 (GtkSettingsPropertyValue, i);
  i = 0;
  g_object_freeze_notify (G_OBJECT (settings));

  for (p = pspecs; *p; p++)
    {
      GParamSpec *pspec = *p;
      GType value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);

      if (pspec->owner_type != G_OBJECT_TYPE (settings))
        continue;
      g_value_init (&priv->property_values[i].value, value_type);
      g_param_value_set_default (pspec, &priv->property_values[i].value);

      g_object_notify_by_pspec (G_OBJECT (settings), pspec);
      priv->property_values[i].source = GTK_SETTINGS_SOURCE_DEFAULT;
      i++;
    }
  g_free (pspecs);

  path = g_build_filename (_gtk_get_data_prefix (), "share", "gtk-4.0", "settings.ini", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  path = g_build_filename (_gtk_get_sysconfdir (), "gtk-4.0", "settings.ini", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  config_dirs = g_get_system_config_dirs ();
  for (i = 0; config_dirs[i] != NULL; i++)
    {
      path = g_build_filename (config_dirs[i], "gtk-4.0", "settings.ini", NULL);
      if (g_file_test (path, G_FILE_TEST_EXISTS))
        gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
      g_free (path);
    }

  path = g_build_filename (g_get_user_config_dir (), "gtk-4.0", "settings.ini", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  g_object_thaw_notify (G_OBJECT (settings));

  /* ensure that derived fields are initialized */
  if (priv->font_size == 0)
    settings_update_font_values (settings);
}

static void
gtk_settings_class_init (GtkSettingsClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  guint result;

  gobject_class->finalize = gtk_settings_finalize;
  gobject_class->get_property = gtk_settings_get_property;
  gobject_class->set_property = gtk_settings_set_property;
  gobject_class->notify = gtk_settings_notify;

  quark_gtk_settings = g_quark_from_static_string ("gtk-settings");

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-time",
                                                               P_("Double Click Time"),
                                                               P_("Maximum time allowed between two clicks for them to be considered a double click (in milliseconds)"),
                                                               0, G_MAXINT, 400,
                                                               GTK_PARAM_READWRITE));
  g_assert (result == PROP_DOUBLE_CLICK_TIME);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-distance",
                                                               P_("Double Click Distance"),
                                                               P_("Maximum distance allowed between two clicks for them to be considered a double click (in pixels)"),
                                                               0, G_MAXINT, 5,
                                                               GTK_PARAM_READWRITE));
  g_assert (result == PROP_DOUBLE_CLICK_DISTANCE);

  /**
   * GtkSettings:gtk-cursor-blink:
   *
   * Whether the cursor should blink.
   *
   * Also see the #GtkSettings:gtk-cursor-blink-timeout setting,
   * which allows more flexible control over cursor blinking.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-cursor-blink",
                                                                   P_("Cursor Blink"),
                                                                   P_("Whether the cursor should blink"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_CURSOR_BLINK);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-blink-time",
                                                               P_("Cursor Blink Time"),
                                                               P_("Length of the cursor blink cycle, in milliseconds"),
                                                               100, G_MAXINT, 1200,
                                                               GTK_PARAM_READWRITE));
  g_assert (result == PROP_CURSOR_BLINK_TIME);

  /**
   * GtkSettings:gtk-cursor-blink-timeout:
   *
   * Time after which the cursor stops blinking, in seconds.
   * The timer is reset after each user interaction.
   *
   * Setting this to zero has the same effect as setting
   * #GtkSettings:gtk-cursor-blink to %FALSE.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-blink-timeout",
                                                               P_("Cursor Blink Timeout"),
                                                               P_("Time after which the cursor stops blinking, in seconds"),
                                                               1, G_MAXINT, 10,
                                                               GTK_PARAM_READWRITE));
  g_assert (result == PROP_CURSOR_BLINK_TIMEOUT);
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-split-cursor",
                                                                   P_("Split Cursor"),
                                                                   P_("Whether two cursors should be displayed for mixed left-to-right and right-to-left text"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_SPLIT_CURSOR);
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-theme-name",
                                                                   P_("Theme Name"),
                                                                   P_("Name of theme to load"),
                                                                  DEFAULT_THEME_NAME,
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_THEME_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-icon-theme-name",
                                                                  P_("Icon Theme Name"),
                                                                  P_("Name of icon theme to use"),
                                                                  DEFAULT_ICON_THEME,
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_ICON_THEME_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-dnd-drag-threshold",
                                                               P_("Drag threshold"),
                                                               P_("Number of pixels the cursor can move before dragging"),
                                                               1, G_MAXINT, 8,
                                                               GTK_PARAM_READWRITE));
  g_assert (result == PROP_DND_DRAG_THRESHOLD);

  /**
   * GtkSettings:gtk-font-name:
   *
   * The default font to use. GTK uses the family name and size from this string.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-font-name",
                                                                   P_("Font Name"),
                                                                   P_("The default font family and size to use"),
                                                                  "Sans 10",
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_FONT_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-xft-antialias",
                                                               P_("Xft Antialias"),
                                                               P_("Whether to antialias Xft fonts; 0=no, 1=yes, -1=default"),
                                                               -1, 1, -1,
                                                               GTK_PARAM_READWRITE));

  g_assert (result == PROP_XFT_ANTIALIAS);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-xft-hinting",
                                                               P_("Xft Hinting"),
                                                               P_("Whether to hint Xft fonts; 0=no, 1=yes, -1=default"),
                                                               -1, 1, -1,
                                                               GTK_PARAM_READWRITE));

  g_assert (result == PROP_XFT_HINTING);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-xft-hintstyle",
                                                                  P_("Xft Hint Style"),
                                                                  P_("What degree of hinting to use; hintnone, hintslight, hintmedium, or hintfull"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE));

  g_assert (result == PROP_XFT_HINTSTYLE);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-xft-rgba",
                                                                  P_("Xft RGBA"),
                                                                  P_("Type of subpixel antialiasing; none, rgb, bgr, vrgb, vbgr"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE));

  g_assert (result == PROP_XFT_RGBA);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-xft-dpi",
                                                               P_("Xft DPI"),
                                                               P_("Resolution for Xft, in 1024 * dots/inch. -1 to use default value"),
                                                               -1, 1024*1024, -1,
                                                               GTK_PARAM_READWRITE));

  g_assert (result == PROP_XFT_DPI);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-cursor-theme-name",
                                                                  P_("Cursor theme name"),
                                                                  P_("Name of the cursor theme to use, or NULL to use the default theme"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_CURSOR_THEME_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-theme-size",
                                                               P_("Cursor theme size"),
                                                               P_("Size to use for cursors, or 0 to use the default size"),
                                                               0, 128, 0,
                                                               GTK_PARAM_READWRITE));

  g_assert (result == PROP_CURSOR_THEME_SIZE);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-alternative-button-order",
                                                                   P_("Alternative button order"),
                                                                   P_("Whether buttons in dialogs should use the alternative button order"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ALTERNATIVE_BUTTON_ORDER);

  /**
   * GtkSettings:gtk-alternative-sort-arrows:
   *
   * Controls the direction of the sort indicators in sorted list and tree
   * views. By default an arrow pointing down means the column is sorted
   * in ascending order. When set to %TRUE, this order will be inverted.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-alternative-sort-arrows",
                                                                   P_("Alternative sort indicator direction"),
                                                                   P_("Whether the direction of the sort indicators in list and tree views is inverted compared to the default (where down means ascending)"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ALTERNATIVE_SORT_ARROWS);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-animations",
                                                                   P_("Enable Animations"),
                                                                   P_("Whether to enable toolkit-wide animations."),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));

  g_assert (result == PROP_ENABLE_ANIMATIONS);

  /**
   * GtkSettings:gtk-error-bell:
   *
   * When %TRUE, keyboard navigation and other input-related errors
   * will cause a beep. Since the error bell is implemented using
   * gdk_surface_beep(), the windowing system may offer ways to
   * configure the error bell in many ways, such as flashing the
   * window or similar visual effects.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-error-bell",
                                                                   P_("Error Bell"),
                                                                   P_("When TRUE, keyboard navigation and other errors will cause a beep"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));

  g_assert (result == PROP_ERROR_BELL);

  /**
   * GtkSettings:gtk-print-backends:
   *
   * A comma-separated list of print backends to use in the print
   * dialog. Available print backends depend on the GTK installation,
   * and may include "file", "cups", "lpr" or "papi".
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-print-backends",
                                                                  P_("Default print backend"),
                                                                  P_("List of the GtkPrintBackend backends to use by default"),
                                                                  GTK_PRINT_BACKENDS,
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_PRINT_BACKENDS);

  /**
   * GtkSettings:gtk-print-preview-command:
   *
   * A command to run for displaying the print preview. The command
   * should contain a `%f` placeholder, which will get replaced by
   * the path to the pdf file. The command may also contain a `%s`
   * placeholder, which will get replaced by the path to a file
   * containing the print settings in the format produced by
   * gtk_print_settings_to_file().
   *
   * The preview application is responsible for removing the pdf file
   * and the print settings file when it is done.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-print-preview-command",
                                                                  P_("Default command to run when displaying a print preview"),
                                                                  P_("Command to run when displaying a print preview"),
                                                                  PRINT_PREVIEW_COMMAND,
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_PRINT_PREVIEW_COMMAND);

  /**
   * GtkSettings:gtk-enable-accels:
   *
   * Whether menu items should have visible accelerators which can be
   * activated.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-accels",
                                                                   P_("Enable Accelerators"),
                                                                   P_("Whether menu items should have accelerators"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ENABLE_ACCELS);

  /**
   * GtkSettings:gtk-im-module:
   *
   * Which IM (input method) module should be used by default. This is the
   * input method that will be used if the user has not explicitly chosen
   * another input method from the IM context menu.
   * This also can be a colon-separated list of input methods, which GTK
   * will try in turn until it finds one available on the system.
   *
   * See #GtkIMContext.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-im-module",
                                                                  P_("Default IM module"),
                                                                  P_("Which IM module should be used by default"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_IM_MODULE);

  /**
   * GtkSettings:gtk-recent-files-max-age:
   *
   * The maximum age, in days, of the items inside the recently used
   * resources list. Items older than this setting will be excised
   * from the list. If set to 0, the list will always be empty; if
   * set to -1, no item will be removed.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-recent-files-max-age",
                                                               P_("Recent Files Max Age"),
                                                               P_("Maximum age of recently used files, in days"),
                                                               -1, G_MAXINT,
                                                               30,
                                                               GTK_PARAM_READWRITE));
  g_assert (result == PROP_RECENT_FILES_MAX_AGE);

  result = settings_install_property_parser (class,
                                             g_param_spec_uint ("gtk-fontconfig-timestamp",
                                                                P_("Fontconfig configuration timestamp"),
                                                                P_("Timestamp of current fontconfig configuration"),
                                                                0, G_MAXUINT, 0,
                                                                GTK_PARAM_READWRITE));

  g_assert (result == PROP_FONTCONFIG_TIMESTAMP);

  /**
   * GtkSettings:gtk-sound-theme-name:
   *
   * The XDG sound theme to use for event sounds.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-sound-theme-name",
                                                                  P_("Sound Theme Name"),
                                                                  P_("XDG sound theme name"),
                                                                  "freedesktop",
                                                                  GTK_PARAM_READWRITE));
  g_assert (result == PROP_SOUND_THEME_NAME);

  /**
   * GtkSettings:gtk-enable-input-feedback-sounds:
   *
   * Whether to play event sounds as feedback to user input.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-input-feedback-sounds",
                                                                   /* Translators: this means sounds that are played as feedback to user input */
                                                                   P_("Audible Input Feedback"),
                                                                   P_("Whether to play event sounds as feedback to user input"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ENABLE_INPUT_FEEDBACK_SOUNDS);

  /**
   * GtkSettings:gtk-enable-event-sounds:
   *
   * Whether to play any event sounds at all.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-event-sounds",
                                                                   P_("Enable Event Sounds"),
                                                                   P_("Whether to play any event sounds at all"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ENABLE_EVENT_SOUNDS);

  /**
   * GtkSettings:gtk-primary-button-warps-slider:
   *
   * If the value of this setting is %TRUE, clicking the primary button in a
   * #GtkRange trough will move the slider, and hence set the range’s value, to
   * the point that you clicked. If it is %FALSE, a primary click will cause the
   * slider/value to move by the range’s page-size towards the point clicked.
   *
   * Whichever action you choose for the primary button, the other action will
   * be available by holding Shift and primary-clicking, or (since GTK 3.22.25)
   * clicking the middle mouse button.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-primary-button-warps-slider",
                                                                   P_("Primary button warps slider"),
                                                                   P_("Whether a primary click on the trough should warp the slider into position"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_PRIMARY_BUTTON_WARPS_SLIDER);

  /**
   * GtkSettings:gtk-application-prefer-dark-theme:
   *
   * Whether the application prefers to use a dark theme. If a GTK theme
   * includes a dark variant, it will be used instead of the configured
   * theme.
   *
   * Some applications benefit from minimizing the amount of light pollution that
   * interferes with the content. Good candidates for dark themes are photo and
   * video editors that make the actual content get all the attention and minimize
   * the distraction of the chrome.
   *
   * Dark themes should not be used for documents, where large spaces are white/light
   * and the dark chrome creates too much contrast (web browser, text editor...).
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-application-prefer-dark-theme",
                                                                 P_("Application prefers a dark theme"),
                                                                 P_("Whether the application prefers to have a dark theme."),
                                                                 FALSE,
                                                                 GTK_PARAM_READWRITE));
  g_assert (result == PROP_APPLICATION_PREFER_DARK_THEME);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-entry-select-on-focus",
                                                                   P_("Select on focus"),
                                                                   P_("Whether to select the contents of an entry when it is focused"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ENTRY_SELECT_ON_FOCUS);

  /**
   * GtkSettings:gtk-entry-password-hint-timeout:
   *
   * How long to show the last input character in hidden
   * entries. This value is in milliseconds. 0 disables showing the
   * last char. 600 is a good value for enabling it.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_uint ("gtk-entry-password-hint-timeout",
                                                                P_("Password Hint Timeout"),
                                                                P_("How long to show the last input character in hidden entries"),
                                                                0, G_MAXUINT,
                                                                0,
                                                                GTK_PARAM_READWRITE));
  g_assert (result == PROP_ENTRY_PASSWORD_HINT_TIMEOUT);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-label-select-on-focus",
                                                                   P_("Select on focus"),
                                                                   P_("Whether to select the contents of a selectable label when it is focused"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_LABEL_SELECT_ON_FOCUS);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-shell-shows-app-menu",
                                                                   P_("Desktop shell shows app menu"),
                                                                   P_("Set to TRUE if the desktop environment "
                                                                      "is displaying the app menu, FALSE if "
                                                                      "the app should display it itself."),
                                                                   FALSE, GTK_PARAM_READWRITE));
  g_assert (result == PROP_SHELL_SHOWS_APP_MENU);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-shell-shows-menubar",
                                                                   P_("Desktop shell shows the menubar"),
                                                                   P_("Set to TRUE if the desktop environment "
                                                                      "is displaying the menubar, FALSE if "
                                                                      "the app should display it itself."),
                                                                   FALSE, GTK_PARAM_READWRITE));
  g_assert (result == PROP_SHELL_SHOWS_MENUBAR);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-shell-shows-desktop",
                                                                   P_("Desktop environment shows the desktop folder"),
                                                                   P_("Set to TRUE if the desktop environment "
                                                                      "is displaying the desktop folder, FALSE "
                                                                      "if not."),
                                                                   TRUE, GTK_PARAM_READWRITE));
  g_assert (result == PROP_SHELL_SHOWS_DESKTOP);

  /**
   * GtkSettings:gtk-decoration-layout:
   *
   * This setting determines which buttons should be put in the
   * titlebar of client-side decorated windows, and whether they
   * should be placed at the left of right.
   *
   * The format of the string is button names, separated by commas.
   * A colon separates the buttons that should appear on the left
   * from those on the right. Recognized button names are minimize,
   * maximize, close, icon (the window icon) and menu (a menu button
   * for the fallback app menu).
   *
   * For example, "menu:minimize,maximize,close" specifies a menu
   * on the left, and minimize, maximize and close buttons on the right.
   *
   * Note that buttons will only be shown when they are meaningful.
   * E.g. a menu button only appears when the desktop shell does not
   * show the app menu, and a close button only appears on a window
   * that can be closed.
   *
   * Also note that the setting can be overridden with the
   * #GtkHeaderBar:decoration-layout property.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-decoration-layout",
                                                                  P_("Decoration Layout"),
                                                                  P_("The layout for window decorations"),
                                                                  "menu:minimize,maximize,close", GTK_PARAM_READWRITE));
  g_assert (result == PROP_DECORATION_LAYOUT);

  /**
   * GtkSettings:gtk-titlebar-double-click:
   *
   * This setting determines the action to take when a double-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-titlebar-double-click",
                                                                  P_("Titlebar double-click action"),
                                                                  P_("The action to take on titlebar double-click"),
                                                                  "toggle-maximize", GTK_PARAM_READWRITE));
  g_assert (result == PROP_TITLEBAR_DOUBLE_CLICK);

  /**
   * GtkSettings:gtk-titlebar-middle-click:
   *
   * This setting determines the action to take when a middle-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-titlebar-middle-click",
                                                                  P_("Titlebar middle-click action"),
                                                                  P_("The action to take on titlebar middle-click"),
                                                                  "none", GTK_PARAM_READWRITE));
  g_assert (result == PROP_TITLEBAR_MIDDLE_CLICK);

  /**
   * GtkSettings:gtk-titlebar-right-click:
   *
   * This setting determines the action to take when a right-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-titlebar-right-click",
                                                                  P_("Titlebar right-click action"),
                                                                  P_("The action to take on titlebar right-click"),
                                                                  "menu", GTK_PARAM_READWRITE));
  g_assert (result == PROP_TITLEBAR_RIGHT_CLICK);

  /**
   * GtkSettings:gtk-dialogs-use-header:
   *
   * Whether builtin GTK dialogs such as the file chooser, the
   * color chooser or the font chooser will use a header bar at
   * the top to show action widgets, or an action area at the bottom.
   *
   * This setting does not affect custom dialogs using GtkDialog
   * directly, or message dialogs.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-dialogs-use-header",
                                                                   P_("Dialogs use header bar"),
                                                                   P_("Whether builtin GTK dialogs should use a header bar instead of an action area."),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_DIALOGS_USE_HEADER);

  /**
   * GtkSettings:gtk-enable-primary-paste:
   *
   * Whether a middle click on a mouse should paste the
   * 'PRIMARY' clipboard content at the cursor location.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-primary-paste",
                                                                   P_("Enable primary paste"),
                                                                   P_("Whether a middle click on a mouse should paste the “PRIMARY” clipboard content at the cursor location."),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_ENABLE_PRIMARY_PASTE);

  /**
   * GtkSettings:gtk-recent-files-enabled:
   *
   * Whether GTK should keep track of items inside the recently used
   * resources list. If set to %FALSE, the list will always be empty.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-recent-files-enabled",
                                                                   P_("Recent Files Enabled"),
                                                                   P_("Whether GTK remembers recent files"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_RECENT_FILES_ENABLED);

  /**
   * GtkSettings:gtk-long-press-time:
   *
   * The time for a button or touch press to be considered a "long press".
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_uint ("gtk-long-press-time",
								P_("Long press time"),
								P_("Time for a button/touch press to be considered a long press (in milliseconds)"),
								0, G_MAXINT, 500,
								GTK_PARAM_READWRITE));
  g_assert (result == PROP_LONG_PRESS_TIME);

  /**
   * GtkSettings:gtk-keynav-use-caret:
   *
   * Whether GTK should make sure that text can be navigated with
   * a caret, even if it is not editable. This is useful when using
   * a screen reader.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-keynav-use-caret",
                                                                   P_("Whether to show cursor in text"),
                                                                   P_("Whether to show cursor in text"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_KEYNAV_USE_CARET);

  /**
   * GtkSettings:gtk-overlay-scrolling:
   *
   * Whether scrolled windows may use overlayed scrolling indicators.
   * If this is set to %FALSE, scrolled windows will have permanent
   * scrollbars.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-overlay-scrolling",
                                                                   P_("Whether to use overlay scrollbars"),
                                                                   P_("Whether to use overlay scrollbars"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE));
  g_assert (result == PROP_OVERLAY_SCROLLING);
}

static void
gtk_settings_finalize (GObject *object)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  guint i;

  object_list = g_slist_remove (object_list, settings);

  for (i = 0; i < class_n_properties; i++)
    g_value_unset (&priv->property_values[i].value);
  g_free (priv->property_values);

  g_datalist_clear (&priv->queued_settings);

  g_object_unref (priv->theme_style_sheet);
  g_object_unref (priv->style_cascade);

  if (priv->font_options)
    cairo_font_options_destroy (priv->font_options);

  g_free (priv->font_family);

  G_OBJECT_CLASS (gtk_settings_parent_class)->finalize (object);
}

GtkStyleCascade *
gtk_settings_get_style_cascade (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  
  return priv->style_cascade;
}

static void
settings_init_style (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  static GtkCssStyleSheet *css_style_sheet = NULL;

  /* Add stylesheet for user file */
  if (G_UNLIKELY (!css_style_sheet))
    {
      gchar *css_path;

      css_style_sheet = gtk_css_style_sheet_new ();
      gtk_css_style_sheet_set_priority (css_style_sheet, GTK_STYLE_PROVIDER_PRIORITY_USER);
      css_path = g_build_filename (g_get_user_config_dir (),
                                   "gtk-4.0",
                                   "gtk.css",
                                   NULL);

      if (g_file_test (css_path,
                       G_FILE_TEST_IS_REGULAR))
        gtk_css_style_sheet_load_from_path (css_style_sheet, css_path);

      g_free (css_path);
    }

  priv->style_cascade = _gtk_style_cascade_new ();

  gtk_style_cascade_add_style_sheet (priv->style_cascade, css_style_sheet);
  gtk_style_cascade_add_style_sheet (priv->style_cascade, priv->theme_style_sheet),

  settings_update_theme (settings);
}

static void
setting_changed (GdkDisplay       *display,
                 const char       *name,
                 gpointer          data)
{
  GtkSettings *settings = data;
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);

  if (!pspec)
    return;

  if (settings_update_xsetting (settings, pspec, TRUE))
    g_object_notify_by_pspec (G_OBJECT (settings), pspec);
}

static GtkSettings *
gtk_settings_create_for_display (GdkDisplay *display)
{
  GtkSettings *settings;
  GtkSettingsPrivate *priv;

#ifdef GDK_WINDOWING_QUARTZ
  if (GDK_IS_QUARTZ_DISPLAY (display))
    settings = g_object_new (GTK_TYPE_SETTINGS,
                             "gtk-key-theme-name", "Mac",
                             "gtk-shell-shows-app-menu", TRUE,
                             "gtk-shell-shows-menubar", TRUE,
                             NULL);
  else
#endif
    settings = g_object_new (GTK_TYPE_SETTINGS, NULL);

  priv = gtk_settings_get_instance_private (settings);

  priv->display = display;

  g_signal_connect_object (display, "setting-changed", G_CALLBACK (setting_changed), settings, 0);

  g_ptr_array_add (display_settings, settings);

  settings_init_style (settings);
  settings_update_xsettings (settings);
  settings_update_double_click (settings);
  settings_update_cursor_theme (settings);
  settings_update_font_options (settings);
  settings_update_font_values (settings);

  return settings;
}

/**
 * gtk_settings_get_for_display:
 * @display: a #GdkDisplay.
 *
 * Gets the #GtkSettings object for @display, creating it if necessary.
 *
 * Returns: (transfer none): a #GtkSettings object.
 */
GtkSettings *
gtk_settings_get_for_display (GdkDisplay *display)
{
  int i;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if G_UNLIKELY (display_settings == NULL)
    display_settings = g_ptr_array_new ();

  for (i = 0; i < display_settings->len; i++)
    {
      GtkSettings *settings = g_ptr_array_index (display_settings, i);
      GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
      if (priv->display == display)
        return settings;
    }

  return gtk_settings_create_for_display (display);
}

/**
 * gtk_settings_get_default:
 *
 * Gets the #GtkSettings object for the default display, creating
 * it if necessary. See gtk_settings_get_for_display().
 *
 * Returns: (nullable) (transfer none): a #GtkSettings object. If there is
 * no default display, then returns %NULL.
 **/
GtkSettings*
gtk_settings_get_default (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  if (display)
    return gtk_settings_get_for_display (display);

  g_debug ("%s() returning NULL GtkSettings object. Is a display available?",
           G_STRFUNC);
  return NULL;
}

static void
gtk_settings_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);

  g_value_copy (value, &priv->property_values[property_id - 1].value);
  priv->property_values[property_id - 1].source = GTK_SETTINGS_SOURCE_APPLICATION;
}

/* This function exists to avoid roots needing to connect to
 * GtkSettings::notify to track CSS changes
 */
static void
settings_invalidate_style (GtkSettings  *settings,
                           GtkCssChange  change)
{
  GList *list, *toplevels;
  GdkDisplay *display;

  display = _gtk_settings_get_display (settings);
  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) g_object_ref, NULL);

  for (list = toplevels; list; list = list->next)
    {
      if (gtk_widget_get_display (list->data) == display)
        gtk_css_node_invalidate (gtk_widget_get_css_node (list->data), change);

      g_object_unref (list->data);
    }

  g_list_free (toplevels);
}

static void
settings_update_font_values (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  PangoFontDescription *desc;
  const gchar *font_name;

  font_name = g_value_get_string (&priv->property_values[PROP_FONT_NAME - 1].value);
  desc = pango_font_description_from_string (font_name);

  if (desc != NULL &&
      (pango_font_description_get_set_fields (desc) & PANGO_FONT_MASK_SIZE) != 0)
    {
      priv->font_size = pango_font_description_get_size (desc);
      priv->font_size_absolute = pango_font_description_get_size_is_absolute (desc);
    }
  else
    {
      priv->font_size = 10 * PANGO_SCALE;
      priv->font_size_absolute = FALSE;
    }

  g_free (priv->font_family);

  if (desc != NULL &&
      (pango_font_description_get_set_fields (desc) & PANGO_FONT_MASK_FAMILY) != 0)
    {
      priv->font_family = g_strdup (pango_font_description_get_family (desc));
    }
  else
    {
      priv->font_family = g_strdup ("Sans");
    }

  if (desc)
    pango_font_description_free (desc);
}

static void
gtk_settings_notify (GObject    *object,
                     GParamSpec *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  guint property_id = pspec->param_id;

  if (priv->display == NULL) /* initialization */
    return;

  switch (property_id)
    {
    case PROP_DOUBLE_CLICK_TIME:
    case PROP_DOUBLE_CLICK_DISTANCE:
      settings_update_double_click (settings);
      break;
    case PROP_FONT_NAME:
      settings_update_font_values (settings);
      settings_invalidate_style (settings, GTK_CSS_CHANGE_ROOT);
      break;
    case PROP_THEME_NAME:
    case PROP_APPLICATION_PREFER_DARK_THEME:
      settings_update_theme (settings);
      break;
    case PROP_XFT_DPI:
      /* This is a hack because with gtk_rc_reset_styles() doesn't get
       * widgets with gtk_widget_style_set(), and also causes more
       * recomputation than necessary.
       */
      settings_invalidate_style (settings, GTK_CSS_CHANGE_ROOT);
      gtk_style_context_reset_widgets (priv->display);
      break;
    case PROP_XFT_ANTIALIAS:
    case PROP_XFT_HINTING:
    case PROP_XFT_HINTSTYLE:
    case PROP_XFT_RGBA:
      settings_update_font_options (settings);
      gtk_style_context_reset_widgets (priv->display);
      break;
    case PROP_FONTCONFIG_TIMESTAMP:
      if (settings_update_fontconfig (settings))
        gtk_style_context_reset_widgets (priv->display);
      break;
    case PROP_ENABLE_ANIMATIONS:
      gtk_style_context_reset_widgets (priv->display);
      break;
    case PROP_CURSOR_THEME_NAME:
    case PROP_CURSOR_THEME_SIZE:
      settings_update_cursor_theme (settings);
      break;
    default:
      break;
    }
}

static gboolean
_gtk_settings_parse_convert (const GValue       *src_value,
                             GParamSpec         *pspec,
                             GValue             *dest_value)
{
  gboolean success = FALSE;

  g_return_val_if_fail (G_VALUE_HOLDS (dest_value, G_PARAM_SPEC_VALUE_TYPE (pspec)), FALSE);

  if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
    {
      if (G_VALUE_HOLDS (dest_value, G_TYPE_STRING))
        {
          GString *gstring = g_value_get_boxed (src_value);

          g_value_set_string (dest_value, gstring ? gstring->str : NULL);
          success = !g_param_value_validate (pspec, dest_value);
        }
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (src_value), G_VALUE_TYPE (dest_value)))
    success = g_param_value_convert (pspec, src_value, dest_value, TRUE);

  return success;
}

static void
apply_queued_setting (GtkSettings             *settings,
                      GParamSpec              *pspec,
                      GtkSettingsValuePrivate *qvalue)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GValue tmp_value = G_VALUE_INIT;

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (_gtk_settings_parse_convert (&qvalue->public.value,
                                   pspec, &tmp_value))
    {
      if (priv->property_values[pspec->param_id - 1].source <= qvalue->source)
        {
          g_value_copy (&tmp_value, &priv->property_values[pspec->param_id - 1].value);
          priv->property_values[pspec->param_id - 1].source = qvalue->source;
          g_object_notify_by_pspec (G_OBJECT (settings), pspec);
        }

    }
  else
    {
      gchar *debug = g_strdup_value_contents (&qvalue->public.value);

      g_message ("%s: failed to retrieve property '%s' of type '%s' from rc file value \"%s\" of type '%s'",
                 qvalue->public.origin ? qvalue->public.origin : "(for origin information, set GTK_DEBUG)",
                 pspec->name,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                 debug,
                 G_VALUE_TYPE_NAME (&tmp_value));
      g_free (debug);
    }
  g_value_unset (&tmp_value);
}

static guint
settings_install_property_parser (GtkSettingsClass   *class,
                                  GParamSpec         *pspec)
{
  GSList *node, *next;

  switch (G_TYPE_FUNDAMENTAL (G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
    case G_TYPE_UINT:
    case G_TYPE_INT:
    case G_TYPE_ULONG:
    case G_TYPE_LONG:
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
    case G_TYPE_STRING:
    case G_TYPE_ENUM:
      break;
    case G_TYPE_BOXED:
    default:
      g_warning (G_STRLOC ": no parser for property \"%s\" of type '%s'",
                 pspec->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      return 0;
    }
  if (g_object_class_find_property (G_OBJECT_CLASS (class), pspec->name))
    {
      g_warning (G_STRLOC ": an rc-data property \"%s\" already exists",
                 pspec->name);
      return 0;
    }

  for (node = object_list; node; node = node->next)
    g_object_freeze_notify (node->data);

  g_object_class_install_property (G_OBJECT_CLASS (class), ++class_n_properties, pspec);

  for (node = object_list; node; node = node->next)
    {
      GtkSettings *settings = node->data;
      GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
      GtkSettingsValuePrivate *qvalue;

      priv->property_values = g_renew (GtkSettingsPropertyValue, priv->property_values, class_n_properties);
      priv->property_values[class_n_properties - 1].value.g_type = 0;
      g_value_init (&priv->property_values[class_n_properties - 1].value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, &priv->property_values[class_n_properties - 1].value);
      priv->property_values[class_n_properties - 1].source = GTK_SETTINGS_SOURCE_DEFAULT;
      g_object_notify_by_pspec (G_OBJECT (settings), pspec);

      qvalue = g_datalist_id_dup_data (&priv->queued_settings, g_param_spec_get_name_quark (pspec), NULL, NULL);
      if (qvalue)
        apply_queued_setting (settings, pspec, qvalue);
    }

  for (node = object_list; node; node = next)
    {
      next = node->next;
      g_object_thaw_notify (node->data);
    }

  return class_n_properties;
}

static void
free_value (gpointer data)
{
  GtkSettingsValuePrivate *qvalue = data;

  g_value_unset (&qvalue->public.value);
  g_free (qvalue->public.origin);
  g_slice_free (GtkSettingsValuePrivate, qvalue);
}

static void
gtk_settings_set_property_value_internal (GtkSettings            *settings,
                                          const gchar            *prop_name,
                                          const GtkSettingsValue *new_value,
                                          GtkSettingsSource       source)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GtkSettingsValuePrivate *qvalue;
  GParamSpec *pspec;
  gchar *name;
  GQuark name_quark;

  if (!G_VALUE_HOLDS_LONG (&new_value->value) &&
      !G_VALUE_HOLDS_DOUBLE (&new_value->value) &&
      !G_VALUE_HOLDS_STRING (&new_value->value) &&
      !G_VALUE_HOLDS (&new_value->value, G_TYPE_GSTRING))
    {
      g_warning (G_STRLOC ": value type invalid (%s)", g_type_name (G_VALUE_TYPE (&new_value->value)));
      return;
    }

  name = g_strdup (prop_name);
  g_strcanon (name, G_CSET_DIGITS "-" G_CSET_a_2_z G_CSET_A_2_Z, '-');
  name_quark = g_quark_from_string (name);
  g_free (name);

  qvalue = g_datalist_id_dup_data (&priv->queued_settings, name_quark, NULL, NULL);
  if (!qvalue)
    {
      qvalue = g_slice_new0 (GtkSettingsValuePrivate);
      g_datalist_id_set_data_full (&priv->queued_settings, name_quark, qvalue, free_value);
    }
  else
    {
      g_free (qvalue->public.origin);
      g_value_unset (&qvalue->public.value);
    }
  qvalue->public.origin = g_strdup (new_value->origin);
  g_value_init (&qvalue->public.value, G_VALUE_TYPE (&new_value->value));
  g_value_copy (&new_value->value, &qvalue->public.value);
  qvalue->source = source;
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), g_quark_to_string (name_quark));
  if (pspec)
    apply_queued_setting (settings, pspec, qvalue);
}

static void
settings_update_double_click (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  gint double_click_time;
  gint double_click_distance;

  g_object_get (settings,
                "gtk-double-click-time", &double_click_time,
                "gtk-double-click-distance", &double_click_distance,
                NULL);

  gdk_display_set_double_click_time (priv->display, double_click_time);
  gdk_display_set_double_click_distance (priv->display, double_click_distance);
}

static void
settings_update_cursor_theme (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  gchar *theme = NULL;
  gint size = 0;

  g_object_get (settings,
                "gtk-cursor-theme-name", &theme,
                "gtk-cursor-theme-size", &size,
                NULL);
  if (theme)
    {
      gdk_display_set_cursor_theme (priv->display, theme, size);
      g_free (theme);
    }
}

static void
settings_update_font_options (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  gint hinting;
  gchar *hint_style_str;
  cairo_hint_style_t hint_style;
  gint antialias;
  cairo_antialias_t antialias_mode;
  gchar *rgba_str;
  cairo_subpixel_order_t subpixel_order;

  if (priv->font_options)
    cairo_font_options_destroy (priv->font_options);

  g_object_get (settings,
                "gtk-xft-antialias", &antialias,
                "gtk-xft-hinting", &hinting,
                "gtk-xft-hintstyle", &hint_style_str,
                "gtk-xft-rgba", &rgba_str,
                NULL);

  priv->font_options = cairo_font_options_create ();

  cairo_font_options_set_hint_metrics (priv->font_options, CAIRO_HINT_METRICS_OFF);

  hint_style = CAIRO_HINT_STYLE_DEFAULT;
  if (hinting == 0)
    {
      hint_style = CAIRO_HINT_STYLE_NONE;
    }
  else if (hinting == 1)
    {
      if (hint_style_str)
        {
          if (strcmp (hint_style_str, "hintnone") == 0)
            hint_style = CAIRO_HINT_STYLE_NONE;
          else if (strcmp (hint_style_str, "hintslight") == 0)
            hint_style = CAIRO_HINT_STYLE_SLIGHT;
          else if (strcmp (hint_style_str, "hintmedium") == 0)
            hint_style = CAIRO_HINT_STYLE_MEDIUM;
          else if (strcmp (hint_style_str, "hintfull") == 0)
            hint_style = CAIRO_HINT_STYLE_FULL;
        }
    }

  g_free (hint_style_str);

  cairo_font_options_set_hint_style (priv->font_options, hint_style);

  subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
  if (rgba_str)
    {
      if (strcmp (rgba_str, "rgb") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
      else if (strcmp (rgba_str, "bgr") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
      else if (strcmp (rgba_str, "vrgb") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
      else if (strcmp (rgba_str, "vbgr") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
    }

  g_free (rgba_str);

  cairo_font_options_set_subpixel_order (priv->font_options, subpixel_order);

  antialias_mode = CAIRO_ANTIALIAS_DEFAULT;
  if (antialias == 0)
    {
      antialias_mode = CAIRO_ANTIALIAS_NONE;
    }
  else if (antialias == 1)
    {
      if (subpixel_order != CAIRO_SUBPIXEL_ORDER_DEFAULT)
        antialias_mode = CAIRO_ANTIALIAS_SUBPIXEL;
      else
        antialias_mode = CAIRO_ANTIALIAS_GRAY;
    }

  cairo_font_options_set_antialias (priv->font_options, antialias_mode);
}

static gboolean
settings_update_fontconfig (GtkSettings *settings)
{
#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
  static guint    last_update_timestamp;
  static gboolean last_update_needed;

  guint timestamp;

  g_object_get (settings,
                "gtk-fontconfig-timestamp", &timestamp,
                NULL);

  /* if timestamp is the same as last_update_timestamp, we already have
   * updated fontconig on this timestamp (another screen requested it perhaps?),
   * just return the cached result.*/

  if (timestamp != last_update_timestamp)
    {
      PangoFontMap *fontmap = pango_cairo_font_map_get_default ();
      gboolean update_needed = FALSE;

      /* bug 547680 */
      if (PANGO_IS_FC_FONT_MAP (fontmap) && !FcConfigUptoDate (NULL))
        {
          pango_fc_font_map_config_changed (PANGO_FC_FONT_MAP (fontmap));
          if (FcInitReinitialize ())
            update_needed = TRUE;
        }

      last_update_timestamp = timestamp;
      last_update_needed = update_needed;
    }

  return last_update_needed;
#else
  return FALSE;
#endif /* GDK_WINDOWING_X11 || GDK_WINDOWING_WAYLAND */
}

static void
get_theme_name (GtkSettings  *settings,
                gchar       **theme_name,
                gchar       **theme_variant)
{
  gboolean prefer_dark;

  *theme_name = NULL;
  *theme_variant = NULL;

  if (g_getenv ("GTK_THEME"))
    *theme_name = g_strdup (g_getenv ("GTK_THEME"));

  if (*theme_name && **theme_name)
    {
      char *p;
      p = strrchr (*theme_name, ':');
      if (p) {
        *p = '\0';
        p++;
        *theme_variant = g_strdup (p);
      }

      return;
    }

  g_free (*theme_name);

  g_object_get (settings,
                "gtk-theme-name", theme_name,
                "gtk-application-prefer-dark-theme", &prefer_dark,
                NULL);

  if (prefer_dark)
    *theme_variant = g_strdup ("dark");

  if (*theme_name && **theme_name)
    return;

  g_free (*theme_name);
  *theme_name = g_strdup (DEFAULT_THEME_NAME);
}

static void
settings_update_theme (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  gchar *theme_name;
  gchar *theme_variant;
  const gchar *theme_dir;
  gchar *path;

  get_theme_name (settings, &theme_name, &theme_variant);

  gtk_css_style_sheet_load_named (priv->theme_style_sheet,
                                  theme_name,
                                  theme_variant);

  /* reload per-theme settings */
  theme_dir = gtk_css_style_sheet_get_theme_dir (priv->theme_style_sheet);
  if (theme_dir)
    {
      path = g_build_filename (theme_dir, "settings.ini", NULL);
      if (g_file_test (path, G_FILE_TEST_EXISTS))
        gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_THEME);
      g_free (path);
    }

  g_free (theme_name);
  g_free (theme_variant);
}

const cairo_font_options_t *
gtk_settings_get_font_options (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  return priv->font_options;
}

GdkDisplay *
_gtk_settings_get_display (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  return priv->display;
}

static void
gvalue_free (gpointer data)
{
  g_value_unset (data);
  g_free (data);
}

static void
gtk_settings_load_from_key_file (GtkSettings       *settings,
                                 const gchar       *path,
                                 GtkSettingsSource  source)
{
  GError *error;
  GKeyFile *keyfile;
  gchar **keys;
  gsize n_keys;
  gint i;

  error = NULL;
  keys = NULL;

  keyfile = g_key_file_new ();

  if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error))
    {
      g_warning ("Failed to parse %s: %s", path, error->message);

      g_error_free (error);

      goto out;
    }

  keys = g_key_file_get_keys (keyfile, "Settings", &n_keys, &error);
  if (error)
    {
      g_warning ("Failed to parse %s: %s", path, error->message);
      g_error_free (error);
      goto out;
    }

  for (i = 0; i < n_keys; i++)
    {
      gchar *key;
      GParamSpec *pspec;
      GType value_type;
      GtkSettingsValue svalue = { NULL, { 0, }, };

      key = keys[i];
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), key);
      if (!pspec)
        {
          g_warning ("Unknown key %s in %s", key, path);
          continue;
        }

      if (pspec->owner_type != G_OBJECT_TYPE (settings))
        continue;

      value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);
      switch (value_type)
        {
        case G_TYPE_BOOLEAN:
          {
            gboolean b_val;

            g_value_init (&svalue.value, G_TYPE_LONG);
            b_val = g_key_file_get_boolean (keyfile, "Settings", key, &error);
            if (!error)
              g_value_set_long (&svalue.value, b_val);
            break;
          }

        case G_TYPE_INT:
        case G_TYPE_UINT:
          {
            gint i_val;

            g_value_init (&svalue.value, G_TYPE_LONG);
            i_val = g_key_file_get_integer (keyfile, "Settings", key, &error);
            if (!error)
              g_value_set_long (&svalue.value, i_val);
            break;
          }

        case G_TYPE_DOUBLE:
          {
            gdouble d_val;

            g_value_init (&svalue.value, G_TYPE_DOUBLE);
            d_val = g_key_file_get_double (keyfile, "Settings", key, &error);
            if (!error)
              g_value_set_double (&svalue.value, d_val);
            break;
          }

        default:
          {
            gchar *s_val;

            g_value_init (&svalue.value, G_TYPE_GSTRING);
            s_val = g_key_file_get_string (keyfile, "Settings", key, &error);
            if (!error)
              g_value_take_boxed (&svalue.value, g_string_new (s_val));
            g_free (s_val);
            break;
          }
        }
      if (error)
        {
          g_warning ("Error setting %s in %s: %s", key, path, error->message);
          g_error_free (error);
          error = NULL;
        }
      else
        {
          GValue *copy;

          copy = g_new0 (GValue, 1);

          g_value_init (copy, G_VALUE_TYPE (&svalue.value));
          g_value_copy (&svalue.value, copy);

          g_param_spec_set_qdata_full (pspec, g_quark_from_string (key),
                                       copy, gvalue_free);

          if (g_getenv ("GTK_DEBUG"))
            svalue.origin = (gchar *)path;

          gtk_settings_set_property_value_internal (settings, key, &svalue, source);
          g_value_unset (&svalue.value);
        }
    }

 out:
  g_strfreev (keys);
  g_key_file_free (keyfile);
}

static gboolean
settings_update_xsetting (GtkSettings *settings,
                          GParamSpec  *pspec,
                          gboolean     force)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GType value_type;
  GType fundamental_type;
  gboolean retval = FALSE;

  if (priv->property_values[pspec->param_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION)
    return FALSE;

  if (priv->property_values[pspec->param_id - 1].source == GTK_SETTINGS_SOURCE_XSETTING && !force)
    return FALSE;

  value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);
  fundamental_type = G_TYPE_FUNDAMENTAL (value_type);

  if ((g_value_type_transformable (G_TYPE_INT, value_type) &&
       !(fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)) ||
      g_value_type_transformable (G_TYPE_STRING, value_type) ||
      g_value_type_transformable (GDK_TYPE_RGBA, value_type))
    {
      GValue val = G_VALUE_INIT;

      g_value_init (&val, value_type);

      if (!gdk_display_get_setting (priv->display, pspec->name, &val))
        return FALSE;

      g_param_value_validate (pspec, &val);
      g_value_copy (&val, &priv->property_values[pspec->param_id - 1].value);
      priv->property_values[pspec->param_id - 1].source = GTK_SETTINGS_SOURCE_XSETTING;

      g_value_unset (&val);

      retval = TRUE;
    }
  else
    {
      return FALSE;
    }

  return retval;
}

static void
settings_update_xsettings (GtkSettings *settings)
{
  GParamSpec **pspecs;
  gint i;

  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), NULL);
  for (i = 0; pspecs[i]; i++)
    settings_update_xsetting (settings, pspecs[i], FALSE);
  g_free (pspecs);
}

static void
gtk_settings_get_property (GObject     *object,
                           guint        property_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);

  settings_update_xsetting (settings, pspec, FALSE);

  g_value_copy (&priv->property_values[property_id - 1].value, value);
}

GtkSettingsSource
_gtk_settings_get_setting_source (GtkSettings *settings,
                                  const gchar *name)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);
  if (!pspec)
    return GTK_SETTINGS_SOURCE_DEFAULT;

  return priv->property_values[pspec->param_id - 1].source;
}

/**
 * gtk_settings_reset_property:
 * @settings: a #GtkSettings object
 * @name: the name of the setting to reset
 *
 * Undoes the effect of calling g_object_set() to install an
 * application-specific value for a setting. After this call,
 * the setting will again follow the session-wide value for
 * this setting.
 */
void
gtk_settings_reset_property (GtkSettings *settings,
                             const gchar *name)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GParamSpec *pspec;
  GValue *value;
  GValue tmp_value = G_VALUE_INIT;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);

  g_return_if_fail (pspec != NULL);

  value = g_param_spec_get_qdata (pspec, g_quark_from_string (name));

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (value && _gtk_settings_parse_convert (value, pspec, &tmp_value))
    g_value_copy (&tmp_value, &priv->property_values[pspec->param_id - 1].value);
  else
    g_param_value_set_default (pspec, &priv->property_values[pspec->param_id - 1].value);

  priv->property_values[pspec->param_id - 1].source = GTK_SETTINGS_SOURCE_DEFAULT;
  g_object_notify_by_pspec (G_OBJECT (settings), pspec);
}

gboolean
gtk_settings_get_enable_animations (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GtkSettingsPropertyValue *svalue = &priv->property_values[PROP_ENABLE_ANIMATIONS - 1];

  if (svalue->source < GTK_SETTINGS_SOURCE_XSETTING)
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-enable-animations");
      if (settings_update_xsetting (settings, pspec, FALSE))
        g_object_notify_by_pspec (G_OBJECT (settings), pspec);
    }

  return g_value_get_boolean (&svalue->value);
}

gint
gtk_settings_get_dnd_drag_threshold (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GtkSettingsPropertyValue *svalue = &priv->property_values[PROP_DND_DRAG_THRESHOLD - 1];

  if (svalue->source < GTK_SETTINGS_SOURCE_XSETTING)
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-dnd-drag-threshold");
      if (settings_update_xsetting (settings, pspec, FALSE))
        g_object_notify_by_pspec (G_OBJECT (settings), pspec);
    }

  return g_value_get_int (&svalue->value);
}

static void
settings_update_font_name (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  GtkSettingsPropertyValue *svalue = &priv->property_values[PROP_FONT_NAME - 1];

  if (svalue->source < GTK_SETTINGS_SOURCE_XSETTING)
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-font-name");
      if (settings_update_xsetting (settings, pspec, FALSE))
        g_object_notify_by_pspec (G_OBJECT (settings), pspec);
    }
}

const gchar *
gtk_settings_get_font_family (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  settings_update_font_name (settings);

  return priv->font_family;
}

gint
gtk_settings_get_font_size (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  settings_update_font_name (settings);

  return priv->font_size;
}

gboolean
gtk_settings_get_font_size_is_absolute (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = gtk_settings_get_instance_private (settings);
  settings_update_font_name (settings);

  return priv->font_size_absolute;
}
