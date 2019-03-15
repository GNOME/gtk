/* GTK - The GIMP Toolkit
 * gtkfilechoosernativeprivate.h: Native File selector dialog
 * Copyright (C) 2015, Red Hat, Inc.
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

#ifndef __GTK_FILE_CHOOSER_NATIVE_PRIVATE_H__
#define __GTK_FILE_CHOOSER_NATIVE_PRIVATE_H__

#include <gtk/gtkfilechoosernative.h>
#ifdef GDK_WINDOWING_QUARTZ
#include <AvailabilityMacros.h>
#endif

G_BEGIN_DECLS

typedef struct {
  char *id;
  char *label;
  char **options;
  char **option_labels;
  char *selected;
} GtkFileChooserNativeChoice;

struct _GtkFileChooserNative
{
  GtkNativeDialog parent_instance;

  char *accept_label;
  char *cancel_label;

  int mode;
  GSList *custom_files;

  GFile *current_folder;
  GFile *current_file;
  char *current_name;
  GtkFileFilter *current_filter;
  GSList *choices;

  /* Fallback mode */
  GtkWidget *dialog;
  GtkWidget *accept_button;
  GtkWidget *cancel_button;

  gpointer mode_data;
};

gboolean gtk_file_chooser_native_win32_show (GtkFileChooserNative *self);
void gtk_file_chooser_native_win32_hide (GtkFileChooserNative *self);

#if defined GDK_WINDOWING_QUARTZ && MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
gboolean gtk_file_chooser_native_quartz_show (GtkFileChooserNative *self);
void gtk_file_chooser_native_quartz_hide (GtkFileChooserNative *self);
#endif

gboolean gtk_file_chooser_native_portal_show (GtkFileChooserNative *self);
void gtk_file_chooser_native_portal_hide (GtkFileChooserNative *self);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_NATIVE_PRIVATE_H__ */
