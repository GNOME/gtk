/*
 * Copyright © 2025 Red Hat, Inc.
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

#include "gdksettings-wayland.h"
#include "gdk/gdkprivate.h"

#include "wm-button-layout-translation.h"

/* {{{ Settings handling */

typedef enum
{
  GSD_FONT_ANTIALIASING_MODE_NONE,
  GSD_FONT_ANTIALIASING_MODE_GRAYSCALE,
  GSD_FONT_ANTIALIASING_MODE_RGBA
} GsdFontAntialiasingMode;

static int
get_antialiasing (const char *s)
{
  const char *names[] = { "none", "grayscale", "rgba" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_HINTING_NONE,
  GSD_FONT_HINTING_SLIGHT,
  GSD_FONT_HINTING_MEDIUM,
  GSD_FONT_HINTING_FULL
} GsdFontHinting;

static int
get_hinting (const char *s)
{
  const char *names[] = { "none", "slight", "medium", "full" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_RGBA_ORDER_RGBA,
  GSD_FONT_RGBA_ORDER_RGB,
  GSD_FONT_RGBA_ORDER_BGR,
  GSD_FONT_RGBA_ORDER_VRGB,
  GSD_FONT_RGBA_ORDER_VBGR
} GsdFontRgbaOrder;

static int
get_order (const char *s)
{
  const char *names[] = { "rgba", "rgb", "bgr", "vrgb", "vbgr" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

static int
get_font_rendering (const char *s)
{
  const char *names[] = { "automatic", "manual" };

  for (int i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

/* When using the Settings portal, we cache the value in
 * the fallback member, and we ignore the valid field
 */
typedef struct _TranslationEntry TranslationEntry;
struct _TranslationEntry {
  gboolean valid;
  const char *schema;
  const char *key;
  const char *setting;
  GType type;
  union {
    const char *s;
    int i;
    gboolean b;
  } fallback;
};

static TranslationEntry * find_translation_entry_by_schema (const char *schema,
                                                            const char *key);

static void
update_xft_settings (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GsdFontAntialiasingMode antialiasing;
  GsdFontHinting hinting;
  GsdFontRgbaOrder order;
  gboolean use_rgba = FALSE;
  GsdXftSettings xft_settings;
  double dpi;

  if (display_wayland->settings_portal)
    {
      TranslationEntry *entry;

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");
      g_assert (entry);

      if (entry->valid)
        {
          antialiasing = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-hinting");
          g_assert (entry);
          hinting = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-rgba-order");
          g_assert (entry);
          order = entry->fallback.i;
        }
      else
        {
          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "antialiasing");
          g_assert (entry);
          antialiasing = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "hinting");
          g_assert (entry);
          hinting = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "rgba-order");
          g_assert (entry);
          order = entry->fallback.i;
        }

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "text-scaling-factor");
      g_assert (entry);
      dpi = 96.0 * entry->fallback.i / 65536.0 * 1024; /* Xft wants 1/1024th of an inch */
    }
  else
    {
      antialiasing = GSD_FONT_ANTIALIASING_MODE_GRAYSCALE;
      hinting = GSD_FONT_HINTING_MEDIUM;
      order = GSD_FONT_RGBA_ORDER_RGB;
      dpi = 96.0 * 1024;
    }

  xft_settings.hinting = (hinting != GSD_FONT_HINTING_NONE);
  xft_settings.dpi = dpi;

  switch (hinting)
    {
    case GSD_FONT_HINTING_NONE:
      xft_settings.hintstyle = "hintnone";
      break;
    case GSD_FONT_HINTING_SLIGHT:
      xft_settings.hintstyle = "hintslight";
      break;
    case GSD_FONT_HINTING_MEDIUM:
      xft_settings.hintstyle = "hintmedium";
      break;
    case GSD_FONT_HINTING_FULL:
    default:
      xft_settings.hintstyle = "hintfull";
      break;
    }

  switch (order)
    {
    case GSD_FONT_RGBA_ORDER_RGBA:
      xft_settings.rgba = "rgba";
      break;
    default:
    case GSD_FONT_RGBA_ORDER_RGB:
      xft_settings.rgba = "rgb";
      break;
    case GSD_FONT_RGBA_ORDER_BGR:
      xft_settings.rgba = "bgr";
      break;
    case GSD_FONT_RGBA_ORDER_VRGB:
      xft_settings.rgba = "vrgb";
      break;
    case GSD_FONT_RGBA_ORDER_VBGR:
      xft_settings.rgba = "vbgr";
      break;
    }

  switch (antialiasing)
   {
   default:
   case GSD_FONT_ANTIALIASING_MODE_NONE:
     xft_settings.antialias = FALSE;
     break;
   case GSD_FONT_ANTIALIASING_MODE_GRAYSCALE:
     xft_settings.antialias = TRUE;
     break;
   case GSD_FONT_ANTIALIASING_MODE_RGBA:
     xft_settings.antialias = TRUE;
     use_rgba = TRUE;
   }

  if (!use_rgba)
    xft_settings.rgba = "none";

  if (display_wayland->xft_settings.antialias != xft_settings.antialias)
    {
      display_wayland->xft_settings.antialias = xft_settings.antialias;
      gdk_display_setting_changed (display, "gtk-xft-antialias");
    }

  if (display_wayland->xft_settings.hinting != xft_settings.hinting)
    {
      display_wayland->xft_settings.hinting = xft_settings.hinting;
      gdk_display_setting_changed (display, "gtk-xft-hinting");
    }

  if (display_wayland->xft_settings.hintstyle != xft_settings.hintstyle)
    {
      display_wayland->xft_settings.hintstyle = xft_settings.hintstyle;
      gdk_display_setting_changed (display, "gtk-xft-hintstyle");
    }

  if (display_wayland->xft_settings.rgba != xft_settings.rgba)
    {
      display_wayland->xft_settings.rgba = xft_settings.rgba;
      gdk_display_setting_changed (display, "gtk-xft-rgba");
    }

  if (display_wayland->xft_settings.dpi != xft_settings.dpi)
    {
      display_wayland->xft_settings.dpi = xft_settings.dpi;
      gdk_display_setting_changed (display, "gtk-xft-dpi");
    }
}

static TranslationEntry translations[] = {
  { FALSE, "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" , G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name", G_TYPE_STRING, { .s = "gnome" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name", G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size", G_TYPE_INT, { .i = 24 } },
  { FALSE, "org.gnome.desktop.interface", "font-name", "gtk-font-name", G_TYPE_STRING, { .s = "Adwaita Sans 11" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink", G_TYPE_BOOLEAN,  { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time", G_TYPE_INT, { .i = 1200 } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout", G_TYPE_INT, { .i = 3600 } },
  { FALSE, "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module", G_TYPE_STRING, { .s = "simple" } },
  { FALSE, "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "gtk-enable-primary-paste", "gtk-enable-primary-paste", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "overlay-scrolling", "gtk-overlay-scrolling", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name", G_TYPE_STRING, { .s = "freedesktop" } },
  { FALSE, "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds", G_TYPE_BOOLEAN, { . b = FALSE } },
  { FALSE, "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age", G_TYPE_INT, { .i = 30 } },
  { FALSE, "org.gnome.desktop.privacy", "remember-recent-files",    "gtk-recent-files-enabled", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.wm.preferences", "button-layout",    "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, "org.gnome.desktop.interface", "font-antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.interface", "font-rendering", "gtk-font-rendering", G_TYPE_ENUM, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.interface", "text-scaling-factor", "gtk-xft-dpi" , G_TYPE_NONE, { .i = 0 } }, /* We store the factor as 16.16 */
  { FALSE, "org.gnome.desktop.wm.preferences", "action-double-click-titlebar", "gtk-titlebar-double-click", G_TYPE_STRING, { .s = "toggle-maximize" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-middle-click-titlebar", "gtk-titlebar-middle-click", G_TYPE_STRING, { .s = "none" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-right-click-titlebar", "gtk-titlebar-right-click", G_TYPE_STRING, { .s = "menu" } },
  { FALSE, "org.gnome.desktop.a11y", "always-show-text-caret", "gtk-keynav-use-caret", G_TYPE_BOOLEAN, { .b = FALSE } },
  { FALSE, "org.gnome.desktop.a11y.interface", "high-contrast", "high-contrast", G_TYPE_NONE, { .b = FALSE } },
  { FALSE, "org.gnome.desktop.a11y.interface", "show-status-shapes", "gtk-show-status-shapes", G_TYPE_BOOLEAN, { .b = FALSE } },
  { FALSE, "org.freedesktop.appearance", "color-scheme", "gtk-interface-color-scheme", G_TYPE_ENUM, { .i = 0 } },
  { FALSE, "org.freedesktop.appearance", "contrast", "gtk-interface-contrast", G_TYPE_ENUM, { .i = 0 } },
  /* Note, this setting doesn't exist, the portal and gsd fake it */
  { FALSE, "org.gnome.fontconfig", "serial", "gtk-fontconfig-timestamp", G_TYPE_NONE, { .i = 0 } },
};


static TranslationEntry *
find_translation_entry_by_schema (const char *schema,
                                  const char *key)
{
  for (gsize i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (schema, translations[i].schema) &&
          g_str_equal (key, translations[i].key))
        return &translations[i];
    }

  return NULL;
}

static TranslationEntry *
find_translation_entry_by_setting (const char *setting)
{
  for (gsize i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (setting, translations[i].setting))
        return &translations[i];
    }

  return NULL;
}

static void
apply_portal_setting (TranslationEntry *entry,
                      GVariant         *value,
                      GdkDisplay       *display)
{
  switch (entry->type)
    {
    case G_TYPE_STRING:
      entry->fallback.s = g_intern_string (g_variant_get_string (value, NULL));
      break;
    case G_TYPE_INT:
      entry->fallback.i = g_variant_get_int32 (value);
      break;
    case G_TYPE_ENUM:
      if (strcmp (entry->key, "font-rendering") == 0)
        entry->fallback.i = get_font_rendering (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "color-scheme") == 0)
        entry->fallback.i = (int) (g_variant_get_uint32 (value) + 1);
      else if (strcmp (entry->key, "contrast") == 0)
        entry->fallback.i = (int) (g_variant_get_uint32 (value) + 1);
      break;
    case G_TYPE_BOOLEAN:
      entry->fallback.b = g_variant_get_boolean (value);
      break;
    case G_TYPE_NONE:
      if (strcmp (entry->key, "serial") == 0)
        {
          entry->fallback.i = g_variant_get_int32 (value);
          break;
        }
      if (strcmp (entry->key, "antialiasing") == 0 ||
          strcmp (entry->key, "font-antialiasing") == 0)
        entry->fallback.i = get_antialiasing (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "hinting") == 0 ||
               strcmp (entry->key, "font-hinting") == 0)
        entry->fallback.i = get_hinting (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "rgba-order") == 0 ||
               strcmp (entry->key, "font-rgba-order") == 0)
        entry->fallback.i = get_order (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "text-scaling-factor") == 0)
        entry->fallback.i = (int) (g_variant_get_double (value) * 65536.0);
      update_xft_settings (display);
      break;
    default:
      break;
    }
}

static void
settings_portal_changed (GDBusProxy *proxy,
                         const char *sender_name,
                         const char *signal_name,
                         GVariant   *parameters,
                         GdkDisplay *display)
{
  if (strcmp (signal_name, "SettingChanged") == 0)
    {
      const char *namespace;
      const char *name;
      GVariant *value;
      TranslationEntry *entry;

      g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

      entry = find_translation_entry_by_schema (namespace, name);
      if (entry != NULL)
        {
          char *a = g_variant_print (value, FALSE);
          g_debug ("Using changed portal setting %s %s: %s", namespace, name, a);
          g_free (a);
          entry->valid = TRUE;
          apply_portal_setting (entry, value, display);
          gdk_display_setting_changed (display, entry->setting);
        }
      else
        g_debug ("Ignoring portal setting %s %s", namespace, name);

      g_variant_unref (value);
    }
}

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

void
gdk_wayland_display_init_settings (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (gdk_display_should_use_portal (display, PORTAL_SETTINGS_INTERFACE, 0) &&
      !(gdk_display_get_debug_flags (display) & GDK_DEBUG_DEFAULT_SETTINGS))
    {
      GVariant *ret;
      GError *error = NULL;
      const char *schema_str;
      GVariant *val;
      GVariantIter *iter;
      const char *patterns[] = { "org.gnome.*", "org.freedesktop.appearance", NULL }; 

      display_wayland->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                                        NULL,
                                                                        PORTAL_BUS_NAME,
                                                                        PORTAL_OBJECT_PATH,
                                                                        PORTAL_SETTINGS_INTERFACE,
                                                                        NULL,
                                                                        &error);
      if (error)
        {
          g_warning ("Settings portal not found: %s", error->message);
          g_error_free (error);

          goto fallback;
        }

      ret = g_dbus_proxy_call_sync (display_wayland->settings_portal,
                                    "ReadAll",
                                    g_variant_new ("(^as)", patterns),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);

      if (error)
        {
          g_warning ("Failed to read portal settings: %s", error->message);
          g_error_free (error);
          g_clear_object (&display_wayland->settings_portal);

          goto fallback;
        }

      g_variant_get (ret, "(a{sa{sv}})", &iter);

      if (g_variant_iter_n_children (iter) == 0)
        {
          g_debug ("Received no portal settings");
          g_clear_pointer (&iter, g_variant_iter_free);
          g_clear_pointer (&ret, g_variant_unref);
          g_clear_object (&display_wayland->settings_portal);

          goto fallback;
        }

      while (g_variant_iter_loop (iter, "{s@a{sv}}", &schema_str, &val))
        {
          GVariantIter *iter2 = g_variant_iter_new (val);
          const char *key;
          GVariant *v;

          while (g_variant_iter_loop (iter2, "{sv}", &key, &v))
            {
              TranslationEntry *entry = find_translation_entry_by_schema (schema_str, key);
              if (entry)
                {
                  char *a = g_variant_print (v, FALSE);
                  g_debug ("Using portal setting for %s %s: %s", schema_str, key, a);
                  g_free (a);
                  entry->valid = TRUE;
                  apply_portal_setting (entry, v, display);
                }
              else
                {
                  g_debug ("Ignoring portal setting for %s %s", schema_str, key);
                }
            }
          g_variant_iter_free (iter2);
        }
      g_variant_iter_free (iter);

      g_variant_unref (ret);

      g_signal_connect (display_wayland->settings_portal, "g-signal",
                        G_CALLBACK (settings_portal_changed), display_wayland);

      return;

fallback:
      g_debug ("Failed to use Settings portal, falling back to defaults");
    }
}

static void
set_value_from_entry (GdkDisplay       *display,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (display_wayland->settings_portal)
    {
      switch (entry->type)
        {
        case G_TYPE_STRING:
          g_value_set_string (value, entry->fallback.s);
          break;
        case G_TYPE_INT:
          g_value_set_int (value, entry->fallback.i);
          break;
        case G_TYPE_BOOLEAN:
          g_value_set_boolean (value, entry->fallback.b);
          break;
        case G_TYPE_ENUM:
          g_value_set_enum (value, entry->fallback.i);
          break;
        case G_TYPE_NONE:
          if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
            g_value_set_uint (value, (guint)entry->fallback.i);
          else if (g_str_equal (entry->setting, "gtk-xft-antialias"))
            g_value_set_int (value, display_wayland->xft_settings.antialias);
          else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
            g_value_set_int (value, display_wayland->xft_settings.hinting);
          else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
            g_value_set_static_string (value, display_wayland->xft_settings.hintstyle);
          else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
            g_value_set_static_string (value, display_wayland->xft_settings.rgba);
          else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
            g_value_set_int (value, display_wayland->xft_settings.dpi);
          else
            g_assert_not_reached ();
          break;
        default:
          g_assert_not_reached ();
          break;
        }
    }
}

static gboolean
set_capability_setting (GdkDisplay                *display,
                        GValue                    *value,
                        enum gtk_shell1_capability test)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  int testbit = 1 << (test - 1);

  g_value_set_boolean (value, (display_wayland->shell_capabilities & testbit) == testbit);

  return TRUE;
}

gboolean
gdk_wayland_display_get_setting (GdkDisplay *display,
                                 const char *name,
                                 GValue     *value)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  TranslationEntry *entry;

  if (gdk_display_get_debug_flags (display) & GDK_DEBUG_DEFAULT_SETTINGS)
      return FALSE;

  if (display_wayland->settings_portal)
    {
      entry = find_translation_entry_by_setting (name);
      if (entry != NULL)
        {
          set_value_from_entry (display, entry, value);
          return TRUE;
        }
    }

  if (strcmp (name, "gtk-shell-shows-app-menu") == 0)
    return set_capability_setting (display, value, GTK_SHELL1_CAPABILITY_GLOBAL_APP_MENU);

  if (strcmp (name, "gtk-shell-shows-menubar") == 0)
    return set_capability_setting (display, value, GTK_SHELL1_CAPABILITY_GLOBAL_MENU_BAR);

  if (strcmp (name, "gtk-shell-shows-desktop") == 0)
    return set_capability_setting (display, value, GTK_SHELL1_CAPABILITY_DESKTOP_ICONS);

  if (strcmp (name, "gtk-dialogs-use-header") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  return FALSE;
}

/* }}} */

/* vim:set foldmethod=marker: */
