/* GTK - The GIMP Toolkit
 * gtkfilesystem.c: Abstract file system interfaces
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

#include <gtk/gtkicontheme.h>

#include "gtkfilesystem.h"

#include <string.h>

struct _GtkFileInfo
{
  GtkFileTime modification_time;
  gint64 size;
  gchar *display_name;
  gchar *display_key;
  gchar *mime_type;
  GtkFileIconType icon_type : 4;
  guint is_folder : 1;
  guint is_hidden : 1;
};

static void gtk_file_system_base_init (gpointer g_class);
static void gtk_file_folder_base_init (gpointer g_class);

GQuark
gtk_file_system_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("gtk-file-system-error-quark");
  return quark;
}

/*****************************************
 *             GtkFileInfo               *
 *****************************************/
GType
gtk_file_info_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GtkFileInfo",
					     (GBoxedCopyFunc) gtk_file_info_copy,
					     (GBoxedFreeFunc) gtk_file_info_free);

  return our_type;
}

GtkFileInfo *
gtk_file_info_new  (void)
{
  GtkFileInfo *info;
  
  info = g_new0 (GtkFileInfo, 1);

  return info;
}

GtkFileInfo *
gtk_file_info_copy (GtkFileInfo *info)
{
  GtkFileInfo *new_info;

  g_return_val_if_fail (info != NULL, NULL);

  new_info = g_memdup (info, sizeof (GtkFileInfo));
  if (new_info->display_name)
    new_info->display_name = g_strdup (new_info->display_name);
  if (new_info->mime_type)
    new_info->mime_type = g_strdup (new_info->mime_type);

  return new_info;
}

void
gtk_file_info_free (GtkFileInfo *info)
{
  g_return_if_fail (info != NULL);

  if (info->display_name)
    g_free (info->display_name);
  if (info->mime_type)
    g_free (info->mime_type);
  if (info->display_key)
    g_free (info->display_key);

  g_free (info);
}

G_CONST_RETURN gchar *
gtk_file_info_get_display_name (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->display_name;
}

/**
 * gtk_file_info_get_display_key:
 * @info: a #GtkFileInfo
 * 
 * Returns results of g_utf8_collate_key() on the display name
 * for @info. This is useful when sorting a bunch of #GtkFileInfo
 * structures since the collate key will be only computed once.
 * 
 * Return value: The collate key for the display name, or %NULL
 *   if the display name hasn't been set.
 **/
G_CONST_RETURN gchar *
gtk_file_info_get_display_key (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);

  if (!info->display_key && info->display_name)
    {
      /* Since info->display_key is only a cache, we cast off the const
       */
      ((GtkFileInfo *)info)->display_key = g_utf8_collate_key (info->display_name, -1);
    }
	
  return info->display_key;
}

void
gtk_file_info_set_display_name (GtkFileInfo *info,
				const gchar *display_name)
{
  g_return_if_fail (info != NULL);

  if (info->display_name)
    g_free (info->display_name);
  if (info->display_key)
    {
      g_free (info->display_key);
      info->display_key = NULL;
    }

  info->display_name = g_strdup (display_name);
}

gboolean
gtk_file_info_get_is_folder (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  
  return info->is_folder;
}

void
gtk_file_info_set_is_folder (GtkFileInfo *info,
			     gboolean     is_folder)
{
  g_return_if_fail (info != NULL);

  info->is_folder = is_folder != FALSE;
}

gboolean
gtk_file_info_get_is_hidden (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  
  return info->is_hidden;
}

void
gtk_file_info_set_is_hidden (GtkFileInfo *info,
			     gboolean     is_hidden)
{
  g_return_if_fail (info != NULL);

  info->is_hidden = is_hidden != FALSE;
}

G_CONST_RETURN gchar *
gtk_file_info_get_mime_type (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->mime_type;
}

void
gtk_file_info_set_mime_type (GtkFileInfo *info,
			     const gchar *mime_type)
{
  g_return_if_fail (info != NULL);
  
  if (info->mime_type)
    g_free (info->mime_type);

  info->mime_type = g_strdup (mime_type);
}

GtkFileTime
gtk_file_info_get_modification_time (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, 0);

  return info->modification_time;
}

void
gtk_file_info_set_modification_time (GtkFileInfo *info,
				     GtkFileTime  modification_time)
{
  g_return_if_fail (info != NULL);
  
  info->modification_time = modification_time;
}

gint64
gtk_file_info_get_size (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, 0);
  
  return info->size;
}

void
gtk_file_info_set_size (GtkFileInfo *info,
			gint64       size)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (size >= 0);
  
  info->size = size;
}

void
gtk_file_info_set_icon_type  (GtkFileInfo      *info,
			      GtkFileIconType   icon_type)
{
  g_return_if_fail (info != NULL);

  info->icon_type = icon_type;
}

GtkFileIconType
gtk_file_info_get_icon_type (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, GTK_FILE_ICON_REGULAR);

  return info->icon_type;
}

typedef struct _IconCacheElement IconCacheElement;

struct _IconCacheElement
{
  gint size;
  GdkPixbuf *pixbuf;
};

static void
icon_cache_element_free (IconCacheElement *element)
{
  if (element->pixbuf)
    g_object_unref (element->pixbuf);
  g_free (element);
}

static void
icon_theme_changed (GtkIconTheme *icon_theme)
{
  GHashTable *cache;
  
  /* Difference from the initial creation is that we don't
   * reconnect the signal
   */
  cache = g_hash_table_new_full (g_str_hash, g_str_equal,
				 (GDestroyNotify)g_free,
				 (GDestroyNotify)icon_cache_element_free);
  g_object_set_data_full (G_OBJECT (icon_theme), "gtk-file-icon-cache",
			  cache, (GDestroyNotify)g_hash_table_destroy);
}

static GdkPixbuf *
get_cached_icon (GtkWidget   *widget,
		 const gchar *name,
		 gint         pixel_size)
{
  GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  GHashTable *cache = g_object_get_data (G_OBJECT (icon_theme), "gtk-file-icon-cache");
  IconCacheElement *element;
  
  if (!cache)
    {
      cache = g_hash_table_new_full (g_str_hash, g_str_equal,
				     (GDestroyNotify)g_free,
				     (GDestroyNotify)icon_cache_element_free);
      
      g_object_set_data_full (G_OBJECT (icon_theme), "gtk-file-icon-cache",
			      cache, (GDestroyNotify)g_hash_table_destroy);
      g_signal_connect (icon_theme, "changed",
			G_CALLBACK (icon_theme_changed), NULL);
    }

  element = g_hash_table_lookup (cache, name);
  if (!element)
    {
      element = g_new0 (IconCacheElement, 1);
      g_hash_table_insert (cache, g_strdup (name), element);
    }

  if (element->size != pixel_size)
    {
      if (element->pixbuf)
	g_object_unref (element->pixbuf);
      element->size = pixel_size;
      element->pixbuf = gtk_icon_theme_load_icon (icon_theme, name,
						  pixel_size, 0, NULL);
    }

  return element->pixbuf ? g_object_ref (element->pixbuf) : NULL;
}
		 

GdkPixbuf *
gtk_file_info_render_icon (const GtkFileInfo *info,
			   GtkWidget         *widget,
			   gint               pixel_size)
{
  const gchar *separator;
  GdkPixbuf *pixbuf;
  GString *icon_name;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (pixel_size > 0, NULL);

  if (info->icon_type != GTK_FILE_ICON_REGULAR)
    {
      const char *name = NULL;	/* Quiet gcc */
      
      switch (info->icon_type)
	{
	case GTK_FILE_ICON_BLOCK_DEVICE:
          name ="gnome-fs-blockdev";
	  break;
	case GTK_FILE_ICON_BROKEN_SYMBOLIC_LINK:
	  name = "gnome-fs-symlink";
	  break;
	case GTK_FILE_ICON_CHARACTER_DEVICE:
	  name = "gnome-fs-chardev";
	  break;
	case GTK_FILE_ICON_DIRECTORY:
	  name = "gnome-fs-directory";
	  break;
	case GTK_FILE_ICON_EXECUTABLE:
	  name ="gnome-fs-executable";
	  break;
	case GTK_FILE_ICON_FIFO:
	  name = "gnome-fs-fifo";
	  break;
	case GTK_FILE_ICON_SOCKET:
	  name = "gnome-fs-socket";
	  break;
	case GTK_FILE_ICON_REGULAR:
	  g_assert_not_reached ();
	}

      return get_cached_icon (widget, name, pixel_size);
    }
  
  if (!info->mime_type)
    return NULL;

  separator = strchr (info->mime_type, '/');
  if (!separator)
    return NULL;

  icon_name = g_string_new ("gnome-mime-");
  g_string_append_len (icon_name, info->mime_type, separator - info->mime_type);
  g_string_append_c (icon_name, '-');
  g_string_append (icon_name, separator + 1);
  pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
  g_string_free (icon_name, TRUE);
  if (pixbuf)
    return pixbuf;

  icon_name = g_string_new ("gnome-mime-");
  g_string_append_len (icon_name, info->mime_type, separator - info->mime_type);
  pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
  g_string_free (icon_name, TRUE);
  if (pixbuf)
    return pixbuf;

  return get_cached_icon (widget, "gnome-fs-regular", pixel_size);
}

/*****************************************
 *             GtkFileSystem             *
 *****************************************/
GType
gtk_file_system_get_type (void)
{
  static GType file_system_type = 0;

  if (!file_system_type)
    {
      static const GTypeInfo file_system_info =
      {
	sizeof (GtkFileSystemIface),  /* class_size */
	gtk_file_system_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      file_system_type = g_type_register_static (G_TYPE_INTERFACE,
						 "GtkFileSystem",
						 &file_system_info, 0);

      g_type_interface_add_prerequisite (file_system_type, G_TYPE_OBJECT);
    }

  return file_system_type;
}

static void
gtk_file_system_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_class);

      g_signal_new ("roots-changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileSystemIface, roots_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new ("bookmarks-changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileSystemIface, bookmarks_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

GSList *
gtk_file_system_list_roots (GtkFileSystem  *file_system)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->list_roots (file_system);
}

GtkFileInfo *
gtk_file_system_get_root_info  (GtkFileSystem     *file_system,
				const GtkFilePath *path,
				GtkFileInfoType    types,
				GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_root_info (file_system, path, types, error);
}

GtkFileFolder *
gtk_file_system_get_folder (GtkFileSystem     *file_system,
			    const GtkFilePath *path,
			    GtkFileInfoType    types,
			    GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_folder (file_system, path, types, error);
}

gboolean
gtk_file_system_create_folder(GtkFileSystem     *file_system,
			      const GtkFilePath *path,
			      GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->create_folder (file_system, path, error);
}

gboolean
gtk_file_system_get_parent (GtkFileSystem     *file_system,
			    const GtkFilePath *path,
			    GtkFilePath      **parent,
			    GError           **error)
{
  GtkFilePath *tmp_parent = NULL;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  result = GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_parent (file_system, path, &tmp_parent, error);
  g_assert (result || tmp_parent == NULL);

  if (parent)
    *parent = tmp_parent;
  else
    gtk_file_path_free (tmp_parent);
  
  return result;
}

GtkFilePath *
gtk_file_system_make_path (GtkFileSystem    *file_system,
			  const GtkFilePath *base_path,
			  const gchar       *display_name,
			  GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (base_path != NULL, NULL);
  g_return_val_if_fail (display_name != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->make_path (file_system, base_path, display_name, error);
}

/**
 * gtk_file_system_parse:
 * @file_system: a #GtkFileSystem
 * @base_path: reference folder with respect to which relative
 *            paths should be interpreted.
 * @str: the string to parse
 * @folder: location to store folder portion of result, or %NULL
 * @file_part: location to store file portion of result, or %NULL
 * @error: location to store error, or %NULL
 * 
 * Given a string entered by a user, parse it (possibly using
 * heuristics) into a folder path and a UTF-8 encoded
 * filename part. (Suitable for passing to gtk_file_system_make_path())
 *
 * Note that the returned filename point may point to a subfolder
 * of the returned folder. Adding a trailing path separator is needed
 * to enforce the interpretation as a folder name.
 *
 * If parsing fails because the syntax of @str is not understood,
 * and error of type GTK_FILE_SYSTEM_ERROR_BAD_FILENAME will
 * be set in @error and %FALSE returned.
 *
 * If parsing fails because a path was encountered that doesn't
 * exist on the filesystem, then an error of type
 * %GTK_FILE_SYSTEM_ERROR_NONEXISTENT will be set in @error
 * and %FALSE returned. (This only applies to parsing relative paths,
 * not to interpretation of @file_part. No check is made as
 * to whether @file_part exists.)
 *
 * Return value: %TRUE if the parsing succeeds, otherwise, %FALSE.
 **/
gboolean
gtk_file_system_parse (GtkFileSystem     *file_system,
		       const GtkFilePath *base_path,
		       const gchar       *str,
		       GtkFilePath      **folder,
		       gchar            **file_part,
		       GError           **error)
{
  GtkFilePath *tmp_folder = NULL;
  gchar *tmp_file_part = NULL;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (base_path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);


  result = GTK_FILE_SYSTEM_GET_IFACE (file_system)->parse (file_system, base_path, str,
							   &tmp_folder, &tmp_file_part,
							   error);
  g_assert (result || (tmp_folder == NULL && tmp_file_part == NULL));

  if (folder)
    *folder = tmp_folder;
  else
    gtk_file_path_free (tmp_folder);

  if (file_part)
    *file_part = tmp_file_part;
  else
    g_free (tmp_file_part);

  return result;
}


gchar *
gtk_file_system_path_to_uri (GtkFileSystem     *file_system,
			     const GtkFilePath *path)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  
  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->path_to_uri (file_system, path);
}

gchar *
gtk_file_system_path_to_filename (GtkFileSystem     *file_system,
				  const GtkFilePath *path)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  
  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->path_to_filename (file_system, path);
}

GtkFilePath *
gtk_file_system_uri_to_path (GtkFileSystem *file_system,
			     const gchar    *uri)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  
  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->uri_to_path (file_system, uri);
}

GtkFilePath *
gtk_file_system_filename_to_path (GtkFileSystem *file_system,
				  const gchar   *filename)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->filename_to_path (file_system, filename);
}

/**
 * gtk_file_system_add_bookmark:
 * @file_system: a #GtkFileSystem
 * @bookmark: path of the bookmark to add
 * @error: location to store error, or %NULL
 * 
 * Adds a bookmark folder to the user's bookmarks list.  If the operation succeeds,
 * the "bookmarks_changed" signal will be emitted.
 * 
 * Return value: TRUE if the operation succeeds, FALSE otherwise.  In the latter case,
 * the @error value will be set.
 **/
gboolean
gtk_file_system_add_bookmark (GtkFileSystem     *file_system,
			      const GtkFilePath *path,
			      GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->add_bookmark (file_system, path, error);
}

/**
 * gtk_file_system_remove_bookmark:
 * @file_system: a #GtkFileSystem
 * @bookmark: path of the bookmark to remove
 * @error: location to store error, or %NULL
 * 
 * Removes a bookmark folder from the user's bookmarks list.  If the operation
 * succeeds, the "bookmarks_changed" signal will be emitted.
 * 
 * Return value: TRUE if the operation succeeds, FALSE otherwise.  In the latter
 * case, the @error value will be set.
 **/
gboolean
gtk_file_system_remove_bookmark (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->remove_bookmark (file_system, path, error);
}

/**
 * gtk_file_system_list_bookmarks:
 * @file_system: a #GtkFileSystem
 * 
 * Queries the list of bookmarks in the file system.
 * 
 * Return value: A list of #GtkFilePath, or NULL if there are no configured
 * bookmarks.  You should use gtk_file_paths_free() to free this list.
 *
 * See also: gtk_file_system_get_supports_bookmarks()
 **/
GSList *
gtk_file_system_list_bookmarks (GtkFileSystem *file_system)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->list_bookmarks (file_system);
}

/*****************************************
 *             GtkFileFolder             *
 *****************************************/
GType
gtk_file_folder_get_type (void)
{
  static GType file_folder_type = 0;

  if (!file_folder_type)
    {
      static const GTypeInfo file_folder_info =
      {
	sizeof (GtkFileFolderIface),  /* class_size */
	gtk_file_folder_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      file_folder_type = g_type_register_static (G_TYPE_INTERFACE,
						 "GtkFileFolder",
						 &file_folder_info, 0);
      
      g_type_interface_add_prerequisite (file_folder_type, G_TYPE_OBJECT);
    }

  return file_folder_type;
}

static void
gtk_file_folder_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_class);

      g_signal_new ("deleted",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, deleted),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new ("files-added",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_added),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);
      g_signal_new ("files-changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);
      g_signal_new ("files-removed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_removed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);

      initialized = TRUE;
    }
}

gboolean
gtk_file_folder_list_children (GtkFileFolder    *folder,
			       GSList          **children,
			       GError          **error)
{
  gboolean result;
  GSList *tmp_children = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_FOLDER (folder), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  result = GTK_FILE_FOLDER_GET_IFACE (folder)->list_children (folder, &tmp_children, error);
  g_assert (result || tmp_children == NULL);

  if (children)
    *children = tmp_children;
  else
    gtk_file_paths_free (tmp_children);

  return result;
}

GtkFileInfo *
gtk_file_folder_get_info (GtkFileFolder     *folder,
			  const GtkFilePath *path,
			  GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_FOLDER (folder), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_FOLDER_GET_IFACE (folder)->get_info (folder, path, error);
}

GSList *
gtk_file_paths_sort (GSList *paths)
{
  return g_slist_sort (paths, (GCompareFunc)strcmp);
}

/**
 * gtk_file_paths_copy:
 * @paths: A #GSList of 3GtkFilePath structures.
 * 
 * Copies a list of #GtkFilePath structures.
 * 
 * Return value: A copy of @paths.  Since the contents of the list are copied as
 * well, you should use gtk_file_paths_free() to free the result.
 **/
GSList *
gtk_file_paths_copy (GSList *paths)
{
  GSList *head, *tail, *l;

  head = tail = NULL;

  for (l = paths; l; l = l->next)
    {
      GtkFilePath *path;
      GSList *node;

      path = l->data;
      node = g_slist_alloc ();

      if (tail)
	tail->next = node;
      else
	head = node;

      node->data = gtk_file_path_copy (path);
      tail = node;
    }

  return head;
}

void
gtk_file_paths_free (GSList *paths)
{
  GSList *tmp_list;

  for (tmp_list = paths; tmp_list; tmp_list = tmp_list->next)
    gtk_file_path_free (tmp_list->data);

  g_slist_free (paths);
}
