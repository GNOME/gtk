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

#include <string.h>

#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkfilechooserentry.h"

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

  GtkFileSystem *file_system;
  GtkFilePath *base_folder;
  GtkFilePath *current_folder_path;
  gchar *file_part;
  GSource *completion_idle;

  GtkFileFolder *current_folder;

  GtkListStore *completion_store;

  guint has_completion : 1;
};

static void gtk_file_chooser_entry_class_init (GtkFileChooserEntryClass *class);
static void gtk_file_chooser_entry_iface_init (GtkEditableClass         *iface);
static void gtk_file_chooser_entry_init       (GtkFileChooserEntry      *chooser_entry);

static void     gtk_file_chooser_entry_finalize       (GObject          *object);
static gboolean gtk_file_chooser_entry_focus          (GtkWidget        *widget,
						       GtkDirectionType  direction);
static void     gtk_file_chooser_entry_changed        (GtkEditable      *editable);
static void     gtk_file_chooser_entry_do_insert_text (GtkEditable      *editable,
						       const gchar      *new_text,
						       gint              new_text_length,
						       gint             *position);

static void clear_completion_callback (GtkFileChooserEntry *chooser_entry,
				       GParamSpec          *pspec);

static gboolean completion_match_func (GtkEntryCompletion *comp,
				       const char         *key,
				       GtkTreeIter        *iter,
				       gpointer            data);

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

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_chooser_entry_finalize;

  widget_class->focus = gtk_file_chooser_entry_focus;
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
  GtkCellRenderer *renderer;

  chooser_entry->completion_store = gtk_list_store_new (1, G_TYPE_STRING);

  comp = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (comp, GTK_TREE_MODEL (chooser_entry->completion_store));
  gtk_entry_completion_set_match_func (comp,
				       completion_match_func,
				       chooser_entry,
				       NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (comp), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (comp), renderer, "text", 0);

  gtk_entry_set_completion (GTK_ENTRY (chooser_entry), comp);

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
    g_object_unref (chooser_entry->current_folder);
  
  if (chooser_entry->file_system)
    g_object_unref (chooser_entry->file_system);
  
  gtk_file_path_free (chooser_entry->base_folder);
  gtk_file_path_free (chooser_entry->current_folder_path);
  g_free (chooser_entry->file_part);

  parent_class->finalize (object);
}

/* Match function for the GtkEntryCompletion */
static gboolean
completion_match_func (GtkEntryCompletion *comp,
		       const char         *key,
		       GtkTreeIter        *iter,
		       gpointer            data)
{
  GtkFileChooserEntry *chooser_entry;
  char *name;
  gboolean result;

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (data);

  gtk_tree_model_get (GTK_TREE_MODEL (chooser_entry->completion_store), iter, 0, &name, -1);
  if (!name)
    return FALSE; /* Uninitialized row, ugh */

  /* We ignore the key because it is the contents of the entry.  Instead, we
   * just use our precomputed file_part.
   */

  if (chooser_entry->file_part)
    {
      char *norm_file_part;
      char *norm_name;

      norm_file_part = g_utf8_normalize (chooser_entry->file_part, -1, G_NORMALIZE_ALL);
      norm_name = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);

      result = (strncmp (norm_file_part, norm_name, strlen (norm_file_part)) == 0);

      /* FIXME: Owen suggests first doing a case-folded comparison, then a
       * non-case-folded one if the first one yielded more than one match.
       */

      g_free (norm_file_part);
      g_free (norm_name);
    }
  else
    result = FALSE;
  

  result = (strncmp (key, name, strlen (key)) == 0);

  g_free (name);
  return result;
}

static gboolean
completion_idle_callback (GtkFileChooserEntry *chooser_entry)
{
  GtkEditable *editable = GTK_EDITABLE (chooser_entry);
  GSList *child_paths = NULL;
  GSList *tmp_list;
  gchar *common_prefix = NULL;
  GtkFilePath *unique_path = NULL;
  GtkListStore *new_store;

  chooser_entry->completion_idle = NULL;

  if (strcmp (chooser_entry->file_part, "") == 0)
    return FALSE;

  if (!chooser_entry->current_folder &&
      chooser_entry->file_system &&
      chooser_entry->current_folder_path)
    chooser_entry->current_folder = gtk_file_system_get_folder (chooser_entry->file_system,
								chooser_entry->current_folder_path,
								GTK_FILE_INFO_DISPLAY_NAME | GTK_FILE_INFO_IS_FOLDER,
								NULL); /* NULL-GError */
  if (chooser_entry->current_folder)
    gtk_file_folder_list_children (chooser_entry->current_folder,
				   &child_paths,
				   NULL); /* NULL-GError */

  new_store = gtk_list_store_new (1, G_TYPE_STRING);

  for (tmp_list = child_paths; tmp_list; tmp_list = tmp_list->next)
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

	  gtk_list_store_append (new_store, &iter);
	  gtk_list_store_set (new_store, &iter,
			      0, display_name,
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
	  
	  gtk_file_info_free (info);
	}
    }

  g_object_unref (chooser_entry->completion_store);
  chooser_entry->completion_store = new_store;
  gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)),
				  GTK_TREE_MODEL (new_store));

  if (unique_path)
    {
      GtkFileInfo *info;
	    
      info = gtk_file_folder_get_info (chooser_entry->current_folder,
				       unique_path,
				       NULL); /* NULL-GError */

      if (info)
	{
	  if (gtk_file_info_get_is_folder (info))
	    {
	      gchar *tmp = common_prefix;
	      common_prefix = g_strconcat (tmp, "/", NULL);
	      g_free (tmp);
	    }
	  
	  gtk_file_info_free (info);
	}

      gtk_file_path_free (unique_path);
    }
  
  gtk_file_paths_free (child_paths);

  if (common_prefix)
    {
      gint total_len;
      gint file_part_len;
      gint common_prefix_len;
      gint pos;

      total_len = g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (chooser_entry)), -1);
      file_part_len = g_utf8_strlen (chooser_entry->file_part, -1);
      common_prefix_len = g_utf8_strlen (common_prefix, -1);

      if (common_prefix_len > file_part_len)
	{
	  pos = total_len - file_part_len;
	  gtk_editable_delete_text (editable,
				    pos, -1);
	  gtk_editable_insert_text (editable,
				    common_prefix, -1, 
				    &pos);
	  gtk_editable_select_region (editable,
				      total_len,
				      total_len - file_part_len + common_prefix_len);

	  chooser_entry->has_completion = TRUE;
	}
	  
      g_free (common_prefix);
    }
  
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

  if (*position == GTK_ENTRY (editable)->text_length &&
      !chooser_entry->completion_idle)
    {
      chooser_entry->completion_idle = g_idle_source_new ();
      g_source_set_priority (chooser_entry->completion_idle, G_PRIORITY_HIGH);
      g_source_set_closure (chooser_entry->completion_idle,
			    g_cclosure_new_object (G_CALLBACK (completion_idle_callback),
						   G_OBJECT (chooser_entry)));
      g_source_attach (chooser_entry->completion_idle, NULL);
    }
}

static gboolean
gtk_file_chooser_entry_focus (GtkWidget        *widget,
			      GtkDirectionType  direction)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (widget);
  
  if (direction == GTK_DIR_TAB_FORWARD &&
      GTK_WIDGET_HAS_FOCUS (widget) &&
      chooser_entry->has_completion)
    {
      gtk_editable_set_position (GTK_EDITABLE (widget),
				 GTK_ENTRY (widget)->text_length);
      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (parent_class)->focus (widget, direction);
}

static void
gtk_file_chooser_entry_changed (GtkEditable *editable)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);
  const gchar *text;
  GtkFilePath *folder_path;
  gchar *file_part;

  text = gtk_entry_get_text (GTK_ENTRY (editable));

  if (!chooser_entry->file_system ||
      !chooser_entry->base_folder ||
      !gtk_file_system_parse (chooser_entry->file_system,
			      chooser_entry->base_folder, text,
			      &folder_path, &file_part, NULL)) /* NULL-GError */
    {
      folder_path = gtk_file_path_copy (chooser_entry->base_folder);
      file_part = g_strdup ("");
    }

  if (chooser_entry->current_folder_path)
    {
      if (gtk_file_path_compare (folder_path, chooser_entry->current_folder_path) != 0)
	{
	  if (chooser_entry->current_folder)
	    {
	      g_object_unref (chooser_entry->current_folder);
	      chooser_entry->current_folder = NULL;
	    }
	}
      
      gtk_file_path_free (chooser_entry->current_folder_path);
    }
  
  chooser_entry->current_folder_path = folder_path;

  if (chooser_entry->file_part)
    g_free (chooser_entry->file_part);

  chooser_entry->file_part = file_part;
}

static void
clear_completion_callback (GtkFileChooserEntry *chooser_entry,
			   GParamSpec          *pspec)
{
  chooser_entry->has_completion = FALSE;
}

/**
 * _gtk_file_chooser_entry_new:
 * 
 * Creates a new #GtkFileChooserEntry object. #GtkFileChooserEntry
 * is an internal implementation widget for the GTK+ file chooser
 * which is an entry with completion with respect to a
 * #GtkFileSystem object.
 * 
 * Return value: the newly created #GtkFileChooserEntry
 **/
GtkWidget *
_gtk_file_chooser_entry_new (void)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_ENTRY, NULL);
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
