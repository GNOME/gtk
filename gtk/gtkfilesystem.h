/* GTK - The GIMP Toolkit
 * gtkfilesystem.h: Filesystem abstraction functions.
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

#ifndef __GTK_FILE_SYSTEM_H__
#define __GTK_FILE_SYSTEM_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtkwidget.h>	/* For icon handling */

G_BEGIN_DECLS

#define GTK_TYPE_FILE_SYSTEM         (gtk_file_system_get_type ())
#define GTK_FILE_SYSTEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FILE_SYSTEM, GtkFileSystem))
#define GTK_FILE_SYSTEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_FILE_SYSTEM, GtkFileSystemClass))
#define GTK_IS_FILE_SYSTEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FILE_SYSTEM))
#define GTK_IS_FILE_SYSTEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_FILE_SYSTEM))
#define GTK_FILE_SYSTEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_FILE_SYSTEM, GtkFileSystemClass))

#define GTK_TYPE_FOLDER         (gtk_folder_get_type ())
#define GTK_FOLDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FOLDER, GtkFolder))
#define GTK_FOLDER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_FOLDER, GtkFolderClass))
#define GTK_IS_FOLDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FOLDER))
#define GTK_IS_FOLDER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_FOLDER))
#define GTK_FOLDER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_FOLDER, GtkFolderClass))

/* GError enumeration for GtkFileSystem
 */

#define GTK_FILE_SYSTEM_ERROR (gtk_file_system_error_quark ())

typedef enum
{
  GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
  GTK_FILE_SYSTEM_ERROR_NOT_FOLDER,
  GTK_FILE_SYSTEM_ERROR_INVALID_URI,
  GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
  GTK_FILE_SYSTEM_ERROR_FAILED,
  GTK_FILE_SYSTEM_ERROR_ALREADY_EXISTS
} GtkFileSystemError;

GQuark     gtk_file_system_error_quark      (void);

typedef struct GtkFileSystemClass GtkFileSystemClass;
typedef struct GtkFileSystem GtkFileSystem;
typedef struct GtkFolderClass GtkFolderClass;
typedef struct GtkFolder GtkFolder;
typedef struct GtkFileSystemVolume GtkFileSystemVolume; /* opaque struct */
typedef struct GtkFileSystemBookmark GtkFileSystemBookmark; /* opaque struct */

struct GtkFileSystemClass
{
  GObjectClass parent_class;

  void (*bookmarks_changed) (GtkFileSystem *file_system);
  void (*volumes_changed)   (GtkFileSystem *file_system);
};

struct GtkFileSystem
{
  GObject parent_object;
};

struct GtkFolderClass
{
  GObjectClass parent_class;

  void (*files_added)      (GtkFolder *folder,
			    GList     *paths);
  void (*files_removed)    (GtkFolder *folder,
			    GList     *paths);
  void (*files_changed)    (GtkFolder *folder,
			    GList     *paths);
  void (*finished_loading) (GtkFolder *folder);
  void (*deleted)          (GtkFolder *folder);
};

struct GtkFolder
{
  GObject parent_object;
};

typedef void (* GtkFileSystemGetFolderCallback)    (GCancellable        *cancellable,
						    GtkFolder           *folder,
						    const GError        *error,
						    gpointer             data);
typedef void (* GtkFileSystemGetInfoCallback)      (GCancellable        *cancellable,
						    GFileInfo           *file_info,
						    const GError        *error,
						    gpointer             data);
typedef void (* GtkFileSystemVolumeMountCallback)  (GCancellable        *cancellable,
						    GtkFileSystemVolume *volume,
						    const GError        *error,
						    gpointer             data);

/* GtkFileSystem methods */
GType           gtk_file_system_get_type     (void) G_GNUC_CONST;

GtkFileSystem * gtk_file_system_new          (void);

GSList *        gtk_file_system_list_volumes   (GtkFileSystem *file_system);
GSList *        gtk_file_system_list_bookmarks (GtkFileSystem *file_system);

gboolean        gtk_file_system_parse          (GtkFileSystem     *file_system,
						GFile             *base_file,
						const gchar       *str,
						GFile            **folder,
						gchar            **file_part,
						GError           **error);

GCancellable *  gtk_file_system_get_folder             (GtkFileSystem                     *file_system,
							GFile                             *file,
							const gchar                       *attributes,
							GtkFileSystemGetFolderCallback     callback,
							gpointer                           data);
GCancellable *  gtk_file_system_get_info               (GtkFileSystem                     *file_system,
							GFile                             *file,
							const gchar                       *attributes,
							GtkFileSystemGetInfoCallback       callback,
							gpointer                           data);
GCancellable *  gtk_file_system_mount_volume           (GtkFileSystem                     *file_system,
							GtkFileSystemVolume               *volume,
							GMountOperation                   *mount_operation,
							GtkFileSystemVolumeMountCallback   callback,
							gpointer                           data);
GCancellable *  gtk_file_system_mount_enclosing_volume (GtkFileSystem                     *file_system,
							GFile                             *file,
							GMountOperation                   *mount_operation,
							GtkFileSystemVolumeMountCallback   callback,
							gpointer                           data);

gboolean        gtk_file_system_insert_bookmark    (GtkFileSystem      *file_system,
						    GFile              *file,
						    gint                position,
						    GError            **error);
gboolean        gtk_file_system_remove_bookmark    (GtkFileSystem      *file_system,
						    GFile              *file,
						    GError            **error);

gchar *         gtk_file_system_get_bookmark_label (GtkFileSystem *file_system,
						    GFile         *file);
void            gtk_file_system_set_bookmark_label (GtkFileSystem *file_system,
						    GFile         *file,
						    const gchar   *label);

GtkFileSystemVolume * gtk_file_system_get_volume_for_file (GtkFileSystem       *file_system,
							   GFile               *file);

/* GtkFolder functions */
GSList *     gtk_folder_list_children (GtkFolder  *folder);
GFileInfo *  gtk_folder_get_info      (GtkFolder  *folder,
				       GFile      *file);

gboolean     gtk_folder_is_finished_loading (GtkFolder *folder);


/* GtkFileSystemVolume methods */
gchar *               gtk_file_system_volume_get_display_name (GtkFileSystemVolume *volume);
gboolean              gtk_file_system_volume_is_mounted       (GtkFileSystemVolume *volume);
GFile *               gtk_file_system_volume_get_root         (GtkFileSystemVolume *volume);
GdkPixbuf *           gtk_file_system_volume_render_icon      (GtkFileSystemVolume  *volume,
							       GtkWidget            *widget,
							       gint                  icon_size,
							       GError              **error);

void                  gtk_file_system_volume_free             (GtkFileSystemVolume *volume);

/* GtkFileSystemBookmark methods */
void                   gtk_file_system_bookmark_free          (GtkFileSystemBookmark *bookmark);

/* GFileInfo helper functions */
GdkPixbuf *     gtk_file_info_render_icon (GFileInfo *info,
					   GtkWidget *widget,
					   gint       icon_size);

G_END_DECLS

#endif /* __GTK_FILE_SYSTEM_H__ */
