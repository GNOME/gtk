/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "reftest-compare.h"
#include "reftest-module.h"

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

typedef enum {
  SNAPSHOT_WINDOW,
  SNAPSHOT_DRAW
} SnapshotMode;

/* This is exactly the style information you've been looking for */
#define GTK_STYLE_PROVIDER_PRIORITY_FORCE G_MAXUINT

static char *arg_output_dir = NULL;
static char *arg_base_dir = NULL;
static char *arg_direction = NULL;

static const GOptionEntry test_args[] = {
  { "output",         'o', 0, G_OPTION_ARG_FILENAME, &arg_output_dir,
    "Directory to save image files to", "DIR" },
  { "directory",        'd', 0, G_OPTION_ARG_FILENAME, &arg_base_dir,
    "Directory to run tests from", "DIR" },
  { "direction",       0, 0, G_OPTION_ARG_STRING, &arg_direction,
    "Set text direction", "ltr|rtl" },
  { NULL }
};

static gboolean
parse_command_line (int *argc, char ***argv)
{
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- run GTK reftests");
  g_option_context_add_main_entries (context, test_args, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, argc, argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return FALSE;
    }

  gtk_test_init (argc, argv);

  if (g_strcmp0 (arg_direction, "rtl") == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
  else if (g_strcmp0 (arg_direction, "ltr") == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
  else if (arg_direction != NULL)
    g_printerr ("Invalid argument passed to --direction argument. Valid arguments are 'ltr' and 'rtl'\n");

  return TRUE;
}

static const char *
get_output_dir (void)
{
  static const char *output_dir = NULL;
  GError *error = NULL;

  if (output_dir)
    return output_dir;

  if (arg_output_dir)
    {
      GFile *file = g_file_new_for_commandline_arg (arg_output_dir);
      output_dir = g_file_get_path (file);
      g_object_unref (file);
    }
  else
    {
      output_dir = g_get_tmp_dir ();
    }

  if (!g_file_test (output_dir, G_FILE_TEST_EXISTS))
    {
      GFile *file;

      file = g_file_new_for_path (output_dir);
      g_assert (g_file_make_directory_with_parents (file, NULL, &error));
      g_assert_no_error (error);
      g_object_unref (file);
    }

  return output_dir;
}

static void
get_components_of_test_file (const char  *test_file,
                             char       **directory,
                             char       **basename)
{
  if (directory)
    {
      *directory = g_path_get_dirname (test_file);
    }

  if (basename)
    {
      char *base = g_path_get_basename (test_file);
      
      if (g_str_has_suffix (base, ".ui"))
        base[strlen (base) - strlen (".ui")] = '\0';

      *basename = base;
    }
}

static char *
get_output_file (const char *test_file,
                 const char *extension)
{
  const char *output_dir = get_output_dir ();
  char *result, *base;

  get_components_of_test_file (test_file, NULL, &base);

  result = g_strconcat (output_dir, G_DIR_SEPARATOR_S, base, extension, NULL);
  g_free (base);

  return result;
}

static char *
get_test_file (const char *test_file,
               const char *extension,
               gboolean    must_exist)
{
  GString *file = g_string_new (NULL);
  char *dir, *base;

  get_components_of_test_file (test_file, &dir, &base);

  file = g_string_new (dir);
  g_string_append (file, G_DIR_SEPARATOR_S);
  g_string_append (file, base);
  g_string_append (file, extension);

  g_free (dir);
  g_free (base);

  if (must_exist &&
      !g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static GtkStyleProvider *
add_extra_css (const char *testname,
               const char *extension)
{
  GtkStyleProvider *provider = NULL;
  char *css_file;
  
  css_file = get_test_file (testname, extension, TRUE);
  if (css_file == NULL)
    return NULL;

  provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
  gtk_css_provider_load_from_path (GTK_CSS_PROVIDER (provider),
                                   css_file,
                                   NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_FORCE);

  g_free (css_file);
  
  return provider;
}

static void
remove_extra_css (GtkStyleProvider *provider)
{
  if (provider == NULL)
    return;

  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                provider);
}

static GtkWidget *
builder_get_toplevel (GtkBuilder *builder)
{
  GSList *list, *walk;
  GtkWidget *window = NULL;

  list = gtk_builder_get_objects (builder);
  for (walk = list; walk; walk = walk->next)
    {
      if (GTK_IS_WINDOW (walk->data) &&
          gtk_widget_get_parent (walk->data) == NULL)
        {
          window = walk->data;
          break;
        }
    }
  
  g_slist_free (list);

  return window;
}

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static gint inhibit_count;
static GMainLoop *loop;

void
reftest_inhibit_snapshot (void)
{
  inhibit_count++;
}

void
reftest_uninhibit_snapshot (void)
{
  g_assert (inhibit_count > 0);
  inhibit_count--;

  if (inhibit_count == 0)
    g_idle_add (quit_when_idle, loop);
}

static void
check_for_draw (GdkEvent *event, gpointer data)
{
  if (event->type == GDK_EXPOSE)
    {
      reftest_uninhibit_snapshot ();
      gdk_event_handler_set ((GdkEventFunc) gtk_main_do_event, NULL, NULL);
    }

  gtk_main_do_event (event);
}

static cairo_surface_t *
snapshot_widget (GtkWidget *widget, SnapshotMode mode)
{
  cairo_surface_t *surface;
  cairo_pattern_t *bg;
  cairo_t *cr;

  g_assert (gtk_widget_get_realized (widget));

  loop = g_main_loop_new (NULL, FALSE);

  /* We wait until the widget is drawn for the first time.
   * We can not wait for a GtkWidget::draw event, because that might not
   * happen if the window is fully obscured by windowed child widgets.
   * Alternatively, we could wait for an expose event on widget's window.
   * Both of these are rather hairy, not sure what's best.
   *
   * We also use an inhibit mechanism, to give module functions a chance
   * to delay the snapshot.
   */
  reftest_inhibit_snapshot ();
  gdk_event_handler_set (check_for_draw, NULL, NULL);
  g_main_loop_run (loop);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget));

  cr = cairo_create (surface);

  switch (mode)
    {
    case SNAPSHOT_WINDOW:
      {
        GdkWindow *window = gtk_widget_get_window (widget);
        if (gdk_window_get_window_type (window) == GDK_WINDOW_TOPLEVEL ||
            gdk_window_get_window_type (window) == GDK_WINDOW_FOREIGN)
          {
            /* give the WM/server some time to sync. They need it.
             * Also, do use popups instead of toplevls in your tests
             * whenever you can. */
            gdk_display_sync (gdk_window_get_display (window));
            g_timeout_add (500, quit_when_idle, loop);
            g_main_loop_run (loop);
          }
        gdk_cairo_set_source_window (cr, window, 0, 0);
        cairo_paint (cr);
      }
      break;
    case SNAPSHOT_DRAW:
      bg = gdk_window_get_background_pattern (gtk_widget_get_window (widget));
      if (bg)
        {
          cairo_set_source (cr, bg);
          cairo_paint (cr);
        }
      gtk_widget_draw (widget, cr);
      break;
    default:
      g_assert_not_reached();
      break;
    }

  cairo_destroy (cr);
  g_main_loop_unref (loop);
  gtk_widget_destroy (widget);

  return surface;
}

static void
connect_signals (GtkBuilder    *builder,
                 GObject       *object,
                 const gchar   *signal_name,
                 const gchar   *handler_name,
                 GObject       *connect_object,
                 GConnectFlags  flags,
                 gpointer       user_data)
{
  ReftestModule *module;
  const char *directory;
  GCallback func;
  GClosure *closure;
  char **split;

  directory = user_data;
  split = g_strsplit (handler_name, ":", -1);

  switch (g_strv_length (split))
    {
    case 1:
      func = gtk_builder_lookup_callback_symbol (builder, split[0]);

      if (func)
        {
          module = NULL;
        }
      else
        {
          module = reftest_module_new_self ();
          if (module == NULL)
            {
              g_error ("glib compiled without module support.");
              return;
            }
          func = reftest_module_lookup (module, split[0]);
          if (!func)
            {
              g_error ("failed to lookup handler for name '%s' when connecting signals", split[0]);
              return;
            }
        }
      break;
    case 2:
      if (g_getenv ("REFTEST_MODULE_DIR"))
        directory = g_getenv ("REFTEST_MODULE_DIR");
      module = reftest_module_new (directory, split[0]);
      if (module == NULL)
        {
          g_error ("Could not load module '%s' from '%s' when looking up '%s'", split[0], directory, handler_name);
          return;
        }
      func = reftest_module_lookup (module, split[1]);
      if (!func)
        {
          g_error ("failed to lookup handler for name '%s' in module '%s'", split[1], split[0]);
          return;
        }
      break;
    default:
      g_error ("Could not connect signal handler named '%s'", handler_name);
      return;
    }

  g_strfreev (split);

  if (connect_object)
    {
      if (flags & G_CONNECT_SWAPPED)
        closure = g_cclosure_new_object_swap (func, connect_object);
      else
        closure = g_cclosure_new_object (func, connect_object);
    }
  else
    {
      if (flags & G_CONNECT_SWAPPED)
        closure = g_cclosure_new_swap (func, NULL, NULL);
      else
        closure = g_cclosure_new (func, NULL, NULL);
    }
  
  if (module)
    g_closure_add_finalize_notifier (closure, module, (GClosureNotify) reftest_module_unref);

  g_signal_connect_closure (object, signal_name, closure, flags & G_CONNECT_AFTER ? TRUE : FALSE);
}

static cairo_surface_t *
snapshot_ui_file (const char *ui_file)
{
  GtkWidget *window;
  GtkBuilder *builder;
  GError *error = NULL;
  char *directory;

  get_components_of_test_file (ui_file, &directory, NULL);

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  gtk_builder_connect_signals_full (builder, connect_signals, directory);
  window = builder_get_toplevel (builder);
  g_object_unref (builder);
  g_free (directory);
  g_assert (window);

  gtk_widget_show (window);

  return snapshot_widget (window, SNAPSHOT_WINDOW);
}

static void
save_image (cairo_surface_t *surface,
            const char      *test_name,
            const char      *extension)
{
  char *filename = get_output_file (test_name, extension);

  g_test_message ("Storing test result image at %s", filename);
  g_assert (cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS);

  g_free (filename);
}

static void
test_ui_file (GFile *file)
{
  char *ui_file, *reference_file;
  cairo_surface_t *ui_image, *reference_image, *diff_image;
  GtkStyleProvider *provider;

  ui_file = g_file_get_path (file);

  provider = add_extra_css (ui_file, ".css");

  ui_image = snapshot_ui_file (ui_file);
  
  reference_file = get_test_file (ui_file, ".ref.ui", TRUE);
  if (reference_file)
    reference_image = snapshot_ui_file (reference_file);
  else
    {
      reference_image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
      g_test_message ("No reference image.");
      g_test_fail ();
    }
  g_free (reference_file);

  diff_image = reftest_compare_surfaces (ui_image, reference_image);

  save_image (ui_image, ui_file, ".out.png");
  save_image (reference_image, ui_file, ".ref.png");
  if (diff_image)
    {
      save_image (diff_image, ui_file, ".diff.png");
      g_test_fail ();
    }

  remove_extra_css (provider);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_test_for_file (GFile *file)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;


  if (g_file_query_file_type (file, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    {
      g_test_add_vtable (g_file_get_path (file),
                         0,
                         g_object_ref (file),
                         NULL,
                         (GTestFixtureFunc) test_ui_file,
                         (GTestFixtureFunc) g_object_unref);
      return;
    }


  enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".ui") ||
          g_str_has_suffix (filename, ".ref.ui"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (file, filename));

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

int
main (int argc, char **argv)
{
  const char *basedir;
  
  /* I don't want to fight fuzzy scaling algorithms in GPUs,
   * so unless you explicitly set it to something else, we
   * will use Cairo's image surface.
   */
  g_setenv ("GDK_RENDERING", "image", FALSE);

  if (!parse_command_line (&argc, &argv))
    return 1;

  if (arg_base_dir)
    basedir = arg_base_dir;
  else
    basedir = g_test_get_dir (G_TEST_DIST);

  if (argc < 2)
    {
      GFile *dir;

      dir = g_file_new_for_path (basedir);
      
      add_test_for_file (dir);

      g_object_unref (dir);
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          add_test_for_file (file);

          g_object_unref (file);
        }
    }

  /* We need to ensure the process' current working directory
   * is the same as the reftest data, because we're using the
   * "file" property of GtkImage as a relative path in builder files.
   */
  chdir (basedir);

  return g_test_run ();
}

