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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_FILE_CHOOSER_PRIVATE_H__
#define __GTK_FILE_CHOOSER_PRIVATE_H__

#include "gtkbookmarksmanager.h"
#include "gtkfilechooser.h"
#include "gtkfilesystem.h"
#include "gtkfilesystemmodel.h"
#include "gtkliststore.h"
#include "gtkrecentmanager.h"
#include "gtksearchengine.h"
#include "gtkquery.h"
#include "gtksizegroup.h"
#include "gtktreemodelsort.h"
#include "gtktreestore.h"
#include "gtktreeview.h"
#include "gtkbox.h"

G_BEGIN_DECLS

#define SETTINGS_KEY_LOCATION_MODE       "location-mode"
#define SETTINGS_KEY_SHOW_HIDDEN         "show-hidden"
#define SETTINGS_KEY_SHOW_SIZE_COLUMN    "show-size-column"
#define SETTINGS_KEY_SORT_COLUMN         "sort-column"
#define SETTINGS_KEY_SORT_ORDER          "sort-order"
#define SETTINGS_KEY_WINDOW_POSITION     "window-position"
#define SETTINGS_KEY_WINDOW_SIZE         "window-size"
#define SETTINGS_KEY_SIDEBAR_WIDTH       "sidebar-width"
#define SETTINGS_KEY_STARTUP_MODE        "startup-mode"

#define GTK_FILE_CHOOSER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FILE_CHOOSER, GtkFileChooserIface))

typedef struct _GtkFileChooserIface GtkFileChooserIface;

struct _GtkFileChooserIface
{
  GTypeInterface base_iface;

  /* Methods
   */
  gboolean       (*set_current_folder) 	   (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  GFile *        (*get_current_folder) 	   (GtkFileChooser    *chooser);
  void           (*set_current_name)   	   (GtkFileChooser    *chooser,
					    const gchar       *name);
  gchar *        (*get_current_name)       (GtkFileChooser    *chooser);
  gboolean       (*select_file)        	   (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  void           (*unselect_file)      	   (GtkFileChooser    *chooser,
					    GFile             *file);
  void           (*select_all)         	   (GtkFileChooser    *chooser);
  void           (*unselect_all)       	   (GtkFileChooser    *chooser);
  GSList *       (*get_files)          	   (GtkFileChooser    *chooser);
  GFile *        (*get_preview_file)   	   (GtkFileChooser    *chooser);
  GtkFileSystem *(*get_file_system)    	   (GtkFileChooser    *chooser);
  void           (*add_filter)         	   (GtkFileChooser    *chooser,
					    GtkFileFilter     *filter);
  void           (*remove_filter)      	   (GtkFileChooser    *chooser,
					    GtkFileFilter     *filter);
  GSList *       (*list_filters)       	   (GtkFileChooser    *chooser);
  gboolean       (*add_shortcut_folder)    (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  gboolean       (*remove_shortcut_folder) (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  GSList *       (*list_shortcut_folders)  (GtkFileChooser    *chooser);
  
  /* Signals
   */
  void (*current_folder_changed) (GtkFileChooser *chooser);
  void (*selection_changed)      (GtkFileChooser *chooser);
  void (*update_preview)         (GtkFileChooser *chooser);
  void (*file_activated)         (GtkFileChooser *chooser);
  GtkFileChooserConfirmation (*confirm_overwrite) (GtkFileChooser *chooser);
};

GtkFileSystem *_gtk_file_chooser_get_file_system         (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_add_shortcut_folder     (GtkFileChooser    *chooser,
							  GFile             *folder,
							  GError           **error);
gboolean       _gtk_file_chooser_remove_shortcut_folder  (GtkFileChooser    *chooser,
							  GFile             *folder,
							  GError           **error);
GSList *       _gtk_file_chooser_list_shortcut_folder_files (GtkFileChooser *chooser);


G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_PRIVATE_H__ */
