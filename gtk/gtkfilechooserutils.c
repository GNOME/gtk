/* GTK - The GIMP Toolkit
 * gtkfilechooserutils.c: Private utility functions useful for
 *                        implementing a GtkFileChooser interface
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

#include "config.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilesystem.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"


static gboolean       delegate_set_current_folder     (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static GFile *        delegate_get_current_folder     (GtkFileChooser    *chooser);
static void           delegate_set_current_name       (GtkFileChooser    *chooser,
						       const gchar       *name);
static gchar *        delegate_get_current_name       (GtkFileChooser    *chooser);
static gboolean       delegate_select_file            (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static void           delegate_unselect_file          (GtkFileChooser    *chooser,
						       GFile             *file);
static void           delegate_select_all             (GtkFileChooser    *chooser);
static void           delegate_unselect_all           (GtkFileChooser    *chooser);
static GSList *       delegate_get_files              (GtkFileChooser    *chooser);
static GFile *        delegate_get_preview_file       (GtkFileChooser    *chooser);
static GtkFileSystem *delegate_get_file_system        (GtkFileChooser    *chooser);
static void           delegate_add_filter             (GtkFileChooser    *chooser,
						       GtkFileFilter     *filter);
static void           delegate_remove_filter          (GtkFileChooser    *chooser,
						       GtkFileFilter     *filter);
static GSList *       delegate_list_filters           (GtkFileChooser    *chooser);
static gboolean       delegate_add_shortcut_folder    (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static gboolean       delegate_remove_shortcut_folder (GtkFileChooser    *chooser,
						       GFile             *file,
						       GError           **error);
static GSList *       delegate_list_shortcut_folders  (GtkFileChooser    *chooser);
static void           delegate_notify                 (GObject           *object,
						       GParamSpec        *pspec,
						       gpointer           data);
static void           delegate_current_folder_changed (GtkFileChooser    *chooser,
						       gpointer           data);
static void           delegate_selection_changed      (GtkFileChooser    *chooser,
						       gpointer           data);
static void           delegate_update_preview         (GtkFileChooser    *chooser,
						       gpointer           data);
static void           delegate_file_activated         (GtkFileChooser    *chooser,
						       gpointer           data);

static GtkFileChooserConfirmation delegate_confirm_overwrite (GtkFileChooser    *chooser,
							      gpointer           data);
static void           delegate_add_choice             (GtkFileChooser  *chooser,
                                                       const char      *id,
                                                       const char      *label,
                                                       const char     **options,
                                                       const char     **option_labels);
static void           delegate_remove_choice          (GtkFileChooser  *chooser,
                                                       const char      *id);
static void           delegate_set_choice             (GtkFileChooser  *chooser,
                                                       const char      *id,
                                                       const char      *option);
static const char *   delegate_get_choice             (GtkFileChooser  *chooser,
                                                       const char      *id);


/**
 * _gtk_file_chooser_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 *
 * Installs the necessary properties for a class implementing
 * #GtkFileChooser. A #GtkParamSpecOverride property is installed
 * for each property, using the values from the #GtkFileChooserProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don’t collide with some other property values they
 * are using.
 **/
void
_gtk_file_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_ACTION,
				    "action");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET,
				    "extra-widget");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_FILTER,
				    "filter");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_LOCAL_ONLY,
				    "local-only");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET,
				    "preview-widget");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE,
				    "preview-widget-active");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL,
				    "use-preview-label");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
				    "select-multiple");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN,
				    "show-hidden");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION,
				    "do-overwrite-confirmation");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS,
				    "create-folders");
}

/**
 * _gtk_file_chooser_delegate_iface_init:
 * @iface: a #GtkFileChoserIface structure
 *
 * An interface-initialization function for use in cases where
 * an object is simply delegating the methods, signals of
 * the #GtkFileChooser interface to another object.
 * _gtk_file_chooser_set_delegate() must be called on each
 * instance of the object so that the delegate object can
 * be found.
 **/
void
_gtk_file_chooser_delegate_iface_init (GtkFileChooserIface *iface)
{
  iface->set_current_folder = delegate_set_current_folder;
  iface->get_current_folder = delegate_get_current_folder;
  iface->set_current_name = delegate_set_current_name;
  iface->get_current_name = delegate_get_current_name;
  iface->select_file = delegate_select_file;
  iface->unselect_file = delegate_unselect_file;
  iface->select_all = delegate_select_all;
  iface->unselect_all = delegate_unselect_all;
  iface->get_files = delegate_get_files;
  iface->get_preview_file = delegate_get_preview_file;
  iface->get_file_system = delegate_get_file_system;
  iface->add_filter = delegate_add_filter;
  iface->remove_filter = delegate_remove_filter;
  iface->list_filters = delegate_list_filters;
  iface->add_shortcut_folder = delegate_add_shortcut_folder;
  iface->remove_shortcut_folder = delegate_remove_shortcut_folder;
  iface->list_shortcut_folders = delegate_list_shortcut_folders;
  iface->add_choice = delegate_add_choice;
  iface->remove_choice = delegate_remove_choice;
  iface->set_choice = delegate_set_choice;
  iface->get_choice = delegate_get_choice;
}

/**
 * _gtk_file_chooser_set_delegate:
 * @receiver: a #GObject implementing #GtkFileChooser
 * @delegate: another #GObject implementing #GtkFileChooser
 *
 * Establishes that calls on @receiver for #GtkFileChooser
 * methods should be delegated to @delegate, and that
 * #GtkFileChooser signals emitted on @delegate should be
 * forwarded to @receiver. Must be used in conjunction with
 * _gtk_file_chooser_delegate_iface_init().
 **/
void
_gtk_file_chooser_set_delegate (GtkFileChooser *receiver,
				GtkFileChooser *delegate)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (receiver));
  g_return_if_fail (GTK_IS_FILE_CHOOSER (delegate));

  g_object_set_data (G_OBJECT (receiver), I_("gtk-file-chooser-delegate"), delegate);
  g_signal_connect (delegate, "notify",
		    G_CALLBACK (delegate_notify), receiver);
  g_signal_connect (delegate, "current-folder-changed",
		    G_CALLBACK (delegate_current_folder_changed), receiver);
  g_signal_connect (delegate, "selection-changed",
		    G_CALLBACK (delegate_selection_changed), receiver);
  g_signal_connect (delegate, "update-preview",
		    G_CALLBACK (delegate_update_preview), receiver);
  g_signal_connect (delegate, "file-activated",
		    G_CALLBACK (delegate_file_activated), receiver);
  g_signal_connect (delegate, "confirm-overwrite",
		    G_CALLBACK (delegate_confirm_overwrite), receiver);
}

GQuark
_gtk_file_chooser_delegate_get_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gtk-file-chooser-delegate");
  
  return quark;
}

static GtkFileChooser *
get_delegate (GtkFileChooser *receiver)
{
  return g_object_get_qdata (G_OBJECT (receiver),
			     GTK_FILE_CHOOSER_DELEGATE_QUARK);
}

static gboolean
delegate_select_file (GtkFileChooser    *chooser,
		      GFile             *file,
		      GError           **error)
{
  return gtk_file_chooser_select_file (get_delegate (chooser), file, error);
}

static void
delegate_unselect_file (GtkFileChooser *chooser,
			GFile          *file)
{
  gtk_file_chooser_unselect_file (get_delegate (chooser), file);
}

static void
delegate_select_all (GtkFileChooser *chooser)
{
  gtk_file_chooser_select_all (get_delegate (chooser));
}

static void
delegate_unselect_all (GtkFileChooser *chooser)
{
  gtk_file_chooser_unselect_all (get_delegate (chooser));
}

static GSList *
delegate_get_files (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_files (get_delegate (chooser));
}

static GFile *
delegate_get_preview_file (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_preview_file (get_delegate (chooser));
}

static GtkFileSystem *
delegate_get_file_system (GtkFileChooser *chooser)
{
  return _gtk_file_chooser_get_file_system (get_delegate (chooser));
}

static void
delegate_add_filter (GtkFileChooser *chooser,
		     GtkFileFilter  *filter)
{
  gtk_file_chooser_add_filter (get_delegate (chooser), filter);
}

static void
delegate_remove_filter (GtkFileChooser *chooser,
			GtkFileFilter  *filter)
{
  gtk_file_chooser_remove_filter (get_delegate (chooser), filter);
}

static GSList *
delegate_list_filters (GtkFileChooser *chooser)
{
  return gtk_file_chooser_list_filters (get_delegate (chooser));
}

static gboolean
delegate_add_shortcut_folder (GtkFileChooser  *chooser,
			      GFile           *file,
			      GError         **error)
{
  return _gtk_file_chooser_add_shortcut_folder (get_delegate (chooser), file, error);
}

static gboolean
delegate_remove_shortcut_folder (GtkFileChooser  *chooser,
				 GFile           *file,
				 GError         **error)
{
  return _gtk_file_chooser_remove_shortcut_folder (get_delegate (chooser), file, error);
}

static GSList *
delegate_list_shortcut_folders (GtkFileChooser *chooser)
{
  return _gtk_file_chooser_list_shortcut_folder_files (get_delegate (chooser));
}

static gboolean
delegate_set_current_folder (GtkFileChooser  *chooser,
			     GFile           *file,
			     GError         **error)
{
  return gtk_file_chooser_set_current_folder_file (get_delegate (chooser), file, error);
}

static GFile *
delegate_get_current_folder (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_current_folder_file (get_delegate (chooser));
}

static void
delegate_set_current_name (GtkFileChooser *chooser,
			   const gchar    *name)
{
  gtk_file_chooser_set_current_name (get_delegate (chooser), name);
}

static gchar *
delegate_get_current_name (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_current_name (get_delegate (chooser));
}

static void
delegate_notify (GObject    *object,
		 GParamSpec *pspec,
		 gpointer    data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)),
				 gtk_file_chooser_get_type ());
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify (data, pspec->name);
}

static void
delegate_selection_changed (GtkFileChooser *chooser,
			    gpointer        data)
{
  g_signal_emit_by_name (data, "selection-changed");
}

static void
delegate_current_folder_changed (GtkFileChooser *chooser,
				 gpointer        data)
{
  g_signal_emit_by_name (data, "current-folder-changed");
}

static void
delegate_update_preview (GtkFileChooser    *chooser,
			 gpointer           data)
{
  g_signal_emit_by_name (data, "update-preview");
}

static void
delegate_file_activated (GtkFileChooser    *chooser,
			 gpointer           data)
{
  g_signal_emit_by_name (data, "file-activated");
}

static GtkFileChooserConfirmation
delegate_confirm_overwrite (GtkFileChooser    *chooser,
			    gpointer           data)
{
  GtkFileChooserConfirmation conf;

  g_signal_emit_by_name (data, "confirm-overwrite", &conf);
  return conf;
}

static GFile *
get_parent_for_uri (const char *uri)
{
  GFile *file;
  GFile *parent;

  file = g_file_new_for_uri (uri);
  parent = g_file_get_parent (file);

  g_object_unref (file);
  return parent;
	
}

/* Extracts the parent folders out of the supplied list of GtkRecentInfo* items, and returns
 * a list of GFile* for those unique parents.
 */
GList *
_gtk_file_chooser_extract_recent_folders (GList *infos)
{
  GList *l;
  GList *result;
  GHashTable *folders;

  result = NULL;

  folders = g_hash_table_new (g_file_hash, (GEqualFunc) g_file_equal);

  for (l = infos; l; l = l->next)
    {
      GtkRecentInfo *info = l->data;
      const char *uri;
      GFile *parent;

      uri = gtk_recent_info_get_uri (info);
      parent = get_parent_for_uri (uri);

      if (parent)
	{
	  if (!g_hash_table_lookup (folders, parent))
	    {
	      g_hash_table_insert (folders, parent, (gpointer) 1);
	      result = g_list_prepend (result, g_object_ref (parent));
	    }

	  g_object_unref (parent);
	}
    }

  result = g_list_reverse (result);

  g_hash_table_destroy (folders);

  return result;
}

GSettings *
_gtk_file_chooser_get_settings_for_widget (GtkWidget *widget)
{
  static GQuark file_chooser_settings_quark = 0;
  GtkSettings *gtksettings;
  GSettings *settings;

  if (G_UNLIKELY (file_chooser_settings_quark == 0))
    file_chooser_settings_quark = g_quark_from_static_string ("-gtk-file-chooser-settings");

  gtksettings = gtk_widget_get_settings (widget);
  settings = g_object_get_qdata (G_OBJECT (gtksettings), file_chooser_settings_quark);

  if (G_UNLIKELY (settings == NULL))
    {
      settings = g_settings_new ("org.gtk.Settings.FileChooser");
      g_settings_delay (settings);

      g_object_set_qdata_full (G_OBJECT (gtksettings),
                               file_chooser_settings_quark,
                               settings,
                               g_object_unref);
    }

  return settings;
}

gchar *
_gtk_file_chooser_label_for_file (GFile *file)
{
  const gchar *path, *start, *end, *p;
  gchar *uri, *host, *label;

  uri = g_file_get_uri (file);

  start = strstr (uri, "://");
  if (start)
    {
      start += 3;
      path = strchr (start, '/');
      if (path)
        end = path;
      else
        {
          end = uri + strlen (uri);
          path = "/";
        }

      /* strip username */
      p = strchr (start, '@');
      if (p && p < end)
        start = p + 1;

      p = strchr (start, ':');
      if (p && p < end)
        end = p;

      host = g_strndup (start, end - start);
      /* Translators: the first string is a path and the second string 
       * is a hostname. Nautilus and the panel contain the same string 
       * to translate. 
       */
      label = g_strdup_printf (_("%1$s on %2$s"), path, host);

      g_free (host);
    }
  else
    {
      label = g_strdup (uri);
    }

  g_free (uri);

  return label;
}

static void
delegate_add_choice (GtkFileChooser *chooser,
                     const char      *id,
                     const char      *label,
                     const char     **options,
                     const char     **option_labels)
{
  gtk_file_chooser_add_choice (get_delegate (chooser),
                               id, label, options, option_labels);
}
static void
delegate_remove_choice (GtkFileChooser  *chooser,
                        const char      *id)
{
  gtk_file_chooser_remove_choice (get_delegate (chooser), id);
}

static void
delegate_set_choice (GtkFileChooser  *chooser,
                     const char      *id,
                     const char      *option)
{
  gtk_file_chooser_set_choice (get_delegate (chooser), id, option);
}


static const char *
delegate_get_choice (GtkFileChooser  *chooser,
                     const char      *id)
{
  return gtk_file_chooser_get_choice (get_delegate (chooser), id);
}
