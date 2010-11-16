/*
 * gtkopenwithdialog.c: an open-with dialog
 *
 * Copyright (C) 2004 Novell, Inc.
 * Copyright (C) 2007, 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 */

#include <config.h>

#include "gtkopenwithdialog.h"

#include "gtkintl.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#define sure_string(s) ((const char *) ((s) != NULL ? (s) : ""))

struct _GtkOpenWithDialogPrivate {
  GAppInfo *selected_app_info;

  char *content_type;
  GFile *gfile;
  GtkOpenWithDialogMode mode;

  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *checkbox;

  GtkWidget *desc_label;
  GtkWidget *open_label;

  GtkWidget *program_list;
  GtkListStore *program_list_store;
  gint add_items_idle_id;
};

enum {
  COLUMN_APP_INFO,
  COLUMN_GICON,
  COLUMN_NAME,
  COLUMN_COMMENT,
  COLUMN_EXEC,
  COLUMN_HEADING,
  COLUMN_HEADING_TEXT,
  COLUMN_RECOMMENDED,
  NUM_COLUMNS
};

enum {
  APPLICATION_SELECTED,
  LAST_SIGNAL
};

enum {
  PROP_GFILE = 1,
  PROP_CONTENT_TYPE,
  PROP_MODE,
  N_PROPERTIES
};

#define RESPONSE_REMOVE 1

static GParamSpec *properties[N_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkOpenWithDialog, gtk_open_with_dialog, GTK_TYPE_DIALOG); 

static void
show_error_dialog (const gchar *primary,
		   const gchar *secondary,
		   GtkWindow *parent)
{
  GtkWidget *message_dialog;

  message_dialog = gtk_message_dialog_new (parent, 0,
					   GTK_MESSAGE_ERROR,
					   GTK_BUTTONS_OK,
					   NULL);
  g_object_set (message_dialog,
		"text", primary,
		"secondary-text", secondary,
		NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (message_dialog), GTK_RESPONSE_OK);

  gtk_widget_show (message_dialog);

  g_signal_connect (message_dialog, "response",
		    G_CALLBACK (gtk_widget_destroy), NULL);
}

/* An application is valid if:
 *
 * 1) The file exists
 * 2) The user has permissions to run the file
 */
static gboolean
check_application (GtkOpenWithDialog *self)
{
  char *command;
  char *path = NULL;
  char **argv = NULL;
  int argc;
  GError *error = NULL;
  gint retval = TRUE;

  command = NULL;
  if (self->priv->selected_app_info != NULL)
    command = g_strdup (g_app_info_get_executable (self->priv->selected_app_info));

  if (command == NULL)
    command = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->priv->entry)));
	
  g_shell_parse_argv (command, &argc, &argv, &error);
  if (error)
    {
      show_error_dialog (_("Could not run application"),
			 error->message,
			 GTK_WINDOW (self));
      g_error_free (error);
      retval = FALSE;
      goto cleanup;
    }

  path = g_find_program_in_path (argv[0]);
  if (!path)
    {
      char *error_message;

      error_message = g_strdup_printf (_("Could not find '%s'"),
				       argv[0]);

      show_error_dialog (_("Could not find application"),
			 error_message,
			 GTK_WINDOW (self));
      g_free (error_message);
      retval = FALSE;
      goto cleanup;
    }

 cleanup:
  g_strfreev (argv);
  g_free (path);
  g_free (command);

  return retval;
}

/* Only called for non-desktop files */
static char *
get_app_name (const char *commandline,
	      GError **error)
{
  char *basename;
  char *unquoted;
  char **argv;
  int argc;

  if (!g_shell_parse_argv (commandline,
			   &argc, &argv, error))
    return NULL;
	
  unquoted = g_shell_unquote (argv[0], NULL);
  if (unquoted)
    basename = g_path_get_basename (unquoted);
  else
    basename = g_strdup (argv[0]);

  g_free (unquoted);
  g_strfreev (argv);

  return basename;
}

/* This will check if the application the user wanted exists will return that
 * application.  If it doesn't exist, it will create one and return that.
 * It also sets the app info as the default for this type.
 */
static GAppInfo *
add_or_find_application (GtkOpenWithDialog *self)
{
  GAppInfo *app;
  char *app_name;
  const char *commandline;
  GError *error;
  gboolean success, should_set_default;
  char *message;
  GList *applications;

  error = NULL;
  app = NULL;
  if (self->priv->selected_app_info)
    {
      app = g_object_ref (self->priv->selected_app_info);
    }
  else
    {
      commandline = gtk_entry_get_text (GTK_ENTRY (self->priv->entry));
      app_name = get_app_name (commandline, &error);
      if (app_name != NULL)
	{
	  app = g_app_info_create_from_commandline (commandline,
						    app_name,
						    G_APP_INFO_CREATE_NONE,
						    &error);
	  g_free (app_name);
	}
    }

  if (app == NULL)
    {
      message = g_strdup_printf (_("Could not add application to the application database: %s"), error->message);
      show_error_dialog (_("Could not add application"),
			 message,
			 GTK_WINDOW (self));
      g_free (message);
      g_error_free (error);
      return NULL;
    }

  should_set_default =
    (self->priv->mode == GTK_OPEN_WITH_DIALOG_MODE_SELECT_DEFAULT) ||
    (self->priv->mode == GTK_OPEN_WITH_DIALOG_MODE_OPEN_FILE &&
     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->checkbox)));
  success = TRUE;

  if (should_set_default)
    {
      success = g_app_info_set_as_default_for_type (app,
						    self->priv->content_type,
						    &error);
    }
  else
    {
      applications = g_app_info_get_all_for_type (self->priv->content_type);
      if (self->priv->content_type && applications != NULL)
	{
	  /* we don't care about reporting errors here */
	  g_app_info_add_supports_type (app,
					self->priv->content_type,
					NULL);
	}

      if (applications != NULL) {
	g_list_free_full (applications, g_object_unref);
      }
    }

  if (!success && should_set_default)
    {
      message = g_strdup_printf (_("Could not set application as the default: %s"), error->message);
      show_error_dialog (_("Could not set as default application"),
			 message,
			 GTK_WINDOW (self));
      g_free (message);
      g_error_free (error);
    }

  return app;
}

static void
emit_application_selected (GtkOpenWithDialog *self,
			   GAppInfo *application)
{
	g_signal_emit (self, signals[APPLICATION_SELECTED], 0,
		       application);
}

static void
gtk_open_with_dialog_response (GtkDialog *dialog,
			       gint response_id)
{
  GAppInfo *application;
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (dialog);

  switch (response_id)
    {
    case GTK_RESPONSE_OK:
      if (check_application (self))
	{
	  application = add_or_find_application (self);

	  if (application)
	    {
	      emit_application_selected (self, application);
	      g_object_unref (application);
	    }
	}

      break;
    case RESPONSE_REMOVE:
      if (self->priv->selected_app_info != NULL)
	{
	  if (g_app_info_delete (self->priv->selected_app_info))
	    {
	      GtkTreeModel *model;
	      GtkTreeIter iter;
	      GAppInfo *info, *selected;

	      selected = self->priv->selected_app_info;
	      self->priv->selected_app_info = NULL;

	      model = GTK_TREE_MODEL (self->priv->program_list_store);
	      if (gtk_tree_model_get_iter_first (model, &iter))
		{
		  do
		    {
		      gtk_tree_model_get (model, &iter,
					  COLUMN_APP_INFO, &info,
					  -1);
		      if (g_app_info_equal (selected, info))
			{
			  gtk_list_store_remove (self->priv->program_list_store, &iter);
			  break;
			}
		    }
		  while (gtk_tree_model_iter_next (model, &iter));
		}

	      g_object_unref (selected);
	    }
	}

      break;
    default :
      break;
    }
}

static void
chooser_response_cb (GtkFileChooser *chooser,
		     int response,
		     gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;

  if (response == GTK_RESPONSE_OK)
    {
      char *filename;

      filename = gtk_file_chooser_get_filename (chooser);

      if (filename)
	{
	  char *quoted_text;

	  quoted_text = g_shell_quote (filename);

	  gtk_entry_set_text (GTK_ENTRY (self->priv->entry),
			      quoted_text);
	  gtk_editable_set_position (GTK_EDITABLE (self->priv->entry), -1);
	  g_free (quoted_text);
	  g_free (filename);
	}
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
browse_clicked_cb (GtkWidget *button,
		   gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;
  GtkWidget *chooser;

  chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
					 GTK_WINDOW (self),
					 GTK_FILE_CHOOSER_ACTION_OPEN,
					 GTK_STOCK_CANCEL,
					 GTK_RESPONSE_CANCEL,
					 GTK_STOCK_OPEN,
					 GTK_RESPONSE_OK,
					 NULL);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser), TRUE);
  g_signal_connect (chooser, "response",
		    G_CALLBACK (chooser_response_cb), self);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser),
				   GTK_RESPONSE_OK);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser),
					FALSE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
				       "/usr/bin");

  gtk_widget_show (chooser);
}

static void
entry_changed_cb (GtkWidget *entry,
		  gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;

  /* We are writing in the entry, so we are not using a known appinfo anymore */
  if (self->priv->selected_app_info != NULL)
    {
    g_object_unref (self->priv->selected_app_info);
    self->priv->selected_app_info = NULL;
    }

  if (gtk_entry_get_text (GTK_ENTRY (self->priv->entry))[0] == '\000')
    gtk_widget_set_sensitive (self->priv->button, FALSE);
  else
    gtk_widget_set_sensitive (self->priv->button, TRUE);
}

static gboolean
gtk_open_with_search_equal_func (GtkTreeModel *model,
				 int column,
				 const char *key,
				 GtkTreeIter *iter,
				 gpointer user_data)
{
  char *normalized_key;
  char *name, *normalized_name;
  char *path, *normalized_path;
  char *basename, *normalized_basename;
  gboolean ret;

  if (key != NULL)
    {
      normalized_key = g_utf8_casefold (key, -1);
      g_assert (normalized_key != NULL);

      ret = TRUE;

      gtk_tree_model_get (model, iter,
			  COLUMN_NAME, &name,
			  COLUMN_EXEC, &path,
			  -1);

      if (name != NULL)
	{
	  normalized_name = g_utf8_casefold (name, -1);
	  g_assert (normalized_name != NULL);

	  if (strncmp (normalized_name, normalized_key, strlen (normalized_key)) == 0) {
	    ret = FALSE;
	  }

	  g_free (normalized_name);
	}

      if (ret && path != NULL)
	{
	  normalized_path = g_utf8_casefold (path, -1);
	  g_assert (normalized_path != NULL);

	  basename = g_path_get_basename (path);
	  g_assert (basename != NULL);

	  normalized_basename = g_utf8_casefold (basename, -1);
	  g_assert (normalized_basename != NULL);

	  if (strncmp (normalized_path, normalized_key, strlen (normalized_key)) == 0 ||
	      strncmp (normalized_basename, normalized_key, strlen (normalized_key)) == 0) {
	    ret = FALSE;
	  }

	  g_free (basename);
	  g_free (normalized_basename);
	  g_free (normalized_path);
	}

      g_free (name);
      g_free (path);
      g_free (normalized_key);

      return ret;
    }
  else
    {
      return TRUE;
    }
}

static gint
gtk_open_with_sort_func (GtkTreeModel *model,
			 GtkTreeIter *a,
			 GtkTreeIter *b,
			 gpointer user_data)
{
  gboolean a_recommended, b_recommended;
  gchar *a_name, *b_name, *a_casefold, *b_casefold;
  gint retval;

  /* this returns:
   * - <0 if a should show before b
   * - =0 if a is the same as b
   * - >0 if a should show after b
   */

  gtk_tree_model_get (model, a,
		      COLUMN_NAME, &a_name,
		      COLUMN_RECOMMENDED, &a_recommended,
		      -1);

  gtk_tree_model_get (model, b,
		      COLUMN_NAME, &b_name,
		      COLUMN_RECOMMENDED, &b_recommended,
		      -1);

  /* the recommended one always wins */
  if (a_recommended && !b_recommended)
    {
      retval = -1;
      goto out;
    }

  if (b_recommended && !a_recommended)
    {
      retval = 1;
      goto out;
    }

  a_casefold = a_name != NULL ?
    g_utf8_casefold (a_name, -1) : NULL;
  b_casefold = b_name != NULL ?
    g_utf8_casefold (b_name, -1) : NULL;

  retval = g_strcmp0 (a_casefold, b_casefold);

  g_free (a_casefold);
  g_free (b_casefold);

 out:
  g_free (a_name);
  g_free (b_name);

  return retval;
}

static void
heading_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer _user_data)
{
  gboolean heading;

  gtk_tree_model_get (model, iter,
		      COLUMN_HEADING, &heading,
		      -1);

  g_object_set  (cell,
		 "visible", heading,
		 NULL);
}

static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
  gboolean heading;

  gtk_tree_model_get (model, iter,
		      COLUMN_HEADING, &heading,
		      -1);
  if (heading)
    g_object_set (cell,
		  "visible", FALSE,
		  "xpad", 0,
		  "ypad", 0,
		  NULL);
  else
    g_object_set (cell,
		  "visible", TRUE,
		  "xpad", 3,
		  "ypad", 3,
		  NULL);
}

static gboolean
gtk_open_with_selection_func (GtkTreeSelection *selection,
			      GtkTreeModel *model,
			      GtkTreePath *path,
			      gboolean path_currently_selected,
			      gpointer user_data)
{
  GtkTreeIter iter;
  gboolean heading;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
		      COLUMN_HEADING, &heading,
		      -1);

  return !heading;
}

static gint
compare_apps_func (gconstpointer a,
		   gconstpointer b)
{
  return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static gboolean
gtk_open_with_dialog_add_items_idle (gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *sort;
  GList *all_applications, *content_type_apps;
  GList *l;
  gboolean heading_added;

  /* create list store */
  self->priv->program_list_store = gtk_list_store_new (NUM_COLUMNS,
						       G_TYPE_APP_INFO,
						       G_TYPE_ICON,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_BOOLEAN,
						       G_TYPE_STRING,
						       G_TYPE_BOOLEAN);
  sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (self->priv->program_list_store));
  content_type_apps = g_app_info_get_all_for_type (self->priv->content_type);
  all_applications = g_app_info_get_all ();

  heading_added = FALSE;
  
  for (l = content_type_apps; l != NULL; l = l->next)
    {
      GAppInfo *app = l->data;
      GtkTreeIter iter;

      if (!g_app_info_supports_uris (app) &&
	  !g_app_info_supports_files (app))
	continue;

      if (!heading_added)
	{
	  gtk_list_store_append (self->priv->program_list_store, &iter);
	  gtk_list_store_set (self->priv->program_list_store, &iter,
			      COLUMN_HEADING_TEXT, _("Recommended Applications"),
			      COLUMN_HEADING, TRUE,
			      COLUMN_RECOMMENDED, TRUE,
			      -1);

	  heading_added = TRUE;
	}

      gtk_list_store_append (self->priv->program_list_store, &iter);
      gtk_list_store_set (self->priv->program_list_store, &iter,
			  COLUMN_APP_INFO, app,
			  COLUMN_GICON, g_app_info_get_icon (app),
			  COLUMN_NAME, g_app_info_get_display_name (app),
			  COLUMN_COMMENT, g_app_info_get_description (app),
			  COLUMN_EXEC, g_app_info_get_executable,
			  COLUMN_HEADING, FALSE,
			  COLUMN_RECOMMENDED, TRUE,
			  -1);
    }

  heading_added = FALSE;

  for (l = all_applications; l != NULL; l = l->next)
    {
      GAppInfo *app = l->data;
      GtkTreeIter iter;

      if (!g_app_info_supports_uris (app) &&
	  !g_app_info_supports_files (app))
	continue;

      if (g_list_find_custom (content_type_apps, app,
			      (GCompareFunc) compare_apps_func))
	continue;

      if (!heading_added)
	{
	  gtk_list_store_append (self->priv->program_list_store, &iter);
	  gtk_list_store_set (self->priv->program_list_store, &iter,
			      COLUMN_HEADING_TEXT, _("Other Applications"),
			      COLUMN_HEADING, TRUE,
			      COLUMN_RECOMMENDED, FALSE,
			      -1);

	  heading_added = TRUE;
	}

      gtk_list_store_append (self->priv->program_list_store, &iter);
      gtk_list_store_set (self->priv->program_list_store, &iter,
			  COLUMN_APP_INFO, app,
			  COLUMN_GICON, g_app_info_get_icon (app),
			  COLUMN_NAME, g_app_info_get_display_name (app),
			  COLUMN_COMMENT, g_app_info_get_description (app),
			  COLUMN_EXEC, g_app_info_get_executable,
			  COLUMN_HEADING, FALSE,
			  COLUMN_RECOMMENDED, FALSE,
			  -1);
    }

  g_list_free_full (content_type_apps, g_object_unref);
  g_list_free_full (all_applications, g_object_unref);

  gtk_tree_view_set_model (GTK_TREE_VIEW (self->priv->program_list), 
			   GTK_TREE_MODEL (sort));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort),
					COLUMN_NAME,
					GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort),
				   COLUMN_NAME,
				   gtk_open_with_sort_func,
				   self, NULL);
  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (self->priv->program_list),
  				       gtk_open_with_search_equal_func,
  				       NULL, NULL);

  column = gtk_tree_view_column_new ();

  /* heading text renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "text", COLUMN_HEADING_TEXT,
				       NULL);
  g_object_set (renderer,
		"weight", PANGO_WEIGHT_BOLD,
		"weight-set", TRUE,
		"ypad", 6,
		"xpad", 0,
		NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   heading_cell_renderer_func,
					   NULL, NULL);

  /* padding renderer for non-heading cells */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   padding_cell_renderer_func,
					   NULL, NULL);

  /* app icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "gicon", COLUMN_GICON,
				       NULL);

  /* app name renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "text", COLUMN_NAME,
				       NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->program_list), column);

  self->priv->add_items_idle_id = 0;

  return FALSE;
}

static void
program_list_selection_changed (GtkTreeSelection  *selection,
				gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GAppInfo *info;
		
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_widget_set_sensitive (self->priv->button, FALSE);
      gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
					 RESPONSE_REMOVE,
					 FALSE);
      return;
    }

  info = NULL;
  gtk_tree_model_get (model, &iter,
		      COLUMN_APP_INFO, &info,
		      -1);
				  
  if (info == NULL)
    return;

  gtk_entry_set_text (GTK_ENTRY (self->priv->entry),
		      sure_string (g_app_info_get_executable (info)));
  gtk_label_set_text (GTK_LABEL (self->priv->desc_label),
		      sure_string (g_app_info_get_description (info)));
  gtk_widget_set_sensitive (self->priv->button, TRUE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
				     RESPONSE_REMOVE,
				     g_app_info_can_delete (info));

  if (self->priv->selected_app_info)
    g_object_unref (self->priv->selected_app_info);

  self->priv->selected_app_info = info;
}

static void
program_list_selection_activated (GtkTreeView *view,
				  GtkTreePath *path,
				  GtkTreeViewColumn *column,
				  gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;
  GtkTreeSelection *selection;

  /* update the entry with the info from the selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->program_list));	
  program_list_selection_changed (selection, self);

  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
}

static void
expander_toggled (GtkWidget *expander,
		  gpointer user_data)
{
  GtkOpenWithDialog *self = user_data;

  if (gtk_expander_get_expanded (GTK_EXPANDER (expander)) == TRUE)
    {
      gtk_widget_grab_focus (self->priv->entry);
      gtk_window_resize (GTK_WINDOW (self), 400, 1);
    }
  else
    {
      GtkTreeSelection *selection;

      gtk_widget_grab_focus (self->priv->program_list);
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->program_list));
      program_list_selection_changed (selection, self);
    }
}

static char *
get_extension (const char *basename)
{
  char *p;

  p = strrchr (basename, '.');

  if (p && *(p + 1) != '\0')
    return g_strdup (p + 1);

  return NULL;
}

static void
set_dialog_properties (GtkOpenWithDialog *self)
{
  char *label, *emname, *name, *extension, *description, *checkbox_text;
  GFileInfo *info;

  name = NULL;
  extension = NULL;
  label = NULL;
  emname = NULL;
  description = NULL;

  if (self->priv->gfile != NULL)
    {
      name = g_file_get_basename (self->priv->gfile);
      emname = g_strdup_printf ("<i>%s</i>", name);
      extension = get_extension (name);
      info = g_file_query_info (self->priv->gfile,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				0, NULL, NULL);
      self->priv->content_type = g_strdup (g_file_info_get_content_type (info));

      g_object_unref (info);
    }

  description = g_content_type_get_description (self->priv->content_type);

  if (self->priv->mode == GTK_OPEN_WITH_DIALOG_MODE_OPEN_FILE)
    {
      /* we have the GFile, its content type and its name and extension. */
      gtk_window_set_title (GTK_WINDOW (self), _("Open With"));

      /* Translators: %s is a filename */
      label = g_strdup_printf (_("Open %s with:"), emname);

      if (g_content_type_is_unknown (self->priv->content_type))
	/* Translators: the %s is the extension of the file */
	checkbox_text = g_strdup_printf (_("_Remember this application for %s documents"), 
					 extension);
      else
	/* Translators: %s is a file type description */
	checkbox_text = g_strdup_printf (_("_Remember this application for \"%s\" files"),
					 description);

      gtk_button_set_label (GTK_BUTTON (self->priv->checkbox), checkbox_text);
      g_free (checkbox_text);
    }
  else
    {
      if (g_content_type_is_unknown (self->priv->content_type))
	{
	  if (extension != NULL)
	    /* Translators: first %s is a filename and second %s is a file extension */
	    label = g_strdup_printf (_("Open %s and other %s document with:"),
				     emname, extension);
	  else;
	    /* TODO: content type is unknown and no file provided?? */
	}
      else
	{
	  if (name != NULL)
	    /* Translators: first %s is a filename, second is a description
	     * of the type, eg "plain text document" */
	    label = g_strdup_printf (_("Open %s and other \"%s\" files with:"), 
				     emname, description);
	  else
	    /* Translators: %s is a description of the file type,
	     * e.g. "plain text document" */
	    label = g_strdup_printf (_("Open all \"%s\" files with:"), description);
	}

      gtk_widget_hide (self->priv->checkbox);
      gtk_label_set_text_with_mnemonic (GTK_LABEL (self->priv->open_label),
					_("_Select"));
      gtk_window_set_title (GTK_WINDOW (self), _("Select default application"));
    }

  gtk_label_set_markup (GTK_LABEL (self->priv->label), label);

  g_free (label);
  g_free (name);
  g_free (extension);
  g_free (description);
  g_free (emname);
}

static void
gtk_open_with_dialog_constructed (GObject *object)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  g_assert (self->priv->content_type != NULL ||
	    self->priv->gfile != NULL);

  if (G_OBJECT_CLASS (gtk_open_with_dialog_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_open_with_dialog_parent_class)->constructed (object);

  set_dialog_properties (self);
}

static void
gtk_open_with_dialog_finalize (GObject *object)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  if (self->priv->add_items_idle_id)
    g_source_remove (self->priv->add_items_idle_id);

  if (self->priv->selected_app_info)
    g_object_unref (self->priv->selected_app_info);

  if (self->priv->gfile)
    g_object_unref (self->priv->gfile);

  g_free (self->priv->content_type);

  G_OBJECT_CLASS (gtk_open_with_dialog_parent_class)->finalize (object);
}

static void
gtk_open_with_dialog_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  switch (property_id)
    {
    case PROP_GFILE:
      self->priv->gfile = g_value_dup_object (value);
      break;
    case PROP_CONTENT_TYPE:
      self->priv->content_type = g_value_dup_string (value);
      break;
    case PROP_MODE:
      self->priv->mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_open_with_dialog_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
  GtkOpenWithDialog *self = GTK_OPEN_WITH_DIALOG (object);

  switch (property_id)
    {
    case PROP_GFILE:
      if (self->priv->gfile != NULL)
	g_value_set_object (value, self->priv->gfile);
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->priv->content_type);
      break;
    case PROP_MODE:
      g_value_set_enum (value, self->priv->mode);
      break;;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_open_with_dialog_class_init (GtkOpenWithDialogClass *klass)
{
  GObjectClass *gobject_class;
  GtkDialogClass *dialog_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gtk_open_with_dialog_finalize;
  gobject_class->set_property = gtk_open_with_dialog_set_property;
  gobject_class->get_property = gtk_open_with_dialog_get_property;
  gobject_class->constructed = gtk_open_with_dialog_constructed;

  dialog_class = GTK_DIALOG_CLASS (klass);
  dialog_class->response = gtk_open_with_dialog_response;

  properties[PROP_GFILE] =
    g_param_spec_object ("gfile",
			 P_("A GFile object"),
			 P_("The GFile for this dialog"),
			 G_TYPE_FILE,
			 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
			 G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTENT_TYPE] =
    g_param_spec_string ("content-type",
			 P_("A content type string"),
			 P_("The content type for this dialog"),
			 NULL,
			 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
			 G_PARAM_STATIC_STRINGS);
  properties[PROP_MODE] =
    g_param_spec_enum ("mode",
		       P_("The dialog mode"),
		       P_("The operation mode for this dialog"),
		       GTK_TYPE_OPEN_WITH_DIALOG_MODE,
		       GTK_OPEN_WITH_DIALOG_MODE_OPEN_FILE,
		       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES,
				     properties);

  signals[APPLICATION_SELECTED] =
    g_signal_new ("application_selected",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkOpenWithDialogClass,
				   application_selected),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1, G_TYPE_APP_INFO);

  g_type_class_add_private (klass, sizeof (GtkOpenWithDialogPrivate));
}

static void
gtk_open_with_dialog_init (GtkOpenWithDialog *self)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *scrolled_window;
  GtkWidget *expander;
  GtkTreeSelection *selection;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_OPEN_WITH_DIALOG,
					    GtkOpenWithDialogPrivate);

  gtk_container_set_border_width (GTK_CONTAINER (self), 5);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

  vbox2 = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), vbox2, TRUE, TRUE, 0);

  self->priv->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (self->priv->label), 0.0, 0.5);
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->label,
		      FALSE, FALSE, 0);
  gtk_widget_show (self->priv->label);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolled_window, 400, 300);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  self->priv->program_list = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (self->priv->program_list),
				     FALSE);
  gtk_container_add (GTK_CONTAINER (scrolled_window), self->priv->program_list);
  gtk_box_pack_start (GTK_BOX (vbox2), scrolled_window, TRUE, TRUE, 0);

  self->priv->desc_label = gtk_label_new (_("Select an application to view its description."));
  gtk_misc_set_alignment (GTK_MISC (self->priv->desc_label), 0.0, 0.5);
  gtk_label_set_justify (GTK_LABEL (self->priv->desc_label), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->desc_label), TRUE);
  gtk_label_set_single_line_mode (GTK_LABEL (self->priv->desc_label), FALSE);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->desc_label, FALSE, FALSE, 0);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->program_list));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function (selection, gtk_open_with_selection_func,
					  self, NULL);
  g_signal_connect (selection, "changed",
		    G_CALLBACK (program_list_selection_changed),
		    self);
  g_signal_connect (self->priv->program_list, "row-activated",
		    G_CALLBACK (program_list_selection_activated),
		    self);

  self->priv->add_items_idle_id =
    g_idle_add (gtk_open_with_dialog_add_items_idle, self);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), vbox, TRUE, TRUE, 0);
  gtk_widget_show_all (vbox);

  expander = gtk_expander_new_with_mnemonic (_("_Use a custom command"));
  gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, FALSE, 0);
  g_signal_connect_after (expander, "activate", G_CALLBACK (expander_toggled), self);
  gtk_widget_show (expander);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (expander), hbox);
  gtk_widget_show (hbox);

  self->priv->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (self->priv->entry), TRUE);

  gtk_box_pack_start (GTK_BOX (hbox), self->priv->entry,
		      TRUE, TRUE, 0);
  gtk_widget_show (self->priv->entry);

  button = gtk_button_new_with_mnemonic (_("_Browse..."));
  g_signal_connect (button, "clicked",
		    G_CALLBACK (browse_clicked_cb), self);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* Add remember this application checkbox - only visible in open mode */
  self->priv->checkbox = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->checkbox), TRUE);
  gtk_button_set_use_underline (GTK_BUTTON (self->priv->checkbox), TRUE);
  gtk_widget_show (GTK_WIDGET (self->priv->checkbox));
  gtk_box_pack_start (GTK_BOX (vbox), self->priv->checkbox, FALSE, FALSE, 0);

  gtk_dialog_add_button (GTK_DIALOG (self),
			 GTK_STOCK_REMOVE,
			 RESPONSE_REMOVE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
				     RESPONSE_REMOVE,
				     FALSE);

  gtk_dialog_add_button (GTK_DIALOG (self),
			 GTK_STOCK_CANCEL,
			 GTK_RESPONSE_CANCEL);

  /* Create a custom stock icon */
  self->priv->button = gtk_button_new ();

  /* Hook up the entry to the button */
  gtk_widget_set_sensitive (self->priv->button, FALSE);
  g_signal_connect (self->priv->entry, "changed",
		    G_CALLBACK (entry_changed_cb), self);

  label = gtk_label_new_with_mnemonic (_("_Open"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (self->priv->button));
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_show (label);
  self->priv->open_label = label;

  gtk_container_add (GTK_CONTAINER (self->priv->button),
		     self->priv->open_label);

  gtk_widget_show (self->priv->button);
  gtk_widget_set_can_default (self->priv->button, TRUE);

  gtk_dialog_add_action_widget (GTK_DIALOG (self),
				self->priv->button, GTK_RESPONSE_OK);

  gtk_dialog_set_default_response (GTK_DIALOG (self),
				   GTK_RESPONSE_OK);
}

static void
set_parent_and_flags (GtkWidget *dialog,
		      GtkWindow *parent,
		      GtkDialogFlags flags)
{
  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  parent);
  
  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
}

GtkWidget *
gtk_open_with_dialog_new (GtkWindow *parent,
			  GtkDialogFlags flags,
			  GtkOpenWithDialogMode mode,
			  GFile *file)
{
  GtkWidget *retval;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  retval = g_object_new (GTK_TYPE_OPEN_WITH_DIALOG,
			 "gfile", file,
			 "mode", mode,
			 NULL);

  set_parent_and_flags (retval, parent, flags);

  return retval;
}

GtkWidget *
gtk_open_with_dialog_new_for_content_type (GtkWindow *parent,
					   GtkDialogFlags flags,
					   const gchar *content_type)
{
  GtkWidget *retval;

  g_return_val_if_fail (content_type != NULL, NULL);

  retval = g_object_new (GTK_TYPE_OPEN_WITH_DIALOG,
			 "content-type", content_type,
			 "mode", GTK_OPEN_WITH_DIALOG_MODE_SELECT_DEFAULT,
			 NULL);

  set_parent_and_flags (retval, parent, flags);

  return retval;
}
