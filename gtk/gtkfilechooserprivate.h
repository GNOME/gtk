/* GTK - The GIMP Toolkit
 * gtkfilechooserprivate.h: Interface definition for file selector GUIs
 * Copyright (C) 2003, Red Hat, Inc.
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

#ifndef __GTK_FILE_CHOOSER_PRIVATE_H__
#define __GTK_FILE_CHOOSER_PRIVATE_H__

#include "gtkfilechooser.h"
#include "gtkfilesystem.h"

G_BEGIN_DECLS

#define GTK_FILE_CHOOSER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FILE_CHOOSER, GtkFileChooserIface))

typedef struct _GtkFileChooserIface GtkFileChooserIface;

struct _GtkFileChooserIface
{
  GTypeInterface base_iface;

  /* GtkFileChooser interface has the following properties:
   *
   *  action:          GtkFileChooserAction
   *  folder_mode:     boolean
   *  select_multiple: boolean
   *  show_hidden:     boolean
   *  local_only:      boolean
   *
   *  preview_widget:  GtkWidget
   *  preview_widget_active: boolean
   */

  /* Methods
   */
  void           (*set_current_folder) (GtkFileChooser    *chooser,
		 		        const GtkFilePath *path);
  GtkFilePath *  (*get_current_folder) (GtkFileChooser    *chooser);
  void           (*select_path)        (GtkFileChooser    *chooser,
		 		        const GtkFilePath *path);
  void           (*unselect_path)      (GtkFileChooser    *chooser,
		 		        const GtkFilePath *path);
  void           (*select_all)         (GtkFileChooser    *chooser);
  void           (*unselect_all)       (GtkFileChooser    *chooser);
  GSList *       (*get_paths)          (GtkFileChooser    *chooser);
  GtkFileSystem *(*get_file_system)    (GtkFileChooser    *chooser);
  
  /* Signals
   */
  void (*current_folder_changed) (GtkFileChooser *chooser);
  void (*selection_changed)      (GtkFileChooser *chooser);
  void (*update_preview)         (GtkFileChooser *chooser,
				  const gchar     *uri);
};

GtkFileSystem *_gtk_file_chooser_get_file_system    (GtkFileChooser    *chooser);
void           _gtk_file_chooser_set_current_folder (GtkFileChooser    *chooser,
						     const GtkFilePath *path);
GtkFilePath *  _gtk_file_chooser_get_current_folder (GtkFileChooser    *chooser);
void           _gtk_file_chooser_select_path        (GtkFileChooser    *chooser,
						     const GtkFilePath *path);
void           _gtk_file_chooser_unselect_path      (GtkFileChooser    *chooser,
						     const GtkFilePath *path);
GSList *       _gtk_file_chooser_get_paths          (GtkFileChooser    *chooser);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_PRIVATE_H__ */
