/* GTK - The GIMP Toolkit
 * gtkfilechooserwidget.c: Embeddable file selector widget
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

#include "gtkfilechooserimpldefault.h"
#include "gtkfilechooserenums.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilesystemmodel.h"

#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkvbox.h>

typedef struct _GtkFileChooserImplDefaultClass GtkFileChooserImplDefaultClass;

#define GTK_FILE_CHOOSER_IMPL_DEFAULT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_IMPL_DEFAULT, GtkFileChooserImplDefaultClass))
#define GTK_IS_FILE_CHOOSER_IMPL_DEFAULT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_IMPL_DEFAULT))
#define GTK_FILE_CHOOSER_IMPL_DEFAULT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_IMPL_DEFAULT, GtkFileChooserImplDefaultClass))

struct _GtkFileChooserImplDefaultClass
{
  GtkVBoxClass parent_class;
};

struct _GtkFileChooserImplDefault
{
  GtkVBox parent_instance;

  GtkFileSystem *file_system;
  GtkFileSystemModel *tree_model;
  GtkFileSystemModel *list_model;
  GtkTreeModelSort *sort_model;

  GtkFileChooserAction action;

  guint folder_mode : 1;
  guint local_only : 1;
  guint preview_widget_active : 1;
  guint select_multiple : 1;
  guint show_hidden : 1;

  GtkWidget *tree_scrollwin;
  GtkWidget *tree;
  GtkWidget *list_scrollwin;
  GtkWidget *list;
  GtkWidget *preview_widget;
};

static void gtk_file_chooser_impl_default_class_init   (GtkFileChooserImplDefaultClass *class);
static void gtk_file_chooser_impl_default_iface_init   (GtkFileChooserIface            *iface);
static void gtk_file_chooser_impl_default_init         (GtkFileChooserImplDefault      *impl);
static void gtk_file_chooser_impl_default_set_property (GObject                        *object,
							guint                           prop_id,
							const GValue                   *value,
							GParamSpec                     *pspec);
static void gtk_file_chooser_impl_default_get_property (GObject                        *object,
							guint                           prop_id,
							GValue                         *value,
							GParamSpec                     *pspec);

static void    gtk_file_chooser_impl_default_set_current_folder (GtkFileChooser *chooser,
								 const char     *uri);
static char *  gtk_file_chooser_impl_default_get_current_folder (GtkFileChooser *chooser);
static void    gtk_file_chooser_impl_default_select_uri         (GtkFileChooser *chooser,
								 const char     *uri);
static void    gtk_file_chooser_impl_default_unselect_uri       (GtkFileChooser *chooser,
								 const char     *uri);
static void    gtk_file_chooser_impl_default_select_all         (GtkFileChooser *chooser);
static void    gtk_file_chooser_impl_default_unselect_all       (GtkFileChooser *chooser);
static GSList *gtk_file_chooser_impl_default_get_uris           (GtkFileChooser *chooser);

static void tree_selection_changed (GtkTreeSelection          *tree_selection,
				    GtkFileChooserImplDefault *impl);
static void list_selection_changed (GtkTreeSelection          *tree_selection,
				    GtkFileChooserImplDefault *impl);

GType
_gtk_file_chooser_impl_default_get_type (void)
{
  static GType file_chooser_impl_default_type = 0;

  if (!file_chooser_impl_default_type)
    {
      static const GTypeInfo file_chooser_impl_default_info =
      {
	sizeof (GtkFileChooserImplDefaultClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_chooser_impl_default_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileChooserImplDefault),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_chooser_impl_default_init,
      };
      
      static const GInterfaceInfo file_chooser_info =
      {
	(GInterfaceInitFunc) gtk_file_chooser_impl_default_iface_init, /* interface_init */
	NULL,			                                       /* interface_finalize */
	NULL			                                       /* interface_data */
      };

      file_chooser_impl_default_type = g_type_register_static (GTK_TYPE_VBOX, "GtkFileChooserImplDefault",
							 &file_chooser_impl_default_info, 0);
      g_type_add_interface_static (file_chooser_impl_default_type,
				   GTK_TYPE_FILE_CHOOSER,
				   &file_chooser_info);
    }

  return file_chooser_impl_default_type;
}

static void
gtk_file_chooser_impl_default_class_init (GtkFileChooserImplDefaultClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_file_chooser_impl_default_set_property;
  gobject_class->get_property = gtk_file_chooser_impl_default_get_property;

  _gtk_file_chooser_install_properties (gobject_class);
}

static void
gtk_file_chooser_impl_default_iface_init (GtkFileChooserIface *iface)
{
  iface->select_uri = gtk_file_chooser_impl_default_select_uri;
  iface->unselect_uri = gtk_file_chooser_impl_default_unselect_uri;
  iface->select_all = gtk_file_chooser_impl_default_select_all;
  iface->unselect_all = gtk_file_chooser_impl_default_unselect_all;
  iface->get_uris = gtk_file_chooser_impl_default_get_uris;
  iface->set_current_folder = gtk_file_chooser_impl_default_set_current_folder;
  iface->get_current_folder = gtk_file_chooser_impl_default_get_current_folder;
}

static void
gtk_file_chooser_impl_default_init (GtkFileChooserImplDefault *impl)
{
  GtkWidget *hpaned;
  GtkTreeSelection *selection;

  impl->folder_mode = FALSE;
  impl->local_only = TRUE;
  impl->preview_widget_active = TRUE;
  impl->select_multiple = FALSE;
  impl->show_hidden = FALSE;
  
  gtk_widget_push_composite_child ();

  hpaned = gtk_hpaned_new ();
  gtk_box_pack_start (GTK_BOX (impl), hpaned, TRUE, TRUE, 0);
  gtk_widget_show (hpaned);

  impl->tree_scrollwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (impl->tree_scrollwin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_paned_add1 (GTK_PANED (hpaned), impl->tree_scrollwin);
  gtk_widget_show (impl->tree_scrollwin);
  
  impl->tree = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (impl->tree), FALSE);
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->tree));
  g_signal_connect (selection, "changed",
		    G_CALLBACK (tree_selection_changed), impl);

  gtk_paned_set_position (GTK_PANED (hpaned), 200);

  gtk_container_add (GTK_CONTAINER (impl->tree_scrollwin), impl->tree);
  gtk_widget_show (impl->tree);
  
  impl->list_scrollwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (impl->list_scrollwin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_paned_add2 (GTK_PANED (hpaned), impl->list_scrollwin);
  gtk_widget_show (impl->list_scrollwin);
  
  impl->list = gtk_tree_view_new ();
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (impl->list), TRUE);
  gtk_container_add (GTK_CONTAINER (impl->list_scrollwin), impl->list);
  gtk_widget_show (impl->list);
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
  g_signal_connect (selection, "changed",
		    G_CALLBACK (list_selection_changed), impl);
  
  gtk_widget_pop_composite_child ();
}

static void
set_preview_widget (GtkFileChooserImplDefault *impl,
		    GtkWidget                 *preview_widget)
{
  if (preview_widget == impl->preview_widget)
    return;

  if (impl->preview_widget)
    {
      g_object_unref (impl->preview_widget);
      impl->preview_widget = NULL;
    }

  impl->preview_widget = preview_widget;
  if (impl->preview_widget)
    {
      g_object_ref (impl->preview_widget);
      gtk_object_sink (GTK_OBJECT (impl->preview_widget));
    }
}

static void
gtk_file_chooser_impl_default_set_property (GObject         *object,
					    guint            prop_id,
					    const GValue    *value,
					    GParamSpec      *pspec)
     
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (object);
  
  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      impl->action = g_value_get_enum (value);
      break;
    case GTK_FILE_CHOOSER_PROP_FOLDER_MODE:
      {	
	gboolean folder_mode = g_value_get_boolean (value);
	if (folder_mode != impl->folder_mode)
	  {
	    impl->folder_mode = folder_mode;
	    if (impl->folder_mode)
	      gtk_widget_hide (impl->list_scrollwin);
	    else
	      gtk_widget_show (impl->list_scrollwin);
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
      break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      {
	gboolean select_multiple = g_value_get_boolean (value);
	if (select_multiple != impl->select_multiple)
	  {
	    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
	    
	    impl->select_multiple = select_multiple;
	    gtk_tree_selection_set_mode (selection,
					 (select_multiple ?
					  GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE));
					 
	    
	  }
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
gtk_file_chooser_impl_default_get_property (GObject         *object,
					    guint            prop_id,
					    GValue          *value,
					    GParamSpec      *pspec)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (object);
  
  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_value_set_enum (value, impl->action);
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

static void
expand_and_select_func (GtkFileSystemModel *model,
			GtkTreePath        *path,
			GtkTreeIter        *iter,
			gpointer            user_data)
{
  GtkFileChooserImplDefault *impl = user_data;
  GtkTreeView *tree_view;

  if (model == impl->tree_model)
    tree_view = GTK_TREE_VIEW (impl->tree);
  else
    tree_view = GTK_TREE_VIEW (impl->list);

  gtk_tree_view_expand_to_path (tree_view, path);
  gtk_tree_view_expand_row (tree_view, path, FALSE);
  gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->tree), path, NULL, TRUE, 0.3, 0.0);
}

static void
gtk_file_chooser_impl_default_set_current_folder (GtkFileChooser *chooser,
						  const char     *uri)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);
  _gtk_file_system_model_uri_do (impl->tree_model, uri,
				 expand_and_select_func, impl);
}

static char *
gtk_file_chooser_impl_default_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->tree));
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      const gchar *uri = _gtk_file_system_model_get_uri (impl->tree_model, &iter);
      return g_strdup (uri);
    }
  else
    return NULL;
}

static void
select_func (GtkFileSystemModel *model,
	     GtkTreePath        *path,
	     GtkTreeIter        *iter,
	     gpointer            user_data)
{
  GtkFileChooserImplDefault *impl = user_data;
  GtkTreeView *tree_view = GTK_TREE_VIEW (impl->list);
  GtkTreePath *sorted_path;

  sorted_path = gtk_tree_model_sort_convert_child_path_to_path (impl->sort_model, path);
  gtk_tree_view_set_cursor (tree_view, sorted_path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->tree), sorted_path, NULL, TRUE, 0.3, 0.0);
  gtk_tree_path_free (sorted_path);
}

static void
gtk_file_chooser_impl_default_select_uri (GtkFileChooser *chooser,
					  const char     *uri)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);
  gchar *parent_uri;
  
  if (!gtk_file_system_get_parent (impl->file_system, uri, &parent_uri, NULL))	/* NULL-GError */
    return;

  if (!parent_uri)
    {
      gtk_file_chooser_set_current_folder_uri (chooser, uri);
    }
  else
    {
      gtk_file_chooser_set_current_folder_uri (chooser, parent_uri);
      g_free (parent_uri);
      _gtk_file_system_model_uri_do (impl->list_model, uri,
				     select_func, impl);
    }
}

static void
unselect_func (GtkFileSystemModel *model,
	       GtkTreePath        *path,
	       GtkTreeIter        *iter,
	       gpointer            user_data)
{
  GtkFileChooserImplDefault *impl = user_data;
  GtkTreeView *tree_view = GTK_TREE_VIEW (impl->list);
  GtkTreePath *sorted_path;

  sorted_path = gtk_tree_model_sort_convert_child_path_to_path (impl->sort_model,
								path);
  gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (tree_view),
				    sorted_path);
  gtk_tree_path_free (sorted_path);
}

static void
gtk_file_chooser_impl_default_unselect_uri (GtkFileChooser *chooser,
					    const char     *uri)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);

  _gtk_file_system_model_uri_do (impl->list_model, uri,
				 unselect_func, impl);
}

static void
gtk_file_chooser_impl_default_select_all (GtkFileChooser *chooser)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);
  if (impl->select_multiple)
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
      gtk_tree_selection_select_all (selection);
    }
}

static void
gtk_file_chooser_impl_default_unselect_all (GtkFileChooser *chooser)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
  
  gtk_tree_selection_unselect_all (selection);
}

static void
get_uris_foreach (GtkTreeModel      *model,
		  GtkTreePath       *path,
		  GtkTreeIter       *iter,
		  gpointer           data)
{
  GtkTreePath *child_path;
  GtkTreeIter child_iter;
  const gchar *uri;
  
  struct {
    GSList *result;
    GtkFileChooserImplDefault *impl;
  } *info = data;
  
  child_path = gtk_tree_model_sort_convert_path_to_child_path (info->impl->sort_model, path);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (info->impl->list_model), &child_iter, child_path);
  gtk_tree_path_free (child_path);
  
  uri = _gtk_file_system_model_get_uri (info->impl->tree_model, &child_iter);
  info->result = g_slist_prepend (info->result, g_strdup (uri));
}

static GSList *
gtk_file_chooser_impl_default_get_uris (GtkFileChooser *chooser)
{
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (chooser);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->list));
  
  struct {
    GSList *result;
    GtkFileChooserImplDefault *impl;
  } info = { NULL, };
  
  info.impl = impl;

  gtk_tree_selection_selected_foreach (selection,
				       get_uris_foreach, &info);
  return g_slist_reverse (info.result);
}

static gint
name_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  GtkFileChooserImplDefault *impl = user_data;
  const GtkFileInfo *info_a = _gtk_file_system_model_get_info (impl->tree_model, a);
  const GtkFileInfo *info_b = _gtk_file_system_model_get_info (impl->tree_model, b);

  return g_utf8_collate (gtk_file_info_get_display_name (info_a), gtk_file_info_get_display_name (info_b));
}

static gint
size_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  GtkFileChooserImplDefault *impl = user_data;
  const GtkFileInfo *info_a = _gtk_file_system_model_get_info (impl->tree_model, a);
  const GtkFileInfo *info_b = _gtk_file_system_model_get_info (impl->tree_model, b);
  gint64 size_a = gtk_file_info_get_size (info_a);
  gint64 size_b = gtk_file_info_get_size (info_b);

  return size_a > size_b ? -1 : (size_a == size_b ? 0 : 1);
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
tree_selection_changed (GtkTreeSelection          *selection,
			GtkFileChooserImplDefault *impl)
{
  GtkTreeIter iter;

  if (impl->list_model)
    {
      g_object_unref (impl->list_model);
      impl->list_model = NULL;

      g_object_unref (impl->sort_model);
      impl->sort_model = NULL;
    }

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      GtkTreePath *path;
      const gchar *uri;

      /* Close the tree up to only the parents of the newly selected
       * node and it's immediate children visible.
       */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->tree_model), &iter);
      open_and_close (GTK_TREE_VIEW (impl->tree), path);
      gtk_tree_path_free (path);
      
      /* Now update the list view to show the new row.
       */
      uri = _gtk_file_system_model_get_uri (impl->tree_model, &iter);

      impl->list_model = _gtk_file_system_model_new (impl->file_system,
						     uri, 0,
						     GTK_FILE_INFO_DISPLAY_NAME |
						     GTK_FILE_INFO_SIZE); 
      _gtk_file_system_model_set_show_folders (impl->list_model, FALSE);
      
      impl->sort_model = (GtkTreeModelSort *)gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (impl->list_model));
      gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), 0, name_sort_func, impl, NULL);
      gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), 1, size_sort_func, impl, NULL);
      gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (impl->sort_model),
					       name_sort_func, impl, NULL);
    }
      
  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->list),
			   GTK_TREE_MODEL (impl->sort_model));
  
  g_signal_emit_by_name (impl, "current_folder_changed", 0);
  g_signal_emit_by_name (impl, "selection_changed", 0);
}

static void
list_selection_changed (GtkTreeSelection          *selection,
			GtkFileChooserImplDefault *impl)
{
  g_signal_emit_by_name (impl, "selection_changed", 0);
}

const GtkFileInfo *
get_list_file_info (GtkFileChooserImplDefault *impl,
		    GtkTreeIter               *iter)
{
  GtkTreeIter child_iter;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  iter);

  return _gtk_file_system_model_get_info (impl->tree_model, &child_iter);
}

static void
tree_name_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserImplDefault *impl = data;
  const GtkFileInfo *info = _gtk_file_system_model_get_info (impl->tree_model, iter);

  if (info)
    {
      g_object_set (cell,
		    "text", gtk_file_info_get_display_name (info),
		    NULL);
    }
}

static void
list_name_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserImplDefault *impl = data;
  const GtkFileInfo *info = get_list_file_info (impl, iter);

  if (info)
    {
      g_object_set (cell,
		    "text", gtk_file_info_get_display_name (info),
		    NULL);
    }
}

static void
list_size_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserImplDefault *impl = data;
  const GtkFileInfo *info = get_list_file_info (impl, iter);

  if (info)
    {
      gint64 size = gtk_file_info_get_size (info);
      gchar *str;
      
      if (size < (gint64)1024)
	str = g_strdup_printf ("%d bytes", (gint)size);
      else if (size < (gint64)1024*1024)
	str = g_strdup_printf ("%.1f K", size / (1024.));
      else if (size < (gint64)1024*1024*1024)
	str = g_strdup_printf ("%.1f M", size / (1024.*1024.));
      else
	str = g_strdup_printf ("%.1f G", size / (1024.*1024.*1024.));

      g_object_set (cell,
		    "text", str,
		    NULL);
      
      g_free (str);
    }

}

GtkWidget *
_gtk_file_chooser_impl_default_new (GtkFileSystem *file_system)
{
  GtkWidget *result = g_object_new (GTK_TYPE_FILE_CHOOSER_IMPL_DEFAULT, NULL);
  GtkFileChooserImplDefault *impl = GTK_FILE_CHOOSER_IMPL_DEFAULT (result);
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  impl->file_system = file_system;
  impl->tree_model = _gtk_file_system_model_new (file_system, NULL, -1,
						 GTK_FILE_INFO_DISPLAY_NAME);
  _gtk_file_system_model_set_show_files (impl->tree_model, FALSE);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->tree),
			   GTK_TREE_MODEL (impl->tree_model));

  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (impl->tree), 0,
					      "File name",
					      gtk_cell_renderer_text_new (),
					      tree_name_data_func, impl, NULL);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title  (column, "File name");
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   list_name_data_func, impl, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 0);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->list), column);
  
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title  (column, "Size");
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   list_size_data_func, impl, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->list), column);

  return result;
}
