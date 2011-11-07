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

  GFile *base_folder;
  GFile *current_folder_file;
  gchar *dir_part;
  gchar *file_part;
  gint file_part_pos;

  LoadCompleteAction load_complete_action;

  GtkTreeModel *completion_store;

  guint current_folder_loaded : 1;
  guint in_change      : 1;
  guint eat_tabs       : 1;
  guint local_only     : 1;
};

enum
{
  DISPLAY_NAME_COLUMN,
  FULL_PATH_COLUMN,
  FILE_COLUMN,
  N_COLUMNS
};

static void     gtk_file_chooser_entry_finalize       (GObject          *object);
static void     gtk_file_chooser_entry_dispose        (GObject          *object);
static void     gtk_file_chooser_entry_grab_focus     (GtkWidget        *widget);
static gboolean gtk_file_chooser_entry_key_press_event (GtkWidget *widget,
							GdkEventKey *event);
static gboolean gtk_file_chooser_entry_focus_out_event (GtkWidget       *widget,
							GdkEventFocus   *event);
static void     gtk_file_chooser_entry_activate       (GtkEntry         *entry);

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

static RefreshStatus refresh_current_folder_and_file_part (GtkFileChooserEntry *chooser_entry,
						           const char          *text);
static void finished_loading_cb (GtkFileSystemModel  *model,
                                 GError              *error,
		                 GtkFileChooserEntry *chooser_entry);

G_DEFINE_TYPE (GtkFileChooserEntry, _gtk_file_chooser_entry, GTK_TYPE_ENTRY)

static void
gtk_file_chooser_entry_dispatch_properties_changed (GObject     *object,
                                                    guint        n_pspecs,
                                                    GParamSpec **pspecs)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);
  GtkEditable *editable = GTK_EDITABLE (object);
  guint i;

  G_OBJECT_CLASS (_gtk_file_chooser_entry_parent_class)->dispatch_properties_changed (object, n_pspecs, pspecs);

  /* What we are after: The text in front of the cursor was modified.
   * Unfortunately, there's no other way to catch this. */

  if (chooser_entry->in_change)
    return;

  for (i = 0; i < n_pspecs; i++)
    {
      if (pspecs[i]->name == I_("cursor-position") ||
          pspecs[i]->name == I_("selection-bound") ||
          pspecs[i]->name == I_("text"))
        {
          char *text;
          int start, end;

          chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;

          gtk_editable_get_selection_bounds (editable, &start, &end);
          text = gtk_editable_get_chars (editable, 0, MIN (start, end));
          refresh_current_folder_and_file_part (chooser_entry, text);
          g_free (text);

          break;
        }
    }
}

static void
_gtk_file_chooser_entry_class_init (GtkFileChooserEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkEntryClass *entry_class = GTK_ENTRY_CLASS (class);

  gobject_class->finalize = gtk_file_chooser_entry_finalize;
  gobject_class->dispose = gtk_file_chooser_entry_dispose;
  gobject_class->dispatch_properties_changed = gtk_file_chooser_entry_dispatch_properties_changed;

  widget_class->grab_focus = gtk_file_chooser_entry_grab_focus;
  widget_class->key_press_event = gtk_file_chooser_entry_key_press_event;
  widget_class->focus_out_event = gtk_file_chooser_entry_focus_out_event;

  entry_class->activate = gtk_file_chooser_entry_activate;
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
discard_loading_and_current_folder_file (GtkFileChooserEntry *chooser_entry)
{
  if (chooser_entry->current_folder_file)
    {
      g_object_unref (chooser_entry->current_folder_file);
      chooser_entry->current_folder_file = NULL;
    }
}

static void
gtk_file_chooser_entry_dispose (GObject *object)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (object);

  discard_loading_and_current_folder_file (chooser_entry);

  if (chooser_entry->completion_store)
    {
      g_object_unref (chooser_entry->completion_store);
      chooser_entry->completion_store = NULL;
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
  char *path;
  
  gtk_tree_model_get (model, iter,
		      FULL_PATH_COLUMN, &path,
                      -1);

  /* We don't set in_change here as we want to update the current_folder
   * variable */
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

static gboolean
gtk_file_chooser_entry_parse (GtkFileChooserEntry  *chooser_entry,
                              const gchar          *str,
                              GFile               **folder,
                              gchar               **file_part,
                              GError              **error)
{
  GFile *file;
  gboolean result = FALSE;
  gboolean is_dir = FALSE;
  gchar *last_slash = NULL;

  if (str && *str)
    is_dir = (str [strlen (str) - 1] == G_DIR_SEPARATOR);

  last_slash = strrchr (str, G_DIR_SEPARATOR);

  file = gtk_file_chooser_get_file_for_text (chooser_entry, str);

  if (g_file_equal (chooser_entry->base_folder, file))
    {
      /* this is when user types '.', could be the
       * beginning of a hidden file, ./ or ../
       */
      *folder = g_object_ref (file);
      *file_part = g_strdup (str);
      result = TRUE;
    }
  else if (is_dir)
    {
      /* it's a dir, or at least it ends with the dir separator */
      *folder = g_object_ref (file);
      *file_part = g_strdup ("");
      result = TRUE;
    }
  else
    {
      GFile *parent_file;

      parent_file = g_file_get_parent (file);

      if (!parent_file)
	{
	  g_set_error (error,
		       GTK_FILE_CHOOSER_ERROR,
		       GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
		       "Could not get parent file");
	  *folder = NULL;
	  *file_part = NULL;
	}
      else
	{
	  *folder = parent_file;
	  result = TRUE;

	  if (last_slash)
	    *file_part = g_strdup (last_slash + 1);
	  else
	    *file_part = g_strdup (str);
	}
    }

  g_object_unref (file);

  return result;
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

  parsed = gtk_file_chooser_entry_parse (chooser_entry,
                                         text_up_to_cursor,
                                         &parsed_folder_file,
                                         &parsed_file_part,
                                         error);

  g_free (text_up_to_cursor);

  if (!parsed)
    return FALSE;

  g_assert (parsed_folder_file != NULL
	    && g_file_equal (parsed_folder_file, chooser_entry->current_folder_file));

  g_object_unref (parsed_folder_file);

  /* First pass: find the common prefix */

  valid = gtk_tree_model_get_iter_first (chooser_entry->completion_store, &iter);

  while (valid)
    {
      gchar *display_name;
      GFile *file;

      gtk_tree_model_get (chooser_entry->completion_store,
			  &iter,
			  DISPLAY_NAME_COLUMN, &display_name,
			  FILE_COLUMN, &file,
			  -1);

      if (g_str_has_prefix (display_name, parsed_file_part))
	{
	  if (!*common_prefix_ret)
	    {
	      *common_prefix_ret = g_strdup (display_name);
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
      valid = gtk_tree_model_iter_next (chooser_entry->completion_store, &iter);
    }

  /* Second pass: see if the prefix we found is a complete match */

  if (*common_prefix_ret != NULL)
    {
      valid = gtk_tree_model_get_iter_first (chooser_entry->completion_store, &iter);

      while (valid)
	{
	  gchar *display_name;
	  int len;

	  gtk_tree_model_get (chooser_entry->completion_store,
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
	  valid = gtk_tree_model_iter_next (chooser_entry->completion_store, &iter);
	}

      /* Finally:  Did we generate a new completion, or was the user's input already completed as far as it could go? */

      *prefix_expands_the_file_part_ret = g_utf8_strlen (*common_prefix_ret, -1) > g_utf8_strlen (parsed_file_part, -1);
    }

  g_free (parsed_file_part);

  return TRUE;
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
append_common_prefix (GtkFileChooserEntry *chooser_entry)
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
      g_error_free (error);

      return INVALID_INPUT;
    }

  have_result = FALSE;

  if (unique_file)
    {
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
gtk_file_chooser_entry_grab_focus (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (_gtk_file_chooser_entry_parent_class)->grab_focus (widget);
  _gtk_file_chooser_entry_select_filename (GTK_FILE_CHOOSER_ENTRY (widget));
}

static void
explicitly_complete (GtkFileChooserEntry *chooser_entry)
{
  CommonPrefixResult result;

  g_assert (chooser_entry->current_folder_loaded);

  /* FIXME: see what Emacs does in case there is no common prefix, or there is more than one match:
   *
   * - If there is a common prefix, insert it (done)
   * - If there is no common prefix, pop up the suggestion window
   * - If there are no matches at all, beep and bring up a tooltip (done)
   * - If the suggestion window is already up, scroll it
   */
  result = append_common_prefix (chooser_entry);

  switch (result)
    {
    case INVALID_INPUT:
      /* We already beeped in append_common_prefix(); do nothing here */
      break;

    case NO_MATCH:
      beep (chooser_entry);
      break;

    case NOTHING_INSERTED_COMPLETE:
      /* FIXME: pop up the suggestion window or scroll it */
      break;

    case NOTHING_INSERTED_UNIQUE:
      break;

    case COMPLETED:
      /* Nothing to do */
      break;

    case COMPLETED_UNIQUE:
      /* Nothing to do */
      break;

    case COMPLETE_BUT_NOT_UNIQUE:
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
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (chooser_entry),
                                 0, gtk_editable_get_position (GTK_EDITABLE (chooser_entry)));
  status = refresh_current_folder_and_file_part (chooser_entry, text);
  g_free (text);

  is_error = FALSE;

  switch (status)
    {
    case REFRESH_OK:
      g_assert (chooser_entry->current_folder_file != NULL);

      if (chooser_entry->current_folder_loaded)
	explicitly_complete (chooser_entry);
      else
	{
	  chooser_entry->load_complete_action = LOAD_COMPLETE_EXPLICIT_COMPLETION;
	}

      break;

    case REFRESH_INVALID_INPUT:
      is_error = TRUE;
      break;

    case REFRESH_INCOMPLETE_HOSTNAME:
      is_error = TRUE;
      break;

    case REFRESH_NONEXISTENT:
      is_error = TRUE;
      break;

    case REFRESH_NOT_LOCAL:
      is_error = TRUE;
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  if (is_error)
    {
      g_assert (chooser_entry->current_folder_file == NULL);

      beep (chooser_entry);
      chooser_entry->load_complete_action = LOAD_COMPLETE_NOTHING;
    }
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
commit_completion_and_refresh (GtkFileChooserEntry *chooser_entry)
{
  /* Here we ignore the result of refresh_current_folder_and_file_part(); there is nothing we can do with it */
  refresh_current_folder_and_file_part (chooser_entry, gtk_entry_get_text (GTK_ENTRY (chooser_entry)));
}

static void
gtk_file_chooser_entry_activate (GtkEntry *entry)
{
  GtkFileChooserEntry *chooser_entry = GTK_FILE_CHOOSER_ENTRY (entry);

  commit_completion_and_refresh (chooser_entry);
  GTK_ENTRY_CLASS (_gtk_file_chooser_entry_parent_class)->activate (entry);
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
    case FILE_COLUMN:
      g_value_set_object (value, file);
      break;
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
  discard_completion_store (chooser_entry);

  chooser_entry->completion_store = GTK_TREE_MODEL (
      _gtk_file_system_model_new_for_directory (chooser_entry->current_folder_file,
                                                "standard::name,standard::display-name,standard::type",
                                                completion_store_set,
                                                chooser_entry,
                                                N_COLUMNS,
                                                G_TYPE_STRING,
                                                G_TYPE_STRING,
                                                G_TYPE_FILE));
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

static RefreshStatus
reload_current_folder (GtkFileChooserEntry *chooser_entry,
		       GFile               *folder_file)
{
  g_assert (folder_file != NULL);

  if (chooser_entry->current_folder_file
      && g_file_equal (folder_file, chooser_entry->current_folder_file))
    return REFRESH_OK;

  if (chooser_entry->current_folder_file)
    {
      discard_loading_and_current_folder_file (chooser_entry);
    }
  
  if (chooser_entry->local_only
      && !g_file_is_native (folder_file))
    return REFRESH_NOT_LOCAL;

  chooser_entry->current_folder_file = g_object_ref (folder_file);
  chooser_entry->current_folder_loaded = FALSE;

  populate_completion_store (chooser_entry);

  return REFRESH_OK;
}

static RefreshStatus
refresh_current_folder_and_file_part (GtkFileChooserEntry *chooser_entry,
				      const gchar *        text)
{
  GFile *folder_file;
  gchar *file_part;
  gsize total_len, file_part_len;
  gint file_part_pos;
  GError *error;
  RefreshStatus result;

  error = NULL;
  if (!gtk_file_chooser_entry_parse (chooser_entry,
			             text, &folder_file, &file_part, &error))
    {
      folder_file = g_object_ref (chooser_entry->base_folder);

      if (g_error_matches (error, GTK_FILE_CHOOSER_ERROR, GTK_FILE_CHOOSER_ERROR_NONEXISTENT))
        result = REFRESH_NONEXISTENT;
      else
	result = REFRESH_INVALID_INPUT;

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

  g_free (chooser_entry->file_part);
  g_free (chooser_entry->dir_part);

  chooser_entry->dir_part = file_part_pos > 0 ? g_strndup (text, file_part_pos) : g_strdup ("");
  chooser_entry->file_part = file_part;
  chooser_entry->file_part_pos = file_part_pos;

  if (result == REFRESH_OK)
    {
      result = reload_current_folder (chooser_entry, folder_file);
    }
  else
    {
      discard_loading_and_current_folder_file (chooser_entry);
    }

  if (folder_file)
    g_object_unref (folder_file);

  g_assert (/* we are OK and we have a current folder file and (loading process or folder handle)... */
	    ((result == REFRESH_OK)
	     && (chooser_entry->current_folder_file != NULL))
	    /* ... OR we have an error, and we don't have a current folder file nor a loading process nor a folder handle */
	    || ((result != REFRESH_OK)
		&& (chooser_entry->current_folder_file == NULL)));

  return result;
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
