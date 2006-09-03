/* GTK - The GIMP Toolkit
 * autotestfilechooser.c: Automated unit tests for the GtkFileChooser widget
 * Copyright (C) 2005, Novell, Inc.
 *
 * Authors:
 *   Federico Mena-Quintero <federico@novell.com>
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

/* TODO:
 *
 * - In test_reload_sequence(), test that the selection is preserved properly
 *   between unmap/map.
 *
 * - More tests!
 */

#define GTK_FILE_SYSTEM_ENABLE_UNSUPPORTED

#include <config.h>
#include <string.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include "gtk/gtkfilechooserprivate.h"
#include "gtk/gtkfilechooserdefault.h"
#include "gtk/gtkfilechooserentry.h"

static void
log_test (gboolean passed, const char *test_name, ...)
{
  va_list args;
  char *str;

  va_start (args, test_name);
  str = g_strdup_vprintf (test_name, args);
  va_end (args);

  g_printf ("%s: %s\n", passed ? "PASSED" : "FAILED", str);
  g_free (str);
}

typedef void (* SetFilenameFn) (GtkFileChooser *chooser, gpointer data);
typedef gboolean (* CompareFilenameFn) (GtkFileChooser *chooser, gpointer data);

struct test_set_filename_closure {
  GtkWidget *chooser;
  GtkWidget *accept_button;
  gboolean focus_button;
};

static gboolean
set_filename_timeout_cb (gpointer data)
{
  struct test_set_filename_closure *closure;

  closure = data;

  if (closure->focus_button)
    gtk_widget_grab_focus (closure->accept_button);

  gtk_button_clicked (GTK_BUTTON (closure->accept_button));

  return FALSE;
}


static guint wait_for_idle_id = 0;

static gboolean
wait_for_idle_idle (gpointer data)
{
  wait_for_idle_id = 0;

  return FALSE;
}

static void
wait_for_idle (void)
{
  wait_for_idle_id = g_idle_add_full (G_PRIORITY_LOW + 100,
				      wait_for_idle_idle,
				      NULL, NULL);

  while (wait_for_idle_id)
    gtk_main_iteration ();
}

static gboolean
test_set_filename (GtkFileChooserAction action,
		   gboolean focus_button,
		   SetFilenameFn set_filename_fn,const
		   CompareFilenameFn compare_filename_fn,
		   gpointer data)
{
  GtkWidget *chooser;
  struct test_set_filename_closure closure;
  gboolean retval;

  chooser = gtk_file_chooser_dialog_new ("hello", NULL, action,
					 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					 NULL);

  closure.chooser = chooser;
  closure.accept_button = gtk_dialog_add_button (GTK_DIALOG (chooser), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
  closure.focus_button = focus_button;

  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);

  (* set_filename_fn) (GTK_FILE_CHOOSER (chooser), data);

  g_timeout_add (2000, set_filename_timeout_cb, &closure);
  gtk_dialog_run (GTK_DIALOG (chooser));

  retval = (* compare_filename_fn) (GTK_FILE_CHOOSER (chooser), data);

  gtk_widget_destroy (chooser);

  return retval;
}

static void
set_filename_cb (GtkFileChooser *chooser, gpointer data)
{
  const char *filename;

  filename = data;
  gtk_file_chooser_set_filename (chooser, filename);
}

static gboolean
compare_filename_cb (GtkFileChooser *chooser, gpointer data)
{
  const char *filename;
  char *out_filename;
  gboolean retval;

  filename = data;
  out_filename = gtk_file_chooser_get_filename (chooser);

  if (out_filename)
    {
      retval = (strcmp (out_filename, filename) == 0);
      g_free (out_filename);
    } else
      retval = FALSE;

  return retval;
}

static gboolean
test_black_box_set_filename (GtkFileChooserAction action, const char *filename, gboolean focus_button)
{
  gboolean passed;

  passed = test_set_filename (action, focus_button, set_filename_cb, compare_filename_cb, (char *) filename);

  log_test (passed, "set_filename: action %d, focus_button=%s",
	    (int) action,
	    focus_button ? "TRUE" : "FALSE");

  return passed;

}

struct current_name_closure {
	const char *path;
	const char *current_name;
};

static void
set_current_name_cb (GtkFileChooser *chooser, gpointer data)
{
  struct current_name_closure *closure;

  closure = data;

  gtk_file_chooser_set_current_folder (chooser, closure->path);
  gtk_file_chooser_set_current_name (chooser, closure->current_name);
}

static gboolean
compare_current_name_cb (GtkFileChooser *chooser, gpointer data)
{
  struct current_name_closure *closure;
  char *out_filename;
  gboolean retval;

  closure = data;

  out_filename = gtk_file_chooser_get_filename (chooser);

  if (out_filename)
    {
      char *filename;

      filename = g_build_filename (closure->path, closure->current_name, NULL);
      retval = (strcmp (filename, out_filename) == 0);
      g_free (filename);
      g_free (out_filename);
    } else
      retval = FALSE;

  return retval;
}

static gboolean
test_black_box_set_current_name (const char *path, const char *current_name, gboolean focus_button)
{
  struct current_name_closure closure;
  gboolean passed;

  closure.path = path;
  closure.current_name = current_name;

  passed = test_set_filename (GTK_FILE_CHOOSER_ACTION_SAVE, focus_button,
			      set_current_name_cb, compare_current_name_cb, &closure);

  log_test (passed, "set_current_name, focus_button=%s", focus_button ? "TRUE" : "FALSE");

  return passed;
}

/* FIXME: fails in CREATE_FOLDER mode when FOLDER_NAME == "/" */

#if 0
#define FILE_NAME "/nonexistent"
#define FOLDER_NAME "/etc"
#else
#define FILE_NAME "/etc/passwd"
#define FOLDER_NAME "/etc"
#endif

#define CURRENT_NAME "parangaricutirimicuaro.txt"

/* https://bugzilla.novell.com/show_bug.cgi?id=184875
 * http://bugzilla.gnome.org/show_bug.cgi?id=347066
 */
static gboolean
test_black_box (void)
{
  gboolean passed;
  char *cwd;

  passed = TRUE;

  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_OPEN, FILE_NAME, FALSE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_OPEN, FILE_NAME, TRUE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_SAVE, FILE_NAME, FALSE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_SAVE, FILE_NAME, TRUE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, FOLDER_NAME, FALSE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, FOLDER_NAME, TRUE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER, FOLDER_NAME, FALSE);
  passed = passed && test_black_box_set_filename (GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER, FOLDER_NAME, TRUE);

  cwd = g_get_current_dir ();

  passed = passed && test_black_box_set_current_name (cwd, CURRENT_NAME, FALSE);
  passed = passed && test_black_box_set_current_name (cwd, CURRENT_NAME, TRUE);

  g_free (cwd);

  log_test (passed, "Black box tests");

  return passed;
}

struct confirm_overwrite_closure {
  GtkWidget *chooser;
  GtkWidget *accept_button;
  gboolean emitted_confirm_overwrite_signal;
};

static GtkFileChooserConfirmation
confirm_overwrite_cb (GtkFileChooser *chooser, gpointer data)
{
  struct confirm_overwrite_closure *closure;

  closure = data;

  printf ("bling!\n");

  closure->emitted_confirm_overwrite_signal = TRUE;

  return GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;
}

static gboolean
confirm_overwrite_timeout_cb (gpointer data)
{
  struct confirm_overwrite_closure *closure;

  closure = data;
  gtk_button_clicked (GTK_BUTTON (closure->accept_button));

  return FALSE;
}

/* http://bugzilla.gnome.org/show_bug.cgi?id=347883 */
static gboolean
test_confirm_overwrite_for_path (const char *path)
{
  gboolean passed;
  struct confirm_overwrite_closure closure;
  char *filename;

  passed = TRUE;

  closure.emitted_confirm_overwrite_signal = FALSE;
  closure.chooser = gtk_file_chooser_dialog_new ("hello", NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
						 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						 NULL);
  closure.accept_button = gtk_dialog_add_button (GTK_DIALOG (closure.chooser), GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (closure.chooser), GTK_RESPONSE_ACCEPT);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (closure.chooser), TRUE);
  g_signal_connect (closure.chooser, "confirm-overwrite",
		    G_CALLBACK (confirm_overwrite_cb), &closure);

  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (closure.chooser), path);

  g_timeout_add (2000, confirm_overwrite_timeout_cb, &closure);
  gtk_dialog_run (GTK_DIALOG (closure.chooser));

  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (closure.chooser));
  passed = passed && filename && (strcmp (filename, path) == 0);
  g_free (filename);
  
  gtk_widget_destroy (closure.chooser);

  passed = passed && closure.emitted_confirm_overwrite_signal;

  log_test (passed, "Confirm overwrite");

  return passed;
}

static gboolean
test_confirm_overwrite (void)
{
  gboolean passed = TRUE;

  passed = passed && test_confirm_overwrite_for_path ("/etc/passwd"); /* a file we know will always exist */
  
  return passed;
}

static const GtkFileChooserAction open_actions[] = {
  GTK_FILE_CHOOSER_ACTION_OPEN,
  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
};

static const GtkFileChooserAction save_actions[] = {
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
};


static gboolean
has_action (const GtkFileChooserAction *actions,
	    int n_actions,
	    GtkFileChooserAction sought_action)
{
  int i;

  for (i = 0; i < n_actions; i++)
    if (actions[i] == sought_action)
      return TRUE;

  return FALSE;
}

static const char *
get_action_name (GtkFileChooserAction action)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_peek (GTK_TYPE_FILE_CHOOSER_ACTION);
  if (!enum_class)
    g_error ("BUG: get_action_name(): no GEnumClass for GTK_TYPE_FILE_CHOOSER_ACTION");

  enum_value = g_enum_get_value (enum_class, (int) action);
  if (!enum_value)
    g_error ("BUG: get_action_name(): no GEnumValue for GtkFileChooserAction %d", (int) action);

  return enum_value->value_name;
}

static GtkFileChooserDefault *
get_impl_from_dialog (GtkWidget *dialog)
{
  GtkFileChooserDialog *d;
  GtkFileChooserDialogPrivate *dialog_priv;
  GtkFileChooserWidget *chooser_widget;
  GtkFileChooserWidgetPrivate *widget_priv;
  GtkFileChooserDefault *impl;

  d = GTK_FILE_CHOOSER_DIALOG (dialog);
  dialog_priv = d->priv;
  chooser_widget = GTK_FILE_CHOOSER_WIDGET (dialog_priv->widget);
  if (!chooser_widget)
    g_error ("BUG: dialog_priv->widget is not a GtkFileChooserWidget");

  widget_priv = chooser_widget->priv;
  impl = (GtkFileChooserDefault *) (widget_priv->impl);
  if (!impl)
    g_error ("BUG: widget_priv->impl is not a GtkFileChooserDefault");

  return impl;
}

static gboolean
test_widgets_for_current_action (GtkFileChooserDialog *dialog,
				 GtkFileChooserAction  expected_action)
{
  GtkFileChooserDefault *impl;
  gboolean passed;

  if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog)) != expected_action)
    return FALSE;

  impl = get_impl_from_dialog (GTK_WIDGET (dialog));

  g_assert (impl->action == expected_action);

  passed = TRUE;

  /* OPEN implies that the "new folder" button is hidden; otherwise it is shown */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    passed = passed && !GTK_WIDGET_VISIBLE (impl->browse_new_folder_button);
  else
    passed = passed && GTK_WIDGET_VISIBLE (impl->browse_new_folder_button);

  /* Check that the widgets are present/visible or not */
  if (has_action (open_actions, G_N_ELEMENTS (open_actions), impl->action))
    {
      passed = passed && (impl->save_widgets == NULL
			  && (impl->location_mode == LOCATION_MODE_PATH_BAR
			      ? impl->location_entry == NULL
			      : impl->location_entry != NULL)
			  && impl->save_folder_label == NULL
			  && impl->save_folder_combo == NULL
			  && impl->save_expander == NULL
			  && GTK_IS_CONTAINER (impl->browse_widgets) && GTK_WIDGET_DRAWABLE (impl->browse_widgets));
    }
  else if (has_action (save_actions, G_N_ELEMENTS (save_actions), impl->action))
    {
      /* FIXME: we can't use GTK_IS_FILE_CHOOSER_ENTRY() because it uses
       * _gtk_file_chooser_entry_get_type(), which is a non-exported symbol.
       * So, we just test impl->location_entry for being non-NULL
       */
      passed = passed && (GTK_IS_CONTAINER (impl->save_widgets) && GTK_WIDGET_DRAWABLE (impl->save_widgets)
			  && impl->location_entry != NULL && GTK_WIDGET_DRAWABLE (impl->location_entry)
			  && GTK_IS_LABEL (impl->save_folder_label) && GTK_WIDGET_DRAWABLE (impl->save_folder_label)
			  && GTK_IS_COMBO_BOX (impl->save_folder_combo) && GTK_WIDGET_DRAWABLE (impl->save_folder_combo)
			  && GTK_IS_EXPANDER (impl->save_expander) && GTK_WIDGET_DRAWABLE (impl->save_expander)
			  && GTK_IS_CONTAINER (impl->browse_widgets));

      /* FIXME: we are in a SAVE mode; test the visibility and sensitivity of
       * the children that change depending on the state of the expander.
       */
    }
  else
    {
      g_error ("BAD TEST: test_widgets_for_current_action() doesn't know about %s", get_action_name (impl->action));
      passed = FALSE;
    }

  return passed;
}

typedef gboolean (* ForeachActionCallback) (GtkFileChooserDialog *dialog,
					    GtkFileChooserAction  action,
					    gpointer              user_data);

static gboolean
foreach_action (GtkFileChooserDialog *dialog,
		ForeachActionCallback callback,
		gpointer              user_data)
{
  GEnumClass *enum_class;
  int i;

  enum_class = g_type_class_peek (GTK_TYPE_FILE_CHOOSER_ACTION);
  if (!enum_class)
    g_error ("BUG: get_action_name(): no GEnumClass for GTK_TYPE_FILE_CHOOSER_ACTION");

  for (i = 0; i < enum_class->n_values; i++)
    {
      GEnumValue *enum_value;
      GtkFileChooserAction action;
      gboolean passed;

      enum_value = enum_class->values + i;
      action = enum_value->value;

      passed = (* callback) (dialog, action, user_data);
      if (!passed)
	return FALSE;
    }

  return TRUE;
}

struct action_closure {
  GtkFileChooserAction from_action;
};

static gboolean
switch_from_to_action_cb (GtkFileChooserDialog *dialog,
			  GtkFileChooserAction  action,
			  gpointer              user_data)
{
  struct action_closure *closure;
  gboolean passed;

  closure = user_data;

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), closure->from_action);

  passed = test_widgets_for_current_action (dialog, closure->from_action);
  log_test (passed, "switch_from_to_action_cb(): reset to action %s", get_action_name (closure->from_action));
  if (!passed)
    return FALSE;

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), action);

  passed = test_widgets_for_current_action (dialog, action);
  log_test (passed, "switch_from_to_action_cb(): transition from %s to %s",
	    get_action_name (closure->from_action),
	    get_action_name (action));
  return passed;
}

static gboolean
switch_from_action_cb (GtkFileChooserDialog *dialog,
		       GtkFileChooserAction  action,
		       gpointer              user_data)
{
  struct action_closure closure;

  closure.from_action = action;

  return foreach_action (dialog, switch_from_to_action_cb, &closure);
}

static gboolean
test_action_widgets (void)
{
  GtkWidget *dialog;
  GtkFileChooserAction action;
  gboolean passed;

  dialog = gtk_file_chooser_dialog_new ("Test file chooser",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK,
					GTK_RESPONSE_ACCEPT,
					NULL);
  gtk_widget_show_now (dialog);

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog));

  passed = test_widgets_for_current_action (GTK_FILE_CHOOSER_DIALOG (dialog), action);
  log_test (passed, "test_action_widgets(): widgets for initial action %s", get_action_name (action));
  if (!passed)
    return FALSE;

  passed = foreach_action (GTK_FILE_CHOOSER_DIALOG (dialog), switch_from_action_cb, NULL);
  log_test (passed, "test_action_widgets(): all transitions through property change");

  gtk_widget_destroy (dialog);

  return passed;
}

static gboolean
test_reload_sequence (gboolean set_folder_before_map)
{
  GtkWidget *dialog;
  GtkFileChooserDefault *impl;
  gboolean passed;
  char *folder;
  char *current_working_dir;

  passed = TRUE;

  current_working_dir = g_get_current_dir ();

  dialog = gtk_file_chooser_dialog_new ("Test file chooser",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK,
					GTK_RESPONSE_ACCEPT,
					NULL);
  impl = get_impl_from_dialog (dialog);

  if (set_folder_before_map)
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_home_dir ());

      wait_for_idle ();

      passed = passed && (impl->current_folder != NULL
			  && impl->browse_files_model != NULL
			  && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
			  && impl->reload_state == RELOAD_HAS_FOLDER
			  && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
			  && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
			      ? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
			      : TRUE));

      wait_for_idle ();

      folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
      passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);
      g_free (folder);
    }
  else
    {
      /* Initially, no folder is not loaded or pending */
      passed = passed && (impl->current_folder == NULL
			  && impl->sort_model == NULL
			  && impl->browse_files_model == NULL
			  && impl->load_state == LOAD_EMPTY
			  && impl->reload_state == RELOAD_EMPTY
			  && impl->load_timeout_id == 0);

      wait_for_idle ();

      folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
      passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);
    }

  log_test (passed, "test_reload_sequence(): initial status");

  /* After mapping, it is loading some folder, either the one that was explicitly set or the default one */

  gtk_widget_show_now (dialog);

  wait_for_idle ();

  passed = passed && (impl->current_folder != NULL
		      && impl->browse_files_model != NULL
		      && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		      && impl->reload_state == RELOAD_HAS_FOLDER
		      && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
		      && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
			  ? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
			  : TRUE));

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  if (set_folder_before_map)
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);

  g_free (folder);

  log_test (passed, "test_reload_sequence(): status after map");

  /* Unmap it; we should still have a folder */

  gtk_widget_hide (dialog);

  wait_for_idle ();

  passed = passed && (impl->current_folder != NULL
		      && impl->browse_files_model != NULL
		      && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		      && impl->reload_state == RELOAD_WAS_UNMAPPED
		      && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
		      && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
			  ? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
			  : TRUE));

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  if (set_folder_before_map)
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);

  g_free (folder);

  log_test (passed, "test_reload_sequence(): status after unmap");

  /* Map it again! */

  gtk_widget_show_now (dialog);

  wait_for_idle ();

  passed = passed && (impl->current_folder != NULL
		      && impl->browse_files_model != NULL
		      && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		      && impl->reload_state == RELOAD_HAS_FOLDER
		      && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
		      && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
			  ? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
			  : TRUE));

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  if (set_folder_before_map)
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);

  g_free (folder);

  log_test (passed, "test_reload_sequence(): status after re-map");

  gtk_widget_destroy (dialog);
  g_free (current_working_dir);

  return passed;
}

static gboolean
test_reload (void)
{
  gboolean passed;

  passed = test_reload_sequence (FALSE);
  log_test (passed, "test_reload(): create and use the default folder");
  if (!passed)
    return FALSE;

  passed = test_reload_sequence (TRUE);
  log_test (passed, "test_reload(): set a folder explicitly before mapping");

  return passed;
}

static gboolean
test_button_folder_states_for_action (GtkFileChooserAction action, gboolean use_dialog, gboolean set_folder_on_dialog)
{
  gboolean passed;
  GtkWidget *window;
  GtkWidget *button;
  char *folder;
  GtkWidget *dialog;
  char *current_working_dir;
  gboolean must_have_cwd;

  passed = TRUE;

  current_working_dir = g_get_current_dir ();
  must_have_cwd = !(use_dialog && set_folder_on_dialog);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  if (use_dialog)
    {
      dialog = gtk_file_chooser_dialog_new ("Test", NULL, action,
					    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					    NULL);
      button = gtk_file_chooser_button_new_with_dialog (dialog);

      if (set_folder_on_dialog)
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_home_dir ());
    }
  else
    {
      button = gtk_file_chooser_button_new ("Test", action);
      dialog = NULL; /* keep gcc happy */
    }

  gtk_container_add (GTK_CONTAINER (window), button);

  /* Pre-map; no folder is set */
  wait_for_idle ();

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (button));
  if (must_have_cwd)
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);

  log_test (passed, "test_button_folder_states_for_action(): %s, use_dialog=%d, set_folder_on_dialog=%d, pre-map, %s",
	    get_action_name (action),
	    use_dialog,
	    set_folder_on_dialog,
	    must_have_cwd ? "must have $cwd" : "must have explicit folder");

  /* Map; folder should be set */

  gtk_widget_show_all (window);
  gtk_widget_show_now (window);

  wait_for_idle ();

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (button));

  if (must_have_cwd)
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);

  log_test (passed, "test_button_folder_states_for_action(): %s, use_dialog=%d, set_folder_on_dialog=%d, mapped, %s",
	    get_action_name (action),
	    use_dialog,
	    set_folder_on_dialog,
	    must_have_cwd ? "must have $cwd" : "must have explicit folder");
  g_free (folder);

  /* Unmap; folder should be set */

  gtk_widget_hide (window);
  wait_for_idle ();
  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (button));

  if (must_have_cwd)
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);

  log_test (passed, "test_button_folder_states_for_action(): %s, use_dialog=%d, set_folder_on_dialog=%d, unmapped, %s",
	    get_action_name (action),
	    use_dialog,
	    set_folder_on_dialog,
	    must_have_cwd ? "must have $cwd" : "must have explicit folder");
  g_free (folder);

  /* Re-map; folder should be set */

  gtk_widget_show_now (window);
  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (button));

  if (must_have_cwd)
    passed = passed && (folder != NULL && strcmp (folder, current_working_dir) == 0);
  else
    passed = passed && (folder != NULL && strcmp (folder, g_get_home_dir()) == 0);
  wait_for_idle ();
  log_test (passed, "test_button_folder_states_for_action(): %s, use_dialog=%d, set_folder_on_dialog=%d, re-mapped, %s",
	    get_action_name (action),
	    use_dialog,
	    set_folder_on_dialog,
	    must_have_cwd ? "must have $cwd" : "must have explicit folder");
  g_free (folder);

  g_free (current_working_dir);

  gtk_widget_destroy (window);

  return passed;
}

static gboolean
test_button_folder_states (void)
{
  /* GtkFileChooserButton only supports OPEN and SELECT_FOLDER */
  static const GtkFileChooserAction actions_to_test[] = {
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
  };
  gboolean passed;
  int i;

  passed = TRUE;

  for (i = 0; i < G_N_ELEMENTS (actions_to_test); i++)
    {
      passed = passed && test_button_folder_states_for_action (actions_to_test[i], FALSE, FALSE);
      passed = passed && test_button_folder_states_for_action (actions_to_test[i], TRUE, FALSE);
      passed = passed && test_button_folder_states_for_action (actions_to_test[i], TRUE, TRUE);
      log_test (passed, "test_button_folder_states(): action %s", get_action_name (actions_to_test[i]));
    }

  log_test (passed, "test_button_folder_states(): all supported actions");
  return passed;
}

static gboolean
sleep_timeout_cb (gpointer data)
{
  gtk_main_quit ();
  return FALSE;
}

static void
sleep_in_main_loop (int milliseconds)
{
  g_timeout_add (milliseconds, sleep_timeout_cb, NULL);
  gtk_main ();
}

static gboolean
test_folder_switch_and_filters (void)
{
  gboolean passed;
  char *cwd;
  char *base_dir;
  GtkFilePath *cwd_path;
  GtkFilePath *base_dir_path;
  GtkWidget *dialog;
  GtkFileFilter *all_filter;
  GtkFileFilter *txt_filter;
  GtkFileChooserDefault *impl;

  passed = TRUE;

  cwd = g_get_current_dir ();
  base_dir = g_build_filename (cwd, "file-chooser-test-dir", NULL);

  dialog = gtk_file_chooser_dialog_new ("Test", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					NULL);
  impl = get_impl_from_dialog (dialog);

  cwd_path = gtk_file_system_filename_to_path (impl->file_system, cwd);
  base_dir_path = gtk_file_system_filename_to_path (impl->file_system, base_dir);

  passed = passed && gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), base_dir);
  if (!passed)
    goto out;

  /* All files filter */

  all_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, "All files");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  /* *.txt filter */

  txt_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, "*.txt");
  gtk_file_filter_add_pattern (txt_filter, "*.txt");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), txt_filter);

  /* Test filter set */

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), all_filter);
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == all_filter);

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), txt_filter);
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == txt_filter);

  log_test (passed, "test_folder_switch_and_filters(): set and get filter");

  gtk_widget_show (dialog);

  /* Test that filter is unchanged when we switch folders */

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), cwd);
  sleep_in_main_loop (1000);
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == txt_filter);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), base_dir);
  sleep_in_main_loop (500);

  g_signal_emit_by_name (impl->browse_path_bar, "path-clicked",
			 (GtkFilePath *) cwd_path,
			 (GtkFilePath *) base_dir_path,
			 FALSE);
  sleep_in_main_loop (500);
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == txt_filter);

  log_test (passed, "test_folder_switch_and_filters(): filter after changing folder");

 out:
  g_free (cwd);
  g_free (base_dir);
  gtk_file_path_free (cwd_path);
  gtk_file_path_free (base_dir_path);

  gtk_widget_destroy (dialog);

  log_test (passed, "test_folder_switch_and_filters(): all filter tests");
  return passed;
}

static GLogFunc default_log_handler;
static int num_warnings;
static int num_errors;
static int num_critical_errors;

static void
log_override_cb (const gchar   *log_domain,
		 GLogLevelFlags log_level,
		 const gchar   *message,
		 gpointer       user_data)
{
  if (log_level & G_LOG_LEVEL_WARNING)
    num_warnings++;

  if (log_level & G_LOG_LEVEL_ERROR)
    num_errors++;

  if (log_level & G_LOG_LEVEL_CRITICAL)
    num_critical_errors++;

  (* default_log_handler) (log_domain, log_level, message, user_data);
}

int
main (int argc, char **argv)
{
  gboolean passed;
  gboolean zero_warnings;
  gboolean zero_errors;
  gboolean zero_critical_errors;

  default_log_handler = g_log_set_default_handler (log_override_cb, NULL);
  passed = TRUE;

  gtk_init (&argc, &argv);

  /* Start tests */

  passed = passed && test_black_box ();
  passed = passed && test_confirm_overwrite ();
  passed = passed && test_action_widgets ();
  passed = passed && test_reload ();
  passed = passed && test_button_folder_states ();
  passed = passed && test_folder_switch_and_filters ();
  log_test (passed, "main(): main tests");

  /* Warnings and errors */

  zero_warnings = num_warnings == 0;
  zero_errors = num_errors == 0;
  zero_critical_errors = num_critical_errors == 0;

  log_test (zero_warnings, "main(): zero warnings (actual number %d)", num_warnings);
  log_test (zero_errors, "main(): zero errors (actual number %d)", num_errors);
  log_test (zero_critical_errors, "main(): zero critical errors (actual number %d)", num_critical_errors);

  /* Done */

  passed = passed && zero_warnings && zero_errors && zero_critical_errors;

  log_test (passed, "main(): ALL TESTS");

  return 0;
}
