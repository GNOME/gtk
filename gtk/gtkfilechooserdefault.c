/* GTK - The GIMP Toolkit
 * gtkfilechooserdefault.c: Default implementation of GtkFileChooser
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

#include "gtkalignment.h"
#include "gtkbutton.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrendererseptext.h"
#include "gtkcellrenderertext.h"
#include "gtkcombobox.h"
#include "gtkentry.h"
#include "gtkfilechooserdefault.h"
#include "gtkfilechooserentry.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilesystemmodel.h"
#include "gtkframe.h"
#include "gtkhbox.h"
#include "gtkhpaned.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmenuitem.h"
#include "gtkmessagedialog.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "gtksizegroup.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtktreeview.h"
#include "gtktreemodelsort.h"
#include "gtktreeselection.h"
#include "gtktreestore.h"
#include "gtktypebuiltins.h"
#include "gtkvbox.h"

#if defined (G_OS_UNIX)
#include "gtkfilesystemunix.h"
#elif defined (G_OS_WIN32)
#include "gtkfilesystemwin32.h"
#endif

#include <string.h>
#include <time.h>

typedef struct _GtkFileChooserDefaultClass GtkFileChooserDefaultClass;

#define GTK_FILE_CHOOSER_DEFAULT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefaultClass))
#define GTK_IS_FILE_CHOOSER_DEFAULT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_DEFAULT))
#define GTK_FILE_CHOOSER_DEFAULT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefaultClass))

struct _GtkFileChooserDefaultClass
{
  GtkVBoxClass parent_class;
};

struct _GtkFileChooserDefault
{
  GtkVBox parent_instance;

  GtkFileSystem *file_system;
  GtkFileSystemModel *tree_model;
  GtkTreeStore *shortcuts_model;
  GtkFileSystemModel *list_model;
  GtkTreeModelSort *sort_model;

  GtkFileChooserAction action;

  GtkFileFilter *current_filter;
  GSList *filters;

  gboolean has_home;
  gboolean has_desktop;

  int num_volumes;
  int num_shortcuts;
  int num_bookmarks;

  guint volumes_changed_id;

  guint bookmarks_changed_id;

  GtkFilePath *current_volume_path;
  GtkFilePath *current_folder;
  GtkFilePath *preview_path;

  GtkWidget *up_button;
  GtkWidget *new_folder_button;

  GtkWidget *preview_frame;

  GtkWidget *filter_combo;
  GtkWidget *folder_label;
  GtkWidget *tree_scrollwin;
  GtkWidget *tree;
  GtkWidget *shortcuts_scrollwin;
  GtkWidget *shortcuts_tree;
  GtkWidget *add_bookmark_button;
  GtkWidget *remove_bookmark_button;
  GtkWidget *list_scrollwin;
  GtkWidget *list;
  GtkWidget *entry;
  GtkWidget *preview_widget;
  GtkWidget *extra_widget;

  GtkTreeViewColumn *list_name_column;
  GtkCellRenderer *list_name_renderer;

  guint folder_mode : 1;
  guint local_only : 1;
  guint preview_widget_active : 1;
  guint select_multiple : 1;
  guint show_hidden : 1;
  guint list_sort_ascending : 1;
  guint changing_folder : 1;
};

/* Column numbers for the shortcuts tree.  Keep these in sync with create_shortcuts_model() */
enum {
  SHORTCUTS_COL_PIXBUF,
  SHORTCUTS_COL_NAME,
  SHORTCUTS_COL_PATH,
  SHORTCUTS_COL_REMOVABLE,
  SHORTCUTS_COL_NUM_COLUMNS
};

/* Column numbers for the file list */
enum {
  FILE_LIST_COL_NAME,
  FILE_LIST_COL_SIZE,
  FILE_LIST_COL_MTIME,
  FILE_LIST_COL_NUM_COLUMNS
};

/* Identifiers for target types */
enum {
  TEXT_URI_LIST
};

/* Target types for DnD in the shortcuts list */
static GtkTargetEntry shortcuts_targets[] = {
  { "text/uri-list", 0, TEXT_URI_LIST }
};

static const int num_shortcuts_targets = sizeof (shortcuts_targets) / sizeof (shortcuts_targets[0]);

/* Interesting places in the shortcuts bar */
typedef enum {
  SHORTCUTS_HOME,
  SHORTCUTS_DESKTOP,
  SHORTCUTS_VOLUMES,
  SHORTCUTS_SHORTCUTS,
  SHORTCUTS_SEPARATOR,
  SHORTCUTS_BOOKMARKS
} ShortcutsIndex;

/* Standard icon size */
/* FIXME: maybe this should correspond to the font size in the tree views... */
#define ICON_SIZE 20

static void gtk_file_chooser_default_class_init   (GtkFileChooserDefaultClass *class);
static void gtk_file_chooser_default_iface_init   (GtkFileChooserIface        *iface);
static void gtk_file_chooser_default_init         (GtkFileChooserDefault      *impl);

static GObject* gtk_file_chooser_default_constructor  (GType                  type,
						       guint                  n_construct_properties,
						       GObjectConstructParam *construct_params);
static void     gtk_file_chooser_default_finalize     (GObject               *object);
static void     gtk_file_chooser_default_set_property (GObject               *object,
						       guint                  prop_id,
						       const GValue          *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_default_get_property (GObject               *object,
						       guint                  prop_id,
						       GValue                *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_default_show_all     (GtkWidget             *widget);

static void           gtk_file_chooser_default_set_current_folder 	   (GtkFileChooser    *chooser,
									    const GtkFilePath *path);
static GtkFilePath *  gtk_file_chooser_default_get_current_folder 	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_set_current_name   	   (GtkFileChooser    *chooser,
									    const gchar       *name);
static void           gtk_file_chooser_default_select_path        	   (GtkFileChooser    *chooser,
									    const GtkFilePath *path);
static void           gtk_file_chooser_default_unselect_path      	   (GtkFileChooser    *chooser,
									    const GtkFilePath *path);
static void           gtk_file_chooser_default_select_all         	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_unselect_all       	   (GtkFileChooser    *chooser);
static GSList *       gtk_file_chooser_default_get_paths          	   (GtkFileChooser    *chooser);
static GtkFilePath *  gtk_file_chooser_default_get_preview_path   	   (GtkFileChooser    *chooser);
static GtkFileSystem *gtk_file_chooser_default_get_file_system    	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_add_filter         	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static void           gtk_file_chooser_default_remove_filter      	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static GSList *       gtk_file_chooser_default_list_filters       	   (GtkFileChooser    *chooser);
static gboolean       gtk_file_chooser_default_add_shortcut_folder    (GtkFileChooser    *chooser,
								       const GtkFilePath *path,
								       GError           **error);
static gboolean       gtk_file_chooser_default_remove_shortcut_folder (GtkFileChooser    *chooser,
								       const GtkFilePath *path,
								       GError           **error);
static GSList *       gtk_file_chooser_default_list_shortcut_folders  (GtkFileChooser    *chooser);

static void set_current_filter   (GtkFileChooserDefault *impl,
				  GtkFileFilter         *filter);
static void check_preview_change (GtkFileChooserDefault *impl);

static void filter_combo_changed       (GtkComboBox           *combo_box,
					GtkFileChooserDefault *impl);
static void tree_selection_changed     (GtkTreeSelection      *tree_selection,
					GtkFileChooserDefault *impl);

static void     shortcuts_row_activated_cb (GtkTreeView           *tree_view,
					    GtkTreePath           *path,
					    GtkTreeViewColumn     *column,
					    GtkFileChooserDefault *impl);
static gboolean shortcuts_select_func   (GtkTreeSelection      *selection,
					 GtkTreeModel          *model,
					 GtkTreePath           *path,
					 gboolean               path_currentlny_selected,
					 gpointer               data);

static void list_selection_changed     (GtkTreeSelection      *tree_selection,
					GtkFileChooserDefault *impl);
static void list_row_activated         (GtkTreeView           *tree_view,
					GtkTreePath           *path,
					GtkTreeViewColumn     *column,
					GtkFileChooserDefault *impl);
static void entry_activate             (GtkEntry              *entry,
					GtkFileChooserDefault *impl);

static void add_bookmark_button_clicked_cb    (GtkButton             *button,
					       GtkFileChooserDefault *impl);
static void remove_bookmark_button_clicked_cb (GtkButton             *button,
					       GtkFileChooserDefault *impl);

static void tree_name_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
static void list_icon_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
static void list_name_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
#if 0
static void list_size_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
#endif
static void list_mtime_data_func (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *tree_model,
				  GtkTreeIter       *iter,
				  gpointer           data);

static GObjectClass *parent_class;

GType
_gtk_file_chooser_default_get_type (void)
{
  static GType file_chooser_default_type = 0;

  if (!file_chooser_default_type)
    {
      static const GTypeInfo file_chooser_default_info =
      {
	sizeof (GtkFileChooserDefaultClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_chooser_default_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileChooserDefault),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_chooser_default_init,
      };

      static const GInterfaceInfo file_chooser_info =
      {
	(GInterfaceInitFunc) gtk_file_chooser_default_iface_init, /* interface_init */
	NULL,			                                       /* interface_finalize */
	NULL			                                       /* interface_data */
      };

      file_chooser_default_type = g_type_register_static (GTK_TYPE_VBOX, "GtkFileChooserDefault",
							 &file_chooser_default_info, 0);
      g_type_add_interface_static (file_chooser_default_type,
				   GTK_TYPE_FILE_CHOOSER,
				   &file_chooser_info);
    }

  return file_chooser_default_type;
}

static void
gtk_file_chooser_default_class_init (GtkFileChooserDefaultClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_chooser_default_finalize;
  gobject_class->constructor = gtk_file_chooser_default_constructor;
  gobject_class->set_property = gtk_file_chooser_default_set_property;
  gobject_class->get_property = gtk_file_chooser_default_get_property;

  widget_class->show_all = gtk_file_chooser_default_show_all;

  _gtk_file_chooser_install_properties (gobject_class);
}

static void
gtk_file_chooser_default_iface_init (GtkFileChooserIface *iface)
{
  iface->select_path = gtk_file_chooser_default_select_path;
  iface->unselect_path = gtk_file_chooser_default_unselect_path;
  iface->select_all = gtk_file_chooser_default_select_all;
  iface->unselect_all = gtk_file_chooser_default_unselect_all;
  iface->get_paths = gtk_file_chooser_default_get_paths;
  iface->get_preview_path = gtk_file_chooser_default_get_preview_path;
  iface->get_file_system = gtk_file_chooser_default_get_file_system;
  iface->set_current_folder = gtk_file_chooser_default_set_current_folder;
  iface->get_current_folder = gtk_file_chooser_default_get_current_folder;
  iface->set_current_name = gtk_file_chooser_default_set_current_name;
  iface->add_filter = gtk_file_chooser_default_add_filter;
  iface->remove_filter = gtk_file_chooser_default_remove_filter;
  iface->list_filters = gtk_file_chooser_default_list_filters;
  iface->add_shortcut_folder = gtk_file_chooser_default_add_shortcut_folder;
  iface->remove_shortcut_folder = gtk_file_chooser_default_remove_shortcut_folder;
  iface->list_shortcut_folders = gtk_file_chooser_default_list_shortcut_folders;
}

static void
gtk_file_chooser_default_init (GtkFileChooserDefault *impl)
{
  impl->folder_mode = FALSE;
  impl->local_only = TRUE;
  impl->preview_widget_active = TRUE;
  impl->select_multiple = FALSE;
  impl->show_hidden = FALSE;

  gtk_box_set_spacing (GTK_BOX (impl), 12);
}

static void
gtk_file_chooser_default_finalize (GObject *object)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);
  GSList *l;

  g_signal_handler_disconnect (impl->file_system, impl->volumes_changed_id);
  impl->volumes_changed_id = 0;
  g_signal_handler_disconnect (impl->file_system, impl->bookmarks_changed_id);
  impl->bookmarks_changed_id = 0;
  g_object_unref (impl->file_system);

  for (l = impl->filters; l; l = l->next)
    {
      GtkFileFilter *filter;

      filter = GTK_FILE_FILTER (l->data);
      g_object_unref (filter);
    }
  g_slist_free (impl->filters);

  if (impl->current_filter)
    g_object_unref (impl->current_filter);

  if (impl->current_volume_path)
    gtk_file_path_free (impl->current_volume_path);

  if (impl->current_folder)
    gtk_file_path_free (impl->current_folder);

  if (impl->preview_path)
    gtk_file_path_free (impl->preview_path);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Shows an error dialog */
static void
error_message (GtkFileChooserDefault *impl,
	       const char            *msg)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));
  if (!GTK_WIDGET_TOPLEVEL (toplevel))
    toplevel = NULL;

  dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_CLOSE,
				   "%s",
				   msg);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* Shows a simple error dialog relative to a path.  Frees the GError as well. */
static void
error_dialog (GtkFileChooserDefault *impl,
	      const char            *msg,
	      const GtkFilePath     *path,
	      GError                *error)
{
  char *text;

  text = g_strdup_printf (msg,
			  gtk_file_path_get_string (path),
			  error->message);
  error_message (impl, text);
  g_free (text);
  g_error_free (error);
}

/* Displays an error message about not being able to get information for a file.
 * Frees the GError as well.
 */
static void
error_getting_info_dialog (GtkFileChooserDefault *impl,
			   const GtkFilePath     *path,
			   GError                *error)
{
  error_dialog (impl,
		_("Could not retrieve information about %s:\n%s"),
		path, error);
}

/* Shows an error dialog about not being able to add a bookmark */
static void
error_could_not_add_bookmark_dialog (GtkFileChooserDefault *impl,
				     const GtkFilePath     *path,
				     GError                *error)
{
  error_dialog (impl,
		_("Could not add a bookmark for %s:\n%s"),
		path, error);
}

/* Shows an error dialog about not being able to compose a filename */
static void
error_building_filename_dialog (GtkFileChooserDefault *impl,
				const GtkFilePath     *base_path,
				const char            *file_part,
				GError                *error)
{
  char *msg;

  msg = g_strdup_printf (_("Could not build file name from '%s' and '%s':\n%s"),
			 gtk_file_path_get_string (base_path),
			 file_part,
			 error->message);
  error_message (impl, msg);
  g_free (msg);
  g_error_free (error);
}

static void
update_preview_widget_visibility (GtkFileChooserDefault *impl)
{
  if (impl->preview_widget_active && impl->preview_widget)
    gtk_widget_show (impl->preview_frame);
  else
    gtk_widget_hide (impl->preview_frame);
}

static void
set_preview_widget (GtkFileChooserDefault *impl,
		    GtkWidget             *preview_widget)
{
  if (preview_widget == impl->preview_widget)
    return;

  if (impl->preview_widget)
    gtk_container_remove (GTK_CONTAINER (impl->preview_frame),
			  impl->preview_widget);

  impl->preview_widget = preview_widget;
  if (impl->preview_widget)
    {
      gtk_widget_show_all (impl->preview_widget);
      gtk_container_add (GTK_CONTAINER (impl->preview_frame),
			 impl->preview_widget);
    }

  update_preview_widget_visibility (impl);
}

/* Clears the selection in the shortcuts tree */
static void
shortcuts_unselect_all (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->shortcuts_tree));
  gtk_tree_selection_unselect_all (selection);
}

/* Convenience function to get the display name and icon info for a path */
static GtkFileInfo *
get_file_info (GtkFileSystem *file_system, const GtkFilePath *path, GError **error)
{
  GtkFilePath *parent_path;
  GtkFileFolder *parent_folder;
  GtkFileInfo *info;

  if (!gtk_file_system_get_parent (file_system, path, &parent_path, error))
    return NULL;

  parent_folder = gtk_file_system_get_folder (file_system, parent_path,
					      GTK_FILE_INFO_DISPLAY_NAME
#if 0
					      | GTK_FILE_INFO_ICON
#endif
					      | GTK_FILE_INFO_IS_FOLDER,
					      error);
  gtk_file_path_free (parent_path);

  if (!parent_folder)
    return NULL;

  info = gtk_file_folder_get_info (parent_folder, path, error);
  g_object_unref (parent_folder);

  return info;
}

/* Inserts a path in the shortcuts tree, making a copy of it; alternatively,
 * inserts a volume.  A position of -1 indicates the end of the tree.
 */
static gboolean
shortcuts_insert_path (GtkFileChooserDefault *impl,
		       int                    pos,
		       gboolean               is_volume,
		       GtkFileSystemVolume   *volume,
		       const GtkFilePath     *path,
		       const char            *label,
		       gboolean               removable,
		       GError               **error)
{
  char *label_copy;
  GdkPixbuf *pixbuf;
  gpointer data;
  GtkTreeIter iter;

  if (is_volume)
    {
      data = volume;
      label_copy = gtk_file_system_volume_get_display_name (impl->file_system, volume);
      pixbuf = gtk_file_system_volume_render_icon (impl->file_system,
						   volume,
						   GTK_WIDGET (impl),
						   ICON_SIZE,
						   NULL);
    }
  else
    {
      GtkFileInfo *info;

      info = get_file_info (impl->file_system, path, error);
      if (!info)
	return FALSE;

      data = gtk_file_path_copy (path);

      if (label)
	label_copy = g_strdup (label);
      else
	label_copy = g_strdup (gtk_file_info_get_display_name (info));

      pixbuf = gtk_file_system_render_icon (impl->file_system, path, GTK_WIDGET (impl), ICON_SIZE, NULL);

      gtk_file_info_free (info);
    }

  gtk_tree_store_insert (impl->shortcuts_model, &iter, NULL, pos);

  gtk_tree_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, pixbuf,
		      SHORTCUTS_COL_NAME, label_copy,
		      SHORTCUTS_COL_PATH, data,
		      SHORTCUTS_COL_REMOVABLE, removable,
		      -1);

  g_free (label_copy);

  if (pixbuf)
    g_object_unref (pixbuf);

  return TRUE;
}

/* Appends an item for the user's home directory to the shortcuts model */
static void
shortcuts_append_home (GtkFileChooserDefault *impl)
{
  const char *home;
  GtkFilePath *home_path;
  GError *error;

  home = g_get_home_dir ();
  home_path = gtk_file_system_filename_to_path (impl->file_system, home);

  error = NULL;
  impl->has_home = shortcuts_insert_path (impl, -1, FALSE, NULL, home_path, _("Home"), FALSE, &error);
  if (!impl->has_home)
    error_getting_info_dialog (impl, home_path, error);

  gtk_file_path_free (home_path);
}

/* Appends the ~/Desktop directory to the shortcuts model */
static void
shortcuts_append_desktop (GtkFileChooserDefault *impl)
{
  char *name;
  GtkFilePath *path;

  name = g_build_filename (g_get_home_dir (), "Desktop", NULL);
  path = gtk_file_system_filename_to_path (impl->file_system, name);
  g_free (name);

  impl->has_desktop = shortcuts_insert_path (impl, -1, FALSE, NULL, path, NULL, FALSE, NULL);
  /* We do not actually pop up an error dialog if there is no desktop directory
   * because some people may really not want to have one.
   */

  gtk_file_path_free (path);
}

/* Appends a list of GtkFilePath to the shortcuts model; returns how many were inserted */
static int
shortcuts_append_paths (GtkFileChooserDefault *impl,
			GSList                *paths)
{
  int num_inserted;

  num_inserted = 0;

  for (; paths; paths = paths->next)
    {
      GtkFilePath *path;
      GError *error;

      path = paths->data;
      error = NULL;

      /* NULL GError, but we don't really want to show error boxes here */

      if (shortcuts_insert_path (impl, -1, FALSE, NULL, path, NULL, TRUE, NULL))
	num_inserted++;
    }

  return num_inserted;
}

/* Returns the index for the corresponding item in the shortcuts bar */
static int
shortcuts_get_index (GtkFileChooserDefault *impl,
		     ShortcutsIndex         where)
{
  int n;

  n = 0;

  if (where == SHORTCUTS_HOME)
    goto out;

  n += impl->has_home ? 1 : 0;

  if (where == SHORTCUTS_DESKTOP)
    goto out;

  n += impl->has_desktop ? 1 : 0;

  if (where == SHORTCUTS_VOLUMES)
    goto out;

  n += impl->num_volumes;

  if (where == SHORTCUTS_SHORTCUTS)
    goto out;

  n += impl->num_shortcuts;

  if (where == SHORTCUTS_SEPARATOR)
    goto out;

  n += 1;

  if (where == SHORTCUTS_BOOKMARKS)
    goto out;

  g_assert_not_reached ();

 out:

  return n;
}

typedef void (* RemoveFunc) (GtkFileChooserDefault *impl, gpointer data);

/* Removes the specified number of rows from the shortcuts list */
static void
shortcuts_remove_rows (GtkFileChooserDefault *impl,
		       int start_row,
		       int n_rows,
		       RemoveFunc remove_fn)
{
  GtkTreePath *path;

  path = gtk_tree_path_new_from_indices (start_row, -1);

  for (; n_rows; n_rows--)
    {
      GtkTreeIter iter;
      gpointer data;

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->shortcuts_model), &iter, path))
	g_assert_not_reached ();

      if (remove_fn)
	{
	  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter, SHORTCUTS_COL_PATH, &data, -1);
	  (* remove_fn) (impl, data);
	}

      gtk_tree_store_remove (impl->shortcuts_model, &iter);
    }

  gtk_tree_path_free (path);
}

/* Used from shortcuts_remove_rows() */
static void
volume_remove_cb (GtkFileChooserDefault *impl, gpointer data)
{
  GtkFileSystemVolume *volume;

  volume = data;
  gtk_file_system_volume_free (impl->file_system, volume);
}

/* Adds all the file system volumes to the shortcuts model */
static void
shortcuts_add_volumes (GtkFileChooserDefault *impl)
{
  int start_row;
  GSList *list, *l;
  int n;

  start_row = shortcuts_get_index (impl, SHORTCUTS_VOLUMES);
  shortcuts_remove_rows (impl, start_row, impl->num_volumes, volume_remove_cb);
  impl->num_volumes = 0;

  list = gtk_file_system_list_volumes (impl->file_system);

  n = 0;

  for (l = list; l; l = l->next)
    {
      GtkFileSystemVolume *volume;

      volume = l->data;

      shortcuts_insert_path (impl, start_row + n, TRUE, volume, NULL, NULL, FALSE, NULL);
      n++;
    }

  impl->num_volumes = n;

  g_slist_free (list);
}

/* Used from shortcuts_remove_rows() */
static void
remove_bookmark_cb (GtkFileChooserDefault *impl, gpointer data)
{
  GtkFilePath *path;

  path = data;
  gtk_file_path_free (path);
}

/* Updates the list of bookmarks */
static void
shortcuts_add_bookmarks (GtkFileChooserDefault *impl)
{
  GSList *bookmarks;

  shortcuts_remove_rows (impl,
			 shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS),
			 impl->num_bookmarks,
			 remove_bookmark_cb);

  bookmarks = gtk_file_system_list_bookmarks (impl->file_system);
  impl->num_bookmarks = shortcuts_append_paths (impl, bookmarks);
  gtk_file_paths_free (bookmarks);
}

/* Appends the bookmarks separator node and the bookmarks from the file system. */
static void
shortcuts_append_bookmarks (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;

  gtk_tree_store_append (impl->shortcuts_model, &iter, NULL);
  gtk_tree_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, NULL,
		      SHORTCUTS_COL_NAME, NULL,
		      SHORTCUTS_COL_PATH, NULL,
		      -1);
  shortcuts_add_bookmarks (impl);
}

/* Creates the GtkTreeStore used as the shortcuts model */
static void
create_shortcuts_model (GtkFileChooserDefault *impl)
{
  if (impl->shortcuts_model)
    g_object_unref (impl->shortcuts_model);

  /* Keep this order in sync with the SHORCUTS_COL_* enum values */
  impl->shortcuts_model = gtk_tree_store_new (SHORTCUTS_COL_NUM_COLUMNS,
					      GDK_TYPE_PIXBUF,	/* pixbuf */
					      G_TYPE_STRING,	/* name */
					      G_TYPE_POINTER,	/* path or volume */
					      G_TYPE_BOOLEAN);  /* removable */

  if (impl->file_system)
    {
      shortcuts_append_home (impl);
      shortcuts_append_desktop (impl);
      shortcuts_add_volumes (impl);
      shortcuts_append_bookmarks (impl);
    }

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->shortcuts_tree), GTK_TREE_MODEL (impl->shortcuts_model));
}

/* Callback used when the "Up" toolbar button is clicked */
static void
up_button_clicked_cb (GtkButton             *button,
		      GtkFileChooserDefault *impl)
{
  GtkFilePath *parent_path;
  GError *error;

  error = NULL;
  if (gtk_file_system_get_parent (impl->file_system, impl->current_folder, &parent_path, &error))
    {
      if (parent_path) /* If we were on a root, parent_path will be NULL */
	{
	  _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), parent_path);
	  gtk_file_path_free (parent_path);
	}
    }
  else
    error_dialog (impl,
		  _("Could not go to the parent folder of %s:\n%s"),
		  impl->current_folder,
		  error);
}

/* Callback used when the "New Folder" toolbar button is clicked */
static void
new_folder_button_clicked (GtkButton             *button,
			   GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  /* FIXME: this doesn't work for folder mode, just for file mode */

  _gtk_file_system_model_add_editable (impl->list_model, &iter);
  g_object_set (impl->list_name_renderer, "editable", TRUE, NULL);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->list_model), &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (impl->list),
			    path,
			    impl->list_name_column,
			    TRUE);
}

/* Callback used from the text cell renderer when the new folder is named */
static void
renderer_edited_cb (GtkCellRendererText   *cell_renderer_text,
		    const gchar           *path,
		    const gchar           *new_text,
		    GtkFileChooserDefault *impl)
{
  GError *error;
  GtkFilePath *file_path;

  _gtk_file_system_model_remove_editable (impl->list_model);
  g_object_set (impl->list_name_renderer, "editable", FALSE, NULL);

  error = NULL;
  file_path = gtk_file_system_make_path (impl->file_system, impl->current_folder, new_text, &error);
  if (!file_path)
    {
      error_building_filename_dialog (impl, impl->current_folder, new_text, error);
      return;
    }

  error = NULL;
  if (!gtk_file_system_create_folder (impl->file_system, file_path, &error))
    error_dialog (impl,
		  _("Could not create folder %s:\n%s"),
		  file_path, error);

  gtk_file_path_free (file_path);

  /* FIXME: scroll to the new folder and select it */
}

/* Callback used from the text cell renderer when the new folder edition gets
 * canceled.
 */
static void
renderer_editing_canceled_cb (GtkCellRendererText   *cell_renderer_text,
			      GtkFileChooserDefault *impl)
{
  _gtk_file_system_model_remove_editable (impl->list_model);
  g_object_set (impl->list_name_renderer, "editable", FALSE, NULL);
}

/* Creates the widgets for the filter combo box */
static GtkWidget *
filter_create (GtkFileChooserDefault *impl)
{
  impl->filter_combo = gtk_combo_box_new_text ();
  g_signal_connect (impl->filter_combo, "changed",
		    G_CALLBACK (filter_combo_changed), impl);

  return impl->filter_combo;
}

static GtkWidget *
button_new (GtkFileChooserDefault *impl,
	    const char *text,
	    const char *stock_id,
	    gboolean    sensitive,
	    gboolean    show,
	    GCallback   callback)
{
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *widget;

  button = gtk_button_new ();

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (button), hbox);

  widget = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  widget = gtk_label_new (text);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  gtk_widget_set_sensitive (button, sensitive);
  g_signal_connect (button, "clicked", callback, impl);

  gtk_widget_show_all (hbox);

  if (show)
    gtk_widget_show (button);

  return button;
}

/* Creates the widgets for the current folder indicator */
static GtkWidget *
current_folder_create (GtkFileChooserDefault *impl)
{
  GtkWidget *hbox;

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_widget_show (hbox);

  /* Up button */

  impl->up_button = button_new (impl,
				_("Up"),
				GTK_STOCK_GO_UP,
				FALSE,
				TRUE,
				G_CALLBACK (up_button_clicked_cb));
  gtk_box_pack_start (GTK_BOX (hbox), impl->up_button, FALSE, FALSE, 0);

  /* Current folder label */

  impl->folder_label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (impl->folder_label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), impl->folder_label, FALSE, FALSE, 0);
  gtk_widget_show (impl->folder_label);

  /* New folder button for save mode */

  impl->new_folder_button = gtk_button_new_from_stock (GTK_STOCK_NEW);
  g_signal_connect (impl->new_folder_button, "clicked",
		    G_CALLBACK (new_folder_button_clicked), impl);
  gtk_box_pack_end (GTK_BOX (hbox), impl->new_folder_button, FALSE, FALSE, 0);

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    gtk_widget_show (impl->new_folder_button);

  return hbox;
}

/* Sets the sensitivity of the toolbar buttons */
static void
toolbar_check_sensitivity (GtkFileChooserDefault *impl)
{
  GtkFilePath *parent_path;
  gboolean has_parent;

  has_parent = FALSE;

  /* I don't think we need to check GError here, do we? */
  if (gtk_file_system_get_parent (impl->file_system, impl->current_folder, &parent_path, NULL))
    {
      if (parent_path)
	{
	  gtk_file_path_free (parent_path);
	  has_parent = TRUE;
	}
    }

  gtk_widget_set_sensitive (GTK_WIDGET (impl->up_button), has_parent);
}

/* Creates the widgets for the folder tree */
static GtkWidget *
create_folder_tree (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  /* Scrolled window */

  impl->tree_scrollwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (impl->tree_scrollwin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (impl->tree_scrollwin),
				       GTK_SHADOW_IN);
  if (impl->folder_mode)
    gtk_widget_show (impl->tree_scrollwin);

  /* Tree */

  impl->tree = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (impl->tree), FALSE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->tree));
  g_signal_connect (selection, "changed",
		    G_CALLBACK (tree_selection_changed), impl);

  gtk_container_add (GTK_CONTAINER (impl->tree_scrollwin), impl->tree);
  gtk_widget_show (impl->tree);

  /* Column */

  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (impl->tree), 0,
					      _("File name"),
					      gtk_cell_renderer_text_new (),
					      tree_name_data_func, impl, NULL);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (impl->tree),
				   GTK_FILE_SYSTEM_MODEL_DISPLAY_NAME);

  return impl->tree_scrollwin;
}

/* Tries to add a bookmark from a path name */
static void
shortcuts_add_bookmark_from_path (GtkFileChooserDefault *impl,
				  const GtkFilePath     *path)
{
  GtkFileInfo *info;
  GError *error;

  error = NULL;
  info = get_file_info (impl->file_system, path, &error);

  if (!info)
    error_getting_info_dialog (impl, path, error);
  else if (!gtk_file_info_get_is_folder (info))
    {
      char *msg;

      msg = g_strdup_printf (_("Could not add bookmark for %s because it is not a folder."),
			     gtk_file_path_get_string (path));
      error_message (impl, msg);
      g_free (msg);
    }
  else
    {
      error = NULL;
      if (!gtk_file_system_add_bookmark (impl->file_system, path, &error))
	error_could_not_add_bookmark_dialog (impl, path, error);
    }
}

/* Callback used when the "Add bookmark" button is clicked */
static void
add_bookmark_button_clicked_cb (GtkButton *button,
				GtkFileChooserDefault *impl)
{
  shortcuts_add_bookmark_from_path (impl, impl->current_folder);
}

/* Callback used when the "Remove bookmark" button is clicked */
static void
remove_bookmark_button_clicked_cb (GtkButton *button,
				   GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkFilePath *path;
  gboolean removable;
  GError *error;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->shortcuts_tree));

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_PATH, &path,
			  SHORTCUTS_COL_REMOVABLE, &removable, -1);
      if (!removable)
	{
	  g_assert_not_reached ();
	  return;
	}

      error = NULL;
      if (!gtk_file_system_remove_bookmark (impl->file_system, path, &error))
	error_dialog (impl,
		      _("Could not remove bookmark for %s:\n%s"),
		      path,
		      error);
    }
}

/* Sensitize the "add bookmark" button if the current folder is not in the
 * bookmarks list, or de-sensitize it otherwise.
 */
static void
bookmarks_check_add_sensitivity (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  gboolean exists;
  int volumes_idx;
  int separator_idx;

  exists = FALSE;

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    {
      int i;

      separator_idx = shortcuts_get_index (impl, SHORTCUTS_SEPARATOR);
      volumes_idx = shortcuts_get_index (impl, SHORTCUTS_VOLUMES);

      i = 0;

      do
	{
	  gpointer data;

	  if (i == separator_idx)
	    continue;

	  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter, SHORTCUTS_COL_PATH, &data, -1);

	  if (i >= volumes_idx && i < volumes_idx + impl->num_volumes)
	    {
	      GtkFileSystemVolume *volume;
	      GtkFilePath *base_path;

	      volume = data;
	      base_path = gtk_file_system_volume_get_base_path (impl->file_system, volume);

	      exists = strcmp (gtk_file_path_get_string (impl->current_folder),
			       gtk_file_path_get_string (base_path)) == 0;
	      g_free (base_path);

	      if (exists)
		break;
	    }
	  else
	    {
	      GtkFilePath *path;

	      path = data;

	      if (path && gtk_file_path_compare (path, impl->current_folder) == 0)
		{
		  exists = TRUE;
		  break;
		}
	    }
	}
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter));
    }

  gtk_widget_set_sensitive (impl->add_bookmark_button, !exists);
}

/* Sets the sensitivity of the "remove bookmark" button depending on whether a
 * bookmark row is selected in the shortcuts tree.
 */
static void
bookmarks_check_remove_sensitivity (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  gboolean removable = FALSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->shortcuts_tree));

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			SHORTCUTS_COL_REMOVABLE, &removable,
			-1);

  gtk_widget_set_sensitive (impl->remove_bookmark_button, removable);
}

/* Converts raw selection data from text/uri-list to a list of strings */
static GSList *
split_uris (const char *data)
{
  GSList *uris;
  const char *p, *start;

  uris = NULL;

  start = data;

  for (p = start; *p != 0; p++)
    if (*p == '\r' && *(p + 1) == '\n')
      {
	char *name;

	name = g_strndup (start, p - start);
	uris = g_slist_prepend (uris, name);

	start = p + 2;
	p = start;
      }

  uris = g_slist_reverse (uris);
  return uris;
}

/* Callback used when we get the drag data for the bookmarks list.  We add the
 * received URIs as bookmarks if they are folders.
 */
static void
shortcuts_drag_data_received_cb (GtkWidget          *widget,
				 GdkDragContext     *context,
				 gint                x,
				 gint                y,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time_,
				 gpointer            data)
{
  GtkFileChooserDefault *impl;
  GSList *uris, *l;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  uris = split_uris (selection_data->data);

  for (l = uris; l; l = l->next)
    {
      char *uri;
      GtkFilePath *path;

      uri = l->data;
      path = gtk_file_system_uri_to_path (impl->file_system, uri);

      if (path)
	{
	  shortcuts_add_bookmark_from_path (impl, path);
	  gtk_file_path_free (path);
	}
      else
	{
	  char *msg;

	  msg = g_strdup_printf (_("Could not add a bookmark for %s because it is an invalid path name."),
				 uri);
	  error_message (impl, msg);
	  g_free (msg);
	}

      g_free (uri);
    }

  g_slist_free (uris);
}

/* Callback used when the selection in the shortcuts tree changes */
static void
shortcuts_selection_changed_cb (GtkTreeSelection      *selection,
				GtkFileChooserDefault *impl)
{
  bookmarks_check_remove_sensitivity (impl);
}

/* Creates the widgets for the shortcuts and bookmarks tree */
static GtkWidget *
shortcuts_tree_create (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  /* Scrolled window */

  impl->shortcuts_scrollwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (impl->shortcuts_scrollwin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (impl->shortcuts_scrollwin),
				       GTK_SHADOW_IN);
  gtk_widget_show (impl->shortcuts_scrollwin);

  /* Tree */

  impl->shortcuts_tree = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (impl->shortcuts_tree), FALSE);

  gtk_drag_dest_set (impl->shortcuts_tree,
		     GTK_DEST_DEFAULT_ALL,
		     shortcuts_targets,
		     num_shortcuts_targets,
		     GDK_ACTION_COPY);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->shortcuts_tree));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  gtk_tree_selection_set_select_function (selection,
					  shortcuts_select_func,
					  impl, NULL);

  g_signal_connect (selection, "changed",
		    G_CALLBACK (shortcuts_selection_changed_cb), impl);

  g_signal_connect (impl->shortcuts_tree, "row-activated",
		    G_CALLBACK (shortcuts_row_activated_cb), impl);

  g_signal_connect (impl->shortcuts_tree, "drag-data-received",
		    G_CALLBACK (shortcuts_drag_data_received_cb), impl);

  gtk_container_add (GTK_CONTAINER (impl->shortcuts_scrollwin), impl->shortcuts_tree);
  gtk_widget_show (impl->shortcuts_tree);

  /* Model */

  create_shortcuts_model (impl);

  /* Column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Folder"));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "pixbuf", SHORTCUTS_COL_PIXBUF,
				       NULL);

  renderer = _gtk_cell_renderer_sep_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "text", SHORTCUTS_COL_NAME,
				       NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->shortcuts_tree), column);

  return impl->shortcuts_scrollwin;
}

/* Creates the widgets for the shortcuts/bookmarks pane */
static GtkWidget *
shortcuts_pane_create (GtkFileChooserDefault *impl)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *widget;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  /* Shortcuts tree */

  widget = shortcuts_tree_create (impl);
  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);

  /* Box for buttons */

  hbox = gtk_hbox_new (TRUE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Add bookmark button */

  impl->add_bookmark_button = button_new (impl,
					  _("Add"),
					  GTK_STOCK_ADD,
					  FALSE,
					  TRUE,
					  G_CALLBACK (add_bookmark_button_clicked_cb));
  gtk_box_pack_start (GTK_BOX (hbox), impl->add_bookmark_button, TRUE, TRUE, 0);

  /* Remove bookmark button */

  impl->remove_bookmark_button = button_new (impl,
					     _("Remove"),
					     GTK_STOCK_REMOVE,
					     FALSE,
					     TRUE,
					     G_CALLBACK (remove_bookmark_button_clicked_cb));
  gtk_box_pack_start (GTK_BOX (hbox), impl->remove_bookmark_button, TRUE, TRUE, 0);

  return vbox;
}

/* Creates the widgets for the file list */
static GtkWidget *
create_file_list (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  /* Scrolled window */

  impl->list_scrollwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (impl->list_scrollwin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (impl->list_scrollwin),
				       GTK_SHADOW_IN);
  if (!impl->folder_mode)
    gtk_widget_show (impl->list_scrollwin);

  /* Tree/list view */

  impl->list = gtk_tree_view_new ();
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (impl->list), TRUE);
  gtk_container_add (GTK_CONTAINER (impl->list_scrollwin), impl->list);
  g_signal_connect (impl->list, "row_activated",
		    G_CALLBACK (list_row_activated), impl);
  gtk_widget_show (impl->list);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
  g_signal_connect (selection, "changed",
		    G_CALLBACK (list_selection_changed), impl);

  /* Filename column */

  impl->list_name_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (impl->list_name_column, TRUE);
  gtk_tree_view_column_set_title (impl->list_name_column, _("File name"));
  gtk_tree_view_column_set_sort_column_id (impl->list_name_column, FILE_LIST_COL_NAME);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (impl->list_name_column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (impl->list_name_column, renderer,
					   list_icon_data_func, impl, NULL);

  impl->list_name_renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (impl->list_name_renderer, "edited",
		    G_CALLBACK (renderer_edited_cb), impl);
  g_signal_connect (impl->list_name_renderer, "editing-canceled",
		    G_CALLBACK (renderer_editing_canceled_cb), impl);
  gtk_tree_view_column_pack_start (impl->list_name_column, impl->list_name_renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (impl->list_name_column, impl->list_name_renderer,
					   list_name_data_func, impl, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->list), impl->list_name_column);
#if 0
  /* Size column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Size"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   list_size_data_func, impl, NULL);
  gtk_tree_view_column_set_sort_column_id (column, FILE_LIST_COL_SIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->list), column);
#endif
  /* Modification time column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Modified"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   list_mtime_data_func, impl, NULL);
  gtk_tree_view_column_set_sort_column_id (column, FILE_LIST_COL_MTIME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->list), column);

  return impl->list_scrollwin;
}

/* Creates the widgets for the files/folders pane */
static GtkWidget *
file_pane_create (GtkFileChooserDefault *impl)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *widget;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  /* Current folder indicator */

  widget = current_folder_create (impl);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  /* Box for lists and preview */

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* Folder tree */

  widget = create_folder_tree (impl);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

  /* File list */

  widget = create_file_list (impl);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

  /* Preview */

  impl->preview_frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (hbox), impl->preview_frame, FALSE, FALSE, 0);
  /* Don't show preview frame initially */

  return vbox;
}

static GtkWidget *
create_filename_entry_and_filter_combo (GtkFileChooserDefault *impl)
{
  GtkWidget *hbox;
  GtkWidget *widget;

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_widget_show (hbox);

  /* Label and entry */

  widget = gtk_label_new_with_mnemonic (_("_Filename:"));
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  impl->entry = _gtk_file_chooser_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (impl->entry), TRUE);
  g_signal_connect (impl->entry, "activate",
		    G_CALLBACK (entry_activate), impl);
  _gtk_file_chooser_entry_set_file_system (GTK_FILE_CHOOSER_ENTRY (impl->entry),
					   impl->file_system);

  gtk_box_pack_start (GTK_BOX (hbox), impl->entry, TRUE, TRUE, 0);
  gtk_widget_show (impl->entry);

  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), impl->entry);

  /* Filter combo */

  widget = filter_create (impl);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  return hbox;
}

static GObject*
gtk_file_chooser_default_constructor (GType                  type,
				      guint                  n_construct_properties,
				      GObjectConstructParam *construct_params)
{
  GtkFileChooserDefault *impl;
  GObject *object;
  GtkWidget *hpaned;
  GtkWidget *widget;
  GList *focus_chain;
  GtkWidget *entry_widget;

  object = parent_class->constructor (type,
				      n_construct_properties,
				      construct_params);
  impl = GTK_FILE_CHOOSER_DEFAULT (object);

  g_assert (impl->file_system);

  gtk_widget_push_composite_child ();

  /* Paned widget */

  hpaned = gtk_hpaned_new ();
  gtk_box_pack_start (GTK_BOX (impl), hpaned, TRUE, TRUE, 0);
  gtk_paned_set_position (GTK_PANED (hpaned), 200); /* FIXME: this sucks */
  gtk_widget_show (hpaned);

  /* Shortcuts pane */

  widget = shortcuts_pane_create (impl);
  gtk_paned_pack1 (GTK_PANED (hpaned), widget, FALSE, FALSE);

  /* File/folder pane */

  widget = file_pane_create (impl);
  gtk_paned_pack2 (GTK_PANED (hpaned), widget, TRUE, FALSE);

  /* Filename entry and filter combo */

  entry_widget = create_filename_entry_and_filter_combo (impl);
  gtk_box_pack_start (GTK_BOX (impl), entry_widget, FALSE, FALSE, 0);

  /* Make the entry the first widget in the focus chain
   */
  focus_chain = g_list_append (NULL, entry_widget);
  focus_chain = g_list_append (focus_chain, hpaned);
  gtk_container_set_focus_chain (GTK_CONTAINER (impl), focus_chain);
  g_list_free (focus_chain);

  gtk_widget_pop_composite_child ();

  return object;
}

/* Sets the extra_widget by packing it in the appropriate place */
static void
set_extra_widget (GtkFileChooserDefault *impl,
		  GtkWidget             *extra_widget)
{
  if (extra_widget == impl->extra_widget)
    return;

  if (impl->extra_widget)
    gtk_container_remove (GTK_CONTAINER (impl), impl->extra_widget);

  impl->extra_widget = extra_widget;
  if (impl->extra_widget)
    {
      gtk_widget_show_all (impl->extra_widget);
      gtk_box_pack_end (GTK_BOX (impl), impl->extra_widget, FALSE, FALSE, 0);
    }
}

static void
volumes_changed_cb (GtkFileSystem         *file_system,
		    GtkFileChooserDefault *impl)
{
  shortcuts_add_volumes (impl);
}

/* Callback used when the set of bookmarks changes in the file system */
static void
bookmarks_changed_cb (GtkFileSystem         *file_system,
		      GtkFileChooserDefault *impl)
{
  shortcuts_add_bookmarks (impl);

  bookmarks_check_add_sensitivity (impl);
  bookmarks_check_remove_sensitivity (impl);
}

/* Sets the file chooser to multiple selection mode */
static void
set_select_multiple (GtkFileChooserDefault *impl,
		     gboolean               select_multiple,
		     gboolean               property_notify)
{
  GtkTreeSelection *selection;
  GtkSelectionMode mode;

  if (select_multiple == impl->select_multiple)
    return;

  impl->select_multiple = select_multiple;

  mode = select_multiple ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->tree));
  gtk_tree_selection_set_mode (selection, mode);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
  gtk_tree_selection_set_mode (selection, mode);

  g_object_notify (G_OBJECT (impl), "select-multiple");

  /* FIXME #132255: See note in check_preview_change() */
  check_preview_change (impl);
}

static void
set_file_system_backend (GtkFileChooserDefault *impl,
			 const char *backend)
{
  if (impl->file_system)
    {
      g_signal_handler_disconnect (impl->file_system, impl->volumes_changed_id);
      impl->volumes_changed_id = 0;
      g_signal_handler_disconnect (impl->file_system, impl->bookmarks_changed_id);
      impl->bookmarks_changed_id = 0;
      g_object_unref (impl->file_system);
    }
  
  impl->file_system = NULL;
  if (backend)
    impl->file_system = _gtk_file_system_create (backend);
  
  if (!impl->file_system)
    {
#if defined (G_OS_UNIX)
      impl->file_system = gtk_file_system_unix_new ();
#elif defined (G_OS_WIN32)
      impl->file_system = gtk_file_system_win32_new ();
#else
#error "No default filesystem implementation on the platform"
#endif
    }
  
  if (impl->file_system)
    {
      impl->volumes_changed_id = g_signal_connect (impl->file_system, "volumes-changed",
						   G_CALLBACK (volumes_changed_cb),
						   impl);
      impl->bookmarks_changed_id = g_signal_connect (impl->file_system, "bookmarks-changed",
						     G_CALLBACK (bookmarks_changed_cb),
						     impl);
    }
}

static void
gtk_file_chooser_default_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)

{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      impl->action = g_value_get_enum (value);
      if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  gtk_widget_show (impl->new_folder_button);

	  if (impl->select_multiple)
	    {
	      g_warning ("Save mode cannot be set in conjunction with multiple selection mode.  "
			 "Re-setting to single selection mode.");
	      set_select_multiple (impl, FALSE, TRUE);
	    }
	}
      else
	gtk_widget_hide (impl->new_folder_button);

      break;
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      set_file_system_backend (impl, g_value_get_string (value));
      break;
    case GTK_FILE_CHOOSER_PROP_FILTER:
      set_current_filter (impl, g_value_get_object (value));
      break;
    case GTK_FILE_CHOOSER_PROP_FOLDER_MODE:
      {
	gboolean folder_mode = g_value_get_boolean (value);
	if (folder_mode != impl->folder_mode)
	  {
	    impl->folder_mode = folder_mode;
	    if (impl->folder_mode)
	      {
		gtk_widget_hide (impl->list_scrollwin);
		gtk_widget_show (impl->tree_scrollwin);
	      }
	    else
	      {
		gtk_widget_hide (impl->tree_scrollwin);
		gtk_widget_show (impl->list_scrollwin);
	      }
	  }
      }
      break;
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      impl->local_only = g_value_get_boolean (value);
      break;
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      set_preview_widget (impl, g_value_get_object (value));
      break;
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      impl->preview_widget_active = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      set_extra_widget (impl, g_value_get_object (value));
      break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      {
	gboolean select_multiple = g_value_get_boolean (value);
	if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE && select_multiple)
	  {
	    g_warning ("Multiple selection mode is not allowed in Save mode");
	    return;
	  }

	set_select_multiple (impl, select_multiple, FALSE);
      }
      break;
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      {
	gboolean show_hidden = g_value_get_boolean (value);
	if (show_hidden != impl->show_hidden)
	  {
	    impl->show_hidden = show_hidden;
	    _gtk_file_system_model_set_show_hidden (impl->tree_model, show_hidden);
	    _gtk_file_system_model_set_show_hidden (impl->list_model, show_hidden);
	  }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_default_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_value_set_enum (value, impl->action);
      break;
    case GTK_FILE_CHOOSER_PROP_FILTER:
      g_value_set_object (value, impl->current_filter);
      break;
    case GTK_FILE_CHOOSER_PROP_FOLDER_MODE:
      g_value_set_boolean (value, impl->folder_mode);
      break;
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, impl->local_only);
      break;
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      g_value_set_object (value, impl->preview_widget);
      break;
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      g_value_set_boolean (value, impl->preview_widget_active);
      break;
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      g_value_set_object (value, impl->extra_widget);
      break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, impl->select_multiple);
      break;
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, impl->show_hidden);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* We override show-all since we have internal widgets that
 * shouldn't be shown when you call show_all(), like the filter
 * combo box.
 */
static void
gtk_file_chooser_default_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}

static void
expand_and_select_func (GtkFileSystemModel *model,
			GtkTreePath        *path,
			GtkTreeIter        *iter,
			gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkTreeView *tree_view;

  if (model == impl->tree_model)
    tree_view = GTK_TREE_VIEW (impl->tree);
  else
    tree_view = GTK_TREE_VIEW (impl->list);

  gtk_tree_view_expand_to_path (tree_view, path);
  gtk_tree_view_expand_row (tree_view, path, FALSE);
  gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->tree), path, NULL, TRUE, 0.3, 0.5);
}

static gboolean
list_model_filter_func (GtkFileSystemModel *model,
			GtkFilePath        *path,
			const GtkFileInfo  *file_info,
			gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkFileFilterInfo filter_info;
  GtkFileFilterFlags needed;
  gboolean result;

  if (!impl->current_filter)
    return TRUE;

  if (gtk_file_info_get_is_folder (file_info))
    return TRUE;

  filter_info.contains = GTK_FILE_FILTER_DISPLAY_NAME | GTK_FILE_FILTER_MIME_TYPE;

  needed = gtk_file_filter_get_needed (impl->current_filter);

  filter_info.display_name = gtk_file_info_get_display_name (file_info);
  filter_info.mime_type = gtk_file_info_get_mime_type (file_info);

  if (needed & GTK_FILE_FILTER_FILENAME)
    {
      filter_info.filename = gtk_file_system_path_to_filename (impl->file_system, path);
      if (filter_info.filename)
	filter_info.contains |= GTK_FILE_FILTER_FILENAME;
    }
  else
    filter_info.filename = NULL;

  if (needed & GTK_FILE_FILTER_URI)
    {
      filter_info.uri = gtk_file_system_path_to_uri (impl->file_system, path);
      if (filter_info.filename)
	filter_info.contains |= GTK_FILE_FILTER_URI;
    }
  else
    filter_info.uri = NULL;

  result = gtk_file_filter_filter (impl->current_filter, &filter_info);

  if (filter_info.filename)
    g_free ((gchar *)filter_info.filename);
  if (filter_info.uri)
    g_free ((gchar *)filter_info.uri);

  return result;
}

static void
install_list_model_filter (GtkFileChooserDefault *impl)
{
  if (impl->current_filter)
    _gtk_file_system_model_set_filter (impl->list_model,
				       list_model_filter_func,
				       impl);
}

#define COMPARE_DIRECTORIES										       \
  GtkFileChooserDefault *impl = user_data;								       \
  const GtkFileInfo *info_a = _gtk_file_system_model_get_info (impl->list_model, a);			       \
  const GtkFileInfo *info_b = _gtk_file_system_model_get_info (impl->list_model, b);			       \
  gboolean dir_a, dir_b;										       \
													       \
  if (info_a)												       \
    dir_a = gtk_file_info_get_is_folder (info_a);							       \
  else													       \
    return impl->list_sort_ascending ? -1 : 1;								       \
													       \
  if (info_b)												       \
    dir_b = gtk_file_info_get_is_folder (info_b);							       \
  else													       \
    return impl->list_sort_ascending ? 1 : -1;  							       \
													       \
  if (dir_a != dir_b)											       \
    return impl->list_sort_ascending ? (dir_a ? -1 : 1) : (dir_a ? 1 : -1) /* Directories *always* go first */

/* Sort callback for the filename column */
static gint
name_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    return strcmp (gtk_file_info_get_display_key (info_a), gtk_file_info_get_display_key (info_b));
}

/* Sort callback for the size column */
static gint
size_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      gint64 size_a = gtk_file_info_get_size (info_a);
      gint64 size_b = gtk_file_info_get_size (info_b);

      return size_a > size_b ? -1 : (size_a == size_b ? 0 : 1);
    }
}

/* Sort callback for the mtime column */
static gint
mtime_sort_func (GtkTreeModel *model,
		 GtkTreeIter  *a,
		 GtkTreeIter  *b,
		 gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      GtkFileTime ta = gtk_file_info_get_modification_time (info_a);
      GtkFileTime tb = gtk_file_info_get_modification_time (info_b);

      return ta > tb ? -1 : (ta == tb ? 0 : 1);
    }
}

/* Callback used when the sort column changes.  We cache the sort order for use
 * in name_sort_func().
 */
static void
list_sort_column_changed_cb (GtkTreeSortable       *sortable,
			     GtkFileChooserDefault *impl)
{
  GtkSortType sort_type;

  if (gtk_tree_sortable_get_sort_column_id (sortable, NULL, &sort_type))
    impl->list_sort_ascending = (sort_type == GTK_SORT_ASCENDING);
}

/* Gets rid of the old list model and creates a new one for the current folder */
static void
set_list_model (GtkFileChooserDefault *impl)
{
  if (impl->list_model)
    {
      g_object_unref (impl->list_model);
      g_object_unref (impl->sort_model);
    }

  impl->list_model = _gtk_file_system_model_new (impl->file_system,
						 impl->current_folder, 0,
						 GTK_FILE_INFO_ALL);
  _gtk_file_system_model_set_show_hidden (impl->list_model, impl->show_hidden);
  install_list_model_filter (impl);

  impl->sort_model = (GtkTreeModelSort *)gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (impl->list_model));
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_NAME, name_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_SIZE, size_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_MTIME, mtime_sort_func, impl, NULL);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (impl->sort_model), NULL, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_NAME, GTK_SORT_ASCENDING);
  impl->list_sort_ascending = TRUE;

  g_signal_connect (impl->sort_model, "sort_column_changed",
		    G_CALLBACK (list_sort_column_changed_cb), impl);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->list),
			   GTK_TREE_MODEL (impl->sort_model));
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (impl->list));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (impl->list),
				   GTK_FILE_SYSTEM_MODEL_DISPLAY_NAME);
}

/* Gets rid of the old folder tree model and creates a new one for the volume
 * corresponding to the specified path.
 */
static void
set_tree_model (GtkFileChooserDefault *impl, const GtkFilePath *path)
{
  GtkFileSystemVolume *volume;
  GtkFilePath *base_path, *parent_path;

  base_path = NULL;
  
  volume = gtk_file_system_get_volume_for_path (impl->file_system, path);
  
  if (volume)
    base_path = gtk_file_system_volume_get_base_path (impl->file_system, volume);
  
  if (base_path == NULL)
    {
      base_path = gtk_file_path_copy (path);
      while (gtk_file_system_get_parent (impl->file_system,
					 base_path,
					 &parent_path,
					 NULL) &&
	     parent_path != NULL)
	{
	  gtk_file_path_free (base_path);
	  base_path = parent_path;
	}
    }

  if (impl->current_volume_path && gtk_file_path_compare (base_path, impl->current_volume_path) == 0)
    goto out;

  if (impl->tree_model)
    g_object_unref (impl->tree_model);

  impl->current_volume_path = gtk_file_path_copy (base_path);

  impl->tree_model = _gtk_file_system_model_new (impl->file_system, impl->current_volume_path, -1,
						 GTK_FILE_INFO_DISPLAY_NAME);
  _gtk_file_system_model_set_show_files (impl->tree_model, FALSE);
  _gtk_file_system_model_set_show_hidden (impl->tree_model, impl->show_hidden);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->tree),
			   GTK_TREE_MODEL (impl->tree_model));

 out:

  gtk_file_path_free (base_path);
  if (volume) 
    gtk_file_system_volume_free (impl->file_system, volume);
}

static void
update_chooser_entry (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
  const GtkFileInfo *info;
  GtkTreeIter iter;
  GtkTreeIter child_iter;

  /* FIXME #132255: Fixing this for multiple selection involves getting the full
   * selection and diffing to find out what the most recently selected file is;
   * there is logic in GtkFileSelection that probably can be copied;
   * check_preview_change() is similar.
   */
  if (impl->select_multiple
      || !gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  &iter);

  info = _gtk_file_system_model_get_info (impl->list_model, &child_iter);

  if (!gtk_file_info_get_is_folder (info))
    _gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (impl->entry),
					   gtk_file_info_get_display_name (info));
}

static void
gtk_file_chooser_default_set_current_folder (GtkFileChooser    *chooser,
					     const GtkFilePath *path)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  char *str;

  if (impl->current_folder)
    gtk_file_path_free (impl->current_folder);

  impl->current_folder = gtk_file_path_copy (path);

  /* Change the current folder label */

  str = g_strdup_printf (_("Current folder: %s"), gtk_file_path_get_string (path));
  gtk_label_set_text (GTK_LABEL (impl->folder_label), str);
  g_free (str);

  /* Update the folder tree */

  if (!impl->changing_folder)
    {
      impl->changing_folder = TRUE;
      set_tree_model (impl, impl->current_folder);
      _gtk_file_system_model_path_do (impl->tree_model, path,
				      expand_and_select_func, impl);
      impl->changing_folder = FALSE;
    }

  /* Notify the location entry */

  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (impl->entry), impl->current_folder);

  /* Create a new list model */
  set_list_model (impl);

  /* Refresh controls */

  shortcuts_unselect_all (impl);
  toolbar_check_sensitivity (impl);

  g_signal_emit_by_name (impl, "current-folder-changed", 0);

  update_chooser_entry (impl);
  check_preview_change (impl);
  bookmarks_check_add_sensitivity (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);
}

static GtkFilePath *
gtk_file_chooser_default_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return gtk_file_path_copy (impl->current_folder);
}

static void
gtk_file_chooser_default_set_current_name (GtkFileChooser *chooser,
					   const gchar    *name)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  _gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (impl->entry), name);
}

static void
select_func (GtkFileSystemModel *model,
	     GtkTreePath        *path,
	     GtkTreeIter        *iter,
	     gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkTreeView *tree_view = GTK_TREE_VIEW (impl->list);
  GtkTreePath *sorted_path;

  sorted_path = gtk_tree_model_sort_convert_child_path_to_path (impl->sort_model, path);
  gtk_tree_view_set_cursor (tree_view, sorted_path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->tree), sorted_path, NULL, TRUE, 0.3, 0.0);
  gtk_tree_path_free (sorted_path);
}

static void
gtk_file_chooser_default_select_path (GtkFileChooser    *chooser,
				      const GtkFilePath *path)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkFilePath *parent_path;
  GError *error;

  error = NULL;
  if (!gtk_file_system_get_parent (impl->file_system, path, &parent_path, &error))
    {
      error_getting_info_dialog (impl, path, error);
      return;
    }

  if (!parent_path)
    {
      _gtk_file_chooser_set_current_folder_path (chooser, path);
    }
  else
    {
      _gtk_file_chooser_set_current_folder_path (chooser, parent_path);
      gtk_file_path_free (parent_path);
      _gtk_file_system_model_path_do (impl->list_model, path,
				      select_func, impl);
    }
}

static void
unselect_func (GtkFileSystemModel *model,
	       GtkTreePath        *path,
	       GtkTreeIter        *iter,
	       gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkTreeView *tree_view = GTK_TREE_VIEW (impl->list);
  GtkTreePath *sorted_path;

  sorted_path = gtk_tree_model_sort_convert_child_path_to_path (impl->sort_model,
								path);
  gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (tree_view),
				    sorted_path);
  gtk_tree_path_free (sorted_path);
}

static void
gtk_file_chooser_default_unselect_path (GtkFileChooser    *chooser,
					const GtkFilePath *path)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  _gtk_file_system_model_path_do (impl->list_model, path,
				 unselect_func, impl);
}

static void
gtk_file_chooser_default_select_all (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  if (impl->select_multiple)
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
      gtk_tree_selection_select_all (selection);
    }
}

static void
gtk_file_chooser_default_unselect_all (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));

  gtk_tree_selection_unselect_all (selection);
}

struct get_paths_closure {
  GtkFileChooserDefault *impl;
  GSList *result;
  GtkFilePath *path_from_entry;
};

static void
get_paths_foreach (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer      data)
{
  struct get_paths_closure *info;
  const GtkFilePath *file_path;
  GtkFileSystemModel *fs_model;
  GtkTreeIter sel_iter;

  info = data;

  if (info->impl->folder_mode)
    {
      fs_model = info->impl->tree_model;
      sel_iter = *iter;
    }
  else
    {
      fs_model = info->impl->list_model;
      gtk_tree_model_sort_convert_iter_to_child_iter (info->impl->sort_model, &sel_iter, iter);
    }

  file_path = _gtk_file_system_model_get_path (fs_model, &sel_iter);

  if (!info->path_from_entry
      || gtk_file_path_compare (info->path_from_entry, file_path) != 0)
    info->result = g_slist_prepend (info->result, gtk_file_path_copy (file_path));
}

static GSList *
gtk_file_chooser_default_get_paths (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkFileChooserEntry *chooser_entry;
  const GtkFilePath *folder_path;
  const gchar *file_part;
  struct get_paths_closure info;

  info.impl = impl;
  info.result = NULL;
  info.path_from_entry = NULL;

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (impl->entry);
  folder_path = _gtk_file_chooser_entry_get_current_folder (chooser_entry);
  file_part = _gtk_file_chooser_entry_get_file_part (chooser_entry);

  if (file_part != NULL && file_part[0] != '\0')
    {
      GtkFilePath *selected;
      GError *error = NULL;

      selected = gtk_file_system_make_path (impl->file_system, folder_path, file_part, &error);

      if (!selected)
	{
	  error_building_filename_dialog (impl, folder_path, file_part, error);
	  return NULL;
	}

      info.path_from_entry = selected;
    }

  if (!info.path_from_entry || impl->select_multiple)
    {
      GtkTreeSelection *selection;

      selection = NULL;

      if (impl->folder_mode)
	{
	  if (impl->tree_model)
	    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->tree));
	}
      else
	{
	  if (impl->sort_model)
	    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
	}

      if (selection)
	gtk_tree_selection_selected_foreach (selection, get_paths_foreach, &info);
    }

  if (info.path_from_entry)
    info.result = g_slist_prepend (info.result, info.path_from_entry);

  return g_slist_reverse (info.result);
}

static GtkFilePath *
gtk_file_chooser_default_get_preview_path (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  if (impl->preview_path)
    return gtk_file_path_copy (impl->preview_path);
  else
    return NULL;
}

static GtkFileSystem *
gtk_file_chooser_default_get_file_system (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return impl->file_system;
}

/* Shows or hides the filter widgets */
static void
toolbar_show_filters (GtkFileChooserDefault *impl,
		      gboolean               show)
{
  if (show)
    gtk_widget_show (impl->filter_combo);
  else
    gtk_widget_hide (impl->filter_combo);
}

static void
gtk_file_chooser_default_add_filter (GtkFileChooser *chooser,
				     GtkFileFilter  *filter)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  const gchar *name;

  if (g_slist_find (impl->filters, filter))
    {
      g_warning ("gtk_file_chooser_add_filter() called on filter already in list\n");
      return;
    }

  g_object_ref (filter);
  gtk_object_sink (GTK_OBJECT (filter));
  impl->filters = g_slist_append (impl->filters, filter);

  name = gtk_file_filter_get_name (filter);
  if (!name)
    name = "Untitled filter";	/* Place-holder, doesn't need to be marked for translation */

  gtk_combo_box_append_text (GTK_COMBO_BOX (impl->filter_combo), name);

  if (!g_slist_find (impl->filters, impl->current_filter))
    set_current_filter (impl, filter);

  toolbar_show_filters (impl, TRUE);
}

static void
gtk_file_chooser_default_remove_filter (GtkFileChooser *chooser,
					GtkFileFilter  *filter)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint filter_index;

  filter_index = g_slist_index (impl->filters, filter);

  if (filter_index < 0)
    {
      g_warning ("gtk_file_chooser_remove_filter() called on filter not in list\n");
      return;
    }

  impl->filters = g_slist_remove (impl->filters, filter);

  if (filter == impl->current_filter)
    {
      if (impl->filters)
	set_current_filter (impl, impl->filters->data);
      else
	set_current_filter (impl, NULL);
    }

  /* Remove row from the combo box */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (impl->filter_combo));
  gtk_tree_model_iter_nth_child  (model, &iter, NULL, filter_index);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  g_object_unref (filter);

  if (!impl->filters)
    toolbar_show_filters (impl, FALSE);
}

static GSList *
gtk_file_chooser_default_list_filters (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return g_slist_copy (impl->filters);
}

/* Returns the position in the shortcuts tree where the nth specified shortcut would appear */
static int
shortcuts_get_pos_for_shortcut_folder (GtkFileChooserDefault *impl,
				       int                    pos)
{
  return pos + shortcuts_get_index (impl, SHORTCUTS_SHORTCUTS);
}

static gboolean
gtk_file_chooser_default_add_shortcut_folder (GtkFileChooser    *chooser,
					      const GtkFilePath *path,
					      GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  gboolean result;
  int pos;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, impl->num_shortcuts);

  result = shortcuts_insert_path (impl, pos, FALSE, NULL, path, NULL, FALSE, error);

  if (result)
    impl->num_shortcuts++;

  return result;
}

static gboolean
gtk_file_chooser_default_remove_shortcut_folder (GtkFileChooser    *chooser,
						 const GtkFilePath *path,
						 GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  int pos;
  GtkTreeIter iter;
  int i;

  if (impl->num_shortcuts == 0)
    goto out;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, 0);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  for (i = 0; i < impl->num_shortcuts; i++)
    {
      GtkFilePath *shortcut;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter, SHORTCUTS_COL_PATH, &shortcut, -1);
      g_assert (shortcut != NULL);

      if (gtk_file_path_compare (shortcut, path) == 0)
	{
	  /* The other columns are freed by the GtkTreeStore */
	  gtk_file_path_free (shortcut);
	  gtk_tree_store_remove (impl->shortcuts_model, &iter);
	  impl->num_shortcuts--;
	  return TRUE;
	}

      if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	g_assert_not_reached ();
    }

 out:

  g_set_error (error,
	       GTK_FILE_CHOOSER_ERROR,
	       GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
	       _("shortcut %s does not exist"),
	       gtk_file_path_get_string (path));

  return FALSE;
}

static GSList *
gtk_file_chooser_default_list_shortcut_folders (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  int pos;
  GtkTreeIter iter;
  int i;
  GSList *list;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, 0);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  list = NULL;

  for (i = 0; i < impl->num_shortcuts; i++)
    {
      GtkFilePath *shortcut;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter, SHORTCUTS_COL_PATH, &shortcut, -1);
      g_assert (shortcut != NULL);

      list = g_slist_prepend (list, gtk_file_path_copy (shortcut));

      if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	g_assert_not_reached ();
    }

  return g_slist_reverse (list);
}

static void
set_current_filter (GtkFileChooserDefault *impl,
		    GtkFileFilter         *filter)
{
  if (impl->current_filter != filter)
    {
      int filter_index;

      /* If we have filters, new filter must be one of them
       */
      filter_index = g_slist_index (impl->filters, filter);
      if (impl->filters && filter_index < 0)
	return;

      if (impl->current_filter)
	g_object_unref (impl->current_filter);
      impl->current_filter = filter;
      if (impl->current_filter)
	{
	  g_object_ref (impl->current_filter);
	  gtk_object_sink (GTK_OBJECT (filter));
	}

      if (impl->filters)
	gtk_combo_box_set_active (GTK_COMBO_BOX (impl->filter_combo),
				  filter_index);

      install_list_model_filter (impl);

      g_object_notify (G_OBJECT (impl), "filter");
    }
}

static void
open_and_close (GtkTreeView *tree_view,
		GtkTreePath *target_path)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreeIter iter;
  GtkTreePath *path;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);

  gtk_tree_model_get_iter (model, &iter, path);

  while (TRUE)
    {
      if (gtk_tree_path_is_ancestor (path, target_path) ||
	  gtk_tree_path_compare (path, target_path) == 0)
	{
	  GtkTreeIter child_iter;
	  gtk_tree_view_expand_row (tree_view, path, FALSE);
	  if (gtk_tree_model_iter_children (model, &child_iter, &iter))
	    {
	      iter = child_iter;
	      gtk_tree_path_down (path);
	      goto next;
	    }
	}
      else
	gtk_tree_view_collapse_row (tree_view, path);

      while (TRUE)
	{
	  GtkTreeIter parent_iter;
	  GtkTreeIter next_iter;

	  next_iter = iter;
	  if (gtk_tree_model_iter_next (model, &next_iter))
	    {
	      iter = next_iter;
	      gtk_tree_path_next (path);
	      goto next;
	    }

	  if (!gtk_tree_model_iter_parent (model, &parent_iter, &iter))
	    goto out;

	  iter = parent_iter;
	  gtk_tree_path_up (path);
	}
    next:
      ;
    }

 out:
  gtk_tree_path_free (path);
}

static void
filter_combo_changed (GtkComboBox           *combo_box,
		      GtkFileChooserDefault *impl)
{
  gint new_index = gtk_combo_box_get_active (combo_box);
  GtkFileFilter *new_filter = g_slist_nth_data (impl->filters, new_index);

  set_current_filter (impl, new_filter);
}

static void
check_preview_change (GtkFileChooserDefault *impl)
{
  const GtkFilePath *new_path = NULL;

  /* FIXME #132255: Fixing preview for multiple selection involves getting the
   * full selection and diffing to find out what the most recently selected file
   * is; there is logic in GtkFileSelection that probably can be
   * copied. update_chooser_entry() is similar.
   */
  if (impl->sort_model && !impl->select_multiple)
    {
      GtkTreeSelection *selection;
      GtkTreeIter iter;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
      if (gtk_tree_selection_get_selected  (selection, NULL, &iter))
	{
	  GtkTreeIter child_iter;

	  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
							  &child_iter, &iter);

	  new_path = _gtk_file_system_model_get_path (impl->list_model, &child_iter);
	}
    }

  if (new_path != impl->preview_path &&
      !(new_path && impl->preview_path &&
	gtk_file_path_compare (new_path, impl->preview_path) == 0))
    {
      if (impl->preview_path)
	gtk_file_path_free (impl->preview_path);

      if (new_path)
	impl->preview_path = gtk_file_path_copy (new_path);
      else
	impl->preview_path = NULL;

      g_signal_emit_by_name (impl, "update-preview");
    }
}

static void
tree_selection_changed (GtkTreeSelection      *selection,
			GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  const GtkFilePath *file_path;
  GtkTreePath *path;

  /* FIXME #132255: Fixing this for multiple selection involves getting the full
   * selection and diffing to find out what the most recently selected file is;
   * there is logic in GtkFileSelection that probably can be copied;
   * check_preview_change() is similar.
   */
  if (impl->select_multiple
      || !gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  file_path = _gtk_file_system_model_get_path (impl->tree_model, &iter);
  if (impl->current_folder && gtk_file_path_compare (file_path, impl->current_folder) == 0)
    return;

  /* Close the tree up to only the parents of the newly selected
   * node and it's immediate children are visible.
   */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->tree_model), &iter);
  open_and_close (GTK_TREE_VIEW (impl->tree), path);
  gtk_tree_path_free (path);

  if (!impl->changing_folder)
    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), file_path);
}

/* Activates a volume by mounting it if necessary and then switching to its
 * base path.
 */
static void
shortcuts_activate_volume (GtkFileChooserDefault *impl,
			   GtkFileSystemVolume   *volume)
{
  GtkFilePath *path;

  if (!gtk_file_system_volume_get_is_mounted (impl->file_system, volume))
    {
      GError *error;

      error = NULL;
      if (!gtk_file_system_volume_mount (impl->file_system, volume, &error))
	{
	  char *msg;

	  msg = g_strdup_printf ("Could not mount %s:\n%s",
				 gtk_file_system_volume_get_display_name (impl->file_system, volume),
				 error->message);
	  error_message (impl, msg);
	  g_free (msg);
	  g_error_free (error);

	  return;
	}
    }

  path = gtk_file_system_volume_get_base_path (impl->file_system, volume);
  _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), path);
  gtk_file_path_free (path);
}

/* Callback used when a row in the shortcuts list is activated */
static void
shortcuts_row_activated_cb (GtkTreeView           *tree_view,
			    GtkTreePath           *path,
			    GtkTreeViewColumn     *column,
			    GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  int selected, start_row;
  gpointer data;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->shortcuts_model), &iter, path))
    return;

  selected = *gtk_tree_path_get_indices (path);

  if (selected == shortcuts_get_index (impl, SHORTCUTS_SEPARATOR))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter, SHORTCUTS_COL_PATH, &data, -1);

  start_row = shortcuts_get_index (impl, SHORTCUTS_VOLUMES);
  if (selected >= start_row && selected < start_row + impl->num_volumes)
    {
      GtkFileSystemVolume *volume;

      volume = data;
      shortcuts_activate_volume (impl, volume);
    }
  else
    {
      GtkFilePath *file_path;

      file_path = data;
      _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), file_path);
    }
}

static gboolean
shortcuts_select_func  (GtkTreeSelection  *selection,
			GtkTreeModel      *model,
			GtkTreePath       *path,
			gboolean           path_currently_selected,
			gpointer           data)
{
  GtkFileChooserDefault *impl = data;

  return (*gtk_tree_path_get_indices (path) != shortcuts_get_index (impl, SHORTCUTS_SEPARATOR));
}

static void
list_selection_changed (GtkTreeSelection      *selection,
			GtkFileChooserDefault *impl)
{
  /* See if we are in the new folder editable row for Save mode */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      GtkTreeSelection *selection;
      GtkTreeIter iter, child_iter;
      const GtkFileInfo *info;

      g_assert (!impl->select_multiple);
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
      if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
	return;

      gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						      &child_iter,
						      &iter);

      info = _gtk_file_system_model_get_info (impl->list_model, &child_iter);
      if (!info)
	return; /* We are on the editable row for New Folder */
    }

  update_chooser_entry (impl);
  check_preview_change (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);
}

/* Callback used when a row in the file list is activated */
static void
list_row_activated (GtkTreeView           *tree_view,
		    GtkTreePath           *path,
		    GtkTreeViewColumn     *column,
		    GtkFileChooserDefault *impl)
{
  GtkTreeIter iter, child_iter;
  const GtkFileInfo *info;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->sort_model), &iter, path))
    return;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model, &child_iter, &iter);

  info = _gtk_file_system_model_get_info (impl->list_model, &child_iter);

  if (gtk_file_info_get_is_folder (info))
    {
      const GtkFilePath *file_path;

      file_path = _gtk_file_system_model_get_path (impl->list_model, &child_iter);
      _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), file_path);

      return;
    }

  g_signal_emit_by_name (impl, "file-activated");
}

static void
entry_activate (GtkEntry              *entry,
		GtkFileChooserDefault *impl)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (entry);
  const GtkFilePath *folder_path = _gtk_file_chooser_entry_get_current_folder (chooser_entry);
  const gchar *file_part = _gtk_file_chooser_entry_get_file_part (chooser_entry);
  GtkFilePath *new_folder = NULL;

  if (!folder_path)
    return; /* The entry got a nonexistent path */

  if (file_part[0] == '\0')
    {
      if (gtk_file_path_compare (impl->current_folder, folder_path) != 0)
	new_folder = gtk_file_path_copy (folder_path);
      else
	return;
    }
  else
    {
      GtkFileFolder *folder = NULL;
      GtkFilePath *subfolder_path = NULL;
      GtkFileInfo *info = NULL;
      GError *error;

      /* If the file part is non-empty, we need to figure out if it
       * refers to a folder within folder. We could optimize the case
       * here where the folder is already loaded for one of our tree models.
       */

      error = NULL;
      folder = gtk_file_system_get_folder (impl->file_system, folder_path, GTK_FILE_INFO_IS_FOLDER, &error);

      if (!folder)
	{
	  error_getting_info_dialog (impl, folder_path, error);
	  return;
	}

      error = NULL;
      subfolder_path = gtk_file_system_make_path (impl->file_system, folder_path, file_part, &error);

      if (!subfolder_path)
	{
	  char *msg;

	  msg = g_strdup_printf (_("Could not build file name from '%s' and '%s':\n%s"),
				 gtk_file_path_get_string (folder_path),
				 file_part,
				 error->message);
	  error_message (impl, msg);
	  g_free (msg);
	  g_object_unref (folder);
	  return;
	}

      error = NULL;
      info = gtk_file_folder_get_info (folder, subfolder_path, &error);

      if (!info)
	{
	  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    {
	      g_object_unref (folder);
	      gtk_file_path_free (subfolder_path);
	      return;
	    }
	  error_getting_info_dialog (impl, subfolder_path, error);
	  g_object_unref (folder);
	  gtk_file_path_free (subfolder_path);
	  return;
	}

      if (gtk_file_info_get_is_folder (info))
	new_folder = gtk_file_path_copy (subfolder_path);

      g_object_unref (folder);
      gtk_file_path_free (subfolder_path);
      gtk_file_info_free (info);
    }

  if (new_folder)
    {
      g_signal_stop_emission_by_name (entry, "activate");

      _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), new_folder);
      _gtk_file_chooser_entry_set_file_part (chooser_entry, "");

      gtk_file_path_free (new_folder);
    }
}

static const GtkFileInfo *
get_list_file_info (GtkFileChooserDefault *impl,
		    GtkTreeIter           *iter)
{
  GtkTreeIter child_iter;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  iter);

  return _gtk_file_system_model_get_info (impl->list_model, &child_iter);
}

static void
tree_name_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  const GtkFileInfo *info = _gtk_file_system_model_get_info (impl->tree_model, iter);

  if (info)
    {
      g_object_set (cell,
		    "text", gtk_file_info_get_display_name (info),
		    NULL);
    }
}

static void
list_icon_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  GtkTreeIter child_iter;
  const GtkFilePath *path;
  GdkPixbuf *pixbuf;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  iter);
  path = _gtk_file_system_model_get_path (impl->list_model, &child_iter);
  if (!path)
    return;

  /* FIXME: NULL GError */
  pixbuf = gtk_file_system_render_icon (impl->file_system, path, GTK_WIDGET (impl), ICON_SIZE, NULL);
  g_object_set (cell,
		"pixbuf", pixbuf,
		NULL);

  if (pixbuf)
    g_object_unref (pixbuf);

#if 0
  const GtkFileInfo *info = get_list_file_info (impl, iter);

  if (info)
    {
      GtkWidget *widget = GTK_TREE_VIEW_COLUMN (tree_column)->tree_view;
      GdkPixbuf *pixbuf = gtk_file_info_render_icon (info, widget, ICON_SIZE);

      g_object_set (cell,
		    "pixbuf", pixbuf,
		    NULL);

      if (pixbuf)
	g_object_unref (pixbuf);
    }
#endif
}

/* Sets a cellrenderer's text, making it bold if the GtkFileInfo is a folder */
static void
set_cell_text_bold_if_folder (const GtkFileInfo *info, GtkCellRenderer *cell, const char *text)
{
  g_object_set (cell,
		"text", text,
		"weight", gtk_file_info_get_is_folder (info) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		NULL);
}

static void
list_name_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  const GtkFileInfo *info = get_list_file_info (impl, iter);

  if (!info)
    {
      g_object_set (cell,
		    "text", _("Type name of new folder"),
		    NULL);
      return;
    }

  set_cell_text_bold_if_folder (info, cell, gtk_file_info_get_display_name (info));
}

#if 0
static void
list_size_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  const GtkFileInfo *info = get_list_file_info (impl, iter);
  gint64 size;
  gchar *str;

  if (!info || gtk_file_info_get_is_folder (info))
    return;

  size = gtk_file_info_get_size (info);

  if (size < (gint64)1024)
    str = g_strdup_printf (ngettext ("%d byte", "%d bytes", (gint)size), (gint)size);
  else if (size < (gint64)1024*1024)
    str = g_strdup_printf (_("%.1f K"), size / (1024.));
  else if (size < (gint64)1024*1024*1024)
    str = g_strdup_printf (_("%.1f M"), size / (1024.*1024.));
  else
    str = g_strdup_printf (_("%.1f G"), size / (1024.*1024.*1024.));

  g_object_set (cell,
		"text", str,
		NULL);

  g_free (str);
}
#endif

/* Tree column data callback for the file list; fetches the mtime of a file */
static void
list_mtime_data_func (GtkTreeViewColumn *tree_column,
		      GtkCellRenderer   *cell,
		      GtkTreeModel      *tree_model,
		      GtkTreeIter       *iter,
		      gpointer           data)
{
  GtkFileChooserDefault *impl;
  const GtkFileInfo *info;
  GtkFileTime time_mtime, time_now;
  GDate mtime, now;
  int days_diff;
  char buf[256];

  impl = data;

  info = get_list_file_info (impl, iter);
  if (!info)
    {
      g_object_set (cell,
		    "text", "",
		    NULL);
      return;
    }

  time_mtime = gtk_file_info_get_modification_time (info);
  g_date_set_time (&mtime, (GTime) time_mtime);

  time_now = (GTime ) time (NULL);
  g_date_set_time (&now, (GTime) time_now);

  days_diff = g_date_get_julian (&now) - g_date_get_julian (&mtime);

  if (days_diff == 0)
    strcpy (buf, _("Today"));
  else if (days_diff == 1)
    strcpy (buf, _("Yesterday"));
  else
    {
      char *format;

      if (days_diff > 1 && days_diff < 7)
	format = "%A"; /* Days from last week */
      else
	format = _("%d/%b/%Y"); /* Any other date */

      if (g_date_strftime (buf, sizeof (buf), format, &mtime) == 0)
	strcpy (buf, _("Unknown"));
    }

  set_cell_text_bold_if_folder (info, cell, buf);
}

GtkWidget *
_gtk_file_chooser_default_new (const char *file_system)
{
  return  g_object_new (GTK_TYPE_FILE_CHOOSER_DEFAULT,
			"file-system-backend", file_system,
			NULL);
}
