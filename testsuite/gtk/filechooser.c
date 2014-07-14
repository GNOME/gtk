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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* TODO:
 *
 * - In test_reload_sequence(), test that the selection is preserved properly
 *   between unmap/map.
 *
 * - More tests!
 */

#define SLEEP_DURATION  100

#include "config.h"
#include <string.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include "gtk/gtkfilechooserdefault.h"
#include "gtk/gtkfilechooserentry.h"

#if 0
static const char *
get_action_name (GtkFileChooserAction action)
{
  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:          return "OPEN";
    case GTK_FILE_CHOOSER_ACTION_SAVE:          return "SAVE";
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER: return "SELECT_FOLDER";
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER: return "CREATE_FOLDER";

    default:
      g_assert_not_reached ();
      return NULL;
    }
}
#endif

#ifdef BROKEN_TESTS
static void
log_test (gboolean passed, const char *test_name, ...)
{
  va_list args;
  char *str;

  va_start (args, test_name);
  str = g_strdup_vprintf (test_name, args);
  va_end (args);

  if (g_test_verbose())
    g_printf ("%s: %s\n", passed ? "PASSED" : "FAILED", str);
  g_free (str);
}

typedef void (* SetFilenameFn) (GtkFileChooser *chooser, gpointer data);
typedef void (* CompareFilenameFn) (GtkFileChooser *chooser, gpointer data);

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
#endif


static guint wait_for_idle_id = 0;

static gboolean
wait_for_idle_idle (gpointer data)
{
  wait_for_idle_id = 0;

  return G_SOURCE_REMOVE;
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

#ifdef BROKEN_TESTS
static void
test_set_filename (GtkFileChooserAction action,
		   gboolean focus_button,
		   SetFilenameFn set_filename_fn,const
		   CompareFilenameFn compare_filename_fn,
		   gpointer data)
{
  GtkWidget *chooser;
  struct test_set_filename_closure closure;
  guint timeout_id;

  chooser = gtk_file_chooser_dialog_new ("hello", NULL, action,
					 _("_Cancel"), GTK_RESPONSE_CANCEL,
					 NULL);

  closure.chooser = chooser;
  closure.accept_button = gtk_dialog_add_button (GTK_DIALOG (chooser), _("_OK"), GTK_RESPONSE_ACCEPT);
  closure.focus_button = focus_button;

  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);

  (* set_filename_fn) (GTK_FILE_CHOOSER (chooser), data);

  timeout_id = gdk_threads_add_timeout_full (G_MAXINT, SLEEP_DURATION, set_filename_timeout_cb, &closure, NULL);
  gtk_dialog_run (GTK_DIALOG (chooser));
  g_source_remove (timeout_id);

  (* compare_filename_fn) (GTK_FILE_CHOOSER (chooser), data);

  gtk_widget_destroy (chooser);
}

static void
set_filename_cb (GtkFileChooser *chooser, gpointer data)
{
  const char *filename;

  filename = data;
  gtk_file_chooser_set_filename (chooser, filename);
}

static void
compare_filename_cb (GtkFileChooser *chooser, gpointer data)
{
  const char *filename;
  char *out_filename;

  filename = data;
  out_filename = gtk_file_chooser_get_filename (chooser);

  g_assert_cmpstr (out_filename, ==, filename);

  g_free (out_filename);
}

typedef struct
{
  const char *test_name;
  GtkFileChooserAction action;
  const char *filename;
  gboolean focus_button;
} TestSetFilenameSetup;

static void
test_black_box_set_filename (gconstpointer data)
{
  const TestSetFilenameSetup *setup = data;

  test_set_filename (setup->action, setup->focus_button, set_filename_cb, compare_filename_cb, (char *) setup->filename);
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

static void
compare_current_name_cb (GtkFileChooser *chooser, gpointer data)
{
  struct current_name_closure *closure;
  char *out_filename;
  char *filename;

  closure = data;

  out_filename = gtk_file_chooser_get_filename (chooser);

  g_assert (out_filename != NULL);

  filename = g_build_filename (closure->path, closure->current_name, NULL);
  g_assert_cmpstr (filename, ==, out_filename);

  g_free (filename);
  g_free (out_filename);
}

typedef struct
{
  const char *test_name;
  GtkFileChooserAction action;
  const char *current_name;
  gboolean focus_button;
} TestSetCurrentNameSetup;

static void
test_black_box_set_current_name (gconstpointer data)
{
  const TestSetCurrentNameSetup *setup = data;
  struct current_name_closure closure;
  char *cwd;

  cwd = g_get_current_dir ();

  closure.path = cwd;
  closure.current_name = setup->current_name;

  test_set_filename (setup->action, setup->focus_button, set_current_name_cb, compare_current_name_cb, &closure);

  g_free (cwd);
}
#endif

/* FIXME: fails in CREATE_FOLDER mode when FOLDER_NAME == "/" */

#if 0
#define FILE_NAME "/nonexistent"
#define FILE_NAME_2 "/nonexistent2"
#define FOLDER_NAME "/etc"
#define FOLDER_NAME_2 "/usr"
#else
#define FILE_NAME "/etc/passwd"
#define FILE_NAME_2 "/etc/group"
#define FOLDER_NAME "/etc"
#define FOLDER_NAME_2 "/usr"
#endif

#define CURRENT_NAME "parangaricutirimicuaro.txt"
#define CURRENT_NAME_FOLDER "parangaricutirimicuaro"

/* https://bugzilla.novell.com/show_bug.cgi?id=184875
 * http://bugzilla.gnome.org/show_bug.cgi?id=347066
 * http://bugzilla.gnome.org/show_bug.cgi?id=346058
 */

#ifdef BROKEN_TESTS
static void
setup_set_filename_tests (void)
{
  static TestSetFilenameSetup tests[] =
    {
      { "/GtkFileChooser/black_box/set_filename/open/no_focus",		 GTK_FILE_CHOOSER_ACTION_OPEN,		FILE_NAME,  FALSE },
      { "/GtkFileChooser/black_box/set_filename/open/focus",		 GTK_FILE_CHOOSER_ACTION_OPEN,		FILE_NAME,  TRUE  },
      { "/GtkFileChooser/black_box/set_filename/save/no_focus",		 GTK_FILE_CHOOSER_ACTION_SAVE,		FILE_NAME,  FALSE },
      { "/GtkFileChooser/black_box/set_filename/save/focus",		 GTK_FILE_CHOOSER_ACTION_SAVE,		FILE_NAME,  TRUE  },
      { "/GtkFileChooser/black_box/set_filename/select_folder/no_focus", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,	FOLDER_NAME,FALSE },
      { "/GtkFileChooser/black_box/set_filename/select_folder/focus",	 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,	FOLDER_NAME,TRUE  },
      { "/GtkFileChooser/black_box/set_filename/create_folder/no_focus", GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,	FOLDER_NAME,FALSE },
      { "/GtkFileChooser/black_box/set_filename/create_folder/focus",	 GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,	FOLDER_NAME,TRUE  },
    };
  int i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    g_test_add_data_func (tests[i].test_name, &tests[i], test_black_box_set_filename);
}

static void
setup_set_current_name_tests (void)
{
  static TestSetCurrentNameSetup tests[] =
    {
      { "/GtkFileChooser/black_box/set_current_name/save/no_focus",	     GTK_FILE_CHOOSER_ACTION_SAVE,	    CURRENT_NAME,	 FALSE },
      { "/GtkFileChooser/black_box/set_current_name/save/focus",	     GTK_FILE_CHOOSER_ACTION_SAVE,	    CURRENT_NAME,	 TRUE  },
      { "/GtkFileChooser/black_box/set_current_name/create_folder/no_focus", GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER, CURRENT_NAME_FOLDER, FALSE },
      { "/GtkFileChooser/black_box/set_current_name/create_folder/focus",    GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER, CURRENT_NAME_FOLDER, TRUE  },
    };
  int i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    g_test_add_data_func (tests[i].test_name, &tests[i], test_black_box_set_current_name);
}
#endif

typedef struct
{
  const char *shortname;
  GtkFileChooserAction action;
  const char *initial_current_folder;
  const char *initial_filename;
  gboolean open_dialog;
  enum {
    BUTTON,
    DIALOG
  } what_to_tweak;
  const char *tweak_current_folder;
  const char *tweak_filename;
  gint dialog_response;
  gboolean unselect_all;
  const char *final_current_folder;
  const char *final_filename;
} FileChooserButtonTest;

static char *
make_button_test_name (FileChooserButtonTest *t)
{
  return g_strdup_printf ("/GtkFileChooserButton/%s", t->shortname);
#if 0
  GString *s = g_string_new ("/GtkFileChooserButton");

  g_string_append_printf (s, "/%s/%s/%s/%s",
			  get_action_name (t->action),
			  t->initial_current_folder ? "set_initial_folder" : "no_default_folder",
			  t->initial_filename ? "set_initial_filename" : "no_initial_filename",
			  t->open_dialog ? "open_dialog" : "no_dialog");

  if (t->tweak_current_folder)
    g_string_append (s, "/tweak_current_folder");

  if (t->tweak_filename)
    g_string_append (s, "/tweak_filename");

  if (t->open_dialog)
    g_string_append_printf (s, "/%s",
			    t->dialog_response == GTK_RESPONSE_ACCEPT ? "accept" : "cancel");

  if (t->final_current_folder)
    g_string_append (s, "/final_current_folder");

  if (t->final_filename)
    g_string_append (s, "/final_filename");

  return g_string_free (s, FALSE);
#endif
}

static gboolean
sleep_timeout_cb (gpointer data)
{
  gtk_main_quit ();
  return FALSE;
}

static void
sleep_in_main_loop (void)
{
  guint timeout_id;

  timeout_id = gdk_threads_add_timeout_full (G_MAXINT, 250, sleep_timeout_cb, NULL, NULL);
  gtk_main ();
  g_source_remove (timeout_id);
}

static void
build_children_list (GtkWidget *widget, gpointer data)
{
  GList **list;

  list = data;
  *list = g_list_prepend (*list, widget);
}

static GtkWidget *
find_child_widget_with_atk_role (GtkWidget *widget, AtkRole role)
{
  AtkObject *accessible;
  AtkRole a_role;

  accessible = gtk_widget_get_accessible (widget);
  a_role = atk_object_get_role (accessible);

  if (a_role == role)
    return widget;
  else
    {
      GtkWidget *found_child;

      found_child = NULL;

      if (GTK_IS_CONTAINER (widget))
	{
	  GList *children;
	  GList *l;

	  children = NULL;
	  gtk_container_forall (GTK_CONTAINER (widget), build_children_list, &children);

	  l = children;

	  while (l && !found_child)
	    {
	      GtkWidget *child;

	      child = GTK_WIDGET (l->data);

	      found_child = find_child_widget_with_atk_role (child, role);

	      l = l->next;
	    }

	  g_list_free (children);
	}

      return found_child;
    }
}

static const char *
get_atk_name_for_filechooser_button (GtkFileChooserButton *button)
{
  GtkFileChooserAction action;
  GtkWidget *widget;
  AtkObject *accessible;

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (button));
  g_assert (action == GTK_FILE_CHOOSER_ACTION_OPEN || action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

  if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
    widget = find_child_widget_with_atk_role (GTK_WIDGET (button), ATK_ROLE_PUSH_BUTTON);
  else
    widget = find_child_widget_with_atk_role (GTK_WIDGET (button), ATK_ROLE_COMBO_BOX);

  accessible = gtk_widget_get_accessible (widget);
  return atk_object_get_name (accessible);
}

static void
check_that_basename_is_shown (GtkFileChooserButton *button, const char *expected_filename)
{
  GtkFileChooserAction action;
  const char *name_on_button;
  char *expected_basename;

  name_on_button = get_atk_name_for_filechooser_button (button);

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (button));
  g_assert (action == GTK_FILE_CHOOSER_ACTION_OPEN || action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

  if (expected_filename)
    expected_basename = g_path_get_basename (expected_filename);
  else
    expected_basename = NULL;

  if (expected_basename)
    g_assert_cmpstr (expected_basename, ==, name_on_button);
  else
    g_assert_cmpstr (name_on_button, ==, "(None)"); /* see gtkfilechooserbutton.c:FALLBACK_DISPLAY_NAME */ /* FIXME: how do we translate this? */

  g_free (expected_basename);
}

static const char *
get_expected_shown_filename (GtkFileChooserAction action, const char *folder_name, const char *filename)
{
  if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (filename)
	return filename;
      else
	return folder_name;
    }
  else
    return filename;
}

static GtkWidget *
get_file_chooser_dialog_from_button (GtkFileChooserButton *button)
{
  GtkWidget *fc_dialog;

  /* Give me the internal dialog, damnit */
  fc_dialog = g_object_get_qdata (G_OBJECT (button), g_quark_from_static_string ("gtk-file-chooser-delegate"));
  g_assert (GTK_IS_FILE_CHOOSER (fc_dialog));
  g_assert (GTK_IS_DIALOG (fc_dialog));

  return fc_dialog;
}

typedef struct {
  GtkWidget *window;
  GtkWidget *fc_button;
} WindowAndButton;

static WindowAndButton
create_window_and_file_chooser_button (GtkFileChooserAction action)
{
  WindowAndButton w;

  w.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  w.fc_button = gtk_file_chooser_button_new (action == GTK_FILE_CHOOSER_ACTION_OPEN ? "Select a file" : "Select a folder",
					     action);
  gtk_container_add (GTK_CONTAINER (w.window), w.fc_button);

  return w;
}

typedef struct
{
  GObject *object;
  GHashTable *signals;
  gboolean in_main_loop;
} SignalWatcher;

typedef struct
{
  SignalWatcher *watcher;
  char *signal_name;
  gulong id;
  gboolean emitted;
} SignalConnection;

static SignalWatcher *
signal_watcher_new (GObject *object)
{
  SignalWatcher *watcher = g_new0 (SignalWatcher, 1);

  watcher->object = g_object_ref (object);
  watcher->signals = g_hash_table_new (g_str_hash, g_str_equal);

  return watcher;
}

static void
dummy_callback (GObject *object)
{
  /* nothing */
}

static void
marshal_notify_cb (gpointer data, GClosure *closure)
{
  if (data)
    {
      SignalConnection *conn;

      conn = data;
      conn->emitted = TRUE;

      if (conn->watcher->in_main_loop)
	{
	  gtk_main_quit ();
	  conn->watcher->in_main_loop = FALSE;
	}
    }
}

static void
signal_watcher_watch_signal (SignalWatcher *watcher, const char *signal_name)
{
  SignalConnection *conn;

  conn = g_hash_table_lookup (watcher->signals, signal_name);
  if (!conn)
    {
      GClosure *closure;

      conn = g_new0 (SignalConnection, 1);
      conn->watcher = watcher;
      conn->signal_name = g_strdup (signal_name);

      closure = g_cclosure_new (G_CALLBACK (dummy_callback), NULL, NULL);
      g_closure_add_marshal_guards (closure, conn, marshal_notify_cb, NULL, marshal_notify_cb);
      conn->id = g_signal_connect_closure (watcher->object, signal_name, closure, FALSE);
      conn->emitted = FALSE;

      g_hash_table_insert (watcher->signals, conn->signal_name, conn);
    }
  else
    conn->emitted = FALSE;
}

static gboolean
signal_watcher_expect (SignalWatcher *watcher, const char *signal_name, char *unused_description)
{
  SignalConnection *conn;
  gboolean emitted;

  conn = g_hash_table_lookup (watcher->signals, signal_name);
  g_assert (conn != NULL);

  if (!conn->emitted)
    {
      guint timeout_id;
      
      timeout_id = gdk_threads_add_timeout_full (G_MAXINT, 1000, sleep_timeout_cb, NULL, NULL);

      watcher->in_main_loop = TRUE;
      gtk_main ();
      watcher->in_main_loop = FALSE;

      g_source_remove (timeout_id);
    }

  emitted = conn->emitted;
  conn->emitted = FALSE;

  return emitted;
}

static void
destroy_connection (gpointer key, gpointer value, gpointer user_data)
{
  SignalConnection *conn;

  conn = value;
  g_signal_handler_disconnect (conn->watcher->object, conn->id);
  g_free (conn->signal_name);
  g_free (conn);
}

static void
signal_watcher_destroy (SignalWatcher *watcher)
{
  g_hash_table_foreach (watcher->signals, destroy_connection, NULL);
  g_hash_table_destroy (watcher->signals);
  g_object_unref (watcher->object);
  g_free (watcher);
}

static void
test_file_chooser_button_with_response (const FileChooserButtonTest *setup, gint dialog_response)
{
  WindowAndButton w;
  SignalWatcher *watcher;
  GtkWidget *fc_dialog = NULL;
  int iterations;
  int i;

  w = create_window_and_file_chooser_button (setup->action);

  watcher = signal_watcher_new (G_OBJECT (w.fc_button));
  signal_watcher_watch_signal (watcher, "current-folder-changed");
  signal_watcher_watch_signal (watcher, "selection-changed");

  if (setup->initial_current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (w.fc_button), setup->initial_current_folder);

  if (setup->initial_filename)
    gtk_file_chooser_select_filename (GTK_FILE_CHOOSER (w.fc_button), setup->initial_filename);

  gtk_widget_show_all (w.window);
  wait_for_idle ();

  if (setup->initial_current_folder)
    g_assert (signal_watcher_expect (watcher, "current-folder-changed", "initial current folder"));

  if (setup->initial_filename)
    g_assert (signal_watcher_expect (watcher, "selection-changed", "initial filename"));

  check_that_basename_is_shown (GTK_FILE_CHOOSER_BUTTON (w.fc_button),
				get_expected_shown_filename (setup->action, setup->initial_current_folder, setup->initial_filename));

  /* If there is a dialog to be opened, we actually test going through it a
   * couple of times.  This ensures that any state that the button frobs for
   * each appearance of the dialog will make sense.
   */
  if (setup->open_dialog)
    iterations = 2;
  else
    iterations = 1;

  for (i = 0; i < iterations; i++)
    {
      GtkFileChooser *chooser_to_tweak;

      if (setup->open_dialog)
	{
	  GList *children;

	  /* Hack our way into the file chooser button; get its GtkButton child and click it */
	  children = gtk_container_get_children (GTK_CONTAINER (w.fc_button));
	  g_assert (children && GTK_IS_BUTTON (children->data));
	  gtk_button_clicked (GTK_BUTTON (children->data));
	  g_list_free (children);

	  wait_for_idle ();

	  fc_dialog = get_file_chooser_dialog_from_button (GTK_FILE_CHOOSER_BUTTON (w.fc_button));
	}

      if (setup->what_to_tweak == BUTTON)
	chooser_to_tweak = GTK_FILE_CHOOSER (w.fc_button);
      else if (setup->what_to_tweak == DIALOG)
	chooser_to_tweak = GTK_FILE_CHOOSER (fc_dialog);
      else
	g_assert_not_reached ();

      /* Okay, now frob the button or its optional dialog */

      if (setup->tweak_current_folder)
	{
	  if (setup->what_to_tweak == BUTTON)
	    signal_watcher_watch_signal (watcher, "current-folder-changed");

	  gtk_file_chooser_set_current_folder (chooser_to_tweak, setup->tweak_current_folder);

	  if (setup->what_to_tweak == BUTTON)
	    g_assert (signal_watcher_expect (watcher, "current-folder-changed", "tweak current folder in button"));
	}

      if (setup->tweak_filename)
	{
	  if (setup->what_to_tweak == BUTTON)
	    signal_watcher_watch_signal (watcher, "selection-changed");

	  gtk_file_chooser_select_filename (chooser_to_tweak, setup->tweak_filename);

	  if (setup->what_to_tweak == BUTTON)
	    g_assert (signal_watcher_expect (watcher, "selection-changed", "tweak filename in button"));
	}

      if (setup->unselect_all)
	{
	  if (setup->what_to_tweak == BUTTON)
	    signal_watcher_watch_signal (watcher, "selection-changed");

	  gtk_file_chooser_unselect_all (chooser_to_tweak);

	  if (setup->what_to_tweak == BUTTON)
	    g_assert (signal_watcher_expect (watcher, "selection-changed", "tweak unselect_all in button"));
	}

      wait_for_idle ();

      if (setup->open_dialog)
	{
	  gtk_dialog_response (GTK_DIALOG (fc_dialog), dialog_response);
	  wait_for_idle ();

	  gtk_window_resize (GTK_WINDOW (fc_dialog), 500, 500);
	}

      if (setup->final_current_folder)
	{
	  char *folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (w.fc_button));

	  g_assert_cmpstr (folder, ==, setup->final_current_folder);
	  g_free (folder);
	}

      if (setup->final_filename)
	{
	  char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (w.fc_button));

	  g_assert_cmpstr (filename, ==, setup->final_filename);
	  g_free (filename);
	}

      check_that_basename_is_shown (GTK_FILE_CHOOSER_BUTTON (w.fc_button),
				    get_expected_shown_filename (setup->action, setup->final_current_folder, setup->final_filename));
    }

  signal_watcher_destroy (watcher);
  gtk_widget_destroy (w.window);
}

static void
test_file_chooser_button (gconstpointer data)
{
  const FileChooserButtonTest *setup = data;

  test_file_chooser_button_with_response (setup, setup->dialog_response);

  if (setup->open_dialog && setup->dialog_response == GTK_RESPONSE_CANCEL)
    {
      /* Runs the test again, with DELETE_EVENT (as if the user closed the
       * dialog instead of using the Cancel button), since the button misbehaved
       * in that case sometimes.
       */
      test_file_chooser_button_with_response (setup, GTK_RESPONSE_DELETE_EVENT);
    }
}

static int
find_accessible_action_num (AtkObject *object, const char *action_name)
{
  AtkAction *action_a;
  int num_actions;
  int i;

  action_a = ATK_ACTION (object);

  num_actions = atk_action_get_n_actions (action_a);

  for (i = 0; i < num_actions; i++)
    if (strcmp (atk_action_get_name (action_a, i), action_name) == 0)
      return i;

  return -1;
}

static void
do_accessible_action (AtkObject *object, const char *action_name)
{
  int action_num;

  action_num = find_accessible_action_num (object, action_name);
  g_assert (action_num != -1);

  atk_action_do_action (ATK_ACTION (object), action_num);
}

static void
test_file_chooser_button_combo_box_1 (void)
{
  WindowAndButton w;
  GtkWidget *combo_box;
  AtkObject *combo_box_a;
  AtkObject *menu_a;
  int num_items;
  int other_index;
  AtkObject *item_a;
  GtkWidget *fc_dialog;

  w = create_window_and_file_chooser_button (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

  gtk_file_chooser_select_filename (GTK_FILE_CHOOSER (w.fc_button), FOLDER_NAME);

  gtk_widget_show_all (w.window);

  /* Get the accessible for the combo box */

  combo_box = find_child_widget_with_atk_role (GTK_WIDGET (w.fc_button), ATK_ROLE_COMBO_BOX);
  combo_box_a = gtk_widget_get_accessible (combo_box);

  /* Press the combo box to bring up the menu */

  do_accessible_action (combo_box_a, "press");
  sleep_in_main_loop (); /* have to wait because bringing up the menu is asynchronous... */

  /* Get the menu from the combo box; it's the first child */

  menu_a = atk_object_ref_accessible_child (combo_box_a, 0);
  g_assert (atk_object_get_role (menu_a) == ATK_ROLE_MENU);

  /* Check that the last item in the menu is the "Other…" one */

  num_items = atk_object_get_n_accessible_children (menu_a);
  g_assert (num_items > 0);

  other_index = num_items - 1;

  item_a = atk_object_ref_accessible_child (menu_a, other_index);
  g_assert_cmpstr (atk_object_get_name (item_a), ==, "Other…");  /* FIXME: how do we translate this? */

  /* Activate the item */

  do_accessible_action (item_a, "click");

  /* Cancel the dialog */

  sleep_in_main_loop ();
  fc_dialog = get_file_chooser_dialog_from_button (GTK_FILE_CHOOSER_BUTTON (w.fc_button));

  gtk_dialog_response (GTK_DIALOG (fc_dialog), GTK_RESPONSE_CANCEL);

  /* Now check the selection in the combo box */
  check_that_basename_is_shown (GTK_FILE_CHOOSER_BUTTON (w.fc_button), FOLDER_NAME);

  gtk_widget_destroy (w.window);
}

static void
setup_file_chooser_button_combo_box_tests (void)
{
  g_test_add_func ("/GtkFileChooserButton/combo_box-1", test_file_chooser_button_combo_box_1);
}

static FileChooserButtonTest button_tests[] =
  {
    /* OPEN tests without dialog */

    {
      "open-1",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-2",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME, 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-3",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME,		/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-4",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME, 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME_2,		/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME_2		/* final_filename */
    },
    {
      "open-5",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-6",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME_2,		/* final_current_folder */
      NULL			/* final_filename */
    },

    /* SELECT_FOLDER tests without dialog */

    {
      "select-folder-1",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-2",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME, 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-3",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME, 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },
    {
      "select-folder-4",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-5",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-6",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },
    {
      "select-folder-7",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME_2,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-8",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,	 		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      0,			/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },

    /* OPEN tests with dialog, cancelled
     *
     * Test names are "open-dialog-cancel-A-B", where A and B can be:
     *
     *   A:
     *      ni - no initial filename
     *       i - initial filename
     *      nf - no initial folder
     *       f - initial folder
     *
     *   B:
     *      nt - no tweaks
     *       b - tweak button
     *       d - tweak dialog
     */

    {
      "open-dialog-cancel-ni-nt",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-ni-b",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-dialog-cancel-ni-d",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-i-nt",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-dialog-cancel-i-b",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME_2		/* final_filename */
    },
    {
      "open-dialog-cancel-i-d",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-dialog-cancel-nf-nt",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-nf-b",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-nf-d",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      FOLDER_NAME,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-f-nt",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-f-b",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME_2,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "open-dialog-cancel-f-d",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },

    /* SELECT_FOLDER tests with dialog, cancelled */

    {
      "select-folder-dialog-cancel-ni-nt",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-ni-b",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-dialog-cancel-ni-d",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-i-nt",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-dialog-cancel-i-b",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },
    {
      "select-folder-dialog-cancel-i-d",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-dialog-cancel-nf-nt",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL, 			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-nf-b",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-nf-d",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      FOLDER_NAME,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-f-nt",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-f-nt-2",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-dialog-cancel-f-b",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME_2,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-f-b-2",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },
    {
      "select-folder-dialog-cancel-f-d",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      FOLDER_NAME_2,		/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "select-folder-dialog-cancel-f-d-2",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      DIALOG,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_CANCEL,	/* dialog_response */
      FALSE,			/* unselect_all */
      FOLDER_NAME,		/* final_current_folder */
      NULL			/* final_filename */
    },

    /* OPEN tests with dialog */

    {
      "open-dialog-1",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME,		/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-dialog-2",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },
    {
      "open-dialog-3",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME_2		/* final_filename */
    },
    {
      "open-dialog-4",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME,		/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FILE_NAME			/* final_filename */
    },

    /* SELECT_FOLDER tests with dialog */

    {
      "select-folder-dialog-1",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-dialog-2",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME		/* final_filename */
    },
    {
      "select-folder-dialog-3",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },
    {
      "select-folder-dialog-4",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      FOLDER_NAME,		/* initial_current_folder */
      NULL,			/* initial_filename */
      TRUE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      GTK_RESPONSE_ACCEPT,	/* dialog_response */
      FALSE,			/* unselect_all */
      NULL,			/* final_current_folder */
      FOLDER_NAME_2		/* final_filename */
    },

    /* Unselection tests */
    {
      "unselect-all-1",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      TRUE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "unselect-all-2",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      TRUE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "unselect-all-3",
      GTK_FILE_CHOOSER_ACTION_OPEN,
      NULL,			/* initial_current_folder */
      FILE_NAME,		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FILE_NAME_2,		/* tweak_filename */
      0,			/* dialog_response */
      TRUE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "unselect-all-4",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      NULL,			/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      TRUE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "unselect-all-5",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      NULL,			/* tweak_filename */
      0,			/* dialog_response */
      TRUE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },
    {
      "unselect-all-6",
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      NULL,			/* initial_current_folder */
      FOLDER_NAME,		/* initial_filename */
      FALSE,			/* open_dialog */
      BUTTON,			/* what_to_tweak */
      NULL,			/* tweak_current_folder */
      FOLDER_NAME_2,		/* tweak_filename */
      0,			/* dialog_response */
      TRUE,			/* unselect_all */
      NULL,			/* final_current_folder */
      NULL			/* final_filename */
    },

  };

static void
setup_file_chooser_button_tests (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (button_tests); i++)
    {
      char *test_name;

      test_name = make_button_test_name (&button_tests[i]);
      g_test_add_data_func (test_name, &button_tests[i], test_file_chooser_button);
      g_free (test_name);
    }

  setup_file_chooser_button_combo_box_tests ();
}

#ifdef BROKEN_TESTS
struct confirm_overwrite_closure {
  GtkWidget *chooser;
  GtkWidget *accept_button;
  gint confirm_overwrite_signal_emitted;
  gchar *extension;
};

static GtkFileChooserConfirmation
confirm_overwrite_cb (GtkFileChooser *chooser, gpointer data)
{
  struct confirm_overwrite_closure *closure = data;

  if (g_test_verbose())
    printf ("bling!\n");
  closure->confirm_overwrite_signal_emitted += 1;

  return GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;
}

static void
overwrite_response_cb (GtkFileChooser *chooser, gint response, gpointer data)
{
  struct confirm_overwrite_closure *closure = data;
  char *filename;

  if (g_test_verbose())
    printf ("plong!\n");

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  filename = gtk_file_chooser_get_filename (chooser);

  if (!g_str_has_suffix (filename, closure->extension))
    {
      char *basename;

      basename = g_path_get_basename (filename);
      g_free (filename);

      filename = g_strconcat (basename, closure->extension, NULL);
      gtk_file_chooser_set_current_name (chooser, filename);

      g_signal_stop_emission_by_name (chooser, "response");
      gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
    }
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
test_confirm_overwrite_for_path (const char *path, gboolean append_extension)
{
  gboolean passed;
  struct confirm_overwrite_closure closure;
  char *filename;
  guint timeout_id;

  passed = TRUE;

  closure.extension = NULL;
  closure.confirm_overwrite_signal_emitted = 0;
  closure.chooser = gtk_file_chooser_dialog_new ("hello", NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
						 _("_Cancel"), GTK_RESPONSE_CANCEL,
						 NULL);
  closure.accept_button = gtk_dialog_add_button (GTK_DIALOG (closure.chooser),
                                                 _("_Save"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (closure.chooser), GTK_RESPONSE_ACCEPT);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (closure.chooser), TRUE);

  g_signal_connect (closure.chooser, "confirm-overwrite",
		    G_CALLBACK (confirm_overwrite_cb), &closure);

  if (append_extension)
    {
      char *extension;

      filename = g_path_get_dirname (path);
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (closure.chooser), filename);
      g_free (filename);

      filename = g_path_get_basename (path);
      extension = strchr (filename, '.');

      if (extension)
        {
          closure.extension = g_strdup (extension);
          *extension = '\0';
        }

      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (closure.chooser), filename);
      g_free (filename);

      g_signal_connect (closure.chooser, "response",
                        G_CALLBACK (overwrite_response_cb), &closure);
    }
  else
    {
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (closure.chooser), path);
    }

  timeout_id = gdk_threads_add_timeout_full (G_MAXINT, SLEEP_DURATION, confirm_overwrite_timeout_cb, &closure, NULL);
  gtk_dialog_run (GTK_DIALOG (closure.chooser));
  g_source_remove (timeout_id);

  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (closure.chooser));
  passed = passed && filename && (strcmp (filename, path) == 0);
  g_free (filename);

  gtk_widget_destroy (closure.chooser);

  passed = passed && (1 == closure.confirm_overwrite_signal_emitted);

  log_test (passed, "Confirm overwrite for %s", path);

  return passed;
}

static void
test_confirm_overwrite (void)
{
  gboolean passed = TRUE;

  /* first test for a file we know will always exist */
  passed = passed && test_confirm_overwrite_for_path ("/etc/passwd", FALSE);
  g_assert (passed);
  passed = passed && test_confirm_overwrite_for_path ("/etc/resolv.conf", TRUE);
  g_assert (passed);
}
#endif

static const GtkFileChooserAction open_actions[] = {
  GTK_FILE_CHOOSER_ACTION_OPEN,
  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
};

static const GtkFileChooserAction save_actions[] = {
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
};


#ifdef BROKEN_TESTS
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

static GtkFileChooserWidgetPrivate *
get_widget_priv_from_dialog (GtkWidget *dialog)
{
  GtkFileChooserDialog *d;
  GtkFileChooserDialogPrivate *dialog_priv;
  GtkFileChooserWidget *chooser_widget;
  GtkFileChooserWidgetPrivate *widget_priv;

  d = GTK_FILE_CHOOSER_DIALOG (dialog);
  dialog_priv = d->priv;
  chooser_widget = GTK_FILE_CHOOSER_WIDGET (dialog_priv->widget);
  if (!chooser_widget)
    g_error ("BUG: dialog_priv->widget is not a GtkFileChooserWidget");

  widget_priv = chooser_widget->priv;

  return widget_priv;
}

static gboolean
test_widgets_for_current_action (GtkFileChooserDialog *dialog,
				 GtkFileChooserAction  expected_action)
{
  GtkFileChooserWidgetPrivate *priv;
  gboolean passed;

  if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog)) != expected_action)
    return FALSE;

  priv = get_widget_priv_from_dialog (GTK_WIDGET (dialog));

  g_assert (priv->action == expected_action);

  passed = TRUE;

  /* OPEN implies that the "new folder" button is hidden; otherwise it is shown */
  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    passed = passed && !gtk_widget_get_visible (priv->browse_new_folder_button);
  else
    passed = passed && gtk_widget_get_visible (priv->browse_new_folder_button);

  /* Check that the widgets are present/visible or not */
  if (has_action (open_actions, G_N_ELEMENTS (open_actions), priv->action))
    {
      passed = passed && (priv->save_widgets == NULL
			  && (priv->location_mode == LOCATION_MODE_PATH_BAR
			      ? priv->location_entry == NULL
			      : priv->location_entry != NULL)
			  && priv->save_folder_label == NULL
			  && priv->save_folder_combo == NULL
			  && priv->save_expander == NULL
			  && GTK_IS_CONTAINER (priv->browse_widgets) && gtk_widget_is_drawable (priv->browse_widgets));
    }
  else if (has_action (save_actions, G_N_ELEMENTS (save_actions), priv->action))
    {
      /* FIXME: we can't use GTK_IS_FILE_CHOOSER_ENTRY() because it uses
       * _gtk_file_chooser_entry_get_type(), which is a non-exported symbol.
       * So, we just test priv->location_entry for being non-NULL
       */
      passed = passed && (GTK_IS_CONTAINER (priv->save_widgets) && gtk_widget_is_drawable (priv->save_widgets)
			  && priv->location_entry != NULL && gtk_widget_is_drawable (priv->location_entry)
			  && GTK_IS_LABEL (priv->save_folder_label) && gtk_widget_is_drawable (priv->save_folder_label)
			  && GTK_IS_COMBO_BOX (priv->save_folder_combo) && gtk_widget_is_drawable (priv->save_folder_combo)
			  && GTK_IS_EXPANDER (priv->save_expander) && gtk_widget_is_drawable (priv->save_expander)
			  && GTK_IS_CONTAINER (priv->browse_widgets));

      /* FIXME: we are in a SAVE mode; test the visibility and sensitivity of
       * the children that change depending on the state of the expander.
       */
    }
  else
    {
      g_error ("BAD TEST: test_widgets_for_current_action() doesn't know about %s", get_action_name (priv->action));
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

static void
test_action_widgets (void)
{
  GtkWidget *dialog;
  GtkFileChooserAction action;
  gboolean passed;

  dialog = gtk_file_chooser_dialog_new ("Test file chooser",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					_("_Cancel"),
					GTK_RESPONSE_CANCEL,
					_("_OK"),
					GTK_RESPONSE_ACCEPT,
					NULL);
  gtk_widget_show_now (dialog);

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog));

  passed = test_widgets_for_current_action (GTK_FILE_CHOOSER_DIALOG (dialog), action);
  log_test (passed, "test_action_widgets(): widgets for initial action %s", get_action_name (action));
  g_assert (passed);

  passed = foreach_action (GTK_FILE_CHOOSER_DIALOG (dialog), switch_from_action_cb, NULL);
  log_test (passed, "test_action_widgets(): all transitions through property change");
  g_assert (passed);

  gtk_widget_destroy (dialog);
}
#endif

#ifdef BROKEN_TESTS
static gboolean
test_reload_sequence (gboolean set_folder_before_map)
{
  GtkWidget *dialog;
  GtkFileChooserWidgetPrivate *priv;
  gboolean passed;
  char *folder;
  char *current_working_dir;

  passed = TRUE;

  current_working_dir = g_get_current_dir ();

  dialog = gtk_file_chooser_dialog_new ("Test file chooser",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					_("_Cancel"),
					GTK_RESPONSE_CANCEL,
					_("_OK"),
					GTK_RESPONSE_ACCEPT,
					NULL);
  priv = get_widget_priv_from_dialog (dialog);

  if (set_folder_before_map)
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_home_dir ());

      wait_for_idle ();

      passed = passed && (priv->current_folder != NULL
			  && priv->browse_files_model != NULL
			  && (priv->load_state == LOAD_PRELOAD || priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
			  && priv->reload_state == RELOAD_HAS_FOLDER
			  && (priv->load_state == LOAD_PRELOAD ? (priv->load_timeout_id != 0) : TRUE)
			  && ((priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
			      ? (priv->load_timeout_id == 0 && priv->sort_model != NULL)
			      : TRUE));

      wait_for_idle ();

      folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
      passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);
      g_free (folder);
    }
  else
    {
      /* Initially, no folder is not loaded or pending */
      passed = passed && (priv->current_folder == NULL
			  && priv->sort_model == NULL
			  && priv->browse_files_model == NULL
			  && priv->load_state == LOAD_EMPTY
			  && priv->reload_state == RELOAD_EMPTY
			  && priv->load_timeout_id == 0);

      wait_for_idle ();

      folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
      passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);
    }

  log_test (passed, "test_reload_sequence(): initial status");

  /* After mapping, it is loading some folder, either the one that was explicitly set or the default one */

  gtk_widget_show_now (dialog);

  wait_for_idle ();

  passed = passed && (priv->current_folder != NULL
		      && priv->browse_files_model != NULL
		      && (priv->load_state == LOAD_PRELOAD || priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
		      && priv->reload_state == RELOAD_HAS_FOLDER
		      && (priv->load_state == LOAD_PRELOAD ? (priv->load_timeout_id != 0) : TRUE)
		      && ((priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
			  ? (priv->load_timeout_id == 0 && priv->sort_model != NULL)
			  : TRUE));

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  if (set_folder_before_map)
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);
  else
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);

  g_free (folder);

  log_test (passed, "test_reload_sequence(): status after map");

  /* Unmap it; we should still have a folder */

  gtk_widget_hide (dialog);

  wait_for_idle ();

  passed = passed && (priv->current_folder != NULL
		      && priv->browse_files_model != NULL
		      && (priv->load_state == LOAD_PRELOAD || priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
		      && (priv->load_state == LOAD_PRELOAD ? (priv->load_timeout_id != 0) : TRUE)
		      && ((priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
			  ? (priv->load_timeout_id == 0 && priv->sort_model != NULL)
			  : TRUE));

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  if (set_folder_before_map)
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);
  else
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);

  g_free (folder);

  log_test (passed, "test_reload_sequence(): status after unmap");

  /* Map it again! */

  gtk_widget_show_now (dialog);

  wait_for_idle ();

  passed = passed && (priv->current_folder != NULL
		      && priv->browse_files_model != NULL
		      && (priv->load_state == LOAD_PRELOAD || priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
		      && priv->reload_state == RELOAD_HAS_FOLDER
		      && (priv->load_state == LOAD_PRELOAD ? (priv->load_timeout_id != 0) : TRUE)
		      && ((priv->load_state == LOAD_LOADING || priv->load_state == LOAD_FINISHED)
			  ? (priv->load_timeout_id == 0 && priv->sort_model != NULL)
			  : TRUE));

  folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  if (set_folder_before_map)
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);
  else
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);

  g_free (folder);

  log_test (passed, "test_reload_sequence(): status after re-map");

  gtk_widget_destroy (dialog);
  g_free (current_working_dir);

  return passed;
}

static void
test_reload (void)
{
  gboolean passed;

  passed = test_reload_sequence (FALSE);
  log_test (passed, "test_reload(): create and use the default folder");
  g_assert (passed);

  passed = test_reload_sequence (TRUE);
  log_test (passed, "test_reload(): set a folder explicitly before mapping");
  g_assert (passed);
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
					    _("_Cancel"), GTK_RESPONSE_CANCEL,
					    _("_OK"), GTK_RESPONSE_ACCEPT,
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
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);
  else
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);

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
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);
  else
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);

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
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);
  else
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);

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
    passed = passed && (g_strcmp0 (folder, current_working_dir) == 0);
  else
    passed = passed && (g_strcmp0 (folder, g_get_home_dir()) == 0);
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

static void
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
      g_assert (passed);
      passed = passed && test_button_folder_states_for_action (actions_to_test[i], TRUE, FALSE);
      g_assert (passed);
      passed = passed && test_button_folder_states_for_action (actions_to_test[i], TRUE, TRUE);
      g_assert (passed);
      log_test (passed, "test_button_folder_states(): action %s", get_action_name (actions_to_test[i]));
    }

  log_test (passed, "test_button_folder_states(): all supported actions");
}

static void
test_folder_switch_and_filters (void)
{
  gboolean passed;
  char *cwd;
  char *base_dir;
  GFile *cwd_file;
  GFile *base_dir_file;
  GtkWidget *dialog;
  GtkFileFilter *all_filter;
  GtkFileFilter *txt_filter;
  GtkFileChooserWidgetPrivate *priv;

  passed = TRUE;

  cwd = g_get_current_dir ();
  base_dir = g_build_filename (cwd, "file-chooser-test-dir", NULL);

  dialog = gtk_file_chooser_dialog_new ("Test", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
					_("_Cancel"), GTK_RESPONSE_CANCEL,
					_("_OK"), GTK_RESPONSE_ACCEPT,
					NULL);
  priv = get_widget_priv_from_dialog (dialog);

  cwd_file = g_file_new_for_path (cwd);
  base_dir_file = g_file_new_for_path (base_dir);

  passed = passed && gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), base_dir);
  g_assert (passed);

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
  g_assert (passed);

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), txt_filter);
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == txt_filter);
  log_test (passed, "test_folder_switch_and_filters(): set and get filter");
  g_assert (passed);

  gtk_widget_show (dialog);

  /* Test that filter is unchanged when we switch folders */

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), cwd);
  sleep_in_main_loop ();
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == txt_filter);
  g_assert (passed);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), base_dir);
  sleep_in_main_loop ();

  g_signal_emit_by_name (priv->browse_path_bar, "path-clicked",
			 cwd_file,
			 base_dir_file,
			 FALSE);
  sleep_in_main_loop ();
  passed = passed && (gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog)) == txt_filter);
  log_test (passed, "test_folder_switch_and_filters(): filter after changing folder");
  g_assert (passed);

  /* cleanups */
  g_free (cwd);
  g_free (base_dir);
  g_object_unref (cwd_file);
  g_object_unref (base_dir_file);

  gtk_widget_destroy (dialog);

  log_test (passed, "test_folder_switch_and_filters(): all filter tests");
}
#endif

int
main (int    argc,
      char **argv)
{
  /* initialize test program */
  gtk_test_init (&argc, &argv);

  /* Register tests */

  setup_file_chooser_button_tests ();
#ifdef BROKEN_TESTS
  setup_set_filename_tests ();
  setup_set_current_name_tests ();

  g_test_add_func ("/GtkFileChooser/confirm_overwrite", test_confirm_overwrite);
  g_test_add_func ("/GtkFileChooser/action_widgets", test_action_widgets);
  g_test_add_func ("/GtkFileChooser/reload", test_reload);
  g_test_add_func ("/GtkFileChooser/button_folder_states", test_button_folder_states);
  g_test_add_func ("/GtkFileChooser/folder_switch_and_filters", test_folder_switch_and_filters);
#endif

  /* run and check selected tests */
  return g_test_run();
}
