/* GTK - The GIMP Toolkit
 * gtkfilechoosersettings.h: Internal settings for the GtkFileChooser widget
 * Copyright (C) 2006, Novell, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@novell.com>
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

#ifndef __GTK_FILE_CHOOSER_SETTINGS_H__
#define __GTK_FILE_CHOOSER_SETTINGS_H__

#include "gtkfilechooserprivate.h"

G_BEGIN_DECLS

#define GTK_FILE_CHOOSER_SETTINGS_TYPE (_gtk_file_chooser_settings_get_type ())

typedef struct _GtkFileChooserSettings GtkFileChooserSettings;
typedef struct _GtkFileChooserSettingsClass GtkFileChooserSettingsClass;

struct _GtkFileChooserSettings
{
  GObject object;

  LocationMode location_mode;

  int geometry_x;
  int geometry_y;
  int geometry_width;
  int geometry_height;

  guint settings_read    : 1;
  guint show_hidden      : 1;
  guint show_size_column : 1;
  guint expand_folders   : 1;
};

struct _GtkFileChooserSettingsClass
{
  GObjectClass parent_class;
};

GType _gtk_file_chooser_settings_get_type (void) G_GNUC_CONST;

GtkFileChooserSettings *_gtk_file_chooser_settings_new (void);

LocationMode _gtk_file_chooser_settings_get_location_mode (GtkFileChooserSettings *settings);
void         _gtk_file_chooser_settings_set_location_mode (GtkFileChooserSettings *settings,
							   LocationMode            location_mode);

gboolean _gtk_file_chooser_settings_get_show_hidden (GtkFileChooserSettings *settings);
void     _gtk_file_chooser_settings_set_show_hidden (GtkFileChooserSettings *settings,
						     gboolean                show_hidden);

gboolean _gtk_file_chooser_settings_get_expand_folders (GtkFileChooserSettings *settings);
void     _gtk_file_chooser_settings_set_expand_folders (GtkFileChooserSettings *settings,
							gboolean                expand_folders);

gboolean _gtk_file_chooser_settings_get_show_size_column (GtkFileChooserSettings *settings);
void     _gtk_file_chooser_settings_set_show_size_column (GtkFileChooserSettings *settings,
                                                          gboolean                show_column);

void _gtk_file_chooser_settings_get_geometry (GtkFileChooserSettings *settings,
					      int                    *out_x,
					      int                    *out_y,
					      int                    *out_width,
					      int                    *out_heigth);
void _gtk_file_chooser_settings_set_geometry (GtkFileChooserSettings *settings,
					      int                     x,
					      int                     y,
					      int                     width,
					      int                     heigth);

gboolean _gtk_file_chooser_settings_save (GtkFileChooserSettings *settings,
					  GError                **error);

/* FIXME: persist these options:
 *
 * - paned width
 * - show_hidden
 */

G_END_DECLS

#endif
