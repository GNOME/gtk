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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_SETTINGS_PRIVATE_H__
#define __GTK_SETTINGS_PRIVATE_H__

#include <gtk/gtksettings.h>
#include "gtkstylecascadeprivate.h"

G_BEGIN_DECLS

#define DEFAULT_THEME_NAME      "Adwaita"
#define DEFAULT_ICON_THEME      "Adwaita"

void                _gtk_settings_set_property_value_from_rc (GtkSettings            *settings,
                                                              const gchar            *name,
                                                              const GtkSettingsValue *svalue);
void                _gtk_settings_reset_rc_values            (GtkSettings            *settings);

void                _gtk_settings_handle_event               (GdkEventSetting        *event);
GtkRcPropertyParser _gtk_rc_property_parser_from_type        (GType                   type);
gboolean            _gtk_settings_parse_convert              (GtkRcPropertyParser     parser,
                                                              const GValue           *src_value,
                                                              GParamSpec             *pspec,
                                                              GValue                 *dest_value);
GdkScreen          *_gtk_settings_get_screen                 (GtkSettings            *settings);
GtkStyleCascade    *_gtk_settings_get_style_cascade          (GtkSettings            *settings,
                                                              gint                    scale);

typedef enum
{
  GTK_SETTINGS_SOURCE_DEFAULT,
  GTK_SETTINGS_SOURCE_THEME,
  GTK_SETTINGS_SOURCE_XSETTING,
  GTK_SETTINGS_SOURCE_APPLICATION
} GtkSettingsSource;

GtkSettingsSource  _gtk_settings_get_setting_source (GtkSettings *settings,
                                                     const gchar *name);

gboolean gtk_settings_get_enable_animations  (GtkSettings *settings);
gint     gtk_settings_get_dnd_drag_threshold (GtkSettings *settings);
const gchar *gtk_settings_get_font_family    (GtkSettings *settings);
gint         gtk_settings_get_font_size      (GtkSettings *settings);
gboolean     gtk_settings_get_font_size_is_absolute (GtkSettings *settings);

G_END_DECLS

#endif /* __GTK_SETTINGS_PRIVATE_H__ */
