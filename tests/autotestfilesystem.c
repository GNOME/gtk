/* GTK - The GIMP Toolkit
 * autotestfilesystem.c: Automated tests for GtkFileSystem implementations
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

#define GTK_FILE_SYSTEM_ENABLE_UNSUPPORTED

#include "config.h"
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gtk/gtkfilesystem.h>

#ifdef G_OS_WIN32
#include <direct.h>
#define rmdir(d) _rmdir(d)
#endif

#define CALLBACK_TIMEOUT_MS 3000	/* Period after which the callback must have been called */
#define CANCEL_TIMEOUT_MS 100		/* We'll sleep for this much time before cancelling */

#define GET_FOLDER_FILENAME "/etc"
#define GET_INFO_FILENAME "/etc/passwd"
#define CREATE_FOLDER_FILENAME "/tmp/autotestfilesystem-tmp"
#define VOLUME_MOUNT_FILENAME "/"

/* This is stolen from gtkfilechooserdefault.c:set_file_system_backend() */
static GtkFileSystem *
get_file_system (void)
{
  GtkFileSystem *file_system = NULL;

#if 1
  file_system = gtk_file_system_create ("gnome-vfs");
#else
  GtkSettings *settings = gtk_settings_get_default ();
  gchar *default_backend = NULL;

  g_object_get (settings, "gtk-file-chooser-backend", &default_backend, NULL);
  if (default_backend)
    {
      file_system = gtk_file_system_create (default_backend);
      g_free (default_backend);
    }

  if (!file_system)
    {
#if defined (G_OS_UNIX)
      file_system = gtk_file_system_unix_new ();
#elif defined (G_OS_WIN32)
      file_system = gtk_file_system_win32_new ();
#else
#error "No default filesystem implementation on the platform"
#endif
    }

#endif

  return file_system;
}



/***** Testing infrastructure *****/

typedef struct {
  gboolean callback_was_called;
  gboolean timeout_was_called;
} TestCallbackClosure;

static void
notify_callback_called (TestCallbackClosure *closure)
{
  closure->callback_was_called = TRUE;
  gtk_main_quit ();
}

static gboolean
timeout_cb (gpointer data)
{
  TestCallbackClosure *closure;

  closure = data;

  closure->timeout_was_called = TRUE;
  gtk_main_quit ();

  return FALSE;
}

static void
wait_for_callback (TestCallbackClosure *closure)
{
  gdk_threads_add_timeout (CALLBACK_TIMEOUT_MS, timeout_cb, closure);
  gtk_main ();
}

typedef struct {
  const char *test_name;
  gboolean callback_must_be_called;
  gpointer (* setup_fn) (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			 TestCallbackClosure *callback_closure);
  void (* cleanup_fn) (gpointer data, gboolean *success, char **failure_reason);
} TestSpec;

static gboolean
run_test (TestSpec *test_spec)
{
  GtkFileSystem *file_system;
  TestCallbackClosure closure;
  gboolean success_setup, success_cleanup;
  gpointer test_data;
  gboolean callback_success;
  gboolean passed;
  char *setup_failure_reason;
  char *cleanup_failure_reason;

  file_system = get_file_system ();
  if (!file_system)
    {
      printf ("FAIL: test \"%s\"\n", test_spec->test_name);
      printf ("      could not create file system!\n");
      return FALSE;
    }

  closure.callback_was_called = FALSE;
  closure.timeout_was_called = FALSE;

  success_setup = success_cleanup = callback_success = FALSE;
  setup_failure_reason = cleanup_failure_reason = NULL;

  test_data = test_spec->setup_fn (file_system, &success_setup, &setup_failure_reason, &closure);
  if (success_setup)
    {
      wait_for_callback (&closure);

      test_spec->cleanup_fn (test_data, &success_cleanup, &cleanup_failure_reason);

      callback_success = (test_spec->callback_must_be_called == closure.callback_was_called);
    }

  g_object_unref (file_system);

  passed = (success_setup && success_cleanup && callback_success);

  printf ("%s: test \"%s\"\n", passed ? "PASS" : "FAIL", test_spec->test_name);

  if (!passed)
    {
      if (!success_setup)
	printf ("      failure during setup: %s\n",
		setup_failure_reason ? setup_failure_reason : "unknown failure");
      else
	{
	  if (!success_cleanup)
	    printf ("      failure during cleanup: %s\n",
		    cleanup_failure_reason ? cleanup_failure_reason : "unknown failure");

	  if (!callback_success)
	    printf ("      callback %s called but it %s called\n",
		    test_spec->callback_must_be_called ? "MUST BE" : "MUST NOT BE",
		    closure.callback_was_called ? "WAS" : "WAS NOT");
	}
    }

  g_free (setup_failure_reason);
  g_free (cleanup_failure_reason);

  return passed;
}

static gboolean
run_tests (TestSpec *test_specs, int num_tests)
{
  int i;
  int num_passed;

  num_passed = 0;

  for (i = 0; i < num_tests; i++)
    if (run_test (test_specs + i))
      num_passed++;

  if (num_passed == num_tests)
    printf ("ALL TESTS PASSED\n");
  else
    printf ("%d of %d tests FAILED\n", (num_tests - num_passed), num_tests);

  return (num_passed == num_tests);
}



/***** Test functions *****/

static void
sleep_and_cancel_handle (GtkFileSystemHandle *handle)
{
  g_usleep (CANCEL_TIMEOUT_MS * 1000);
  gtk_file_system_cancel_operation (handle);
}

/* get_folder */

struct get_folder_data {
  TestCallbackClosure *callback_closure;
  GtkFileSystemHandle *handle;
  GtkFileFolder *folder;
};

static void
get_folder_cb (GtkFileSystemHandle *handle,
	       GtkFileFolder       *folder,
	       const GError        *error,
	       gpointer             data)
{
  struct get_folder_data *get_folder_data;

  get_folder_data = data;
  get_folder_data->folder = folder;
  notify_callback_called (get_folder_data->callback_closure);
}

static gpointer
get_folder_generic_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			  TestCallbackClosure *callback_closure)
{
  struct get_folder_data *get_folder_data;
  GtkFilePath *path;

  path = gtk_file_system_filename_to_path (file_system, GET_FOLDER_FILENAME);
  if (!path)
    {
      *success = FALSE;
      *failure_reason = g_strdup_printf ("could not turn \"%s\" into a GtkFilePath", GET_FOLDER_FILENAME);
      return NULL;
    }

  get_folder_data = g_new (struct get_folder_data, 1);

  get_folder_data->callback_closure = callback_closure;
  get_folder_data->folder = NULL;

  get_folder_data->handle = gtk_file_system_get_folder (file_system,
							path,
							GTK_FILE_INFO_ALL,
							get_folder_cb,
							get_folder_data);
  gtk_file_path_free (path);

  if (!get_folder_data->handle)
    {
      g_free (get_folder_data);
      *success = FALSE;
      *failure_reason = g_strdup ("gtk_file_system_get_folder() returned a NULL handle");
      return NULL;
    }

  *success = TRUE;

  return get_folder_data;
}

static gpointer
get_folder_no_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			    TestCallbackClosure *callback_closure)
{
  return get_folder_generic_setup (file_system, success, failure_reason, callback_closure);
}

static void
get_folder_cleanup (gpointer data, gboolean *success, char **failure_reason)
{
  struct get_folder_data *get_folder_data;

  get_folder_data = data;

  if (get_folder_data->folder)
    g_object_unref (get_folder_data->folder);

  g_free (get_folder_data);

  *success = TRUE;
}

static gpointer
get_folder_with_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			      TestCallbackClosure *callback_closure)
{
  struct get_folder_data *get_folder_data;

  get_folder_data = get_folder_generic_setup (file_system, success, failure_reason, callback_closure);

  if (*success)
    sleep_and_cancel_handle (get_folder_data->handle);

  return get_folder_data;
}

/* get_info */

struct get_info_data {
  TestCallbackClosure *callback_closure;
  GtkFileSystemHandle *handle;
};

static void
get_info_cb (GtkFileSystemHandle *handle,
	     const GtkFileInfo   *file_info,
	     const GError        *error,
	     gpointer             data)
{
  struct get_info_data *get_info_data;

  get_info_data = data;
  notify_callback_called (get_info_data->callback_closure);
}

static gpointer
get_info_generic_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			TestCallbackClosure *callback_closure)
{
  GtkFilePath *path;
  struct get_info_data *get_info_data;

  path = gtk_file_system_filename_to_path (file_system, GET_INFO_FILENAME);
  if (!path)
    {
      *success = FALSE;
      *failure_reason = g_strdup_printf ("could not turn \"%s\" into a GtkFilePath", GET_INFO_FILENAME);
      return NULL;
    }

  get_info_data = g_new (struct get_info_data, 1);

  get_info_data->callback_closure = callback_closure;
  get_info_data->handle = gtk_file_system_get_info (file_system,
						    path,
						    GTK_FILE_INFO_ALL,
						    get_info_cb,
						    get_info_data);
  gtk_file_path_free (path);

  if (!get_info_data->handle)
    {
      g_free (get_info_data);
      *success = FALSE;
      *failure_reason = g_strdup ("gtk_file_system_get_info() returned a NULL handle");
      return NULL;
    }

  *success = TRUE;
  return get_info_data;
}

static gpointer
get_info_no_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			  TestCallbackClosure *callback_closure)
{
  return get_info_generic_setup (file_system, success, failure_reason, callback_closure);
}

static void
get_info_cleanup (gpointer data, gboolean *success, char **failure_reason)
{
  struct get_info_data *get_info_data;

  get_info_data = data;
  g_free (get_info_data);

  *success = TRUE;
}

static gpointer
get_info_with_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			    TestCallbackClosure *callback_closure)
{
  struct get_info_data *get_info_data;

  get_info_data = get_info_generic_setup (file_system, success, failure_reason, callback_closure);

  if (*success)
    sleep_and_cancel_handle (get_info_data->handle);

  return get_info_data;
}

/* create_folder */

struct create_folder_data {
  TestCallbackClosure *callback_closure;
  GtkFileSystemHandle *handle;
};

static void
create_folder_cb (GtkFileSystemHandle *handle,
		  const GtkFilePath   *path,
		  const GError        *error,
		  gpointer             data)
{
  struct get_folder_data *get_folder_data;

  get_folder_data = data;
  notify_callback_called (get_folder_data->callback_closure);
}

static gpointer
create_folder_generic_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			     TestCallbackClosure *callback_closure)
{
  GtkFilePath *path;
  struct create_folder_data *create_folder_data;

  path = gtk_file_system_filename_to_path (file_system, CREATE_FOLDER_FILENAME);
  if (!path)
    {
      *success = FALSE;
      *failure_reason = g_strdup_printf ("could not turn \"%s\" into a GtkFilePath", CREATE_FOLDER_FILENAME);
      return NULL;
    }

  create_folder_data = g_new (struct create_folder_data, 1);

  create_folder_data->callback_closure = callback_closure;
  create_folder_data->handle = gtk_file_system_create_folder (file_system,
							      path,
							      create_folder_cb,
							      create_folder_data);
  gtk_file_path_free (path);

  if (!create_folder_data->handle)
    {
      g_free (create_folder_data);
      *success = FALSE;
      *failure_reason = g_strdup ("gtk_file_system_create_folder() returned a NULL handle");
      return NULL;
    }

  *success = TRUE;
  return create_folder_data;
}

static gpointer
create_folder_no_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			       TestCallbackClosure *callback_closure)
{
  return create_folder_generic_setup (file_system, success, failure_reason, callback_closure);
}

static void
create_folder_cleanup (gpointer data, gboolean *success, char **failure_reason)
{
  struct create_folder_data *create_folder_data;

  create_folder_data = data;

  rmdir (CREATE_FOLDER_FILENAME);

  g_free (create_folder_data);

  *success = TRUE;
}

static gpointer
create_folder_with_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
				 TestCallbackClosure *callback_closure)
{
  struct create_folder_data *create_folder_data;

  create_folder_data = create_folder_generic_setup (file_system, success, failure_reason, callback_closure);

  if (*success)
    sleep_and_cancel_handle (create_folder_data->handle);

  return create_folder_data;
}

/* volume_mount */

struct volume_mount_data {
  TestCallbackClosure *callback_closure;
  GtkFileSystemVolume *volume;
  GtkFileSystemHandle *handle;
};

static void
volume_mount_cb (GtkFileSystemHandle *handle,
		 GtkFileSystemVolume *volume,
		 const GError        *error,
		 gpointer             data)
{
  struct volume_mount_data *volume_mount_data;

  volume_mount_data = data;
  notify_callback_called (volume_mount_data->callback_closure);
}

static gpointer
volume_mount_generic_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			    TestCallbackClosure *callback_closure)
{
  GtkFilePath *path;
  struct volume_mount_data *volume_mount_data;

  path = gtk_file_system_filename_to_path (file_system, VOLUME_MOUNT_FILENAME);
  if (!path)
    {
      *success = FALSE;
      *failure_reason = g_strdup_printf ("could not turn \"%s\" into a GtkFilePath", VOLUME_MOUNT_FILENAME);
      return NULL;
    }

  volume_mount_data = g_new (struct volume_mount_data, 1);

  volume_mount_data->callback_closure = callback_closure;
  volume_mount_data->volume = gtk_file_system_get_volume_for_path (file_system, path);
  gtk_file_path_free (path);

  if (!volume_mount_data->volume)
    {
      g_free (volume_mount_data);
      *success = FALSE;
      *failure_reason = g_strdup ("gtk_file_system_get_volume_for_path() returned a NULL volume");
      return NULL;
    }

  volume_mount_data->handle = gtk_file_system_volume_mount (file_system,
							    volume_mount_data->volume,
							    volume_mount_cb,
							    volume_mount_data);
  if (!volume_mount_data->handle)
    {
      g_object_unref (volume_mount_data->volume);
      g_free (volume_mount_data);
      *success = FALSE;
      *failure_reason = g_strdup ("gtk_file_system_volume_mount() returned a NULL handle");
      return NULL;
    }

  *success = TRUE;
  return volume_mount_data;
}

static gpointer
volume_mount_no_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			      TestCallbackClosure *callback_closure)
{
  return volume_mount_generic_setup (file_system, success, failure_reason, callback_closure);
}

static void
volume_mount_cleanup (gpointer data, gboolean *success, char **failure_reason)
{
  struct volume_mount_data *volume_mount_data;

  volume_mount_data = data;

  g_object_unref (volume_mount_data->volume);
  g_free (volume_mount_data);

  *success = TRUE;
}

static gpointer
volume_mount_with_cancel_setup (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
				TestCallbackClosure *callback_closure)
{
  struct volume_mount_data *volume_mount_data;

  volume_mount_data = volume_mount_generic_setup (file_system, success, failure_reason, callback_closure);

  if (*success)
    sleep_and_cancel_handle (volume_mount_data->handle);

  return volume_mount_data;
}

/* tests */

static TestSpec tests[] = {
  {
    "get_folder no cancel",
    TRUE,
    get_folder_no_cancel_setup,
    get_folder_cleanup
  },
  {
    "get_folder with cancel",
    FALSE,
    get_folder_with_cancel_setup,
    get_folder_cleanup
  },
  {
    "get_info no cancel",
    TRUE,
    get_info_no_cancel_setup,
    get_info_cleanup
  },
  {
    "get_info with cancel",
    FALSE,
    get_info_with_cancel_setup,
    get_info_cleanup
  },
  {
    "create_folder no cancel",
    TRUE,
    create_folder_no_cancel_setup,
    create_folder_cleanup
  },
  {
    "create_folder with cancel",
    FALSE,
    create_folder_with_cancel_setup,
    create_folder_cleanup
  },
  {
    "volume_mount no cancel",
    TRUE,
    volume_mount_no_cancel_setup,
    volume_mount_cleanup
  },
  {
    "volume_mount with cancel",
    FALSE,
    volume_mount_with_cancel_setup,
    volume_mount_cleanup
  }
};



/***** main *****/

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

int
main (int argc, char **argv)
{
  gboolean passed;
  gboolean zero_warnings;
  gboolean zero_errors;
  gboolean zero_critical_errors;

  default_log_handler = g_log_set_default_handler (log_override_cb, NULL);

  gtk_init (&argc, &argv);

  /* Start tests */

  passed = run_tests (tests, G_N_ELEMENTS (tests));

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
