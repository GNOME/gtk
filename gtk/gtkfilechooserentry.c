/* GTK - The GIMP Toolkit
 * gtkfilechooserentry.c: Entry with filename completion
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

#include <config.h>
#include <string.h>

#include "gtkalias.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkfilechooserentry.h"
#include "gtkmain.h"

typedef struct _GtkFileChooserEntryClass GtkFileChooserEntryClass;

#define GTK_FILE_CHOOSER_ENTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_ENTRY, GtkFileChooserEntryClass))
#define GTK_IS_FILE_CHOOSER_ENTRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_ENTRY))
#define GTK_FILE_CHOOSER_ENTRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_ENTRY, GtkFileChooserEntryClass))

struct _GtkFileChooserEntryClass
{
  GtkEntryClass parent_class;
};

struct _GtkFileChooserEntry
{
  GtkEntry parent_instance;

  GtkFileChooserAction action;

  GtkFileSystem *file_system;
  GtkFilePath *base_folder;
  GtkFilePath *current_folder_path;
  gchar *file_part;
  gint file_part_pos;
  GSource *check_completion_idle;
  GSource *load_directory_idle;

  GtkFileFolder *current_folder;

  GtkListStore *completion_store;

  guint has_completion : 1;
  guint in_change      : 1;
  guint eat_tabs       : 1;
};

enum
{
  DISPLAY_NAME_COLUMN,
  PATH_COLUMN,
  N_COLUMNS
};

static void gtk_file_chooser_entry_class_init (GtkFileChooserEntryClass *class);
static void gtk_file_chooser_entry_iface_init (GtkEditableClass         *iface);
static void gtk_file_chooser_entry_init       (GtkFileChooserEntry      *chooser_entry);

static void     gtk_file_chooser_entry_finalize       (GObject          *object);
static gboolean gtk_file_chooser_entry_focus          (GtkWidget        *widget,
						       GtkDirectionType  direction);
static void     gtk_file_chooser_entry_activate       (GtkEntry         *entry);
static void     gtk_file_chooser_entry_changed        (GtkEditable      *editable);
static void     gtk_file_chooser_entry_do_insert_text (GtkEditable *editable,
						       const gchar *new_text,
						       gint         new_text_length,
						       gint        *position);

static void     clear_completion_callback (GtkFileChooserEntry *chooser_entry,
					   GParamSpec          *pspec);
static gboolean match_selected_callback   (GtkEntryCompletion  *completion,
					   GtkTreeModel        *model,
					   GtkTreeIter         *iter,
					   GtkFileChooserEntry *chooser_entry);
static gboolean completion_match_func     (GtkEntryCompletion  *comp,
					   const char          *key,
					   GtkTreeIter         *iter,
					   gpointer             data);
static void     files_added_cb            (GtkFileSystem       *file_system,
					   GSList              *added_uris,
					   GtkFileChooserEntry *chooser_entry);
static void     files_deleted_cb          (GtkFileSystem       *file_system,
					   GSList              *deleted_uris,
					   GtkFileChooserEntry *chooser_entry);
static char    *maybe_append_separator_to_path (GtkFileChooserEntry *chooser_entry,
						GtkFilePath         *path,
						gchar               *display_name);

static GObjectClass *parent_class;
static GtkEditableClass *parent_editable_iface;

GType
_gtk_file_chooser_entry_get_type (void)
{
  static GType file_chooser_entry_type = 0;

  if (!file_chooser_entry_type)
    {
      static const GTypeInfo file_chooser_entry_info =
      {
	sizeof (GtkFileChooserEntryClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_chooser_entry_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileChooserEntry),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_chooser_entry_init,
      };

      static const GInterfaceInfo editable_info =
      {
	(GInterfaceInitFunc) gtk_file_chooser_entry_iface_init, /* interface_init */
	NULL,			                              /* interface_finalize */
	NULL			                              /* interface_data */
      };


      file_chooser_entry_type = g_type_register_static (GTK_TYPE_ENTRY, "GtkFileChooserEntry",
							&file_chooser_entry_info, 0);
      g_type_add_interface_static (file_chooser_entry_type,
				   GTK_TYPE_EDITABLE,
				   &editable_info);
    }


  return file_chooser_entry_type;
}

static void
gtk_file_chooser_entry_class_init (GtkFileChooserEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkEntryClass *entry_class = GTK_ENTRY_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_chooser_entry_finalize;

  widget_class->focus = gtk_file_chooser_entry_focus;

  entry_class->activate = gtk_file_chooser_entry_activate;
}

static void
gtk_file_chooser_entry_iface_init (GtkEditableClass *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->do_insert_text = gtk_file_chooser_entry_do_insert_text;
  iface->changed = gtk_file_chooser_entry_changed;
}

static void
gtk_file_chooser_entry_init (GtkFileChooserEntry *chooser_entry)
{
  GtkEntryCompletion *comp;
  GtkCellRenderer *cell;

  comp = gtk_entry_completion_new ();

  gtk_entry_completion_set_match_func (comp,
				       completion_match_func,
				       chooser_entry,
				       NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (comp),
                              cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (comp),
                                 cell,
                                 "text", 0);

  g_signal_connect (comp, "match-selected",
		    G_CALLBACK (match_selected_callback), chooser_entry);

  gtk_entry_set_completion (GTK_ENTRY (chooser_entry), comp);
  g_object_unref (comp);

  g_signal_connect (chooser_entry, "notify::cursor-position",
		    G_CALLBACK (clear_completion_callback), NULL);
  g_signal_connect (chooser_entry, "notify::selection-bound",
		    G_CALLBACK (clear_completion_callback), NULL);
}

static void
gtk_file_chooser_entry_finalize (GObject *object)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);

  if (chooser_entry->completion_store)
    g_object_unref (chooser_entry->completion_store);

  if (chooser_entry->current_folder)
    {
      g_signal_handlers_disconnect_by_func (chooser_entry->current_folder,
					    G_CALLBACK (files_added_cb), chooser_entry);
      g_signal_handlers_disconnect_by_func (chooser_entry->current_folder,
					    G_CALLBACK (files_deleted_cb), chooser_entry);
      g_object_unref (chooser_entry->current_folder);
    }

  if (chooser_entry->file_system)
    g_object_unref (chooser_entry->file_system);

  gtk_file_path_free (chooser_entry->base_folder);
  gtk_file_path_free (chooser_entry->current_folder_path);
  g_free (chooser_entry->file_part);

  parent_class->finalize (object);
}

/* Match functions for the GtkEntryCompletion */
static gboolean
match_selected_callback (GtkEntryCompletion  *completion,
			 GtkTreeModel        *model,
			 GtkTreeIter         *iter,
			 GtkFileChooserEntry *chooser_entry)
{
  char *display_name;
  GtkFilePath *path;
  gint pos;
  
  gtk_tree_model_get (model, iter,
		      DISPLAY_NAME_COLUMN, &display_name,
		      PATH_COLUMN, &path,
		      -1);

  if (!display_name || !path)
    {
      /* these shouldn't complain if passed NULL */
      gtk_file_path_free (path);
      g_free (display_name);
      return FALSE;
    }

  display_name = maybe_append_separator_to_path (chooser_entry, path, display_name);

  pos = chooser_entry->file_part_pos;

  /* We don't set in_change here as we want to update the current_folder
   * variable */
  gtk_editable_delete_text (GTK_EDITABLE (chooser_entry),
			    pos, -1);
  gtk_editable_insert_text (GTK_EDITABLE (chooser_entry),
			    display_name, -1, 
			    &pos);
  gtk_editable_set_position (GTK_EDITABLE (chooser_entry), -1);

  gtk_file_path_free (path);
  g_free (display_name);

  return TRUE;
}

/* Match function for the GtkEntryCompletion */
static gboolean
completion_match_func (GtkEntryCompletion *comp,
		       const char         *key_unused,
		       GtkTreeIter        *iter,
		       gpointer            data)
{
  GtkFileChooserEntry *chooser_entry;
  char *name;
  gboolean result;
  char *norm_file_part;
  char *norm_name;

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (data);

  /* We ignore the key because it is the contents of the entry.  Instead, we
   * just use our precomputed file_part.
   */
  if (!chooser_entry->file_part)
    {
      return FALSE;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (chooser_entry->completion_store), iter, DISPLAY_NAME_COLUMN, &name, -1);
  if (!name)
    {
      return FALSE; /* Uninitialized row, ugh */
    }

  /* If we have an empty file_part, then we're at the root of a directory.  In
   * that case, we want to match all non-dot files.  We might want to match
   * dot_files too if show_hidden is TRUE on the fileselector in the future.
   */
  /* Additionally, support for gnome .hidden files would be sweet, too */
  if (chooser_entry->file_part[0] == '\000')
    {
      if (name[0] == '.')
	result = FALSE;
      else
	result = TRUE;
      g_free (name);

      return result;
    }


  norm_file_part = g_utf8_normalize (chooser_entry->file_part, -1, G_NORMALIZE_ALL);
  norm_name = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);

#ifdef G_PLATFORM_WIN32
  {
    gchar *temp;

    temp = norm_file_part;
    norm_file_part = g_utf8_casefold (norm_file_part, -1);
    g_free (temp);

    temp = norm_name;
    norm_name = g_utf8_casefold (norm_name, -1);
    g_free (temp);
  }
#endif

  result = (strncmp (norm_file_part, norm_name, strlen (norm_file_part)) == 0);

  g_free (norm_file_part);
  g_free (norm_name);
  g_free (name);
  
  return result;
}

/* This function will append a directory separator to paths to
 * display_name iff the path associated with it is a directory.
 * maybe_append_separator_to_path will g_free the display_name and
 * return a new one if needed.  Otherwise, it will return the old one.
 * You should be safe calling
 *
 * display_name = maybe_append_separator_to_path (entry, path, display_name);
 * ...
 * g_free (display_name);
 */
static char *
maybe_append_separator_to_path (GtkFileChooserEntry *chooser_entry,
				GtkFilePath         *path,
				gchar               *display_name)
{
  if (path)
    {
      GtkFileInfo *info;
	    
      info = gtk_file_folder_get_info (chooser_entry->current_folder,
				       path, NULL); /* NULL-GError */

      if (info)
	{
	  if (gtk_file_info_get_is_folder (info))
	    {
	      gchar *tmp = display_name;
	      display_name = g_strconcat (tmp, G_DIR_SEPARATOR_S, NULL);
	      g_free (tmp);
	    }
	  
	  gtk_file_info_free (info);
	}

    }

  return display_name;
}

static gboolean
check_completion_callback (GtkFileChooserEntry *chooser_entry)
{
  GtkTreeIter iter;
  gchar *common_prefix = NULL;
  GtkFilePath *unique_path = NULL;
  gboolean valid;

  GDK_THREADS_ENTER ();

  g_assert (chooser_entry->file_part);

  chooser_entry->check_completion_idle = NULL;

  if (strcmp (chooser_entry->file_part, "") == 0)
    goto done;

  if (chooser_entry->completion_store == NULL)
    goto done;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser_entry->completion_store),
					 &iter);

  while (valid)
    {
      gchar *display_name;
      GtkFilePath *path;

      gtk_tree_model_get (GTK_TREE_MODEL (chooser_entry->completion_store),
			  &iter,
			  DISPLAY_NAME_COLUMN, &display_name,
			  PATH_COLUMN, &path,
			  -1);

      if (g_str_has_prefix (display_name, chooser_entry->file_part))
	{
	  if (!common_prefix)
	    {
	      common_prefix = g_strdup (display_name);
	      unique_path = gtk_file_path_copy (path);
	    }
	  else
	    {
	      gchar *p = common_prefix;
	      const gchar *q = display_name;
		  
	      while (*p && *p == *q)
		{
		  p++;
		  q++;
		}
		  
	      *p = '\0';

	      gtk_file_path_free (unique_path);
	      unique_path = NULL;
	    }
	}

      g_free (display_name);
      gtk_file_path_free (path);
      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser_entry->completion_store),
					&iter);
    }

  if (unique_path)
    {
      common_prefix = maybe_append_separator_to_path (chooser_entry,
						      unique_path,
						      common_prefix);
      gtk_file_path_free (unique_path);
    }

  switch (chooser_entry->action)
    {
    case GTK_FILE_CHOOSER_ACTION_SAVE:
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      if (common_prefix && !g_str_has_suffix (common_prefix, "/"))
	{
	  g_free (common_prefix);
	  common_prefix = NULL;
	}
      break;
    default: ;
    }

  if (common_prefix)
    {
      gint file_part_len;
      gint common_prefix_len;
      gint pos;

      file_part_len = g_utf8_strlen (chooser_entry->file_part, -1);
      common_prefix_len = g_utf8_strlen (common_prefix, -1);

      if (common_prefix_len > file_part_len)
	{
	  pos = chooser_entry->file_part_pos;

	  chooser_entry->in_change = TRUE;
	  gtk_editable_delete_text (GTK_EDITABLE (chooser_entry),
				    pos, -1);
	  gtk_editable_insert_text (GTK_EDITABLE (chooser_entry),
				    common_prefix, -1, 
				    &pos);
	  gtk_editable_select_region (GTK_EDITABLE (chooser_entry),
				      chooser_entry->file_part_pos + file_part_len,
				      chooser_entry->file_part_pos + common_prefix_len);
	  chooser_entry->in_change = FALSE;

	  chooser_entry->has_completion = TRUE;
	}
	  
      g_free (common_prefix);
    }

 done:

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
add_completion_idle (GtkFileChooserEntry *chooser_entry)
{
  /* idle to update the selection based on the file list */
  if (chooser_entry->check_completion_idle == NULL)
    {
      chooser_entry->check_completion_idle = g_idle_source_new ();
      g_source_set_priority (chooser_entry->check_completion_idle, G_PRIORITY_HIGH);
      g_source_set_closure (chooser_entry->check_completion_idle,
			    g_cclosure_new_object (G_CALLBACK (check_completion_callback),
						   G_OBJECT (chooser_entry)));
      g_source_attach (chooser_entry->check_completion_idle, NULL);
    }
}


static void
update_current_folder_files (GtkFileChooserEntry *chooser_entry,
			     GSList              *added_uris)
{
  GSList *tmp_list;

  g_assert (chooser_entry->completion_store != NULL);

  /* Bah.  Need to turn off sorting */
  for (tmp_list = added_uris; tmp_list; tmp_list = tmp_list->next)
    {
      GtkFileInfo *info;
      GtkFilePath *path;

      path = tmp_list->data;

      info = gtk_file_folder_get_info (chooser_entry->current_folder,
				       path,
				       NULL); /* NULL-GError */
      if (info)
	{
	  const gchar *display_name = gtk_file_info_get_display_name (info);
	  GtkTreeIter iter;

	  gtk_list_store_append (chooser_entry->completion_store, &iter);
	  gtk_list_store_set (chooser_entry->completion_store, &iter,
			      DISPLAY_NAME_COLUMN, display_name,
			      PATH_COLUMN, path,
			      -1);

	  gtk_file_info_free (info);
	}
    }

  /* FIXME: we want to turn off sorting temporarily.  I suck... */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (chooser_entry->completion_store),
					DISPLAY_NAME_COLUMN, GTK_SORT_ASCENDING);

  add_completion_idle (chooser_entry);
}

static void
files_added_cb (GtkFileSystem       *file_system,
		GSList              *added_uris,
		GtkFileChooserEntry *chooser_entry)
{
  update_current_folder_files (chooser_entry, added_uris);
}

static void
files_deleted_cb (GtkFileSystem       *file_system,
		  GSList              *deleted_uris,
		  GtkFileChooserEntry *chooser_entry)
{
  /* FIXME: gravy... */
}

static gboolean
load_directory_callback (GtkFileChooserEntry *chooser_entry)
{
  GSList *child_paths = NULL;

  GDK_THREADS_ENTER ();

  chooser_entry->load_directory_idle = NULL;

  /* guard against bogus settings*/
  if (chooser_entry->current_folder_path == NULL ||
      chooser_entry->file_system == NULL)
    goto done;

  if (chooser_entry->current_folder != NULL)
    {
      g_warning ("idle activate multiple times without clearing the folder object first.");
      goto done;
    }
  g_assert (chooser_entry->completion_store == NULL);

  /* Load the folder */
  chooser_entry->current_folder = gtk_file_system_get_folder (chooser_entry->file_system,
							      chooser_entry->current_folder_path,
							      GTK_FILE_INFO_DISPLAY_NAME | GTK_FILE_INFO_IS_FOLDER,
							      NULL); /* NULL-GError */

  /* There is no folder by that name */
  if (!chooser_entry->current_folder)
    goto done;
  g_signal_connect (chooser_entry->current_folder, "files-added",
		    G_CALLBACK (files_added_cb), chooser_entry);
  g_signal_connect (chooser_entry->current_folder, "files-removed",
		    G_CALLBACK (files_deleted_cb), chooser_entry);
  
  chooser_entry->completion_store = gtk_list_store_new (N_COLUMNS,
							G_TYPE_STRING,
							GTK_TYPE_FILE_PATH);

  if (chooser_entry->file_part_pos != -1)
    {
      gtk_file_folder_list_children (chooser_entry->current_folder,
				     &child_paths,
				     NULL); /* NULL-GError */
      if (child_paths)
	{
	  update_current_folder_files (chooser_entry, child_paths);
	  add_completion_idle (chooser_entry);
	  gtk_file_paths_free (child_paths);
	}
    }

  gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)),
				  GTK_TREE_MODEL (chooser_entry->completion_store));

 done:
  
  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_file_chooser_entry_do_insert_text (GtkEditable *editable,
				       const gchar *new_text,
				       gint         new_text_length,
				       gint        *position)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);

  parent_editable_iface->do_insert_text (editable, new_text, new_text_length, position);

  if (! chooser_entry->in_change)
    add_completion_idle (GTK_FILE_CHOOSER_ENTRY (editable));
}

static gboolean
gtk_file_chooser_entry_focus (GtkWidget        *widget,
			      GtkDirectionType  direction)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (widget);
  GdkModifierType state;
  gboolean control_pressed = FALSE;

  if (!chooser_entry->eat_tabs)
    return GTK_WIDGET_CLASS (parent_class)->focus (widget, direction);

  if (gtk_get_current_event_state (&state))
    {
      if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
	control_pressed = TRUE;
    }

  /* This is a bit evil -- it makes Tab never leave the entry. It basically
   * makes it 'safe' for people to hit. */
  if ((direction == GTK_DIR_TAB_FORWARD) &&
      (GTK_WIDGET_HAS_FOCUS (widget)) &&
      (! control_pressed))
    {
      gint pos = 0;

      if (chooser_entry->has_completion)
	gtk_editable_set_position (GTK_EDITABLE (widget),
				   GTK_ENTRY (widget)->text_length);
      /* Trigger the completion window to pop up again by a 
       * zero-length insertion, a bit of a hack.
       */
      gtk_editable_insert_text (GTK_EDITABLE (widget), "", -1, &pos);

      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (parent_class)->focus (widget, direction);
}

static void
gtk_file_chooser_entry_activate (GtkEntry *entry)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (entry);

  if (chooser_entry->has_completion)
    {
      gtk_editable_set_position (GTK_EDITABLE (entry),
				 entry->text_length);
    }
  
  GTK_ENTRY_CLASS (parent_class)->activate (entry);
}

/* This will see if a path typed by the user is new, and installs the loading
 * idle if it is.
 */
static void
gtk_file_chooser_entry_maybe_update_directory (GtkFileChooserEntry *chooser_entry,
					       GtkFilePath         *folder_path,
					       gboolean             force_reload)
{
  gboolean queue_idle = FALSE;

  if (chooser_entry->current_folder_path)
    {
      if (gtk_file_path_compare (folder_path, chooser_entry->current_folder_path) != 0 || force_reload)
	{
	  /* We changed our current directory.  We need to clear out the old
	   * directory information.
	   */
	  if (chooser_entry->current_folder)
	    {
	      g_signal_handlers_disconnect_by_func (chooser_entry->current_folder,
						    G_CALLBACK (files_added_cb), chooser_entry);
	      g_signal_handlers_disconnect_by_func (chooser_entry->current_folder,
						    G_CALLBACK (files_deleted_cb), chooser_entry);

	      g_object_unref (chooser_entry->current_folder);
	      chooser_entry->current_folder = NULL;
	    }
	  if (chooser_entry->completion_store)
	    {
	      gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)), NULL);
	      g_object_unref (chooser_entry->completion_store);
	      chooser_entry->completion_store = NULL;
	    }

	  queue_idle = TRUE;
	}

      gtk_file_path_free (chooser_entry->current_folder_path);
    }
  else
    {
      queue_idle = TRUE;
    }

  chooser_entry->current_folder_path = folder_path;

  if (queue_idle && chooser_entry->load_directory_idle == NULL)
    {
      chooser_entry->load_directory_idle = g_idle_source_new ();
      g_source_set_priority (chooser_entry->load_directory_idle, G_PRIORITY_HIGH);
      g_source_set_closure (chooser_entry->load_directory_idle,
			    g_cclosure_new_object (G_CALLBACK (load_directory_callback),
						   G_OBJECT (chooser_entry)));
      g_source_attach (chooser_entry->load_directory_idle, NULL);
    }
}



static void
gtk_file_chooser_entry_changed (GtkEditable *editable)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);
  const gchar *text;
  GtkFilePath *folder_path;
  gchar *file_part;
  gsize total_len, file_part_len;
  gint file_part_pos;

  if (chooser_entry->in_change)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (editable));
  
  if (!chooser_entry->file_system ||
      !chooser_entry->base_folder ||
      !gtk_file_system_parse (chooser_entry->file_system,
			      chooser_entry->base_folder, text,
			      &folder_path, &file_part, NULL)) /* NULL-GError */
    {
      folder_path = gtk_file_path_copy (chooser_entry->base_folder);
      file_part = g_strdup ("");
      file_part_pos = -1;
    }
  else
    {
      file_part_len = strlen (file_part);
      total_len = strlen (text);
      if (total_len > file_part_len)
	file_part_pos = g_utf8_strlen (text, total_len - file_part_len);
      else
	file_part_pos = 0;
    }

  if (chooser_entry->file_part)
    g_free (chooser_entry->file_part);

  chooser_entry->file_part = file_part;
  chooser_entry->file_part_pos = file_part_pos;

  gtk_file_chooser_entry_maybe_update_directory (chooser_entry, folder_path, file_part_pos == -1);
}

static void
clear_completion_callback (GtkFileChooserEntry *chooser_entry,
			   GParamSpec          *pspec)
{
  if (chooser_entry->has_completion)
    {
      chooser_entry->has_completion = FALSE;
      gtk_file_chooser_entry_changed (GTK_EDITABLE (chooser_entry));
    }
}

/**
 * _gtk_file_chooser_entry_new:
 * @eat_tabs: If %FALSE, allow focus navigation with the tab key.
 *
 * Creates a new #GtkFileChooserEntry object. #GtkFileChooserEntry
 * is an internal implementation widget for the GTK+ file chooser
 * which is an entry with completion with respect to a
 * #GtkFileSystem object.
 *
 * Return value: the newly created #GtkFileChooserEntry
 **/
GtkWidget *
_gtk_file_chooser_entry_new (gboolean eat_tabs)
{
  GtkFileChooserEntry *chooser_entry;

  chooser_entry = g_object_new (GTK_TYPE_FILE_CHOOSER_ENTRY, NULL);
  chooser_entry->eat_tabs = (eat_tabs != FALSE);

  return GTK_WIDGET (chooser_entry);
}

/**
 * _gtk_file_chooser_entry_set_file_system:
 * @chooser_entry: a #GtkFileChooser
 * @file_system: an object implementing #GtkFileSystem
 *
 * Sets the file system for @chooser_entry.
 **/
void
_gtk_file_chooser_entry_set_file_system (GtkFileChooserEntry *chooser_entry,
					 GtkFileSystem       *file_system)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_ENTRY (chooser_entry));
  g_return_if_fail (GTK_IS_FILE_SYSTEM (file_system));

  if (file_system != chooser_entry->file_system)
    {
      if (chooser_entry->file_system)
	g_object_unref (chooser_entry->file_system);

      chooser_entry->file_system = g_object_ref (file_system);
    }
}

/**
 * _gtk_file_chooser_entry_set_base_folder:
 * @chooser_entry: a #GtkFileChooserEntry
 * @path: path of a folder in the chooser entries current file system.
 *
 * Sets the folder with respect to which completions occur.
 **/
void
_gtk_file_chooser_entry_set_base_folder (GtkFileChooserEntry *chooser_entry,
					 const GtkFilePath   *path)
{
  if (chooser_entry->base_folder)
    gtk_file_path_free (chooser_entry->base_folder);

  chooser_entry->base_folder = gtk_file_path_copy (path);

  gtk_file_chooser_entry_changed (GTK_EDITABLE (chooser_entry));
  gtk_editable_select_region (GTK_EDITABLE (chooser_entry), 0, -1);
}

/**
 * _gtk_file_chooser_entry_get_current_folder:
 * @chooser_entry: a #GtkFileChooserEntry
 *
 * Gets the current folder for the #GtkFileChooserEntry. If the
 * user has only entered a filename, this will be the base folder
 * (see _gtk_file_chooser_entry_set_base_folder()), but if the
 * user has entered a relative or absolute path, then it will
 * be different. If the user has entered a relative or absolute
 * path that doesn't point to a folder in the file system, it will
 * be %NULL.
 *
 * Return value: the path of current folder - this value is owned by the
 *  chooser entry and must not be modified or freed.
 **/
const GtkFilePath *
_gtk_file_chooser_entry_get_current_folder (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->has_completion)
    {
      gtk_editable_set_position (GTK_EDITABLE (chooser_entry),
				 GTK_ENTRY (chooser_entry)->text_length);
    }
  return chooser_entry->current_folder_path;
}

/**
 * _gtk_file_chooser_entry_get_file_part:
 * @chooser_entry: a #GtkFileChooserEntry
 *
 * Gets the non-folder portion of whatever the user has entered
 * into the file selector. What is returned is a UTF-8 string,
 * and if a filename path is needed, gtk_file_system_make_path()
 * must be used
  *
 * Return value: the entered filename - this value is owned by the
 *  chooser entry and must not be modified or freed.
 **/
const gchar *
_gtk_file_chooser_entry_get_file_part (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->has_completion)
    {
      gtk_editable_set_position (GTK_EDITABLE (chooser_entry),
				 GTK_ENTRY (chooser_entry)->text_length);
    }
  return chooser_entry->file_part;
}

/**
 * _gtk_file_chooser_entry_set_file_part:
 * @chooser_entry: a #GtkFileChooserEntry
 * @file_part: text to display in the entry, in UTF-8
 *
 * Sets the current text shown in the file chooser entry.
 **/
void
_gtk_file_chooser_entry_set_file_part (GtkFileChooserEntry *chooser_entry,
				       const gchar         *file_part)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_ENTRY (chooser_entry));

  gtk_entry_set_text (GTK_ENTRY (chooser_entry), file_part);
}


/**
 * _gtk_file_chooser_entry_set_action:
 * @chooser_entry: a #GtkFileChooserEntry
 * @action: the action which is performed by the file selector using this entry
 *
 * Sets action which is performed by the file selector using this entry. 
 * The #GtkFileChooserEntry will use different completion strategies for 
 * different actions.
 **/
void
_gtk_file_chooser_entry_set_action (GtkFileChooserEntry *chooser_entry,
				    GtkFileChooserAction action)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_ENTRY (chooser_entry));
  
  if (  chooser_entry->action != action)
    {
      chooser_entry->action = action;
    }
}


/**
 * _gtk_file_chooser_entry_get_action:
 * @chooser_entry: a #GtkFileChooserEntry
 *
 * Gets the action for this entry. 
 *
 * Returns: the action
 **/
GtkFileChooserAction
_gtk_file_chooser_entry_get_action (GtkFileChooserEntry *chooser_entry)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_ENTRY (chooser_entry),
			GTK_FILE_CHOOSER_ACTION_OPEN);
  
  return chooser_entry->action;
}
