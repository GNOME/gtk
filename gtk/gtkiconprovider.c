/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkiconprovider.h"
#include "gtkicontheme.h"
#include "gtkenums.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"


G_DEFINE_INTERFACE (GtkIconProvider, gtk_icon_provider, G_TYPE_OBJECT)

static void
gtk_icon_provider_default_init (GtkIconProviderInterface *iface)
{
}

void
gtk_icon_provider_set_for_display (GdkDisplay      *display,
                                   GtkIconProvider *provider)
{
  g_object_set_data_full (G_OBJECT (display), "--gtk-icon-provider",
                          g_object_ref (provider), g_object_unref);

  gtk_system_setting_changed (display, GTK_SYSTEM_SETTING_ICON_THEME);
}

GtkIconProvider *
gtk_icon_provider_get_for_display (GdkDisplay *display)
{
  GtkIconProvider *provider;

  provider = g_object_get_data (G_OBJECT (display), "--gtk-icon-provider");

  if (provider)
    return provider;

  return GTK_ICON_PROVIDER (gtk_icon_theme_get_for_display (display));
}

static GdkPaintable *gtk_lookup_builtin_icon (GdkDisplay *display,
                                              const char *name,
                                              int         size,
                                              float       scale);

GdkPaintable *
gtk_lookup_icon (GdkDisplay *display,
                 const char *name,
                 int         size,
                 float       scale)
{
  GtkIconProvider *provider;
  GdkPaintable *icon;

  provider = gtk_icon_provider_get_for_display (display);

  icon = GTK_ICON_PROVIDER_GET_IFACE (provider)->lookup_icon (provider, name, size, scale);

  if (!icon)
    {
      GTK_DISPLAY_DEBUG (display, ICONTHEME,
                         "%s: Looking up icon %s size %d@%f: not found",
                         G_OBJECT_TYPE_NAME (provider), name, size, scale);

      icon = gtk_lookup_builtin_icon (display, name, size, scale);
    }

  return icon;
}

static GdkPaintable *
gtk_lookup_builtin_icon (GdkDisplay *display,
                         const char *name,
                         int         size,
                         float       scale)
{
  int sizes[] = { 16, 32, 64 };
  char pixel_size[24] = { 0, };
  const char *contexts[] = {
    "actions",
    "categories",
    "devices",
    "emblems",
    "emotes",
    "mimetypes",
    "places",
    "status",
  };
  const char *extension;
  GdkPaintable *icon = NULL;

  for (int i = 0; i < G_N_ELEMENTS (sizes); i++)
    {
      if (sizes[i] >= size * scale)
        {
          size = sizes[i];
          g_snprintf (pixel_size, sizeof (pixel_size), "%dx%d", size, size);
          break;
        }
    }
  if (pixel_size[0] == '\0')
    {
      g_snprintf (pixel_size, sizeof (pixel_size), "64x64");
      size = 64;
    }

  if (g_str_has_suffix (name, "-symbolic"))
    extension = ".symbolic.png";
  else
    extension = ".png";

  for (int i = 0; i < G_N_ELEMENTS (contexts); i++)
    {
      char *uri;
      GFile *file;

      uri = g_strconcat ("resource:///org/gtk/libgtk/icons/", pixel_size, "/", contexts[i], "/", name, extension, NULL);
      file = g_file_new_for_uri (uri);

      if (g_file_query_exists (file, NULL))
        {
          GTK_DISPLAY_DEBUG (display, ICONTHEME,
                             "Looking up builtin icon %s size %d@%f: %s",
                             name, size, scale, uri);

          icon = GDK_PAINTABLE (gtk_icon_paintable_new_for_file (file, size, 1));
        }

      g_object_unref (file);
      g_free (uri);

      if (icon)
        break;
    }

  if (!icon)
    {
      GTK_DISPLAY_DEBUG (display, ICONTHEME,
                         "Looking up builtin icon %s size %d@%f: not found",
                         name, size, scale);

      icon = GDK_PAINTABLE (gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/status/image-missing.png"));
    }

  return icon;
}
