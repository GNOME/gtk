/* GTK - The GIMP Toolkit
 * gtkfilesystem.h: Abstract file system interfaces
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

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef gint64 GtkFileTime;

typedef struct _GtkFileFolder      GtkFileFolder;
typedef struct _GtkFileFolderIface GtkFileFolderIface;
typedef struct _GtkFileInfo        GtkFileInfo;
typedef struct _GtkFileSystem      GtkFileSystem;
typedef struct _GtkFileSystemIface GtkFileSystemIface;

/* Mask of information about a file, for monitoring and
 * gtk_file_system_get_info()
 */
typedef enum {
  GTK_FILE_INFO_DISPLAY_NAME      = 1 << 0,
  GTK_FILE_INFO_IS_FOLDER         = 1 << 1,
  GTK_FILE_INFO_IS_HIDDEN         = 1 << 2,
  GTK_FILE_INFO_MIME_TYPE         = 1 << 3,
  GTK_FILE_INFO_MODIFICATION_TIME = 1 << 4,
  GTK_FILE_INFO_SIZE              = 1 << 5,
  GTK_FILE_INFO_ICON              = 1 << 6
} GtkFileInfoType;

/* GError enumeration for GtkFileSystem
 */

#define GTK_FILE_SYSTEM_ERROR (gtk_file_system_error_quark ())

typedef enum
{
  GTK_FILE_SYSTEM_ERROR_NONEXISTANT,
  GTK_FILE_SYSTEM_ERROR_NOT_FOLDER,
  GTK_FILE_SYSTEM_ERROR_INVALID_URI,
  GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
  GTK_FILE_SYSTEM_ERROR_FAILED,
} GtkFileSystemError;

GQuark     gtk_file_system_error_quark      (void);

/* Boxed-type for gtk_file_folder_get_info() results
 */
#define GTK_TYPE_FILE_INFO (gtk_file_info_get_type ())

GType       gtk_file_info_get_type (void);

GtkFileInfo *gtk_file_info_new  (void);
GtkFileInfo *gtk_file_info_copy (GtkFileInfo *info);
void         gtk_file_info_free (GtkFileInfo *info);


G_CONST_RETURN gchar *gtk_file_info_get_display_name      (const GtkFileInfo *info);
G_CONST_RETURN gchar *gtk_file_info_get_display_key       (const GtkFileInfo *info);
void                  gtk_file_info_set_display_name      (GtkFileInfo       *info,
							   const gchar       *display_name);
gboolean              gtk_file_info_get_is_folder         (const GtkFileInfo *info);
void                  gtk_file_info_set_is_folder         (GtkFileInfo       *info,
							   gboolean           is_folder);
gboolean              gtk_file_info_get_is_hidden         (const GtkFileInfo *info);
void                  gtk_file_info_set_is_hidden         (GtkFileInfo       *info,
							   gboolean           is_hidden);
G_CONST_RETURN gchar *gtk_file_info_get_mime_type         (const GtkFileInfo *info);
void                  gtk_file_info_set_mime_type         (GtkFileInfo       *info,
							   const gchar       *mime_type);
GtkFileTime           gtk_file_info_get_modification_time (const GtkFileInfo *info);
void                  gtk_file_info_set_modification_time (GtkFileInfo       *info,
							   GtkFileTime        modification_time);
gint64                gtk_file_info_get_size              (const GtkFileInfo *info);
void                  gtk_file_info_set_size              (GtkFileInfo       *info,
							   gint64             size);
GdkPixbuf *           gtk_file_info_get_icon              (const GtkFileInfo *info);
void                  gtk_file_info_set_icon              (GtkFileInfo       *info,
							   GdkPixbuf         *icon);

/* The base GtkFileSystem interface
 */
#define GTK_TYPE_FILE_SYSTEM             (gtk_file_system_get_type ())
#define GTK_FILE_SYSTEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_SYSTEM, GtkFileSystem))
#define GTK_IS_FILE_SYSTEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_SYSTEM))
#define GTK_FILE_SYSTEM_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FILE_SYSTEM, GtkFileSystemIface))

struct _GtkFileSystemIface
{
  GTypeInterface base_iface;

  /* Methods
   */
  GSList *           (*list_roots)     (GtkFileSystem *file_system);

  GtkFileInfo *      (*get_root_info)  (GtkFileSystem   *file_system,
					const gchar     *uri,
					GtkFileInfoType  types,
				        GError         **error);
  
  GtkFileFolder *    (*get_folder)     (GtkFileSystem  *file_system,
					const gchar    *uri,
					GtkFileInfoType types,
				        GError        **error);
  gboolean           (*create_folder)  (GtkFileSystem  *file_system,
					const gchar    *uri,
			                GError        **error);

  /* URI Manipulation
   */
  gboolean           (*get_parent)     (GtkFileSystem  *file_system,
					const gchar    *uri,
					gchar         **parent,
				        GError        **error);
  gchar *            (*make_uri)       (GtkFileSystem  *file_system,
					const gchar    *base_uri,
					const gchar    *display_name,
				        GError        **error);					
  gboolean           (*parse)          (GtkFileSystem  *file_system,
					const gchar    *base_uri,
					const gchar    *str,
					gchar         **folder,
					gchar         **file_part,
				        GError        **error);
  /* Signals
   */
  void (*roots_changed) (GtkFileSystem *file_system);
};

GType             gtk_file_system_get_type       (void);

GSList *          gtk_file_system_list_roots     (GtkFileSystem    *file_system);
GtkFileInfo *     gtk_file_system_get_root_info  (GtkFileSystem    *file_system,
						  const gchar      *uri,
						  GtkFileInfoType   types,
						  GError          **error);
  
gboolean          gtk_file_system_get_parent     (GtkFileSystem    *file_system,
						  const gchar      *uri,
						  gchar           **parent,
						  GError          **error);
GtkFileFolder    *gtk_file_system_get_folder     (GtkFileSystem    *file_system,
						  const gchar      *uri,
						  GtkFileInfoType   types,
						  GError          **error);
gboolean          gtk_file_system_create_folder  (GtkFileSystem    *file_system,
						  const gchar      *uri,
						  GError          **error);
gchar *           gtk_file_system_make_uri       (GtkFileSystem    *file_system,
						  const gchar      *base_uri,
						  const gchar      *display_name,
						  GError          **error);
gboolean          gtk_file_system_parse          (GtkFileSystem    *file_system,
						  const gchar      *base_uri,
						  const gchar      *str,
						  gchar           **folder,
						  gchar           **file_part,
						  GError          **error);
/*
 * Detailed information about a particular folder
 */
#define GTK_TYPE_FILE_FOLDER             (gtk_file_folder_get_type ())
#define GTK_FILE_FOLDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_FOLDER, GtkFileFolder))
#define GTK_IS_FILE_FOLDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_FOLDER))
#define GTK_FILE_FOLDER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FILE_FOLDER, GtkFileFolderIface))

struct _GtkFileFolderIface
{
  GTypeInterface base_iface;

  /* Methods
   */
  GtkFileInfo *      (*get_info)       (GtkFileFolder  *folder,
					const gchar    *uri,
				        GError        **error);
  gboolean           (*list_children)  (GtkFileFolder  *folder,
				        GSList        **children,
				        GError        **error);

  /* ??? refresh() ??? */

  /* Signals
   */
  void (*deleted)       (GtkFileFolder *monitor);
  void (*files_added)   (GtkFileFolder *monitor,
			 GSList        *uris);
  void (*files_changed) (GtkFileFolder *monitor,
			 GSList        *uris);
  void (*files_removed) (GtkFileFolder *monitor,
			 GSList        *uris);
};

GType        gtk_file_folder_get_type      (void);
gboolean     gtk_file_folder_list_children (GtkFileFolder    *folder,
					    GSList          **children,
					    GError          **error);
GtkFileInfo *gtk_file_folder_get_info      (GtkFileFolder    *folder,
					    const gchar      *uri,
					    GError          **error);

G_END_DECLS

#endif /* __GTK_FILE_SYSTEM_H__ */
