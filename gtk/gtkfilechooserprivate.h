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

  /* Methods
   */
  gboolean       (*set_current_folder) 	   (GtkFileChooser    *chooser,
		 		       	    const GtkFilePath *path,
					    GError           **error);
  GtkFilePath *  (*get_current_folder) 	   (GtkFileChooser    *chooser);
  void           (*set_current_name)   	   (GtkFileChooser    *chooser,
					    const gchar       *name);
  gboolean       (*select_path)        	   (GtkFileChooser    *chooser,
		 		       	    const GtkFilePath *path,
					    GError           **error);
  void           (*unselect_path)      	   (GtkFileChooser    *chooser,
		 		       	    const GtkFilePath *path);
  void           (*select_all)         	   (GtkFileChooser    *chooser);
  void           (*unselect_all)       	   (GtkFileChooser    *chooser);
  GSList *       (*get_paths)          	   (GtkFileChooser    *chooser);
  GtkFilePath *  (*get_preview_path)   	   (GtkFileChooser    *chooser);
  GtkFileSystem *(*get_file_system)    	   (GtkFileChooser    *chooser);
  void           (*add_filter)         	   (GtkFileChooser    *chooser,
					    GtkFileFilter     *filter);
  void           (*remove_filter)      	   (GtkFileChooser    *chooser,
					    GtkFileFilter     *filter);
  GSList *       (*list_filters)       	   (GtkFileChooser    *chooser);
  gboolean       (*add_shortcut_folder)    (GtkFileChooser    *chooser,
					    const GtkFilePath *path,
					    GError           **error);
  gboolean       (*remove_shortcut_folder) (GtkFileChooser    *chooser,
					    const GtkFilePath *path,
					    GError           **error);
  GSList *       (*list_shortcut_folders)  (GtkFileChooser    *chooser);
  
  /* Signals
   */
  void (*current_folder_changed) (GtkFileChooser *chooser);
  void (*selection_changed)      (GtkFileChooser *chooser);
  void (*update_preview)         (GtkFileChooser *chooser);
  void (*file_activated)         (GtkFileChooser *chooser);
};

GtkFileSystem *_gtk_file_chooser_get_file_system         (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_set_current_folder_path (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);
GtkFilePath *  _gtk_file_chooser_get_current_folder_path (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_select_path             (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);
void           _gtk_file_chooser_unselect_path           (GtkFileChooser    *chooser,
							  const GtkFilePath *path);
GSList *       _gtk_file_chooser_get_paths               (GtkFileChooser    *chooser);
GtkFilePath *  _gtk_file_chooser_get_preview_path        (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_add_shortcut_folder     (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);
gboolean       _gtk_file_chooser_remove_shortcut_folder  (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_PRIVATE_H__ */
