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

#include <config.h>
#include <string.h>
#include <unistd.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gtk/gtkfilesystem.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
  g_timeout_add (CALLBACK_TIMEOUT_MS, timeout_cb, closure);
  gtk_main ();
}

typedef enum {
  TEST_CALLBACK_TYPE_MUST_BE_CALLED,
  TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED,
  TEST_CALLBACK_TYPE_IRRELEVANT
} TestCallbackType;

typedef struct {
  const char *test_name;
  TestCallbackType callback_type;
  gpointer (* setup_fn) (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
			 TestCallbackClosure *callback_closure);
  void (* cleanup_fn) (gpointer data, gboolean *success, char **failure_reason);
} TestSpec;

static gboolean
run_test (TestSpec *test_spec, int test_num)
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
      printf ("FAIL: %d. test \"%s\"\n", test_num, test_spec->test_name);
      printf ("      could not create file system!\n");
      return FALSE;
    }

  closure.callback_was_called = FALSE;
  closure.timeout_was_called = FALSE;

  success_setup = success_cleanup = callback_success = FALSE;
  setup_failure_reason = cleanup_failure_reason = NULL;

  g_assert (test_spec->setup_fn != NULL);

  test_data = test_spec->setup_fn (file_system, &success_setup, &setup_failure_reason, &closure);
  if (success_setup)
    {
      if (test_spec->callback_type == TEST_CALLBACK_TYPE_MUST_BE_CALLED
	  || test_spec->callback_type == TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED)
	wait_for_callback (&closure);

      if (test_spec->cleanup_fn)
	test_spec->cleanup_fn (test_data, &success_cleanup, &cleanup_failure_reason);
      else
	success_cleanup = TRUE;

      callback_success = (test_spec->callback_type == TEST_CALLBACK_TYPE_IRRELEVANT
			  || (test_spec->callback_type == TEST_CALLBACK_TYPE_MUST_BE_CALLED && closure.callback_was_called)
			  || (test_spec->callback_type == TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED && !closure.callback_was_called));
    }

  g_object_unref (file_system);

  passed = (success_setup && success_cleanup && callback_success);

  printf ("%s: %d. test \"%s\"\n", passed ? "PASS" : "FAIL", test_num, test_spec->test_name);

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
	    {
	      const char *str;

	      if (test_spec->callback_type == TEST_CALLBACK_TYPE_MUST_BE_CALLED)
		str = "MUST BE";
	      else if (test_spec->callback_type == TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED)
		str = "MUST NOT BE";
	      else
		{
		  g_assert_not_reached ();
		  str = NULL;
		}

	      printf ("      callback %s called but it %s called\n",
		      str,
		      closure.callback_was_called ? "WAS" : "WAS NOT");
	    }
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
    if (run_test (test_specs + i, i + 1))
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

/* folder load */

struct test_folder_load_data {
  GtkFileSystem *file_system;
  GHashTable *files;
  GtkFileFolder *folder;
  gboolean success;
  char *error_msg;
};

static gboolean
add_path_to_load_data (struct test_folder_load_data *load_data, const GtkFilePath *path)
{
  char *filename;

  if (!load_data->success)
    return FALSE;

  filename = gtk_file_system_path_to_filename (load_data->file_system, path);
  g_assert (filename != NULL);

  if (g_hash_table_lookup (load_data->files, filename))
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("duplicate filename %s", filename);
      g_free (filename);
      return FALSE;
    }

  g_hash_table_insert (load_data->files, filename, filename);

  return TRUE;
}

static gboolean
add_paths_to_load_data (struct test_folder_load_data *load_data, GSList *paths)
{
  GSList *l;

  if (!load_data->success)
    return FALSE;

  for (l = paths; l; l = l->next)
    {
      GtkFilePath *path;

      path = l->data;
      if (!add_path_to_load_data (load_data, path))
	return FALSE;
    }

  return TRUE;
}

static void
test_folder_load_files_added_cb (GtkFileFolder *folder, GSList *paths, gpointer data)
{
  struct test_folder_load_data *load_data;

  load_data = data;

  add_paths_to_load_data (load_data, paths);
}

static void
test_folder_load_cb (GtkFileSystemHandle *handle,
		     GtkFileFolder       *folder,
		     const GError        *error,
		     gpointer             data)
{
  struct test_folder_load_data *load_data;
  GSList *children;
  GError *my_error;

  load_data = data;

  if (!folder)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup (error->message);
      return;
    }

  load_data->folder = folder;

  g_signal_connect (folder, "files-added", G_CALLBACK (test_folder_load_files_added_cb), load_data);

  my_error = NULL;
  if (gtk_file_folder_list_children (load_data->folder, &children, &my_error))
    {
      add_paths_to_load_data (load_data, children);
      gtk_file_paths_free (children);
    }
  else
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup (my_error->message);
      g_error_free (my_error);
    }
}

static gboolean
test_folder_load_timeout_cb (gpointer data)
{
  struct test_folder_load_data *load_data;

  if (load_data->success
      && (load_data->folder == NULL || !gtk_file_folder_is_finished_loading (load_data->folder)))
    return TRUE; /* another round of waiting */

  gtk_main_quit ();
  return FALSE;  
}

static void
foreach_file_free_cb (gpointer key, gpointer value, gpointer data)
{
  char *filename;

  filename = key;
  g_free (filename);
}

static void
check_file_info (struct test_folder_load_data *load_data, const char *filename, GtkFileInfo *file_info)
{
  const char *display_name;
  char *display_basename;
  struct stat st;
  gboolean file_info_is_folder;
  gboolean stat_is_folder;
  GtkFileTime file_info_mtime;
  gint64 file_info_size;

  if (!load_data->success)
    return;

  display_name = gtk_file_info_get_display_name (file_info);
  if (!display_name)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("display_name for %s was NULL", filename);
      return;
    }

  display_basename = g_filename_display_basename (filename);
  if (!display_basename)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("display_basename for %s was NULL", filename);
      return;
    }

  if (strcmp (display_name, display_basename) != 0)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("%s had gtk_file_info_get_display_name()=\"%s\", but g_filename_display_basename()=\"%s\"",
					      filename,
					      display_name,
					      display_basename);
      g_free (display_basename);
      return;
    }
  g_free (display_basename);

  if (stat (filename, &st) != 0)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("could not stat %s", filename);
      return;
    }

  file_info_is_folder = (gtk_file_info_get_is_folder (file_info) != FALSE);
  stat_is_folder = (S_ISDIR (st.st_mode) != FALSE);
  if (file_info_is_folder != stat_is_folder)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("file_info_is_folder=%d but stat_is_folder=%d",
					      (int) file_info_is_folder,
					      (int) stat_is_folder);
      return;
    }

  file_info_mtime = gtk_file_info_get_modification_time (file_info);
  if (file_info_mtime != st.st_mtime)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("file_info_mtime=%" G_GUINT64_FORMAT " but stat_mtime=%" G_GUINT64_FORMAT,
					      (guint64) file_info_mtime, (guint64) st.st_mtime);
      return;
    }

  file_info_size = gtk_file_info_get_size (file_info);
  if (file_info_size != st.st_size)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup_printf ("file_info_size=%" G_GINT64_FORMAT " but stat_size=%" G_GINT64_FORMAT,
					      file_info_mtime, (gint64) st.st_size);
      return;
    }

  /* FIXME: test gtk_file_info_get_is_hidden() */

  /* FIXME: test gtk_file_info_get_mime_type() */

  /* FIXME: test gtk_file_info_get_icon_name() and gtk_file_info_render_icon() */
}

static void
test_folder_load_foreach_test_file_cb (gpointer key, gpointer value, gpointer data)
{
  struct test_folder_load_data *load_data;
  const char *filename;
  GtkFilePath *path;
  GtkFileInfo *file_info;
  GError *error;

  load_data = data;

  if (!load_data->success)
    return;

  filename = key;

  path = gtk_file_system_filename_to_path (load_data->file_system, filename);
  g_assert (path != NULL);

  error = NULL;
  file_info = gtk_file_folder_get_info (load_data->folder, path, &error);

  if (!file_info)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup (error->message);
      g_error_free (error);
    }
  else
    {
      check_file_info (load_data, filename, file_info);
      gtk_file_info_free (file_info);
    }

  gtk_file_path_free (path);
}

static void
test_files_in_directory (struct test_folder_load_data *load_data, const char *dirname)
{
  GDir *dir;
  GError *error;
  const char *filename;

  if (!load_data->success)
    return;

  error = NULL;
  dir = g_dir_open (dirname, 0, &error);
  if (!dir)
    {
      load_data->success = FALSE;
      load_data->error_msg = g_strdup (error->message);
      g_free (error);
      return;
    }

  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      char *full_name;

      full_name = g_build_filename (dirname, filename, NULL);

      if (!g_hash_table_lookup (load_data->files, full_name))
	{
	  load_data->success = FALSE;
	  load_data->error_msg = g_strdup_printf ("g_dir_read_name() returned \"%s\" but it is not present in the folder",
						  full_name);
	  g_free (full_name);
	  break;
	}

      g_free (full_name);
    }

  g_dir_close (dir);
}

static gpointer
test_folder_load (GtkFileSystem *file_system, gboolean *success, char **failure_reason,
		  TestCallbackClosure *callback_closure)
{
  struct test_folder_load_data load_data;
  GtkFilePath *path;
  GtkFileSystemHandle *handle;

  load_data.files = g_hash_table_new (g_str_hash, g_str_equal);
  load_data.file_system = file_system;
  load_data.folder = NULL;
  load_data.success = TRUE;
  load_data.error_msg = NULL;

  path = gtk_file_system_filename_to_path (load_data.file_system, GET_FOLDER_FILENAME);
  if (!path)
    goto out;

  /* Test loading the folder */

  handle = gtk_file_system_get_folder (load_data.file_system,
				       path,
				       GTK_FILE_INFO_ALL,
				       test_folder_load_cb,
				       &load_data);

  g_timeout_add (2000, test_folder_load_timeout_cb, &load_data);
  gtk_main ();

  if (!load_data.success)
    goto out;

  /* Test that the folder has the right information for file data */

  g_hash_table_foreach (load_data.files, test_folder_load_foreach_test_file_cb, &load_data);

  if (!load_data.success)
    goto out;

  /* Test that the folder loaded all the files in the directory */

  test_files_in_directory (&load_data, GET_FOLDER_FILENAME);

 out:

  g_hash_table_foreach (load_data.files, foreach_file_free_cb, NULL);
  g_hash_table_destroy (load_data.files);

  *success = load_data.success;
  *failure_reason = load_data.error_msg;

  if (load_data.folder)
    g_object_unref (load_data.folder);

  if (path)
    gtk_file_path_free (path);

  return NULL;
}

/* tests */

static TestSpec tests[] = {
  {
    "get_folder no cancel",
    TEST_CALLBACK_TYPE_MUST_BE_CALLED,
    get_folder_no_cancel_setup,
    get_folder_cleanup
  },
  {
    "get_folder with cancel",
    TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED,
    get_folder_with_cancel_setup,
    get_folder_cleanup
  },
  {
    "get_info no cancel",
    TEST_CALLBACK_TYPE_MUST_BE_CALLED,
    get_info_no_cancel_setup,
    get_info_cleanup
  },
  {
    "get_info with cancel",
    TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED,
    get_info_with_cancel_setup,
    get_info_cleanup
  },
  {
    "create_folder no cancel",
    TEST_CALLBACK_TYPE_MUST_BE_CALLED,
    create_folder_no_cancel_setup,
    create_folder_cleanup
  },
  {
    "create_folder with cancel",
    TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED,
    create_folder_with_cancel_setup,
    create_folder_cleanup
  },
  {
    "volume_mount no cancel",
    TEST_CALLBACK_TYPE_MUST_BE_CALLED,
    volume_mount_no_cancel_setup,
    volume_mount_cleanup
  },
  {
    "volume_mount with cancel",
    TEST_CALLBACK_TYPE_MUST_NOT_BE_CALLED,
    volume_mount_with_cancel_setup,
    volume_mount_cleanup
  },
  {
    "load folder",
    TEST_CALLBACK_TYPE_IRRELEVANT,
    test_folder_load,
    NULL
  }
};



#if 0
typedef struct GetFolderCallbackData GetFolderCallbackData;

typedef struct {
  GtkFileSystem *file_system;
  int num_callbacks;
  int num_idles;
  GetFolderCallbackData *callbacks;
} SetOfCallbacks;

struct GetFolderCallbackData {
  SetOfCallbacks *set;
  GtkFileSystemHandle *handle;
  gboolean called;
  gboolean cancelled;
};

static SetOfCallbacks *
set_of_callbacks_new (GtkFileSystem *file_system, int num_callbacks)
{
  SetOfCallbacks *set;
  int i;

  set = g_slice_new (SetOfCallbacks);
  set->num_callbacks = num_callbacks;
  set->callbacks = g_slice_alloc0 (sizeof (GetFolderCallbackData) * num_callbacks);

  for (i = 0; i < num_callbacks; i++)
    set->callbacks[i].set = set;

  return set;
}

static void
set_of_callbacks_free (SetOfCallbacks *set)
{
  g_slice_free1 (sizeof (GetFolderCallbackData) * set->num_callbacks, set->callbacks);
  g_slice_free (SetOfCallbacks, set);
}

static void
multi_get_folder_cb (GtkFileSystemHandle *handle,
		     GtkFileFolder       *folder,
		     const GError        *error,
		     gpointer             data)
{
  GetFolderCallbackData *cb_data;

  cb_data = data;

  cb_data->handle = NULL;
  cb_data->called = TRUE;
}

static void
cancel_handle (SetOfCallbacks *set, int num_callback)
{
  g_assert (num_callback < set->num_callbacks);
  g_assert (set->callbacks[num_callback].handle != NULL);
  g_assert (!set->callbacks[num_callback].cancelled);

  gtk_file_system_cancel_operation (set->callbacks[num_callback].handle);
  set->callbacks[num_callback].handle = NULL;
  set->callbacks[num_callback].cancelled = TRUE;
}

static gboolean
get_folder_idle_cb (gpointer data)
{
  SetOfCallbacks *set;

  set = data;
  set->num_idles++;

  if (set->num_idles == 1)
    {
      cancel_handle (set, 0);
    }
  else if (set->num_idles == 2)
    {
      cancel_handle (set, 1);
    }
  else if (set->num_idles == 3)
    {
      cancel_handle (set, 2);
      return FALSE;
    }

  return TRUE;
}

static gboolean
get_folder_timeout_cb (gpointer data)
{
  gtk_main_quit ();
  return FALSE;
}


static gboolean
test_three_get_folders (void)
{
  gboolean retval;
  GtkFileSystem *file_system;
  SetOfCallbacks *set;
  GtkFilePath *path;
  int i;

  retval = FALSE;

  file_system = get_file_system ();
  if (!file_system)
    {
      printf ("FAIL: test two get_folders\n");
      printf ("      could not create file system!\n");
      goto out;
    }

  path = gtk_file_system_filename_to_path (file_system, GET_FOLDER_FILENAME);
  if (!path)
    goto out;

  set = set_of_callbacks_new (file_system, 3);

  set->callbacks[0].handle = gtk_file_system_get_folder (file_system,
							 path,
							 GTK_FILE_INFO_ALL,
							 multi_get_folder_cb,
							 set->callbacks + 0);
  set->callbacks[1].handle = gtk_file_system_get_folder (file_system,
							 path,
							 GTK_FILE_INFO_ALL,
							 multi_get_folder_cb,
							 set->callbacks + 1);
  set->callbacks[2].handle = gtk_file_system_get_folder (file_system,
							 path,
							 GTK_FILE_INFO_ALL,
							 multi_get_folder_cb,
							 set->callbacks + 2);
#if 0
  g_idle_add_full (G_PRIORITY_HIGH,
		   get_folder_idle_cb,
		   set,
		   NULL);
#endif

  g_usleep (500 * 1000);
  if (set->callbacks[0].handle)
    cancel_handle (set, 0);
  gtk_main_iteration ();

  g_usleep (500 * 1000);
/*   if (set->callbacks[1].handle) */
/*     cancel_handle (set, 1); */
  gtk_main_iteration ();

  g_usleep (500 * 1000);
  if (set->callbacks[2].handle)
    cancel_handle (set, 2);

  g_timeout_add (2000, get_folder_timeout_cb, NULL);
  gtk_main ();

  for (i = 0; i < set->num_callbacks; i++)
    {
      printf ("callback %d: called=%s, cancelled=%s\n",
	      i,
	      set->callbacks[i].called ? "TRUE" : "FALSE",
	      set->callbacks[i].cancelled ? "TRUE" : "FALSE");
    }

  retval = TRUE;

 out:

  if (file_system)
    g_object_unref (file_system);

  if (path)
    gtk_file_path_free (path);

  return retval;
}
#endif



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

#if 0
  test_three_get_folders ();
#endif

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
