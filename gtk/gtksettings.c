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


#define PANGO_ENABLE_BACKEND /* for pango_fc_font_map_cache_clear() */

#include "config.h"

#include <string.h>

#include "gtksettings.h"

#include "gtkmodules.h"
#include "gtkmodulesprivate.h"
#include "gtksettingsprivate.h"
#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkprivate.h"
#include "gtkcssproviderprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtktypebuiltins.h"
#include "gtkversion.h"
#include "gtkscrolledwindow.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include <pango/pangofc-fontmap.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "quartz/gdkquartz.h"
#endif

#ifdef G_OS_WIN32
#include "gtkwin32themeprivate.h"
#endif

#include "deprecated/gtkrc.h"

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
 * an Xsettings manager, GTK+ reads default values for settings from
 * `settings.ini` files in
 * `/etc/gtk-3.0`, `$XDG_CONFIG_DIRS/gtk-3.0`
 * and `$XDG_CONFIG_HOME/gtk-3.0`.
 * These files must be valid key files (see #GKeyFile), and have
 * a section called Settings. Themes can also provide default values
 * for settings by installing a `settings.ini` file
 * next to their `gtk.css` file.
 *
 * Applications can override system-wide settings by setting the property
 * of the GtkSettings object with g_object_set(). This should be restricted
 * to special cases though; GtkSettings are not meant as an application
 * configuration facility. When doing so, you need to be aware that settings
 * that are specific to individual widgets may not be available before the
 * widget type has been realized at least once. The following example
 * demonstrates a way to do this:
 * |[<!-- language="C" -->
 *   gtk_init (&argc, &argv);
 *
 *   // make sure the type is realized
 *   g_type_class_unref (g_type_class_ref (GTK_TYPE_IMAGE_MENU_ITEM));
 *
 *   g_object_set (gtk_settings_get_default (), "gtk-enable-animations", FALSE, NULL);
 * ]|
 *
 * There is one GtkSettings instance per screen. It can be obtained with
 * gtk_settings_get_for_screen(), but in many cases, it is more convenient
 * to use gtk_widget_get_settings(). gtk_settings_get_default() returns the
 * GtkSettings instance for the default screen.
 */


#define DEFAULT_TIMEOUT_INITIAL 500
#define DEFAULT_TIMEOUT_REPEAT   50
#define DEFAULT_TIMEOUT_EXPAND  500

typedef struct _GtkSettingsPropertyValue GtkSettingsPropertyValue;
typedef struct _GtkSettingsValuePrivate GtkSettingsValuePrivate;

struct _GtkSettingsPrivate
{
  GData *queued_settings;      /* of type GtkSettingsValue* */
  GtkSettingsPropertyValue *property_values;
  GdkScreen *screen;
  GtkStyleCascade *style_cascade;
  GtkCssProvider *theme_provider;
  GtkCssProvider *key_theme_provider;
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
  PROP_FALLBACK_ICON_THEME,
  PROP_KEY_THEME_NAME,
  PROP_MENU_BAR_ACCEL,
  PROP_DND_DRAG_THRESHOLD,
  PROP_FONT_NAME,
  PROP_ICON_SIZES,
  PROP_MODULES,
  PROP_XFT_ANTIALIAS,
  PROP_XFT_HINTING,
  PROP_XFT_HINTSTYLE,
  PROP_XFT_RGBA,
  PROP_XFT_DPI,
  PROP_CURSOR_THEME_NAME,
  PROP_CURSOR_THEME_SIZE,
  PROP_ALTERNATIVE_BUTTON_ORDER,
  PROP_ALTERNATIVE_SORT_ARROWS,
  PROP_SHOW_INPUT_METHOD_MENU,
  PROP_SHOW_UNICODE_MENU,
  PROP_TIMEOUT_INITIAL,
  PROP_TIMEOUT_REPEAT,
  PROP_TIMEOUT_EXPAND,
  PROP_COLOR_SCHEME,
  PROP_ENABLE_ANIMATIONS,
  PROP_TOUCHSCREEN_MODE,
  PROP_TOOLTIP_TIMEOUT,
  PROP_TOOLTIP_BROWSE_TIMEOUT,
  PROP_TOOLTIP_BROWSE_MODE_TIMEOUT,
  PROP_KEYNAV_CURSOR_ONLY,
  PROP_KEYNAV_WRAP_AROUND,
  PROP_ERROR_BELL,
  PROP_COLOR_HASH,
  PROP_FILE_CHOOSER_BACKEND,
  PROP_PRINT_BACKENDS,
  PROP_PRINT_PREVIEW_COMMAND,
  PROP_ENABLE_MNEMONICS,
  PROP_ENABLE_ACCELS,
  PROP_RECENT_FILES_LIMIT,
  PROP_IM_MODULE,
  PROP_RECENT_FILES_MAX_AGE,
  PROP_FONTCONFIG_TIMESTAMP,
  PROP_SOUND_THEME_NAME,
  PROP_ENABLE_INPUT_FEEDBACK_SOUNDS,
  PROP_ENABLE_EVENT_SOUNDS,
  PROP_ENABLE_TOOLTIPS,
  PROP_TOOLBAR_STYLE,
  PROP_TOOLBAR_ICON_SIZE,
  PROP_AUTO_MNEMONICS,
  PROP_PRIMARY_BUTTON_WARPS_SLIDER,
  PROP_VISIBLE_FOCUS,
  PROP_APPLICATION_PREFER_DARK_THEME,
  PROP_BUTTON_IMAGES,
  PROP_ENTRY_SELECT_ON_FOCUS,
  PROP_ENTRY_PASSWORD_HINT_TIMEOUT,
  PROP_MENU_IMAGES,
  PROP_MENU_BAR_POPUP_DELAY,
  PROP_SCROLLED_WINDOW_PLACEMENT,
  PROP_CAN_CHANGE_ACCELS,
  PROP_MENU_POPUP_DELAY,
  PROP_MENU_POPDOWN_DELAY,
  PROP_LABEL_SELECT_ON_FOCUS,
  PROP_COLOR_PALETTE,
  PROP_IM_PREEDIT_STYLE,
  PROP_IM_STATUS_STYLE,
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
  PROP_LONG_PRESS_TIME
};

/* --- prototypes --- */
static void     gtk_settings_provider_iface_init (GtkStyleProviderIface *iface);
static void     gtk_settings_provider_private_init (GtkStyleProviderPrivateInterface *iface);

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
                                                  GParamSpec            *pspec,
                                                  GtkRcPropertyParser    parser);
static void    settings_update_double_click      (GtkSettings           *settings);
static void    settings_update_modules           (GtkSettings           *settings);

static void    settings_update_cursor_theme      (GtkSettings           *settings);
static void    settings_update_resolution        (GtkSettings           *settings);
static void    settings_update_font_options      (GtkSettings           *settings);
static gboolean settings_update_fontconfig       (GtkSettings           *settings);
static void    settings_update_theme             (GtkSettings *settings);
static void    settings_update_key_theme         (GtkSettings *settings);

static void gtk_settings_load_from_key_file      (GtkSettings           *settings,
                                                  const gchar           *path,
                                                  GtkSettingsSource      source);
static void settings_update_provider             (GdkScreen             *screen,
                                                  GtkCssProvider       **old,
                                                  GtkCssProvider        *new);

/* the default palette for GtkColorSelelection */
static const gchar default_color_palette[] =
  "black:white:gray50:red:purple:blue:light blue:green:yellow:orange:"
  "lavender:brown:goldenrod4:dodger blue:pink:light green:gray10:gray30:gray75:gray90";

/* --- variables --- */
static GQuark            quark_property_parser = 0;
static GSList           *object_list = NULL;
static guint             class_n_properties = 0;


G_DEFINE_TYPE_EXTENDED (GtkSettings, gtk_settings, G_TYPE_OBJECT, 0,
                        G_ADD_PRIVATE (GtkSettings)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_settings_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER_PRIVATE,
                                               gtk_settings_provider_private_init));

/* --- functions --- */
static void
gtk_settings_init (GtkSettings *settings)
{
  GtkSettingsPrivate *priv;
  GParamSpec **pspecs, **p;
  guint i = 0;
  gchar *path;
  const gchar * const *config_dirs;
  const gchar *config_dir;

  priv = gtk_settings_get_instance_private (settings);
  settings->priv = priv;

  g_datalist_init (&priv->queued_settings);
  object_list = g_slist_prepend (object_list, settings);

  priv->style_cascade = _gtk_style_cascade_new ();
  priv->theme_provider = gtk_css_provider_new ();

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

      g_object_notify (G_OBJECT (settings), pspec->name);
      priv->property_values[i].source = GTK_SETTINGS_SOURCE_DEFAULT;
      i++;
    }
  g_free (pspecs);

  path = g_build_filename (_gtk_get_data_prefix (), "share", "gtk-3.0", "settings.ini", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  path = g_build_filename (_gtk_get_sysconfdir (), "gtk-3.0", "settings.ini", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  config_dirs = g_get_system_config_dirs ();
  for (config_dir = *config_dirs; *config_dirs != NULL; config_dirs++)
    {
      path = g_build_filename (config_dir, "gtk-3.0", "settings.ini", NULL);
      if (g_file_test (path, G_FILE_TEST_EXISTS))
        gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
      g_free (path);
    }

  path = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "settings.ini", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  g_object_thaw_notify (G_OBJECT (settings));
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

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-time",
                                                               P_("Double Click Time"),
                                                               P_("Maximum time allowed between two clicks for them to be considered a double click (in milliseconds)"),
                                                               0, G_MAXINT, 250,
                                                               GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DOUBLE_CLICK_TIME);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-distance",
                                                               P_("Double Click Distance"),
                                                               P_("Maximum distance allowed between two clicks for them to be considered a double click (in pixels)"),
                                                               0, G_MAXINT, 5,
                                                               GTK_PARAM_READWRITE),
                                             NULL);
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
                                                                   GTK_PARAM_READWRITE ),
                                             NULL);
  g_assert (result == PROP_CURSOR_BLINK);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-blink-time",
                                                               P_("Cursor Blink Time"),
                                                               P_("Length of the cursor blink cycle, in milliseconds"),
                                                               100, G_MAXINT, 1200,
                                                               GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_CURSOR_BLINK_TIME);

  /**
   * GtkSettings:gtk-cursor-blink-timeout:
   *
   * Time after which the cursor stops blinking, in seconds.
   * The timer is reset after each user interaction.
   *
   * Setting this to zero has the same effect as setting
   * #GtkSettings:gtk-cursor-blink to %FALSE.
   *
   * Since: 2.12
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-blink-timeout",
                                                               P_("Cursor Blink Timeout"),
                                                               P_("Time after which the cursor stops blinking, in seconds"),
                                                               1, G_MAXINT, 10,
                                                               GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_CURSOR_BLINK_TIMEOUT);
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-split-cursor",
                                                                   P_("Split Cursor"),
                                                                   P_("Whether two cursors should be displayed for mixed left-to-right and right-to-left text"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_SPLIT_CURSOR);
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-theme-name",
                                                                   P_("Theme Name"),
                                                                   P_("Name of theme to load"),
                                                                  DEFAULT_THEME_NAME,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_THEME_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-icon-theme-name",
                                                                  P_("Icon Theme Name"),
                                                                  P_("Name of icon theme to use"),
                                                                  DEFAULT_ICON_THEME,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ICON_THEME_NAME);

  /**
   * GtkSettings:gtk-fallback-icon-theme:
   *
   * Name of a icon theme to fall back to.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-fallback-icon-theme",
                                                                  P_("Fallback Icon Theme Name"),
                                                                  P_("Name of a icon theme to fall back to"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_FALLBACK_ICON_THEME);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-key-theme-name",
                                                                  P_("Key Theme Name"),
                                                                  P_("Name of key theme to load"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_KEY_THEME_NAME);

  /**
   * GtkSettings:gtk-menu-bar-accel:
   *
   * Keybinding to activate the menu bar.
   *
   * Deprecated: 3.10: This setting can still be used for application
   *      overrides, but will be ignored in the future
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-menu-bar-accel",
                                                                  P_("Menu bar accelerator"),
                                                                  P_("Keybinding to activate the menu bar"),
                                                                  "F10",
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_MENU_BAR_ACCEL);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-dnd-drag-threshold",
                                                               P_("Drag threshold"),
                                                               P_("Number of pixels the cursor can move before dragging"),
                                                               1, G_MAXINT, 8,
                                                               GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DND_DRAG_THRESHOLD);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-font-name",
                                                                   P_("Font Name"),
                                                                   P_("Name of default font to use"),
                                                                  "Sans 10",
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_FONT_NAME);

  /**
   * GtkSettings:gtk-icon-sizes:
   *
   * A list of icon sizes. The list is separated by colons, and
   * item has the form:
   *
   * `size-name` = `width` , `height`
   *
   * E.g. "gtk-menu=16,16:gtk-button=20,20:gtk-dialog=48,48".
   * GTK+ itself use the following named icon sizes: gtk-menu,
   * gtk-button, gtk-small-toolbar, gtk-large-toolbar, gtk-dnd,
   * gtk-dialog. Applications can register their own named icon
   * sizes with gtk_icon_size_register().
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-icon-sizes",
                                                                   P_("Icon Sizes"),
                                                                   P_("List of icon sizes (gtk-menu=16,16:gtk-button=20,20..."),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_ICON_SIZES);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-modules",
                                                                  P_("GTK Modules"),
                                                                  P_("List of currently active GTK modules"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_MODULES);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-xft-antialias",
                                                               P_("Xft Antialias"),
                                                               P_("Whether to antialias Xft fonts; 0=no, 1=yes, -1=default"),
                                                               -1, 1, -1,
                                                               GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_XFT_ANTIALIAS);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-xft-hinting",
                                                               P_("Xft Hinting"),
                                                               P_("Whether to hint Xft fonts; 0=no, 1=yes, -1=default"),
                                                               -1, 1, -1,
                                                               GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_XFT_HINTING);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-xft-hintstyle",
                                                                  P_("Xft Hint Style"),
                                                                  P_("What degree of hinting to use; hintnone, hintslight, hintmedium, or hintfull"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE),
                                              NULL);

  g_assert (result == PROP_XFT_HINTSTYLE);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-xft-rgba",
                                                                  P_("Xft RGBA"),
                                                                  P_("Type of subpixel antialiasing; none, rgb, bgr, vrgb, vbgr"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_XFT_RGBA);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-xft-dpi",
                                                               P_("Xft DPI"),
                                                               P_("Resolution for Xft, in 1024 * dots/inch. -1 to use default value"),
                                                               -1, 1024*1024, -1,
                                                               GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_XFT_DPI);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-cursor-theme-name",
                                                                  P_("Cursor theme name"),
                                                                  P_("Name of the cursor theme to use, or NULL to use the default theme"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_CURSOR_THEME_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-theme-size",
                                                               P_("Cursor theme size"),
                                                               P_("Size to use for cursors, or 0 to use the default size"),
                                                               0, 128, 0,
                                                               GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_CURSOR_THEME_SIZE);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-alternative-button-order",
                                                                   P_("Alternative button order"),
                                                                   P_("Whether buttons in dialogs should use the alternative button order"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ALTERNATIVE_BUTTON_ORDER);

  /**
   * GtkSettings:gtk-alternative-sort-arrows:
   *
   * Controls the direction of the sort indicators in sorted list and tree
   * views. By default an arrow pointing down means the column is sorted
   * in ascending order. When set to %TRUE, this order will be inverted.
   *
   * Since: 2.12
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-alternative-sort-arrows",
                                                                   P_("Alternative sort indicator direction"),
                                                                   P_("Whether the direction of the sort indicators in list and tree views is inverted compared to the default (where down means ascending)"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ALTERNATIVE_SORT_ARROWS);

  /**
   * GtkSettings:gtk-show-input-method-menu:
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-show-input-method-menu",
                                                                   P_("Show the 'Input Methods' menu"),
                                                                   P_("Whether the context menus of entries and text views should offer to change the input method"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_SHOW_INPUT_METHOD_MENU);

  /**
   * GtkSettings:gtk-show-unicode-menu:
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-show-unicode-menu",
                                                                   P_("Show the 'Insert Unicode Control Character' menu"),
                                                                   P_("Whether the context menus of entries and text views should offer to insert control characters"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_SHOW_UNICODE_MENU);

  /**
   * GtkSettings:gtk-timeout-initial:
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-timeout-initial",
                                                               P_("Start timeout"),
                                                               P_("Starting value for timeouts, when button is pressed"),
                                                               0, G_MAXINT, DEFAULT_TIMEOUT_INITIAL,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TIMEOUT_INITIAL);

  /**
   * GtkSettings:gtk-timeout-repeat:
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-timeout-repeat",
                                                               P_("Repeat timeout"),
                                                               P_("Repeat value for timeouts, when button is pressed"),
                                                               0, G_MAXINT, DEFAULT_TIMEOUT_REPEAT,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TIMEOUT_REPEAT);

  /**
   * GtkSettings:gtk-timeout-expand:
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-timeout-expand",
                                                               P_("Expand timeout"),
                                                               P_("Expand value for timeouts, when a widget is expanding a new region"),
                                                               0, G_MAXINT, DEFAULT_TIMEOUT_EXPAND,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TIMEOUT_EXPAND);

  /**
   * GtkSettings:gtk-color-scheme:
   *
   * A palette of named colors for use in themes. The format of the string is
   * |[
   * name1: color1
   * name2: color2
   * ...
   * ]|
   * Color names must be acceptable as identifiers in the
   * [gtkrc][gtk3-Resource-Files] syntax, and
   * color specifications must be in the format accepted by
   * gdk_color_parse().
   *
   * Note that due to the way the color tables from different sources are
   * merged, color specifications will be converted to hexadecimal form
   * when getting this property.
   *
   * Starting with GTK+ 2.12, the entries can alternatively be separated
   * by ';' instead of newlines:
   * |[
   * name1: color1; name2: color2; ...
   * ]|
   *
   * Since: 2.10
   *
   * Deprecated: 3.8: Color scheme support was dropped and is no longer supported.
   *     You can still set this property, but it will be ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-color-scheme",
                                                                  P_("Color scheme"),
                                                                  P_("A palette of named colors for use in themes"),
                                                                  "",
                                                                  GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_COLOR_SCHEME);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-animations",
                                                                   P_("Enable Animations"),
                                                                   P_("Whether to enable toolkit-wide animations."),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_ENABLE_ANIMATIONS);

  /**
   * GtkSettings:gtk-touchscreen-mode:
   *
   * When %TRUE, there are no motion notify events delivered on this screen,
   * and widgets can't use the pointer hovering them for any essential
   * functionality.
   *
   * Since: 2.10
   *
   * Deprecated: 3.4. Generally, the behavior for touchscreen input should be
   *             performed dynamically based on gdk_event_get_source_device().
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-touchscreen-mode",
                                                                   P_("Enable Touchscreen Mode"),
                                                                   P_("When TRUE, there are no motion notify events delivered on this screen"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TOUCHSCREEN_MODE);

  /**
   * GtkSettings:gtk-tooltip-timeout:
   *
   * Time, in milliseconds, after which a tooltip could appear if the
   * cursor is hovering on top of a widget.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-tooltip-timeout",
                                                               P_("Tooltip timeout"),
                                                               P_("Timeout before tooltip is shown"),
                                                               0, G_MAXINT,
                                                               500,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TOOLTIP_TIMEOUT);

  /**
   * GtkSettings:gtk-tooltip-browse-timeout:
   *
   * Controls the time after which tooltips will appear when
   * browse mode is enabled, in milliseconds.
   *
   * Browse mode is enabled when the mouse pointer moves off an object
   * where a tooltip was currently being displayed. If the mouse pointer
   * hits another object before the browse mode timeout expires (see
   * #GtkSettings:gtk-tooltip-browse-mode-timeout), it will take the
   * amount of milliseconds specified by this setting to popup the tooltip
   * for the new object.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-tooltip-browse-timeout",
                                                               P_("Tooltip browse timeout"),
                                                               P_("Timeout before tooltip is shown when browse mode is enabled"),
                                                               0, G_MAXINT,
                                                               60,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TOOLTIP_BROWSE_TIMEOUT);

  /**
   * GtkSettings:gtk-tooltip-browse-mode-timeout:
   *
   * Amount of time, in milliseconds, after which the browse mode
   * will be disabled.
   *
   * See #GtkSettings:gtk-tooltip-browse-timeout for more information
   * about browse mode.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-tooltip-browse-mode-timeout",
                                                               P_("Tooltip browse mode timeout"),
                                                               P_("Timeout after which browse mode is disabled"),
                                                               0, G_MAXINT,
                                                               500,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_TOOLTIP_BROWSE_MODE_TIMEOUT);

  /**
   * GtkSettings:gtk-keynav-cursor-only:
   *
   * When %TRUE, keyboard navigation should be able to reach all widgets
   * by using the cursor keys only. Tab, Shift etc. keys can't be expected
   * to be present on the used input device.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: Generally, the behavior for touchscreen input should be
   *             performed dynamically based on gdk_event_get_source_device().
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-keynav-cursor-only",
                                                                   P_("Keynav Cursor Only"),
                                                                   P_("When TRUE, there are only cursor keys available to navigate widgets"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_KEYNAV_CURSOR_ONLY);

  /**
   * GtkSettings:gtk-keynav-wrap-around:
   *
   * When %TRUE, some widgets will wrap around when doing keyboard
   * navigation, such as menus, menubars and notebooks.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-keynav-wrap-around",
                                                                   P_("Keynav Wrap Around"),
                                                                   P_("Whether to wrap around when keyboard-navigating widgets"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);

  g_assert (result == PROP_KEYNAV_WRAP_AROUND);

  /**
   * GtkSettings:gtk-error-bell:
   *
   * When %TRUE, keyboard navigation and other input-related errors
   * will cause a beep. Since the error bell is implemented using
   * gdk_window_beep(), the windowing system may offer ways to
   * configure the error bell in many ways, such as flashing the
   * window or similar visual effects.
   *
   * Since: 2.12
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-error-bell",
                                                                   P_("Error Bell"),
                                                                   P_("When TRUE, keyboard navigation and other errors will cause a beep"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_ERROR_BELL);

  /**
   * GtkSettings:color-hash: (element-type utf8 Gdk.Color):
   *
   * Holds a hash table representation of the #GtkSettings:gtk-color-scheme
   * setting, mapping color names to #GdkColors.
   *
   * Since: 2.10
   *
   * Deprecated: 3.8: Will always return an empty hash table.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boxed ("color-hash",
                                                                 P_("Color Hash"),
                                                                 P_("A hash table representation of the color scheme."),
                                                                 G_TYPE_HASH_TABLE,
                                                                 GTK_PARAM_READABLE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_COLOR_HASH);

  /**
   * GtkSettings:gtk-file-chooser-backend:
   *
   * Name of the GtkFileChooser backend to use by default.
   *
   * Deprecated: 3.10: This setting is ignored. #GtkFileChooser uses GIO by default.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-file-chooser-backend",
                                                                  P_("Default file chooser backend"),
                                                                  P_("Name of the GtkFileChooser backend to use by default"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_FILE_CHOOSER_BACKEND);

  /**
   * GtkSettings:gtk-print-backends:
   *
   * A comma-separated list of print backends to use in the print
   * dialog. Available print backends depend on the GTK+ installation,
   * and may include "file", "cups", "lpr" or "papi".
   *
   * Since: 2.10
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-print-backends",
                                                                  P_("Default print backend"),
                                                                  P_("List of the GtkPrintBackend backends to use by default"),
                                                                  GTK_PRINT_BACKENDS,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
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
   *
   * Since: 2.10
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-print-preview-command",
                                                                  P_("Default command to run when displaying a print preview"),
                                                                  P_("Command to run when displaying a print preview"),
                                                                  GTK_PRINT_PREVIEW_COMMAND,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_PRINT_PREVIEW_COMMAND);

  /**
   * GtkSettings:gtk-enable-mnemonics:
   *
   * Whether labels and menu items should have visible mnemonics which
   * can be activated.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: This setting can still be used for application
   *      overrides, but will be ignored in the future
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-mnemonics",
                                                                   P_("Enable Mnemonics"),
                                                                   P_("Whether labels should have mnemonics"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENABLE_MNEMONICS);

  /**
   * GtkSettings:gtk-enable-accels:
   *
   * Whether menu items should have visible accelerators which can be
   * activated.
   *
   * Since: 2.12
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-accels",
                                                                   P_("Enable Accelerators"),
                                                                   P_("Whether menu items should have accelerators"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENABLE_ACCELS);

  /**
   * GtkSettings:gtk-recent-files-limit:
   *
   * The number of recently used files that should be displayed by default by
   * #GtkRecentChooser implementations and by the #GtkFileChooser. A value of
   * -1 means every recently used file stored.
   *
   * Since: 2.12
   *
   * Deprecated: 3.10: This setting is ignored
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-recent-files-limit",
                                                               P_("Recent Files Limit"),
                                                               P_("Number of recently used files"),
                                                               -1, G_MAXINT,
                                                               50,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_RECENT_FILES_LIMIT);

  /**
   * GtkSettings:gtk-im-module:
   *
   * Which IM (input method) module should be used by default. This is the
   * input method that will be used if the user has not explicitly chosen
   * another input method from the IM context menu.
   * This also can be a colon-separated list of input methods, which GTK+
   * will try in turn until it finds one available on the system.
   *
   * See #GtkIMContext.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-im-module",
                                                                  P_("Default IM module"),
                                                                  P_("Which IM module should be used by default"),
                                                                  NULL,
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_IM_MODULE);

  /**
   * GtkSettings:gtk-recent-files-max-age:
   *
   * The maximum age, in days, of the items inside the recently used
   * resources list. Items older than this setting will be excised
   * from the list. If set to 0, the list will always be empty; if
   * set to -1, no item will be removed.
   *
   * Since: 2.14
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-recent-files-max-age",
                                                               P_("Recent Files Max Age"),
                                                               P_("Maximum age of recently used files, in days"),
                                                               -1, G_MAXINT,
                                                               30,
                                                               GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_RECENT_FILES_MAX_AGE);

  result = settings_install_property_parser (class,
                                             g_param_spec_uint ("gtk-fontconfig-timestamp",
                                                                P_("Fontconfig configuration timestamp"),
                                                                P_("Timestamp of current fontconfig configuration"),
                                                                0, G_MAXUINT, 0,
                                                                GTK_PARAM_READWRITE),
                                             NULL);

  g_assert (result == PROP_FONTCONFIG_TIMESTAMP);

  /**
   * GtkSettings:gtk-sound-theme-name:
   *
   * The XDG sound theme to use for event sounds.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK+ itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   *
   * Since: 2.14
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-sound-theme-name",
                                                                  P_("Sound Theme Name"),
                                                                  P_("XDG sound theme name"),
                                                                  "freedesktop",
                                                                  GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_SOUND_THEME_NAME);

  /**
   * GtkSettings:gtk-enable-input-feedback-sounds:
   *
   * Whether to play event sounds as feedback to user input.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK+ itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   *
   * Since: 2.14
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-input-feedback-sounds",
                                                                   /* Translators: this means sounds that are played as feedback to user input */
                                                                   P_("Audible Input Feedback"),
                                                                   P_("Whether to play event sounds as feedback to user input"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENABLE_INPUT_FEEDBACK_SOUNDS);

  /**
   * GtkSettings:gtk-enable-event-sounds:
   *
   * Whether to play any event sounds at all.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK+ itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   *
   * Since: 2.14
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-event-sounds",
                                                                   P_("Enable Event Sounds"),
                                                                   P_("Whether to play any event sounds at all"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENABLE_EVENT_SOUNDS);

  /**
   * GtkSettings:gtk-enable-tooltips:
   *
   * Whether tooltips should be shown on widgets.
   *
   * Since: 2.14
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-tooltips",
                                                                   P_("Enable Tooltips"),
                                                                   P_("Whether tooltips should be shown on widgets"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_ENABLE_TOOLTIPS);

  /**
   * GtkSettings:gtk-toolbar-style:
   *
   * The size of icons in default toolbars.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_enum ("gtk-toolbar-style",
                                                                   P_("Toolbar style"),
                                                                   P_("Whether default toolbars have text only, text and icons, icons only, etc."),
                                                                   GTK_TYPE_TOOLBAR_STYLE,
                                                                   GTK_TOOLBAR_BOTH_HORIZ,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             gtk_rc_property_parse_enum);
  g_assert (result == PROP_TOOLBAR_STYLE);

  /**
   * GtkSettings:gtk-toolbar-icon-size:
   *
   * The size of icons in default toolbars.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_enum ("gtk-toolbar-icon-size",
                                                                   P_("Toolbar Icon Size"),
                                                                   P_("The size of icons in default toolbars."),
                                                                   GTK_TYPE_ICON_SIZE,
                                                                   GTK_ICON_SIZE_LARGE_TOOLBAR,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             gtk_rc_property_parse_enum);
  g_assert (result == PROP_TOOLBAR_ICON_SIZE);

  /**
   * GtkSettings:gtk-auto-mnemonics:
   *
   * Whether mnemonics should be automatically shown and hidden when the user
   * presses the mnemonic activator.
   *
   * Since: 2.20
   *
   * Deprecated: 3.10: This setting is ignored
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-auto-mnemonics",
                                                                   P_("Auto Mnemonics"),
                                                                   P_("Whether mnemonics should be automatically shown and hidden when the user presses the mnemonic activator."),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_AUTO_MNEMONICS);

  /**
   * GtkSettings:gtk-primary-button-warps-slider:
   *
   * Whether a click in a #GtkRange trough should scroll to the click position or
   * scroll by a single page in the respective direction.
   *
   * Since: 3.6
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-primary-button-warps-slider",
                                                                   P_("Primary button warps slider"),
                                                                   P_("Whether a primary click on the trough should warp the slider into position"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_PRIMARY_BUTTON_WARPS_SLIDER);

  /**
   * GtkSettings:gtk-visible-focus:
   *
   * Whether 'focus rectangles' should be always visible, never visible,
   * or hidden until the user starts to use the keyboard.
   *
   * Since: 3.2
   *
   * Deprecated: 3.10: This setting is ignored
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_enum ("gtk-visible-focus",
                                                                P_("Visible Focus"),
                                                                P_("Whether 'focus rectangles' should be hidden until the user starts to use the keyboard."),
                                                                GTK_TYPE_POLICY_TYPE,
                                                                GTK_POLICY_AUTOMATIC,
                                                                GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             gtk_rc_property_parse_enum);
  g_assert (result == PROP_VISIBLE_FOCUS);

  /**
   * GtkSettings:gtk-application-prefer-dark-theme:
   *
   * Whether the application prefers to use a dark theme. If a GTK+ theme
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
   *
   * Since: 3.0
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-application-prefer-dark-theme",
                                                                 P_("Application prefers a dark theme"),
                                                                 P_("Whether the application prefers to have a dark theme."),
                                                                 FALSE,
                                                                 GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_APPLICATION_PREFER_DARK_THEME);

  /**
   * GtkSettings::gtk-button-images:
   *
   * Whether images should be shown on buttons
   *
   * Since: 2.4
   *
   * Deprecated: 3.10: This setting is deprecated
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-button-images",
                                                                   P_("Show button images"),
                                                                   P_("Whether images should be shown on buttons"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_BUTTON_IMAGES);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-entry-select-on-focus",
                                                                   P_("Select on focus"),
                                                                   P_("Whether to select the contents of an entry when it is focused"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENTRY_SELECT_ON_FOCUS);

  /**
   * GtkSettings:gtk-entry-password-hint-timeout:
   *
   * How long to show the last input character in hidden
   * entries. This value is in milliseconds. 0 disables showing the
   * last char. 600 is a good value for enabling it.
   *
   * Since: 2.10
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_uint ("gtk-entry-password-hint-timeout",
                                                                P_("Password Hint Timeout"),
                                                                P_("How long to show the last input character in hidden entries"),
                                                                0, G_MAXUINT,
                                                                0,
                                                                GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENTRY_PASSWORD_HINT_TIMEOUT);

  /**
   * GtkSettings::gtk-menu-images:
   *
   * Whether images should be shown in menu items
   *
   * Deprecated: 3.10: This setting is deprecated
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-menu-images",
                                                                   P_("Show menu images"),
                                                                   P_("Whether images should be shown in menus"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_MENU_IMAGES);

  /**
   * GtkSettings:gtk-menu-bar-popup-delay:
   *
   * Delay before the submenus of a menu bar appear.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-menu-bar-popup-delay",
                                                               P_("Delay before drop down menus appear"),
                                                               P_("Delay before the submenus of a menu bar appear"),
                                                               0, G_MAXINT,
                                                               0,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_MENU_BAR_POPUP_DELAY);

  /**
   * GtkSettings:gtk-scrolled-window-placement:
   *
   * Where the contents of scrolled windows are located with respect to the
   * scrollbars, if not overridden by the scrolled window's own placement.
   *
   * Since: 2.10
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_enum ("gtk-scrolled-window-placement",
                                                                P_("Scrolled Window Placement"),
                                                                P_("Where the contents of scrolled windows are located with respect to the scrollbars, if not overridden by the scrolled window's own placement."),
                                                                GTK_TYPE_CORNER_TYPE,
                                                                GTK_CORNER_TOP_LEFT,
                                                                GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             gtk_rc_property_parse_enum);
  g_assert (result == PROP_SCROLLED_WINDOW_PLACEMENT);

  /**
   * GtkSettings:gtk-can-change-accels:
   *
   * Whether menu accelerators can be changed by pressing a key over the menu item.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-can-change-accels",
                                                                   P_("Can change accelerators"),
                                                                   P_("Whether menu accelerators can be changed by pressing a key over the menu item"),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_CAN_CHANGE_ACCELS);

  /**
   * GtkSettings:gtk-menu-popup-delay:
   *
   * Minimum time the pointer must stay over a menu item before the submenu appear.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-menu-popup-delay",
                                                               P_("Delay before submenus appear"),
                                                               P_("Minimum time the pointer must stay over a menu item before the submenu appear"),
                                                               0, G_MAXINT,
                                                               225,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_MENU_POPUP_DELAY);

  /**
   * GtkSettings:gtk-menu-popdown-delay:
   *
   * The time before hiding a submenu when the pointer is moving towards the submenu.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-menu-popdown-delay",
                                                               P_("Delay before hiding a submenu"),
                                                               P_("The time before hiding a submenu when the pointer is moving towards the submenu"),
                                                               0, G_MAXINT,
                                                               1000,
                                                               GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_MENU_POPDOWN_DELAY);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-label-select-on-focus",
                                                                   P_("Select on focus"),
                                                                   P_("Whether to select the contents of a selectable label when it is focused"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_LABEL_SELECT_ON_FOCUS);

  /**
   * GtkSettings:gtk-color-palette:
   *
   * Palette to use in the deprecated color selector.
   *
   * Deprecated: 3.10: Only used by the deprecated color selector widget.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-color-palette",
                                                                  P_("Custom palette"),
                                                                  P_("Palette to use in the color selector"),
                                                                  default_color_palette,
                                                                  GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             NULL);
  g_assert (result == PROP_COLOR_PALETTE);

  /**
   * GtkSettings:gtk-im-preedit-style:
   *
   * How to draw the input method preedit string.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_enum ("gtk-im-preedit-style",
                                                                P_("IM Preedit style"),
                                                                P_("How to draw the input method preedit string"),
                                                                GTK_TYPE_IM_PREEDIT_STYLE,
                                                                GTK_IM_PREEDIT_CALLBACK,
                                                                GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             gtk_rc_property_parse_enum);
  g_assert (result == PROP_IM_PREEDIT_STYLE);

  /**
   * GtkSettings:gtk-im-status-style:
   *
   * How to draw the input method statusbar.
   *
   * Deprecated: 3.10: This setting is ignored.
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_enum ("gtk-im-status-style",
                                                                P_("IM Status style"),
                                                                P_("How to draw the input method statusbar"),
                                                                GTK_TYPE_IM_STATUS_STYLE,
                                                                GTK_IM_STATUS_CALLBACK,
                                                                GTK_PARAM_READWRITE | G_PARAM_DEPRECATED),
                                             gtk_rc_property_parse_enum);
  g_assert (result == PROP_IM_STATUS_STYLE);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-shell-shows-app-menu",
                                                                   P_("Desktop shell shows app menu"),
                                                                   P_("Set to TRUE if the desktop environment "
                                                                      "is displaying the app menu, FALSE if "
                                                                      "the app should display it itself."),
                                                                   FALSE, GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_SHELL_SHOWS_APP_MENU);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-shell-shows-menubar",
                                                                   P_("Desktop shell shows the menubar"),
                                                                   P_("Set to TRUE if the desktop environment "
                                                                      "is displaying the menubar, FALSE if "
                                                                      "the app should display it itself."),
                                                                   FALSE, GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_SHELL_SHOWS_MENUBAR);

  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-shell-shows-desktop",
                                                                   P_("Desktop environment shows the desktop folder"),
                                                                   P_("Set to TRUE if the desktop environment "
                                                                      "is displaying the desktop folder, FALSE "
                                                                      "if not."),
                                                                   TRUE, GTK_PARAM_READWRITE),
                                             NULL);
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
   *
   * Since: 3.12
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-decoration-layout",
                                                                  P_("Decoration Layout"),
                                                                   P_("The layout for window decorations"),
                                                                   "menu:minimize,maximize,close", GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DECORATION_LAYOUT);

  /**
   * GtkSettings:gtk-titlebar-double-click:
   *
   * This setting determines the action to take when a double-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   *
   * Since: 3.14.1
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-titlebar-double-click",
                                                                  P_("Titlebar double-click action"),
                                                                   P_("The action to take on titlebar double-click"),
                                                                   "toggle-maximize", GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_TITLEBAR_DOUBLE_CLICK);

  /**
   * GtkSettings:gtk-titlebar-middle-click:
   *
   * This setting determines the action to take when a middle-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   *
   * Since: 3.14.1
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-titlebar-middle-click",
                                                                  P_("Titlebar middle-click action"),
                                                                   P_("The action to take on titlebar middle-click"),
                                                                   "none", GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_TITLEBAR_MIDDLE_CLICK);

  /**
   * GtkSettings:gtk-titlebar-right-click:
   *
   * This setting determines the action to take when a right-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   *
   * Since: 3.14.1
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-titlebar-right-click",
                                                                  P_("Titlebar right-click action"),
                                                                   P_("The action to take on titlebar right-click"),
                                                                   "menu", GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_TITLEBAR_RIGHT_CLICK);




  /**
   * GtkSettings:gtk-dialogs-use-header:
   *
   * Whether builtin GTK+ dialogs such as the file chooser, the
   * color chooser or the font chooser will use a header bar at
   * the top to show action widgets, or an action area at the bottom.
   *
   * This setting does not affect custom dialogs using GtkDialog
   * directly, or message dialogs.
   *
   * Since: 3.12
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-dialogs-use-header",
                                                                   P_("Dialogs use header bar"),
                                                                   P_("Whether builtin GTK+ dialogs should use a header bar instead of an action area."),
                                                                   FALSE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DIALOGS_USE_HEADER);

  /**
   * GtkSettings:gtk-enable-primary-paste:
   *
   * Whether a middle click on a mouse should paste the
   * 'PRIMARY' clipboard content at the cursor location.
   *
   * Since: 3.4
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-enable-primary-paste",
                                                                   P_("Enable primary paste"),
                                                                   P_("Whether a middle click on a mouse should paste the 'PRIMARY' clipboard content at the cursor location."),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ENABLE_PRIMARY_PASTE);

  /**
   * GtkSettings:gtk-recent-files-enabled:
   *
   * Whether GTK+ should keep track of items inside the recently used
   * resources list. If set to %FALSE, the list will always be empty.
   *
   * Since: 3.8
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-recent-files-enabled",
                                                                   P_("Recent Files Enabled"),
                                                                   P_("Whether GTK+ remembers recent files"),
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_RECENT_FILES_ENABLED);

  /**
   * GtkSettings:gtk-long-press-time:
   *
   * The time for a button or touch press to be considered a "long press".
   *
   * Since: 3.14
   */
  result = settings_install_property_parser (class,
                                             g_param_spec_uint ("gtk-long-press-time",
								P_("Long press time"),
								P_("Time for a button/touch press to be considered a long press (in milliseconds)"),
								0, G_MAXINT, 500,
								GTK_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_LONG_PRESS_TIME);
}

static void
gtk_settings_provider_iface_init (GtkStyleProviderIface *iface)
{
}

static GtkCssChange
gtk_settings_style_provider_get_change (GtkStyleProviderPrivate *provider,
					const GtkCssMatcher *matcher)
{
  return 0;
}


static GtkSettings *
gtk_settings_style_provider_get_settings (GtkStyleProviderPrivate *provider)
{
  return GTK_SETTINGS (provider);
}

static void
gtk_settings_provider_private_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_settings = gtk_settings_style_provider_get_settings;
  iface->get_change = gtk_settings_style_provider_get_change;
}

static void
gtk_settings_finalize (GObject *object)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = settings->priv;
  guint i;

  object_list = g_slist_remove (object_list, settings);

  for (i = 0; i < class_n_properties; i++)
    g_value_unset (&priv->property_values[i].value);
  g_free (priv->property_values);

  g_datalist_clear (&priv->queued_settings);

  settings_update_provider (priv->screen, &priv->theme_provider, NULL);
  settings_update_provider (priv->screen, &priv->key_theme_provider, NULL);
  g_clear_object (&priv->style_cascade);

  G_OBJECT_CLASS (gtk_settings_parent_class)->finalize (object);
}

GtkStyleCascade *
_gtk_settings_get_style_cascade (GtkSettings *settings)
{
  g_return_val_if_fail (GTK_IS_SETTINGS (settings), NULL);

  return settings->priv->style_cascade;
}

static void
settings_init_style (GtkSettings *settings)
{
  static GtkCssProvider *css_provider = NULL;
  GtkSettingsPrivate *priv = settings->priv;

  /* Add provider for user file */
  if (G_UNLIKELY (!css_provider))
    {
      gchar *css_path;

      css_provider = gtk_css_provider_new ();
      css_path = g_build_filename (g_get_user_config_dir (),
                                   "gtk-3.0",
                                   "gtk.css",
                                   NULL);

      if (g_file_test (css_path,
                       G_FILE_TEST_IS_REGULAR))
        gtk_css_provider_load_from_path (css_provider, css_path, NULL);

      g_free (css_path);
    }

  _gtk_style_cascade_add_provider (priv->style_cascade,
                                   GTK_STYLE_PROVIDER (css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

  _gtk_style_cascade_add_provider (priv->style_cascade,
                                   GTK_STYLE_PROVIDER (settings),
                                   GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  _gtk_style_cascade_add_provider (priv->style_cascade,
                                   GTK_STYLE_PROVIDER (settings->priv->theme_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  settings_update_theme (settings);
  settings_update_key_theme (settings);
}

/**
 * gtk_settings_get_for_screen:
 * @screen: a #GdkScreen.
 *
 * Gets the #GtkSettings object for @screen, creating it if necessary.
 *
 * Returns: (transfer none): a #GtkSettings object.
 *
 * Since: 2.2
 */
GtkSettings*
gtk_settings_get_for_screen (GdkScreen *screen)
{
  GtkSettings *settings;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  settings = g_object_get_data (G_OBJECT (screen), "gtk-settings");
  if (!settings)
    {
#ifdef GDK_WINDOWING_QUARTZ
      if (GDK_IS_QUARTZ_SCREEN (screen))
        {
          settings = g_object_new (GTK_TYPE_SETTINGS,
                                   "gtk-key-theme-name", "Mac",
                                   "gtk-shell-shows-app-menu", TRUE,
                                   "gtk-shell-shows-menubar", TRUE,
                                   NULL);
        }
      else
#endif
#ifdef GDK_WINDOWING_BROADWAY
      if (GDK_IS_BROADWAY_DISPLAY (gdk_screen_get_display (screen)))
        {
          settings = g_object_new (GTK_TYPE_SETTINGS,
                                   "gtk-im-module", "broadway",
                                   NULL);
        }
      else
#endif
        settings = g_object_new (GTK_TYPE_SETTINGS, NULL);
      settings->priv->screen = screen;
      g_object_set_data_full (G_OBJECT (screen), I_("gtk-settings"),
                              settings, g_object_unref);

      settings_init_style (settings);
      settings_update_modules (settings);
      settings_update_double_click (settings);
      settings_update_cursor_theme (settings);
      settings_update_resolution (settings);
      settings_update_font_options (settings);
    }

  return settings;
}

/**
 * gtk_settings_get_default:
 *
 * Gets the #GtkSettings object for the default GDK screen, creating
 * it if necessary. See gtk_settings_get_for_screen().
 *
 * Returns: (transfer none): a #GtkSettings object. If there is no default
 *  screen, then returns %NULL.
 **/
GtkSettings*
gtk_settings_get_default (void)
{
  GdkScreen *screen = gdk_screen_get_default ();

  if (screen)
    return gtk_settings_get_for_screen (screen);
  else
    return NULL;
}

static void
gtk_settings_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = settings->priv;

  g_value_copy (value, &priv->property_values[property_id - 1].value);
  priv->property_values[property_id - 1].source = GTK_SETTINGS_SOURCE_APPLICATION;
}

static void
gtk_settings_get_property (GObject     *object,
                           guint        property_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = settings->priv;
  GType value_type = G_VALUE_TYPE (value);
  GType fundamental_type = G_TYPE_FUNDAMENTAL (value_type);

  /* handle internal properties */
  switch (property_id)
    {
    case PROP_COLOR_HASH:
      g_value_take_boxed (value, g_hash_table_new (g_str_hash, g_str_equal));
      return;
    default: ;
    }

  /* For enums and strings, we need to get the value as a string,
   * not as an int, since we support using names/nicks as the setting
   * value.
   */
  if ((g_value_type_transformable (G_TYPE_INT, value_type) &&
       !(fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)) ||
      g_value_type_transformable (G_TYPE_STRING, G_VALUE_TYPE (value)) ||
      g_value_type_transformable (GDK_TYPE_RGBA, G_VALUE_TYPE (value)))
    {
      if (priv->property_values[property_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION ||
          !gdk_screen_get_setting (priv->screen, pspec->name, value))
        g_value_copy (&priv->property_values[property_id - 1].value, value);
      else
        g_param_value_validate (pspec, value);
    }
  else
    {
      GValue val = G_VALUE_INIT;

      /* Try to get xsetting as a string and parse it. */

      g_value_init (&val, G_TYPE_STRING);

      if (priv->property_values[property_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION ||
          !gdk_screen_get_setting (priv->screen, pspec->name, &val))
        {
          g_value_copy (&priv->property_values[property_id - 1].value, value);
        }
      else
        {
          GValue tmp_value = G_VALUE_INIT;
          GValue gstring_value = G_VALUE_INIT;
          GtkRcPropertyParser parser = (GtkRcPropertyParser) g_param_spec_get_qdata (pspec, quark_property_parser);

          g_value_init (&gstring_value, G_TYPE_GSTRING);
          g_value_take_boxed (&gstring_value,
                              g_string_new (g_value_get_string (&val)));

          g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));

          if (parser && _gtk_settings_parse_convert (parser, &gstring_value,
                                                     pspec, &tmp_value))
            {
              g_value_copy (&tmp_value, value);
              g_param_value_validate (pspec, value);
            }
          else
            {
              g_value_copy (&priv->property_values[property_id - 1].value, value);
            }

          g_value_unset (&gstring_value);
          g_value_unset (&tmp_value);
        }

      g_value_unset (&val);
    }
}

static void
settings_invalidate_style (GtkSettings *settings)
{
  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (settings));
}

static void
gtk_settings_notify (GObject    *object,
                     GParamSpec *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GtkSettingsPrivate *priv = settings->priv;
  guint property_id = pspec->param_id;

  if (priv->screen == NULL) /* initialization */
    return;

  switch (property_id)
    {
    case PROP_MODULES:
      settings_update_modules (settings);
      break;
    case PROP_DOUBLE_CLICK_TIME:
    case PROP_DOUBLE_CLICK_DISTANCE:
      settings_update_double_click (settings);
      break;
    case PROP_FONT_NAME:
      settings_invalidate_style (settings);
      gtk_style_context_reset_widgets (priv->screen);
      break;
    case PROP_KEY_THEME_NAME:
      settings_update_key_theme (settings);
      break;
    case PROP_THEME_NAME:
    case PROP_APPLICATION_PREFER_DARK_THEME:
      settings_update_theme (settings);
      break;
    case PROP_XFT_DPI:
      settings_update_resolution (settings);
      /* This is a hack because with gtk_rc_reset_styles() doesn't get
       * widgets with gtk_widget_style_set(), and also causes more
       * recomputation than necessary.
       */
      gtk_style_context_reset_widgets (priv->screen);
      break;
    case PROP_XFT_ANTIALIAS:
    case PROP_XFT_HINTING:
    case PROP_XFT_HINTSTYLE:
    case PROP_XFT_RGBA:
      settings_update_font_options (settings);
      gtk_style_context_reset_widgets (priv->screen);
      break;
    case PROP_FONTCONFIG_TIMESTAMP:
      if (settings_update_fontconfig (settings))
        gtk_style_context_reset_widgets (priv->screen);
      break;
    case PROP_ENABLE_ANIMATIONS:
      gtk_style_context_reset_widgets (priv->screen);
      break;
    case PROP_CURSOR_THEME_NAME:
    case PROP_CURSOR_THEME_SIZE:
      settings_update_cursor_theme (settings);
      break;
    }
}

gboolean
_gtk_settings_parse_convert (GtkRcPropertyParser parser,
                             const GValue       *src_value,
                             GParamSpec         *pspec,
                             GValue             *dest_value)
{
  gboolean success = FALSE;

  g_return_val_if_fail (G_VALUE_HOLDS (dest_value, G_PARAM_SPEC_VALUE_TYPE (pspec)), FALSE);

  if (parser)
    {
      GString *gstring;
      gboolean free_gstring = TRUE;

      if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
        {
          gstring = g_value_get_boxed (src_value);
          free_gstring = FALSE;
        }
      else if (G_VALUE_HOLDS_LONG (src_value))
        {
          gstring = g_string_new (NULL);
          g_string_append_printf (gstring, "%ld", g_value_get_long (src_value));
        }
      else if (G_VALUE_HOLDS_DOUBLE (src_value))
        {
          gstring = g_string_new (NULL);
          g_string_append_printf (gstring, "%f", g_value_get_double (src_value));
        }
      else if (G_VALUE_HOLDS_STRING (src_value))
        {
          gchar *tstr = g_strescape (g_value_get_string (src_value), NULL);

          gstring = g_string_new (NULL);
          g_string_append_c (gstring, '\"');
          g_string_append (gstring, tstr);
          g_string_append_c (gstring, '\"');
          g_free (tstr);
        }
      else
        {
          g_return_val_if_fail (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING), FALSE);
          gstring = NULL; /* silence compiler */
        }

      success = (parser (pspec, gstring, dest_value) &&
                 !g_param_value_validate (pspec, dest_value));

      if (free_gstring)
        g_string_free (gstring, TRUE);
    }
  else if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
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
  GtkSettingsPrivate *priv = settings->priv;
  GValue tmp_value = G_VALUE_INIT;
  GtkRcPropertyParser parser = (GtkRcPropertyParser) g_param_spec_get_qdata (pspec, quark_property_parser);

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (_gtk_settings_parse_convert (parser, &qvalue->public.value,
                                   pspec, &tmp_value))
    {
      if (priv->property_values[pspec->param_id - 1].source <= qvalue->source)
        {
          g_value_copy (&tmp_value, &priv->property_values[pspec->param_id - 1].value);
          priv->property_values[pspec->param_id - 1].source = qvalue->source;
          g_object_notify (G_OBJECT (settings), g_param_spec_get_name (pspec));
        }

    }
  else
    {
      gchar *debug = g_strdup_value_contents (&qvalue->public.value);

      g_message ("%s: failed to retrieve property `%s' of type `%s' from rc file value \"%s\" of type `%s'",
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
                                  GParamSpec         *pspec,
                                  GtkRcPropertyParser parser)
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
      if (strcmp (g_param_spec_get_name (pspec), "color-hash") == 0)
        {
          break;
        }
      /* fall through */
    default:
      if (!parser)
        {
          g_warning (G_STRLOC ": parser needs to be specified for property \"%s\" of type `%s'",
                     pspec->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
          return 0;
        }
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
  g_param_spec_set_qdata (pspec, quark_property_parser, (gpointer) parser);

  for (node = object_list; node; node = node->next)
    {
      GtkSettings *settings = node->data;
      GtkSettingsPrivate *priv = settings->priv;
      GtkSettingsValuePrivate *qvalue;

      priv->property_values = g_renew (GtkSettingsPropertyValue, priv->property_values, class_n_properties);
      priv->property_values[class_n_properties - 1].value.g_type = 0;
      g_value_init (&priv->property_values[class_n_properties - 1].value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, &priv->property_values[class_n_properties - 1].value);
      priv->property_values[class_n_properties - 1].source = GTK_SETTINGS_SOURCE_DEFAULT;
      g_object_notify (G_OBJECT (settings), pspec->name);

      qvalue = g_datalist_get_data (&priv->queued_settings, pspec->name);
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

GtkRcPropertyParser
_gtk_rc_property_parser_from_type (GType type)
{
  if (type == g_type_from_name ("GdkColor"))
    return gtk_rc_property_parse_color;
  else if (type == GTK_TYPE_REQUISITION)
    return gtk_rc_property_parse_requisition;
  else if (type == GTK_TYPE_BORDER)
    return gtk_rc_property_parse_border;
  else if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_ENUM && G_TYPE_IS_DERIVED (type))
    return gtk_rc_property_parse_enum;
  else if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_FLAGS && G_TYPE_IS_DERIVED (type))
    return gtk_rc_property_parse_flags;
  else
    return NULL;
}

/**
 * gtk_settings_install_property:
 * @pspec:
 *
 * Deprecated: 3.16: This function is not useful outside GTK+.
 */
void
gtk_settings_install_property (GParamSpec *pspec)
{
  static GtkSettingsClass *klass = NULL;

  GtkRcPropertyParser parser;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if (! klass)
    klass = g_type_class_ref (GTK_TYPE_SETTINGS);

  parser = _gtk_rc_property_parser_from_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  settings_install_property_parser (klass, pspec, parser);
}

/**
 * gtk_settings_install_property_parser:
 * @pspec:
 * @parser: (scope call):
 *
 * Deprecated: 3.16: This function is not useful outside GTK+.
 */
void
gtk_settings_install_property_parser (GParamSpec          *pspec,
                                      GtkRcPropertyParser  parser)
{
  static GtkSettingsClass *klass = NULL;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (parser != NULL);

  if (! klass)
    klass = g_type_class_ref (GTK_TYPE_SETTINGS);

  settings_install_property_parser (klass, pspec, parser);
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
  GtkSettingsPrivate *priv = settings->priv;
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

  qvalue = g_datalist_id_get_data (&priv->queued_settings, name_quark);
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

/**
 * gtk_settings_set_property_value:
 * @settings:
 * @name:
 * @svalue:
 *
 * Deprecated: 3.16: Use g_object_set() instead.
 */
void
gtk_settings_set_property_value (GtkSettings            *settings,
                                 const gchar            *name,
                                 const GtkSettingsValue *svalue)
{
  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (svalue != NULL);

  gtk_settings_set_property_value_internal (settings, name, svalue,
                                            GTK_SETTINGS_SOURCE_APPLICATION);
}

void
_gtk_settings_set_property_value_from_rc (GtkSettings            *settings,
                                          const gchar            *prop_name,
                                          const GtkSettingsValue *new_value)
{
  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (prop_name != NULL);
  g_return_if_fail (new_value != NULL);

  gtk_settings_set_property_value_internal (settings, prop_name, new_value,
                                            GTK_SETTINGS_SOURCE_THEME);
}

/**
 * gtk_settings_set_string_property:
 * @settings:
 * @name:
 * @v_string:
 * @origin:
 *
 * Deprecated: 3.16: Use g_object_set() instead.
 */
void
gtk_settings_set_string_property (GtkSettings *settings,
                                  const gchar *name,
                                  const gchar *v_string,
                                  const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (v_string != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_STRING);
  g_value_set_static_string (&svalue.value, v_string);
  gtk_settings_set_property_value_internal (settings, name, &svalue,
                                            GTK_SETTINGS_SOURCE_APPLICATION);
  g_value_unset (&svalue.value);
}

/**
 * gtk_settings_set_long_property:
 * @settings:
 * @name:
 * @v_long:
 * @origin:
 *
 * Deprecated: 3.16: Use g_object_set() instead.
 */
void
gtk_settings_set_long_property (GtkSettings *settings,
                                const gchar *name,
                                glong        v_long,
                                const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_LONG);
  g_value_set_long (&svalue.value, v_long);
  gtk_settings_set_property_value_internal (settings, name, &svalue,
                                            GTK_SETTINGS_SOURCE_APPLICATION);
  g_value_unset (&svalue.value);
}

/**
 * gtk_settings_set_double_property:
 * @settings:
 * @name:
 * @v_double:
 * @origin:
 *
 * Deprecated: 3.16: Use g_object_set() instead.
 */
void
gtk_settings_set_double_property (GtkSettings *settings,
                                  const gchar *name,
                                  gdouble      v_double,
                                  const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_DOUBLE);
  g_value_set_double (&svalue.value, v_double);
  gtk_settings_set_property_value_internal (settings, name, &svalue,
                                            GTK_SETTINGS_SOURCE_APPLICATION);
  g_value_unset (&svalue.value);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * gtk_rc_property_parse_color:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold #GdkColor values.
 *
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses a
 * color given either by its name or in the form
 * `{ red, green, blue }` where red, green and
 * blue are integers between 0 and 65535 or floating-point numbers
 * between 0 and 1.
 *
 * Returns: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GdkColor.
 **/
gboolean
gtk_rc_property_parse_color (const GParamSpec *pspec,
                             const GString    *gstring,
                             GValue           *property_value)
{
  GdkColor color = { 0, 0, 0, 0, };
  GScanner *scanner;
  gboolean success;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS (property_value, GDK_TYPE_COLOR), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);
  if (gtk_rc_parse_color (scanner, &color) == G_TOKEN_NONE &&
      g_scanner_get_next_token (scanner) == G_TOKEN_EOF)
    {
      g_value_set_boxed (property_value, &color);
      success = TRUE;
    }
  else
    success = FALSE;
  g_scanner_destroy (scanner);

  return success;
}

/**
 * gtk_rc_property_parse_enum:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold enum values.
 *
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses a single
 * enumeration value.
 *
 * The enumeration value can be specified by its name, its nickname or
 * its numeric value. For consistency with flags parsing, the value
 * may be surrounded by parentheses.
 *
 * Returns: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GEnumValue.
 **/
gboolean
gtk_rc_property_parse_enum (const GParamSpec *pspec,
                            const GString    *gstring,
                            GValue           *property_value)
{
  gboolean need_closing_brace = FALSE, success = FALSE;
  GScanner *scanner;
  GEnumValue *enum_value = NULL;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_ENUM (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  /* we just want to parse _one_ value, but for consistency with flags parsing
   * we support optional parenthesis
   */
  g_scanner_get_next_token (scanner);
  if (scanner->token == '(')
    {
      need_closing_brace = TRUE;
      g_scanner_get_next_token (scanner);
    }
  if (scanner->token == G_TOKEN_IDENTIFIER)
    {
      GEnumClass *class = G_PARAM_SPEC_ENUM (pspec)->enum_class;

      enum_value = g_enum_get_value_by_name (class, scanner->value.v_identifier);
      if (!enum_value)
        enum_value = g_enum_get_value_by_nick (class, scanner->value.v_identifier);
      if (enum_value)
        {
          g_value_set_enum (property_value, enum_value->value);
          success = TRUE;
        }
    }
  else if (scanner->token == G_TOKEN_INT)
    {
      g_value_set_enum (property_value, scanner->value.v_int);
      success = TRUE;
    }
  if (need_closing_brace && g_scanner_get_next_token (scanner) != ')')
    success = FALSE;
  if (g_scanner_get_next_token (scanner) != G_TOKEN_EOF)
    success = FALSE;

  g_scanner_destroy (scanner);

  return success;
}

static guint
parse_flags_value (GScanner    *scanner,
                   GFlagsClass *class,
                   guint       *number)
{
  g_scanner_get_next_token (scanner);
  if (scanner->token == G_TOKEN_IDENTIFIER)
    {
      GFlagsValue *flags_value;

      flags_value = g_flags_get_value_by_name (class, scanner->value.v_identifier);
      if (!flags_value)
        flags_value = g_flags_get_value_by_nick (class, scanner->value.v_identifier);
      if (flags_value)
        {
          *number |= flags_value->value;
          return G_TOKEN_NONE;
        }
    }
  else if (scanner->token == G_TOKEN_INT)
    {
      *number |= scanner->value.v_int;
      return G_TOKEN_NONE;
    }
  return G_TOKEN_IDENTIFIER;
}

/**
 * gtk_rc_property_parse_flags:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold flags values.
 *
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses flags.
 *
 * Flags can be specified by their name, their nickname or
 * numerically. Multiple flags can be specified in the form
 * `"( flag1 | flag2 | ... )"`.
 *
 * Returns: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting flags value.
 **/
gboolean
gtk_rc_property_parse_flags (const GParamSpec *pspec,
                             const GString    *gstring,
                             GValue           *property_value)
{
  GFlagsClass *class;
   gboolean success = FALSE;
  GScanner *scanner;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (property_value), FALSE);

  class = G_PARAM_SPEC_FLAGS (pspec)->flags_class;
  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  /* parse either a single flags value or a "\( ... [ \| ... ] \)" compound */
  if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER ||
      scanner->next_token == G_TOKEN_INT)
    {
      guint token, flags_value = 0;

      token = parse_flags_value (scanner, class, &flags_value);

      if (token == G_TOKEN_NONE && g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
        {
          success = TRUE;
          g_value_set_flags (property_value, flags_value);
        }

    }
  else if (g_scanner_get_next_token (scanner) == '(')
    {
      guint token, flags_value = 0;

      /* parse first value */
      token = parse_flags_value (scanner, class, &flags_value);

      /* parse nth values, preceeded by '|' */
      while (token == G_TOKEN_NONE && g_scanner_get_next_token (scanner) == '|')
        token = parse_flags_value (scanner, class, &flags_value);

      /* done, last token must have closed expression */
      if (token == G_TOKEN_NONE && scanner->token == ')' &&
          g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
        {
          g_value_set_flags (property_value, flags_value);
          success = TRUE;
        }
    }
  g_scanner_destroy (scanner);

  return success;
}

static gboolean
get_braced_int (GScanner *scanner,
                gboolean  first,
                gboolean  last,
                gint     *value)
{
  if (first)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != '{')
        return FALSE;
    }

  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_INT)
    return FALSE;

  *value = scanner->value.v_int;

  if (last)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != '}')
        return FALSE;
    }
  else
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != ',')
        return FALSE;
    }

  return TRUE;
}

/**
 * gtk_rc_property_parse_requisition:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold boxed values.
 *
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses a
 * requisition in the form
 * `"{ width, height }"` for integers %width and %height.
 *
 * Returns: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GtkRequisition.
 **/
gboolean
gtk_rc_property_parse_requisition  (const GParamSpec *pspec,
                                    const GString    *gstring,
                                    GValue           *property_value)
{
  GtkRequisition requisition;
  GScanner *scanner;
  gboolean success = FALSE;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  if (get_braced_int (scanner, TRUE, FALSE, &requisition.width) &&
      get_braced_int (scanner, FALSE, TRUE, &requisition.height))
    {
      g_value_set_boxed (property_value, &requisition);
      success = TRUE;
    }

  g_scanner_destroy (scanner);

  return success;
}

/**
 * gtk_rc_property_parse_border:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold boxed values.
 *
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses
 * borders in the form
 * `"{ left, right, top, bottom }"` for integers
 * left, right, top and bottom.
 *
 * Returns: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GtkBorder.
 **/
gboolean
gtk_rc_property_parse_border (const GParamSpec *pspec,
                              const GString    *gstring,
                              GValue           *property_value)
{
  GtkBorder border;
  GScanner *scanner;
  gboolean success = FALSE;
  int left, right, top, bottom;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  if (get_braced_int (scanner, TRUE, FALSE, &left) &&
      get_braced_int (scanner, FALSE, FALSE, &right) &&
      get_braced_int (scanner, FALSE, FALSE, &top) &&
      get_braced_int (scanner, FALSE, TRUE, &bottom))
    {
      border.left = left;
      border.right = right;
      border.top = top;
      border.bottom = bottom;
      g_value_set_boxed (property_value, &border);
      success = TRUE;
    }

  g_scanner_destroy (scanner);

  return success;
}

G_GNUC_END_IGNORE_DEPRECATIONS

void
_gtk_settings_handle_event (GdkEventSetting *event)
{
  GdkScreen *screen;
  GtkSettings *settings;
  GParamSpec *pspec;

  screen = gdk_window_get_screen (event->window);
  settings = gtk_settings_get_for_screen (screen);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), event->name);

  if (pspec)
    g_object_notify (G_OBJECT (settings), pspec->name);
}

static void
reset_rc_values_foreach (GQuark   key_id,
                         gpointer data,
                         gpointer user_data)
{
  GtkSettingsValuePrivate *qvalue = data;
  GSList **to_reset = user_data;

  if (qvalue->source == GTK_SETTINGS_SOURCE_THEME)
    *to_reset = g_slist_prepend (*to_reset, GUINT_TO_POINTER (key_id));
}

void
_gtk_settings_reset_rc_values (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = settings->priv;
  GSList *to_reset = NULL;
  GSList *tmp_list;
  GParamSpec **pspecs, **p;
  gint i;

  /* Remove any queued settings */
  g_datalist_foreach (&priv->queued_settings,
                      reset_rc_values_foreach,
                      &to_reset);

  for (tmp_list = to_reset; tmp_list; tmp_list = tmp_list->next)
    {
      GQuark key_id = GPOINTER_TO_UINT (tmp_list->data);
      g_datalist_id_remove_data (&priv->queued_settings, key_id);
    }

   g_slist_free (to_reset);

  /* Now reset the active settings
   */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), NULL);
  i = 0;

  g_object_freeze_notify (G_OBJECT (settings));
  for (p = pspecs; *p; p++)
    {
      if (priv->property_values[i].source == GTK_SETTINGS_SOURCE_THEME)
        {
          GParamSpec *pspec = *p;

          g_param_value_set_default (pspec, &priv->property_values[i].value);
          g_object_notify (G_OBJECT (settings), pspec->name);
        }
      i++;
    }
  g_object_thaw_notify (G_OBJECT (settings));
  g_free (pspecs);
}

static void
settings_update_double_click (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = settings->priv;

  if (gdk_screen_get_number (priv->screen) == 0)
    {
      GdkDisplay *display = gdk_screen_get_display (priv->screen);
      gint double_click_time;
      gint double_click_distance;

      g_object_get (settings,
                    "gtk-double-click-time", &double_click_time,
                    "gtk-double-click-distance", &double_click_distance,
                    NULL);

      gdk_display_set_double_click_time (display, double_click_time);
      gdk_display_set_double_click_distance (display, double_click_distance);
    }
}

static void
settings_update_modules (GtkSettings *settings)
{
  gchar *modules;

  g_object_get (settings,
                "gtk-modules", &modules,
                NULL);

  _gtk_modules_settings_changed (settings, modules);

  g_free (modules);
}

static void
settings_update_cursor_theme (GtkSettings *settings)
{
  gchar *theme = NULL;
  gint size = 0;
#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
  GdkDisplay *display = gdk_screen_get_display (settings->priv->screen);
#endif

  g_object_get (settings,
                "gtk-cursor-theme-name", &theme,
                "gtk-cursor-theme-size", &size,
                NULL);
  if (theme == NULL)
    return;
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    gdk_x11_display_set_cursor_theme (display, theme, size);
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    gdk_wayland_display_set_cursor_theme (display, theme, size);
  else
#endif
    g_warning ("GtkSettings Cursor Theme: Unsupported GDK backend\n");
  g_free (theme);
}

static void
settings_update_font_options (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = settings->priv;
  gint hinting;
  gchar *hint_style_str;
  cairo_hint_style_t hint_style = CAIRO_HINT_STYLE_NONE;
  gint antialias;
  cairo_antialias_t antialias_mode = CAIRO_ANTIALIAS_GRAY;
  gchar *rgba_str;
  cairo_subpixel_order_t subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
  cairo_font_options_t *options;

  g_object_get (settings,
                "gtk-xft-antialias", &antialias,
                "gtk-xft-hinting", &hinting,
                "gtk-xft-hintstyle", &hint_style_str,
                "gtk-xft-rgba", &rgba_str,
                NULL);

  options = cairo_font_options_create ();

  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);

  if (hinting >= 0 && !hinting)
    {
      hint_style = CAIRO_HINT_STYLE_NONE;
    }
  else if (hint_style_str)
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

  g_free (hint_style_str);

  cairo_font_options_set_hint_style (options, hint_style);

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

      g_free (rgba_str);
    }

  cairo_font_options_set_subpixel_order (options, subpixel_order);

  if (antialias >= 0 && !antialias)
    antialias_mode = CAIRO_ANTIALIAS_NONE;
  else if (subpixel_order != CAIRO_SUBPIXEL_ORDER_DEFAULT)
    antialias_mode = CAIRO_ANTIALIAS_SUBPIXEL;
  else if (antialias >= 0)
    antialias_mode = CAIRO_ANTIALIAS_GRAY;

  cairo_font_options_set_antialias (options, antialias_mode);

  gdk_screen_set_font_options (priv->screen, options);

  cairo_font_options_destroy (options);
}

static gboolean
settings_update_fontconfig (GtkSettings *settings)
{
#ifdef GDK_WINDOWING_X11
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
          pango_fc_font_map_cache_clear (PANGO_FC_FONT_MAP (fontmap));
          if (FcInitReinitialize ())
            update_needed = TRUE;
        }

      last_update_timestamp = timestamp;
      last_update_needed = update_needed;
    }

  return last_update_needed;
#else
  return FALSE;
#endif /* GDK_WINDOWING_X11 */
}

static void
settings_update_resolution (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = settings->priv;
  gint dpi_int;
  gdouble dpi;
  const char *scale_env;
  double scale;

  /* We handle this here in the case that the dpi was set on the GtkSettings
   * object by the application. Other cases are handled in
   * xsettings-client.c:read-settings(). See comment there for the rationale.
   */
  if (priv->property_values[PROP_XFT_DPI - 1].source == GTK_SETTINGS_SOURCE_APPLICATION)
    {
      g_object_get (settings,
                    "gtk-xft-dpi", &dpi_int,
                    NULL);

      if (dpi_int > 0)
        dpi = dpi_int / 1024.;
      else
        dpi = -1.;

      scale_env = g_getenv ("GDK_DPI_SCALE");
      if (scale_env)
        {
          scale = g_ascii_strtod (scale_env, NULL);
          if (scale != 0 && dpi > 0)
            dpi *= scale;
        }

      gdk_screen_set_resolution (priv->screen, dpi);
    }
}

static void
settings_update_provider (GdkScreen       *screen,
                          GtkCssProvider **old,
                          GtkCssProvider  *new)
{
  if (screen != NULL && *old != new)
    {
      if (*old)
        {
          gtk_style_context_remove_provider_for_screen (screen,
                                                        GTK_STYLE_PROVIDER (*old));
          g_object_unref (*old);
          *old = NULL;
        }

      if (new)
        {
          gtk_style_context_add_provider_for_screen (screen,
                                                     GTK_STYLE_PROVIDER (new),
                                                     GTK_STYLE_PROVIDER_PRIORITY_THEME);
          *old = g_object_ref (new);
        }
    }
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
  GtkSettingsPrivate *priv = settings->priv;
  gchar *theme_name;
  gchar *theme_variant;
  gchar *theme_dir;
  gchar *path;

  get_theme_name (settings, &theme_name, &theme_variant);

  _gtk_css_provider_load_named (priv->theme_provider,
                                theme_name, theme_variant);

  /* reload per-theme settings */
  theme_dir = _gtk_css_provider_get_theme_dir ();
  path = g_build_filename (theme_dir, theme_name, "gtk-3.0", "settings.ini", NULL);

  if (g_file_test (path, G_FILE_TEST_EXISTS))
    gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_THEME);

  g_free (theme_name);
  g_free (theme_variant);
  g_free (theme_dir);
  g_free (path);
}

static void
settings_update_key_theme (GtkSettings *settings)
{
  GtkSettingsPrivate *priv = settings->priv;
  GtkCssProvider *provider = NULL;
  gchar *key_theme_name;

  g_object_get (settings,
                "gtk-key-theme-name", &key_theme_name,
                NULL);

  if (key_theme_name && *key_theme_name)
    provider = gtk_css_provider_get_named (key_theme_name, "keys");

  settings_update_provider (priv->screen, &priv->key_theme_provider, provider);
  g_free (key_theme_name);
}


GdkScreen *
_gtk_settings_get_screen (GtkSettings *settings)
{
  return settings->priv->screen;
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

GtkSettingsSource
_gtk_settings_get_setting_source (GtkSettings *settings,
                                  const gchar *name)
{
  GtkSettingsPrivate *priv = settings->priv;
  GParamSpec *pspec;
  GValue val = G_VALUE_INIT;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);
  if (!pspec)
    return GTK_SETTINGS_SOURCE_DEFAULT;

  if (priv->property_values[pspec->param_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION)
    return GTK_SETTINGS_SOURCE_APPLICATION;

  /* We never actually store GTK_SETTINGS_SOURCE_XSETTING as a source
   * value in the property_values array - we just try to load the xsetting,
   * and use it when available. Do the same here.
   */
  g_value_init (&val, G_TYPE_STRING);
  if (gdk_screen_get_setting (priv->screen, pspec->name, &val))
    {
      g_value_unset (&val);
      return GTK_SETTINGS_SOURCE_XSETTING; 
    }

  return priv->property_values[pspec->param_id - 1].source;  
}
