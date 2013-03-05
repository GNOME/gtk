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
#include <string.h>

#include "gtkalignment.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkfilechooserentry.h"
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
  LOAD_COMPLETE_AUTOCOMPLETE,
  LOAD_COMPLETE_EXPLICIT_COMPLETION
} LoadCompleteAction;

typedef enum
{
  REFRESH_OK,
  REFRESH_INVALID_INPUT,
  REFRESH_INCOMPLETE_HOSTNAME,
  REFRESH_NONEXISTENT,
  REFRESH_NOT_LOCAL
} RefreshStatus;

struct _GtkFileChooserEntry
{
  GtkEntry parent_instance;

  GtkFileChooserAction action;

  GtkFileSystem *file_system;
  GFile *base_folder;
  GFile *current_folder_file;
  gchar *file_part;
  gint file_part_pos;

  /* Folder being loaded or already loaded */
  GtkFolder *current_folder;
  GCancellable *load_folder_cancellable;

  LoadCompleteAction load_complete_action;

  GtkListStore *completion_store;

  guint start_autocompletion_idle_id;

  GtkWidget *completion_feedback_window;
  GtkWidget *completion_feedback_label;
  guint completion_feedback_timeout_id;

  guint has_completion : 1;
  guint in_change      : 1;
  guint eat_tabs       : 1;
  guint local_only     : 1;
};

enum
{
  DISPLAY_NAME_COLUMN,
  FILE_COLUMN,
  N_COLUMNS
};

#define COMPLETION_FEEDBACK_TIMEOUT_MS 2000

static void     gtk_file_chooser_entry_iface_init     (GtkEditableClass *iface);

static void     gtk_file_chooser_entry_finalize       (GObject          *object);
static void     gtk_file_chooser_entry_dispose        (GObject          *object);
static void     gtk_file_chooser_entry_grab_focus     (GtkWidget        *widget);
static void     gtk_file_chooser_entry_unmap          (GtkWidget        *widget);
static gboolean gtk_file_chooser_entry_key_press_event (GtkWidget *widget,
							GdkEventKey *event);
static gboolean gtk_file_chooser_entry_focus_out_event (GtkWidget       *widget,
							GdkEventFocus   *event);
static void     gtk_file_chooser_entry_activate       (GtkEntry         *entry);
static void     gtk_file_chooser_entry_do_insert_text (GtkEditable *editable,
						       const gchar *new_text,
						       gint         new_text_length,
						       gint        *position);
static void     gtk_file_chooser_entry_do_delete_text (GtkEditable *editable,
						       gint         start_pos,
						       gint         end_pos);
static void     gtk_file_chooser_entry_set_position (GtkEditable *editable,
						     gint         position);
static void     gtk_file_chooser_entry_set_selection_bounds (GtkEditable *editable,
							     gint         start_pos,
							     gint         end_pos);

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
static char    *maybe_append_separator_to_file (GtkFileChooserEntry *chooser_entry,
						GFile               *file,
						gchar               *display_name,
						gboolean            *appended);

typedef enum {
  REFRESH_UP_TO_CURSOR_POSITION,
  REFRESH_WHOLE_TEXT
} RefreshMode;

static RefreshStatus refresh_current_folder_and_file_part (GtkFileChooserEntry *chooser_entry,
						  RefreshMode refresh_mode);
static void finished_loading_cb (GtkFolder *folder,
				 gpointer   data);
static void autocomplete (GtkFileChooserEntry *chooser_entry);
static void install_start_autocompletion_idle (GtkFileChooserEntry *chooser_entry);
static void remove_completion_feedback (GtkFileChooserEntry *chooser_entry);
static void pop_up_completion_feedback (GtkFileChooserEntry *chooser_entry,
					const gchar         *feedback);

static GtkEditableClass *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserEntry, _gtk_file_chooser_entry, GTK_TYPE_ENTRY,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
						gtk_file_chooser_entry_iface_init))

static void
_gtk_file_chooser_entry_class_init (GtkFileChooserEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkEntryClass *entry_class = GTK_ENTRY_CLASS (class);

  gobject_class->finalize = gtk_file_chooser_entry_finalize;
  gobject_class->dispose = gtk_file_chooser_entry_dispose;

  widget_class->grab_focus = gtk_file_chooser_entry_grab_focus;
  widget_class->unmap = gtk_file_chooser_entry_unmap;
  widget_class->key_press_event = gtk_file_chooser_entry_key_press_event;
  widget_class->focus_out_event = gtk_file_chooser_entry_focus_out_event;

  entry_class->activate = gtk_file_chooser_entry_activate;
}

static void
gtk_file_chooser_entry_iface_init (GtkEditableClass *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->do_insert_text = gtk_file_chooser_entry_do_insert_text;
  iface->do_delete_text = gtk_file_chooser_entry_do_delete_text;
  iface->set_position = gtk_file_chooser_entry_set_position;
  iface->set_selection_bounds = gtk_file_chooser_entry_set_selection_bounds;
}

static void
_gtk_file_chooser_entry_init (GtkFileChooserEntry *chooser_entry)
{
  GtkEntryCompletion *comp;
  GtkCellRenderer *cell;

  chooser_entry->local_only = TRUE;

  g_object_set (chooser_entry, "truncate-multiline", TRUE, NULL);

  comp = gtk_entry_completion_new ();
  gtk_entry_completion_set_popup_single_match (comp, FALSE);

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

  if (chooser_entry->base_folder)
    g_object_unref (chooser_entry->base_folder);

  if (chooser_entry->current_folder)
    g_object_unref (chooser_entry->current_folder);

  if (chooser_entry->current_folder_file)
    g_object_unref (chooser_entry->current_folder_file);

  g_free (chooser_entry->file_part);

  G_OBJECT_CLASS (_gtk_file_chooser_entry_parent_class)->finalize (object);
}

static void
discard_current_folder (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->current_folder)
    {
      g_signal_handlers_disconnect_by_func (chooser_entry->current_folder,
					    G_CALLBACK (finished_loading_cb), chooser_entry);
      g_object_unref (chooser_entry->current_folder);
      chooser_entry->current_folder = NULL;
    }
}

static void
discard_loading_and_current_folder_file (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->load_folder_cancellable)
    {
      g_cancellable_cancel (chooser_entry->load_folder_cancellable);
      chooser_entry->load_folder_cancellable = NULL;
    }

  if (chooser_entry->current_folder_file)
    {
      g_object_unref (chooser_entry->current_folder_file);
      chooser_entry->current_folder_file = NULL;
    }
}

static void
discard_completion_store (GtkFileChooserEntry *chooser_entry)
{
  if (!chooser_entry->completion_store)
    return;

  gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)), NULL);
  g_object_unref (chooser_entry->completion_store);
  chooser_entry->completion_store = NULL;
}

static void
gtk_file_chooser_entry_dispose (GObject *object)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);

  remove_completion_feedback (chooser_entry);
  discard_current_folder (chooser_entry);
  discard_loading_and_current_folder_file (chooser_entry);

  if (chooser_entry->start_autocompletion_idle_id != 0)
    {
      g_source_remove (chooser_entry->start_autocompletion_idle_id);
      chooser_entry->start_autocompletion_idle_id = 0;
    }

  discard_completion_store (chooser_entry);

  if (chooser_entry->file_system)
    {
      g_object_unref (chooser_entry->file_system);
      chooser_entry->file_system = NULL;
    }

  G_OBJECT_CLASS (_gtk_file_chooser_entry_parent_class)->dispose (object);
}

/* Match functions for the GtkEntryCompletion */
static gboolean
match_selected_callback (GtkEntryCompletion  *completion,
			 GtkTreeModel        *model,
			 GtkTreeIter         *iter,
			 GtkFileChooserEntry *chooser_entry)
{
  char *display_name;
  GFile *file;
  gint pos;
  gboolean dummy;
  
  gtk_tree_model_get (model, iter,
		      DISPLAY_NAME_COLUMN, &display_name,
		      FILE_COLUMN, &file,
		      -1);

  if (!display_name || !file)
    {
      /* these shouldn't complain if passed NULL */
      g_object_unref (file);
      g_free (display_name);
      return FALSE;
    }

  display_name = maybe_append_separator_to_file (chooser_entry, file, display_name, &dummy);

  pos = chooser_entry->file_part_pos;

  /* We don't set in_change here as we want to update the current_folder
   * variable */
  gtk_editable_delete_text (GTK_EDITABLE (chooser_entry),
			    pos, -1);
  gtk_editable_insert_text (GTK_EDITABLE (chooser_entry),
			    display_name, -1, 
			    &pos);
  gtk_editable_set_position (GTK_EDITABLE (chooser_entry), -1);

  g_object_unref (file);
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

static void
clear_completions (GtkFileChooserEntry *chooser_entry)
{
  chooser_entry->has_completion = FALSE;
  chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;

  remove_completion_feedback (chooser_entry);
}

static void
beep (GtkFileChooserEntry *chooser_entry)
{
  gtk_widget_error_bell (GTK_WIDGET (chooser_entry));
}

/* This function will append a directory separator to paths to
 * display_name iff the path associated with it is a directory.
 * maybe_append_separator_to_file will g_free the display_name and
 * return a new one if needed.  Otherwise, it will return the old one.
 * You should be safe calling
 *
 * display_name = maybe_append_separator_to_file (entry, file, display_name, &appended);
 * ...
 * g_free (display_name);
 */
static char *
maybe_append_separator_to_file (GtkFileChooserEntry *chooser_entry,
				GFile               *file,
				gchar               *display_name,
				gboolean            *appended)
{
  *appended = FALSE;

  if (!g_str_has_suffix (display_name, G_DIR_SEPARATOR_S) && file)
    {
      GFileInfo *info;

      info = _gtk_folder_get_info (chooser_entry->current_folder, file);

      if (info)
	{
	  if (_gtk_file_info_consider_as_directory (info))
	    {
	      gchar *tmp = display_name;
	      display_name = g_strconcat (tmp, G_DIR_SEPARATOR_S, NULL);
	      *appended = TRUE;
	      g_free (tmp);
	    }

	  g_object_unref (info);
	}
    }

  return display_name;
}

static char *
trim_dir_separator_suffix (const char *str)
{
  int len;

  len = strlen (str);
  if (len > 0 && G_IS_DIR_SEPARATOR (str[len - 1]))
    return g_strndup (str, len - 1);
  else
    return g_strdup (str);
}

/* Determines if the completion model has entries with a common prefix relative
 * to the current contents of the entry.  Also, if there's one and only one such
 * path, stores it in unique_path_ret.
 */
static gboolean
find_common_prefix (GtkFileChooserEntry *chooser_entry,
		    gchar               **common_prefix_ret,
		    GFile               **unique_file_ret,
		    gboolean             *is_complete_not_unique_ret,
		    gboolean             *prefix_expands_the_file_part_ret,
		    GError              **error)
{
  GtkEditable *editable;
  GtkTreeIter iter;
  gboolean parsed;
  gboolean valid;
  char *text_up_to_cursor;
  GFile *parsed_folder_file;
  char *parsed_file_part;

  *common_prefix_ret = NULL;
  *unique_file_ret = NULL;
  *is_complete_not_unique_ret = FALSE;
  *prefix_expands_the_file_part_ret = FALSE;

  editable = GTK_EDITABLE (chooser_entry);

  text_up_to_cursor = gtk_editable_get_chars (editable, 0, gtk_editable_get_position (editable));

  parsed = _gtk_file_system_parse (chooser_entry->file_system,
				   chooser_entry->base_folder,
				   text_up_to_cursor,
				   &parsed_folder_file,
				   &parsed_file_part,
				   error);

  g_free (text_up_to_cursor);

  if (!parsed)
    return FALSE;

  g_assert (parsed_folder_file != NULL
	    && chooser_entry->current_folder != NULL
	    && g_file_equal (parsed_folder_file, chooser_entry->current_folder_file));

  g_object_unref (parsed_folder_file);

  /* First pass: find the common prefix */

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser_entry->completion_store), &iter);

  while (valid)
    {
      gchar *display_name;
      GFile *file;

      gtk_tree_model_get (GTK_TREE_MODEL (chooser_entry->completion_store),
			  &iter,
			  DISPLAY_NAME_COLUMN, &display_name,
			  FILE_COLUMN, &file,
			  -1);

      if (g_str_has_prefix (display_name, parsed_file_part))
	{
	  if (!*common_prefix_ret)
	    {
	      *common_prefix_ret = trim_dir_separator_suffix (display_name);
	      *unique_file_ret = g_object_ref (file);
	    }
	  else
	    {
	      gchar *p = *common_prefix_ret;
	      const gchar *q = display_name;

	      while (*p && *p == *q)
		{
		  p++;
		  q++;
		}

	      *p = '\0';

	      if (*unique_file_ret)
		{
		  g_object_unref (*unique_file_ret);
		  *unique_file_ret = NULL;
		}
	    }
	}

      g_free (display_name);
      g_object_unref (file);
      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser_entry->completion_store), &iter);
    }

  /* Second pass: see if the prefix we found is a complete match */

  if (*common_prefix_ret != NULL)
    {
      valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser_entry->completion_store), &iter);

      while (valid)
	{
	  gchar *display_name;
	  int len;

	  gtk_tree_model_get (GTK_TREE_MODEL (chooser_entry->completion_store),
			      &iter,
			      DISPLAY_NAME_COLUMN, &display_name,
			      -1);
	  len = strlen (display_name);
	  g_assert (len > 0);

	  if (G_IS_DIR_SEPARATOR (display_name[len - 1]))
	    len--;

	  if (*unique_file_ret == NULL && strncmp (*common_prefix_ret, display_name, len) == 0)
	    *is_complete_not_unique_ret = TRUE;

	  g_free (display_name);
	  valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser_entry->completion_store), &iter);
	}

      /* Finally:  Did we generate a new completion, or was the user's input already completed as far as it could go? */

      *prefix_expands_the_file_part_ret = g_utf8_strlen (*common_prefix_ret, -1) > g_utf8_strlen (parsed_file_part, -1);
    }

  g_free (parsed_file_part);

  return TRUE;
}

static gboolean
char_after_cursor_is_directory_separator (GtkFileChooserEntry *chooser_entry)
{
  int cursor_pos;
  gboolean result;

  result = FALSE;

  cursor_pos = gtk_editable_get_position (GTK_EDITABLE (chooser_entry));
  if (cursor_pos < gtk_entry_get_text_length (GTK_ENTRY (chooser_entry)))
    {
      char *next_char_str;

      next_char_str = gtk_editable_get_chars (GTK_EDITABLE (chooser_entry), cursor_pos, cursor_pos + 1);
      if (G_IS_DIR_SEPARATOR (*next_char_str))
	result = TRUE;

      g_free (next_char_str);
    }

  return result;
}

typedef enum {
  INVALID_INPUT,		/* what the user typed is bogus */
  NO_MATCH,			/* no matches based on what the user typed */
  NOTHING_INSERTED_COMPLETE,	/* what the user typed is already completed as far as it will go */
  NOTHING_INSERTED_UNIQUE,	/* what the user typed is already completed, and is a unique match */
  COMPLETED,			/* completion inserted (ambiguous suffix) */
  COMPLETED_UNIQUE,		/* completion inserted, and it is a complete name and a unique match */
  COMPLETE_BUT_NOT_UNIQUE	/* completion inserted, it is a complete name but not unique */
} CommonPrefixResult;

/* Finds a common prefix based on the contents of the entry
 * and mandatorily appends it
 */
static CommonPrefixResult
append_common_prefix (GtkFileChooserEntry *chooser_entry,
                      gboolean             highlight,
                      gboolean             show_errors)
{
  gchar *common_prefix;
  GFile *unique_file;
  gboolean is_complete_not_unique;
  gboolean prefix_expands_the_file_part;
  GError *error;
  CommonPrefixResult result = NO_MATCH;
  gboolean have_result;

  clear_completions (chooser_entry);

  if (chooser_entry->completion_store == NULL)
    return NO_MATCH;

  error = NULL;
  if (!find_common_prefix (chooser_entry, &common_prefix, &unique_file, &is_complete_not_unique, &prefix_expands_the_file_part, &error))
    {
      /* If the user types an incomplete hostname ("http://foo" without
       * a slash after that), it's not an error.  We just don't want to
       * pop up a meaningless completion window in that state.
       */
      if (!g_error_matches (error, GTK_FILE_CHOOSER_ERROR, GTK_FILE_CHOOSER_ERROR_INCOMPLETE_HOSTNAME)
          && show_errors)
        {
          beep (chooser_entry);
          pop_up_completion_feedback (chooser_entry, _("Invalid path"));
        }

      g_error_free (error);

      return INVALID_INPUT;
    }

  have_result = FALSE;

  if (unique_file)
    {
      if (!char_after_cursor_is_directory_separator (chooser_entry))
        {
          gboolean appended;

          common_prefix = maybe_append_separator_to_file (chooser_entry,
                                                          unique_file,
                                                          common_prefix,
                                                          &appended);
          if (appended)
            prefix_expands_the_file_part = TRUE;
        }

      g_object_unref (unique_file);

      if (prefix_expands_the_file_part)
        result = COMPLETED_UNIQUE;
      else
        result = NOTHING_INSERTED_UNIQUE;

      have_result = TRUE;
    }
  else
    {
      if (is_complete_not_unique)
        {
          result = COMPLETE_BUT_NOT_UNIQUE;
          have_result = TRUE;
        }
    }

  if (common_prefix)
    {
      gint cursor_pos;
      gint pos;

      cursor_pos = gtk_editable_get_position (GTK_EDITABLE (chooser_entry));

      pos = chooser_entry->file_part_pos;

      if (prefix_expands_the_file_part)
        {
          chooser_entry->in_change = TRUE;
          gtk_editable_delete_text (GTK_EDITABLE (chooser_entry),
                                    pos, cursor_pos);
          gtk_editable_insert_text (GTK_EDITABLE (chooser_entry),
                                    common_prefix, -1,
                                    &pos);
          chooser_entry->in_change = FALSE;

          if (highlight)
            {
              /* equivalent to cursor_pos + common_prefix_len); */
              gtk_editable_select_region (GTK_EDITABLE (chooser_entry),
                                          cursor_pos,
                                          pos);
              chooser_entry->has_completion = TRUE;
            }
          else
            gtk_editable_set_position (GTK_EDITABLE (chooser_entry), pos);
        }
      else if (!have_result)
        {
          result = NOTHING_INSERTED_COMPLETE;
          have_result = TRUE;
        }

      g_free (common_prefix);

      if (have_result)
        return result;
      else
        return COMPLETED;
    }
  else
    {
      if (have_result)
        return result;
      else
        return NO_MATCH;
    }
}

static void
gtk_file_chooser_entry_do_insert_text (GtkEditable *editable,
				       const gchar *new_text,
				       gint         new_text_length,
				       gint        *position)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);
  gint old_text_len;
  gint insert_pos;

  old_text_len = gtk_entry_get_text_length (GTK_ENTRY (chooser_entry));
  insert_pos = *position;

  parent_editable_iface->do_insert_text (editable, new_text, new_text_length, position);

  if (chooser_entry->in_change)
    return;

  remove_completion_feedback (chooser_entry);

  if ((chooser_entry->action == GTK_FILE_CHOOSER_ACTION_OPEN
       || chooser_entry->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
      && insert_pos == old_text_len)
    install_start_autocompletion_idle (chooser_entry);
}

static void
clear_completions_if_not_in_change (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->in_change)
    return;

  clear_completions (chooser_entry);
}

static void
gtk_file_chooser_entry_do_delete_text (GtkEditable *editable,
				       gint         start_pos,
				       gint         end_pos)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);

  parent_editable_iface->do_delete_text (editable, start_pos, end_pos);

  clear_completions_if_not_in_change (chooser_entry);
}

static void
gtk_file_chooser_entry_set_position (GtkEditable *editable,
				     gint         position)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);

  parent_editable_iface->set_position (editable, position);

  clear_completions_if_not_in_change (chooser_entry);
}

static void
gtk_file_chooser_entry_set_selection_bounds (GtkEditable *editable,
					     gint         start_pos,
					     gint         end_pos)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (editable);

  parent_editable_iface->set_selection_bounds (editable, start_pos, end_pos);

  clear_completions_if_not_in_change (chooser_entry);
}

static void
gtk_file_chooser_entry_grab_focus (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->grab_focus (widget);
  _gtk_file_chooser_entry_select_filename (GTK_FILE_CHOOSER_ENTRY (widget));
}

static void
gtk_file_chooser_entry_unmap (GtkWidget *widget)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (widget);

  remove_completion_feedback (chooser_entry);

  GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->unmap (widget);
}

static gboolean
completion_feedback_window_expose_event_cb (GtkWidget      *widget,
					    GdkEventExpose *event,
					    gpointer        data)
{
  /* Stolen from gtk_tooltip_paint_window() */

  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (data);

  gtk_paint_flat_box (chooser_entry->completion_feedback_window->style,
		      chooser_entry->completion_feedback_window->window,
		      GTK_STATE_NORMAL,
		      GTK_SHADOW_OUT,
		      NULL,
		      chooser_entry->completion_feedback_window,
		      "tooltip",
		      0, 0,
		      chooser_entry->completion_feedback_window->allocation.width,
		      chooser_entry->completion_feedback_window->allocation.height);

  return FALSE;
}

static void
set_invisible_mouse_cursor (GdkWindow *window)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gdk_window_get_display (window);
  cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);

  gdk_window_set_cursor (window, cursor);

  gdk_cursor_unref (cursor);
}

static void
completion_feedback_window_realize_cb (GtkWidget *widget,
				       gpointer data)
{
  /* We hide the mouse cursor inside the completion feedback window, since
   * GtkEntry hides the cursor when the user types.  We don't want the cursor to
   * come back if the completion feedback ends up where the mouse is.
   */
  set_invisible_mouse_cursor (gtk_widget_get_window (widget));
}

static void
create_completion_feedback_window (GtkFileChooserEntry *chooser_entry)
{
  /* Stolen from gtk_tooltip_init() */

  GtkWidget *alignment;

  chooser_entry->completion_feedback_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_type_hint (GTK_WINDOW (chooser_entry->completion_feedback_window),
			    GDK_WINDOW_TYPE_HINT_TOOLTIP);
  gtk_widget_set_app_paintable (chooser_entry->completion_feedback_window, TRUE);
  gtk_window_set_resizable (GTK_WINDOW (chooser_entry->completion_feedback_window), FALSE);
  gtk_widget_set_name (chooser_entry->completion_feedback_window, "gtk-tooltip");

  alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
			     chooser_entry->completion_feedback_window->style->ythickness,
			     chooser_entry->completion_feedback_window->style->ythickness,
			     chooser_entry->completion_feedback_window->style->xthickness,
			     chooser_entry->completion_feedback_window->style->xthickness);
  gtk_container_add (GTK_CONTAINER (chooser_entry->completion_feedback_window), alignment);
  gtk_widget_show (alignment);

  g_signal_connect (chooser_entry->completion_feedback_window, "expose-event",
		    G_CALLBACK (completion_feedback_window_expose_event_cb), chooser_entry);
  g_signal_connect (chooser_entry->completion_feedback_window, "realize",
		    G_CALLBACK (completion_feedback_window_realize_cb), chooser_entry);
  /* FIXME: connect to motion-notify-event, and *show* the cursor when the mouse moves */

  chooser_entry->completion_feedback_label = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (alignment), chooser_entry->completion_feedback_label);
  gtk_widget_show (chooser_entry->completion_feedback_label);
}

static gboolean
completion_feedback_timeout_cb (gpointer data)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (data);

  chooser_entry->completion_feedback_timeout_id = 0;

  remove_completion_feedback (chooser_entry);
  return FALSE;
}

static void
install_completion_feedback_timer (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->completion_feedback_timeout_id != 0)
    g_source_remove (chooser_entry->completion_feedback_timeout_id);

  chooser_entry->completion_feedback_timeout_id = gdk_threads_add_timeout (COMPLETION_FEEDBACK_TIMEOUT_MS,
									   completion_feedback_timeout_cb,
									   chooser_entry);
}

/* Gets the x position of the text cursor in the entry, in widget coordinates */
static void
get_entry_cursor_x (GtkFileChooserEntry *chooser_entry,
		    gint                *x_ret)
{
  /* FIXME: see the docs for gtk_entry_get_layout_offsets().  As an exercise for
   * the reader, you have to implement support for the entry's scroll offset and
   * RTL layouts and all the fancy Pango stuff.
   */

  PangoLayout *layout;
  gint layout_x, layout_y;
  gint layout_index;
  PangoRectangle strong_pos;
  gint start_pos, end_pos;

  layout = gtk_entry_get_layout (GTK_ENTRY (chooser_entry));

  gtk_entry_get_layout_offsets (GTK_ENTRY (chooser_entry), &layout_x, &layout_y);

  gtk_editable_get_selection_bounds (GTK_EDITABLE (chooser_entry), &start_pos, &end_pos);
  layout_index = gtk_entry_text_index_to_layout_index (GTK_ENTRY (chooser_entry),
                                                       end_pos);


  pango_layout_get_cursor_pos (layout, layout_index, &strong_pos, NULL);

  *x_ret = layout_x + strong_pos.x / PANGO_SCALE;
}

static void
show_completion_feedback_window (GtkFileChooserEntry *chooser_entry)
{
  /* More or less stolen from gtk_tooltip_position() */

  GtkRequisition feedback_req;
  gint entry_x, entry_y;
  gint cursor_x;
  GtkAllocation *entry_allocation;
  int feedback_x, feedback_y;

  gtk_widget_size_request (chooser_entry->completion_feedback_window, &feedback_req);

  gdk_window_get_origin (GTK_WIDGET (chooser_entry)->window, &entry_x, &entry_y);
  entry_allocation = &(GTK_WIDGET (chooser_entry)->allocation);

  get_entry_cursor_x (chooser_entry, &cursor_x);

  /* FIXME: fit to the screen if we bump on the screen's edge */
  /* cheap "half M-width", use height as approximation of character em-size */
  feedback_x = entry_x + cursor_x + entry_allocation->height / 2;
  feedback_y = entry_y + (entry_allocation->height - feedback_req.height) / 2;

  gtk_window_move (GTK_WINDOW (chooser_entry->completion_feedback_window), feedback_x, feedback_y);
  gtk_widget_show (chooser_entry->completion_feedback_window);

  install_completion_feedback_timer (chooser_entry);
}

static void
pop_up_completion_feedback (GtkFileChooserEntry *chooser_entry,
			    const gchar         *feedback)
{
  if (chooser_entry->completion_feedback_window == NULL)
    create_completion_feedback_window (chooser_entry);

  gtk_label_set_text (GTK_LABEL (chooser_entry->completion_feedback_label), feedback);

  show_completion_feedback_window (chooser_entry);
}

static void
remove_completion_feedback (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->completion_feedback_window)
    gtk_widget_destroy (chooser_entry->completion_feedback_window);

  chooser_entry->completion_feedback_window = NULL;
  chooser_entry->completion_feedback_label = NULL;

  if (chooser_entry->completion_feedback_timeout_id != 0)
    {
      g_source_remove (chooser_entry->completion_feedback_timeout_id);
      chooser_entry->completion_feedback_timeout_id = 0;
    }
}

static void
explicitly_complete (GtkFileChooserEntry *chooser_entry)
{
  CommonPrefixResult result;

  g_assert (chooser_entry->current_folder != NULL);
  g_assert (_gtk_folder_is_finished_loading (chooser_entry->current_folder));

  /* FIXME: see what Emacs does in case there is no common prefix, or there is more than one match:
   *
   * - If there is a common prefix, insert it (done)
   * - If there is no common prefix, pop up the suggestion window
   * - If there are no matches at all, beep and bring up a tooltip (done)
   * - If the suggestion window is already up, scroll it
   */
  result = append_common_prefix (chooser_entry, FALSE, TRUE);

  switch (result)
    {
    case INVALID_INPUT:
      /* We already beeped in append_common_prefix(); do nothing here */
      break;

    case NO_MATCH:
      beep (chooser_entry);
      /* translators: this text is shown when there are no completions 
       * for something the user typed in a file chooser entry
       */
      pop_up_completion_feedback (chooser_entry, _("No match"));
      break;

    case NOTHING_INSERTED_COMPLETE:
      /* FIXME: pop up the suggestion window or scroll it */
      break;

    case NOTHING_INSERTED_UNIQUE:
      /* translators: this text is shown when there is exactly one completion 
       * for something the user typed in a file chooser entry
       */
      pop_up_completion_feedback (chooser_entry, _("Sole completion"));
      break;

    case COMPLETED:
      /* Nothing to do */
      break;

    case COMPLETED_UNIQUE:
      /* Nothing to do */
      break;

    case COMPLETE_BUT_NOT_UNIQUE:
      /* translators: this text is shown when the text in a file chooser
       * entry is a complete filename, but could be continued to find
       * a longer match
       */
      pop_up_completion_feedback (chooser_entry, _("Complete, but not unique"));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
start_explicit_completion (GtkFileChooserEntry *chooser_entry)
{
  RefreshStatus status;
  gboolean is_error;
  char *feedback_msg;

  status = refresh_current_folder_and_file_part (chooser_entry, REFRESH_UP_TO_CURSOR_POSITION);

  is_error = FALSE;

  switch (status)
    {
    case REFRESH_OK:
      g_assert (chooser_entry->current_folder_file != NULL);

      if (chooser_entry->current_folder && _gtk_folder_is_finished_loading (chooser_entry->current_folder))
	explicitly_complete (chooser_entry);
      else
	{
	  chooser_entry->load_complete_action = LOAD_COMPLETE_EXPLICIT_COMPLETION;

	  /* Translators: this text is shown while the system is searching
	   * for possible completions for filenames in a file chooser entry. */
	  pop_up_completion_feedback (chooser_entry, _("Completing..."));
	}

      break;

    case REFRESH_INVALID_INPUT:
      is_error = TRUE;
      /* Translators: this is shown in the feedback for Tab-completion in a file
       * chooser's text entry, when the user enters an invalid path. */
      feedback_msg = _("Invalid path");
      break;

    case REFRESH_INCOMPLETE_HOSTNAME:
      is_error = TRUE;

      if (chooser_entry->local_only)
	{
	  /* hostnames in a local_only file chooser?  user error */

	  /* Translators: this is shown in the feedback for Tab-completion in a
	   * file chooser's text entry when the user enters something like
	   * "sftp://blahblah" in an app that only supports local filenames. */
	  feedback_msg = _("Only local files may be selected");
	}
      else
	{
	  /* Another option is to complete the hostname based on the remote volumes that are mounted */

	  /* Translators: this is shown in the feedback for Tab-completion in a
	   * file chooser's text entry when the user hasn't entered the first '/'
	   * after a hostname and yet hits Tab (such as "sftp://blahblah[Tab]") */
	  feedback_msg = _("Incomplete hostname; end it with '/'");
	}

      break;

    case REFRESH_NONEXISTENT:
      is_error = TRUE;

      /* Translators: this is shown in the feedback for Tab-completion in a file
       * chooser's text entry when the user enters a path that does not exist
       * and then hits Tab */
      feedback_msg = _("Path does not exist");
      break;

    case REFRESH_NOT_LOCAL:
      is_error = TRUE;
      feedback_msg = _("Only local files may be selected");
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  if (is_error)
    {
      g_assert (chooser_entry->current_folder_file == NULL);

      beep (chooser_entry);
      pop_up_completion_feedback (chooser_entry, feedback_msg);
      chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;
    }
}

static gboolean
gtk_file_chooser_entry_key_press_event (GtkWidget *widget,
					GdkEventKey *event)
{
  GtkFileChooserEntry *chooser_entry;
  GtkEditable *editable;
  GtkEntry *entry;
  GdkModifierType state;
  gboolean control_pressed;

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (widget);
  editable = GTK_EDITABLE (widget);
  entry = GTK_ENTRY (widget);

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
      if (chooser_entry->has_completion)
	gtk_editable_set_position (editable, gtk_entry_get_text_length (entry));
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
commit_completion_and_refresh (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->has_completion)
    {
      gtk_editable_set_position (GTK_EDITABLE (chooser_entry),
				 gtk_entry_get_text_length (GTK_ENTRY (chooser_entry)));
    }

  /* Here we ignore the result of refresh_current_folder_and_file_part(); there is nothing we can do with it */
  refresh_current_folder_and_file_part (chooser_entry, REFRESH_WHOLE_TEXT);
}

static void
gtk_file_chooser_entry_activate (GtkEntry *entry)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (entry);

  commit_completion_and_refresh (chooser_entry);
  GTK_ENTRY_CLASS (_gtk_file_chooser_entry_parent_class)->activate (entry);
}

/* Fills the completion store from the contents of the current folder */
static void
populate_completion_store (GtkFileChooserEntry *chooser_entry)
{
  GSList *files;
  GSList *tmp_list;

  discard_completion_store (chooser_entry);

  files = _gtk_folder_list_children (chooser_entry->current_folder);

  chooser_entry->completion_store = gtk_list_store_new (N_COLUMNS,
							G_TYPE_STRING,
							G_TYPE_FILE);

  for (tmp_list = files; tmp_list; tmp_list = tmp_list->next)
    {
      GFileInfo *info;
      GFile *file;

      file = tmp_list->data;

      info = _gtk_folder_get_info (chooser_entry->current_folder, file);

      if (info)
	{
	  gchar *display_name = g_strdup (g_file_info_get_display_name (info));
	  GtkTreeIter iter;
	  gboolean dummy;

          display_name = maybe_append_separator_to_file (chooser_entry, file, display_name, &dummy);

	  gtk_list_store_append (chooser_entry->completion_store, &iter);
	  gtk_list_store_set (chooser_entry->completion_store, &iter,
			      DISPLAY_NAME_COLUMN, display_name,
			      FILE_COLUMN, file,
			      -1);

	  g_object_unref (info);
          g_free (display_name);
	}
    }

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (chooser_entry->completion_store),
					DISPLAY_NAME_COLUMN, GTK_SORT_ASCENDING);

  gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (chooser_entry)),
				  GTK_TREE_MODEL (chooser_entry->completion_store));
}

/* When we finish loading the current folder, this function should get called to
 * perform the deferred autocompletion or explicit completion.
 */
static void
perform_load_complete_action (GtkFileChooserEntry *chooser_entry)
{
  switch (chooser_entry->load_complete_action)
    {
    case LOAD_COMPLETE_NOTHING:
      break;

    case LOAD_COMPLETE_AUTOCOMPLETE:
      autocomplete (chooser_entry);
      break;

    case LOAD_COMPLETE_EXPLICIT_COMPLETION:
      explicitly_complete (chooser_entry);
      break;

    default:
      g_assert_not_reached ();
    }

  chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;
}

static void
finish_folder_load (GtkFileChooserEntry *chooser_entry)
{
  populate_completion_store (chooser_entry);
  perform_load_complete_action (chooser_entry);

  gtk_widget_set_tooltip_text (GTK_WIDGET (chooser_entry), NULL);
}

/* Callback when the current folder finishes loading */
static void
finished_loading_cb (GtkFolder *folder,
		     gpointer   data)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (data);

  finish_folder_load (chooser_entry);
}

/* Callback when the current folder's handle gets obtained (not necessarily loaded completely) */
static void
load_directory_get_folder_callback (GCancellable  *cancellable,
				    GtkFolder     *folder,
				    const GError  *error,
				    gpointer       data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GtkFileChooserEntry *chooser_entry = data;

  if (cancellable != chooser_entry->load_folder_cancellable)
    goto out;

  chooser_entry->load_folder_cancellable = NULL;

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
	  pop_up_completion_feedback (chooser_entry, error->message);
	}

      discard_current_folder (chooser_entry);
    }

  if (cancelled || error)
    goto out;

  g_assert (folder != NULL);
  chooser_entry->current_folder = g_object_ref (folder);

  discard_completion_store (chooser_entry);

  if (_gtk_folder_is_finished_loading (chooser_entry->current_folder))
    finish_folder_load (chooser_entry);
  else
    g_signal_connect (chooser_entry->current_folder, "finished-loading",
		      G_CALLBACK (finished_loading_cb), chooser_entry);

out:
  g_object_unref (chooser_entry);
  g_object_unref (cancellable);
}

static RefreshStatus
start_loading_current_folder (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->file_system == NULL)
    return REFRESH_OK;

  g_assert (chooser_entry->current_folder_file != NULL);
  g_assert (chooser_entry->current_folder == NULL);
  g_assert (chooser_entry->load_folder_cancellable == NULL);

  if (chooser_entry->local_only
      && !_gtk_file_has_native_path (chooser_entry->current_folder_file))
    {
      g_object_unref (chooser_entry->current_folder_file);
      chooser_entry->current_folder_file = NULL;

      return REFRESH_NOT_LOCAL;
    }

  chooser_entry->load_folder_cancellable =
    _gtk_file_system_get_folder (chooser_entry->file_system,
			         chooser_entry->current_folder_file,
			 	"standard::name,standard::display-name,standard::type",
			         load_directory_get_folder_callback,
			         g_object_ref (chooser_entry));

  return REFRESH_OK;
}

static RefreshStatus
reload_current_folder (GtkFileChooserEntry *chooser_entry,
		       GFile               *folder_file,
		       gboolean             force_reload)
{
  gboolean reload = FALSE;

  g_assert (folder_file != NULL);

  if (chooser_entry->current_folder_file)
    {
      if ((!(g_file_equal (folder_file, chooser_entry->current_folder_file)
	     && chooser_entry->load_folder_cancellable))
	  || force_reload)
	{
	  reload = TRUE;

          discard_current_folder (chooser_entry);
	  discard_loading_and_current_folder_file (chooser_entry);

	  chooser_entry->current_folder_file = g_object_ref (folder_file);
	}
    }
  else
    {
      chooser_entry->current_folder_file = g_object_ref (folder_file);
      reload = TRUE;
    }

  if (reload)
    return start_loading_current_folder (chooser_entry);
  else
    return REFRESH_OK;
}

static RefreshStatus
refresh_current_folder_and_file_part (GtkFileChooserEntry *chooser_entry,
				      RefreshMode          refresh_mode)
{
  GtkEditable *editable;
  gint end_pos;
  gchar *text;
  GFile *folder_file;
  gchar *file_part;
  gsize total_len, file_part_len;
  gint file_part_pos;
  GError *error;
  RefreshStatus result;

  editable = GTK_EDITABLE (chooser_entry);

  switch (refresh_mode)
    {
    case REFRESH_UP_TO_CURSOR_POSITION:
      end_pos = gtk_editable_get_position (editable);
      break;

    case REFRESH_WHOLE_TEXT:
      end_pos = gtk_entry_get_text_length (GTK_ENTRY (chooser_entry));
      break;

    default:
      g_assert_not_reached ();
      return REFRESH_INVALID_INPUT;
    }

  text = gtk_editable_get_chars (editable, 0, end_pos);

  error = NULL;
  if (!chooser_entry->file_system ||
      !_gtk_file_system_parse (chooser_entry->file_system,
			       chooser_entry->base_folder, text,
			       &folder_file, &file_part, &error))
    {
      if (g_error_matches (error, GTK_FILE_CHOOSER_ERROR, GTK_FILE_CHOOSER_ERROR_INCOMPLETE_HOSTNAME))
	{
	  folder_file = NULL;
	  result = REFRESH_INCOMPLETE_HOSTNAME;
	}
      else
	{
	  folder_file = (chooser_entry->base_folder) ? g_object_ref (chooser_entry->base_folder) : NULL;

	  if (g_error_matches (error, GTK_FILE_CHOOSER_ERROR, GTK_FILE_CHOOSER_ERROR_NONEXISTENT))
	    result = REFRESH_NONEXISTENT;
	  else
	    result = REFRESH_INVALID_INPUT;
	}

      if (error)
	g_error_free (error);

      file_part = g_strdup ("");
      file_part_pos = -1;
    }
  else
    {
      g_assert (folder_file != NULL);

      file_part_len = strlen (file_part);
      total_len = strlen (text);
      if (total_len > file_part_len)
	file_part_pos = g_utf8_strlen (text, total_len - file_part_len);
      else
	file_part_pos = 0;

      result = REFRESH_OK;
    }

  g_free (text);

  g_free (chooser_entry->file_part);

  chooser_entry->file_part = file_part;
  chooser_entry->file_part_pos = file_part_pos;

  if (result == REFRESH_OK)
    {
      result = reload_current_folder (chooser_entry, folder_file, file_part_pos == -1);
    }
  else
    {
      discard_current_folder (chooser_entry);
      discard_loading_and_current_folder_file (chooser_entry);
    }

  if (folder_file)
    g_object_unref (folder_file);

  g_assert (/* we are OK and we have a current folder file and (loading process or folder handle)... */
	    ((result == REFRESH_OK)
	     && (chooser_entry->current_folder_file != NULL)
	     && (((chooser_entry->load_folder_cancellable != NULL) && (chooser_entry->current_folder == NULL))
		 || ((chooser_entry->load_folder_cancellable == NULL) && (chooser_entry->current_folder != NULL))))
	    /* ... OR we have an error, and we don't have a current folder file nor a loading process nor a folder handle */
	    || ((result != REFRESH_OK)
		&& (chooser_entry->current_folder_file == NULL)
		&& (chooser_entry->load_folder_cancellable == NULL)
		&& (chooser_entry->current_folder == NULL)));

  return result;
}

static void
autocomplete (GtkFileChooserEntry *chooser_entry)
{
  if (!(chooser_entry->current_folder != NULL
	&& _gtk_folder_is_finished_loading (chooser_entry->current_folder)
	&& gtk_editable_get_position (GTK_EDITABLE (chooser_entry)) == gtk_entry_get_text_length (GTK_ENTRY (chooser_entry))))
    return;

  append_common_prefix (chooser_entry, TRUE, FALSE);
}

static void
start_autocompletion (GtkFileChooserEntry *chooser_entry)
{
  RefreshStatus status;

  status = refresh_current_folder_and_file_part (chooser_entry, REFRESH_UP_TO_CURSOR_POSITION);

  switch (status)
    {
    case REFRESH_OK:
      g_assert (chooser_entry->current_folder_file != NULL);

      if (chooser_entry->current_folder && _gtk_folder_is_finished_loading (chooser_entry->current_folder))
	autocomplete (chooser_entry);
      else
	chooser_entry->load_complete_action = LOAD_COMPLETE_AUTOCOMPLETE;

      break;

    case REFRESH_INVALID_INPUT:
    case REFRESH_INCOMPLETE_HOSTNAME:
    case REFRESH_NONEXISTENT:
    case REFRESH_NOT_LOCAL:
      /* We don't beep or anything, since this is autocompletion - the user
       * didn't request any action explicitly.
       */
      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
start_autocompletion_idle_handler (gpointer data)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (data);

  start_autocompletion (chooser_entry);

  chooser_entry->start_autocompletion_idle_id = 0;

  return FALSE;
}

static void
install_start_autocompletion_idle (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->start_autocompletion_idle_id != 0)
    return;

  chooser_entry->start_autocompletion_idle_id = gdk_threads_add_idle (start_autocompletion_idle_handler, chooser_entry);
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
 * @file: file for a folder in the chooser entries current file system.
 *
 * Sets the folder with respect to which completions occur.
 **/
void
_gtk_file_chooser_entry_set_base_folder (GtkFileChooserEntry *chooser_entry,
					 GFile               *file)
{
  if (chooser_entry->base_folder)
    g_object_unref (chooser_entry->base_folder);

  chooser_entry->base_folder = file;

  if (chooser_entry->base_folder)
    g_object_ref (chooser_entry->base_folder);

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
 * Return value: the file for the current folder - this value is owned by the
 *  chooser entry and must not be modified or freed.
 **/
GFile *
_gtk_file_chooser_entry_get_current_folder (GtkFileChooserEntry *chooser_entry)
{
  commit_completion_and_refresh (chooser_entry);
  return chooser_entry->current_folder_file;
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
  commit_completion_and_refresh (chooser_entry);
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

  chooser_entry->in_change = TRUE;
  clear_completions (chooser_entry);
  gtk_entry_set_text (GTK_ENTRY (chooser_entry), file_part);
  chooser_entry->in_change = FALSE;
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
  gboolean retval = FALSE;

  if (chooser_entry->current_folder)
    {
      GFileInfo *file_info;

      file_info = _gtk_folder_get_info (chooser_entry->current_folder, file);

      if (file_info)
        {
	  retval = _gtk_file_info_consider_as_directory (file_info);
	  g_object_unref (file_info);
	}
    }

  return retval;
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
