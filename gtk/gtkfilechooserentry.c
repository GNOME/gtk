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

#include "config.h"

#include "gtkfilechooserentry.h"

#include <string.h>

#include "gtkalignment.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkfilesystemmodel.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkwindow.h"
#include "gtkintl.h"
#include "gtkalias.h"

#include "gdkkeysyms.h"

typedef struct _GtkFileChooserEntryClass GtkFileChooserEntryClass;

#define GTK_FILE_CHOOSER_ENTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_ENTRY, GtkFileChooserEntryClass))
#define GTK_IS_FILE_CHOOSER_ENTRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_ENTRY))
#define GTK_FILE_CHOOSER_ENTRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_ENTRY, GtkFileChooserEntryClass))

struct _GtkFileChooserEntryClass
{
  GtkEntryClass parent_class;
};

/* Action to take when the current folder finishes loading (for explicit or automatic completion) */
typedef enum {
  LOAD_COMPLETE_NOTHING,
  LOAD_COMPLETE_EXPLICIT_COMPLETION
} LoadCompleteAction;

struct _GtkFileChooserEntry
{
  GtkEntry parent_instance;

  GtkFileChooserAction action;

  GFile *base_folder;
  GFile *current_folder_file;
  gchar *dir_part;
  gchar *file_part;

  LoadCompleteAction load_complete_action;

  GtkTreeModel *completion_store;

  guint current_folder_loaded : 1;
  guint eat_tabs       : 1;
  guint local_only     : 1;
};

enum
{
  DISPLAY_NAME_COLUMN,
  FULL_PATH_COLUMN,
  N_COLUMNS
};

static void     gtk_file_chooser_entry_finalize       (GObject          *object);
static void     gtk_file_chooser_entry_dispose        (GObject          *object);
static void     gtk_file_chooser_entry_grab_focus     (GtkWidget        *widget);
static gboolean gtk_file_chooser_entry_key_press_event (GtkWidget *widget,
							GdkEventKey *event);
static gboolean gtk_file_chooser_entry_focus_out_event (GtkWidget       *widget,
							GdkEventFocus   *event);

#ifdef G_OS_WIN32
static gint     insert_text_callback      (GtkFileChooserEntry *widget,
					   const gchar         *new_text,
					   gint                 new_text_length,
					   gint                *position,
					   gpointer             user_data);
static void     delete_text_callback      (GtkFileChooserEntry *widget,
					   gint                 start_pos,
					   gint                 end_pos,
					   gpointer             user_data);
#endif

static gboolean match_selected_callback   (GtkEntryCompletion  *completion,
					   GtkTreeModel        *model,
					   GtkTreeIter         *iter,
					   GtkFileChooserEntry *chooser_entry);
static gboolean completion_match_func     (GtkEntryCompletion  *comp,
					   const char          *key,
					   GtkTreeIter         *iter,
					   gpointer             data);

static void refresh_current_folder_and_file_part (GtkFileChooserEntry *chooser_entry);
static void set_completion_folder (GtkFileChooserEntry *chooser_entry,
                                   GFile               *folder);
static void finished_loading_cb (GtkFileSystemModel  *model,
                                 GError              *error,
		                 GtkFileChooserEntry *chooser_entry);

G_DEFINE_TYPE (GtkFileChooserEntry, _gtk_file_chooser_entry, GTK_TYPE_ENTRY)

static char *
gtk_file_chooser_entry_get_completion_text (GtkFileChooserEntry *chooser_entry)
{
  GtkEditable *editable = GTK_EDITABLE (chooser_entry);
  int start, end;

  gtk_editable_get_selection_bounds (editable, &start, &end);
  return gtk_editable_get_chars (editable, 0, MIN (start, end));
}

static void
gtk_file_chooser_entry_dispatch_properties_changed (GObject     *object,
                                                    guint        n_pspecs,
                                                    GParamSpec **pspecs)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);
  guint i;

  G_OBJECT_CLASS (_gtk_file_chooser_entry_parent_class)->dispatch_properties_changed (object, n_pspecs, pspecs);

  /* What we are after: The text in front of the cursor was modified.
   * Unfortunately, there's no other way to catch this. */

  for (i = 0; i < n_pspecs; i++)
    {
      if (pspecs[i]->name == I_("cursor-position") ||
          pspecs[i]->name == I_("selection-bound") ||
          pspecs[i]->name == I_("text"))
        {
          chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;

          refresh_current_folder_and_file_part (chooser_entry);

          break;
        }
    }
}

static void
_gtk_file_chooser_entry_class_init (GtkFileChooserEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->finalize = gtk_file_chooser_entry_finalize;
  gobject_class->dispose = gtk_file_chooser_entry_dispose;
  gobject_class->dispatch_properties_changed = gtk_file_chooser_entry_dispatch_properties_changed;

  widget_class->grab_focus = gtk_file_chooser_entry_grab_focus;
  widget_class->key_press_event = gtk_file_chooser_entry_key_press_event;
  widget_class->focus_out_event = gtk_file_chooser_entry_focus_out_event;
}

static void
_gtk_file_chooser_entry_init (GtkFileChooserEntry *chooser_entry)
{
  GtkEntryCompletion *comp;
  GtkCellRenderer *cell;

  chooser_entry->local_only = TRUE;
  chooser_entry->base_folder = g_file_new_for_path (g_get_home_dir ());

  g_object_set (chooser_entry, "truncate-multiline", TRUE, NULL);

  comp = gtk_entry_completion_new ();
  gtk_entry_completion_set_popup_single_match (comp, FALSE);
  gtk_entry_completion_set_minimum_key_length (comp, 0);
  /* see docs for gtk_entry_completion_set_text_column() */
  g_object_set (comp, "text-column", FULL_PATH_COLUMN, NULL);

  gtk_entry_completion_set_match_func (comp,
				       completion_match_func,
				       chooser_entry,
				       NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (comp),
                              cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (comp),
                                 cell,
                                 "text", DISPLAY_NAME_COLUMN);

  g_signal_connect (comp, "match-selected",
		    G_CALLBACK (match_selected_callback), chooser_entry);

  gtk_entry_set_completion (GTK_ENTRY (chooser_entry), comp);
  g_object_unref (comp);

#ifdef G_OS_WIN32
  g_signal_connect (chooser_entry, "insert-text",
		    G_CALLBACK (insert_text_callback), NULL);
  g_signal_connect (chooser_entry, "delete-text",
		    G_CALLBACK (delete_text_callback), NULL);
#endif
}

static void
gtk_file_chooser_entry_finalize (GObject *object)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);

  g_object_unref (chooser_entry->base_folder);

  if (chooser_entry->current_folder_file)
    g_object_unref (chooser_entry->current_folder_file);

  g_free (chooser_entry->dir_part);
  g_free (chooser_entry->file_part);

  G_OBJECT_CLASS (_gtk_file_chooser_entry_parent_class)->finalize (object);
}

static void
gtk_file_chooser_entry_dispose (GObject *object)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);

  set_completion_folder (chooser_entry, NULL);

  G_OBJECT_CLASS (_gtk_file_chooser_entry_parent_class)->dispose (object);
}

/* Match functions for the GtkEntryCompletion */
static gboolean
match_selected_callback (GtkEntryCompletion  *completion,
			 GtkTreeModel        *model,
			 GtkTreeIter         *iter,
			 GtkFileChooserEntry *chooser_entry)
{
  char *path;
  
  gtk_tree_model_get (model, iter,
		      FULL_PATH_COLUMN, &path,
                      -1);

  gtk_editable_delete_text (GTK_EDITABLE (chooser_entry),
			    0,
                            gtk_editable_get_position (GTK_EDITABLE (chooser_entry)));
  gtk_editable_insert_text (GTK_EDITABLE (chooser_entry),
			    path,
                            0,
                            NULL); 

  g_free (path);

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

  gtk_tree_model_get (chooser_entry->completion_store, iter, DISPLAY_NAME_COLUMN, &name, -1);
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

static void
clear_completions (GtkFileChooserEntry *chooser_entry)
{
  chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;
}

static void
beep (GtkFileChooserEntry *chooser_entry)
{
  gtk_widget_error_bell (GTK_WIDGET (chooser_entry));
}

static gboolean
is_valid_scheme_character (char c)
{
  return g_ascii_isalnum (c) || c == '+' || c == '-' || c == '.';
}

static gboolean
has_uri_scheme (const char *str)
{
  const char *p;

  p = str;

  if (!is_valid_scheme_character (*p))
    return FALSE;

  do
    p++;
  while (is_valid_scheme_character (*p));

  return (strncmp (p, "://", 3) == 0);
}

static GFile *
gtk_file_chooser_get_file_for_text (GtkFileChooserEntry *chooser_entry,
                                    const gchar         *str)
{
  GFile *file;

  if (str[0] == '~' || g_path_is_absolute (str) || has_uri_scheme (str))
    file = g_file_parse_name (str);
  else
    file = g_file_resolve_relative_path (chooser_entry->base_folder, str);

  return file;
}

static GFile *
gtk_file_chooser_get_directory_for_text (GtkFileChooserEntry *chooser_entry,
                                         const char *         text)
{
  GFile *file, *parent;

  file = gtk_file_chooser_get_file_for_text (chooser_entry, text);

  if (text[0] == 0 || text[strlen (text) - 1] == G_DIR_SEPARATOR)
    return file;

  parent = g_file_get_parent (file);
  g_object_unref (file);

  return parent;
}

/* Finds a common prefix based on the contents of the entry
 * and mandatorily appends it
 */
static void
explicitly_complete (GtkFileChooserEntry *chooser_entry)
{
  clear_completions (chooser_entry);

  if (chooser_entry->completion_store)
    {
      char *completion, *text;
      gsize completion_len, text_len;
      
      text = gtk_file_chooser_entry_get_completion_text (chooser_entry);
      text_len = strlen (text);
      completion = gtk_entry_completion_compute_prefix (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)), text);
      completion_len = completion ? strlen (completion) : 0;

      if (completion_len > text_len)
        {
          GtkEditable *editable = GTK_EDITABLE (chooser_entry);
          int pos = gtk_editable_get_position (editable);

          gtk_editable_insert_text (editable,
                                    completion + text_len,
                                    completion_len - text_len,
                                    &pos);
          gtk_editable_set_position (editable, pos);
          return;
        }
    }

  beep (chooser_entry);
}

static void
gtk_file_chooser_entry_grab_focus (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->grab_focus (widget);
  _gtk_file_chooser_entry_select_filename (GTK_FILE_CHOOSER_ENTRY (widget));
}

static void
start_explicit_completion (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->current_folder_loaded)
    explicitly_complete (chooser_entry);
  else
    chooser_entry->load_complete_action = LOAD_COMPLETE_EXPLICIT_COMPLETION;
}

static gboolean
gtk_file_chooser_entry_key_press_event (GtkWidget *widget,
					GdkEventKey *event)
{
  GtkFileChooserEntry *chooser_entry;
  GtkEditable *editable;
  GdkModifierType state;
  gboolean control_pressed;

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  if (!chooser_entry->eat_tabs)
    return GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->key_press_event (widget, event);

  control_pressed = FALSE;

  if (gtk_get_current_event_state (&state))
    {
      if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
	control_pressed = TRUE;
    }

  /* This is a bit evil -- it makes Tab never leave the entry. It basically
   * makes it 'safe' for people to hit. */
  if (event->keyval == GDK_Tab && !control_pressed)
    {
      gint start, end;

      gtk_editable_get_selection_bounds (editable, &start, &end);
      
      if (start != end)
        gtk_editable_set_position (editable, MAX (start, end));
      else
	start_explicit_completion (chooser_entry);

      return TRUE;
     }

  return GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->key_press_event (widget, event);

}

static gboolean
gtk_file_chooser_entry_focus_out_event (GtkWidget     *widget,
					GdkEventFocus *event)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (widget);

  chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;
 
  return GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->focus_out_event (widget, event);
}

static void
discard_completion_store (GtkFileChooserEntry *chooser_entry)
{
  if (!chooser_entry->completion_store)
    return;

  gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)), NULL);
  gtk_entry_completion_set_inline_completion (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)), FALSE);
  g_object_unref (chooser_entry->completion_store);
  chooser_entry->completion_store = NULL;
}

static gboolean
completion_store_set (GtkFileSystemModel  *model,
                      GFile               *file,
                      GFileInfo           *info,
                      int                  column,
                      GValue              *value,
                      gpointer             data)
{
  GtkFileChooserEntry *chooser_entry = data;

  const char *prefix = "";
  const char *suffix = "";

  switch (column)
    {
    case FULL_PATH_COLUMN:
      prefix = chooser_entry->dir_part;
      /* fall through */
    case DISPLAY_NAME_COLUMN:
      if (_gtk_file_info_consider_as_directory (info))
        suffix = G_DIR_SEPARATOR_S;

      g_value_take_string (value, g_strconcat (
              prefix,
              g_file_info_get_display_name (info),
              suffix,
              NULL));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

/* Fills the completion store from the contents of the current folder */
static void
populate_completion_store (GtkFileChooserEntry *chooser_entry)
{
  chooser_entry->completion_store = GTK_TREE_MODEL (
      _gtk_file_system_model_new_for_directory (chooser_entry->current_folder_file,
                                                "standard::name,standard::display-name,standard::type",
                                                completion_store_set,
                                                chooser_entry,
                                                N_COLUMNS,
                                                G_TYPE_STRING,
                                                G_TYPE_STRING));
  g_signal_connect (chooser_entry->completion_store, "finished-loading",
		    G_CALLBACK (finished_loading_cb), chooser_entry);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (chooser_entry->completion_store),
					DISPLAY_NAME_COLUMN, GTK_SORT_ASCENDING);

  gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)),
				  chooser_entry->completion_store);
}

/* When we finish loading the current folder, this function should get called to
 * perform the deferred explicit completion.
 */
static void
perform_load_complete_action (GtkFileChooserEntry *chooser_entry)
{
  switch (chooser_entry->load_complete_action)
    {
    case LOAD_COMPLETE_NOTHING:
      break;

    case LOAD_COMPLETE_EXPLICIT_COMPLETION:
      explicitly_complete (chooser_entry);
      break;

    default:
      g_assert_not_reached ();
    }

}

/* Callback when the current folder finishes loading */
static void
finished_loading_cb (GtkFileSystemModel  *model,
                     GError              *error,
		     GtkFileChooserEntry *chooser_entry)
{
  GtkEntryCompletion *completion;

  chooser_entry->current_folder_loaded = TRUE;

  if (error)
    {
      LoadCompleteAction old_load_complete_action;

      old_load_complete_action = chooser_entry->load_complete_action;

      discard_completion_store (chooser_entry);
      clear_completions (chooser_entry);

      if (old_load_complete_action == LOAD_COMPLETE_EXPLICIT_COMPLETION)
	{
	  /* Since this came from explicit user action (Tab completion), we'll present errors visually */

	  beep (chooser_entry);
	}

      return;
    }

  perform_load_complete_action (chooser_entry);

  gtk_widget_set_tooltip_text (GTK_WIDGET (chooser_entry), NULL);

  completion = gtk_entry_get_completion (GTK_ENTRY (chooser_entry));
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_completion_complete (completion);
  gtk_entry_completion_insert_prefix (completion);
}

static void
set_completion_folder (GtkFileChooserEntry *chooser_entry,
                       GFile               *folder_file)
{
  if (folder_file &&
      chooser_entry->local_only
      && !g_file_is_native (folder_file))
    folder_file = NULL;

  if ((chooser_entry->current_folder_file
       && folder_file
       && g_file_equal (folder_file, chooser_entry->current_folder_file))
      || chooser_entry->current_folder_file == folder_file)
    return;

  if (chooser_entry->current_folder_file)
    {
      g_object_unref (chooser_entry->current_folder_file);
      chooser_entry->current_folder_file = NULL;
    }
  
  chooser_entry->current_folder_loaded = FALSE;

  discard_completion_store (chooser_entry);

  if (folder_file)
    {
      chooser_entry->current_folder_file = g_object_ref (folder_file);
      populate_completion_store (chooser_entry);
    }
}

static void
refresh_current_folder_and_file_part (GtkFileChooserEntry *chooser_entry)
{
  GFile *folder_file;
  char *text, *last_slash;

  g_free (chooser_entry->file_part);
  g_free (chooser_entry->dir_part);

  text = gtk_file_chooser_entry_get_completion_text (chooser_entry);

  last_slash = strrchr (text, G_DIR_SEPARATOR);
  if (last_slash)
    {
      chooser_entry->dir_part = g_strndup (text, last_slash - text + 1);
      chooser_entry->file_part = g_strdup (last_slash + 1);
    }
  else
    {
      chooser_entry->dir_part = g_strdup ("");
      chooser_entry->file_part = g_strdup (text);
    }

  folder_file = gtk_file_chooser_get_directory_for_text (chooser_entry, text);
  set_completion_folder (chooser_entry, folder_file);
  if (folder_file)
    g_object_unref (folder_file);

  g_free (text);
}

#ifdef G_OS_WIN32
static gint
insert_text_callback (GtkFileChooserEntry *chooser_entry,
		      const gchar	  *new_text,
		      gint       	   new_text_length,
		      gint       	  *position,
		      gpointer   	   user_data)
{
  const gchar *colon = memchr (new_text, ':', new_text_length);
  gint i;

  /* Disallow these characters altogether */
  for (i = 0; i < new_text_length; i++)
    {
      if (new_text[i] == '<' ||
	  new_text[i] == '>' ||
	  new_text[i] == '"' ||
	  new_text[i] == '|' ||
	  new_text[i] == '*' ||
	  new_text[i] == '?')
	break;
    }

  if (i < new_text_length ||
      /* Disallow entering text that would cause a colon to be anywhere except
       * after a drive letter.
       */
      (colon != NULL &&
       *position + (colon - new_text) != 1) ||
      (new_text_length > 0 &&
       *position <= 1 &&
       gtk_entry_get_text_length (GTK_ENTRY (chooser_entry)) >= 2 &&
       gtk_entry_get_text (GTK_ENTRY (chooser_entry))[1] == ':'))
    {
      gtk_widget_error_bell (GTK_WIDGET (chooser_entry));
      g_signal_stop_emission_by_name (chooser_entry, "insert_text");
      return FALSE;
    }

  return TRUE;
}

static void
delete_text_callback (GtkFileChooserEntry *chooser_entry,
		      gint                 start_pos,
		      gint                 end_pos,
		      gpointer             user_data)
{
  /* If deleting a drive letter, delete the colon, too */
  if (start_pos == 0 && end_pos == 1 &&
      gtk_entry_get_text_length (GTK_ENTRY (chooser_entry)) >= 2 &&
      gtk_entry_get_text (GTK_ENTRY (chooser_entry))[1] == ':')
    {
      g_signal_handlers_block_by_func (chooser_entry,
				       G_CALLBACK (delete_text_callback),
				       user_data);
      gtk_editable_delete_text (GTK_EDITABLE (chooser_entry), 0, 1);
      g_signal_handlers_unblock_by_func (chooser_entry,
					 G_CALLBACK (delete_text_callback),
					 user_data);
    }
}
#endif

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
_gtk_file_chooser_entry_new (gboolean       eat_tabs)
{
  GtkFileChooserEntry *chooser_entry;

  chooser_entry = g_object_new (GTK_TYPE_FILE_CHOOSER_ENTRY, NULL);
  chooser_entry->eat_tabs = (eat_tabs != FALSE);

  return GTK_WIDGET (chooser_entry);
}

/**
 * _gtk_file_chooser_entry_set_base_folder:
 * @chooser_entry: a #GtkFileChooserEntry
 * @file: file for a folder in the chooser entries current file system.
 *
 * Sets the folder with respect to which completions occur.
 **/
void
_gtk_file_chooser_entry_set_base_folder (GtkFileChooserEntry *chooser_entry,
					 GFile               *file)
{
  if (file)
    g_object_ref (file);
  else
    file = g_file_new_for_path (g_get_home_dir ());

  if (g_file_equal (chooser_entry->base_folder, file))
    {
      g_object_unref (file);
      return;
    }

  if (chooser_entry->base_folder)
    g_object_unref (chooser_entry->base_folder);

  chooser_entry->base_folder = file;

  clear_completions (chooser_entry);
}

/**
 * _gtk_file_chooser_entry_get_current_folder:
 * @chooser_entry: a #GtkFileChooserEntry
 *
 * Gets the current folder for the #GtkFileChooserEntry. If the
 * user has only entered a filename, this will be in the base folder
 * (see _gtk_file_chooser_entry_set_base_folder()), but if the
 * user has entered a relative or absolute path, then it will
 * be different.  If the user has entered unparsable text, or text which
 * the entry cannot handle, this will return %NULL.
 *
 * Return value: the file for the current folder - you must g_object_unref()
 *   the value after use.
 **/
GFile *
_gtk_file_chooser_entry_get_current_folder (GtkFileChooserEntry *chooser_entry)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_ENTRY (chooser_entry), NULL);

  return gtk_file_chooser_get_directory_for_text (chooser_entry,
                                                  gtk_entry_get_text (GTK_ENTRY (chooser_entry)));
}

/**
 * _gtk_file_chooser_entry_get_file_part:
 * @chooser_entry: a #GtkFileChooserEntry
 *
 * Gets the non-folder portion of whatever the user has entered
 * into the file selector. What is returned is a UTF-8 string,
 * and if a filename path is needed, g_file_get_child_for_display_name()
 * must be used
  *
 * Return value: the entered filename - this value is owned by the
 *  chooser entry and must not be modified or freed.
 **/
const gchar *
_gtk_file_chooser_entry_get_file_part (GtkFileChooserEntry *chooser_entry)
{
  const char *last_slash, *text;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_ENTRY (chooser_entry), NULL);

  text = gtk_entry_get_text (GTK_ENTRY (chooser_entry));
  last_slash = strrchr (text, G_DIR_SEPARATOR);
  if (last_slash)
    return last_slash + 1;
  else
    return text;
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
  
  if (chooser_entry->action != action)
    {
      GtkEntryCompletion *comp;

      chooser_entry->action = action;

      comp = gtk_entry_get_completion (GTK_ENTRY (chooser_entry));

      /* FIXME: do we need to actually set the following? */

      switch (action)
	{
	case GTK_FILE_CHOOSER_ACTION_OPEN:
	case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  gtk_entry_completion_set_popup_single_match (comp, FALSE);
	  break;
	case GTK_FILE_CHOOSER_ACTION_SAVE:
	case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
	  gtk_entry_completion_set_popup_single_match (comp, TRUE);
	  break;
	}
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

gboolean
_gtk_file_chooser_entry_get_is_folder (GtkFileChooserEntry *chooser_entry,
				       GFile               *file)
{
  GtkTreeIter iter;
  GFileInfo *info;

  if (chooser_entry->completion_store == NULL ||
      !_gtk_file_system_model_get_iter_for_file (GTK_FILE_SYSTEM_MODEL (chooser_entry->completion_store),
                                                 &iter,
                                                 file))
    return FALSE;

  info = _gtk_file_system_model_get_info (GTK_FILE_SYSTEM_MODEL (chooser_entry->completion_store),
                                          &iter);

  return _gtk_file_info_consider_as_directory (info);
}


/*
 * _gtk_file_chooser_entry_select_filename:
 * @chooser_entry: a #GtkFileChooserEntry
 *
 * Selects the filename (without the extension) for user edition.
 */
void
_gtk_file_chooser_entry_select_filename (GtkFileChooserEntry *chooser_entry)
{
  const gchar *str, *ext;
  glong len = -1;

  if (chooser_entry->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      str = gtk_entry_get_text (GTK_ENTRY (chooser_entry));
      ext = g_strrstr (str, ".");

      if (ext)
       len = g_utf8_pointer_to_offset (str, ext);
    }

  gtk_editable_select_region (GTK_EDITABLE (chooser_entry), 0, (gint) len);
}

void
_gtk_file_chooser_entry_set_local_only (GtkFileChooserEntry *chooser_entry,
                                        gboolean             local_only)
{
  chooser_entry->local_only = local_only;
  clear_completions (chooser_entry);
}

gboolean
_gtk_file_chooser_entry_get_local_only (GtkFileChooserEntry *chooser_entry)
{
  return chooser_entry->local_only;
}
