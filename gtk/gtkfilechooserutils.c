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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilechooserenums.h"
#include "gtkfilesystem.h"

static void           delegate_set_current_folder     (GtkFileChooser    *chooser,
						       const GtkFilePath *path);
static GtkFilePath *  delegate_get_current_folder     (GtkFileChooser    *chooser);
static void           delegate_set_current_name       (GtkFileChooser    *chooser,
						       const gchar       *name);
static void           delegate_select_path            (GtkFileChooser    *chooser,
						       const GtkFilePath *path);
static void           delegate_unselect_path          (GtkFileChooser    *chooser,
						       const GtkFilePath *path);
static void           delegate_select_all             (GtkFileChooser    *chooser);
static void           delegate_unselect_all           (GtkFileChooser    *chooser);
static GSList *       delegate_get_paths              (GtkFileChooser    *chooser);
static GtkFileSystem *delegate_get_file_system        (GtkFileChooser    *chooser);
static void           delegate_current_folder_changed (GtkFileChooser    *chooser,
						       gpointer           data);
static void           delegate_selection_changed      (GtkFileChooser    *chooser,
						       gpointer           data);

/**
 * _gtk_file_chooser_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 * 
 * Installs the necessary properties for a class implementing
 * #GtkFileChooser. A #GtkParamSpecOverride property is installed
 * for each property, using the values from the #GtkFileChooserProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don't collide with some other property values they
 * are using.
 **/
void
_gtk_file_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_ACTION,
				   g_param_spec_override ("action",
							  GTK_TYPE_FILE_CHOOSER_ACTION,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_FILE_SYSTEM,
				   g_param_spec_override ("file-system",
							  GTK_TYPE_FILE_SYSTEM,
							  G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_FOLDER_MODE,
				   g_param_spec_override ("folder-mode",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_LOCAL_ONLY,
				   g_param_spec_override ("local-only",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET,
				   g_param_spec_override ("preview-widget",
							  GTK_TYPE_WIDGET,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE,
				   g_param_spec_override ("preview-widget-active",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
				   g_param_spec_override ("select-multiple",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN,
				   g_param_spec_override ("show-hidden",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
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
  iface->select_path = delegate_select_path;
  iface->unselect_path = delegate_unselect_path;
  iface->select_all = delegate_select_all;
  iface->unselect_all = delegate_unselect_all;
  iface->get_paths = delegate_get_paths;
  iface->get_file_system = delegate_get_file_system;
}

/**
 * _gtk_file_chooser_set_delegate:
 * @receiver: a GOobject implementing #GtkFileChooser
 * @delegate: another GObject implementing #GtkFileChooser
 *
 * Establishes that calls on @receiver for #GtkFileChooser
 * methods should be delegated to @delegate, and that
 * #GtkFileChooser signals emitted on @delegate should be
 * forwarded to @receiver. Must be used in confunction with
 * _gtk_file_chooser_delegate_iface_init().
 **/
void
_gtk_file_chooser_set_delegate (GtkFileChooser *receiver,
				GtkFileChooser *delegate)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (receiver));
  g_return_if_fail (GTK_IS_FILE_CHOOSER (delegate));
  
  g_object_set_data (G_OBJECT (receiver), "gtk-file-chooser-delegate", delegate);

  g_signal_connect (delegate, "current-folder-changed",
		    G_CALLBACK (delegate_current_folder_changed), receiver);
  g_signal_connect (delegate, "selection-changed",
		    G_CALLBACK (delegate_selection_changed), receiver);
}

static GtkFileChooser *
get_delegate (GtkFileChooser *receiver)
{
  return g_object_get_data (G_OBJECT (receiver), "gtk-file-chooser-delegate");
}

static void
delegate_select_path (GtkFileChooser    *chooser,
		      const GtkFilePath *path)
{
  _gtk_file_chooser_select_path (get_delegate (chooser), path);
}

static void
delegate_unselect_path (GtkFileChooser    *chooser,
			const GtkFilePath *path)
{
  _gtk_file_chooser_unselect_path (get_delegate (chooser), path);
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
delegate_get_paths (GtkFileChooser *chooser)
{
  return _gtk_file_chooser_get_paths (get_delegate (chooser));
}

static GtkFileSystem *
delegate_get_file_system (GtkFileChooser *chooser)
{
  return _gtk_file_chooser_get_file_system (get_delegate (chooser));
}

static void
delegate_set_current_folder (GtkFileChooser    *chooser,
			     const GtkFilePath *path)
{
  _gtk_file_chooser_set_current_folder_path (chooser, path);
}

static GtkFilePath *
delegate_get_current_folder (GtkFileChooser *chooser)
{
  return _gtk_file_chooser_get_current_folder_path (get_delegate (chooser));
}

static void
delegate_set_current_name (GtkFileChooser *chooser,
			   const gchar    *name)
{
  gtk_file_chooser_set_current_name (get_delegate (chooser), name);
}

static void
delegate_selection_changed (GtkFileChooser *chooser,
			    gpointer        data)
{
  g_signal_emit_by_name (data, "selection-changed", 0);
}

static void
delegate_current_folder_changed (GtkFileChooser *chooser,
				 gpointer        data)
{
  g_signal_emit_by_name (data, "current-folder-changed", 0);
}
